/*

   ShotManager (2015 x64)
		Luca Gallinari
		Datio Stabili
		Marco Ravazzini

	IMPORTANT:
	This wrapper is a porting of the QTFFmpegWrapper made by Daniel Roggen.
	Changes:
		FFmpeg git-a254452 2011-09-19	->	FFmpeg git-0671dc5 2015-07-22
		32 bit FFmpeg libs				->	64 bit FFmpeg libs
		Internal console for debugging	->	qDebug() for debugging

	We made a lot of changes and now it supports many formats and it works better.

	--------------------------------------------------------------------------------
   
	QTFFmpegWrapper
	Copyright (C) 2009,2010:
		Daniel Roggen, droggen@gmail.com
	All rights reserved.

	Redistribution and use in source and binary forms, with or without modification, 
	are permitted provided that the following conditions are met:

		1.	Redistributions of source code must retain the above copyright notice, this 
			list of conditions and the following disclaimer.
		2.	Redistributions in binary form must reproduce the above copyright notice, 
			this list of conditions and the following disclaimer in the documentation 
			and/or other materials provided with the distribution.

	THIS SOFTWARE IS PROVIDED BY COPYRIGHT HOLDERS ``AS IS'' AND ANY EXPRESS OR IMPLIED 
	WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY 
	AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE FREEBSD 
	PROJECT OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, 
	EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT 
	OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) 
	HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, 
	OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS 
	SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "QVideoDecoder.h"

#include <locale>
#include <algorithm>
#include <QMessageBox>

#define __STDC_CONSTANT_MACROS


/*! \brief Constructor
*
*   Constructor
*/
QVideoDecoder::QVideoDecoder()
{
	InitVars();
	initCodec();
}
/*! \brief Constructor
*
*   Constructor
*/
QVideoDecoder::QVideoDecoder(const QString file)
{
	InitVars();
	initCodec();

	ok = openFile(file.toStdString().c_str());
}

/*! \brief Destroyer
*
*   Destroyer
*/
QVideoDecoder::~QVideoDecoder()
{
	close();
	InitVars();
}

/*! \brief Codec initialization
*
*   Codec initialization
*/
void QVideoDecoder::initCodec()
{
	av_register_all();
	avformat_network_init();
	avcodec_register_all();

	qDebug() << "License: " << avformat_license();
	qDebug() << "AVCodec version: " << avformat_version();
	qDebug() << "AVFormat configuration: " << avformat_configuration();
}

/*! \brief Variables initialization
*
*   Variables initialization
*/
void QVideoDecoder::InitVars()
{
	ok=false;
	flushed = false;
	pFormatCtx = avformat_alloc_context();
	pCodecCtx = 0;
	pCodec=0;
	pFrame=0;
	pFrameRGB=0;
	buffer=0;
	img_convert_ctx=0;
	millisecondbase = { 1, 1000 };
	maxFrameNumber = 0;
	maxCurrentFrameNumber = 0;
}

/*! \brief Close the file and reset all variables
*
*   Close the file and reset all variables
*/
void QVideoDecoder::close()
{
	/*if(!ok)
		return;	*/

	flushed = false;

	// Free the RGB image
	if (buffer)
		// delete [] buffer;
		av_free(buffer);
		
	// Free the YUV frame
	if(pFrame)
		//av_free(pFrame);
		av_frame_free(&pFrame);

	// Free the RGB frame
	if(pFrameRGB)
		//av_free(pFrameRGB);
		av_frame_free(&pFrameRGB);

	// Close the codec
	if(pCodecCtx)
		avcodec_close(pCodecCtx);

	// Close the video file
	if(pFormatCtx)
		avformat_close_input(&pFormatCtx);

	sws_freeContext(img_convert_ctx);
	avformat_close_input(&pFormatCtx);
}


int QVideoDecoder::init_frames(){
	pFrame = av_frame_alloc();
	pFrameRGB = av_frame_alloc();
	if (pFrameRGB == NULL)
		return -1;
	
	buffer = (uint8_t*)av_malloc(av_image_get_buffer_size(AV_PIX_FMT_RGB24, pCodecCtx->width, pCodecCtx->height, 1));
	av_image_fill_arrays(pFrameRGB->data, pFrameRGB->linesize, buffer, AV_PIX_FMT_RGB24, pCodecCtx->width, pCodecCtx->height, 1);

	packet = (AVPacket *)av_malloc(sizeof(AVPacket));

	img_convert_ctx = sws_getContext(pCodecCtx->width, pCodecCtx->height, pCodecCtx->pix_fmt,
		pCodecCtx->width, pCodecCtx->height, AV_PIX_FMT_RGB24, SWS_BICUBIC, NULL, NULL, NULL);

	return 0;
}


