#include <stdio.h>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}

int main() {
    AVFormatContext* fmt_ctx = NULL;
    AVCodecContext* codec_ctx = NULL;
    //AVCodec* codec = NULL;
    int ret;
    const char* filename = "D:\\20250402_192727.mp4";
    int videoStreamIndex = -1;

    bool isError = false;
    if ((ret = avformat_open_input(&fmt_ctx, filename, NULL, NULL))) {
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

    av_dump_format(fmt_ctx, videoStreamIndex, filename, false);

    codec_ctx = avcodec_alloc_context3(NULL);

    if (ret = avcodec_parameters_to_context(codec_ctx, fmt_ctx->streams[videoStreamIndex]->codecpar) < 0) {
        av_log(NULL, AV_LOG_ERROR, "cannot get codec params\n");
        isError = true;
        goto end;
    }

    if (!isError) {
        const AVCodec* codec = avcodec_find_decoder(codec_ctx->codec_id);
        if (codec == NULL) {
            av_log(NULL, AV_LOG_ERROR, "cannot find decoder\n");
            goto end;
        }

        if (ret = avcodec_open2(codec_ctx, codec, NULL) < 0) {
            av_log(NULL, AV_LOG_ERROR, "cannot open decoder\n");
            goto end;
        }

        fprintf(stderr, "\nDecoding codec is: %s \n", codec->name);
    }
    
    
    avformat_close_input(&fmt_ctx);

end:
    if (codec_ctx) {
        avcodec_free_context(&codec_ctx);
    }
    if (fmt_ctx) {
        avformat_close_input(&fmt_ctx);
    }

    return 0;
}