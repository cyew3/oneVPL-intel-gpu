#region Usings

using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using Intel.MediaSDK.Samples.WPFTranscode.MediaFoundation.Interop.Constant;

#endregion

namespace Intel.MediaSDK.Samples.WPFTranscode.MediaFoundation.Interop.Interface {
    [ComImport, Guid(IID.IMFSourceResolver), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    internal interface IMFSourceResolver {
        [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
        void CreateObjectFromURL([In, MarshalAs(UnmanagedType.LPWStr)] string pwszURL, [In] uint dwFlags, [In, MarshalAs(UnmanagedType.Interface)] IPropertyStore pProps, out uint pObjectType, [MarshalAs(UnmanagedType.IUnknown)] out object ppObject);

        [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
        void CreateObjectFromByteStream([In, MarshalAs(UnmanagedType.Interface)] object pByteStream, [In, MarshalAs(UnmanagedType.LPWStr)] string pwszURL, [In] uint dwFlags, [In, MarshalAs(UnmanagedType.Interface)] IPropertyStore pProps, out uint pObjectType, [MarshalAs(UnmanagedType.IUnknown)] out object ppObject);

        [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
        void BeginCreateObjectFromURL([In, MarshalAs(UnmanagedType.LPWStr)] string pwszURL, [In] uint dwFlags, [In, MarshalAs(UnmanagedType.Interface)] IPropertyStore pProps, [Out, MarshalAs(UnmanagedType.Interface)] out object ppIUnknownCancelCookie, [In, MarshalAs(UnmanagedType.Interface)] IMFAsyncCallback pCallback, [In, MarshalAs(UnmanagedType.IUnknown)] object punkState);

        [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
        void EndCreateObjectFromURL([In, MarshalAs(UnmanagedType.IUnknown)] object pResult, out uint pObjectType, [MarshalAs(UnmanagedType.IUnknown)] out object ppObject);

        [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
        void BeginCreateObjectFromByteStream([In, MarshalAs(UnmanagedType.Interface)] object pByteStream, [In, MarshalAs(UnmanagedType.LPWStr)] string pwszURL, [In] uint dwFlags, [In, MarshalAs(UnmanagedType.Interface)] IPropertyStore pProps, [Out, MarshalAs(UnmanagedType.IUnknown)] out IMFAsyncCallback ppIUnknownCancelCookie, [In, MarshalAs(UnmanagedType.Interface)] object pCallback, [In, MarshalAs(UnmanagedType.IUnknown)] object punkState);

        [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
        void EndCreateObjectFromByteStream([In, MarshalAs(UnmanagedType.IUnknown)] object pResult, out uint pObjectType, [MarshalAs(UnmanagedType.IUnknown)] out object ppObject);

        [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
        void CancelObjectCreation([In, MarshalAs(UnmanagedType.IUnknown)] object pIUnknownCancelCookie);
    }
}