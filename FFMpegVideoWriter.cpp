#include "FFMpegVideoWriter.h"

void WriterFunctor(FFMpegVideoWriter *w) {
	w->main_loop();
}


void FFMpegVideoWriter::init_video_writer(){
	int ret;
	AVDictionary *opt = NULL;
	int i;

	av_register_all();

	/* allocate the output media context */
	std::string tmp_filename = m_filename;

	avformat_alloc_output_context2(&oc, NULL, NULL, tmp_filename.c_str());
	if (!oc) {
		printf("Could not deduce output format from file extension: using MPEG.\n");
		avformat_alloc_output_context2(&oc, NULL, "mpeg", tmp_filename.c_str());
	}

	fmt = oc->oformat;

	/* Add the audio and video streams using the default format codecs
	* and initialize the codecs. */
	if (fmt->video_codec != AV_CODEC_ID_NONE) {
		add_stream(&video_st, oc, &video_codec, fmt->video_codec);
		m_data_type = 1;
	}
	//if (fmt->audio_codec != AV_CODEC_ID_NONE) {
	//	add_stream(&audio_st, oc, &audio_codec, fmt->audio_codec);
	//	m_data_type = 2;
	//}

	/* Now that all the parameters are set, we can open the audio and
	* video codecs and allocate the necessary encode buffers. */
	if (m_data_type == 1)
		open_video(oc, video_codec, &video_st, opt);

//	if (m_data_type == 2)
//		open_audio(oc, audio_codec, &audio_st, opt);

	av_dump_format(oc, 0, tmp_filename.c_str(), 1);

	/* open the output file, if needed */
	if (!(fmt->flags & AVFMT_NOFILE)) {
		ret = avio_open(&oc->pb, tmp_filename.c_str(), AVIO_FLAG_WRITE);
		if (ret < 0) {
		}
	}

	/* Write the stream header, if any. */
	ret = avformat_write_header(oc, &opt);
	if (ret < 0) {
	}
}

void FFMpegVideoWriter::close_stream(AVFormatContext *oc, OutputStream *ost)
{
	avcodec_free_context(&ost->enc);
	av_frame_free(&ost->frame);
	av_frame_free(&ost->tmp_frame);
	sws_freeContext(ost->sws_ctx);
	swr_free(&ost->swr_ctx);
}

int FFMpegVideoWriter::write_video_frame(AVFormatContext *oc, OutputStream *ost)
{
	int ret;
	AVCodecContext *c;
	AVFrame *frame;
	int got_packet = 0;
	AVPacket pkt = { 0 };

	c = ost->enc;


	frame = get_video_frame(ost);

	av_init_packet(&pkt);

	/* encode the image */
	//ret = avcodec_encode_video2(c, &pkt, frame, &got_packet);
	ret = avcodec_send_frame(c, frame);
	

    while (ret = avcodec_receive_packet(c, &pkt) >= 0){
		auto err = write_frame(oc, &c->time_base, ost->st, &pkt);

        if (err < 0){
			cout << "Encode error! " << err << endl;
	                char* buf = new char[255];
	                av_strerror(err, buf, 255);
	                cout <<"Reason:" << buf << endl;
		}
	}
	


    /*if (ret < 0) {
		cout << "Encode error! " << ret << endl;
		char* buf = new char[255];
		av_strerror(ret, buf, 255);
		cout <<"Reason:" << buf << endl;
		//exit(ret);
		return 0;
	}

	if (ret == 0) {
		ret = write_frame(oc, &c->time_base, ost->st, &pkt);
	}
	else {
		ret = 0;
	}

	if (ret < 0) {
//		exit(ret);
		return 0;
	}*/

	return (frame || got_packet) ? 0 : 1;
}

AVFrame* FFMpegVideoWriter::get_video_frame(OutputStream *ost)
{
	AVCodecContext *c = ost->enc;

	AVData *f = buffer_ptr->pop();
	int ret = av_frame_make_writable(ost->tmp_frame);

	if (!ost->sws_ctx) {
		ost->sws_ctx = sws_getContext(c->width, c->height,
			src_pixel_format,
			c->width, c->height,
			c->pix_fmt,
			SCALE_FLAGS, NULL, NULL, NULL);
		if (!ost->sws_ctx) {
			fprintf(stderr,	"Could not initialize the conversion context\n");
			exit(1);
		}
	}

	av_image_fill_arrays(ost->tmp_frame->data, ost->tmp_frame->linesize, f->data, AV_PIX_FMT_BGR24, c->width, c->height, p_align_size);

	sws_scale(ost->sws_ctx,	(const uint8_t * const *)ost->tmp_frame->data, ost->tmp_frame->linesize, 0, c->height, ost->frame->data, ost->frame->linesize);

	ost->frame->pts = ost->next_pts++;

	f->~AVData();

	return ost->frame;
}

void FFMpegVideoWriter::fill_yuv_image(AVFrame *pict, int frame_index,	int width, int height)
{
	int x, y, i, ret;

	/* when we pass a frame to the encoder, it may keep a reference to it
	* internally;
	* make sure we do not overwrite it here
	*/
	ret = av_frame_make_writable(pict);
	if (ret < 0)
		exit(1);

	i = frame_index;

	/* Y */
	for (y = 0; y < height; y++)
	for (x = 0; x < width; x++)
		pict->data[0][y * pict->linesize[0] + x] = x + y + i * 3;

	/* Cb and Cr */
	for (y = 0; y < height / 2; y++) {
		for (x = 0; x < width / 2; x++) {
			pict->data[1][y * pict->linesize[1] + x] = 128 + y + i * 2;
			pict->data[2][y * pict->linesize[2] + x] = 64 + x + i * 5;
		}
	}
}

