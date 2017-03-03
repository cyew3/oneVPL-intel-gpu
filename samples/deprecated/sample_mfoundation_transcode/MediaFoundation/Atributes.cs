#region Usings

using System;
using Intel.MediaSDK.Samples.WPFTranscode.MediaFoundation.Interop.Interface;

#endregion

namespace Intel.MediaSDK.Samples.WPFTranscode.MediaFoundation {
    public class Atributes {
        private readonly Guid MF_TRANSFORM_ASYNC_UNLOCK = new Guid(0xe5666d6b, 0x3422, 0x4eb6, 0xa4, 0x21, 0xda, 0x7d, 0xb1, 0xf8, 0xe2, 0x7);
        private readonly Guid MF_MT_H264_SUPPORTED_SLICE_MODES = new Guid("C8BE1937-4D64-4549-8343-A8086C0BFDA5");

        internal Atributes(IMFAttributes mfAttributes) {
            MFAttributes = mfAttributes;
        }

        internal IMFAttributes MFAttributes { get; set; }

        public bool AsyncUnlock {
            set { MFAttributes.SetUINT32(MF_TRANSFORM_ASYNC_UNLOCK, value.ToUint()); }
        }

        public uint H264SupportedSliceModes {
            set { MFAttributes.SetUINT32(MF_MT_H264_SUPPORTED_SLICE_MODES, value); }
        }
    }
}