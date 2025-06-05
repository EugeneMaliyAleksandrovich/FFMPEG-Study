#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
}

void SaveFrame(AVFrame* pFrame, int width, int height, int iFrame) {
	FILE* pFile;
	char szFilename[32];
	int  y;

	// Open file
	sprintf(szFilename, "frame%d.ppm", iFrame);
	pFile = fopen(szFilename, "wb");
	if (pFile == NULL)
		return;

	// Write header
	fprintf(pFile, "P6\n%d %d\n255\n", width, height);

	// Write pixel data
	for (y = 0; y < height; y++)
		fwrite(pFrame->data[0] + y * pFrame->linesize[0], 1, width * 3, pFile);

	// Close file
	fclose(pFile);
}

int main() {
	AVFormatContext *pFormatContext = NULL;
	
	const char* filename = "D:\\Sample1.wmv";

	// open video file, read the header of file.
	if (avformat_open_input(&pFormatContext, filename, NULL, NULL) != 0) {
		printf("Cannot open the file\n");
		return -1;
	}

	// Retrieve stream information
	if (avformat_find_stream_info(pFormatContext, NULL) < 0) {
		printf("Cannot retrieve stream information\n");
		return -1;
	}

	// Dump information about file onto standard error
	av_dump_format(pFormatContext, 0, filename, 0);

	// Find the first video stream
	int videoStream = -1;
	for (int i = 0; i < pFormatContext->nb_streams; i++)
		if (pFormatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
			videoStream = i;
			break;
		}
	if (videoStream == -1) {
		printf("Didn't find a video stream\n");
		return -1;
	}

	// Get a pointer to the codec context for the video stream
	AVCodecContext* pCodecContext = avcodec_alloc_context3(NULL);
	if (avcodec_parameters_to_context(pCodecContext, pFormatContext->streams[videoStream]->codecpar) < 0) {
		printf("Cannot get codec params\n");
		return -1;
	}

	// Find the decoder for the video stream
	// Кодек НЕ используется в моем коде, но манипуляции по его открытию надо выполнить, чтобы работало "под капотом"
	const AVCodec *pCodec = avcodec_find_decoder(pCodecContext->codec_id);
	if (pCodec == NULL) {
		fprintf(stderr, "Unsupported codec!\n");
		return -1; // Codec not found
	}
	// Open codec
	if (avcodec_open2(pCodecContext, pCodec, NULL) < 0) {
		printf("Could not open codec\n");
		return -1; // Could not open codec
	}

	AVFrame* pFrame = NULL, * pFrameRGB = NULL;
	// Allocate video frame
	pFrame = av_frame_alloc();
	// Allocate an AVFrame structure
	pFrameRGB = av_frame_alloc();
	if (pFrameRGB == NULL) {
		return -1;
	}

	// Determine required buffer size and allocate buffer
	int numBytes = av_image_get_buffer_size(AV_PIX_FMT_RGB24, pCodecContext->width,
		pCodecContext->height, 1);
	uint8_t* buffer = (uint8_t*)av_malloc(numBytes * sizeof(uint8_t));

	// Assign appropriate parts of buffer to image planes in pFrameRGB
	// Note that pFrameRGB is an AVFrame, but AVFrame is a superset
	// of AVPicture
	av_image_fill_arrays(pFrameRGB->data, pFrameRGB->linesize, buffer, AV_PIX_FMT_RGB24, pCodecContext->width, pCodecContext->height, 1);

	int frameFinished;
	AVPacket* packet = av_packet_alloc();
	int i = 0;
	while (av_read_frame(pFormatContext, packet) >= 0) {
		// Is this a packet from the video stream?
		if (packet->stream_index == videoStream) {
			int ret;

			// Decode video frame
			ret = avcodec_send_packet(pCodecContext, packet);
			if (ret < 0) {
				fprintf(stderr, "Error sending a packet for decoding\n");
				exit(1);
			}
			ret = avcodec_receive_frame(pCodecContext, pFrame);
			if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
				return -1;
			else if (ret < 0) {
				fprintf(stderr, "Error during decoding\n");
				exit(1);
			}

			// Convert the image from its native format to RGB

			struct  SwsContext* img_convert_ctx = sws_getContext(pFrame->width,
				pFrame->height,
				(enum AVPixelFormat)pFrame->format,
				pFrame->width,
				pFrame->height,
				AV_PIX_FMT_RGB24,
				SWS_FAST_BILINEAR | SWS_FULL_CHR_H_INT | SWS_ACCURATE_RND,
				NULL, NULL, NULL);

			if (img_convert_ctx == NULL)
			{
				fprintf(stderr, "Error during converting img\n");
				exit(1);
			}
			sws_scale(img_convert_ctx, pFrame->data, pFrame->linesize, 0, pFrame->height, pFrameRGB->data, pFrameRGB->linesize);

			// Save the frame to disk
			if (++i == 15) {
				SaveFrame(pFrameRGB, pCodecContext->width, pCodecContext->height, i);
			}

			sws_freeContext(img_convert_ctx);

			// Free the packet that was allocated by av_read_frame
			av_packet_unref(packet);

			printf("Frame %d\n", i);
		}
	}

	av_packet_free(&packet);


	// Free the RGB image
	av_free(buffer);
	av_free(pFrameRGB);

	// Free the YUV frame
	av_free(pFrame);

	// Close the codec
	avcodec_free_context(&pCodecContext);

	// Close the video file
	avformat_close_input(&pFormatContext);

	return 0;
}