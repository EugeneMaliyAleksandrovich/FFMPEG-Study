//https://gist.github.com/Lovesan/ac2434a7e6aa1d5d0d0f6ee32af3d8ad original code

#define __STDC_CONSTANT_MACROS

#include <stdint.h>
#include <inttypes.h>
#include <windows.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C"
{
#endif
#include <libavfilter/avfilter.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavdevice/avdevice.h>
#include <libavutil/avstring.h>
#include <libswresample/swresample.h>
#define SDL_MAIN_HANDLED

#include <sdl2/SDL.h>
#include <sdl2/SDL_thread.h>
#include <sdl2/SDL_render.h>
#include <sdl2/SDL_audio.h>
#ifdef __cplusplus
}
#endif

#define FF_ALLOC_EVENT   (SDL_USEREVENT)
#define FF_REFRESH_EVENT (SDL_USEREVENT + 1)
#define FF_QUIT_EVENT (SDL_USEREVENT + 2)

#define MAX_QUEUE_SIZE (15 * 1024 * 1024)  // 15728640
//#define VIDEO_PICTURE_QUEUE_SIZE (1)
#define SDL_AUDIO_BUFFER_SIZE 1024
#define AVCODEC_MAX_AUDIO_FRAME_SIZE 192000

/** The output bit rate in kbit/s */
#define OUTPUT_BIT_RATE 48000
/** The number of output channels */
#define OUTPUT_CHANNELS 2
/** The audio sample output format */
#define OUTPUT_SAMPLE_FORMAT AV_SAMPLE_FMT_S16


typedef struct _PacketQueue
{
	AVPacketList* first, * last;
	int nb_packets, size;

} PacketQueue;

typedef struct _VideoState
{
	AVFormatContext* pFormatCtx;
	AVCodecContext* audioCtx;
	struct SwrContext* pSwrCtx;
	int audioStream;
	AVStream* audioSt;
	PacketQueue audioq;
	__declspec(align(16)) uint8_t audioBuf[(AVCODEC_MAX_AUDIO_FRAME_SIZE * 3) / 2];   // bigger then parser could give
	unsigned int audioBufSize, audioBufIndex;
	AVPacket audioPkt;

	AVFrame* pAudioFrame;
	__declspec(align(16)) uint8_t audioConvertedData[(AVCODEC_MAX_AUDIO_FRAME_SIZE * 3) / 2];

	//uint8_t * pFrameBuffer;

	HANDLE hParseThread;
	SDL_Renderer* renderer;
	char filename[1024];
	int quit;

} VideoState;

VideoState* global_video_state;

void PacketQueueInit(PacketQueue* pq)
{
	memset(pq, 0, sizeof(PacketQueue));
}

int PacketQueuePut(PacketQueue* pq, const AVPacket* srcPkt)
{
	AVPacketList* elt;
	AVPacket pkt;
	int rv;
	if (!pq) return -1;
	rv = av_packet_ref(&pkt, srcPkt);
	if (rv) return rv;
	elt = (AVPacketList*)av_malloc(sizeof(AVPacketList));
	if (!elt) return -1;
	elt->pkt = pkt;
	elt->next = NULL;

	if (!pq->last)
		pq->first = elt;
	else
		pq->last->next = elt;
	pq->last = elt;
	pq->nb_packets++;
	pq->size += elt->pkt.size;

	return 0;
}

int PacketQueueGet(PacketQueue* pq, AVPacket* pkt)
{
	AVPacketList* elt;
	int rv;

	if (!pq || !pkt) return -1;

	for (;;)
	{
		if (global_video_state->quit)
		{
			rv = -1;
			break;
		}

		elt = pq->first;
		if (elt)
		{
			pq->first = elt->next;
			if (!pq->first)
				pq->last = NULL;
			pq->nb_packets--;
			pq->size -= elt->pkt.size;
			*pkt = elt->pkt;
			av_free(elt);
			rv = 1;
			break;
		}

	}

	return rv;
}

int DecodeAudioFrame(VideoState* is)
{
	int ret;
	int len2, dataSize = 0, outSize = 0;
	uint8_t* converted = &is->audioConvertedData[0];

	while (true)
	{
		if (PacketQueueGet(&is->audioq, &is->audioPkt) < 0)
			return -1;

		ret = avcodec_send_packet(is->audioCtx, &is->audioPkt);
		if (ret) return ret;

		while (!ret)
		{
			ret = avcodec_receive_frame(is->audioCtx, is->pAudioFrame);
			if (ret) return ret; // Малый.Примечание: из-за того, что этой проверки в оригинале не было - я проебал полвоскресенья.

			dataSize = av_samples_get_buffer_size(NULL,
				is->audioCtx->ch_layout.nb_channels,
				is->pAudioFrame->nb_samples,
				is->audioCtx->sample_fmt,
				1);

			outSize = av_samples_get_buffer_size(NULL,
				is->audioCtx->ch_layout.nb_channels,
				is->pAudioFrame->nb_samples,
				AV_SAMPLE_FMT_FLT,
				1);

			// returns the number of samples per channel in one audio frame  -- 8192
			len2 = swr_convert(is->pSwrCtx,
				&converted,							// output
				is->pAudioFrame->nb_samples,
				(const uint8_t**)&is->pAudioFrame->data[0],  // input
				is->pAudioFrame->nb_samples);

			memcpy(is->audioBuf, converted, outSize);
			//	memcpy(is->audioBuf, (const uint8_t*)is->pAudioFrame->data[0], sizeof(is->pAudioFrame->data[0]));
			//	dataSize = outSize;

				/* We have data, return it and come back for more later */
			return outSize;
		}

		av_packet_unref(&is->audioPkt);
	}
	return -1;
}

