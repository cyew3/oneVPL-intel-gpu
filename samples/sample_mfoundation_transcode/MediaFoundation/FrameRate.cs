namespace Intel.MediaSDK.Samples.WPFTranscode.MediaFoundation {
    public class FrameRate {
        public FrameRate(uint numerator, uint denominator) {
            Numerator = numerator;
            Denominator = denominator;
        }

        public uint Numerator { get; set; }
        public uint Denominator { get; set; }

        protected bool Equals(FrameRate other) {
            return Numerator == other.Numerator && Denominator == other.Denominator;
        }

        public override bool Equals(object obj) {
            if (ReferenceEquals(null, obj)) return false;
            if (ReferenceEquals(this, obj)) return true;
            if (obj.GetType() != GetType()) return false;
            return Equals((FrameRate) obj);
        }

        public override int GetHashCode() {
            unchecked {
                return ((int) Numerator*397) ^ (int) Denominator;
            }
        }

        public static bool operator ==(FrameRate left, FrameRate right) {
            return Equals(left, right);
        }

        public static bool operator !=(FrameRate left, FrameRate right) {
            return !Equals(left, right);
        }
    }
}