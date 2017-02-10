#pragma once
#include "sample_defs.h"

class CParametersDumper
{
protected:
    static void SerializeFrameInfoStruct(msdk_ostream& sstr,msdk_string prefix,mfxFrameInfo& info);
    static void SerializeMfxInfoMFXStruct(msdk_ostream& sstr,msdk_string prefix,mfxInfoMFX& info);
    static void SerializeExtensionBuffer(msdk_ostream& sstr,msdk_string prefix,mfxExtBuffer* pExtBuffer);
    static void SerializeVPPCompInputStream(msdk_ostream& sstr,msdk_string prefix,mfxVPPCompInputStream& info);
public:
    static void SerializeVideoParamStruct(msdk_ostream& sstr,msdk_string sectionName,mfxVideoParam& info,bool shouldUseVPPSection=false);
    static mfxStatus DumpLibraryConfiguration(msdk_string fileName, mfxVideoParam* pDecoderParams,mfxVideoParam* pVPPParams,mfxVideoParam* pEncoderParams);
};

