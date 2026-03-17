using NUnit.Framework;
using System;
using System.Diagnostics;
using System.IO;
using System.Threading;

namespace ManagedBass.Ffmpeg.Test
{
    [TestFixture(false, BassFlags.Default, @"..\..\Media\01 In Chains.dts", "00:06:45")]
    [TestFixture(false, BassFlags.Default, @"..\..\Media\01 All Night Long.dts", "00:02:47")]
    [TestFixture(false, BassFlags.Default, @"..\..\Media\01 World In My Eyes.dts", "00:04:22")]
    [TestFixture(false, BassFlags.Default, @"..\..\Media\01 Intro.m4a", "00:01:18")]
    [TestFixture(false, BassFlags.Default, @"..\..\Media\02 Hot Dog.m4a", "00:03:50")]
    [TestFixture(false, BassFlags.Default, @"..\..\Media\03 My Generation.m4a", "00:03:40")]
    [TestFixture(false, BassFlags.Default | BassFlags.Float, @"..\..\Media\01 In Chains.dts", "00:06:45")]
    [TestFixture(false, BassFlags.Default | BassFlags.Float, @"..\..\Media\01 All Night Long.dts", "00:02:47")]
    [TestFixture(false, BassFlags.Default | BassFlags.Float, @"..\..\Media\01 World In My Eyes.dts", "00:04:22")]
    [TestFixture(false, BassFlags.Default | BassFlags.Float, @"..\..\Media\01 Intro.m4a", "00:01:18")]
    [TestFixture(false, BassFlags.Default | BassFlags.Float, @"..\..\Media\02 Hot Dog.m4a", "00:03:50")]
    [TestFixture(false, BassFlags.Default | BassFlags.Float, @"..\..\Media\03 My Generation.m4a", "00:03:40")]
    [TestFixture(true, BassFlags.Default, @"..\..\Media\01 Intro.m4a", "00:01:18")]
    [TestFixture(true, BassFlags.Default, @"..\..\Media\02 Hot Dog.m4a", "00:03:50")]
    [TestFixture(true, BassFlags.Default, @"..\..\Media\03 My Generation.m4a", "00:03:40")]
    [TestFixture(true, BassFlags.Default | BassFlags.Float, @"..\..\Media\01 Intro.m4a", "00:01:18")]
    [TestFixture(true, BassFlags.Default | BassFlags.Float, @"..\..\Media\02 Hot Dog.m4a", "00:03:50")]
    [TestFixture(true, BassFlags.Default | BassFlags.Float, @"..\..\Media\03 My Generation.m4a", "00:03:40")]
    public class Tests
    {
        private static readonly string CurrentDirectory = Path.GetDirectoryName(typeof(Tests).Assembly.Location);

        public Tests(bool plugin, BassFlags bassFlags, string fileName, string length)
        {
            this.Plugin = plugin;
            this.BassFlags = bassFlags;
            this.FileName = fileName;
            this.Length = TimeSpan.Parse(length);
        }

        public bool Plugin { get; private set; }

        public BassFlags BassFlags { get; private set; }

        public string FileName { get; private set; }

        public TimeSpan Length { get; private set; }

        [SetUp]
        public void SetUp()
        {
            Assert.IsTrue(Loader.Load("bass"));
            Assert.IsTrue(BassFfmpeg.Load());
            Assert.IsTrue(Bass.Init(Bass.DefaultDevice));
        }

        [TearDown]
        public void TearDown()
        {
            BassFfmpeg.Unload();
            Bass.Free();
        }
        /// <summary>
        /// A basic end to end test.
        /// </summary>
        [Test]
        public void Test001()
        {
            var sourceChannel = default(int);
            if (this.Plugin)
            {
                sourceChannel = Bass.CreateStream(Path.Combine(CurrentDirectory, this.FileName), 0, 0, this.BassFlags);
            }
            else
            {
                sourceChannel = BassFfmpeg.CreateStream(Path.Combine(CurrentDirectory, this.FileName), 0, 0, this.BassFlags);
            }
            if (sourceChannel == 0)
            {
                Assert.Fail(string.Format("Failed to create source stream: {0}", Enum.GetName(typeof(Errors), Bass.LastError)));
            }

            var channelInfo = default(ChannelInfo);
            if (!Bass.ChannelGetInfo(sourceChannel, out channelInfo))
            {
                Assert.Fail(string.Format("Failed to get stream info: {0}", Enum.GetName(typeof(Errors), Bass.LastError)));
            }
            Assert.AreEqual(BassFfmpeg.ChannelType, channelInfo.ChannelType);

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
        }

