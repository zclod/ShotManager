#include "simple_decoder.h"

#include <locale>
#include <algorithm>

#define __STDC_CONSTANT_MACROS

Decoder::Decoder(){
	av_register_all();
	avformat_network_init();
	pFormatCtx = avformat_alloc_context();
}

int Decoder::open_file(std::string filepath){
	_filepath = filepath;

	if (avformat_open_input(&pFormatCtx, filepath.data(), NULL, NULL) != 0){
		printf("Couldn't open input stream.\n");
		return -1;
	}
	if (avformat_find_stream_info(pFormatCtx, NULL)<0){
		printf("Couldn't find stream information.\n");
		return -1;
	}
	videoStreamIndex = -1;
	for (int i = 0; i<pFormatCtx->nb_streams; i++)
	if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO){
		videoStreamIndex = i;
		break;
	}

	if (videoStreamIndex == -1){
		printf("Didn't find a video stream.\n");
		return -1;
	}

	pCodecCtx = pFormatCtx->streams[videoStreamIndex]->codec;
	pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
	if (pCodec == NULL){
		printf("Codec not found.\n");
		return -1;
	}
	if (avcodec_open2(pCodecCtx, pCodec, NULL)<0){
		printf("Could not open codec.\n");
		return -1;
	}
	return 0;
}


int Decoder::init_frames(){
	pCurFrame = av_frame_alloc();
	// pFrameYUV = av_frame_alloc();
	pFrameRGB = av_frame_alloc();
	if (pFrameRGB == NULL)
		return -1;
	//out_buffer = (unsigned char *)av_malloc(av_image_get_buffer_size(AV_PIX_FMT_YUV420P, pCodecCtx->width, pCodecCtx->height, 1));
	out_buffer = (uint8_t*)av_malloc(av_image_get_buffer_size(AV_PIX_FMT_RGB24, pCodecCtx->width, pCodecCtx->height, 1));
	av_image_fill_arrays(pFrameRGB->data, pFrameRGB->linesize, out_buffer, AV_PIX_FMT_RGB24, pCodecCtx->width, pCodecCtx->height, 1);

	packet = (AVPacket *)av_malloc(sizeof(AVPacket));

	img_convert_ctx = sws_getContext(pCodecCtx->width, pCodecCtx->height, pCodecCtx->pix_fmt,
		pCodecCtx->width, pCodecCtx->height, AV_PIX_FMT_RGB24, SWS_BICUBIC, NULL, NULL, NULL);

	return 0;
}


void Decoder::print_media_info(){
	//Output Info-----------------------------
	printf("--------------- File Information ----------------\n");
	av_dump_format(pFormatCtx, 0, _filepath.data(), 0);
	printf("-------------------------------------------------\n");
}


int Decoder::test_play(){
	int ret, got_picture;
	while (av_read_frame(pFormatCtx, packet) >= 0){
		if (packet->stream_index == videoStreamIndex){
			ret = avcodec_decode_video2(pCodecCtx, pCurFrame, &got_picture, packet);
			if (ret < 0){
				printf("Decode Error.\n");
				return -1;
			}
			if (got_picture){
				sws_scale(img_convert_ctx, (const uint8_t* const*)pCurFrame->data, pCurFrame->linesize, 0, pCodecCtx->height, pFrameRGB->data, pFrameRGB->linesize);

				// y_size = pCodecCtx->width*pCodecCtx->height;
				// fwrite(pFrameYUV->data[0], 1, y_size, fp_yuv);    //Y 
				// fwrite(pFrameYUV->data[1], 1, y_size / 4, fp_yuv);  //U
				// fwrite(pFrameYUV->data[2], 1, y_size / 4, fp_yuv);  //V
				int64_t framePts = av_frame_get_best_effort_timestamp(pCurFrame);
				int64_t pcktPts;

				/*** management of other codecs here using av_frame_get_best_effort_timestamp() ***/
				//----------------------------------------------------------------------------------------------------------------------------------------------------------
				/* With this approach I have been getting correct pts info after many av_read_frame loops */
				if (pCodecCtx->codec->id == AV_CODEC_ID_H264)
				{
					pcktPts = av_rescale_q(packet->pts, //pFrame->pts always invalid here
						pFormatCtx->streams[videoStreamIndex]->time_base,
						pFormatCtx->streams[videoStreamIndex]->codec->time_base);
					pcktPts = (pcktPts / pCodecCtx->ticks_per_frame);
				}

				//----------------------------------------------------------------------------------------------------------------------------------------------------------
				int64_t tsms = 10;
				int64_t DesiredFrameNumber = av_rescale(tsms, pFormatCtx->streams[videoStreamIndex]->time_base.den, pFormatCtx->streams[videoStreamIndex]->time_base.num);
				DesiredFrameNumber /= 1000;

				int target = DesiredFrameNumber *
					(pFormatCtx->streams[videoStreamIndex]->time_base.den /
					pFormatCtx->streams[videoStreamIndex]->time_base.num) /
					(pFormatCtx->streams[videoStreamIndex]->codec->time_base.den /
					pFormatCtx->streams[videoStreamIndex]->codec->time_base.num)*
					pCodecCtx->ticks_per_frame;

				//----------------------------------------------------------------------------------------------------------------------------------------------------------

				SaveFrame(pFrameRGB, pCodecCtx->width, pCodecCtx->height, framePts);
				printf("Succeed to decode 1 frame!\n");

				if (framePts == 100) break;
			}


		}
		av_packet_unref(packet);
	}
}