void FFMpegVideoWriter::open_video(AVFormatContext *oc, AVCodec *codec, OutputStream *ost, AVDictionary *opt_arg)
{
	int ret;
	AVCodecContext *c = ost->enc;
	AVDictionary *opt = NULL;

	av_dict_copy(&opt, opt_arg, 0);

	/* open the codec */
	ret = avcodec_open2(c, codec, &opt);
	av_dict_free(&opt);

	/* allocate and init a re-usable frame */
	ost->frame = alloc_picture(c->pix_fmt, c->width, c->height);

	/* If the output format is not YUV420P, then a temporary YUV420P
	* picture is needed too. It is then converted to the required
	* output format. */
	ost->tmp_frame = NULL;
	//if (c->pix_fmt != AV_PIX_FMT_YUV420P) {
	ost->tmp_frame = alloc_picture(src_pixel_format, c->width, c->height);
	//}

	/* copy the stream parameters to the muxer */
	ret = avcodec_parameters_from_context(ost->st->codecpar, c);
	if (ret < 0) {
		fprintf(stderr, "Could not copy the stream parameters\n");
		exit(1);
	}
}

AVFrame* FFMpegVideoWriter::alloc_picture(enum AVPixelFormat pix_fmt, int width, int height)
{
	AVFrame *picture;
	int ret;

	picture = av_frame_alloc();
	if (!picture)
		return NULL;

	picture->format = pix_fmt;
	picture->width = width;
	picture->height = height;

	/* allocate the buffers for the frame data */
	ret = av_frame_get_buffer(picture, 32);
	if (ret < 0) {
		fprintf(stderr, "Could not allocate frame data.\n");
		exit(1);
	}

	return picture;
}

void FFMpegVideoWriter::add_stream(OutputStream *ost, AVFormatContext *oc, AVCodec **codec, enum AVCodecID codec_id)
{
	AVCodecContext *c;
	int i;

	/* find the encoder */
	*codec = avcodec_find_encoder(codec_id);
	if (!(*codec)) {
		fprintf(stderr, "Could not find encoder for '%s'\n",
			avcodec_get_name(codec_id));
		exit(1);
	}

	ost->st = avformat_new_stream(oc, NULL);
	if (!ost->st) {
		fprintf(stderr, "Could not allocate stream\n");
		exit(1);
	}
	ost->st->id = oc->nb_streams - 1;
	c = avcodec_alloc_context3(*codec);
	if (!c) {
		fprintf(stderr, "Could not alloc an encoding context\n");
		exit(1);
	}
	ost->enc = c;

	switch ((*codec)->type) {
	case AVMEDIA_TYPE_AUDIO:
		c->sample_fmt = (*codec)->sample_fmts ?
			(*codec)->sample_fmts[0] : AV_SAMPLE_FMT_FLTP;
		c->bit_rate = 64000;
		c->sample_rate = 44100;
		if ((*codec)->supported_samplerates) {
			c->sample_rate = (*codec)->supported_samplerates[0];
			for (i = 0; (*codec)->supported_samplerates[i]; i++) {
				if ((*codec)->supported_samplerates[i] == 44100)
					c->sample_rate = 44100;
			}
		}
		c->channels = av_get_channel_layout_nb_channels(c->channel_layout);
		c->channel_layout = AV_CH_LAYOUT_STEREO;
		if ((*codec)->channel_layouts) {
			c->channel_layout = (*codec)->channel_layouts[0];
			for (i = 0; (*codec)->channel_layouts[i]; i++) {
				if ((*codec)->channel_layouts[i] == AV_CH_LAYOUT_STEREO)
					c->channel_layout = AV_CH_LAYOUT_STEREO;
			}
		}
		c->channels = av_get_channel_layout_nb_channels(c->channel_layout);
		ost->st->time_base.num = 1;
		ost->st->time_base.den = c->sample_rate;
		break;

	case AVMEDIA_TYPE_VIDEO:
		c->codec_id = codec_id;

		c->bit_rate = 19e+6;
		/* Resolution must be a multiple of two. */
		c->width = m_frame_size.width;
		c->height = m_frame_size.height;
		/* timebase: This is the fundamental unit of time (in seconds):terms
		* of which frame timestamps are represented. For fixed-fps content,
		* timebase should be 1/framerate and timestamp increments should be
		* identical to 1. */
		ost->st->time_base.num = 1;
		ost->st->time_base.den = m_fps;
		c->time_base = ost->st->time_base;

		c->gop_size = 12; /* emit one intra frame every twelve frames at most */
		c->pix_fmt = STREAM_PIX_FMT;
		if (c->codec_id == AV_CODEC_ID_MPEG1VIDEO) {
			/* Needed to avoid using macroblocks:which some coeffs overflow.
			* This does not happen with normal video, it just happens here as
			* the motion of the chroma plane does not match the luma plane. */
			c->mb_decision = 2;
		}
		break;

	default:
		break;
	}

	/* Some formats want stream headers to be separate. */
	if (oc->oformat->flags & AVFMT_GLOBALHEADER)
		c->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
}
