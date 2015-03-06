#region Usings

using Intel.MediaSDK.Samples.WPFTranscode.MediaFoundation.Interop.Interface;

#endregion

namespace Intel.MediaSDK.Samples.WPFTranscode.MediaFoundation {
    public class StreamDescriptor {
        internal StreamDescriptor(IMFStreamDescriptor mfStreamDescriptor) {
            MFStreamDescriptor = mfStreamDescriptor;
        }

        internal IMFStreamDescriptor MFStreamDescriptor { get; private set; }

        public MediaTypeHandler MediaTypeHandler {
            get {
                return new MediaTypeHandler(Com.Get((out IMFMediaTypeHandler _) =>
                                                    MFStreamDescriptor.GetMediaTypeHandler(out _)));
            }
        }
    }
}