void AudioCallback(void* userdata, uint8_t* stream, int len)
{
	VideoState* is = (VideoState*)userdata;
	int len1, audioSize, len3;
	while (len > 0)
	{
		len3 = len;
		if (is->audioBufIndex >= is->audioBufSize) //   local buffer already empty
		{
			// already sent all data; get more
			audioSize = DecodeAudioFrame(is);
			if (audioSize < 0)
			{
				// error
				is->audioBufSize = SDL_AUDIO_BUFFER_SIZE; // make the buffer up to the preset value and reset it
				memset(is->audioBuf, 0, sizeof(is->audioBuf));
			}
			else
			{
				is->audioBufSize = audioSize; // buffer size make the samples from i decode up to you
			}
			is->audioBufIndex = 0; // reset the buffer index
		}
		len1 = is->audioBufSize - is->audioBufIndex;  // get buffer size that not yet sent to sdl
		if (len1 > len)   // if it is bigger then sdl wants make sure it is as much as sdl wants
			len1 = len;
		memcpy(stream, (uint8_t*)is->audioBuf + is->audioBufIndex, len1); // copy not sent data to sdl stream
		len -= len1; // decrease sdl buffer size as much as you have sent
		stream += len1; // forward sdl stream
		is->audioBufIndex += len1;    // forward buffer index
		printf("BufSize:%d BufIndex:%d StreamLen:%d Len:%d\n", is->audioBufSize, is->audioBufIndex, len1, len3);
	}
}

