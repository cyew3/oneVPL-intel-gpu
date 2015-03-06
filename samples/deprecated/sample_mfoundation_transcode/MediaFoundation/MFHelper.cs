#region Usings

using System;
using System.Globalization;
using System.Runtime.InteropServices;
using Intel.MediaSDK.Samples.WPFTranscode.MediaFoundation.Interop.Interface;

#endregion

namespace Intel.MediaSDK.Samples.WPFTranscode.MediaFoundation {
    internal static class MFHelper {
        private const ulong mediaFoundationVersion = 0x0270;

        [DllImport("mfplat.dll", EntryPoint = "MFStartup")]
        private static extern int ExternMFStartup(
            [In] ulong IVersion,
            [In] uint dwFlags);

        [DllImport("mfplat.dll", EntryPoint = "MFShutdown")]
        private static extern int ExternMFShutdown();

        [DllImport("mfplat.dll", EntryPoint = "MFCreateMediaType")]
        private static extern int ExternMFCreateMediaType(
            [Out, MarshalAs(UnmanagedType.Interface)] out IMFMediaType ppMFType);

        [DllImport("mfplat.dll", EntryPoint = "MFCreateSourceResolver")]
        private static extern int ExternMFCreateSourceResolver(
            [Out, MarshalAs(UnmanagedType.Interface)] out IMFSourceResolver ppISourceResolver);

        [DllImport("mfplat.dll", EntryPoint = "MFCreateAttributes")]
        private static extern int ExternMFCreateAttributes(
            [Out, MarshalAs(UnmanagedType.Interface)] out IMFAttributes ppMFAttributes,
            [In] uint cInitialSize);

        [DllImport("Mf.dll", EntryPoint = "MFCreateMediaSession")]
        private static extern int ExternMFCreateMediaSession(
            [In, MarshalAs(UnmanagedType.Interface)] IMFAttributes pConfiguration,
            [Out, MarshalAs(UnmanagedType.Interface)] out IMFMediaSession ppMS);

        [DllImport("Mf.dll", EntryPoint = "MFCreateTranscodeProfile")]
        private static extern int ExternMFCreateTranscodeProfile(
            [Out, MarshalAs(UnmanagedType.Interface)] out IMFTranscodeProfile ppTranscodeProfile);

        [DllImport("Mf.dll", EntryPoint = "MFTranscodeGetAudioOutputAvailableTypes")]
        private static extern int ExternMFTranscodeGetAudioOutputAvailableTypes(
            [In, MarshalAs(UnmanagedType.LPStruct)] Guid guidSubType,
            [In] uint dwMFTFlags,
            [In] uint pCodecConfig,
            [Out, MarshalAs(UnmanagedType.Interface)] out IMFCollection ppAvailableTypes);

        [DllImport("Mf.dll", EntryPoint = "MFCreateTranscodeTopology")]
        private static extern int ExternMFCreateTranscodeTopology(
            [In, MarshalAs(UnmanagedType.Interface)] IMFMediaSource pSrc,
            [In, MarshalAs(UnmanagedType.LPWStr)] string pwszOutputFilePath,
            [In, MarshalAs(UnmanagedType.Interface)] IMFTranscodeProfile pProfile,
            [Out, MarshalAs(UnmanagedType.Interface)] out IMFTopology ppTranscodeTopo);

        [DllImport("Mf.dll", EntryPoint = "MFCreateTopology")]
        private static extern int ExternMFCreateTopology(
            [Out, MarshalAs(UnmanagedType.Interface)] out IMFTopology ppTopo);

        [DllImport("Mf.dll", EntryPoint = "MFCreateTopologyNode")]
        private static extern int ExternMFCreateTopologyNode(
            [In] uint NodeType,
            [Out, MarshalAs(UnmanagedType.Interface)] out IMFTopologyNode ppNode);

