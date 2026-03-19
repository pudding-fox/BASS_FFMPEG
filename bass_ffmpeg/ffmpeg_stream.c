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
		return sizeof(FLOAT);
	}
	else {
		return sizeof(SHORT);
	}
}

BOOL ffmpeg_stream_create(const char* url, FFMPEG_STREAM** const stream, const DWORD flags) {
	*stream = calloc(sizeof(FFMPEG_STREAM), 1);
	if (!*stream) {
		return FALSE;
	}
	if (avformat_open_input(&(*stream)->format_context, url, NULL, NULL) != 0) {
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
	(*stream)->flags = flags;
	(*stream)->length = ffmpeg_stream_length(*stream);
	if (!ffmpeg_stream_update(*stream)) {
		ffmpeg_stream_free(*stream);
		return FALSE;
	}
	return TRUE;
}

BOOL ffmpeg_buffer_alloc(FFMPEG_STREAM* const stream, AVFrame* source, FFMPEG_FRAME* destination) {
	DWORD samples_per_frame = source->nb_samples * source->channels;
	DWORD bytes_per_frame = samples_per_frame * bass_bytes_per_sample(stream->flags);
	DWORD buffer_size = bytes_per_frame * 2; //TODO: No idea why we need to multiply by 2, but swr_convert encounters a buffer overflow otherwise.
	if (!destination->buffer) {
		destination->buffer = malloc(buffer_size);
		if (!destination->buffer) {
			return FALSE;
		}
		destination->count = bytes_per_frame;
	}
	else if (destination->count < bytes_per_frame) {
		free(destination->buffer);
		destination->buffer = malloc(buffer_size);
		if (!destination->buffer) {
			return FALSE;
		}
		destination->count = bytes_per_frame;
	}
	return TRUE;
}

BOOL ffmpeg_stream_resample(FFMPEG_STREAM* const stream, AVFrame* source, FFMPEG_FRAME* destination) {
	if (!ffmpeg_buffer_alloc(stream, source, destination)) {
		return FALSE;
	}
	DWORD count = swr_convert(
		stream->resample_context,
		&(BYTE*)destination->buffer,
		source->nb_samples,
		(BYTE**)source->data,
		source->nb_samples
	);
	destination->position = 0;
	return TRUE;
}

QWORD ffmpeg_stream_position(FFMPEG_STREAM* const stream, AVFrame* frame) {
	QWORD timestamp = av_frame_get_best_effort_timestamp(frame);
	if (timestamp == AV_NOPTS_VALUE) {
		return 0;
	}
	else {
		DWORD bytes_per_sample = bass_bytes_per_sample(stream->flags);
		DOUBLE position = timestamp *
			av_q2d(stream->stream->time_base) *
			stream->codec_context->sample_rate *
			stream->codec_context->channels *
			bytes_per_sample;
		return (QWORD)position;
	}
}

BOOL ffmpeg_stream_update(FFMPEG_STREAM* const stream) {
	INT result;
	BOOL success = TRUE;
	AVPacket* packet = av_packet_alloc();
	AVFrame* frame = av_frame_alloc();
	goto begin;
retry:
	av_packet_unref(packet);
begin:
	if (av_read_frame(stream->format_context, packet) < 0) {
		success = FALSE;
		goto done;
	}
	if (packet->stream_index != stream->stream_index) {
		goto retry;
	}
	result = avcodec_send_packet(stream->codec_context, packet);
	if (result < 0) {
		if (result == AVERROR(EAGAIN)) {
			goto retry;
		}
		else {
			success = FALSE;
			goto done;
		}
	}
	stream->frame_position = 0;
	stream->frame_count = 0;
	do {
		result = avcodec_receive_frame(stream->codec_context, frame);
		if (result < 0) {
			break;
		}
		if (!ffmpeg_stream_resample(stream, frame, &stream->frames[stream->frame_count])) {
			success = FALSE;
			goto done;
		}
		stream->position = ffmpeg_stream_position(stream, frame);
		stream->frame_count++;
	} while (stream->frame_count < FFMPEG_STREAM_FRAME_COUNT);
done:
	av_frame_free(&frame);
	av_packet_free(&packet);
	return success;
}

DWORD ffmpeg_stream_read_frame(FFMPEG_STREAM* const stream, FFMPEG_FRAME* frame, void* buffer, const DWORD length) {
	DWORD count = frame->count - frame->position;
	if (!count) {
		return 0;
	}
	if (count > length) {
		count = length;
	}
	memcpy(buffer, (BYTE*)frame->buffer + frame->position, count);
	frame->position += count;
	return count;
}

DWORD ffmpeg_stream_read(FFMPEG_STREAM* const stream, void* buffer, const DWORD length) {
	DWORD position = 0;
	DWORD remaining = length;
	while (stream->frame_position < stream->frame_count && remaining > 0) {
		FFMPEG_FRAME* frame = &stream->frames[stream->frame_position];
		DWORD count = ffmpeg_stream_read_frame(stream, frame, (BYTE*)buffer + position, remaining);
		if (count) {
			position += count;
			remaining -= count;
		}
		else {
			if (frame->position == frame->count) {
				stream->frame_position++;
			}
			else {
				break;
			}
		}
	}
	return length - remaining;
}

QWORD ffmpeg_stream_length_seconds(FFMPEG_STREAM* const stream) {
	if (stream->stream->duration == AV_NOPTS_VALUE) {
		QWORD length =
			(stream->format_context->duration / AV_TIME_BASE);
		return length;
	}
	else {
		DOUBLE length =
			stream->stream->duration * av_q2d(stream->stream->time_base);
		return (QWORD)length;
	}
}

QWORD ffmpeg_stream_length(FFMPEG_STREAM* const stream) {
	DWORD bytes_per_sample = bass_bytes_per_sample(stream->flags);
	if (stream->stream->duration == AV_NOPTS_VALUE) {
		QWORD length =
			(stream->format_context->duration / AV_TIME_BASE) *
			stream->codec_context->sample_rate *
			stream->codec_context->channels *
			bytes_per_sample;
		return length;
	}
	else {
		DOUBLE length =
			stream->stream->duration *
			av_q2d(stream->stream->time_base) *
			stream->codec_context->sample_rate *
			stream->codec_context->channels *
			bytes_per_sample;
		return (QWORD)length;
	}
}

BOOL ffmpeg_stream_can_seek(FFMPEG_STREAM* const stream, QWORD position) {
	return position >= 0 && position <= stream->length;
}

BOOL ffmpeg_stream_seek(FFMPEG_STREAM* const stream, QWORD position) {
	QWORD length = ffmpeg_stream_length_seconds(stream);
	DOUBLE position_seconds = (position / (DOUBLE)stream->length) * length;
	QWORD timestamp = av_rescale_q(
		(QWORD)(position_seconds * AV_TIME_BASE),
		AV_TIME_BASE_Q,
		stream->stream->time_base
	);
	DWORD flags = AVSEEK_FLAG_BACKWARD;
	INT result = av_seek_frame(stream->format_context, stream->stream_index, timestamp, flags);
	if (result < 0) {
		return FALSE;
	}
	if (!ffmpeg_stream_reset(stream)) {
		return FALSE;
	}
	return ffmpeg_stream_update(stream);
}

BOOL ffmpeg_stream_reset(FFMPEG_STREAM* const stream) {
	stream->frame_count = 0;
	stream->frame_position = 0;
	avcodec_flush_buffers(stream->codec_context);
	return TRUE;
}

BOOL ffmpeg_stream_free(FFMPEG_STREAM* const stream) {
	if (stream->resample_context) {
		swr_free(&stream->resample_context);
	}
	for (DWORD a = 0; a < FFMPEG_STREAM_FRAME_COUNT; a++) {
		if (stream->frames[a].buffer) {
			free(stream->frames[a].buffer);
		}
	}
	if (stream->codec_context) {
		avcodec_free_context(&stream->codec_context);
	}
	if (stream->format_context) {
		avformat_close_input(&stream->format_context);
	}
	free(stream);
	return TRUE;
}