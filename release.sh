mkdir -p ./release/bass_ffmpeg/x86
mkdir -p ./release/bass_ffmpeg/x64

cp ./bass_ffmpeg/bass_ffmpeg.h ./release/bass_ffmpeg/

cp ./lib/x86/avcodec.lib ./release/bass_ffmpeg/x86
cp ./lib/x86/avcodec-62.dll ./release/bass_ffmpeg/x86
cp ./lib/x86/avformat.lib ./release/bass_ffmpeg/x86
cp ./lib/x86/avformat-62.dll ./release/bass_ffmpeg/x86
cp ./lib/x86/avutil.lib ./release/bass_ffmpeg/x86
cp ./lib/x86/avutil-60.dll ./release/bass_ffmpeg/x86
cp ./lib/x86/bass.lib ./release/bass_ffmpeg/x86
cp ./lib/x86/bass.dll ./release/bass_ffmpeg/x86
cp ./lib/x86/bass_ffmpeg.lib ./release/bass_ffmpeg/x86
cp ./lib/x86/bass_ffmpeg.dll ./release/bass_ffmpeg/x86
cp ./lib/x86/swresample.lib ./release/bass_ffmpeg/x86
cp ./lib/x86/swresample-6.dll ./release/bass_ffmpeg/x86

cp ./lib/x64/avcodec.lib ./release/bass_ffmpeg/x64
cp ./lib/x64/avcodec-62.dll ./release/bass_ffmpeg/x64
cp ./lib/x64/avformat.lib ./release/bass_ffmpeg/x64
cp ./lib/x64/avformat-62.dll ./release/bass_ffmpeg/x64
cp ./lib/x64/avutil.lib ./release/bass_ffmpeg/x64
cp ./lib/x64/avutil-60.dll ./release/bass_ffmpeg/x64
cp ./lib/x64/bass.lib ./release/bass_ffmpeg/x64
cp ./lib/x64/bass.dll ./release/bass_ffmpeg/x64
cp ./lib/x64/bass_ffmpeg.lib ./release/bass_ffmpeg/x64
cp ./lib/x64/bass_ffmpeg.dll ./release/bass_ffmpeg/x64
cp ./lib/x64/swresample.lib ./release/bass_ffmpeg/x64
cp ./lib/x64/swresample-6.dll ./release/bass_ffmpeg/x64