#include "dump.h"


std::string pVoidToHexString(void* x)
{
    std::ostringstream result;
    std::ostringstream tmp;
    tmp << std::hex <<std::uppercase << ((unsigned int)x);
    for (int i = 0; i < (16 - tmp.str().length()); i++)
        result << "0";
    result << tmp.str();
    return result.str();
}

std::string GetStatusString(mfxStatus sts) {
    
    switch (sts) {
    case (MFX_ERR_NONE): return ToString("MFX_ERR_NONE");
    case(MFX_ERR_UNKNOWN): return ToString("MFX_ERR_UNKNOWN");
    case(MFX_ERR_NULL_PTR): return ToString("MFX_ERR_NULL_PTR");
    case(MFX_ERR_UNSUPPORTED): return ToString("MFX_ERR_UNSUPPORTED");
    case(MFX_ERR_MEMORY_ALLOC): return ToString("MFX_ERR_MEMORY_ALLOC");
    case(MFX_ERR_NOT_ENOUGH_BUFFER): return ToString("MFX_ERR_NOT_ENOUGH_BUFFER");
    case(MFX_ERR_INVALID_HANDLE): return ToString("MFX_ERR_INVALID_HANDLE");
    case(MFX_ERR_LOCK_MEMORY): return ToString("MFX_ERR_LOCK_MEMORY");
    case(MFX_ERR_NOT_INITIALIZED): return ToString("MFX_ERR_NOT_INITIALIZED");
    case(MFX_ERR_NOT_FOUND): return ToString("MFX_ERR_NOT_FOUND");
    case(MFX_ERR_MORE_DATA): return ToString("MFX_ERR_MORE_DATA");
    case(MFX_ERR_MORE_SURFACE): return ToString("MFX_ERR_MORE_SURFACE");
    case(MFX_ERR_ABORTED): return ToString("MFX_ERR_ABORTED");
    case(MFX_ERR_DEVICE_LOST): return ToString("MFX_ERR_DEVICE_LOST");
    case(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM): return ToString("MFX_ERR_INCOMPATIBLE_VIDEO_PARAM");
    case(MFX_ERR_INVALID_VIDEO_PARAM): return ToString("MFX_ERR_INVALID_VIDEO_PARAM");
    case(MFX_ERR_UNDEFINED_BEHAVIOR): return ToString("MFX_ERR_UNDEFINED_BEHAVIOR");
    case(MFX_ERR_DEVICE_FAILED): return ToString("MFX_ERR_DEVICE_FAILED");
    case(MFX_ERR_MORE_BITSTREAM): return ToString("MFX_ERR_MORE_BITSTREAM");
    case(MFX_WRN_IN_EXECUTION): return ToString("MFX_WRN_IN_EXECUTION");
    case(MFX_WRN_DEVICE_BUSY): return ToString("MFX_WRN_DEVICE_BUSY");
    case(MFX_WRN_VIDEO_PARAM_CHANGED): return ToString("MFX_WRN_VIDEO_PARAM_CHANGED");
    case(MFX_WRN_PARTIAL_ACCELERATION): return ToString("MFX_WRN_PARTIAL_ACCELERATION");
    case(MFX_WRN_INCOMPATIBLE_VIDEO_PARAM): return ToString("MFX_WRN_INCOMPATIBLE_VIDEO_PARAM");
    case(MFX_WRN_VALUE_NOT_CHANGED): return ToString("MFX_WRN_VALUE_NOT_CHANGED");
    case(MFX_WRN_FILTER_SKIPPED): return ToString("MFX_WRN_FILTER_SKIPPED");
    default:
        return ToString("UNKNOWN_STATUS");
    }
}



struct BufferIdTable
{
    mfxU32 bufferid;
    const char* str;
};

