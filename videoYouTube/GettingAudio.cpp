
#include "stdio.h"

extern "C" {
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
}


// here we assume that the audio holds AV_SAMPLE_FMT_S16 data as WAV
typedef int16_t DATA_TYPE;

int main() {
    //const char* filename = "D:\\Studying\\Projects\\Programming\\FFMPEG\\FFMPEG_First_WithDebug\\sample.wav";
    const char* filename = "D:\\Sample1.wmv";

    DATA_TYPE* data = NULL;  // output of the audio data
    size_t size = 0;  // size of the output
    int ret_code;     // checking return code at each step

    AVFormatContext* fmt = avformat_alloc_context();
    ret_code = avformat_open_input(&fmt, filename, NULL, NULL);
    if (ret_code < 0) {
        fprintf(stderr, "failed to open file at path %s\n", filename);
        avformat_close_input(&fmt);
        return -2;
    }

    // find the first audio stream
    ret_code = avformat_find_stream_info(fmt, NULL);
    if (ret_code < 0) {
        fprintf(stderr, "could not retrieve stream info (%d)\n", ret_code);
        return -3;
    }
    int stream_idx = -1;
    for (size_t i = 0; i < fmt->nb_streams; i++) {
        if (fmt->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            stream_idx = i;
            break;
        }
    }
    if (stream_idx == -1) {
        fprintf(stderr, "found no audio stream in the file\n");
        avformat_close_input(&fmt);
        return -4;
    }

    AVStream* stream = fmt->streams[stream_idx];
    const AVCodec* codec = avcodec_find_decoder(stream->codecpar->codec_id);
    AVCodecContext* codec_ctx = avcodec_alloc_context3(codec);
    ret_code = avcodec_parameters_to_context(codec_ctx, stream->codecpar);
    if (ret_code < 0) {
        fprintf(stderr, "failed to pass codepar to codec context (%d)\n", ret_code);
        avcodec_free_context(&codec_ctx);
        //avcodec_close(codec_ctx);
        avformat_close_input(&fmt);
        return -5;
    }
    ret_code = avcodec_open2(codec_ctx, codec, NULL);
    if (ret_code < 0) {
        fprintf(stderr, "failed to open decoder for stream #%u with error %d\n", stream_idx, ret_code);
        avcodec_free_context(&codec_ctx);
        //avcodec_close(codec_ctx);
        avformat_close_input(&fmt);
        return -6;
    }
    if (stream->codecpar->format != AV_SAMPLE_FMT_FLTP) {
        avcodec_free_context(&codec_ctx);
        //avcodec_close(codec_ctx);
        avformat_close_input(&fmt);
        return -7;
    }

    AVPacket* packet = av_packet_alloc();
    int flag = 0;
    while (av_read_frame(fmt, packet) >= 0) {
        if (packet->stream_index != stream_idx) {
            av_packet_unref(packet);
            continue;
        }
        ret_code = avcodec_send_packet(codec_ctx, packet);
        if (ret_code < 0) {
            fprintf(stderr, "warning: failed to send packet to codec (%d)\n", ret_code);
            av_packet_unref(packet);
            continue;
        }
        AVFrame* frame = av_frame_alloc();
        ret_code = avcodec_receive_frame(codec_ctx, frame);
        if (ret_code == AVERROR(EAGAIN)) {
            // output not available at this stage, try again
            av_frame_unref(frame);
            av_packet_unref(packet);
            continue;
        }
        if (ret_code < 0) {
            fprintf(stderr, "error: failed to receive frame from codec (%d)\n", ret_code);
            av_frame_unref(frame);
            av_packet_unref(packet);
            flag = -8;
            break;
        }
        int out_samples = frame->nb_samples * frame->ch_layout.nb_channels;
        data = (DATA_TYPE*)realloc(data, (size + out_samples) * sizeof(DATA_TYPE));
        memcpy(data + size, frame->data[0], out_samples * sizeof(DATA_TYPE));
        size += out_samples;
        av_frame_unref(frame);
        av_packet_unref(packet);
    }

    avcodec_free_context(&codec_ctx);
    //avcodec_close(codec_ctx);
    avformat_close_input(&fmt);
    if (flag < 0) {
        return flag;
    }

    int sum = 0;
    for (size_t i = 0; i < size; i++) {
        sum += data[i];
    }
    printf("extract sum(data[%zu]) = %d\n", size, sum);
    free(data);

    return 0;
}