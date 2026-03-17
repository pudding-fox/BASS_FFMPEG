#include "bass_ffmpeg.h"
#include "ffmpeg_stream.h"

const BASS_FUNCTIONS* bassfunc;

//2.4.1.0
#define BASSFFMPEGVERSION 0x02040100

//I have no idea how to prevent linking against this routine in msvcrt.
//It doesn't exist on Windows XP.
//Hopefully it doesn't do anything important.
int _except_handler4_common() {
	return 0;
}

HSTREAM WINAPI BASS_FFMPEG_StreamCreate(BASSFILE file, DWORD flags);
DWORD WINAPI BASS_FFMPEG_StreamProc(HSTREAM handle, void* buffer, DWORD length, void* user);
QWORD WINAPI BASS_FFMPEG_GetLength(void* inst, DWORD mode);
VOID WINAPI BASS_FFMPEG_GetInfo(void* inst, BASS_CHANNELINFO* info);
BOOL WINAPI BASS_FFMPEG_CanSetPosition(void* inst, QWORD position, DWORD mode);
QWORD WINAPI BASS_FFMPEG_SetPosition(void* inst, QWORD position, DWORD mode);

VOID WINAPI BASS_FFMPEG_Free(void* inst);

const ADDON_FUNCTIONS addon_functions = {
	0,
	&BASS_FFMPEG_Free,
	&BASS_FFMPEG_GetLength,
	NULL,
	NULL,
	&BASS_FFMPEG_GetInfo,
	&BASS_FFMPEG_CanSetPosition,
	&BASS_FFMPEG_SetPosition,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL
};

static const BASS_PLUGINFORM plugin_form[] = {
	{ BASS_CTYPE_STREAM_FFMPEG, "FFMPEG", NULL }
};

static const BASS_PLUGININFO plugin_info = { BASSFFMPEGVERSION, 0, plugin_form };

BOOL WINAPI DllMain(HANDLE dll, DWORD reason, LPVOID reserved) {
	switch (reason) {
	case DLL_PROCESS_ATTACH:
		DisableThreadLibraryCalls((HMODULE)dll);
		if (HIWORD(BASS_GetVersion()) != BASSVERSION || !GetBassFunc()) {
			MessageBoxA(0, "Incorrect BASS.DLL version (" BASSVERSIONTEXT " is required)", "BASS", MB_ICONERROR | MB_OK);
			return FALSE;
		}
		break;
	}
	return TRUE;
}

const VOID* WINAPI BASSplugin(DWORD face) {
	switch (face) {
	case BASSPLUGIN_INFO:
		return (void*)&plugin_info;
	case BASSPLUGIN_CREATE:
		return (void*)&BASS_FFMPEG_StreamCreate;
	}
	return NULL;
}

HSTREAM WINAPI BASS_FFMPEG_StreamCreate(BASSFILE file, DWORD flags) {
	HSTREAM handle;
	FFMPEG_STREAM* stream;
	BOOL unicode = FALSE;
	const char* file_name = bassfunc->file.GetFileName(file, &unicode);
	if (!file_name) {
		error(BASS_ERROR_NOTFILE);
	}
	if (unicode) {
		const char unicode_filename[MAX_PATH];
		if (WideCharToMultiByte(CP_ACP, 0, (LPCWSTR)file_name, -1, unicode_filename, MAX_PATH, NULL, NULL) <= 0) {
			error(BASS_ERROR_NOTFILE);
		}
		file_name = unicode_filename;
	}
	if (!ffmpeg_stream_create(file_name, &stream, flags)) {
		error(BASS_ERROR_FILEFORM);
	}
	handle = bassfunc->CreateStream(
		stream->stream->codecpar->sample_rate,
		stream->stream->codecpar->channels,
		flags,
		&BASS_FFMPEG_StreamProc,
		stream,
		&addon_functions
	);
	if (!handle) {
		ffmpeg_stream_free(stream);
		return 0;
	}
	bassfunc->file.SetStream(file, handle);
	noerrorn(handle);
}

HSTREAM WINAPI BASS_FFMPEG_StreamCreateFile(DWORD filetype, const void* file, QWORD offset, QWORD length, DWORD flags) {
	HSTREAM handle;
	BASSFILE bass_file = bassfunc->file.Open(filetype, file, offset, length, flags, BASSFILE_EX_TAGS);
	if (!bass_file) {
		return 0;
	}
	handle = BASS_FFMPEG_StreamCreate(bass_file, flags);
	if (!handle) {
		bassfunc->file.Close(bass_file);
		return 0;
	}
	return handle;
}

DWORD WINAPI BASS_FFMPEG_StreamProc(HSTREAM handle, void* buffer, DWORD length, void* user) {
	FFMPEG_STREAM* stream = user;
	DWORD position = 0;
	DWORD remaining = length;
	while (remaining > 0) {
		if (!stream->frame_count || stream->frame_position == stream->frame_count) {
			if (!ffmpeg_stream_update(stream)) {
				return BASS_STREAMPROC_END;
			}
		}
		DWORD count = ffmpeg_stream_read(stream, (BYTE*)buffer + position, remaining);
		if (!count) {
			break;
		}
		position += count;
		remaining -= count;
	}
	return length - remaining;
}

QWORD WINAPI BASS_FFMPEG_GetLength(void* inst, DWORD mode) {
	FFMPEG_STREAM* stream = inst;
	if (mode == BASS_POS_BYTE) {
		noerrorn(ffmpeg_stream_length(stream));
	}
	else {
		errorn(BASS_ERROR_NOTAVAIL);
	}
}

VOID WINAPI BASS_FFMPEG_GetInfo(void* inst, BASS_CHANNELINFO* info) {
	FFMPEG_STREAM* stream = inst;
	info->ctype = BASS_CTYPE_STREAM_FFMPEG;
	info->origres = stream->stream->codecpar->bits_per_coded_sample;
}

BOOL WINAPI BASS_FFMPEG_CanSetPosition(void* inst, QWORD position, DWORD mode) {
	FFMPEG_STREAM* stream = inst;
	if (mode == BASS_POS_BYTE) {
		return ffmpeg_stream_can_seek(stream, position);
	}
	else {
		error(BASS_ERROR_NOTAVAIL);
	}
}

QWORD WINAPI BASS_FFMPEG_SetPosition(void* inst, QWORD position, DWORD mode) {
	FFMPEG_STREAM* stream = inst;
	if (mode == BASS_POS_BYTE) {
		if (ffmpeg_stream_seek(stream, position)) {
			if (ffmpeg_stream_reset(stream)) {
				return position;
			}
		}
	}
	errorn(BASS_ERROR_NOTAVAIL);
}

VOID WINAPI BASS_FFMPEG_Free(void* inst) {
	FFMPEG_STREAM* stream = inst;
	ffmpeg_stream_free(stream);
}