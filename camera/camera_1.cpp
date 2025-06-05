#include "camera.h"

#pragma region environmentPreparing

AVFormatContext* getInputFormatContext() {
    int ret;

    AVFormatContext* inputFormatContext = NULL;

    avdevice_register_all(); // Must be executed, otherwise av_find_input_format failed
    const AVInputFormat* ifmt = av_find_input_format("dshow");    // It can also speed up the efficiency of the detection stream scheme
    if (!ifmt) {
        av_log(NULL, AV_LOG_ERROR, "unknow format \n");
        return NULL;
    }

    AVDictionary* options = NULL;
    av_dict_set(&options, "framerate", "15", 0);

    if ((ret = avformat_open_input(&inputFormatContext, input_file, ifmt, &options)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot open input file\n");
        return NULL;
    }

    // Analysis flow information
    if ((ret = avformat_find_stream_info(inputFormatContext, NULL)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot find stream information\n");
        return NULL;
    }

    // Print information
    av_dump_format(inputFormatContext, 0, input_file, 0);

    av_dict_free(&options);

    return inputFormatContext;
}

int getVideoStreamIndex(AVFormatContext* inputFormatContext) {
    int videoStreamIndex = -1, ret;

    if ((ret = av_find_best_stream(inputFormatContext, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, -1)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot find a video stream in the input file\n");
        avformat_close_input(&inputFormatContext);
        return videoStreamIndex;
    }
    videoStreamIndex = ret;

    return videoStreamIndex;
}

const AVCodec* getVideoCodec(AVFormatContext* inputFormatContext, int videoStreamIndex) {
    AVCodecParameters* codecpar = inputFormatContext->streams[videoStreamIndex]->codecpar;

    const AVCodec* videoCodec = avcodec_find_decoder(codecpar->codec_id);
    if (!videoCodec) {
        av_log(NULL, AV_LOG_ERROR, "Can't find decoder\n");
    }

    return videoCodec;
}

AVCodecContext* getVideoDecoderContext(AVFormatContext* inputFormatContext, const AVCodec* videoCodec, int videoStreamIndex) {
    int ret;

    AVCodecContext* videoDecoderContext = avcodec_alloc_context3(videoCodec);
    if (!videoDecoderContext) {
        av_log(NULL, AV_LOG_ERROR, "Could not allocate a decoding context\n");
        avformat_close_input(&inputFormatContext);
        return NULL;
    }

    // Decoder parameter configuration
    AVCodecParameters* codecpar = inputFormatContext->streams[videoStreamIndex]->codecpar;
    if ((ret = avcodec_parameters_to_context(videoDecoderContext, codecpar)) < 0) {
        avformat_close_input(&inputFormatContext);
        avcodec_free_context(&videoDecoderContext);
        return NULL;
    }

    // Turn on the decoder
    if ((ret = avcodec_open2(videoDecoderContext, videoCodec, NULL)) < 0) {
        avformat_close_input(&inputFormatContext);
        avcodec_free_context(&videoDecoderContext);
        return NULL;
    }

    return videoDecoderContext;
}

#pragma endregion

#pragma region streaming

void pgm_save(unsigned char* buf, int wrap, int xsize, int ysize, FILE* f) {

    fprintf(f, "P5\n%d %d\n%d\n", xsize, ysize, 255);
    for (int i = 0; i < ysize; i++)
        fwrite(buf + i * wrap, 1, xsize, f);
}

void decode(AVCodecContext* videoDecoderContext, AVFrame* frame, AVPacket* packet, FILE* file, uint32_t& frameCnt) {
    int ret;

    ret = avcodec_send_packet(videoDecoderContext, packet);

    while (ret >= 0) {
        ret = avcodec_receive_frame(videoDecoderContext, frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        }
        else if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Error while sending a packet to the decoder\n");
            return;
        }

        printf("\rSucceed to decode frame %d\n", frameCnt++);

        // yuyv422
        //fwrite(frame->data[0], 1, frame->width * frame->height * 2, file);
        pgm_save(frame->data[0], frame->linesize[0], frame->width, frame->height, file);

        av_frame_unref(frame);  // Release frame buffer data
    }

    if (packet != NULL) {
        printf("==============================  packet size %d\n", packet->size);
    }

}

void recordStream(AVFormatContext* input_fmt_ctx, int video_stream_index, AVCodecContext* video_decoder_ctx) {

    FILE* fp = fopen("out.yuv", "w");
    if (!fp) {
        av_log(NULL, AV_LOG_ERROR, "cannot open output file\n");
        return;
    }

    AVPacket* pkt = av_packet_alloc();
    AVFrame* frame = av_frame_alloc();

    uint32_t frameCnt = 0;
    int ret;
    for (int i = 0; i < 100; i++) {

        ret = av_read_frame(input_fmt_ctx, pkt);
        if (ret < 0) {
            break;
        }

        // Only process video stream
        if (pkt->stream_index == video_stream_index) {
            decode(video_decoder_ctx, frame, pkt, fp, frameCnt);

            av_frame_unref(frame);
        }
        av_packet_unref(pkt); // Release pkt buffer data
    }

    // Flush decoder
    decode(video_decoder_ctx, frame, NULL, fp, frameCnt);

    av_packet_free(&pkt);
    av_frame_free(&frame);

    fclose(fp);

}

#pragma endregion

int main()
{
    // Инициализировать служебные объекты
    AVFormatContext* inputFormatContext = getInputFormatContext();
    if (inputFormatContext == NULL) {
        return -1;
    }

    int videoStreamIndex = getVideoStreamIndex(inputFormatContext);
    if (videoStreamIndex < 0) {
        return -1;
    }

    const AVCodec* videoCodec = getVideoCodec(inputFormatContext, videoStreamIndex);
    if (!videoCodec) {
        return -1;
    }

    AVCodecContext* videoDecoderContext = getVideoDecoderContext(inputFormatContext, videoCodec, videoStreamIndex);
    if (videoDecoderContext == NULL) {
        return -1;
    }

    // Основная часть программы
    recordStream(inputFormatContext, videoStreamIndex, videoDecoderContext);

    // Освободить память перед завершением программы
    avcodec_free_context(&videoDecoderContext);
    avformat_close_input(&inputFormatContext);

    return 0;
}