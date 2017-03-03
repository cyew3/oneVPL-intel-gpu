#region Usings

using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using Intel.MediaSDK.Samples.WPFTranscode.MediaFoundation.Interop.Constant;

#endregion

namespace Intel.MediaSDK.Samples.WPFTranscode.MediaFoundation.Interop.Interface {
    [ComImport, Guid(IID.IMFCollection), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    internal interface IMFCollection {
        [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
        void GetElementCount(out uint pcElements);

        [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
        void GetElement([In] uint dwElementIndex, [MarshalAs(UnmanagedType.IUnknown)] out object ppUnkElement);

        [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
        void AddElement([In, MarshalAs(UnmanagedType.IUnknown)] object pUnkElement);

        [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
        void RemoveElement([In] uint dwElementIndex, [MarshalAs(UnmanagedType.IUnknown)] out object ppUnkElement);

        [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
        void InsertElementAt([In] uint dwIndex, [In, MarshalAs(UnmanagedType.IUnknown)] object pUnknown);

        [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
        void RemoveAllElements();
    }
}