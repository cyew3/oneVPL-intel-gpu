#region Usings

using System;
using System.Runtime.InteropServices;
using System.Security;

#endregion

namespace Intel.MediaSDK.Samples.WPFTranscode.MediaFoundation.Interop.Interface {
    [ComImport, SuppressUnmanagedCodeSecurity, InterfaceType(ComInterfaceType.InterfaceIsIUnknown),
     Guid("BF94C121-5B05-4E6F-8000-BA598961414D")]
    internal interface IMFTransform {
        void GetStreamLimits(
            [Out] int pdwInputMinimum,
            [Out] int pdwInputMaximum,
            [Out] int pdwOutputMinimum,
            [Out] int pdwOutputMaximum
            );

        void GetStreamCount(
            [Out] int pcInputStreams,
            [Out] int pcOutputStreams
            );

        void GetStreamIDs(
            int dwInputIDArraySize,
            [Out, MarshalAs(UnmanagedType.LPArray, SizeParamIndex = 0)] int[] pdwInputIDs,
            int dwOutputIDArraySize,
            [Out, MarshalAs(UnmanagedType.LPArray, SizeParamIndex = 2)] int[] pdwOutputIDs
            );

        void GetInputStreamInfo(
            int dwInputStreamID,
            out MFTInputStreamInfo pStreamInfo
            );

        void GetOutputStreamInfo(
            int dwOutputStreamID,
            out MFTOutputStreamInfo pStreamInfo
            );

        void GetAttributes(
            [MarshalAs(UnmanagedType.Interface)] out IMFAttributes pAttributes
            );

        void GetInputStreamAttributes(
            int dwInputStreamID,
            [MarshalAs(UnmanagedType.Interface)] out IMFAttributes pAttributes
            );

        void GetOutputStreamAttributes(
            int dwOutputStreamID,
            [MarshalAs(UnmanagedType.Interface)] out IMFAttributes pAttributes
            );

        void DeleteInputStream(
            int dwStreamID
            );

        void AddInputStreams(
            int cStreams,
            [In, MarshalAs(UnmanagedType.LPArray, SizeParamIndex = 0)] int[] adwStreamIDs
            );

        void GetInputAvailableType(
            int dwInputStreamID,
            int dwTypeIndex,
            [MarshalAs(UnmanagedType.Interface)] out IMFMediaType ppType
            );

        void GetOutputAvailableType(
            int dwOutputStreamID,
            int dwTypeIndex,
            [MarshalAs(UnmanagedType.Interface)] out IMFMediaType ppType
            );

        void SetInputType(
            int dwInputStreamID,
            [In, MarshalAs(UnmanagedType.Interface)] IMFMediaType pType,
            MFTSetTypeFlags dwFlags
            );

        void SetOutputType(
            int dwOutputStreamID,
            [In, MarshalAs(UnmanagedType.Interface)] IMFMediaType pType,
            MFTSetTypeFlags dwFlags
            );

        void GetInputCurrentType(
            int dwInputStreamID,
            [MarshalAs(UnmanagedType.Interface)] out IMFMediaType ppType
            );

        void GetOutputCurrentType(
            int dwOutputStreamID,
            [MarshalAs(UnmanagedType.Interface)] out IMFMediaType ppType
            );

        void GetInputStatus(
            int dwInputStreamID,
            out MFTInputStatusFlags pdwFlags
            );

        void GetOutputStatus(
            out MFTOutputStatusFlags pdwFlags
            );

        void SetOutputBounds(
            long hnsLowerBound,
            long hnsUpperBound
            );

        void ProcessEvent(
            int dwInputStreamID,
            [In, MarshalAs(UnmanagedType.Interface)] IMFMediaEvent pEvent
            );

        void ProcessMessage(
            MFTMessageType eMessage,
            IntPtr ulParam
            );

        void ProcessInput(
            int dwInputStreamID,
            [MarshalAs(UnmanagedType.Interface)] IMFSample pSample,
            int dwFlags // Must be zero
            );

        void ProcessOutput(
            MFTProcessOutputFlags dwFlags,
            int cOutputBufferCount,
            [In, Out, MarshalAs(UnmanagedType.LPArray, SizeParamIndex = 1)] MFTOutputDataBuffer[] pOutputSamples,
            out ProcessOutputStatus pdwStatus
            );
    }

    [Flags]
    internal enum MFTProcessOutputFlags {
        None = 0,
        DiscardWhenNoBuffer = 0x00000001
    }

    [Flags]
    internal enum MFTOutputStatusFlags {
        None = 0,
        SampleReady = 0x00000001
    }

    [Flags]
    internal enum MFTInputStatusFlags {
        None = 0,
        AcceptData = 0x00000001
    }

    [Flags]
    internal enum MFTSetTypeFlags {
        None = 0,
        TestOnly = 0x00000001
    }

    [Flags]
    internal enum MFTOutputStreamInfoFlags {
        None = 0,
        WholeSamples = 0x00000001,
        SingleSamplePerBuffer = 0x00000002,
        FixedSampleSize = 0x00000004,
        Discardable = 0x00000008,
        Optional = 0x00000010,
        ProvidesSamples = 0x00000100,
        CanProvideSamples = 0x00000200,
        LazyRead = 0x00000400,
        Removable = 0x00000800
    }

    [Flags]
    internal enum MFTInputStreamInfoFlags {
        WholeSamples = 0x1,
        SingleSamplePerBuffer = 0x2,
        FixedSampleSize = 0x4,
        HoldsBuffers = 0x8,
        DoesNotAddRef = 0x100,
        Removable = 0x200,
        Optional = 0x400,
        ProcessesInPlace = 0x800
    }

    internal enum MFTMessageType {
        CommandDrain = 1,
        CommandFlush = 0,
        NotifyBeginStreaming = 0x10000000,
        NotifyEndOfStream = 0x10000002,
        NotifyEndStreaming = 0x10000001,
        NotifyStartOfStream = 0x10000003,
        SetD3DManager = 2
    }

    [Flags]
    internal enum MFTOutputDataBufferFlags {
        None = 0,
        Incomplete = 0x01000000,
        FormatChange = 0x00000100,
        StreamEnd = 0x00000200,
        NoSample = 0x00000300
    };

    [Flags]
    internal enum ProcessOutputStatus {
        None = 0,
        NewStreams = 0x00000100
    }

    [StructLayout(LayoutKind.Sequential, Pack = 8)]
    internal struct MFTInputStreamInfo {
        internal long hnsMaxLatency;
        internal MFTInputStreamInfoFlags dwFlags;
        internal int cbSize;
        internal int cbMaxLookahead;
        internal int cbAlignment;
    }

    [StructLayout(LayoutKind.Sequential)]
    internal struct MFTOutputDataBuffer {
        internal int dwStreamID;
        internal IntPtr pSample; // Doesn't release correctly when marshaled as IMFSample
        internal MFTOutputDataBufferFlags dwStatus;
        [MarshalAs(UnmanagedType.Interface)] internal IMFCollection pEvents;
    }

    [StructLayout(LayoutKind.Sequential, Pack = 4)]
    internal struct MFTOutputStreamInfo {
        internal MFTOutputStreamInfoFlags dwFlags;
        internal int cbSize;
        internal int cbAlignment;
    }
}