/*! \brief Open a file and setup all variables
*
*   Open a file and setup all variables
*	@param filename path of the file to open
*	@return success or not
*/
bool QVideoDecoder::openFile(const QString filename)
{
	// Close last video..
	close();

	/*LastLastFrameTime = INT_MIN;       // Last last must be small to handle the seek well
	LastFrameTime = 0;
	LastLastFrameNumber = INT_MIN;
	LastFrameNumber = 0;
	LastIdealFrameNumber = 0;
	LastFrameOk = false;*/
	lastDecodedFrame = 0;
	maxFrameNumber = 0;
	maxCurrentFrameNumber = 0;
	
	if (avformat_open_input(&pFormatCtx, filename.toStdString().c_str(), NULL, NULL) != 0){
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

	init_frames();
	
	// Set variables
	// this is the part added from other students
	path			= filename;
	type			= QString(pFormatCtx->iformat->name);
	duration		= pFormatCtx->duration;
	baseFrameRate	= av_q2d(pFormatCtx->streams[videoStreamIndex]->r_frame_rate);
	frameMSec		= 1000 / baseFrameRate;
	
	frameMSecReal =
			(double)(pFormatCtx->streams[videoStreamIndex]->time_base.den /
			pFormatCtx->streams[videoStreamIndex]->time_base.num) /
			(double)(pFormatCtx->streams[videoStreamIndex]->codec->time_base.den /
			(double)pFormatCtx->streams[videoStreamIndex]->codec->time_base.num)*
			pCodecCtx->ticks_per_frame;

	baseFRateReal	= 1000 / (double) frameMSecReal;
	timeBaseRat		= pFormatCtx->streams[videoStreamIndex]->time_base;
	timeBase		= av_q2d(timeBaseRat);
	w				= pCodecCtx->width;
	h				= pCodecCtx->height;
	
	ok = true;
	
	getFirstPacketInformation();
	//AV_read_frame();

	dumpFormat(0);

	maxFrameNumber = getNumFrames();
	maxCurrentFrameNumber = getInternalFrameNumber(maxFrameNumber);

	return true;
}


bool QVideoDecoder::flush(const int num){
	//flush decoder
	//FIX: Flush Frames remained in Codec
	int64_t fNum = num;
	int ret, got_picture;
	flushed = true;
	while (packet->stream_index == videoStreamIndex && (num < 0 || fNum>0)) {
		ret = avcodec_decode_video2(pCodecCtx, pFrame, &got_picture, packet);
		if (ret < 0)
			return false;
		if (!got_picture)
			return false;;

		--fNum;

		convertFrame();
		// printf("Flush Decoder: Succeed to decode 1 frame!\n");
		av_packet_unref(packet);
	}
	return true;
}


/*! \brief Try to retrieve usefull inforamtion from the first packet
*
*   This is the only way that i found so far to retrieve the ms of a single frame
*	in mkv video file. One of frameMsec and frameMsecReal is used as a single frame
*	duration but i could not find any solution, except this, to find which is correct.
*	And 
*	@return true on success, false when the file is not supported
*/
bool QVideoDecoder::getFirstPacketInformation()
{
	bool done = false;
	while (!done) {

		// Read a frame
		if (av_read_frame(pFormatCtx, packet)<0)
			return false;	// end of stream? impossible     

		// Packet of the video stream?
		if (packet->stream_index == videoStreamIndex) {

			int frameFinished;
			if(avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished, packet)<0)
				return false;

			// Frame is completely decoded?
			if (frameFinished) {

				baseFrameNumber = av_frame_get_best_effort_timestamp(pFrame);
				frameDuration = packet->duration;
				firstDts = packet->dts;
				startTs = packet->dts;
				done = true;

				double frameMSecRealRounded = std::round(std::round(frameMSecReal * 100) / 100);
				if (frameDuration != frameMSecRealRounded){
					frameDuration = frameMSecRealRounded;
				}

				// Convert and save the frame
				convertFrame();

				if (pFormatCtx->streams[videoStreamIndex]->first_dts == AV_NOPTS_VALUE) {
					/*	TODO: I could not find any solution when the firstDts value is not valid.
					*	If you try to seek to 0 it will fail. Maybe in future ffmpeg version
					*	this will be resolved.
					*/
					return false;
				}
			}  // frameFinished
		}  // stream_index==videoStream

		av_packet_unref(packet);
	}
	
	// avcodec_flush_buffers(pCodecCtx);
	return true;
}