int StreamComponentOpen(VideoState* is, int streamIndex)
{
	AVFormatContext* pFormatCtx = is->pFormatCtx; // Содержит ссылку на файл
	
	SDL_AudioSpec wantedSpec = { 0 }, audioSpec = { 0 };
	if (streamIndex < 0 || streamIndex >= pFormatCtx->nb_streams)
		return -1;

	AVStream* stream = pFormatCtx->streams[streamIndex];

	const AVCodec* codec = avcodec_find_decoder(stream->codecpar->codec_id); // Получил нужный кодек для декодирования из кодека файла.
	if (!codec) return -1;

	AVCodecContext* codecCtx = avcodec_alloc_context3(codec); // Содержит рабочее пространство для кодирования и декодирования с заданным кодеком.
	if (!codecCtx) return -1;

	int rv;
	rv = avcodec_parameters_to_context(codecCtx, stream->codecpar);
	if (rv < 0)
	{
		avcodec_free_context(&codecCtx);
		return rv;
	}

	rv = avcodec_open2(codecCtx, codec, NULL);
	if (rv < 0)
	{
		avcodec_free_context(&codecCtx);
		return rv;
	}

	if (stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
	{
		is->audioCtx = codecCtx;

		is->audioCtx->ch_layout.nb_channels = OUTPUT_CHANNELS;
		av_channel_layout_default(&(is->audioCtx->ch_layout), OUTPUT_CHANNELS);

		is->audioStream = streamIndex;
		is->audioBufSize = 0;
		is->audioBufIndex = 0;
		is->audioSt = pFormatCtx->streams[streamIndex];
		memset(&is->audioPkt, 0, sizeof(is->audioPkt));
		is->pAudioFrame = av_frame_alloc();
		if (!is->pAudioFrame) return -1;

		swr_alloc_set_opts2(&(is->pSwrCtx), &(codecCtx->ch_layout), 
					AV_SAMPLE_FMT_FLT, codecCtx->sample_rate, &(codecCtx->ch_layout), 
					codecCtx->sample_fmt, codecCtx->sample_rate, 0, NULL);
			
		if (!is->pSwrCtx) return -1;
		rv = swr_init(is->pSwrCtx);
		if (rv < 0) return rv;

		wantedSpec.channels = codecCtx->ch_layout.nb_channels;
		wantedSpec.freq = codecCtx->sample_rate;
		wantedSpec.format = AUDIO_F32;		// 32 bit floating 
		wantedSpec.silence = 0;
		wantedSpec.samples = SDL_AUDIO_BUFFER_SIZE;        //1024 samples of 4 bytes each * 2 channels = 8192
		wantedSpec.userdata = is;
		wantedSpec.callback = AudioCallback;

		if (SDL_OpenAudio(&wantedSpec, &audioSpec) < 0) {
			fprintf(stderr, "SDL_OpenAudio: %s\n", SDL_GetError());
			return -1;
		}

		PacketQueueInit(&is->audioq);

		SDL_PauseAudio(0);
	}

	else
	{
		avcodec_free_context(&codecCtx);
		return -1;
	}

	return 0;
}

unsigned __stdcall DecodeThread(void* pUserData)
{
	VideoState* is = (VideoState*)pUserData;
	AVPacket* pkt = av_packet_alloc();
	int rv;

	while (av_read_frame((is->pFormatCtx), pkt) >= 0) {
		if (pkt->stream_index != is->audioStream) {
			av_packet_unref(pkt);
			continue;
		}
		
		if (is->quit) break;
		if (is->audioq.size >= MAX_QUEUE_SIZE)
		{
			Sleep(10);
			continue;
		}

		if (pkt->stream_index == is->audioStream) // if it is audio stream packets, put packets to packet queue
		{
			PacketQueuePut(&is->audioq, pkt);
		}
		av_packet_unref(pkt);
	}

	while (!is->quit)
	{
		Sleep(100);
	}

fail:
	if (1)
	{
		SDL_Event evt;
		evt.type = FF_QUIT_EVENT;
		evt.user.data1 = is;
		SDL_PushEvent(&evt);
	}
	return 0;
}

Uint32 TimerRefreshCallback(Uint32 interval, void* param)
{
	SDL_Event evt;
	evt.type = FF_REFRESH_EVENT;
	evt.user.data1 = param;
	SDL_PushEvent(&evt);
	return 0;
}

void ScheduleRefresh(VideoState* is, int delay)
{
	SDL_AddTimer(delay, TimerRefreshCallback, is);
}

int DecodeInterruptCallback(void* userData)
{
	VideoState* is = (VideoState*)userData;
	return is && is->quit;
}


int main()
{
	int rv = 0, audioStream = -1;//, videoStream = -1;
	unsigned int s;
	const char* filename = "D:\\Sample1.wmv";
	//	char* filename = "c:\\aa.mp3";
	//	char* filename = "d:\\movie\\DeathWish2018.mkv";


	char err[1024];
	SDL_Event evt;
	VideoState* is = NULL;

	avdevice_register_all();
	avformat_network_init();

	if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_TIMER))
	{
		fprintf(stderr, "Unable to init SDL: %s \n", SDL_GetError());
		goto cleanup;
	}

	is = (VideoState*)av_mallocz(sizeof(VideoState));
	if (!is) goto cleanup;

	global_video_state = is;

	av_strlcpy(&is->filename[0], filename, sizeof(is->filename));

	is->audioStream = -1;

	is->pFormatCtx = avformat_alloc_context();
	if (!is->pFormatCtx) goto cleanup;

	is->pFormatCtx->interrupt_callback.callback = DecodeInterruptCallback;
	is->pFormatCtx->interrupt_callback.opaque = is;

	rv = avformat_open_input(&(is->pFormatCtx), &(is->filename[0]), NULL, NULL);
	if (rv < 0) goto cleanup;

	rv = avformat_find_stream_info(is->pFormatCtx, NULL);
	if (rv < 0) goto cleanup;

	av_dump_format(is->pFormatCtx, 0, &is->filename[0], 0);

	for (s = 0; s < is->pFormatCtx->nb_streams; ++s)
	{
		av_dump_format(is->pFormatCtx, s, &is->filename[0], FALSE);
		if (is->pFormatCtx->streams[s]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO && audioStream < 0)
		{
			audioStream = s;
		}

	}
	if (audioStream < 0)
	{
		rv = -1;
		goto cleanup;
	}


	is->audioStream = audioStream;


	if (audioStream >= 0)
	{
		rv = StreamComponentOpen(is, audioStream);
		if (rv < 0) goto cleanup;
	}


	is->hParseThread = (HANDLE*)_beginthreadex(NULL, 0, DecodeThread, is, 0, NULL);
	if (!is->hParseThread) goto cleanup;

	ScheduleRefresh(is, 40);

	for (;;)
	{
		SDL_WaitEvent(&evt);
		switch (evt.type)
		{
		case FF_QUIT_EVENT:
		case SDL_QUIT:
			is->quit = 1;

			SDL_Quit();
			goto cleanup;
		default:
			break;
		}
	}
cleanup:

	if (is->hParseThread)
	{
		WaitForSingleObject(is->hParseThread, INFINITE);
	}
	if (is->pAudioFrame)
	{
		av_frame_free(&is->pAudioFrame);
	}

	if (is->audioCtx)
	{
		avcodec_free_context(&is->audioCtx);
	}
	if (is->pSwrCtx)
	{
		swr_free(&is->pSwrCtx);
	}

	if (is->pFormatCtx)
	{
		avformat_close_input(&is->pFormatCtx);
	}
	if (is) av_free(is);
	avformat_network_deinit();
	return rv;
}