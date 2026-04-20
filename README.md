# BASS_FFMPEG
A decoder plugin for BASS which uses the ffmpeg library with .NET bindings.

bass.dll is required for native projects.
ManagedBass is required for .NET projects.

A simple example;

```C#
//If you load as a plugin.
var sourceChannel = Bass.CreateStream(Path.Combine(CurrentDirectory, this.FileName), 0, 0, this.BassFlags);
//Otherwise.
var sourceChannel = BassFfmpeg.CreateStream(Path.Combine(CurrentDirectory, this.FileName), 0, 0, this.BassFlags);

if (sourceChannel == 0)
{
    Assert.Fail(string.Format("Failed to create source stream: {0}", Enum.GetName(typeof(Errors), Bass.LastError)));
}

if (!Bass.ChannelPlay(sourceChannel))
{
    Assert.Fail(string.Format("Failed to play the playback stream: {0}", Enum.GetName(typeof(Errors), Bass.LastError)));
}

var channelLength = Bass.ChannelGetLength(sourceChannel);
var channelLengthSeconds = Bass.ChannelBytes2Seconds(sourceChannel, channelLength);

do
{
    if (Bass.ChannelIsActive(sourceChannel) == PlaybackState.Stopped)
    {
        break;
    }

    var channelPosition = Bass.ChannelGetPosition(sourceChannel);
    var channelPositionSeconds = Bass.ChannelBytes2Seconds(sourceChannel, channelPosition);

    Debug.WriteLine(
        "{0}/{1}",
        TimeSpan.FromSeconds(channelPositionSeconds).ToString("g"),
        TimeSpan.FromSeconds(channelLengthSeconds).ToString("g")
    );

    Thread.Sleep(1000);
} while (true);

if (!Bass.StreamFree(sourceChannel))
{
    Assert.Fail(string.Format("Failed to free the source stream: {0}", Enum.GetName(typeof(Errors), Bass.LastError)));
}
```