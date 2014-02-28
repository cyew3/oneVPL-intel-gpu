#region Usings

using System;
using Intel.MediaSDK.Samples.WPFTranscode.MediaFoundation.Interop.Interface;

#endregion

namespace Intel.MediaSDK.Samples.WPFTranscode.MediaFoundation {
    public abstract class Transform {
        internal IMFTransform MFTransform { get; set; }

        private Atributes atributes;

        public Atributes Atributes {
            get {
                if (atributes != null) return atributes;
                IMFAttributes mfAtributes = null;
                if (Com.NoExceptions(() => MFTransform.GetAttributes(out mfAtributes))) {
                    atributes = new Atributes(mfAtributes);
                }
                return atributes;
            }
        }

        public MediaType GetInputAvailableType(int inputStreamId, int typeIndex) {
            IMFMediaType mfMediaType;
            try {
                MFTransform.GetInputAvailableType(inputStreamId, typeIndex, out mfMediaType);
            }
            catch (Exception) {
                return null;
            }
            return MediaType.Create(mfMediaType);
        }

        public MediaType GetOutputAvailableType(int inputStreamId, int typeIndex) {
            IMFMediaType mfMediaType;
            try {
                MFTransform.GetOutputAvailableType(inputStreamId, typeIndex, out mfMediaType);
            }
            catch (Exception) {
                return null;
            }
            return MediaType.Create(mfMediaType);
        }
    }
}