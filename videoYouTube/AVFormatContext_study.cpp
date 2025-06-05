#include <stdio.h>

extern "C" {
#include <libavformat/avformat.h>
}

int main() {
    AVFormatContext* fmt_ctx = NULL;
    int ret;
    const char* filename = "D:\\20250402_192727.mp4";


    if ((ret = avformat_open_input(&fmt_ctx, filename, NULL, NULL)))
        return ret;

    if ((ret = avformat_find_stream_info(fmt_ctx, NULL)) < 0) {
        return ret;
    }

    for (int i = 0; i < fmt_ctx->nb_streams; i++) {
        av_dump_format(fmt_ctx, i, filename, false);
    }

    avformat_close_input(&fmt_ctx);

    return 0;
}