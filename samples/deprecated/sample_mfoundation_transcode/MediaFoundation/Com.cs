#region Usings

using System;

#endregion

namespace Intel.MediaSDK.Samples.WPFTranscode.MediaFoundation {
    internal static class Com {
        internal delegate void Method<T>(out T result);

        internal delegate void Method<in T, U>(T t, out U result);

        internal delegate void Method<in T, U, V>(T t, out U result1, out V result2);

        internal static T Get<T>(Method<T> action) {
            T result;
            action(out result);
            return result;
        }

        internal static U Get<T, U>(Method<T, U> action, T parameter) {
            U result;
            action(parameter, out result);
            return result;
        }

        internal static bool NoExceptions(Action action) {
            try {
                action();
            }
            catch (Exception) {
                return false;
            }
            return true;
        }

        internal static Tuple<U, V> Get<T, U, V>(Method<T, U, V> action, T parameter) {
            U result1;
            V result2;
            action(parameter, out result1, out result2);
            return new Tuple<U, V>(result1, result2);
        }
    }
}