        /// <summary>
        /// Check seeking.
        /// </summary>
        [Test]
        public void Test002_1()
        {
            var sourceChannel = default(int);
            if (this.Plugin)
            {
                sourceChannel = Bass.CreateStream(Path.Combine(CurrentDirectory, this.FileName), 0, 0, this.BassFlags);
            }
            else
            {
                sourceChannel = BassFfmpeg.CreateStream(Path.Combine(CurrentDirectory, this.FileName), 0, 0, this.BassFlags);
            }
            if (sourceChannel == 0)
            {
                Assert.Fail(string.Format("Failed to create source stream: {0}", Enum.GetName(typeof(Errors), Bass.LastError)));
            }

            var channelInfo = default(ChannelInfo);
            if (!Bass.ChannelGetInfo(sourceChannel, out channelInfo))
            {
                Assert.Fail(string.Format("Failed to get stream info: {0}", Enum.GetName(typeof(Errors), Bass.LastError)));
            }
            Assert.AreEqual(BassFfmpeg.ChannelType, channelInfo.ChannelType);

            if (!Bass.ChannelPlay(sourceChannel))
            {
                Assert.Fail(string.Format("Failed to play the playback stream: {0}", Enum.GetName(typeof(Errors), Bass.LastError)));
            }

            var channelLength = Bass.ChannelGetLength(sourceChannel);
            var channelLengthSeconds = Bass.ChannelBytes2Seconds(sourceChannel, channelLength);

            Bass.ChannelSetPosition(sourceChannel, Bass.ChannelSeconds2Bytes(sourceChannel, channelLengthSeconds - 10), PositionFlags.Bytes);

            Thread.Sleep(2000);

            {
                var channelPosition = Bass.ChannelGetPosition(sourceChannel);
                var channelPositionSeconds = Bass.ChannelBytes2Seconds(sourceChannel, channelPosition);

                Assert.IsTrue(channelPositionSeconds >= channelLengthSeconds - 10);
            }

            Thread.Sleep(10000);

            {
                var channelPosition = Bass.ChannelGetPosition(sourceChannel);
                var channelPositionSeconds = Bass.ChannelBytes2Seconds(sourceChannel, channelPosition);

                Assert.AreEqual(Math.Floor(this.Length.TotalSeconds), Math.Floor(channelPositionSeconds));
            }

            if (!Bass.StreamFree(sourceChannel))
            {
                Assert.Fail(string.Format("Failed to free the source stream: {0}", Enum.GetName(typeof(Errors), Bass.LastError)));
            }
        }

