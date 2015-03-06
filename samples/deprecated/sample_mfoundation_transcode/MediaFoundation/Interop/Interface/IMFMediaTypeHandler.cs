#region Usings

using System;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using Intel.MediaSDK.Samples.WPFTranscode.MediaFoundation.Interop.Constant;

#endregion

namespace Intel.MediaSDK.Samples.WPFTranscode.MediaFoundation.Interop.Interface {
    [ComImport, Guid(IID.IMFMediaTypeHandler), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    internal interface IMFMediaTypeHandler {
        [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
        void IsMediaTypeSupported([In, MarshalAs(UnmanagedType.Interface)] IMFMediaType pMediaType, [MarshalAs(UnmanagedType.Interface)] out IMFMediaType ppMediaType);

        [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
        void GetMediaTypeCount(out uint pdwTypeCount);

        [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
        void GetMediaTypeByIndex([In] uint dwIndex, [MarshalAs(UnmanagedType.Interface)] out IMFMediaType ppType);

        [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
        void SetCurrentMediaType([In, MarshalAs(UnmanagedType.Interface)] IMFMediaType pMediaType);

        [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
        void GetCurrentMediaType([Out] out IMFMediaType pMediaType);

        [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
        void GetMajorType(out Guid pguidMajorType);
    }
}