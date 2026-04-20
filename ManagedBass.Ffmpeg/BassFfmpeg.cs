using System;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;

namespace ManagedBass.Ffmpeg
{
    public static class BassFfmpeg
    {
        const string DllName = "bass_ffmpeg";

        public const ChannelType ChannelType = (ChannelType)0x1f301;

        public static int Module = 0;

        public static bool Load(string folderName = null)
        {
            if (Module != 0)
            {
                return true;
            }
            if (string.IsNullOrEmpty(folderName))
            {
                folderName = Loader.FolderName;
            }
            SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_DEFAULT_DIRS | LOAD_LIBRARY_SEARCH_USER_DIRS);
            var cookie = AddDllDirectory(folderName);
            try
            {
                var fileName = string.Concat(Path.Combine(folderName, DllName), ".", Loader.Extension);
                Module = Bass.PluginLoad(fileName);
            }
            finally
            {
                RemoveDllDirectory(cookie);
            }
            return Module != 0;
        }

        public static bool Unload()
        {
            if (Module != 0)
            {
                if (!Bass.PluginFree(Module))
                {
                    return false;
                }
                Module = 0;
            }
            return true;
        }

        [DllImport(DllName)]
        static extern int BASS_FFMPEG_StreamCreateFile(bool Memory, string File, long Offset, long Length, BassFlags Flags);

        public static int CreateStream(string File, long Offset = 0, long Length = 0, BassFlags Flags = BassFlags.Default)
        {
            return BASS_FFMPEG_StreamCreateFile(false, File, Offset, Length, Flags);
        }

        [DllImport(DllName)]
        static extern int BASS_FFMPEG_GetTracks(int Handle, [Out][MarshalAs(UnmanagedType.LPArray, SizeParamIndex = 2)] FFMPEG_TRACK[] Tracks, int count);

        public static FFMPEG_TRACK[] GetTracks(int Handle)
        {
            var tracks = new FFMPEG_TRACK[16];
            var count = BASS_FFMPEG_GetTracks(Handle, tracks, tracks.Length);
            return tracks.Take(count).ToArray();
        }

        [DllImport(DllName)]
        static extern bool BASS_FFMPEG_SetTrack(int Handle, int Index);

        public static bool SetTrack(int Handle, int Index)
        {
            return BASS_FFMPEG_SetTrack(Handle, Index);
        }

        public static ID3v1Tag ChannelGetTags(int Handle)
        {
            var ptr = Bass.ChannelGetTags(Handle, TagType.ID3);
            if (ptr == IntPtr.Zero)
            {
                return default(ID3v1Tag);
            }
            return Marshal.PtrToStructure(ptr, typeof(ID3v1Tag)) as ID3v1Tag;
        }

        const int FFMPEG_TRACK_TITLE_LENGTH = 30;

        [StructLayout(LayoutKind.Sequential)]
        public struct FFMPEG_TRACK
        {
            public int Index;
            [MarshalAs(UnmanagedType.ByValTStr, SizeConst = FFMPEG_TRACK_TITLE_LENGTH)]
            public string Title;
        }

        public const uint LOAD_LIBRARY_SEARCH_DEFAULT_DIRS = 0x00001000;

        public const uint LOAD_LIBRARY_SEARCH_USER_DIRS = 0x00000400;

        [DllImport("kernel32.dll")]
        public static extern bool SetDefaultDllDirectories(uint DirectoryFlags);

        [DllImport("kernel32.dll", CharSet = CharSet.Unicode)]
        public static extern IntPtr AddDllDirectory(string lpPathName);

        [DllImport("kernel32.dll", SetLastError = true)]
        public static extern bool RemoveDllDirectory(IntPtr Cookie);
    }
}
