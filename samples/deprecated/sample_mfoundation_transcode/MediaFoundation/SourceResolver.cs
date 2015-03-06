#region Usings

using System;
using Intel.MediaSDK.Samples.WPFTranscode.MediaFoundation.Interop.Interface;

#endregion

namespace Intel.MediaSDK.Samples.WPFTranscode.MediaFoundation {
    public class NativeSourceResolver : SourceResolver {
        internal NativeSourceResolver() : base(MFHelper.MFCreateSourceResolver()) {}
    }

    public class DirectShowSourceResolver : SourceResolver {
        internal DirectShowSourceResolver() : base(MFHelper.MFCreateDShowSourceResolver()) {}
    }

    public class SourceResolver {
        internal SourceResolver(IMFSourceResolver mfSourceResolver) {
            MFSourceResolver = mfSourceResolver;
        }

        internal IMFSourceResolver MFSourceResolver { get; private set; }

        internal IMFMediaSource GetMFMediaSource(string sourceFileName) {
            object objectSource;
            try {
                uint objectType;
                MFSourceResolver.CreateObjectFromURL(sourceFileName, Consts.MF_RESOLUTION_MEDIASOURCE, null, out objectType, out objectSource);
            }
            catch (Exception) {
                return null;
            }
            return (IMFMediaSource) objectSource;
        }

        public static SourceResolver Create(string sourceFileName) {
            var sourceResolver = new NativeSourceResolver();
            if (sourceResolver.GetMFMediaSource(sourceFileName) != null) {
                return sourceResolver;
            }
            return new DirectShowSourceResolver();
        }
    }
}