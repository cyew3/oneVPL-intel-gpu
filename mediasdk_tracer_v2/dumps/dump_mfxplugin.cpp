#include "dump.h"

std::string DumpContext::dump(const std::string structName, const mfxPlugin &plugin)
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

std::string DumpContext::dump(const std::string structName, const mfxCoreParam &CoreParam)
{
    std::string str;
    str += structName + ".reserved[]=" + DUMP_RESERVED_ARRAY(CoreParam.reserved) + "\n";
    str += structName + ".Impl=" + GetmfxIMPL(CoreParam.Impl) + "\n";
    str += dump(structName + ".Version=", CoreParam.Version) + "\n";
    str += structName + ".NumWorkingThread=" + ToString(CoreParam.NumWorkingThread) + "\n";
    return str;
}

std::string DumpContext::dump(const std::string structName, const mfxPluginParam &PluginParam)
{
    std::string str;
    str += structName + ".reserved[]=" + DUMP_RESERVED_ARRAY(PluginParam.reserved) + "\n";
    str += structName + ".reserved1=" + ToString(PluginParam.reserved1) + "\n";
    str += structName + ".PluginVersion=" + ToString(PluginParam.PluginVersion) + "\n";
    str += dump(structName + ".APIVersion=", PluginParam.APIVersion) + "\n";
    str += structName + ".PluginUID=" + dump_reserved_array(&PluginParam.PluginUID.Data[0], 16) + "\n";
    str += structName + ".Type=" + ToString(PluginParam.Type) + "\n";
    str += structName + ".CodecId=" + GetCodecIdString(PluginParam.CodecId) + "\n";
    str += structName + ".ThreadPolicy=" + ToString(PluginParam.ThreadPolicy) + "\n";
    str += structName + ".MaxThreadNum=" + ToString(PluginParam.MaxThreadNum) + "\n";
    return str;
}