void QVideoDecoder::convertFrame(){
	currentFrame = av_frame_get_best_effort_timestamp(pFrame);
	lastDecodedFrame = currentFrame;

	// Convert and save the frame
	img_convert_ctx = sws_getCachedContext(
		img_convert_ctx, w, h,
		pCodecCtx->pix_fmt, w, h,
		AV_PIX_FMT_RGB24, SWS_BICUBIC, NULL, NULL, NULL
		);

	if (img_convert_ctx == NULL) {
		qDebug() << "Cannot initialize the conversion context!";
		return;
	}
	sws_scale(img_convert_ctx, (const uint8_t* const*)pFrame->data, pFrame->linesize, 0, pCodecCtx->height, pFrameRGB->data, pFrameRGB->linesize);

	LastFrame = QImage(w, h, QImage::Format_RGB888);

	for (int y = 0; y < h; y++)
		memcpy(LastFrame.scanLine(y), pFrameRGB->data[0] + y * pFrameRGB->linesize[0], w * 3);
}


bool QVideoDecoder::AV_read_frame()
{
	int frame_done;

	while (av_read_frame(pFormatCtx, packet) >= 0) {
		if (packet->stream_index == videoStreamIndex) {
			int ret = avcodec_decode_video2(pCodecCtx, pFrame, &frame_done, packet);
			if (ret < 0){
				printf("Decode Error.\n");
				return false;
			}

			if (packet->duration != frameDuration){
				QMessageBox::critical(NULL, "Codec Error", "Video format is not supported. Frame rate is not costant");
				ok = false;
				return false;
			}

			if (frame_done) {				
				convertFrame();
				av_packet_unref(packet);
				return true;
			}
		}
		av_packet_unref(packet);
	}
	// end of stream?
	return flush(1);
}


/*! \brief Seek the given frame and decode it
*
*   Decodes the video stream until the first frame with number larger or equal 
*	than 'idealFrameNumber' is found.
*	TODO: if we already passed the wanted frame number?
*	@param idealFrameNumber desired frame number
*	@return success or not
*/
bool QVideoDecoder::decodeSeekFrame(const qint64 idealFrameNumber)
{
	qint64 f, t;
	bool done = false;

	currentFrame = av_frame_get_best_effort_timestamp(pFrame);

	// If the last decoded frame satisfies the time condition we return it
	if (idealFrameNumber != -1 && idealFrameNumber < currentFrame){
		seekFrameCorrect(idealFrameNumber);
	}   

	while (!done) {

		// Read a frame
		if (av_read_frame(pFormatCtx, packet)<0)
			return false;	// end of stream?            

		// Packet of the video stream?
		if (packet->stream_index==videoStreamIndex) {

			int frameFinished;
			avcodec_decode_video2(pCodecCtx,pFrame,&frameFinished,packet);

			// Frame is completely decoded?
			if (frameFinished) {

				// Calculate real frame number and time based on the format
				if (type == "mpeg" || type == "asf") {
					f = (long)((packet->dts - startTs) * (baseFrameRate*timeBase) + 0.5);
					t = av_rescale_q(packet->dts - startTs, timeBaseRat, millisecondbase);
				}
				else if (type.indexOf("mp4") != -1) {
					f = (long)((packet->dts + firstDts) * (baseFrameRate*timeBase) + 0.5);
					t = av_rescale_q(packet->dts + firstDts, timeBaseRat, millisecondbase);
				}
				else if (type == "matroska,webm") {
					// t = av_frame_get_best_effort_timestamp(pFrame);
					// f = round(t / frameMSec);
					t = av_rescale_q(packet->dts - firstDts, timeBaseRat, millisecondbase);
					f = round(t / frameMSec);
				}
				else { // avi
					f = packet->dts;
					t = av_rescale_q(packet->dts, timeBaseRat, millisecondbase);
				}

				currentFrame = av_frame_get_best_effort_timestamp(pFrame);
				lastDecodedFrame = currentFrame;
				qDebug() << "id:" << idealFrameNumber;
				qDebug() << "f:" << f;
				qDebug() << "t:" << t;
				qDebug() << "dur:" << packet->duration;
				qDebug() << "dts:" << packet->dts << endl;

				// this is the desired frame or at least one just after it
				if (idealFrameNumber == -1 || currentFrame >= idealFrameNumber)
				{
					// Convert and save the frame
					img_convert_ctx = sws_getCachedContext(
						img_convert_ctx, w, h, 
						pCodecCtx->pix_fmt, w, h, 
						AV_PIX_FMT_RGB24, SWS_BICUBIC, NULL, NULL, NULL
					);

					if (img_convert_ctx == NULL) {
						qDebug() << "Cannot initialize the conversion context!";
						return false;
					}
					sws_scale(img_convert_ctx, (const uint8_t* const*)pFrame->data, pFrame->linesize, 0, pCodecCtx->height, pFrameRGB->data, pFrameRGB->linesize);

					LastFrame = QImage(w, h, QImage::Format_RGB888);

					for (int y=0; y < h; y++)
						memcpy(LastFrame.scanLine(y), pFrameRGB->data[0] + y * pFrameRGB->linesize[0], w * 3);

					//LastFrameOk = true;
					done = true;
				} // frame of interes
			}  // frameFinished
		}  // stream_index==videoStream

		av_packet_unref(packet);
	}
	return done;
}

