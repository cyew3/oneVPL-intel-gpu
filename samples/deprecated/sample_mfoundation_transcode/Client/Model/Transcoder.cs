#region Usings

using System;
using Intel.MediaSDK.Samples.WPFTranscode.MediaFoundation;

#endregion

namespace Intel.MediaSDK.Samples.WPFTranscode.Client.Model {
    public class Transcoder : IDisposable {
        private readonly IFacade facade;
        private MediaSource mediaSource;

        public Transcoder(IFacade facade) {
            this.facade = facade;
            Topology = new Topology();
        }

        public MediaSession MediaSession { get; private set; }
        public Topology Topology { get; private set; }
        public SourcePlugin SourcePlugin { get; private set; }
        public AudioDecoderPlugin AudioDecoderPlugin { get; private set; }
        public AudioEncoderPlugin AudioEncoderPlugin { get; private set; }
        public VideoDecoderPlugin VideoDecoderPlugin { get; private set; }
        public VideoEncoderPlugin VideoEncoderPlugin { get; private set; }
        public TargetPlugin TargetPlugin { get; private set; }

        private bool HasAudioStream {
            get { return SourcePlugin.AudioSourceStreamNode != null; }
        }

        public void InitializeSource(string sourceFileName) {
            mediaSource = new MediaSource(sourceFileName);
            SourcePlugin = new SourcePlugin(mediaSource);
            Topology.Add(SourcePlugin.VideoSourceStreamNode);

            VideoDecoderPlugin = new VideoDecoderPlugin(mediaSource);
            Topology.Add(VideoDecoderPlugin.DecodeTransformNode);
            Topology.Connect(SourcePlugin.VideoSourceStreamNode, VideoDecoderPlugin.DecodeTransformNode);

            if (HasAudioStream) {
                Topology.Add(SourcePlugin.AudioSourceStreamNode);

                if (mediaSource.AudioFormat != AudioFormat.PCM) {
                    AudioDecoderPlugin = new AudioDecoderPlugin(mediaSource);
                    Topology.Add(AudioDecoderPlugin.DecodeTransformNode);
                    Topology.Connect(SourcePlugin.AudioSourceStreamNode, AudioDecoderPlugin.DecodeTransformNode);
                }
            }
        }

        public void InitializeEncoderPlugins(VideoEncoderPluginInit videoEncoderPluginInit, AudioEncoderPluginInit audioEncoderPluginInit) {
            VideoEncoderPlugin = new VideoEncoderPlugin(videoEncoderPluginInit);
            Topology.Add(VideoEncoderPlugin.EncodeTransformNode);
            Topology.Connect(VideoDecoderPlugin.DecodeTransformNode, VideoEncoderPlugin.EncodeTransformNode);

            if (HasAudioStream) {
                AudioEncoderPlugin = new AudioEncoderPlugin(audioEncoderPluginInit);
                Topology.Add(AudioEncoderPlugin.EncodeTransformNode);
                if (mediaSource.AudioFormat == AudioFormat.PCM) {
                    Topology.Connect(SourcePlugin.AudioSourceStreamNode, AudioEncoderPlugin.EncodeTransformNode);
                }
                else {
                    Topology.Connect(AudioDecoderPlugin.DecodeTransformNode, AudioEncoderPlugin.EncodeTransformNode);
                }
            }
        }

        public void InitializeTarget(string targetFileName) {
            TargetPlugin = new TargetPlugin(targetFileName, AudioEncoderPlugin, VideoEncoderPlugin);
            Topology.Add(TargetPlugin.VideoOutputNode);
            Topology.Connect(VideoEncoderPlugin.EncodeTransformNode, TargetPlugin.VideoOutputNode);

            if (HasAudioStream) {
                Topology.Add(TargetPlugin.AudioOutputNode);
                Topology.Connect(AudioEncoderPlugin.EncodeTransformNode, TargetPlugin.AudioOutputNode);
            }
        }

        public void Run() {
            MediaSession = new MediaSession();
            MediaSession.OnFinish += (sender, e) => mediaSource.Shutdown();
            MediaSession.Run(Topology);
        }

        public void Close() {
            if (mediaSource != null) {
                mediaSource.Shutdown();
            }
            facade.Close();
        }

        public void Dispose() {
            Close();
        }
    }
}