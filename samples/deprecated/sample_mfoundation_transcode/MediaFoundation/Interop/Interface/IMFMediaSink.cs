#region Usings

using System;
using System.Globalization;
using System.Runtime.InteropServices;
using System.Security;
using System.Text;

#endregion

namespace Intel.MediaSDK.Samples.WPFTranscode.MediaFoundation.Interop.Interface {

    #region Declarations

    internal enum MFStreamSinkMarkerType {
        Default,
        EndOfSegment,
        Tick,
        Event
    }


    [Flags]
    internal enum MFMediaSinkCharacteristics {
        None = 0,
        FixedStreams = 0x00000001,
        CannotMatchClock = 0x00000002,
        Rateless = 0x00000004,
        ClockRequired = 0x00000008,
        CanPreroll = 0x00000010,
        RequireReferenceMediaType = 0x00000020
    }


    internal enum MFFileAccessMode {
        None = 0,
        Read = 1,
        Write = 2,
        ReadWrite = 3
    }


    internal enum MFFileOpenMode {
        FailIfNotExist = 0,
        FailIfExist = 1,
        ResetIfExist = 2,
        AppendIfExist = 3,
        DeleteIfExist = 4
    }

    [Flags]
    internal enum MFFileFlags {
        None = 0,
        NoBuffering = 0x1
    }

    #endregion