/*! \brief Seek the next frame
*
*   Seek the next frame.
*	@return success or not
*   @see seekFrame()
*   @see seekPrevFrame()
*/
bool QVideoDecoder::seekNextFrame()
{
	//bool ret = decodeSeekFrame(currentFrame + 1);
	//bool ret = seekFrameCorrect(currentFrame + 1);
	bool ret = AV_read_frame();

	if (ret)
		currentFrame = av_frame_get_best_effort_timestamp(pFrame);
		//LastIdealFrameNumber = av_frame_get_best_effort_timestamp(pFrame);
	//else
	//	LastFrameOk=false;
	return ret;
}

/*! \brief Seek the previous frame
*
*   Seek the previous frame.
*	@return success or not
*   @see seekFrame()
*   @see seekNextFrame()
*/
bool QVideoDecoder::seekPrevFrame()
{
	currentFrame = av_frame_get_best_effort_timestamp(pFrame);

	bool ret = seekFrameCorrect(currentFrame - 1);

	//if (!ret)
	//	LastFrameOk = false;      
	return ret;
}

/*! \brief Seek the closest frame to the given time
*
*   Seek the closest frame to the given time.
*	@param tsms time in milliseconds
*	@return success or not
*   @see seekFrame()
*/
bool QVideoDecoder::seekMs(const qint64 tsms)
{
   if(!ok)
	  return false;
	
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
	return seekFrameCorrect(target);
	// but this give same results of seeking with command line ffmpeg application 
	// return AV_seek(DesiredFrameNumber); 
}


qint64 QVideoDecoder::getInternalFrameNumber(qint64 idealFrameNumber){
	return std::round(idealFrameNumber*frameDuration + baseFrameNumber);
}


qint64 QVideoDecoder::getExternalFrameNumber(qint64 internalFrameNumber){
	return std::round((internalFrameNumber - baseFrameNumber) / frameDuration); 
}


bool QVideoDecoder::seekFrame(const qint64 idealFrameNumber){
	qint64 frame = idealFrameNumber;
	if (maxFrameNumber == 0)
		maxFrameNumber = getNumFrames();

	if (idealFrameNumber >= maxFrameNumber)
		//frame = maxFrameNumber-1;
		frame = maxFrameNumber;
		//return false;
	
	if(frameDuration>1)
		return seekFrameCorrect(getInternalFrameNumber(frame), 10 * frameDuration);
	else
		return seekFrameCorrect(getInternalFrameNumber(frame));
}


