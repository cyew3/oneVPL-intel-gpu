#region Usings

using System;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using Intel.MediaSDK.Samples.WPFTranscode.MediaFoundation.Interop.Constant;

#endregion

namespace Intel.MediaSDK.Samples.WPFTranscode.MediaFoundation.Interop.Interface {
    [ComImport, Guid(IID.IMFReadWriteClassFactory), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    internal interface IMFReadWriteClassFactory {
        [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
        void CreateInstanceFromURL([In, MarshalAs(UnmanagedType.LPStruct)] Guid clsid, [In, MarshalAs(UnmanagedType.LPWStr)] string pwszURL, [In, MarshalAs(UnmanagedType.Interface)] IMFAttributes pAttributes, [In, MarshalAs(UnmanagedType.LPStruct)] Guid riid, [Out, MarshalAs(UnmanagedType.Interface)] out object ppvObject);

        [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
        void CreateInstanceFromObject([In, MarshalAs(UnmanagedType.LPStruct)] Guid clsid, [In, MarshalAs(UnmanagedType.IUnknown)] object punkObject, [In, MarshalAs(UnmanagedType.Interface)] IMFAttributes pAttributes, [In, MarshalAs(UnmanagedType.LPStruct)] Guid riid, [Out, MarshalAs(UnmanagedType.Interface)] out object ppvObject);
    }
}