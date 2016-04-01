#ifndef SIMPLE_DECODER_H
#define SIMPLE_DECODER_H

#include <stdio.h>
#include <string>

#ifdef _WIN32
//Windows
extern "C"
{
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
#include "libavutil/imgutils.h"
};
#else
//Linux...
#ifdef __cplusplus
extern "C"
{
#endif
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#ifdef __cplusplus
};
#endif
#endif



class Decoder{

	AVFormatContext* pFormatCtx;
	AVCodecContext* pCodecCtx;
	AVCodec			*pCodec;
	AVFrame	*pCurFrame, *pFrameRGB;
	uint8_t *out_buffer;
	AVPacket *packet;
	struct SwsContext *img_convert_ctx;

	std::string _filepath;

	int videoStreamIndex;

public:

	Decoder();

	int open_file(std::string filepath);


	int init_frames();
	void print_media_info();
	int test_play();
	void flush();

	bool AV_read_frame();
	bool AV_seek(size_t frame, size_t delta = 150);
	bool AV_seekMs(int64_t tsms);


	void SaveCurFrame();
	void SaveFrame(AVFrame *pFrame, int width, int height, int iFrame, const std::string filePrefix = "images/frame");

	~Decoder();

};


#endif /* SIMPLE_DECODER_H */