/*! \brief Seek the desired frame
*
	TODO: case when idelFrameNumber less than 0
*   Seek and retrieve desired frame by number.
*	@param idealFrameNumber number of the desired frame
*	@return success or not
*   @see seekToAndGetFrame()
*   @see seekMs()
*/
bool QVideoDecoder::seekFrameCorrect(const qint64 internalFrame, const qint64 delta)
{
	qint64 frame = internalFrame;
	if (maxCurrentFrameNumber == 0)
		maxCurrentFrameNumber = getInternalFrameNumber(getNumFrames());

	if (internalFrame >= maxCurrentFrameNumber)
		frame = getInternalFrameNumber(getNumFrames()-1);

	// end of stream
	if (flushed && getInternalFrameNumber(getNumFrames() - 1) == currentFrame)
		return false;

	int64_t framePts = currentFrame;
	int64_t frame_delta = frame - framePts;
	frame_delta /= frameDuration;

	int64_t seek_frame = std::max(int(0), int(frame) - int(delta));
	
	int res = 0;
	if (frame_delta < 0 || frame_delta > 5){
		res = av_seek_frame(pFormatCtx, videoStreamIndex, seek_frame, AVSEEK_FLAG_BACKWARD);
		if (res < 0)
			return false;
	}
	/*res = av_seek_frame(pFormatCtx, videoStreamIndex, seek_frame, AVSEEK_FLAG_BACKWARD);
	if (res < 0)
		return false;
	*/
	while (av_frame_get_best_effort_timestamp(pFrame) != frame){
		// printf("\nFrame: %d", av_frame_get_best_effort_timestamp(pCurFrame));
		qDebug() << "Frame: " << av_frame_get_best_effort_timestamp(pFrame);
		/*if (pCurFrame->pict_type == AV_PICTURE_TYPE_I)
		printf("\nIntra Frame: %d", av_frame_get_best_effort_timestamp(pCurFrame));*/
		AV_read_frame();
	}

	currentFrame = av_frame_get_best_effort_timestamp(pFrame);
	// printf("\nSeeked to frame: %d", av_frame_get_best_effort_timestamp(pFrame));
	qDebug() << "Seeked to frame: " << currentFrame;

	// avcodec_flush_buffers(pCodecCtx);
	
	//LastFrameOk = false;

	// decode
	return true;
}

/*! \brief Corrects the seeking operation
*
*   Corrects the seeking operation to a "key frame" because this varies from 
*	format and format. This is based on a prediction so we have to add a margin
*	of error that can allow us to ..TODO
*	@param idealFrameNumber number of the desired frame
*	@return success or not
*   @see seekFrame()
*/
bool QVideoDecoder::correctSeekToKeyFrame(const qint64 idealFrameNumber)
{
	return true;
}

/*! \brief Seek and retrieve desired frame
*
*   Seek and retrieve desired frame by number.
*	@param idealFrameNumber number of the desired frame
*	@param img where it stores the frame
*	@param frameNum where it stores the frame number
*	@param frameTime where it stores the frame time
*	@return success or not
*/
bool QVideoDecoder::seekToAndGetFrame(const qint64 idealFrameNumber, QImage&img, qint64 *frameNum, qint64 *frameTime)
{
	if (!seekFrame(idealFrameNumber))
		return false;
	return getFrame(img, frameNum, frameTime);
}



/***************************************
*********        GETTERS      *********
***************************************/

/*! \brief A file is loaded
*
*   A file is loaded
*	@return video loaded successfully
*/
bool QVideoDecoder::isOk()
{
	return ok;
}

/*! \brief Reached end of file
*
*   Reached end of file
*	@return video loaded successfully
*/
bool QVideoDecoder::isFlushed()
{
	return flushed;
}


/*! \brief Get last loaded frame
*
*   Get last loaded frame
*	@param img where it stores the frame
*	@param frameNum where it stores the frame number
*	@param frameTime where it stores the frame time
*	@return last frame was valid or not
*/
bool QVideoDecoder::getFrame(QImage &img, qint64 *frameNum, qint64 *frameTime)
{
	img = LastFrame;

	if (frameNum)
		//*frameNum = currentFrame-baseFrameNumber;
		*frameNum = getExternalFrameNumber(lastDecodedFrame);
	if (frameTime){
		//*frameTime = (currentFrame - baseFrameNumber)*baseFrameRate;
		qint64 DesiredFrameNumber = getExternalFrameNumber(lastDecodedFrame);
		qint64 time = DesiredFrameNumber * frameMSec;
		*frameTime = time;
	}
	return true;
}

/*! \brief Get last loaded frame "CODEC number"
*
*   Get last loaded frame "CODEC number", "CODEC number" because codecs and
*	formats use a different type of "number", some uses timestamps and some
*	normal integers.
*	@return last loaded frame number
*/
qint64 QVideoDecoder::getActualFrameNumber()
{
	//if (!isOk())
	//	return -1;
	return getExternalFrameNumber(lastDecodedFrame);
}

