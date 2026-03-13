#include "bass.h"

#define BASSFFMPEGDEF(f) WINAPI f

// BASS_CHANNELINFO type
#define BASS_CTYPE_STREAM_FFMPEG 0x1f301

HSTREAM BASSFFMPEGDEF(BASS_FFMPEG_StreamCreateFile)(DWORD filetype, const void* file, QWORD offset, QWORD length, DWORD flags);
