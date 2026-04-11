#include <bass-addon.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>

#define FFMPEG_TRACK_TITLE_LENGTH 30
#define FFMPEG_STREAM_BUFFER_COUNT 10240
#define FFMPEG_STREAM_FRAME_COUNT 16

typedef struct {
	DWORD index;
	char title[FFMPEG_TRACK_TITLE_LENGTH];
} FFMPEG_TRACK;

typedef struct {
	BYTE* buffer;
	DWORD position;
	DWORD count;
} FFMPEG_FRAME;

typedef struct {
	BYTE* buffer;
	AVIOContext* io_context;
	AVFormatContext* format_context;
	AVCodecContext* codec_context;
	AVStream* stream;
	DWORD stream_index;
	AVCodec* codec;
	FFMPEG_FRAME frames[FFMPEG_STREAM_FRAME_COUNT];
	DWORD frame_position;
	DWORD frame_count;
	SwrContext* resample_context;
	DWORD flags;
	QWORD position;
	QWORD length;
	TAG_ID3* tag;
} FFMPEG_STREAM;

BOOL ffmpeg_stream_create(BASSFILE file, FFMPEG_STREAM** const stream, const DWORD flags);

BOOL ffmpeg_stream_update(FFMPEG_STREAM* const stream);

DWORD ffmpeg_stream_read(FFMPEG_STREAM* const stream, void* buffer, const DWORD length);

QWORD ffmpeg_stream_length(FFMPEG_STREAM* const stream);

BOOL ffmpeg_stream_can_seek(FFMPEG_STREAM* const stream, QWORD position);

BOOL ffmpeg_stream_seek(FFMPEG_STREAM* const stream, QWORD position);

BOOL ffmpeg_stream_reset(FFMPEG_STREAM* const stream);

BOOL ffmpeg_stream_tag(FFMPEG_STREAM* const stream);

DWORD ffmpeg_stream_get_tracks(FFMPEG_STREAM* const stream, FFMPEG_TRACK* tracks, DWORD count);

BOOL ffmpeg_stream_set_track(FFMPEG_STREAM* const stream, DWORD index);

BOOL ffmpeg_stream_free(FFMPEG_STREAM* const stream);