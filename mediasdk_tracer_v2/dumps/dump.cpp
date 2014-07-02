#include "dump.h"

std::string StringToHexString(std::string data)
{
    std::ostringstream result;
    result << std::setw(2) << std::setfill('0') << std::hex << std::uppercase;
    std::copy(data.begin(), data.end(), std::ostream_iterator<unsigned int>(result, " "));
    return result.str();
}

struct BufferIdTable
{
    mfxU32 bufferid;
    const char* str;
};

#define TABLE_ENTRY(_name) \
    { _name, #_name }

static BufferIdTable g_BufferIdTable[] =
{
    TABLE_ENTRY(MFX_EXTBUFF_AVC_REFLIST_CTRL),
    TABLE_ENTRY(MFX_EXTBUFF_AVC_TEMPORAL_LAYERS),

    TABLE_ENTRY(MFX_EXTBUFF_CODING_OPTION),
    TABLE_ENTRY(MFX_EXTBUFF_CODING_OPTION2),
    TABLE_ENTRY(MFX_EXTBUFF_CODING_OPTION3),
    TABLE_ENTRY(MFX_EXTBUFF_CODING_OPTION_SPSPPS),

    TABLE_ENTRY(MFX_EXTBUFF_DEC_VIDEO_PROCESSING),

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
};

const char* get_bufferid_str(mfxU32 bufferid)
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

template<typename T>
std::string dump_mfxExtParams(const std::string structName, const T& _struct)
{
    std::string str;
    std::string name;

    str += structName + ".NumExtParam=" + ToString(_struct.NumExtParam) + "\n";
    str += structName + ".ExtParam=" + ToString(_struct.ExtParam) + "\n";
    for (mfxU16 i = 0; i < _struct.NumExtParam; ++i)
    {
        name = structName + ".ExtParam[" + ToString(i) + "]";
        str += name + "=" + ToString(_struct.ExtParam[i]) + "\n";
        if (_struct.ExtParam[i]) {
            switch (_struct.ExtParam[i]->BufferId)
            {
              case MFX_EXTBUFF_CODING_OPTION:
                str += dump(name, *((mfxExtCodingOption*)_struct.ExtParam[i])) + "\n";
                break;
              case MFX_EXTBUFF_CODING_OPTION2:
                str += dump(name, *((mfxExtCodingOption2*)_struct.ExtParam[i])) + "\n";
                break;
              case MFX_EXTBUFF_CODING_OPTION3:
                str += dump(name, *((mfxExtCodingOption3*)_struct.ExtParam[i])) + "\n";
                break;
              case MFX_EXTBUFF_ENCODER_RESET_OPTION:
                str += dump(name, *((mfxExtEncoderResetOption*)_struct.ExtParam[i])) + "\n";
                break;
              default:
                str += dump(name, *(_struct.ExtParam[i])) + "\n";
                break;
            };
        }
    }
    return str;
}

#define INSTANTIATE__dump_mfxExtParams(_type) \
    template \
    std::string dump_mfxExtParams(const std::string structName, const _type& _struct);

INSTANTIATE__dump_mfxExtParams(mfxBitstream);
INSTANTIATE__dump_mfxExtParams(mfxEncodeCtrl);
INSTANTIATE__dump_mfxExtParams(mfxENCOutput);
INSTANTIATE__dump_mfxExtParams(mfxENCInput);
INSTANTIATE__dump_mfxExtParams(mfxVideoParam);