        [DllImport("Mfplat.dll", EntryPoint = "MFCreateFile")]
        private static extern int ExternMFCreateFile(
            [In] uint accessMode,
            [In] uint openMode,
            [In] uint flags,
            [In, MarshalAs(UnmanagedType.LPWStr)] string fileName,
            [Out, MarshalAs(UnmanagedType.Interface)] out IMFByteStream stream);

        [DllImport("Mf.dll", EntryPoint = "MFCreateMPEG4MediaSink")]
        private static extern int ExternMFCreateMPEG4MediaSink(
            [In, MarshalAs(UnmanagedType.Interface)] IMFByteStream pIByteStream,
            [In, MarshalAs(UnmanagedType.Interface)] IMFMediaType pVideoMediaType,
            [In, MarshalAs(UnmanagedType.Interface)] IMFMediaType pAudioMediaType,
            [Out, MarshalAs(UnmanagedType.Interface)] out IMFMediaSink ppIMediaSink);

        [DllImport("Mfplat.dll", EntryPoint = "MFCreateCollection")]
        private static extern int ExternMFCreateCollection(
            [Out, MarshalAs(UnmanagedType.Interface)] out IMFCollection collection);

        /// <summary>
        ///     Starts Media Foundation
        /// </summary>
        /// <remarks>
        ///     Will fail if the OS version is prior Windows 7
        /// </remarks>
        public static void MFStartup() {
            var result = ExternMFStartup(mediaFoundationVersion, 0);
            if (result < 0) {
                throw new COMException("Exception from HRESULT: 0x" + result.ToString("X", NumberFormatInfo.InvariantInfo) + "(MFStartup)", result);
            }
        }

        public static void MFShutdown() {
            var result = ExternMFShutdown();
            if (result < 0) {
                throw new COMException("Exception from HRESULT: 0x" + result.ToString("X", NumberFormatInfo.InvariantInfo) + " (MFShutdown)", result);
            }
        }

        public static IMFMediaType MFCreateMediaType() {
            IMFMediaType mediaType;
            var result = ExternMFCreateMediaType(out mediaType);
            if (result < 0) {
                throw new COMException("Exception from HRESULT: 0x" + result.ToString("X", NumberFormatInfo.InvariantInfo) + " (MFCreateMediaType)", result);
            }
            return mediaType;
        }

        public static IMFSourceResolver MFCreateSourceResolver() {
            IMFSourceResolver sourceResolver;
            var result = ExternMFCreateSourceResolver(out sourceResolver);
            if (result < 0) {
                throw new COMException("Exception from HRESULT: 0x" + result.ToString("X", NumberFormatInfo.InvariantInfo) + " (MFCreateSourceResolver)", result);
            }
            return sourceResolver;
        }

        public static void MFCreateAttributes(out IMFAttributes attributes, uint initialSize) {
            var result = ExternMFCreateAttributes(out attributes, initialSize);
            if (result < 0) {
                throw new COMException("Exception from HRESULT: 0x" + result.ToString("X", NumberFormatInfo.InvariantInfo) + " (MFCreateAttributes)", result);
            }
        }

        public static void MFCreateMediaSession(IMFAttributes configuration, out IMFMediaSession mediaSession) {
            var result = ExternMFCreateMediaSession(configuration, out mediaSession);
            if (result < 0) {
                throw new COMException("Exception from HRESULT: 0x" + result.ToString("X", NumberFormatInfo.InvariantInfo) + " (MFCreateMediaSession)", result);
            }
        }

        public static void MFCreateTranscodeProfile(out IMFTranscodeProfile transcodeProfile) {
            var result = ExternMFCreateTranscodeProfile(out transcodeProfile);
            if (result < 0) {
                throw new COMException("Exception from HRESULT: 0x" + result.ToString("X", NumberFormatInfo.InvariantInfo) + " (MFCreateTranscodeProfile)", result);
            }
        }

