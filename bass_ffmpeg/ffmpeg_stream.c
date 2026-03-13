#include "ffmpeg_stream.h"
#include <libavutil/samplefmt.h>

int64_t bass_channel_layout(const DWORD channels) {
	switch (channels) {
	case 1:
		return AV_CH_LAYOUT_MONO;
	case 2:
		return AV_CH_LAYOUT_STEREO;
	case 3:
	case 5:
	case 6:
		return AV_CH_LAYOUT_5POINT1;
	default:
		return AV_CH_LAYOUT_7POINT1;
	}
}

enum AVSampleFormat bass_sample_format(const DWORD flags) {
	if ((flags & BASS_SAMPLE_FLOAT) == BASS_SAMPLE_FLOAT) {
		return AV_SAMPLE_FMT_FLT;
	}
	else {
		return AV_SAMPLE_FMT_S16;
	}
}

DWORD bass_bytes_per_sample(const DWORD flags) {
	if ((flags & BASS_SAMPLE_FLOAT) == BASS_SAMPLE_FLOAT) {
		return 4;
	}
	else {
		return 2;
	}
}

BOOL ffmpeg_stream_create(const char* url, FFMPEG_STREAM** const stream, const DWORD flags) {
	*stream = calloc(sizeof(FFMPEG_STREAM), 1);
	if (!*stream) {
		return FALSE;
	}
	if (avformat_open_input(&(*stream)->format_context, url, NULL, &(*stream)->options) != 0) {
		ffmpeg_stream_free(*stream);
		return FALSE;
	}
	if (avformat_find_stream_info((*stream)->format_context, NULL) < 0) {
		ffmpeg_stream_free(*stream);
		return FALSE;
	}
	for (DWORD a = 0; a < (*stream)->format_context->nb_streams; a++) {
		if ((*stream)->format_context->streams[a]->codec->codec_type == AVMEDIA_TYPE_AUDIO) {
			(*stream)->stream = (*stream)->format_context->streams[a];
			(*stream)->stream_index = a;
		}
	}
	if (!(*stream)->stream) {
		ffmpeg_stream_free(*stream);
		return FALSE;
	}
	(*stream)->codec = avcodec_find_decoder((*stream)->stream->codecpar->codec_id);
	if (!(*stream)->codec) {
		ffmpeg_stream_free(*stream);
		return FALSE;
	}
	(*stream)->codec_context = avcodec_alloc_context3(NULL);
	if (avcodec_parameters_to_context((*stream)->codec_context, (*stream)->stream->codecpar) < 0) {
		ffmpeg_stream_free(*stream);
		return FALSE;
	}
	if (avcodec_open2((*stream)->codec_context, (*stream)->codec, NULL) < 0) {
		ffmpeg_stream_free(*stream);
		return FALSE;
	}
	(*stream)->packet = av_packet_alloc();
	if (!(*stream)->packet) {
		ffmpeg_stream_free(*stream);
		return FALSE;
	}
	(*stream)->frames = calloc(sizeof(AVFrame*), FFMPEG_STREAM_FRAME_COUNT);
	if (!(*stream)->frames) {
		ffmpeg_stream_free(*stream);
		return FALSE;
	}
	for (DWORD a = 0; a < FFMPEG_STREAM_FRAME_COUNT; a++) {
		(*stream)->frames[a] = av_frame_alloc();
		if (!(*stream)->frames[a]) {
			ffmpeg_stream_free(*stream);
			return FALSE;
		}
	}
	(*stream)->resample_context = swr_alloc();
	if (!(*stream)->resample_context) {
		ffmpeg_stream_free(*stream);
		return FALSE;
	}
	(*stream)->resample_context = swr_alloc_set_opts(
		(*stream)->resample_context,
		bass_channel_layout((*stream)->codec_context->channels),
		bass_sample_format(flags),
		(*stream)->codec_context->sample_rate,
		av_get_default_channel_layout((*stream)->codec_context->channels),
		(*stream)->codec_context->sample_fmt,
		(*stream)->codec_context->sample_rate,
		0,
		NULL
	);
	if (!(*stream)->resample_context) {
		ffmpeg_stream_free(*stream);
		return FALSE;
	}
	if (swr_init((*stream)->resample_context) < 0) {
		ffmpeg_stream_free(*stream);
		return FALSE;
	}
	if (!ffmpeg_stream_update(*stream)) {
		ffmpeg_stream_free(*stream);
		return FALSE;
	}
	return TRUE;
}

BOOL ffmpeg_stream_update(FFMPEG_STREAM* const stream) {
	int result;
retry:
	if (av_read_frame(stream->format_context, stream->packet) < 0) {
		return FALSE;
	}
	if (stream->packet->stream_index != stream->stream_index) {
		goto retry;
	}
	result = avcodec_send_packet(stream->codec_context, stream->packet);
	if (result < 0) {
		if (result == AVERROR(EAGAIN)) {
			goto retry;
		}
		else {
			return FALSE;
		}
	}
	stream->frame_position = 0;
	stream->frame_count = 0;
	do {
		AVFrame* frame = stream->frames[stream->frame_count];
		result = avcodec_receive_frame(stream->codec_context, frame);
		if (result < 0) {
			break;
		}
		stream->frame_count++;
	} while (stream->frame_count < FFMPEG_STREAM_FRAME_COUNT);
	return TRUE;
}

DWORD ffmpeg_stream_read(FFMPEG_STREAM* const stream, void* buffer, const DWORD length, DWORD flags) {
	DWORD position = 0;
	DWORD remaining = length;
	while (stream->frame_position < stream->frame_count) {
		AVFrame* frame = stream->frames[stream->frame_position];
		DWORD samples_per_frame = frame->nb_samples * frame->channels;
		DWORD bytes_per_frame = samples_per_frame * bass_bytes_per_sample(flags);
		if (bytes_per_frame > remaining) {
			break;
		}
		DWORD sample_count = swr_convert(
			stream->resample_context,
			&(BYTE*)buffer,
			frame->nb_samples,
			(BYTE**)frame->data,
			frame->nb_samples
		);
		stream->frame_position++;
		remaining -= bytes_per_frame;
	}
	return length - remaining;
}

BOOL ffmpeg_stream_free(FFMPEG_STREAM* const stream) {
	if (stream->resample_context) {
		swr_free(&stream->resample_context);
	}
	for (DWORD a = 0; a < FFMPEG_STREAM_FRAME_COUNT; a++) {
		if (stream->frames[a]) {
			av_frame_free(&stream->frames[a]);
		}
	}
	if (stream->packet) {
		av_packet_free(&stream->packet);
	}
	if (stream->format_context) {
		avformat_close_input(&stream->format_context);
	}
	return TRUE;
}