/*! \brief Get last loaded frame "IDEAL number"
*
*   Get last loaded frame "IDEAL number", "IDEAL number" because codecs and
*	formats use a different type of "number", some uses timestamps and some
*	normal integers.
*	@return last loaded frame number
*/
qint64 QVideoDecoder::getIdealFrameNumber()
{
	//if (!isOk())
	//	return -1;
	return getExternalFrameNumber(currentFrame);
}

/*! \brief Get last loaded frame time milliseconds
*
*   Get last loaded frame time milliseconds.
*	@return last loaded frame time
*/
qint64 QVideoDecoder::getFrameTime()
{
	if (!isOk())
		return -1;
	qint64 DesiredFrameNumber = getExternalFrameNumber(lastDecodedFrame);
	qint64 time = DesiredFrameNumber * frameMSec;
	return time;
	// return (lastDecodedFrame - baseFrameNumber) / frameDuration *baseFrameRate;
}

/*! \brief Get frame number by time
*
*   Get the number of the closest frame to the given time
*	@return frame number
*/
qint64 QVideoDecoder::getNumFrameByTime(const qint64 tsms)
{
	if (!ok)
		return false;
	if (tsms <= 0)
		return 0;
	//return round(tsms / frameMSec);
	
	int64_t DesiredFrameNumber = av_rescale(tsms, pFormatCtx->streams[videoStreamIndex]->time_base.den, pFormatCtx->streams[videoStreamIndex]->time_base.num);
	DesiredFrameNumber /= 1000;

	/*qint64 target = DesiredFrameNumber *
		(pFormatCtx->streams[videoStreamIndex]->time_base.den /
		pFormatCtx->streams[videoStreamIndex]->time_base.num) /
		(pFormatCtx->streams[videoStreamIndex]->codec->time_base.den /
		pFormatCtx->streams[videoStreamIndex]->codec->time_base.num)*
		pCodecCtx->ticks_per_frame;*/

	return round(std::max(0.0, (DesiredFrameNumber - baseFrameNumber) / frameDuration));
}

/*! \brief Get video duration in milliseconds
*
*   Get video duration in milliseconds.
*	@return video length in ms
*/
qint64 QVideoDecoder::getVideoLengthMs()
{
	if (!isOk())
		return -1;

	qint64 secs = pFormatCtx->duration / AV_TIME_BASE;
	qint64 us = pFormatCtx->duration % AV_TIME_BASE;
	return secs * 1000 + us / 1000;
}

/*! \brief Get number of frames (Not accurate with some formats)
*
*   Get number of frames based on video duration and frame rate. Some containers
*	save a wrong value for duration and so the number of frames could be not so
*	accurate.
*	@return number of frams
*/
qint64 QVideoDecoder::getNumFrames()
{
	//return round(getVideoLengthMs() * (baseFrameRate / 1000.0));
	return getNumFrameByTime(getVideoLengthMs());
}

/*! \brief Get video path
*
*	Retrieve video path
*/
QString QVideoDecoder::getPath() {
	return path;
}

/*! \brief Get video type
*
*	Retrieve video type
*/
QString QVideoDecoder::getType() {
	return type;
}

/*! \brief Get video time base as AVRational
*
*   Get video time base, it's the base time that the container uses
*	@return video time base
*/
AVRational QVideoDecoder::getTimeBaseRat()
{
	return timeBaseRat;
}

/*! \brief Get video time base as double
*
*   Get video time base, it's the base time that the container uses
*	@return video time base
*/
double QVideoDecoder::getTimeBase()
{
	return timeBase;
}

/*! \brief Get video frame rate
*
*	Retrieve video frame rate
*/
double QVideoDecoder::getFrameRate() {
	return baseFrameRate;
}

/*! \brief Get video frame ms (theorycal)
*
*	Retrieve video frame ms (theorycal)
*/
double QVideoDecoder::getFrameMsec() {
	return frameMSec;
}

/*! \brief Get video frame ms (real)
*
*	Retrieve video frame ms (real). Actually only "matroska" and "mp4" files
*	have a different theorycal and real frame msec.
*/
double QVideoDecoder::getFrameMsecReal() {
	return frameMSecReal;
}

/*! \brief Get frame width
*
*	Get frame width
*/
int QVideoDecoder::getFrameWidth() {
	return w;
}

