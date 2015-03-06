#region Usings

using System.Collections.Generic;
using Intel.MediaSDK.Samples.WPFTranscode.MediaFoundation.Interop.Interface;

#endregion

namespace Intel.MediaSDK.Samples.WPFTranscode.MediaFoundation {
    public class MediaTypeHandler {
        internal MediaTypeHandler(IMFMediaTypeHandler mfMediaTypeHandler) {
            MFMediaTypeHandler = mfMediaTypeHandler;
            SetMediaTypes();
        }

        internal IMFMediaTypeHandler MFMediaTypeHandler { get; private set; }

        public MediaType CurrentMediaType {
            get { return MediaType.Create(Com.Get((out IMFMediaType _) => MFMediaTypeHandler.GetCurrentMediaType(out _))); }
            set {
                MFMediaTypeHandler.SetCurrentMediaType(value.MFMediaType);
                isCurrentMediaTypeSet = true;
            }
        }

        public List<MediaType> MediaTypes { get; private set; }

        private bool isCurrentMediaTypeSet;

        private void SetMediaTypes() {
            MediaTypes = new List<MediaType>();
            for (uint mediaTypeIndex = 0; mediaTypeIndex < Com.Get((out uint _) => MFMediaTypeHandler.GetMediaTypeCount(out _)); mediaTypeIndex++) {
                var mediaType = MediaType.Create(Com.Get((uint _, out IMFMediaType __) => MFMediaTypeHandler.GetMediaTypeByIndex(_, out __), mediaTypeIndex));
                MediaTypes.Add(mediaType);
                if (mediaType.IsSupported && !isCurrentMediaTypeSet) {
                    CurrentMediaType = mediaType;
                }
            }
        }
    }
}