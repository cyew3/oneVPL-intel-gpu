#region Usings

using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using Intel.MediaSDK.Samples.WPFTranscode.MediaFoundation.Interop.Constant;

#endregion

namespace Intel.MediaSDK.Samples.WPFTranscode.MediaFoundation.Interop.Interface {
    [ComImport, Guid(IID.IMFTranscodeProfile), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    internal interface IMFTranscodeProfile {
        [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
        void SetAudioAttributes([In, MarshalAs(UnmanagedType.Interface)] IMFAttributes pAttrs);

        [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
        void GetAudioAttributes([MarshalAs(UnmanagedType.Interface)] out IMFAttributes ppAttrs);

        [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
        void SetVideoAttributes([In, MarshalAs(UnmanagedType.Interface)] IMFAttributes pAttrs);

        [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
        void GetVideoAttributes([MarshalAs(UnmanagedType.Interface)] out IMFAttributes ppAttrs);

        [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
        void SetContainerAttributes([In, MarshalAs(UnmanagedType.Interface)] IMFAttributes pAttrs);

        [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
        void GetContainerAttributes([MarshalAs(UnmanagedType.Interface)] out IMFAttributes ppAttrs);
    }
}