        public static void MFTranscodeGetAudioOutputAvailableTypes(
            Guid subType,
            uint flags,
            uint codecConfig,
            out IMFCollection availableTypes) {
            var result = ExternMFTranscodeGetAudioOutputAvailableTypes(subType, flags, codecConfig, out availableTypes);
            if (result != 0) {
                throw new COMException("Exception from HRESULT: 0x" + result.ToString("X", NumberFormatInfo.InvariantInfo) + "(MFTranscodeGetAudioOutputAvailableTypes)", result);
            }
        }

        public static void MFCreateTranscodeTopology(
            IMFMediaSource mediaSource,
            string outputFilePath,
            IMFTranscodeProfile transcodeProfile,
            out IMFTopology transcodeTopology) {
            var result = ExternMFCreateTranscodeTopology(mediaSource, outputFilePath, transcodeProfile, out transcodeTopology);
            if (result < 0) {
                throw new COMException("Exception from HRESULT: 0x" + result.ToString("X", NumberFormatInfo.InvariantInfo) + " (MFCreateTranscodeTopology failed)", result);
            }
        }

        public static IMFTopology MFCreateTopology() {
            IMFTopology topology;
            var result = ExternMFCreateTopology(out topology);
            if (result < 0) {
                throw new COMException("Exception from HRESULT: 0x" + result.ToString("X", NumberFormatInfo.InvariantInfo) + " (MFCreateTopology failed)", result);
            }
            return topology;
        }

        public static IMFTopologyNode MFCreateTopologyNode(NodeType type) {
            IMFTopologyNode topologyNode;
            var result = ExternMFCreateTopologyNode((uint) type, out topologyNode);
            if (result < 0) {
                throw new COMException("Exception from HRESULT: 0x" + result.ToString("X", NumberFormatInfo.InvariantInfo) + " (MFCreateTopologyNode failed)", result);
            }
            return topologyNode;
        }

        public static IMFSourceResolver MFCreateDShowSourceResolver() {
            var mfDShowSourceResolverGuid = new Guid("14D7A407-396B-44B3-BE85-5199A0F0F80A");
            var desktopType = Type.GetTypeFromCLSID(mfDShowSourceResolverGuid, true);
            return (IMFSourceResolver) Activator.CreateInstance(desktopType);
        }

        public static IMFTransform MFCreateTransform(Guid guid) {
            var transformType = Type.GetTypeFromCLSID(guid, true);
            return (IMFTransform) Activator.CreateInstance(transformType);
        }

        public static IMFByteStream MFCreateFile(MFFileAccessMode accessMode, MFFileOpenMode openMode, MFFileFlags flags, string fileName) {
            IMFByteStream stream;
            var result = ExternMFCreateFile((uint) accessMode, (uint) openMode, (uint) flags, fileName, out stream);
            if (result < 0) {
                throw new COMException("Exception from HRESULT: 0x" + result.ToString("X", NumberFormatInfo.InvariantInfo) + " (MFCreateFile failed)", result);
            }
            return stream;
        }

        public static IMFMediaSink MFCreateMPEG4MediaSink(IMFByteStream byteStream, IMFMediaType videoMediaType, IMFMediaType audioMediaType) {
            IMFMediaSink mediaSink;
            var result = ExternMFCreateMPEG4MediaSink(byteStream, videoMediaType, audioMediaType, out mediaSink);
            if (result < 0) {
                throw new COMException("Exception from HRESULT: 0x" + result.ToString("X", NumberFormatInfo.InvariantInfo) + " (MFCreateMPEG4MediaSink failed)", result);
            }
            return mediaSink;
        }

        public static IMFCollection MFCreateCollection() {
            IMFCollection collection;
            var result = ExternMFCreateCollection(out collection);
            if (result < 0) {
                throw new COMException("Exception from HRESULT: 0x" + result.ToString("X", NumberFormatInfo.InvariantInfo) + " (MFCreateCollection failed)", result);
            }
            return collection;
        }
    }
}