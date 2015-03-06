#region Usings

using System;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using Intel.MediaSDK.Samples.WPFTranscode.MediaFoundation.Interop.Constant;

#endregion

namespace Intel.MediaSDK.Samples.WPFTranscode.MediaFoundation.Interop.Interface {
    [ComImport, Guid(IID.IMFTopologyNode), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    internal interface IMFTopologyNode {
        [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
        void GetItem([In, MarshalAs(UnmanagedType.LPStruct)] Guid guidKey, [In, Out] ref object pValue);

        [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
        void GetItemType([In, MarshalAs(UnmanagedType.LPStruct)] Guid guidKey, out uint pType);

        [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
        void CompareItem([In, MarshalAs(UnmanagedType.LPStruct)] Guid guidKey, ref object Value, out int pbResult);

        [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
        void Compare([MarshalAs(UnmanagedType.Interface)] IMFAttributes pTheirs, uint MatchType, out int pbResult);

        [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
        void GetUINT32([In, MarshalAs(UnmanagedType.LPStruct)] Guid guidKey, out uint punValue);

        [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
        void GetUINT64([In, MarshalAs(UnmanagedType.LPStruct)] Guid guidKey, out ulong punValue);

        [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
        void GetDouble([In, MarshalAs(UnmanagedType.LPStruct)] Guid guidKey, out double pfValue);

        [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
        void GetGUID([In, MarshalAs(UnmanagedType.LPStruct)] Guid guidKey, out Guid pguidValue);

        [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
        void GetStringLength([In, MarshalAs(UnmanagedType.LPStruct)] Guid guidKey, out uint pcchLength);

        [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
        void GetString([In, MarshalAs(UnmanagedType.LPStruct)] Guid guidKey, [Out, MarshalAs(UnmanagedType.LPWStr)] string pwszValue, uint cchBufSize, [Out] out uint pcchLength);

        [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
        void GetAllocatedString([In, MarshalAs(UnmanagedType.LPStruct)] Guid guidKey, [MarshalAs(UnmanagedType.LPWStr)] out string ppwszValue, out uint pcchLength);

        [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
        void GetBlobSize([In, MarshalAs(UnmanagedType.LPStruct)] Guid guidKey, out uint pcbBlobSize);

        [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
        void GetBlob([In, MarshalAs(UnmanagedType.LPStruct)] Guid guidKey, [Out, MarshalAs(UnmanagedType.LPArray)] byte[] pBuf, uint cbBufSize, [Out] out uint pcbBlobSize);

        [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
        void GetAllocatedBlob([In, MarshalAs(UnmanagedType.LPStruct)] Guid guidKey, [Out] IntPtr ppBuf, out uint pcbSize);

        [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
        void GetUnknown([In, MarshalAs(UnmanagedType.LPStruct)] Guid guidKey, ref Guid riid, out IntPtr ppv);

        [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
        void SetItem([In, MarshalAs(UnmanagedType.LPStruct)] Guid guidKey, ref object Value);

        [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
        void DeleteItem([In, MarshalAs(UnmanagedType.LPStruct)] Guid guidKey);

        [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
        void DeleteAllItems();

        [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
        void SetUINT32([In, MarshalAs(UnmanagedType.LPStruct)] Guid guidKey, uint unValue);

        [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
        void SetUINT64([In, MarshalAs(UnmanagedType.LPStruct)] Guid guidKey, ulong unValue);

        [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
        void SetDouble([In, MarshalAs(UnmanagedType.LPStruct)] Guid guidKey, double fValue);

        [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
        void SetGUID([In, MarshalAs(UnmanagedType.LPStruct)] Guid guidKey, [In, MarshalAs(UnmanagedType.LPStruct)] Guid guidValue);

        [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
        void SetString([In, MarshalAs(UnmanagedType.LPStruct)] Guid guidKey, [In, MarshalAs(UnmanagedType.LPWStr)] string wszValue);

        [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
        void SetBlob([In, MarshalAs(UnmanagedType.LPStruct)] Guid guidKey, [In, MarshalAs(UnmanagedType.LPArray, SizeParamIndex = 2)] byte[] pBuf, uint cbBufSize);

        [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
        void SetUnknown([In, MarshalAs(UnmanagedType.LPStruct)] Guid guidKey, [In, MarshalAs(UnmanagedType.IUnknown)] object pUnknown);

        [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
        void LockStore();

        [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
        void UnlockStore();

        [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
        void GetCount(out uint pcItems);

        [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
        void GetItemByIndex(uint unIndex, out Guid pguidKey, [In, Out] ref object pValue);

        [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
        void CopyAllItems([In, MarshalAs(UnmanagedType.Interface)] IMFAttributes pDest);

        [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
        void SetObject([In, MarshalAs(UnmanagedType.IUnknown)] object pObject);

        [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
        void GetObject([MarshalAs(UnmanagedType.IUnknown)] out object ppObject);

        [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
        void GetNodeType(out uint pType);

        [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
        void GetTopoNodeID(out ulong pID);

        [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
        void SetTopoNodeID([In] ulong ullTopoID);

        [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
        void GetInputCount(out uint pcInputs);

        [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
        void GetOutputCount(out uint pcOutputs);

        [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
        void ConnectOutput([In] uint dwOutputIndex, [In, MarshalAs(UnmanagedType.Interface)] IMFTopologyNode pDownstreamNode, [In] uint dwInputIndexOnDownstreamNode);

        [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
        void DisconnectOutput([In] uint dwOutputIndex);

        [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
        void GetInput([In] uint dwInputIndex, [MarshalAs(UnmanagedType.Interface)] out IMFTopologyNode ppUpstreamNode, out uint pdwOutputIndexOnUpstreamNode);

        [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
        void GetOutput([In] uint dwOutputIndex, [MarshalAs(UnmanagedType.Interface)] out IMFTopologyNode ppDownstreamNode, out uint pdwInputIndexOnDownstreamNode);

        [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
        void GetOutputPrefType([In] uint dwOutputIndex, [MarshalAs(UnmanagedType.Interface)] out IMFMediaType ppType);

        [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
        void SetOutputPrefType([In] uint dwOutputIndex, [In, MarshalAs(UnmanagedType.Interface)] IMFMediaType pType);

        [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
        void RemoteGetOutputPrefType([In] uint dwOutputIndex, out uint pcbData, [Out] IntPtr ppbData);

        [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
        void SetInputPrefType([In] uint dwInputIndex, [In, MarshalAs(UnmanagedType.Interface)] IMFMediaType pType);

        [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
        void RemoteGetInputPrefType([In] uint dwInputIndex, out uint pcbData, [Out] IntPtr ppbData);

        [MethodImpl(MethodImplOptions.InternalCall, MethodCodeType = MethodCodeType.Runtime)]
        void CloneFrom([In, MarshalAs(UnmanagedType.Interface)] IMFTopologyNode pNode);
    }
}