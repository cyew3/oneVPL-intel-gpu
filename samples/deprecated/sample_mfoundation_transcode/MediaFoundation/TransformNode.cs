#region Usings

using System;

#endregion

namespace Intel.MediaSDK.Samples.WPFTranscode.MediaFoundation {
    public abstract class TransformNode : Node {
        private readonly Guid CLSID_MF_NodeMajorType = new Guid("7E6C6B2D-0240-4BA1-A25B-DF822C446713");

        protected TransformNode(Transform transform) : base(NodeType.Transform) {
            if (transform.Atributes != null) {
                transform.Atributes.AsyncUnlock = true;
            }
            Transform = transform;

            MediaType = transform.GetInputAvailableType(0, 0) ?? transform.GetOutputAvailableType(0, 0);
            MajorType = MediaType.MajorType;
        }

        private Transform transform;

        public Transform Transform {
            get { return transform; }
            private set {
                transform = value;
                MFNode.SetObject(transform.MFTransform);
            }
        }

        public MediaType MediaType { get; protected set; }

        private MajorType majorType;

        public MajorType MajorType {
            get { return majorType; }
            private set {
                majorType = value;
                MFNode.SetGUID(CLSID_MF_NodeMajorType, majorType.MFMajorType);
            }
        }
    }
}