    [ComImport, SuppressUnmanagedCodeSecurity,
     Guid("6EF2A660-47C0-4666-B13D-CBB717F2FA2C"),
     InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    internal interface IMFMediaSink {
        void GetCharacteristics(
            out MFMediaSinkCharacteristics pdwCharacteristics
            );

        void AddStreamSink(
            [In] int dwStreamSinkIdentifier,
            [In, MarshalAs(UnmanagedType.Interface)] IMFMediaType pMediaType,
            [MarshalAs(UnmanagedType.Interface)] out IMFStreamSink ppStreamSink
            );

        void RemoveStreamSink(
            [In] int dwStreamSinkIdentifier
            );

        void GetStreamSinkCount(
            out int pcStreamSinkCount
            );

        void GetStreamSinkByIndex(
            [In] int dwIndex,
            [MarshalAs(UnmanagedType.Interface)] out IMFStreamSink ppStreamSink
            );

        void GetStreamSinkById(
            [In] int dwStreamSinkIdentifier,
            [MarshalAs(UnmanagedType.Interface)] out IMFStreamSink ppStreamSink
            );

        void SetPresentationClock(
            [In, MarshalAs(UnmanagedType.Interface)] IMFPresentationClock pPresentationClock
            );

        void GetPresentationClock(
            [MarshalAs(UnmanagedType.Interface)] out IMFPresentationClock ppPresentationClock
            );

        void Shutdown();
    }

    [ComImport, SuppressUnmanagedCodeSecurity,
     Guid("0A97B3CF-8E7C-4A3D-8F8C-0C843DC247FB"),
     InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    internal interface IMFStreamSink : IMFMediaEventGenerator {
        #region IMFMediaEventGenerator methods

        new void GetEvent(
            [In] MFEventFlag dwFlags,
            [MarshalAs(UnmanagedType.Interface)] out IMFMediaEvent ppEvent
            );

        new void BeginGetEvent(
            [In, MarshalAs(UnmanagedType.Interface)] IMFAsyncCallback pCallback,
            [In, MarshalAs(UnmanagedType.IUnknown)] object o);

        new void EndGetEvent(
            IMFAsyncResult pResult,
            out IMFMediaEvent ppEvent);

        new void QueueEvent(
            [In] MediaEventType met,
            [In, MarshalAs(UnmanagedType.LPStruct)] Guid guidExtendedType,
            [In] int hrStatus,
            [In, MarshalAs(UnmanagedType.LPStruct)] ConstPropVariant pvValue
            );

        #endregion

        void GetMediaSink(
            [MarshalAs(UnmanagedType.Interface)] out IMFMediaSink ppMediaSink
            );

        void GetIdentifier(
            out int pdwIdentifier
            );

        void GetMediaTypeHandler(
            [MarshalAs(UnmanagedType.Interface)] out IMFMediaTypeHandler ppHandler
            );

        void ProcessSample(
            [In, MarshalAs(UnmanagedType.Interface)] IMFSample pSample
            );

        void PlaceMarker(
            [In] MFStreamSinkMarkerType eMarkerType,
            [In, MarshalAs(UnmanagedType.LPStruct)] ConstPropVariant pvarMarkerValue,
            [In, MarshalAs(UnmanagedType.LPStruct)] ConstPropVariant pvarContextValue
            );

        void Flush();
    }

    [StructLayout(LayoutKind.Explicit)]
    public class ConstPropVariant : IDisposable {
        public enum VariantType : short {
            None = 0,
            Short = 2,
            Int32 = 3,
            Float = 4,
            Double = 5,
            IUnknown = 13,
            UByte = 17,
            UShort = 18,
            UInt32 = 19,
            Int64 = 20,
            UInt64 = 21,
            String = 31,
            Guid = 72,
            Blob = 0x1000 + 17,
            StringArray = 0x1000 + 31
        }

        [StructLayout(LayoutKind.Sequential)]
        protected struct Blob {
            public int cbSize;
            public IntPtr pBlobData;
        }

        [StructLayout(LayoutKind.Sequential)]
        protected struct CALPWstr {
            public int cElems;
            public IntPtr pElems;
        }

        #region Member variables

        [FieldOffset(0)] protected VariantType type;

        [FieldOffset(2)] protected short reserved1;

        [FieldOffset(4)] protected short reserved2;

        [FieldOffset(6)] protected short reserved3;

        [FieldOffset(8)] protected short iVal;

        [FieldOffset(8), CLSCompliant(false)] protected ushort uiVal;

        [FieldOffset(8), CLSCompliant(false)] protected byte bVal;

        [FieldOffset(8)] protected int intValue;

        [FieldOffset(8), CLSCompliant(false)] protected uint uintVal;

        [FieldOffset(8)] protected float fltVal;

        [FieldOffset(8)] protected long longValue;

        [FieldOffset(8), CLSCompliant(false)] protected ulong ulongValue;

        [FieldOffset(8)] protected double doubleValue;

        [FieldOffset(8)] protected Blob blobValue;

        [FieldOffset(8)] protected IntPtr ptr;

        [FieldOffset(8)] protected CALPWstr calpwstrVal;

        #endregion

        public ConstPropVariant() {
            type = VariantType.None;
        }

        protected ConstPropVariant(VariantType v) {
            type = v;
        }

        public static explicit operator string(ConstPropVariant f) {
            return f.GetString();
        }

        public static explicit operator string[](ConstPropVariant f) {
            return f.GetStringArray();
        }

        public static explicit operator byte(ConstPropVariant f) {
            return f.GetUByte();
        }

        public static explicit operator short(ConstPropVariant f) {
            return f.GetShort();
        }

        [CLSCompliant(false)]
        public static explicit operator ushort(ConstPropVariant f) {
            return f.GetUShort();
        }

        public static explicit operator int(ConstPropVariant f) {
            return f.GetInt();
        }

        [CLSCompliant(false)]
        public static explicit operator uint(ConstPropVariant f) {
            return f.GetUInt();
        }

        public static explicit operator float(ConstPropVariant f) {
            return f.GetFloat();
        }

        public static explicit operator double(ConstPropVariant f) {
            return f.GetDouble();
        }

        public static explicit operator long(ConstPropVariant f) {
            return f.GetLong();
        }

        [CLSCompliant(false)]
        public static explicit operator ulong(ConstPropVariant f) {
            return f.GetULong();
        }

        public static explicit operator Guid(ConstPropVariant f) {
            return f.GetGuid();
        }

        public static explicit operator byte[](ConstPropVariant f) {
            return f.GetBlob();
        }

        // I decided not to do implicits since perf is likely to be
        // better recycling the PropVariant, and the only way I can
        // see to support Implicit is to create a new PropVariant.
        // Also, since I can't free the previous instance, IUnknowns
        // will linger until the GC cleans up.  Not what I think I
        // want.

        public MFAttributeType GetMFAttributeType() {
            switch (type) {
                case VariantType.None:
                case VariantType.UInt32:
                case VariantType.UInt64:
                case VariantType.Double:
                case VariantType.Guid:
                case VariantType.String:
                case VariantType.Blob:
                case VariantType.IUnknown: {
                    return (MFAttributeType) type;
                }
                default: {
                    throw new Exception("Type is not a MFAttributeType");
                }
            }
        }

        public VariantType GetVariantType() {
            return type;
        }

        public string[] GetStringArray() {
            if (type == VariantType.StringArray) {
                string[] sa;

                var iCount = calpwstrVal.cElems;
                sa = new string[iCount];

                for (var x = 0; x < iCount; x++) {
                    sa[x] = Marshal.PtrToStringUni(Marshal.ReadIntPtr(calpwstrVal.pElems, x*IntPtr.Size));
                }

                return sa;
            }
            throw new ArgumentException("PropVariant contents not a string array");
        }

        public string GetString() {
            if (type == VariantType.String) {
                return Marshal.PtrToStringUni(ptr);
            }
            throw new ArgumentException("PropVariant contents not a string");
        }

        public byte GetUByte() {
            if (type == VariantType.UByte) {
                return bVal;
            }
            throw new ArgumentException("PropVariant contents not a byte");
        }

        public short GetShort() {
            if (type == VariantType.Short) {
                return iVal;
            }
            throw new ArgumentException("PropVariant contents not an Short");
        }

        [CLSCompliant(false)]
        public ushort GetUShort() {
            if (type == VariantType.UShort) {
                return uiVal;
            }
            throw new ArgumentException("PropVariant contents not an UShort");
        }

        public int GetInt() {
            if (type == VariantType.Int32) {
                return intValue;
            }
            throw new ArgumentException("PropVariant contents not an int32");
        }

        [CLSCompliant(false)]
        public uint GetUInt() {
            if (type == VariantType.UInt32) {
                return uintVal;
            }
            throw new ArgumentException("PropVariant contents not an uint32");
        }

        public long GetLong() {
            if (type == VariantType.Int64) {
                return longValue;
            }
            throw new ArgumentException("PropVariant contents not an int64");
        }

        [CLSCompliant(false)]
        public ulong GetULong() {
            if (type == VariantType.UInt64) {
                return ulongValue;
            }
            throw new ArgumentException("PropVariant contents not an uint64");
        }

        public float GetFloat() {
            if (type == VariantType.Float) {
                return fltVal;
            }
            throw new ArgumentException("PropVariant contents not a Float");
        }

        public double GetDouble() {
            if (type == VariantType.Double) {
                return doubleValue;
            }
            throw new ArgumentException("PropVariant contents not a double");
        }

        public Guid GetGuid() {
            if (type == VariantType.Guid) {
                return (Guid) Marshal.PtrToStructure(ptr, typeof (Guid));
            }
            throw new ArgumentException("PropVariant contents not a Guid");
        }

        public byte[] GetBlob() {
            if (type == VariantType.Blob) {
                var b = new byte[blobValue.cbSize];

                Marshal.Copy(blobValue.pBlobData, b, 0, blobValue.cbSize);

                return b;
            }
            throw new ArgumentException("PropVariant contents are not a Blob");
        }

        public object GetIUnknown() {
            if (type == VariantType.IUnknown) {
                return Marshal.GetObjectForIUnknown(ptr);
            }
            throw new ArgumentException("PropVariant contents not an IUnknown");
        }

        public override string ToString() {
            // This method is primarily intended for debugging so that a readable string will show
            // up in the output window
            string sRet;

            switch (type) {
                case VariantType.None: {
                    sRet = "<Empty>";
                    break;
                }

                case VariantType.Blob: {
                    const string FormatString = "x2"; // Hex 2 digit format
                    const int MaxEntries = 16;

                    var blob = GetBlob();

                    // Number of bytes we're going to format
                    var n = Math.Min(MaxEntries, blob.Length);

                    if (n == 0) {
                        sRet = "<Empty Array>";
                    }
                    else {
                        // Only format the first MaxEntries bytes
                        var sb = new StringBuilder(blob[0].ToString(FormatString));
                        for (var i = 1; i < n; i++) {
                            sb.AppendFormat(",{0}", blob[i].ToString(FormatString));
                        }

                        // If the string is longer, add an indicator
                        if (blob.Length > n) {
                            sb.Append("...");
                        }
                        sRet = sb.ToString();
                    }
                    break;
                }

                case VariantType.Float: {
                    sRet = GetFloat().ToString(CultureInfo.InvariantCulture);
                    break;
                }

                case VariantType.Double: {
                    sRet = GetDouble().ToString(CultureInfo.InvariantCulture);
                    break;
                }

                case VariantType.Guid: {
                    sRet = GetGuid().ToString();
                    break;
                }

                case VariantType.IUnknown: {
                    sRet = GetIUnknown().ToString();
                    break;
                }

                case VariantType.String: {
                    sRet = GetString();
                    break;
                }

                case VariantType.Short: {
                    sRet = GetShort().ToString(CultureInfo.InvariantCulture);
                    break;
                }

                case VariantType.UByte: {
                    sRet = GetUByte().ToString(CultureInfo.InvariantCulture);
                    break;
                }

                case VariantType.UShort: {
                    sRet = GetUShort().ToString(CultureInfo.InvariantCulture);
                    break;
                }

                case VariantType.Int32: {
                    sRet = GetInt().ToString(CultureInfo.InvariantCulture);
                    break;
                }

                case VariantType.UInt32: {
                    sRet = GetUInt().ToString(CultureInfo.InvariantCulture);
                    break;
                }

                case VariantType.Int64: {
                    sRet = GetLong().ToString(CultureInfo.InvariantCulture);
                    break;
                }

                case VariantType.UInt64: {
                    sRet = GetULong().ToString(CultureInfo.InvariantCulture);
                    break;
                }

                case VariantType.StringArray: {
                    var sb = new StringBuilder();
                    foreach (var entry in GetStringArray()) {
                        sb.Append((sb.Length == 0 ? "\"" : ",\"") + entry + '\"');
                    }
                    sRet = sb.ToString();
                    break;
                }
                default: {
                    sRet = base.ToString();
                    break;
                }
            }

            return sRet;
        }

        public override int GetHashCode() {
            // Give a (slightly) better hash value in case someone uses PropVariants
            // in a hash table.
            int iRet;

            switch (type) {
                case VariantType.None: {
                    iRet = base.GetHashCode();
                    break;
                }

                case VariantType.Blob: {
                    iRet = GetBlob().GetHashCode();
                    break;
                }

                case VariantType.Float: {
                    iRet = GetFloat().GetHashCode();
                    break;
                }

                case VariantType.Double: {
                    iRet = GetDouble().GetHashCode();
                    break;
                }

                case VariantType.Guid: {
                    iRet = GetGuid().GetHashCode();
                    break;
                }

                case VariantType.IUnknown: {
                    iRet = GetIUnknown().GetHashCode();
                    break;
                }

                case VariantType.String: {
                    iRet = GetString().GetHashCode();
                    break;
                }

                case VariantType.UByte: {
                    iRet = GetUByte().GetHashCode();
                    break;
                }

                case VariantType.Short: {
                    iRet = GetShort().GetHashCode();
                    break;
                }

                case VariantType.UShort: {
                    iRet = GetUShort().GetHashCode();
                    break;
                }

                case VariantType.Int32: {
                    iRet = GetInt().GetHashCode();
                    break;
                }

                case VariantType.UInt32: {
                    iRet = GetUInt().GetHashCode();
                    break;
                }

                case VariantType.Int64: {
                    iRet = GetLong().GetHashCode();
                    break;
                }

                case VariantType.UInt64: {
                    iRet = GetULong().GetHashCode();
                    break;
                }

                case VariantType.StringArray: {
                    iRet = GetStringArray().GetHashCode();
                    break;
                }
                default: {
                    iRet = base.GetHashCode();
                    break;
                }
            }

            return iRet;
        }

        public override bool Equals(object obj) {
            const double epsilon = 0.0001;
            bool bRet;
            var p = obj as PropVariant;

            if ((((object) p) == null) || (p.type != type)) {
                bRet = false;
            }
            else {
                switch (type) {
                    case VariantType.None: {
                        bRet = true;
                        break;
                    }

                    case VariantType.Blob: {
                        byte[] b1;
                        byte[] b2;

                        b1 = GetBlob();
                        b2 = p.GetBlob();

                        if (b1.Length == b2.Length) {
                            bRet = true;
                            for (var x = 0; x < b1.Length; x++) {
                                if (b1[x] != b2[x]) {
                                    bRet = false;
                                    break;
                                }
                            }
                        }
                        else {
                            bRet = false;
                        }
                        break;
                    }

                    case VariantType.Float: {
                        bRet = Math.Abs(GetFloat() - p.GetFloat()) < epsilon;
                        break;
                    }

                    case VariantType.Double: {
                        bRet = Math.Abs(GetDouble() - p.GetDouble()) < epsilon;
                        break;
                    }

                    case VariantType.Guid: {
                        bRet = GetGuid() == p.GetGuid();
                        break;
                    }

                    case VariantType.IUnknown: {
                        bRet = GetIUnknown() == p.GetIUnknown();
                        break;
                    }

                    case VariantType.String: {
                        bRet = GetString() == p.GetString();
                        break;
                    }

                    case VariantType.UByte: {
                        bRet = GetUByte() == p.GetUByte();
                        break;
                    }

                    case VariantType.Short: {
                        bRet = GetShort() == p.GetShort();
                        break;
                    }

                    case VariantType.UShort: {
                        bRet = GetUShort() == p.GetUShort();
                        break;
                    }

                    case VariantType.Int32: {
                        bRet = GetInt() == p.GetInt();
                        break;
                    }

                    case VariantType.UInt32: {
                        bRet = GetUInt() == p.GetUInt();
                        break;
                    }

                    case VariantType.Int64: {
                        bRet = GetLong() == p.GetLong();
                        break;
                    }

                    case VariantType.UInt64: {
                        bRet = GetULong() == p.GetULong();
                        break;
                    }

                    case VariantType.StringArray: {
                        string[] sa1;
                        string[] sa2;

                        sa1 = GetStringArray();
                        sa2 = p.GetStringArray();

                        if (sa1.Length == sa2.Length) {
                            bRet = true;
                            for (var x = 0; x < sa1.Length; x++) {
                                if (sa1[x] != sa2[x]) {
                                    bRet = false;
                                    break;
                                }
                            }
                        }
                        else {
                            bRet = false;
                        }
                        break;
                    }
                    default: {
                        bRet = base.Equals(obj);
                        break;
                    }
                }
            }

            return bRet;
        }

        public static bool operator ==(ConstPropVariant pv1, ConstPropVariant pv2) {
            // If both are null, or both are same instance, return true.
            if (ReferenceEquals(pv1, pv2)) {
                return true;
            }

            // If one is null, but not both, return false.
            if (((object) pv1 == null) || ((object) pv2 == null)) {
                return false;
            }

            return pv1.Equals(pv2);
        }

        public static bool operator !=(ConstPropVariant pv1, ConstPropVariant pv2) {
            return !(pv1 == pv2);
        }

        #region IDisposable Members

        public void Dispose() {
            // If we are a ConstPropVariant, we must *not* call PropVariantClear.  That
            // would release the *caller's* copy of the data, which would probably make
            // him cranky.  If we are a PropVariant, the PropVariant.Dispose gets called
            // as well, which *does* do a PropVariantClear.
            type = VariantType.None;
#if DEBUG
            longValue = 0;
#endif
        }

        #endregion
    }


    [StructLayout(LayoutKind.Explicit)]
    public class PropVariant : ConstPropVariant {
        #region Declarations

        [DllImport("ole32.dll", ExactSpelling = true, PreserveSig = false), SuppressUnmanagedCodeSecurity]
        protected static extern void PropVariantCopy(
            [Out, MarshalAs(UnmanagedType.LPStruct)] PropVariant pvarDest,
            [In, MarshalAs(UnmanagedType.LPStruct)] ConstPropVariant pvarSource
            );

        [DllImport("ole32.dll", ExactSpelling = true, PreserveSig = false), SuppressUnmanagedCodeSecurity]
        protected static extern void PropVariantClear(
            [In, MarshalAs(UnmanagedType.LPStruct)] PropVariant pvar
            );

        #endregion

        public PropVariant() : base(VariantType.None) {}

        public PropVariant(string value) : base(VariantType.String) {
            ptr = Marshal.StringToCoTaskMemUni(value);
        }

        public PropVariant(string[] value) : base(VariantType.StringArray) {
            calpwstrVal.cElems = value.Length;
            calpwstrVal.pElems = Marshal.AllocCoTaskMem(IntPtr.Size*value.Length);

            for (var x = 0; x < value.Length; x++) {
                Marshal.WriteIntPtr(calpwstrVal.pElems, x*IntPtr.Size, Marshal.StringToCoTaskMemUni(value[x]));
            }
        }

        public PropVariant(byte value) : base(VariantType.UByte) {
            bVal = value;
        }

        public PropVariant(short value) : base(VariantType.Short) {
            iVal = value;
        }

        [CLSCompliant(false)]
        public PropVariant(ushort value) : base(VariantType.UShort) {
            uiVal = value;
        }

        public PropVariant(int value) : base(VariantType.Int32) {
            intValue = value;
        }

        [CLSCompliant(false)]
        public PropVariant(uint value) : base(VariantType.UInt32) {
            uintVal = value;
        }

        public PropVariant(float value) : base(VariantType.Float) {
            fltVal = value;
        }

        public PropVariant(double value) : base(VariantType.Double) {
            doubleValue = value;
        }

        public PropVariant(long value) : base(VariantType.Int64) {
            longValue = value;
        }

        [CLSCompliant(false)]
        public PropVariant(ulong value) : base(VariantType.UInt64) {
            ulongValue = value;
        }

        public PropVariant(Guid value) : base(VariantType.Guid) {
            ptr = Marshal.AllocCoTaskMem(Marshal.SizeOf(value));
            Marshal.StructureToPtr(value, ptr, false);
        }

        public PropVariant(byte[] value) : base(VariantType.Blob) {
            blobValue.cbSize = value.Length;
            blobValue.pBlobData = Marshal.AllocCoTaskMem(value.Length);
            Marshal.Copy(value, 0, blobValue.pBlobData, value.Length);
        }

        public PropVariant(object value) : base(VariantType.IUnknown) {
            ptr = Marshal.GetIUnknownForObject(value);
        }

        public PropVariant(IntPtr value) {
            Marshal.PtrToStructure(value, this);
        }

        public PropVariant(ConstPropVariant value) {
            if (value != null) {
                PropVariantCopy(this, value);
            }
            else {
                throw new NullReferenceException("null passed to PropVariant constructor");
            }
        }

        ~PropVariant() {
            Clear();
        }

        public void Copy(PropVariant pval) {
            if (pval == null) {
                throw new Exception("Null PropVariant sent to Copy");
            }

            pval.Clear();

            PropVariantCopy(pval, this);
        }

        public void Clear() {
            PropVariantClear(this);
        }

        #region IDisposable Members

        public new void Dispose() {
            Clear();
            GC.SuppressFinalize(this);
        }

        #endregion
    }


    public enum MFAttributeType {
        None = 0x0,
        Blob = 0x1011,
        Double = 0x5,
        Guid = 0x48,
        IUnknown = 13,
        String = 0x1f,
        Uint32 = 0x13,
        Uint64 = 0x15
    }

    [StructLayout(LayoutKind.Sequential)]
    public class FourCC {
        private int fourCC;

        public FourCC(string fcc) {
            if (fcc.Length != 4) {
                throw new ArgumentException(fcc + " is not a valid FourCC");
            }

            var asc = Encoding.ASCII.GetBytes(fcc);

            LoadFromBytes(asc[0], asc[1], asc[2], asc[3]);
        }

        public FourCC(char a, char b, char c, char d) {
            LoadFromBytes((byte) a, (byte) b, (byte) c, (byte) d);
        }

        public FourCC(int fcc) {
            fourCC = fcc;
        }

        public FourCC(byte a, byte b, byte c, byte d) {
            LoadFromBytes(a, b, c, d);
        }

        public FourCC(Guid g) {
            if (!IsA4ccSubtype(g)) {
                throw new Exception("Not a FourCC Guid");
            }

            var asc = g.ToByteArray();

            LoadFromBytes(asc[0], asc[1], asc[2], asc[3]);
        }

        public void LoadFromBytes(byte a, byte b, byte c, byte d) {
            fourCC = a | (b << 8) | (c << 16) | (d << 24);
        }

        public int ToInt32() {
            return fourCC;
        }

        public static explicit operator int(FourCC f) {
            return f.ToInt32();
        }

        public static explicit operator Guid(FourCC f) {
            return f.ToMediaSubtype();
        }

        public Guid ToMediaSubtype() {
            return new Guid(fourCC, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
        }

        public static bool operator ==(FourCC fcc1, FourCC fcc2) {
            // If both are null, or both are same instance, return true.
            if (ReferenceEquals(fcc1, fcc2)) {
                return true;
            }

            // If one is null, but not both, return false.
            if (((object) fcc1 == null) || ((object) fcc2 == null)) {
                return false;
            }

            return fcc1.fourCC == fcc2.fourCC;
        }

        public static bool operator !=(FourCC fcc1, FourCC fcc2) {
            return !(fcc1 == fcc2);
        }

        public override bool Equals(object obj) {
            if (!(obj is FourCC))
                return false;

            return (obj as FourCC).fourCC == fourCC;
        }

        public override int GetHashCode() {
            return fourCC.GetHashCode();
        }

        public override string ToString() {
            var ca = new char[4];

            var c = Convert.ToChar(fourCC & 255);
            if (!Char.IsLetterOrDigit(c)) {
                c = ' ';
            }
            ca[0] = c;

            c = Convert.ToChar((fourCC >> 8) & 255);
            if (!Char.IsLetterOrDigit(c)) {
                c = ' ';
            }
            ca[1] = c;

            c = Convert.ToChar((fourCC >> 16) & 255);
            if (!Char.IsLetterOrDigit(c)) {
                c = ' ';
            }
            ca[2] = c;

            c = Convert.ToChar((fourCC >> 24) & 255);
            if (!Char.IsLetterOrDigit(c)) {
                c = ' ';
            }
            ca[3] = c;

            var s = new string(ca);

            return s;
        }

        public static bool IsA4ccSubtype(Guid g) {
            return (g.ToString().EndsWith("-0000-0010-8000-00aa00389b71"));
        }
    }

    [ComImport, SuppressUnmanagedCodeSecurity,
     Guid("2CD0BD52-BCD5-4B89-B62C-EADC0C031E7D"),
     InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    internal interface IMFMediaEventGenerator {
        void GetEvent(
            [In] MFEventFlag dwFlags,
            [MarshalAs(UnmanagedType.Interface)] out IMFMediaEvent ppEvent
            );

        void BeginGetEvent(
            [In, MarshalAs(UnmanagedType.Interface)] IMFAsyncCallback pCallback,
            [In, MarshalAs(UnmanagedType.IUnknown)] object o
            );

        void EndGetEvent(
            IMFAsyncResult pResult,
            out IMFMediaEvent ppEvent
            );

        void QueueEvent(
            [In] MediaEventType met,
            [In, MarshalAs(UnmanagedType.LPStruct)] Guid guidExtendedType,
            [In] int hrStatus,
            [In, MarshalAs(UnmanagedType.LPStruct)] ConstPropVariant pvValue
            );
    }

    [Flags]
    public enum MFEventFlag {
        None = 0,
        NoWait = 0x00000001
    }

    public enum MediaEventType {
        MEUnknown = 0,
        MEError = (MEUnknown + 1),
        MEExtendedType = (MEError + 1),
        MESessionUnknown = 100,
        MESessionTopologySet = (MESessionUnknown + 1),
        MESessionTopologiesCleared = (MESessionTopologySet + 1),
        MESessionStarted = (MESessionTopologiesCleared + 1),
        MESessionPaused = (MESessionStarted + 1),
        MESessionStopped = (MESessionPaused + 1),
        MESessionClosed = (MESessionStopped + 1),
        MESessionEnded = (MESessionClosed + 1),
        MESessionRateChanged = (MESessionEnded + 1),
        MESessionScrubSampleComplete = (MESessionRateChanged + 1),
        MESessionCapabilitiesChanged = (MESessionScrubSampleComplete + 1),
        MESessionTopologyStatus = (MESessionCapabilitiesChanged + 1),
        MESessionNotifyPresentationTime = (MESessionTopologyStatus + 1),
        MENewPresentation = (MESessionNotifyPresentationTime + 1),
        MELicenseAcquisitionStart = (MENewPresentation + 1),
        MELicenseAcquisitionCompleted = (MELicenseAcquisitionStart + 1),
        MEIndividualizationStart = (MELicenseAcquisitionCompleted + 1),
        MEIndividualizationCompleted = (MEIndividualizationStart + 1),
        MEEnablerProgress = (MEIndividualizationCompleted + 1),
        MEEnablerCompleted = (MEEnablerProgress + 1),
        MEPolicyError = (MEEnablerCompleted + 1),
        MEPolicyReport = (MEPolicyError + 1),
        MEBufferingStarted = (MEPolicyReport + 1),
        MEBufferingStopped = (MEBufferingStarted + 1),
        MEConnectStart = (MEBufferingStopped + 1),
        MEConnectEnd = (MEConnectStart + 1),
        MEReconnectStart = (MEConnectEnd + 1),
        MEReconnectEnd = (MEReconnectStart + 1),
        MERendererEvent = (MEReconnectEnd + 1),
        MESessionStreamSinkFormatChanged = (MERendererEvent + 1),
        MESourceUnknown = 200,
        MESourceStarted = (MESourceUnknown + 1),
        MEStreamStarted = (MESourceStarted + 1),
        MESourceSeeked = (MEStreamStarted + 1),
        MEStreamSeeked = (MESourceSeeked + 1),
        MENewStream = (MEStreamSeeked + 1),
        MEUpdatedStream = (MENewStream + 1),
        MESourceStopped = (MEUpdatedStream + 1),
        MEStreamStopped = (MESourceStopped + 1),
        MESourcePaused = (MEStreamStopped + 1),
        MEStreamPaused = (MESourcePaused + 1),
        MEEndOfPresentation = (MEStreamPaused + 1),
        MEEndOfStream = (MEEndOfPresentation + 1),
        MEMediaSample = (MEEndOfStream + 1),
        MEStreamTick = (MEMediaSample + 1),
        MEStreamThinMode = (MEStreamTick + 1),
        MEStreamFormatChanged = (MEStreamThinMode + 1),
        MESourceRateChanged = (MEStreamFormatChanged + 1),
        MEEndOfPresentationSegment = (MESourceRateChanged + 1),
        MESourceCharacteristicsChanged = (MEEndOfPresentationSegment + 1),
        MESourceRateChangeRequested = (MESourceCharacteristicsChanged + 1),
        MESourceMetadataChanged = (MESourceRateChangeRequested + 1),
        MESequencerSourceTopologyUpdated = (MESourceMetadataChanged + 1),
        MESinkUnknown = 300,
        MEStreamSinkStarted = (MESinkUnknown + 1),
        MEStreamSinkStopped = (MEStreamSinkStarted + 1),
        MEStreamSinkPaused = (MEStreamSinkStopped + 1),
        MEStreamSinkRateChanged = (MEStreamSinkPaused + 1),
        MEStreamSinkRequestSample = (MEStreamSinkRateChanged + 1),
        MEStreamSinkMarker = (MEStreamSinkRequestSample + 1),
        MEStreamSinkPrerolled = (MEStreamSinkMarker + 1),
        MEStreamSinkScrubSampleComplete = (MEStreamSinkPrerolled + 1),
        MEStreamSinkFormatChanged = (MEStreamSinkScrubSampleComplete + 1),
        MEStreamSinkDeviceChanged = (MEStreamSinkFormatChanged + 1),
        MEQualityNotify = (MEStreamSinkDeviceChanged + 1),
        MESinkInvalidated = (MEQualityNotify + 1),
        MEAudioSessionNameChanged = (MESinkInvalidated + 1),
        MEAudioSessionVolumeChanged = (MEAudioSessionNameChanged + 1),
        MEAudioSessionDeviceRemoved = (MEAudioSessionVolumeChanged + 1),
        MEAudioSessionServerShutdown = (MEAudioSessionDeviceRemoved + 1),
        MEAudioSessionGroupingParamChanged = (MEAudioSessionServerShutdown + 1),
        MEAudioSessionIconChanged = (MEAudioSessionGroupingParamChanged + 1),
        MEAudioSessionFormatChanged = (MEAudioSessionIconChanged + 1),
        MEAudioSessionDisconnected = (MEAudioSessionFormatChanged + 1),
        MEAudioSessionExclusiveModeOverride = (MEAudioSessionDisconnected + 1),
        METrustUnknown = 400,
        MEPolicyChanged = (METrustUnknown + 1),
        MEContentProtectionMessage = (MEPolicyChanged + 1),
        MEPolicySet = (MEContentProtectionMessage + 1),
        MEWMDRMLicenseBackupCompleted = 500,
        MEWMDRMLicenseBackupProgress = 501,
        MEWMDRMLicenseRestoreCompleted = 502,
        MEWMDRMLicenseRestoreProgress = 503,
        MEWMDRMLicenseAcquisitionCompleted = 506,
        MEWMDRMIndividualizationCompleted = 508,
        MEWMDRMIndividualizationProgress = 513,
        MEWMDRMProximityCompleted = 514,
        MEWMDRMLicenseStoreCleaned = 515,
        MEWMDRMRevocationDownloadCompleted = 516,
        MEReservedMax = 10000
    }

    internal class PVMarshaler : ICustomMarshaler {
        // The managed object passed in to MarshalManagedToNative
        protected PropVariant m_prop;

        public IntPtr MarshalManagedToNative(object managedObj) {
            IntPtr p;

            // Cast the object back to a PropVariant
            m_prop = managedObj as PropVariant;

            if (m_prop != null) {
                // Release any memory currently allocated
                m_prop.Clear();

                // Create an appropriately sized buffer, blank it, and send it to
                // the marshaler to make the COM call with.
                var iSize = GetNativeDataSize();
                p = Marshal.AllocCoTaskMem(iSize);

                if (IntPtr.Size == 4) {
                    Marshal.WriteInt64(p, 0);
                    Marshal.WriteInt64(p, 8, 0);
                }
                else {
                    Marshal.WriteInt64(p, 0);
                    Marshal.WriteInt64(p, 8, 0);
                    Marshal.WriteInt64(p, 16, 0);
                }
            }
            else {
                p = IntPtr.Zero;
            }

            return p;
        }

        // Called just after invoking the COM method.  The IntPtr is the same one that just got returned
        // from MarshalManagedToNative.  The return value is unused.
        public object MarshalNativeToManaged(IntPtr pNativeData) {
            Marshal.PtrToStructure(pNativeData, m_prop);
            m_prop = null;

            return m_prop;
        }

        public void CleanUpManagedData(object ManagedObj) {
            m_prop = null;
        }

        public void CleanUpNativeData(IntPtr pNativeData) {
            Marshal.FreeCoTaskMem(pNativeData);
        }

        // The number of bytes to marshal out
        public int GetNativeDataSize() {
            return Marshal.SizeOf(typeof (PropVariant));
        }

        // This method is called by interop to create the custom marshaler.  The (optional)
        // cookie is the value specified in MarshalCookie="asdf", or "" is none is specified.
        public static ICustomMarshaler GetInstance(string cookie) {
            return new PVMarshaler();
        }
    }

    [ComImport, SuppressUnmanagedCodeSecurity,
     InterfaceType(ComInterfaceType.InterfaceIsIUnknown),
     Guid("AD4C1B00-4BF7-422F-9175-756693D9130D")]
    internal interface IMFByteStream {
        void GetCapabilities(
            out MFByteStreamCapabilities pdwCapabilities
            );

        void GetLength(
            out long pqwLength
            );

        void SetLength(
            [In] long qwLength
            );

        void GetCurrentPosition(
            out long pqwPosition
            );

        void SetCurrentPosition(
            [In] long qwPosition
            );

        void IsEndOfStream(
            [MarshalAs(UnmanagedType.Bool)] out bool pfEndOfStream
            );

        void Read(
            IntPtr pb,
            [In] int cb,
            out int pcbRead
            );

        void BeginRead(
            IntPtr pb,
            [In] int cb,
            [In, MarshalAs(UnmanagedType.Interface)] IMFAsyncCallback pCallback,
            [In, MarshalAs(UnmanagedType.IUnknown)] object pUnkState
            );

        void EndRead(
            [In, MarshalAs(UnmanagedType.Interface)] IMFAsyncResult pResult,
            out int pcbRead
            );

        void Write(
            IntPtr pb,
            [In] int cb,
            out int pcbWritten
            );

        void BeginWrite(
            IntPtr pb,
            [In] int cb,
            [In, MarshalAs(UnmanagedType.Interface)] IMFAsyncCallback pCallback,
            [In, MarshalAs(UnmanagedType.IUnknown)] object pUnkState
            );

        void EndWrite(
            [In, MarshalAs(UnmanagedType.Interface)] IMFAsyncResult pResult,
            out int pcbWritten
            );

        void Seek(
            [In] MFByteStreamSeekOrigin SeekOrigin,
            [In] long llSeekOffset,
            [In] MFByteStreamSeekingFlags dwSeekFlags,
            out long pqwCurrentPosition
            );

        void Flush();

        void Close();
    }

    [Flags]
    public enum MFByteStreamCapabilities {
        None = 0x00000000,
        IsReadable = 0x00000001,
        IsWritable = 0x00000002,
        IsSeekable = 0x00000004,
        IsRemote = 0x00000008,
        IsDirectory = 0x00000080,
        HasSlowSeek = 0x00000100,
        IsPartiallyDownloaded = 0x00000200
    }

    public enum MFByteStreamSeekOrigin {
        Begin,
        Current
    }

    [Flags]
    public enum MFByteStreamSeekingFlags {
        None = 0,
        CancelPendingIO = 1
    }
}