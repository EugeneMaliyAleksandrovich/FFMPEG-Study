#define _CRT_SECURE_NO_DEPRECATE

#include <stdio.h>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}

void pgm_save(unsigned char* buf, int wrap, int xsize, int ysize, FILE* f) {

    fprintf(f, "P5\n%d %d\n%d\n", xsize, ysize, 255);
    for (int i = 0; i < ysize; i++)
        fwrite(buf + i * wrap, 1, xsize, f);
}

static void decode(AVCodecContext* dec_ctx, AVFrame* frame, AVPacket* pkt, FILE* f) {
    int ret;

    ret = avcodec_send_packet(dec_ctx, pkt);
    if (ret < 0) {
        fprintf(stderr, "Error sending a packet for decoding\n");
        exit(1);
    }

    while (ret >= 0) {
        ret = avcodec_receive_frame(dec_ctx, frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            return;
        else if (ret < 0) {
            fprintf(stderr, "Error during decoding\n");
            exit(1);
        }

        printf("saving frame %3d\n", dec_ctx->frame_num);
        fflush(stdout);

        pgm_save(frame->data[0], frame->linesize[0],
            frame->width, frame->height, f);
    }
}

int main() {

    AVFormatContext* fmt_ctx = NULL;
    AVCodecContext* codec_ctx = NULL;
    AVCodec* codec = NULL;
    int ret;
    const char* infilename = "D:\\Sample1.wmv";
    const char* outfilename = "D:\\Sample1_.yuv";
    int videoStreamIndex = -1;

    FILE* fin = NULL;
    FILE* fout = NULL;

    AVFrame* frame = NULL;
    AVPacket* pkt = NULL;

    bool isError = false;
    if ((ret = avformat_open_input(&fmt_ctx, infilename, NULL, NULL))) {
        av_log(NULL, AV_LOG_ERROR, "cannot open input file\n");
        isError = true;
        goto end;
    }

    if ((ret = avformat_find_stream_info(fmt_ctx, NULL)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "cannot get stream info\n");
        isError = true;
        goto end;
    }

    for (int i = 0; i < fmt_ctx->nb_streams; i++) {
        if (fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoStreamIndex = i;
            break;
        }
    }

    if (videoStreamIndex < 0) {
        av_log(NULL, AV_LOG_ERROR, "no stream found\n");
        isError = true;
        goto end;
    }

    // dump video stream info
    av_dump_format(fmt_ctx, videoStreamIndex, infilename, false);

    // alloc memory for codec context
    codec_ctx = avcodec_alloc_context3(NULL);
    
    // retrieve codec params from format context
    if (ret = avcodec_parameters_to_context(codec_ctx, fmt_ctx->streams[videoStreamIndex]->codecpar) < 0) {
        av_log(NULL, AV_LOG_ERROR, "cannot get codec params\n");
        isError = true;
        goto end;
    }

    if (!isError) {
        // find decoding codec
        const AVCodec* codec = avcodec_find_decoder(codec_ctx->codec_id);
        if (codec == NULL) {
            av_log(NULL, AV_LOG_ERROR, "cannot find decoder\n");
            goto end;
        }
        
        // try to open codec
        if (ret = avcodec_open2(codec_ctx, codec, NULL) < 0) {
            av_log(NULL, AV_LOG_ERROR, "cannot open decoder\n");
            goto end;
        }

        fprintf(stderr, "\nDecoding codec is: %s \n", codec->name);

        // init packet
        pkt = av_packet_alloc();
        if (!pkt) {
            av_log(NULL, AV_LOG_ERROR, "cannot init packet\n");
            goto end;
        }

        // init frame
        frame = av_frame_alloc();
        if (!frame) {
            av_log(NULL, AV_LOG_ERROR, "cannot init frame\n");
            goto end;
        }

        fin = fopen(infilename, "rb");
        if (!fin) {
            av_log(NULL, AV_LOG_ERROR, "cannot open input file\n");
            goto end;
        }

        fout = fopen(outfilename, "w");
        if (!fout) {
            av_log(NULL, AV_LOG_ERROR, "cannot open output file\n");
            goto end;
        }
    }

    // main loop
    while (1) {
        if (ret = av_read_frame(fmt_ctx, pkt) < 0) {
            av_log(NULL, AV_LOG_ERROR, "cannot read frame\n");
            break;
        }
        if (pkt->stream_index == videoStreamIndex) {
            decode(codec_ctx, frame, pkt, fout);

        }

        av_packet_unref(pkt);
    }

    // flush decoder
    decode(codec_ctx, frame, NULL, fout);

    // clear and out
end:
    if (fin) {
        fclose(fin);
    }
    if (fout) {
        fclose(fout);
    }
    if (codec_ctx) {
        avcodec_free_context(&codec_ctx);
    }
    if (fmt_ctx) {
        avformat_close_input(&fmt_ctx);
    }
    if (frame) {
        av_frame_free(&frame);
    }
    if (pkt) {
        av_packet_free(&pkt);
    }


	return 0;
}