#include "ffmpeg_stream.h"
#include <libavutil/samplefmt.h>

#include <string.h>

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

INT ffmpeg_stream_io_read(void* opaque, BYTE* buffer, INT length) {
	BASSFILE file = (BASSFILE)opaque;
	INT result = bassfunc->file.Read(file, buffer, length);
	return result;
}

INT64 ffmpeg_stream_io_seek(void* opaque, INT64 position, INT whence) {
	BASSFILE file = (BASSFILE)opaque;
	INT64 result;
	switch (whence) {
	case AVSEEK_SIZE:
		result = bassfunc->file.GetPos(file, BASS_FILEPOS_END);
		break;
	default:
		result = bassfunc->file.Seek(file, position);
		break;
	}
	return result;
}

BOOL ffmpeg_stream_create(BASSFILE file, FFMPEG_STREAM** const stream, const DWORD flags) {
	*stream = calloc(sizeof(FFMPEG_STREAM), 1);
	if (!*stream) {
		return FALSE;
	}
	(*stream)->buffer = av_malloc(FFMPEG_STREAM_BUFFER_COUNT);
	if (!(*stream)->buffer) {
		ffmpeg_stream_free(*stream);
		return FALSE;
	}
	(*stream)->flags = flags;
	(*stream)->io_context = avio_alloc_context(
		(*stream)->buffer,
		FFMPEG_STREAM_BUFFER_COUNT,
		0,
		file,
		&ffmpeg_stream_io_read,
		NULL,
		&ffmpeg_stream_io_seek
	);
	if (!(*stream)->io_context) {
		ffmpeg_stream_free(*stream);
		return FALSE;
	}
	(*stream)->format_context = avformat_alloc_context();
	if (!(*stream)->format_context) {
		ffmpeg_stream_free(*stream);
		return FALSE;
	}
	(*stream)->format_context->pb = (*stream)->io_context;
	(*stream)->format_context->flags |= AVFMT_FLAG_CUSTOM_IO;
	if (avformat_open_input(&(*stream)->format_context, NULL, NULL, NULL) < 0) {
		ffmpeg_stream_free(*stream);
		return FALSE;
	}
	if (avformat_find_stream_info((*stream)->format_context, NULL) < 0) {
		ffmpeg_stream_free(*stream);
		return FALSE;
	}
	if (!ffmpeg_stream_set_track(*stream, 0)) {
		ffmpeg_stream_free(*stream);
		return FALSE;
	}
	if (!ffmpeg_stream_tag(*stream)) {
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
		&destination->buffer,
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
	if (stream->codec_context) {
		avcodec_flush_buffers(stream->codec_context);
	}
	return TRUE;
}

const char* GENRES[] =
{
	"Blues", "Classic Rock", "Country", "Dance", "Disco", "Funk", "Grunge", "Hip-Hop", "Jazz", "Metal",
	"New Age", "Oldies", "Other", "Pop", "R&B", "Rap", "Reggae", "Rock", "Techno", "Industrial",
	"Alternative", "Ska", "Death Metal", "Pranks", "Soundtrack", "Euro-Techno", "Ambient", "Trip-Hop", "Vocal", "Jazz+Funk",
	"Fusion", "Trance", "Classical", "Instrumental", "Acid", "House", "Game", "Sound Clip", "Gospel", "Noise",
	"Alternative Rock", "Bass", "Soul", "Punk", "Space", "Meditative", "Instrumental Pop", "Instrumental Rock", "Ethnic", "Gothic",
	"Darkwave", "Techno-Industrial", "Electronic", "Pop-Folk", "Eurodance", "Dream", "Southern Rock", "Comedy", "Cult", "Gangsta",
	"Top Christian Rap", "Pop/Funk", "Jungle", "Native US", "Cabaret", "New Wave", "Psychadelic", "Rave", "Showtunes", "Trailer",
	"Lo-Fi", "Tribal", "Acid Punk", "Acid Jazz", "Polka", "Retro", "Musical", "Rock & Roll", "Hard Rock", "Folk",
	"Folk-Rock", "National Folk", "Swing", "Fast Fusion", "Bebob", "Latin", "Revival", "Celtic", "Bluegrass", "Avantgarde",
	"Gothic Rock", "Progressive Rock", "Psychedelic Rock", "Symphonic Rock", "Slow Rock", "Big Band", "Chorus", "Easy Listening", "Acoustic", "Humour",
	"Speech", "Chanson", "Opera", "Chamber Music", "Sonata", "Symphony", "Booty Bass", "Primus", "Porn Groove", "Satire",
	"Slow Jam", "Club", "Tango", "Samba", "Folklore", "Ballad", "Power Ballad", "Rhythmic Soul", "Freestyle", "Duet",
	"Punk Rock", "Drum Solo", "Acapella", "Euro-House", "Dance Hall", "Goa", "Drum & Bass", "Club - House", "Hardcore", "Terror",
	"Indie", "BritPop", "Negerpunk", "Polsk Punk", "Beat", "Christian Gangsta Rap", "Heavy Metal", "Black Metal", "Crossover", "Contemporary Christian",
	"Christian Rock", "Merengue", "Salsa", "Thrash Metal", "Anime", "JPop", "Synthpop", "Unknown"
};

#define GENRES_MAX (sizeof (GENRES) / sizeof (const char *))

BOOL ffmpeg_stream_tag(FFMPEG_STREAM* const stream) {
	stream->tag = calloc(sizeof(TAG_ID3), 1);
	if (!stream->tag) {
		return FALSE;
	}
	stream->tag->id[0] = 'T';
	stream->tag->id[1] = 'A';
	stream->tag->id[2] = 'G';
	AVDictionaryEntry* tag = NULL;
	tag = av_dict_get(stream->format_context->metadata, "title", NULL, 0);
	if (tag) {
		strncpy(stream->tag->title, tag->value, sizeof(stream->tag->title));
	}
	tag = av_dict_get(stream->format_context->metadata, "artist", NULL, 0);
	if (tag) {
		strncpy(stream->tag->artist, tag->value, sizeof(stream->tag->artist));
	}
	tag = av_dict_get(stream->format_context->metadata, "album", NULL, 0);
	if (tag) {
		strncpy(stream->tag->album, tag->value, sizeof(stream->tag->album));
	}
	tag = av_dict_get(stream->format_context->metadata, "year", NULL, 0);
	if (tag) {
		strncpy(stream->tag->year, tag->value, sizeof(stream->tag->year));
	}
	tag = av_dict_get(stream->format_context->metadata, "comment", NULL, 0);
	if (tag) {
		strncpy(stream->tag->comment, tag->value, sizeof(stream->tag->comment));
	}
	tag = av_dict_get(stream->format_context->metadata, "track", NULL, 0);
	if (tag) {
		DWORD track = atoi(tag->value);
		if (track < 0) {
			track = 0;
		}
		if (track > 255) {
			track = 255;
			track = 255;
		}
		memset(stream->tag->comment + sizeof(stream->tag->comment) - 2, 0, 1);
		memset(stream->tag->comment + sizeof(stream->tag->comment) - 1, track, 1);
	}
	tag = av_dict_get(stream->format_context->metadata, "genre", NULL, 0);
	if (tag) {
		BOOL success = FALSE;
		for (BYTE a = 0; a < GENRES_MAX; a++) {
			if (strcmp(tag->value, GENRES[a]) == 0) {
				stream->tag->genre = a;
				success = TRUE;
				break;
			}
		}
		if (!success) {
			stream->tag->genre = GENRES_MAX;
		}
	}
	return TRUE;
}

DWORD ffmpeg_stream_get_tracks(FFMPEG_STREAM* const stream, FFMPEG_TRACK* tracks, DWORD count) {
	DWORD position = 0;
	for (DWORD a = 0; a < stream->format_context->nb_streams && position < count; a++) {
		if (stream->format_context->streams[a]->codec->codec_type == AVMEDIA_TYPE_AUDIO) {
			AVDictionaryEntry* tag = av_dict_get(stream->format_context->streams[a]->metadata, "title", NULL, 0);
			tracks[position].index = position;
			if (tag) {
				strncpy(tracks[position].title, tag->value, sizeof(tracks[position].title));
			}
			position++;
		}
	}
	return position;
}

BOOL ffmpeg_stream_set_track(FFMPEG_STREAM* const stream, DWORD index) {
	if (stream->codec_context) {
		avcodec_free_context(&stream->codec_context);
	}
	stream->stream = NULL;
	DWORD position = 0;
	for (DWORD a = 0; a < stream->format_context->nb_streams; a++) {
		if (stream->format_context->streams[a]->codec->codec_type == AVMEDIA_TYPE_AUDIO) {
			if (position == index) {
				stream->stream = stream->format_context->streams[a];
				stream->stream_index = a;
				break;
			}
			position++;
		}
	}
	if (!stream->stream) {
		return FALSE;
	}
	if (!ffmpeg_stream_reset(stream)) {
		return FALSE;
	}
	stream->codec = avcodec_find_decoder(stream->stream->codecpar->codec_id);
	if (!stream->codec) {
		return FALSE;
	}
	stream->codec_context = avcodec_alloc_context3(NULL);
	if (avcodec_parameters_to_context(stream->codec_context, stream->stream->codecpar) < 0) {
		return FALSE;
	}
	if (avcodec_open2(stream->codec_context, stream->codec, NULL) < 0) {
		return FALSE;
	}
	if (stream->resample_context) {
		swr_free(&stream->resample_context);
	}
	stream->resample_context = swr_alloc();
	if (!stream->resample_context) {
		return FALSE;
	}
	stream->resample_context = swr_alloc_set_opts(
		stream->resample_context,
		bass_channel_layout(stream->codec_context->channels),
		bass_sample_format(stream->flags),
		stream->codec_context->sample_rate,
		av_get_default_channel_layout(stream->codec_context->channels),
		stream->codec_context->sample_fmt,
		stream->codec_context->sample_rate,
		0,
		NULL
	);
	if (!stream->resample_context) {
		return FALSE;
	}
	if (swr_init(stream->resample_context) < 0) {
		return FALSE;
	}
	stream->length = ffmpeg_stream_length(stream);
	if (!ffmpeg_stream_update(stream)) {
		return FALSE;
	}
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
	if (stream->io_context) {
		avio_context_free(&stream->io_context);
	}
	free(stream);
	return TRUE;
}