#region Usings

using System;
using System.Collections.Generic;
using Intel.MediaSDK.Samples.WPFTranscode.MediaFoundation.Interop.Interface;

#endregion

namespace Intel.MediaSDK.Samples.WPFTranscode.MediaFoundation {
    public class PresentationDescriptor {
        private readonly Guid MF_PD_ASF_DATA_LENGTH = new Guid(0xe7d5b3e8, 0x1f29, 0x45d3, 0x88, 0x22, 0x3e, 0x78, 0xfa, 0xe2, 0x72, 0xed);

        internal PresentationDescriptor(IMFPresentationDescriptor presentationDescriptor) {
            MFPresentationDescriptor = presentationDescriptor;
        }

        internal IMFPresentationDescriptor MFPresentationDescriptor { get; private set; }

        public IEnumerable<StreamDescriptor> StreamDescriptors {
            get {
                var descriptorsCount = Com.Get((out uint _) => MFPresentationDescriptor.GetStreamDescriptorCount(out _));
                var streamDescriptors = new List<StreamDescriptor>();
                for (uint index = 0; index < descriptorsCount; index++) {
                    var mfStreamDescriptor = Com.Get((uint _, out bool __, out IMFStreamDescriptor ___) => MFPresentationDescriptor.GetStreamDescriptorByIndex(_, out __, out ___), index).Item2;
                    streamDescriptors.Add(new StreamDescriptor(mfStreamDescriptor));
                }
                return streamDescriptors;
            }
        }

        public ulong AsfDataLength {
            get {
                return Com.Get((Guid _, out ulong __) =>
                               MFPresentationDescriptor.GetUINT64(_, out __), MF_PD_ASF_DATA_LENGTH);
            }
        }
    }
}