        /// <summary>
        /// Check seeking.
        /// </summary>
        [Test]
        public void Test002_2()
        {
            var sourceChannel = default(int);
            if (this.Plugin)
            {
                sourceChannel = Bass.CreateStream(Path.Combine(CurrentDirectory, this.FileName), 0, 0, this.BassFlags);
            }
            else
            {
                sourceChannel = BassFfmpeg.CreateStream(Path.Combine(CurrentDirectory, this.FileName), 0, 0, this.BassFlags);
            }
            if (sourceChannel == 0)
            {
                Assert.Fail(string.Format("Failed to create source stream: {0}", Enum.GetName(typeof(Errors), Bass.LastError)));
            }

            var channelInfo = default(ChannelInfo);
            if (!Bass.ChannelGetInfo(sourceChannel, out channelInfo))
            {
                Assert.Fail(string.Format("Failed to get stream info: {0}", Enum.GetName(typeof(Errors), Bass.LastError)));
            }
            Assert.AreEqual(BassFfmpeg.ChannelType, channelInfo.ChannelType);

            if (!Bass.ChannelPlay(sourceChannel))
            {
                Assert.Fail(string.Format("Failed to play the playback stream: {0}", Enum.GetName(typeof(Errors), Bass.LastError)));
            }

            var channelLength = Bass.ChannelGetLength(sourceChannel);
            var channelLengthSeconds = Bass.ChannelBytes2Seconds(sourceChannel, channelLength);

            Bass.ChannelSetPosition(sourceChannel, channelLength, PositionFlags.Bytes);

            var channelPosition = Bass.ChannelGetPosition(sourceChannel);
            var channelPositionSeconds = Bass.ChannelBytes2Seconds(sourceChannel, channelPosition);

            Assert.AreEqual(Math.Floor(this.Length.TotalSeconds), Math.Floor(channelPositionSeconds));

            if (!Bass.StreamFree(sourceChannel))
            {
                Assert.Fail(string.Format("Failed to free the source stream: {0}", Enum.GetName(typeof(Errors), Bass.LastError)));
            }
        }

        /// <summary>
        /// Check length.
        /// </summary>
        [Test]
        public void Test003()
        {
            var sourceChannel = default(int);
            if (this.Plugin)
            {
                sourceChannel = Bass.CreateStream(Path.Combine(CurrentDirectory, this.FileName), 0, 0, this.BassFlags);
            }
            else
            {
                sourceChannel = BassFfmpeg.CreateStream(Path.Combine(CurrentDirectory, this.FileName), 0, 0, this.BassFlags);
            }
            if (sourceChannel == 0)
            {
                Assert.Fail(string.Format("Failed to create source stream: {0}", Enum.GetName(typeof(Errors), Bass.LastError)));
            }

            var channelInfo = default(ChannelInfo);
            if (!Bass.ChannelGetInfo(sourceChannel, out channelInfo))
            {
                Assert.Fail(string.Format("Failed to get stream info: {0}", Enum.GetName(typeof(Errors), Bass.LastError)));
            }
            Assert.AreEqual(BassFfmpeg.ChannelType, channelInfo.ChannelType);

            var channelLength = Bass.ChannelGetLength(sourceChannel);
            var channelLengthSeconds = Bass.ChannelBytes2Seconds(sourceChannel, channelLength);

            Assert.AreEqual(Math.Floor(this.Length.TotalSeconds), Math.Floor(channelLengthSeconds));

            if (!Bass.StreamFree(sourceChannel))
            {
                Assert.Fail(string.Format("Failed to free the source stream: {0}", Enum.GetName(typeof(Errors), Bass.LastError)));
            }
        }

        [TestCase(100)]
        public void Test004(int iterations)
        {
            for (var a = 0; a < iterations; a++)
            {
                var sourceChannel = default(int);
                if (this.Plugin)
                {
                    sourceChannel = Bass.CreateStream(Path.Combine(CurrentDirectory, this.FileName), 0, 0, this.BassFlags);
                }
                else
                {
                    sourceChannel = BassFfmpeg.CreateStream(Path.Combine(CurrentDirectory, this.FileName), 0, 0, this.BassFlags);
                }
                if (sourceChannel == 0)
                {
                    Assert.Fail(string.Format("Failed to create source stream: {0}", Enum.GetName(typeof(Errors), Bass.LastError)));
                }

                var channelInfo = default(ChannelInfo);
                if (!Bass.ChannelGetInfo(sourceChannel, out channelInfo))
                {
                    Assert.Fail(string.Format("Failed to get stream info: {0}", Enum.GetName(typeof(Errors), Bass.LastError)));
                }
                Assert.AreEqual(BassFfmpeg.ChannelType, channelInfo.ChannelType);

                if (!Bass.StreamFree(sourceChannel))
                {
                    Assert.Fail(string.Format("Failed to free the source stream: {0}", Enum.GetName(typeof(Errors), Bass.LastError)));
                }
            }
        }
    }
}