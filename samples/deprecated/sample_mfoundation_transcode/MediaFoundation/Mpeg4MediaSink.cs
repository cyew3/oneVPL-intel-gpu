#region Usings

using Intel.MediaSDK.Samples.WPFTranscode.MediaFoundation.Interop.Interface;

#endregion

namespace Intel.MediaSDK.Samples.WPFTranscode.MediaFoundation {
    public class Mpeg4MediaSink {
        public Mpeg4MediaSink(ByteStream byteStream, AudioMediaType audioMediaType, VideoMediaType videoMediaType) {
            MFMediaSink = MFHelper.MFCreateMPEG4MediaSink(
                byteStream.MFByteStream,
                videoMediaType == null ? null : videoMediaType.MFMediaType,
                audioMediaType == null ? null : audioMediaType.MFMediaType);
        }

        internal IMFMediaSink MFMediaSink { get; private set; }

        internal IMFStreamSink GetStreamSinkByIndex(int index) {
            return Com.Get((int _, out IMFStreamSink __) => MFMediaSink.GetStreamSinkByIndex(_, out __), index);
        }
    }
}