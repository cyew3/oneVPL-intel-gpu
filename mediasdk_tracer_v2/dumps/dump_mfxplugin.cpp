#include "dump.h"

std::string dump(const std::string structName, const mfxPlugin &plugin)
{
    std::string str;
    str += structName + ".pthis=" + ToString(plugin.pthis) + "\n";
    str += structName + ".PluginInit=" + ToString(plugin.PluginInit) + "\n";
    str += structName + ".PluginClose=" + ToString(plugin.PluginClose) + "\n";
    str += structName + ".GetPluginParam=" + ToString(plugin.GetPluginParam) + "\n";
    str += structName + ".Submit=" + ToString(plugin.Submit) + "\n";
    str += structName + ".Execute=" + ToString(plugin.Execute) + "\n";
    str += structName + ".FreeResources=" + ToString(plugin.FreeResources) + "\n";
    str += structName + ".reserved[]=" + DUMP_RESERVED_ARRAY(plugin.reserved) + "\n";
    return str;
}
