#ifndef __FFMPEG_VIDEO_WRITER
#define __FFMPEG_VIDEO_WRITER

#include "CircularBuffer.hpp"
#include <opencv2/opencv.hpp>
extern "C"{
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavutil/imgutils.h>
}
#include <iostream>
#include <thread>


#pragma comment(lib, "libavcodec/avcodec.lib")
#pragma comment(lib, "libavutil/avutil.lib")
#pragma comment(lib, "libavformat/avformat.lib")
#pragma comment(lib, "libswscale/swscale.lib")
#pragma comment(lib, "libswresample/swresample.lib")
//#pragma comment(lib, "opencv_world320.lib")

using namespace std;
using namespace cv;

struct AVData{
	unsigned char* data;
	void* addObject;
	AVData(cv::Mat &frame){
		this->addObject = nullptr;
		auto size = frame.elemSize();
		this->data = new unsigned char[frame.total() * frame.elemSize()];
		memcpy(this->data, frame.data, frame.total() * frame.elemSize());	
	}

	~AVData(){
		delete[] data;
	}
};

#define STREAM_PIX_FMT   AV_PIX_FMT_YUV420P 
#define SCALE_FLAGS SWS_BICUBIC

typedef struct OutputStream {
	AVStream *st = nullptr;
	AVCodecContext *enc = nullptr;

	int64_t next_pts = 0;
	int samples_count = 0;

	AVFrame *frame = nullptr;
	AVFrame *tmp_frame = nullptr;

	float t, tincr, tincr2;

	struct SwsContext *sws_ctx = nullptr;
	struct SwrContext *swr_ctx = nullptr;

	OutputStream(): next_pts(0), samples_count(0), t(0), tincr(0), tincr2(0) {
		st = nullptr;
		frame = nullptr;
		tmp_frame = nullptr;
		sws_ctx = nullptr;
		swr_ctx = nullptr;
	}

} OutputStream;

class FFMpegVideoWriter;
void WriterFunctor(FFMpegVideoWriter *w);

class FFMpegVideoWriter{
private:
	enum AVPixelFormat src_pixel_format = AV_PIX_FMT_BGR24;

	std::string m_filename;
	volatile bool isStop;
	int m_fps;
	cv::Size m_frame_size;
	CircularBufferrr<AVData*> *buffer_ptr;

	OutputStream video_st, audio_st;
	AVOutputFormat *fmt;
	AVFormatContext *oc;
	AVCodec *audio_codec, *video_codec;
	std::thread m_thread;
	int m_data_type;
	
	void close_stream(AVFormatContext *oc, OutputStream *ost);
	int write_video_frame(AVFormatContext *oc, OutputStream *ost);
	AVFrame* get_video_frame(OutputStream *ost);
	void fill_yuv_image(AVFrame *pict, int frame_index, int width, int height);
	void open_video(AVFormatContext *oc, AVCodec *codec, OutputStream *ost, AVDictionary *opt_arg);
	AVFrame *alloc_picture(enum AVPixelFormat pix_fmt, int width, int height);
	void add_stream(OutputStream *ost, AVFormatContext *oc, AVCodec **codec, enum AVCodecID codec_id);

	int write_frame(AVFormatContext *fmt_ctx, const AVRational *time_base, AVStream *st, AVPacket *pkt)
	{
		/* rescale output packet timestamp values from codec to stream timebase */
		av_packet_rescale_ts(pkt, *time_base, st->time_base);
		pkt->stream_index = st->index;

		/* Write the compressed frame to the media file. */
		return av_interleaved_write_frame(fmt_ctx, pkt);
	}

public:

	int p_align_size = 1;

	void init_video_writer();

	FFMpegVideoWriter(CircularBufferrr<AVData*> *buf_ptr, std::string filename, int fps, cv::Size size) :
		m_filename(filename), isStop(false), m_fps(fps), m_frame_size(size) {
		buffer_ptr = new CircularBufferrr<AVData*>(5000);//buf_ptr;

		this->init_video_writer();
		m_thread = std::thread(WriterFunctor, this);
	}

	~FFMpegVideoWriter(){
		if (!isStop)
			this->stop_loop();
		
	}


	void push(cv::Mat &frame) {
		while (buffer_ptr->size() > buffer_ptr->capacity() - 10)
			std::this_thread::sleep_for(std::chrono::milliseconds(25));
	
		buffer_ptr->push(new AVData(frame));
	}

	void main_loop(){
		isStop = false;

		while (!isStop) {
			if (buffer_ptr->size() > 2){	
				write_video_frame(oc, &video_st);

    
            }

			//encode_audio = !write_audio_frame(oc, &audio_st);

    
        }

		while (buffer_ptr->size() > 0)
			write_video_frame(oc, &video_st);

	
        /* Write the trailer, if any. The trailer must be written before you
		* close the CodecContexts open when you wrote the header; otherwise
		* av_write_trailer() may try to use memory that was freed on

         * av_codec_close(). */
		av_write_trailer(oc);

		/* Close each codec. */
		if (m_data_type == 1)
			close_stream(oc, &video_st);
		if (m_data_type == 2)
			close_stream(oc, &audio_st);

		if (!(fmt->flags & AVFMT_NOFILE))
			/* Close the output file. */
			avio_closep(&oc->pb);

		/* free the stream */
		avformat_free_context(oc);

		std::cout << "Stop record.\n";

    }


	void stop_loop(){
		isStop = true;
                std::cout << "FFmpeg destruct"<< std::endl;
                std::cout << "Buffer contains:" << buffer_ptr->size() << " elements"<<endl;
		m_thread.join();
	}
	

};

#endif
