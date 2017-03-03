#region Usings

using System;

#endregion

namespace Intel.MediaSDK.Samples.WPFTranscode.MediaFoundation {
    public class MajorType {
        public MajorType(Guid mfMajorType) {
            MFMajorType = mfMajorType;
        }

        internal Guid MFMajorType { get; private set; }

        public static MajorType Audio {
            get { return new MajorType(new Guid(0x73647561, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71)); }
        }

        public static MajorType Video {
            get { return new MajorType(new Guid(0x73646976, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71)); }
        }

        protected bool Equals(MajorType other) {
            return MFMajorType.Equals(other.MFMajorType);
        }

        public override bool Equals(object obj) {
            if (ReferenceEquals(null, obj)) return false;
            if (ReferenceEquals(this, obj)) return true;
            if (obj.GetType() != GetType()) return false;
            return Equals((MajorType) obj);
        }

        public override int GetHashCode() {
            return MFMajorType.GetHashCode();
        }

        public static bool operator ==(MajorType left, MajorType right) {
            return Equals(left, right);
        }

        public static bool operator !=(MajorType left, MajorType right) {
            return !Equals(left, right);
        }
    }
}