void Decoder::flush(){
	//flush decoder
	//FIX: Flush Frames remained in Codec
	int ret, got_picture;
	while (packet->stream_index == videoStreamIndex) {
		ret = avcodec_decode_video2(pCodecCtx, pCurFrame, &got_picture, packet);
		if (ret < 0)
			break;
		if (!got_picture)
			break;

		// printf("Flush Decoder: Succeed to decode 1 frame!\n");
		av_packet_unref(packet);
	}
}

// bool AV_read_frame(AVFormatContext* pFormatCtx, AVCodecContext* pCodecCtx, AVFrame	*pFrame, int videoStreamIndex)
bool Decoder::AV_read_frame()
{
	int frame_done;

	while (av_read_frame(pFormatCtx, packet) >= 0) {
		if (packet->stream_index == videoStreamIndex) {
			int ret = avcodec_decode_video2(pCodecCtx, pCurFrame, &frame_done, packet);
			if (ret < 0){
				printf("Decode Error.\n");
				return false;
			}

			if (frame_done) {
				// ...
				// av->frame_id = packet.dts;
				av_packet_unref(packet);
				return true;
			}
		}
		av_packet_unref(packet);
	}
	return false;
}


bool Decoder::AV_seek(size_t frame, size_t delta)
{
	int64_t framePts = av_frame_get_best_effort_timestamp(pCurFrame);
	int frame_delta = frame - framePts;

	// flush();

	size_t seek_frame = std::max(int(0), int(frame) - int(delta));

	int res = 0;
	if (frame_delta < 0 || frame_delta > 5){
		res = av_seek_frame(pFormatCtx, videoStreamIndex, seek_frame, AVSEEK_FLAG_BACKWARD);
		// res = avformat_seek_file(pFormatCtx, videoStreamIndex, frame-100, frame, frame, AVSEEK_FLAG_BACKWARD);
		// pCodecCtx = pFormatCtx->streams[videoStreamIndex]->codec;
		if (res < 0)
			return false;
	}

	while (av_frame_get_best_effort_timestamp(pCurFrame) != frame){
		printf("\nFrame: %d", av_frame_get_best_effort_timestamp(pCurFrame));
		/*if (pCurFrame->pict_type == AV_PICTURE_TYPE_I)
		printf("\nIntra Frame: %d", av_frame_get_best_effort_timestamp(pCurFrame));*/
		AV_read_frame();
	}

	printf("\nSeeked to frame: %d", av_frame_get_best_effort_timestamp(pCurFrame));

	return true;
}



bool Decoder::AV_seekMs(int64_t tsms)
{
	// Convert time into frame number
	int64_t DesiredFrameNumber = av_rescale(tsms, pFormatCtx->streams[videoStreamIndex]->time_base.den, pFormatCtx->streams[videoStreamIndex]->time_base.num);
	DesiredFrameNumber /= 1000;

	int target = DesiredFrameNumber *
		(pFormatCtx->streams[videoStreamIndex]->time_base.den /
		pFormatCtx->streams[videoStreamIndex]->time_base.num) /
		(pFormatCtx->streams[videoStreamIndex]->codec->time_base.den /
		pFormatCtx->streams[videoStreamIndex]->codec->time_base.num)*
		pCodecCtx->ticks_per_frame;

	// I read this approach is most correct
	return AV_seek(target); 
	// but this give same results of seeking with command line ffmpeg application 
	// return AV_seek(DesiredFrameNumber); 
}


void Decoder::SaveCurFrame() {
	sws_scale(img_convert_ctx, (const uint8_t* const*)pCurFrame->data, pCurFrame->linesize, 0, pCodecCtx->height, pFrameRGB->data, pFrameRGB->linesize);
	SaveFrame(pFrameRGB, pCodecCtx->width, pCodecCtx->height, pCurFrame->best_effort_timestamp, "frameSeek");
}

void Decoder::SaveFrame(AVFrame *pFrame, int width, int height, int iFrame, const std::string filePrefix) {
	FILE *pFile;
	std::string szFilename;
	int  y;

	// Open file
	// sprintf(const_cast<char*>(szFilename.data()), "images\frame%d.ppm", iFrame);
	szFilename = filePrefix;
	szFilename += std::to_string(iFrame) + ".ppm";
	char* aaa = const_cast<char*>(szFilename.data());
	pFile = fopen(szFilename.c_str(), "wb");
	if (pFile == NULL)
		return;

	// Write header
	fprintf(pFile, "P6\n%d %d\n255\n", width, height);

	// Write pixel data
	for (y = 0; y < height; y++)
		fwrite(pFrame->data[0] + y*pFrame->linesize[0], 1, width * 3, pFile);

	// Close file
	fclose(pFile);
}

Decoder::~Decoder(){
	sws_freeContext(img_convert_ctx);
	av_frame_free(&pFrameRGB);
	av_frame_free(&pCurFrame);
	avcodec_close(pCodecCtx);
	avformat_close_input(&pFormatCtx);
}