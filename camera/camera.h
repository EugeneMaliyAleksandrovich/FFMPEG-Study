#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>

extern "C" {

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavdevice/avdevice.h>

}

const char* input_file = "video=HP HD Camera";