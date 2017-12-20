#include "hevc2_trace.h"
#include "hevc2_struct.h"
#include <stdio.h>

namespace BS_HEVC2
{

const char NaluTraceMap[64][16] =
{
    "TRAIL_N", "TRAIL_R", "TSA_N", "TSA_R",
    "STSA_N", "STSA_R", "RADL_N", "RADL_R",
    "RASL_N", "RASL_R", "RSV_VCL_N10", "RSV_VCL_R11",
    "RSV_VCL_N12", "RSV_VCL_R13", "RSV_VCL_N14", "RSV_VCL_R15",
    "BLA_W_LP", "BLA_W_RADL", "BLA_N_LP", "IDR_W_RADL",
    "IDR_N_LP", "CRA_NUT", "RSV_IRAP_VCL22", "RSV_IRAP_VCL23",
    "RSV_VCL24", "RSV_VCL25", "RSV_VCL26", "RSV_VCL27",
    "RSV_VCL28", "RSV_VCL29", "RSV_VCL30", "RSV_VCL31",
    "VPS_NUT", "SPS_NUT", "PPS_NUT", "AUD_NUT",
    "EOS_NUT", "EOB_NUT", "FD_NUT", "PREFIX_SEI_NUT",
    "SUFFIX_SEI_NUT", "RSV_NVCL41", "RSV_NVCL42", "RSV_NVCL43",
    "RSV_NVCL44", "RSV_NVCL45", "RSV_NVCL46", "RSV_NVCL47",
    "UNSPEC48", "UNSPEC49", "UNSPEC50", "UNSPEC51",
    "UNSPEC52", "UNSPEC53", "UNSPEC54", "UNSPEC55",
    "UNSPEC56", "UNSPEC57", "UNSPEC58", "UNSPEC59",
    "UNSPEC60", "UNSPEC61", "UNSPEC62", "UNSPEC63"
};

const char PicTypeTraceMap[3][4] =
{
    "I", "PI", "BPI"
};

const char ProfileTraceMap::map[11][12] =
{
    "UNKNOWN", "MAIN", "MAIN_10", "MAIN_SP",
    "REXT", "REXT_HT", "MAIN_SV", "MAIN_SC",
    "MAIN_3D", "SCC", "REXT_SC"
};

const char* ProfileTraceMap::operator[] (unsigned int i)
{
    return map[(i < (sizeof(map) / sizeof(map[0]))) ? i : 0];
}

const char ChromaFormatTraceMap[4][12] =
{
    "CHROMA_400", "CHROMA_420", "CHROMA_422", "CHROMA_444"
};

const char ARIdcTraceMap::map[19][16] =
{
    "Unspecified",
    "1:1", "12:11", "10:11", "16:11",
    "40:33", "24:11", "20:11", "32:11",
    "80:33", "18:11", "15:11", "64:33",
    "160:99", "4:3", "3:2", "2:1",
    "Reserved",
    "Extended_SAR"
};

const char* ARIdcTraceMap::operator[] (unsigned int i)
{
    if (i < 17)
        return map[i];
    if (i == Extended_SAR)
        return map[18];
    return map[17];
}

const char VideoFormatTraceMap[6][12] =
{
    "Component", "PAL", "NTSC", "SECAM", "MAC", "Unspecified"
};

const char* SEITypeTraceMap::operator[] (unsigned int i)
{
    switch (i)
    {
    case BUFFERING_PERIOD                    : return "BUFFERING_PERIOD";
    case PICTURE_TIMING                      : return "PICTURE_TIMING";
    case PAN_SCAN_RECT                       : return "PAN_SCAN_RECT";
    case FILLER_PAYLOAD                      : return "FILLER_PAYLOAD";
    case USER_DATA_REGISTERED_ITU_T_T35      : return "USER_DATA_REGISTERED_ITU_T_T35";
    case USER_DATA_UNREGISTERED              : return "USER_DATA_UNREGISTERED";
    case RECOVERY_POINT                      : return "RECOVERY_POINT";
    case SCENE_INFO                          : return "SCENE_INFO";
    case FULL_FRAME_SNAPSHOT                 : return "FULL_FRAME_SNAPSHOT";
    case PROGRESSIVE_REFINEMENT_SEGMENT_START: return "PROGRESSIVE_REFINEMENT_SEGMENT_START";
    case PROGRESSIVE_REFINEMENT_SEGMENT_END  : return "PROGRESSIVE_REFINEMENT_SEGMENT_END";
    case FILM_GRAIN_CHARACTERISTICS          : return "FILM_GRAIN_CHARACTERISTICS";
    case POST_FILTER_HINT                    : return "POST_FILTER_HINT";
    case TONE_MAPPING_INFO                   : return "TONE_MAPPING_INFO";
    case FRAME_PACKING                       : return "FRAME_PACKING";
    case DISPLAY_ORIENTATION                 : return "DISPLAY_ORIENTATION";
    case SOP_DESCRIPTION                     : return "SOP_DESCRIPTION";
    case ACTIVE_PARAMETER_SETS               : return "ACTIVE_PARAMETER_SETS";
    case DECODING_UNIT_INFO                  : return "DECODING_UNIT_INFO";
    case TEMPORAL_LEVEL0_INDEX               : return "TEMPORAL_LEVEL0_INDEX";
    case DECODED_PICTURE_HASH                : return "DECODED_PICTURE_HASH";
    case SCALABLE_NESTING                    : return "SCALABLE_NESTING";
    case REGION_REFRESH_INFO                 : return "REGION_REFRESH_INFO";
    case NO_DISPLAY                          : return "NO_DISPLAY";
    case TIME_CODE                           : return "TIME_CODE";
    case MASTERING_DISPLAY_COLOUR_VOLUME     : return "MASTERING_DISPLAY_COLOUR_VOLUME";
    case SEGM_RECT_FRAME_PACKING             : return "SEGM_RECT_FRAME_PACKING";
    case TEMP_MOTION_CONSTRAINED_TILE_SETS   : return "TEMP_MOTION_CONSTRAINED_TILE_SETS";
    case CHROMA_RESAMPLING_FILTER_HINT       : return "CHROMA_RESAMPLING_FILTER_HINT";
    case KNEE_FUNCTION_INFO                  : return "KNEE_FUNCTION_INFO";
    case COLOUR_REMAPPING_INFO               : return "COLOUR_REMAPPING_INFO";
    default                                  : return "Unknown";
    }
}

const char PicStructTraceMap[13][12] =
{
    "FRAME", "TOP", "BOT", "TOP_BOT",
    "BOT_TOP", "TOP_BOT_TOP", "BOT_TOP_BOT", "FRAME_x2",
    "FRAME_x3", "TOP_PREVBOT", "BOT_PREVTOP", "TOP_NEXTBOT",
    "BOT_NEXTTOP"
};

const char ScanTypeTraceMap[4][12] =
{
    "INTERLACED", "PROGRESSIVE", "UNKNOWN", "RESERVED"
};

const char SliceTypeTraceMap[4][2] =
{
    "B", "P", "I", "?"
};

const char* RPLTraceMap::operator[] (const RefPic& r)
{
#ifdef __GNUC__
    sprintf
#else
    sprintf_s
#endif
        (m_buf, "POC: %4d LT: %d Used: %d Lost: %d", r.POC, r.long_term, r.used, r.lost);
    return m_buf;
}

const char PredModeTraceMap[3][6] =
{
    "INTER", "INTRA", "SKIP"
};

const char PartModeTraceMap[8][12] = 
{
    "PART_2Nx2N",
    "PART_2NxN",
    "PART_Nx2N",
    "PART_NxN",
    "PART_2NxnU",
    "PART_2NxnD",
    "PART_nLx2N",
    "PART_nRx2N"
};

const char IntraPredModeTraceMap[35][9] = 
{
    "Planar", "DC",
    "225\xF8", "230.625\xF8", "236.25\xF8", "241.875\xF8", "247.5\xF8", "253.125\xF8", "258.75\xF8", "264.375\xF8",
    "270\xF8", "275.625\xF8", "281.25\xF8", "286.875\xF8", "292.5\xF8", "298.125\xF8", "303.75\xF8", "309.375\xF8",
    "315\xF8", "320.625\xF8", "326.25\xF8", "331.875\xF8", "337.5\xF8", "343.125\xF8", "348.75\xF8", "354.375\xF8",
      "0\xF8",   "5.625\xF8",  "11.25\xF8", " 16.875\xF8",  "22.5\xF8",  "28.125\xF8",  "33.75\xF8",  "39.375\xF8",
     "45\xF8"
};

const char SAOTypeTraceMap[3][5] = 
{
    "N/A", "Band", "Edge"
};

const char EOClassTraceMap[4][5] = 
{
    "0\xF8", "90\xF8", "135\xF8", "45\xF8"
};


};