#define TABLE_ENTRY(_name) \
    { _name, #_name }

struct StringCodeItem{
    mfxU32 code;
    char  name[128];
};

#define CODE_STRING(name_prefix, name_postfix)\
{name_prefix##name_postfix, #name_postfix}

static BufferIdTable g_BufferIdTable[] =
{
    TABLE_ENTRY(MFX_EXTBUFF_AVC_REFLIST_CTRL),
    TABLE_ENTRY(MFX_EXTBUFF_AVC_TEMPORAL_LAYERS),

    TABLE_ENTRY(MFX_EXTBUFF_CODING_OPTION),
    TABLE_ENTRY(MFX_EXTBUFF_CODING_OPTION2),
    TABLE_ENTRY(MFX_EXTBUFF_CODING_OPTION3),
    TABLE_ENTRY(MFX_EXTBUFF_CODING_OPTION_SPSPPS),

//    TABLE_ENTRY(MFX_EXTBUFF_DEC_VIDEO_PROCESSING),

    TABLE_ENTRY(MFX_EXTBUFF_ENCODER_CAPABILITY),
    TABLE_ENTRY(MFX_EXTBUFF_ENCODED_FRAME_INFO),
    TABLE_ENTRY(MFX_EXTBUFF_ENCODER_RESET_OPTION),
    TABLE_ENTRY(MFX_EXTBUFF_ENCODER_ROI),

    TABLE_ENTRY(MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION),

    TABLE_ENTRY(MFX_EXTBUFF_PICTURE_TIMING_SEI),

    TABLE_ENTRY(MFX_EXTBUFF_VPP_AUXDATA),
    TABLE_ENTRY(MFX_EXTBUFF_VPP_COMPOSITE),
    TABLE_ENTRY(MFX_EXTBUFF_VPP_DEINTERLACING),
    TABLE_ENTRY(MFX_EXTBUFF_VPP_DENOISE),
    TABLE_ENTRY(MFX_EXTBUFF_VPP_DETAIL),
    TABLE_ENTRY(MFX_EXTBUFF_VPP_DONOTUSE),
    TABLE_ENTRY(MFX_EXTBUFF_VPP_DOUSE),
    TABLE_ENTRY(MFX_EXTBUFF_VPP_FRAME_RATE_CONVERSION),
    TABLE_ENTRY(MFX_EXTBUFF_VPP_IMAGE_STABILIZATION),
    TABLE_ENTRY(MFX_EXTBUFF_VPP_PICSTRUCT_DETECTION),
    TABLE_ENTRY(MFX_EXTBUFF_VPP_PROCAMP),
    TABLE_ENTRY(MFX_EXTBUFF_VPP_SCENE_ANALYSIS),
    TABLE_ENTRY(MFX_EXTBUFF_VPP_SCENE_CHANGE),
    TABLE_ENTRY(MFX_EXTBUFF_VPP_VIDEO_SIGNAL_INFO),

    TABLE_ENTRY(MFX_EXTBUFF_VIDEO_SIGNAL_INFO),

    TABLE_ENTRY(MFX_EXTBUFF_LOOKAHEAD_CTRL),
    TABLE_ENTRY(MFX_EXTBUFF_LOOKAHEAD_STAT),

    TABLE_ENTRY(MFX_EXTBUFF_FEI_PARAM),
    TABLE_ENTRY(MFX_EXTBUFF_FEI_PREENC_CTRL),
    TABLE_ENTRY(MFX_EXTBUFF_FEI_PREENC_MV_PRED),
    TABLE_ENTRY(MFX_EXTBUFF_FEI_PREENC_QP),
    TABLE_ENTRY(MFX_EXTBUFF_FEI_PREENC_MV),
    TABLE_ENTRY(MFX_EXTBUFF_FEI_PREENC_MB),
    TABLE_ENTRY(MFX_EXTBUFF_FEI_ENC_CTRL),
    TABLE_ENTRY(MFX_EXTBUFF_FEI_ENC_MV_PRED),
    TABLE_ENTRY(MFX_EXTBUFF_FEI_ENC_MB),
    TABLE_ENTRY(MFX_EXTBUFF_FEI_ENC_MV),
    TABLE_ENTRY(MFX_EXTBUFF_FEI_ENC_MB_STAT),
    TABLE_ENTRY(MFX_EXTBUFF_FEI_PAK_CTRL),
    TABLE_ENTRY(MFX_EXTBUFF_FEI_SPS),
    TABLE_ENTRY(MFX_EXTBUFF_FEI_PPS),
    TABLE_ENTRY(MFX_EXTBUFF_FEI_SLICE)

};

StringCodeItem tbl_impl[] = {
    CODE_STRING(MFX_IMPL_,SOFTWARE),
    CODE_STRING(MFX_IMPL_,HARDWARE),
    CODE_STRING(MFX_IMPL_,AUTO_ANY),
    CODE_STRING(MFX_IMPL_,HARDWARE_ANY),
    CODE_STRING(MFX_IMPL_,HARDWARE2),
    CODE_STRING(MFX_IMPL_,HARDWARE3),
    CODE_STRING(MFX_IMPL_,HARDWARE4),
    CODE_STRING(MFX_IMPL_,RUNTIME),
    CODE_STRING(MFX_IMPL_,VIA_ANY),
    CODE_STRING(MFX_IMPL_,VIA_D3D9),
    CODE_STRING(MFX_IMPL_,VIA_D3D11),
    CODE_STRING(MFX_IMPL_,VIA_VAAPI),
    CODE_STRING(MFX_IMPL_,AUDIO),
};


const char* DumpContext::get_bufferid_str(mfxU32 bufferid)
{
    for (size_t i = 0; i < GET_ARRAY_SIZE(g_BufferIdTable); ++i)
    {
        if (g_BufferIdTable[i].bufferid == bufferid)
        {
            return g_BufferIdTable[i].str;
        }
    }
    return NULL;
}

std::string GetmfxIMPL(mfxIMPL impl) {
    
    std::basic_stringstream<char> stream;
    std::string name = "UNKNOWN";
    stream << "MFX_IMPL_";
    
    for (int i = 0; i < (sizeof(tbl_impl) / sizeof(tbl_impl[0])); i++)
        if (tbl_impl[i].code == (impl & (MFX_IMPL_VIA_ANY - 1)))
            name = tbl_impl[i].name;
    stream << name;
    
    int via_flag = impl & ~(MFX_IMPL_VIA_ANY - 1);

    if (0 != via_flag)
    {
        stream << "|";
        stream << "MFX_IMPL_";
        name = "UNKNOWN";
        for (int i = 0; i < (sizeof(tbl_impl) / sizeof(tbl_impl[0])); i++)
            if (tbl_impl[i].code == via_flag)
                name = tbl_impl[i].name;
        stream << name;
    }
    return stream.str();
}

std::string GetFourCC(mfxU32 fourcc) {
    
    std::basic_stringstream<char> stream;
    switch (fourcc) {
    case MFX_FOURCC_NV12:
        stream << "NV12";
        break;
    case MFX_FOURCC_RGB3:
        stream << "RGB3";
        break;
    case MFX_FOURCC_RGB4:
        stream << "RGB4";
        break;
    case MFX_FOURCC_YV12:
        stream << "YV12";
        break;
    case MFX_FOURCC_YUY2:
        stream << "YUY2";
        break;
    case MFX_FOURCC_P8:
        stream << "P8";
        break;
    case MFX_FOURCC_P8_TEXTURE:
        stream << "P8_TEXTURE";
        break;
    case MFX_FOURCC_P010:
        stream << "P010";
        break;
    case MFX_FOURCC_BGR4:
        stream << "BGR4";
        break;
    case MFX_FOURCC_A2RGB10:
        stream << "A2RGB10";
        break;
    case MFX_FOURCC_ARGB16:
        stream << "ARGB16";
        break;
    case MFX_FOURCC_R16:
        stream << "R16";
        break;
    default:
        stream << "UNKNOWN";
    }
    
    
    return stream.str();
}
//mfxcommon
DEFINE_GET_TYPE_DEF(mfxBitstream);
DEFINE_GET_TYPE_DEF(mfxExtBuffer);
DEFINE_GET_TYPE_DEF(mfxIMPL);
DEFINE_GET_TYPE_DEF(mfxInitParam);
DEFINE_GET_TYPE_DEF(mfxPriority);
DEFINE_GET_TYPE_DEF(mfxVersion);
DEFINE_GET_TYPE_DEF(mfxSyncPoint);

//mfxenc
DEFINE_GET_TYPE_DEF(mfxENCInput);
DEFINE_GET_TYPE_DEF(mfxENCOutput);

//mfxplugin
DEFINE_GET_TYPE_DEF(mfxPlugin);

//mfxstructures
DEFINE_GET_TYPE_DEF(mfxDecodeStat);
DEFINE_GET_TYPE_DEF(mfxEncodeCtrl);
DEFINE_GET_TYPE_DEF(mfxEncodeStat);
DEFINE_GET_TYPE_DEF(mfxExtCodingOption);
DEFINE_GET_TYPE_DEF(mfxExtCodingOption2);
DEFINE_GET_TYPE_DEF(mfxExtCodingOption3);
DEFINE_GET_TYPE_DEF(mfxExtEncoderResetOption);
DEFINE_GET_TYPE_DEF(mfxExtVppAuxData);
DEFINE_GET_TYPE_DEF(mfxFrameAllocRequest);
DEFINE_GET_TYPE_DEF(mfxFrameData);
DEFINE_GET_TYPE_DEF(mfxFrameId);
DEFINE_GET_TYPE_DEF(mfxFrameInfo);
DEFINE_GET_TYPE_DEF(mfxFrameSurface1);
DEFINE_GET_TYPE_DEF(mfxHandleType);
DEFINE_GET_TYPE_DEF(mfxInfoMFX);
DEFINE_GET_TYPE_DEF(mfxInfoVPP);
DEFINE_GET_TYPE_DEF(mfxPayload);
DEFINE_GET_TYPE_DEF(mfxSkipMode);
DEFINE_GET_TYPE_DEF(mfxVideoParam);
DEFINE_GET_TYPE_DEF(mfxVPPStat);
DEFINE_GET_TYPE_DEF(mfxExtVPPDoNotUse);

//mfxsession
DEFINE_GET_TYPE_DEF(mfxSession);

//mfxvideo
DEFINE_GET_TYPE_DEF(mfxBufferAllocator);
DEFINE_GET_TYPE_DEF(mfxFrameAllocator);

//mfxfei
DEFINE_GET_TYPE_DEF(mfxExtFeiPreEncCtrl);
DEFINE_GET_TYPE_DEF(mfxExtFeiPreEncMV);
DEFINE_GET_TYPE_DEF(mfxExtFeiPreEncMBStat);
DEFINE_GET_TYPE_DEF(mfxExtFeiEncFrameCtrl);
DEFINE_GET_TYPE_DEF(mfxExtFeiEncMVPredictors);
DEFINE_GET_TYPE_DEF(mfxExtFeiEncQP);
DEFINE_GET_TYPE_DEF(mfxExtFeiEncMBCtrl);
DEFINE_GET_TYPE_DEF(mfxExtFeiEncMV);
DEFINE_GET_TYPE_DEF(mfxExtFeiEncMBStat);
DEFINE_GET_TYPE_DEF(mfxFeiPakMBCtrl);
DEFINE_GET_TYPE_DEF(mfxExtFeiPakMBCtrl);
DEFINE_GET_TYPE_DEF(mfxExtFeiParam);
DEFINE_GET_TYPE_DEF(mfxPAKInput);
DEFINE_GET_TYPE_DEF(mfxPAKOutput);
DEFINE_GET_TYPE_DEF(mfxExtFeiSPS);
DEFINE_GET_TYPE_DEF(mfxExtFeiPPS);
DEFINE_GET_TYPE_DEF(mfxExtFeiSliceHeader);


