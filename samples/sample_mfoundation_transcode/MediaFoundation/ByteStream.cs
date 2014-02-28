#region Usings

using Intel.MediaSDK.Samples.WPFTranscode.MediaFoundation.Interop.Interface;

#endregion

namespace Intel.MediaSDK.Samples.WPFTranscode.MediaFoundation {
    public class ByteStream {
        public ByteStream(string fileName) {
            MFByteStream = MFHelper.MFCreateFile(MFFileAccessMode.ReadWrite, MFFileOpenMode.DeleteIfExist, MFFileFlags.None, fileName);
        }

        internal IMFByteStream MFByteStream { get; private set; }
    }
}