/*! \brief Get frame height
*
*	Get frame height
*/
int QVideoDecoder::getFrameHeight() {
	return h;
}

/*! \brief Get video bitrate
*
*	Get video bitrate
*/
QString QVideoDecoder::getBitrate() {
	return (pFormatCtx->bit_rate ? QString::number((int)(pFormatCtx->bit_rate / 1000)).append(" kb/s") : "N / A");
}

/*! \brief Get string of programs used to make the video
*
*	Get string of programs used to make the video
*/
QString QVideoDecoder::getProgramsString()
{
	QString s;
	// Programs
	if (pFormatCtx->nb_programs) {
		unsigned int j, total = 0;
		for (j = 0; j<pFormatCtx->nb_programs; j++) {
			AVDictionaryEntry *name = av_dict_get(pFormatCtx->programs[j]->metadata, "name", NULL, 0);
			s.push_back(QString("%1 %2 \n").arg(pFormatCtx->programs[j]->id).arg(" ").arg(name ? name->value : ""));
			total += pFormatCtx->programs[j]->nb_stream_indexes;
		}
		if (total < pFormatCtx->nb_streams)
			s.push_back("None");
	}
	else{
		s.push_back("None");
	}
	return s;
}

/*! \brief Get string of metadatas presents in the video
*
*	Get string of metadatas presents in the video
*/
QString QVideoDecoder::getMetadataString()
{
	QString s;
	// Programs
	if (pFormatCtx->metadata) {
		AVDictionaryEntry *tag = NULL;
		while ((tag = av_dict_get(pFormatCtx->metadata, "", tag, AV_DICT_IGNORE_SUFFIX))) {
			s.push_back(QString("%1: %2 \n").arg(tag->key).arg(tag->value));
		}
	}
	else{
		s.push_back("None");
	}
	return s;
}

/***************************************
*********        HELPERS      *********
***************************************/

/*! \brief Save a frame as PPM image
*
*   Save a frame as PPM image. Usefull for debugging.
*	@param pFrame the frame
*	@param width frame width
*	@param height frame height
*	@param iFrame frame number
*/
void QVideoDecoder::saveFramePPM(const AVFrame *pFrame, const int width, const int height, const int iFrame)
{
	FILE *pFile;
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
	for (y = 0; y<height; y++)
		fwrite(pFrame->data[0] + y*pFrame->linesize[0], 1, width * 3, pFile);

	// Close file
	fclose(pFile);
}

/*! \brief Output video's informations
*
*   Write video's information in the stdout. Usefull for debugging.
*	@param path file path
*	@param is_output writing or reading the file?
*/
void QVideoDecoder::dumpFormat(const int is_output) 
{
	qDebug() << (is_output ? "Output" : "Input");
	qDebug() << "File: " << path;
	qDebug() << "Stream: " << videoStreamIndex;
	qDebug() << "Type: " << (is_output ? pFormatCtx->oformat->name : type);
	qDebug() << "AV_TIME_BASE: " << AV_TIME_BASE;

	// General infos
	if (!is_output) {
		qDebug() << "Time Base: " << timeBase;
		qDebug() << "Start: " << startTs;
		qDebug() << "First Dts: " << firstDts;
		qDebug() << "FPS: " << baseFrameRate;
		qDebug() << "Frame ms: " << frameMSec;
		qDebug() << "Special ms: " << frameMSecReal;
		qDebug() << "Frame duration ms: " << frameDuration;
		qDebug() << "Frame w: " << w;
		qDebug() << "Frame h: " << h;
		qDebug() << "Number of frames: " << getNumFrames();
		qDebug() << "Duration: " << duration << " us";

		int hours, mins, secs, us;
		secs = duration / AV_TIME_BASE;
		us = duration % AV_TIME_BASE;
		mins = secs / 60;
		secs %= 60;
		hours = mins / 60;
		mins %= 60;
		qDebug() << "\t" << hours << "h " << mins << "m " << secs << "s " << (100 * us) / AV_TIME_BASE;
		qDebug() << "Bitrate: " << (pFormatCtx->bit_rate ? QString::number((int) (pFormatCtx->bit_rate / 1000)).append(" kb/s") : "N / A");
	}
	// Programs
	qDebug() << "Program: \n" << getProgramsString();
	// Metadata
	qDebug() << "Metadata: \n" << getMetadataString();
}

