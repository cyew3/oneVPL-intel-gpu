#region Usings

using System.Collections.Generic;
using System.Collections.ObjectModel;

#endregion

namespace Intel.MediaSDK.Samples.WPFTranscode.Client.Model {
    public abstract class Plugin {
        private readonly List<Plugin> linkedPlugins = new List<Plugin>();

        public ReadOnlyCollection<Plugin> NextPlugins {
            get { return linkedPlugins.AsReadOnly(); }
        }

        internal void AddNext(Plugin plugin) {
            linkedPlugins.Add(plugin);
        }
    }
}