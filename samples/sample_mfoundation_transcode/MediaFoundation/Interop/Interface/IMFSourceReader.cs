#region Usings

using System;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using Intel.MediaSDK.Samples.WPFTranscode.MediaFoundation.Interop.Constant;

#endregion

namespace Intel.MediaSDK.Samples.WPFTranscode.MediaFoundation.Interop.Interface {
    [ComImport, Guid(IID.IMFSourceReader), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    internal interface IMFSourceReader {
        [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
        void GetStreamSelection([In] uint dwStreamIndex, [Out, MarshalAs(UnmanagedType.Bool)] out bool pfSelected);

        [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
        void SetStreamSelection([In] uint dwStreamIndex, [In, MarshalAs(UnmanagedType.Bool)] bool fSelected);

        [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
        void GetNativeMediaType([In] uint dwStreamIndex, [In] uint dwMediaTypeIndex, [MarshalAs(UnmanagedType.Interface)] out IMFMediaType ppMediaType);

        [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
        void GetCurrentMediaType([In] uint dwStreamIndex, [MarshalAs(UnmanagedType.Interface)] out IMFMediaType ppMediaType);

        [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
        void SetCurrentMediaType([In] uint dwStreamIndex, [In, Out] IntPtr pdwReserved, [In, MarshalAs(UnmanagedType.Interface)] IMFMediaType pMediaType);

        [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
        void SetCurrentPosition([In, MarshalAs(UnmanagedType.LPStruct)] Guid guidTimeFormat, [In] ref object varPosition);

        [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
        void ReadSample([In] uint dwStreamIndex, [In] uint dwControlFlags, out uint pdwActualStreamIndex, out uint pdwStreamFlags, out ulong pllTimestamp, [MarshalAs(UnmanagedType.Interface)] out IMFSample ppSample);

        [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
        void Flush([In] uint dwStreamIndex);

        [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
        void GetServiceForStream([In] uint dwStreamIndex, [In, MarshalAs(UnmanagedType.LPStruct)] Guid guidService, [In, MarshalAs(UnmanagedType.LPStruct)] Guid riid, out IntPtr ppvObject);

        [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
        void GetPresentationAttribute([In] uint dwStreamIndex, [In, MarshalAs(UnmanagedType.LPStruct)] Guid guidAttribute, out object pvarAttribute);
    }
}