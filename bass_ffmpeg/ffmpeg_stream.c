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
	(*stream)->frames = calloc(sizeof(FFMPEG_FRAME*), FFMPEG_STREAM_FRAME_COUNT);
	if (!(*stream)->frames) {
		ffmpeg_stream_free(*stream);
		return FALSE;
	}
	for (DWORD a = 0; a < FFMPEG_STREAM_FRAME_COUNT; a++) {
		(*stream)->frames[a] = calloc(sizeof(FFMPEG_FRAME), 1);
		if (!(*stream)->frames[a]) {
			ffmpeg_stream_free(*stream);
			return FALSE;
		}
		(*stream)->frames[a]->frame = av_frame_alloc();
		if (!(*stream)->frames[a]->frame) {
			ffmpeg_stream_free(*stream);
			return FALSE;
		}
		(*stream)->frames[a]->position = 0;
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
	(*stream)->flags = flags;
	return TRUE;
}

BOOL ffmpeg_stream_resample(FFMPEG_STREAM* const stream, FFMPEG_FRAME* frame) {
	DWORD samples_per_frame = frame->frame->nb_samples * frame->frame->channels;
	DWORD bytes_per_frame = samples_per_frame * bass_bytes_per_sample(stream->flags);
	DWORD buffer_size = bytes_per_frame * 2; //TODO: No idea why we need to multiply by 2, but swr_convert encounters a buffer overflow otherwise.
	if (!frame->buffer) {
		frame->buffer = malloc(buffer_size);
		if (!frame->buffer) {
			return FALSE;
		}
	}
	else if (frame->count < bytes_per_frame) {
		free(frame->buffer);
		frame->buffer = malloc(buffer_size);
		if (!frame->buffer) {
			return FALSE;
		}
	}
	DWORD count = swr_convert(
		stream->resample_context,
		&(BYTE*)frame->buffer,
		frame->frame->nb_samples,
		(BYTE**)frame->frame->data,
		frame->frame->nb_samples
	);
	frame->position = 0;
	frame->count = bytes_per_frame;
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
		FFMPEG_FRAME* frame = stream->frames[stream->frame_count];
		result = avcodec_receive_frame(stream->codec_context, frame->frame);
		if (result < 0) {
			break;
		}
		if (!ffmpeg_stream_resample(stream, frame)) {
			break;
		}
		stream->frame_count++;
	} while (stream->frame_count < FFMPEG_STREAM_FRAME_COUNT);
	return TRUE;
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
		FFMPEG_FRAME* frame = stream->frames[stream->frame_position];
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

QWORD ffmpeg_stream_length(FFMPEG_STREAM* const stream) {
	return stream->stream->duration
		* stream->codec_context->sample_rate
		* stream->codec_context->channels
		* bass_bytes_per_sample(stream->flags)
		/ stream->stream->time_base.den;
}

QWORD ffmpeg_file_length(FFMPEG_STREAM* const stream) {
	return avio_size(stream->format_context->pb);
}

BOOL ffmpeg_stream_can_seek(FFMPEG_STREAM* const stream, QWORD position) {
	return position >= 0 && position <= ffmpeg_stream_length(stream);
}

BOOL ffmpeg_stream_seek(FFMPEG_STREAM* const stream, QWORD position) {
	DWORD result;
	DWORD flags = AVSEEK_FLAG_BYTE | AVSEEK_FLAG_ANY;
	if (position == 0) {
		result = avformat_seek_file(stream->format_context, stream->stream_index, INT64_MIN, INT64_MIN, INT64_MAX, flags);
	}
	else {
		QWORD stream_length = ffmpeg_stream_length(stream);
		QWORD file_length = ffmpeg_file_length(stream);
		if (position == stream_length) {
			result = avformat_seek_file(stream->format_context, stream->stream_index, INT64_MIN, file_length, INT64_MAX, flags);
		}
		else {
			DOUBLE offset = position * ((DOUBLE)file_length / stream_length);
			result = avformat_seek_file(stream->format_context, stream->stream_index, INT64_MIN, (QWORD)offset, INT64_MAX, flags);
		}
	}
	if (result < 0) {
		return FALSE;
	}
	return TRUE;
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
	if (stream->frames) {
		for (DWORD a = 0; a < FFMPEG_STREAM_FRAME_COUNT; a++) {
			if (stream->frames[a]) {
				if (stream->frames[a]->frame) {
					av_frame_free(&stream->frames[a]->frame);
				}
				if (stream->frames[a]->buffer) {
					free(stream->frames[a]->buffer);
				}
				free(stream->frames[a]);
			}
		}
		free(stream->frames);
	}
	if (stream->packet) {
		av_packet_free(&stream->packet);
	}
	if (stream->format_context) {
		avformat_close_input(&stream->format_context);
	}
	free(stream);
	return TRUE;
}