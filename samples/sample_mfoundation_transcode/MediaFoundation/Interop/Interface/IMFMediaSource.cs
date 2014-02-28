#region Usings

using System;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using Intel.MediaSDK.Samples.WPFTranscode.MediaFoundation.Interop.Constant;

#endregion

namespace Intel.MediaSDK.Samples.WPFTranscode.MediaFoundation.Interop.Interface {
    [ComImport, Guid(IID.IMFMediaSource), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    internal interface IMFMediaSource {
        [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
        void GetEvent([In] uint dwFlags, [MarshalAs(UnmanagedType.Interface)] out IMFMediaEvent ppEvent);

        [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
        void BeginGetEvent([In, MarshalAs(UnmanagedType.Interface)] IMFAsyncCallback pCallback, [In, MarshalAs(UnmanagedType.IUnknown)] object punkState);

        [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
        void EndGetEvent([In, MarshalAs(UnmanagedType.Interface)] IMFAsyncResult pResult, [Out, MarshalAs(UnmanagedType.Interface)] out IMFMediaEvent ppEvent);

        [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
        void QueueEvent([In] uint met, [In, MarshalAs(UnmanagedType.LPStruct)] Guid guidExtendedType, [In, MarshalAs(UnmanagedType.Error)] int hrStatus, [In] ref object pvValue);

        [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
        void GetCharacteristics(out uint pdwCharacteristics);

        [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
        void CreatePresentationDescriptor([MarshalAs(UnmanagedType.Interface)] out IMFPresentationDescriptor ppPresentationDescriptor);

        [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
        void Start([In, MarshalAs(UnmanagedType.Interface)] IMFPresentationDescriptor pPresentationDescriptor, [In, MarshalAs(UnmanagedType.LPStruct)] Guid pguidTimeFormat, [In] object pvarStartPosition);

        [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
        void Stop();

        [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
        void Pause();

        [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
        void Shutdown();
    }
}