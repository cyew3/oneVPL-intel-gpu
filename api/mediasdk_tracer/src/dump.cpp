/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2009-2014 Intel Corporation. All Rights Reserved.

File Name: dump.cpp

\* ****************************************************************************** */
#include <d3d9.h>
#include "msdka_serial.h"
#include "ippdc.h"
#include "ippcc.h"
#include "configuration.h"
#include <sstream>

struct StringCodeItem{
    mfxU32 code;
    TCHAR  name[128];
};

#define CODE_STRING(name_prefix, name_postfix)\
{name_prefix##name_postfix, TEXT(#name_postfix)}

StringCodeItem tbl_mem_types[] = {
    CODE_STRING(MFX_MEMTYPE_,DXVA2_DECODER_TARGET),
    CODE_STRING(MFX_MEMTYPE_,DXVA2_PROCESSOR_TARGET),
    CODE_STRING(MFX_MEMTYPE_,SYSTEM_MEMORY),
    //CODE_STRING(MFX_MEMTYPE_PROTECTED),
    CODE_STRING(MFX_MEMTYPE_,FROM_ENCODE),
    CODE_STRING(MFX_MEMTYPE_,FROM_DECODE),
    CODE_STRING(MFX_MEMTYPE_,FROM_VPPIN),
    CODE_STRING(MFX_MEMTYPE_,FROM_VPPOUT),
    CODE_STRING(MFX_MEMTYPE_,INTERNAL_FRAME),
    CODE_STRING(MFX_MEMTYPE_,EXTERNAL_FRAME),
    CODE_STRING(MFX_MEMTYPE_,OPAQUE_FRAME)
};

StringCodeItem tbl_picstruct[] = {
    CODE_STRING(MFX_PICSTRUCT_,UNKNOWN),       
    CODE_STRING(MFX_PICSTRUCT_,PROGRESSIVE),
    CODE_STRING(MFX_PICSTRUCT_,FIELD_TFF),
    CODE_STRING(MFX_PICSTRUCT_,FIELD_BFF),
    CODE_STRING(MFX_PICSTRUCT_,FIELD_REPEATED),
    CODE_STRING(MFX_PICSTRUCT_,FRAME_DOUBLING),
    CODE_STRING(MFX_PICSTRUCT_,FRAME_TRIPLING),
};

StringCodeItem tbl_iopattern[] = {
    CODE_STRING(MFX_IOPATTERN_,IN_VIDEO_MEMORY),
    CODE_STRING(MFX_IOPATTERN_,IN_SYSTEM_MEMORY),
    CODE_STRING(MFX_IOPATTERN_,OUT_VIDEO_MEMORY),
    CODE_STRING(MFX_IOPATTERN_,OUT_SYSTEM_MEMORY)
};

StringCodeItem LevelStrings [] = {
    {DL_INFO,  TEXT("INFO :")},
    {DL_WRN,   TEXT("WARN :")},
    {DL_ERROR, TEXT("ERROR:")}
};

StringCodeItem tbl_vpp_filters [] = {
    CODE_STRING(MFX_EXTBUFF_VPP_,DONOTUSE),
    CODE_STRING(MFX_EXTBUFF_VPP_,AUXDATA),
    CODE_STRING(MFX_EXTBUFF_VPP_,DENOISE),
    CODE_STRING(MFX_EXTBUFF_VPP_,SCENE_ANALYSIS),
    CODE_STRING(MFX_EXTBUFF_VPP_,PROCAMP),
    CODE_STRING(MFX_EXTBUFF_VPP_,DETAIL),
    CODE_STRING(MFX_EXTBUFF_VPP_,IMAGE_STABILIZATION),
    CODE_STRING(MFX_EXTBUFF_VPP_,FRAME_RATE_CONVERSION),
    CODE_STRING(MFX_EXTBUFF_VPP_,PICSTRUCT_DETECTION),
    CODE_STRING(MFX_EXTBUFF_VPP_,COMPOSITE),
    CODE_STRING(MFX_EXTBUFF_VPP_,VIDEO_SIGNAL_INFO),
};

StringCodeItem tbl_frame_type[] = {
    CODE_STRING(MFX_FRAMETYPE_, I),
    CODE_STRING(MFX_FRAMETYPE_, P),  
    CODE_STRING(MFX_FRAMETYPE_, B),   
    CODE_STRING(MFX_FRAMETYPE_, S),   
    CODE_STRING(MFX_FRAMETYPE_, REF), 
    CODE_STRING(MFX_FRAMETYPE_, IDR), 
    CODE_STRING(MFX_FRAMETYPE_, xI),  
    CODE_STRING(MFX_FRAMETYPE_, xP),  
    CODE_STRING(MFX_FRAMETYPE_, xB),  
    CODE_STRING(MFX_FRAMETYPE_, xS),  
    CODE_STRING(MFX_FRAMETYPE_, xREF),
    CODE_STRING(MFX_FRAMETYPE_, xIDR)
};

StringCodeItem tbl_rate_ctrl[] = {
    CODE_STRING(MFX_RATECONTROL_, CBR),
    CODE_STRING(MFX_RATECONTROL_, VBR),
    CODE_STRING(MFX_RATECONTROL_, CQP),
    CODE_STRING(MFX_RATECONTROL_, AVBR),
    CODE_STRING(MFX_RATECONTROL_, LA),
    CODE_STRING(MFX_RATECONTROL_, ICQ),
    CODE_STRING(MFX_RATECONTROL_, VCM),
    CODE_STRING(MFX_RATECONTROL_, LA_ICQ),
    CODE_STRING(MFX_RATECONTROL_, LA_EXT),
};

StringCodeItem tbl_frc_algm[] = {
    CODE_STRING(MFX_FRCALGM_,PRESERVE_TIMESTAMP),
    CODE_STRING(MFX_FRCALGM_,DISTRIBUTED_TIMESTAMP),
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

StringCodeItem tbl_timestamp_calc[] = {
    CODE_STRING(MFX_TIMESTAMPCALC_,UNKNOWN),
    CODE_STRING(MFX_TIMESTAMPCALC_,TELECINE),
};


#define GET_STRING_FROM_CODE_TABLE(value, table)\
    const_cast<TCHAR*>(string_from_code_table((mfxU32)value, table, sizeof(table)/sizeof(table[0])).c_str())

#define GET_STRING_FROM_CODE_TABLE_EQUAL(value, table)\
    const_cast<TCHAR*>(string_from_code_table((mfxU32)value, table, sizeof(table)/sizeof(table[0]), true).c_str())


#define _tcscatformat_s(buf,...)\
    _stprintf_s(buf + _tcslen(buf), sizeof(buf)/ sizeof(buf[0]) - _tcslen(buf), __VA_ARGS__);


//////////////////////////////////////////////////////////////////////////
//Globals
DispatcherLogRecorder DispatcherLogRecorder::m_Instance;

//////////////////////////////////////////////////////////////////////////
StackIncreaser::StackIncreaser()
{
    GlobalConfiguration::ThreadData &data = gc.GetThreadData();
    data.nStackSize++;
}
int StackIncreaser::get()
{
    return gc.GetThreadData().nStackSize;
}
StackIncreaser::~StackIncreaser()
{
    GlobalConfiguration::ThreadData &data = gc.GetThreadData();
    data.nStackSize--;
}

//////////////////////////////////////////////////////////////////////////


std::basic_string<TCHAR> string_from_code_table(mfxU32 value, StringCodeItem* pTable, int nItems, bool bEqual = false)
{
    TCHAR buf[1024], *pBuf = buf;

    bool bFirst = true;

    for (int i = 0; i < nItems; i++)
    {
        if (bEqual && (value != pTable[i].code))
        {
            continue;
        }
        else if (!bEqual && !(value & pTable[i].code))
        {
            continue;
        }

        if (bFirst)
        {
            pBuf += _stprintf_s(pBuf, sizeof(buf)/ sizeof(buf[0]) - (pBuf - buf), TEXT("%s"), pTable[i].name);
            bFirst = false;
        }
        else
        {
            pBuf += _stprintf_s(pBuf, sizeof(buf)/ sizeof(buf[0]) - (pBuf - buf), TEXT("|%s"),pTable[i].name);
        }
    }
    if (bFirst)
    {
        pBuf += _stprintf_s(buf, sizeof(buf)/ sizeof(buf[0]), TEXT("%s(%d)"), TEXT("UNKNOWN"), value);
    }
    
    return buf;
}

void dump_mfxFrameInfo(FILE *fd, int level, TCHAR *prefix, TCHAR *prefix2, TCHAR *prefix3, mfxFrameInfo *fi) {
    if (!fi) {
        dump_format_wprefix(fd,level, 4,prefix,prefix2,prefix3,TEXT("%s=NULL"), TEXT(""));
        return;
    }

    dump_format_wprefix(fd,level, 4,prefix,prefix2,prefix3,TEXT(".BitDepthLuma="),TEXT("(%d)"), fi->BitDepthLuma);
    dump_format_wprefix(fd,level, 4,prefix,prefix2,prefix3,TEXT(".BitDepthChroma="),TEXT("(%d)"), fi->BitDepthChroma);
    dump_format_wprefix(fd,level, 4,prefix,prefix2,prefix3,TEXT(".Shift="),TEXT("(%d)"), fi->Shift);

    switch (fi->FourCC) {
    case MFX_FOURCC_NV12:
        dump_format_wprefix(fd,level, 4,prefix,prefix2,prefix3,TEXT(".FourCC=NV12"), TEXT(""));
        break;
    case MFX_FOURCC_RGB3:
        dump_format_wprefix(fd,level, 4,prefix,prefix2,prefix3,TEXT(".FourCC=RGB3"),TEXT(""));
        break;
    case MFX_FOURCC_RGB4:
        dump_format_wprefix(fd,level, 4,prefix,prefix2,prefix3,TEXT(".FourCC=RGB4"), TEXT(""));
        break;
    case MFX_FOURCC_YV12:
        dump_format_wprefix(fd,level, 4,prefix,prefix2,prefix3,TEXT(".FourCC=YV12"), TEXT(""));
        break;
    case MFX_FOURCC_YUY2:
        dump_format_wprefix(fd,level, 4,prefix,prefix2,prefix3,TEXT(".FourCC=YUY2"), TEXT(""));
        break;
    case MFX_FOURCC_P8:
        dump_format_wprefix(fd,level, 4,prefix,prefix2,prefix3,TEXT(".FourCC=P8"), TEXT(""));
        break;
    case MFX_FOURCC_P8_TEXTURE:
        dump_format_wprefix(fd,level, 4,prefix,prefix2,prefix3,TEXT(".FourCC=P8Texture"), TEXT(""));
        break;
    case MFX_FOURCC_P010:
        dump_format_wprefix(fd,level, 4,prefix,prefix2,prefix3,TEXT(".FourCC=P010"), TEXT(""));
        break;
    case MFX_FOURCC_BGR4:
        dump_format_wprefix(fd,level, 4,prefix,prefix2,prefix3,TEXT(".FourCC=BGR4"), TEXT(""));
        break;
    case MFX_FOURCC_A2RGB10:
        dump_format_wprefix(fd,level, 4,prefix,prefix2,prefix3,TEXT(".FourCC=ARGB10"), TEXT(""));
        break;
    case MFX_FOURCC_ARGB16:
        dump_format_wprefix(fd,level, 4,prefix,prefix2,prefix3,TEXT(".FourCC=ARGB16"), TEXT(""));
        break;
    case MFX_FOURCC_R16:
        dump_format_wprefix(fd,level, 4,prefix,prefix2,prefix3,TEXT(".FourCC=R16"), TEXT(""));
        break;
    default:
        dump_format_wprefix(fd,level, 4,prefix,prefix2,prefix3,TEXT(".FourCC=0x"),TEXT("%x"), fi->FourCC);
    }

    dump_format_wprefix(fd,level, 4,prefix,prefix2,prefix3,TEXT(".Resolution="),TEXT("(%d,%d)"),  fi->Width, fi->Height);
    dump_format_wprefix(fd,level, 4,prefix,prefix2,prefix3,TEXT(".Crop="),TEXT("(%d,%d,%d,%d)"),  fi->CropX,fi->CropY,fi->CropW,fi->CropH);
    dump_format_wprefix(fd,level, 4,prefix,prefix2,prefix3,TEXT(".FrameRate="),TEXT("%d/%d"),  fi->FrameRateExtN, fi->FrameRateExtD);
    dump_format_wprefix(fd,level, 4,prefix,prefix2,prefix3,TEXT(".AspectRatio="),TEXT("%d:%d"),  fi->AspectRatioW, fi->AspectRatioH);
    dump_format_wprefix(fd,level, 4,prefix,prefix2,prefix3,TEXT(".PicStruct="),TEXT("%s"),  GET_STRING_FROM_CODE_TABLE(fi->PicStruct, tbl_picstruct));

    switch (fi->ChromaFormat) {
    case MFX_CHROMAFORMAT_MONOCHROME:
        dump_format_wprefix(fd,level, 4,prefix,prefix2,prefix3,TEXT(".ChromaFormat=MONO"),TEXT(""));
        break;
    case MFX_CHROMAFORMAT_YUV420:
        dump_format_wprefix(fd,level, 4,prefix,prefix2,prefix3,TEXT(".ChromaFormat=4:2:0"),TEXT(""));
        break;
    case MFX_CHROMAFORMAT_YUV422:
        dump_format_wprefix(fd,level, 4,prefix,prefix2,prefix3,TEXT(".ChromaFormat=4:2:2"),TEXT(""));
        break;
    case MFX_CHROMAFORMAT_YUV444:
        dump_format_wprefix(fd,level, 4,prefix,prefix2,prefix3,TEXT(".ChromaFormat=4:4:4"),TEXT(""));
        break;
    default:
        dump_format_wprefix(fd,level, 4,prefix,prefix2,prefix3,TEXT(".ChromaFormat="),TEXT("%d"), fi->ChromaFormat);
    }
}

void dump_mfxInfoMFX(FILE *fd, int level, TCHAR *prefix, TCHAR *prefix2, Component c, mfxInfoMFX *im) {
    if (!im) {
        dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT("=NULL"),TEXT(""));
        return;
    }
    dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".BRCParamMultiplier="),TEXT("%d"), im->BRCParamMultiplier);
    dump_mfxFrameInfo(fd,level, prefix, prefix2, TEXT(".FrameInfo"), &im->FrameInfo);
    switch (im->CodecId) {
    case MFX_CODEC_AVC:
        dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".CodecId=AVC"),TEXT(""));
        break;
    case MFX_CODEC_HEVC:
        dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".CodecId=HEVC"),TEXT(""));
        break;
    case MFX_CODEC_MPEG2:
        dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".CodecId=MPEG2"),TEXT(""));
        break;
    case MFX_CODEC_VC1:
        dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".CodecId=VC1"),TEXT(""));
        break;
    default:
        dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".CodecId="),TEXT("%x"), im->CodecId);
    }
    dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".CodecProfile="),TEXT("%d"),  im->CodecProfile);
    dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".CodecLevel="),TEXT("%d"),  im->CodecLevel);
    dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".NumThread="),TEXT("%d"),  im->NumThread);

    if (c==ENCODE) {
        dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".TargetUsage="),TEXT("%d"),  im->TargetUsage);
        dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".GopPicSize="),TEXT("%d"),  im->GopPicSize);
        dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".GopRefDist="),TEXT("%d"),  im->GopRefDist);
        dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".GopOptFlag="),TEXT("%d"),  im->GopOptFlag);
        dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".IdrInterval="),TEXT("%d"),  im->IdrInterval);
        dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".RateControlMethod="),TEXT("%s"),  GET_STRING_FROM_CODE_TABLE_EQUAL(im->RateControlMethod, tbl_rate_ctrl));
        
        switch (im->RateControlMethod)
        {
            case MFX_RATECONTROL_VBR:
            case MFX_RATECONTROL_CBR:
            {
                dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".InitialDelayInKB="),TEXT("%d"), im->InitialDelayInKB);
                dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".TargetKbps="),TEXT("%d"), im->TargetKbps);
                dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".MaxKbps="),TEXT("%d"), im->MaxKbps);
                break;
            }
            case MFX_RATECONTROL_CQP:
            {
                dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".QPI="),TEXT("%d"), im->QPI);
                dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".QPP="),TEXT("%d"), im->QPP);
                dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".QPB="),TEXT("%d"), im->QPB);
                break;
            }
            case MFX_RATECONTROL_AVBR:
            {
                dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".Accuracy="),TEXT("%d"), im->Accuracy);
                dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".Convergence="),TEXT("%d"), im->Convergence);
                break;
            }
            case MFX_RATECONTROL_ICQ:
            case MFX_RATECONTROL_LA_ICQ:
            {
                dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".ICQQuality="),TEXT("%d"), im->ICQQuality);
                break;
            }
            case MFX_RATECONTROL_LA:
            {
                dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".TargetKbps="),TEXT("%d"), im->TargetKbps);
                break;
            }
        }
        dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".BufferSizeInKB="),TEXT("%d"),  im->BufferSizeInKB);
        dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".NumSlice="),TEXT("%d"),  im->NumSlice);
        dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".NumRefFrame="),TEXT("%d"),  im->NumRefFrame);
        dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".EncodedOrder="),TEXT("%d"),  im->EncodedOrder);
    } else 
    {
        switch (im->CodecId) 
        {
            case MFX_CODEC_AVC:
                dump_format_wprefix(fd,level, 3, prefix, prefix2, TEXT(".SliceGroupsPresent="), TEXT("%d"), im->SliceGroupsPresent);
            case MFX_CODEC_MPEG2:
            case MFX_CODEC_VC1:
            {
                dump_format_wprefix(fd,level, 3, prefix, prefix2, TEXT(".DecodedOrder="), TEXT("%d"), im->DecodedOrder);
                dump_format_wprefix(fd,level, 3, prefix, prefix2, TEXT(".ExtendedPicStruct="), TEXT("%d"), im->ExtendedPicStruct);
                dump_format_wprefix(fd,level, 3, prefix, prefix2, TEXT(".TimeStampCalc="), TEXT("%s"), GET_STRING_FROM_CODE_TABLE_EQUAL(im->TimeStampCalc, tbl_timestamp_calc));
                break;
            }
            default:
                break;
        }
    }
}

void dump_mfxInfoVPP(FILE *fd, int level, TCHAR *prefix, TCHAR *prefix2, mfxInfoVPP *iv) {
    if (iv) {
        dump_mfxFrameInfo(fd,level,  prefix, prefix2, TEXT(".In"), &iv->In);
        dump_mfxFrameInfo(fd,level,  prefix, prefix2, TEXT(".Out"), &iv->Out);
    } else {
        dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT("=NULL"),TEXT(""));
    }
}

void dump_ExtBuffers(FILE *fd, int level, TCHAR *prefix, TCHAR* prefix2, int codec, mfxExtBuffer **bfs, int n) {
    if (!bfs) {
        dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT("=NULL"),TEXT(""));
        return;
    }
    try
    {
        TCHAR buf[1024] = TEXT("=");
        int i;
        for (i=0;i<n && bfs;i++) {
            mfxExtBuffer *eb=bfs[i];
            switch (eb->BufferId) 
            {
                case MFX_EXTBUFF_CODING_OPTION: _tcscat_s(buf, sizeof(buf)/ sizeof(buf[0]), TEXT("mfxExtCodingOption")); break;
                case MFX_EXTBUFF_CODING_OPTION_SPSPPS: _tcscat_s(buf, sizeof(buf)/ sizeof(buf[0]), TEXT("mfxExtCodingOptionSPSPPS")); break;
                case MFX_EXTBUFF_VPP_DONOTUSE: _tcscat_s(buf, sizeof(buf)/ sizeof(buf[0]), TEXT("mfxExtVPPDoNotUse")); break;
                case MFX_EXTBUFF_VPP_DOUSE: _tcscat_s(buf, sizeof(buf)/ sizeof(buf[0]), TEXT("mfxExtVPPDoUse")); break;
                case MFX_EXTBUFF_VPP_DENOISE: _tcscat_s(buf, sizeof(buf)/ sizeof(buf[0]), TEXT("mfxExtVPPDenoise")); break;
                case MFX_EXTBUFF_VPP_PROCAMP: _tcscat_s(buf, sizeof(buf)/ sizeof(buf[0]), TEXT("mfxExtVPPProcAmp,")); break;
                case MFX_EXTBUFF_VPP_IMAGE_STABILIZATION: _tcscat_s(buf, sizeof(buf)/ sizeof(buf[0]), TEXT("mfxExtVPPImageStab,")); break;
                case MFX_EXTBUFF_VPP_DETAIL: _tcscat_s(buf, sizeof(buf)/ sizeof(buf[0]), TEXT("mfxExtVPPDetail")); break;
                case MFX_EXTBUFF_VPP_AUXDATA: _tcscat_s(buf, sizeof(buf)/ sizeof(buf[0]), TEXT("mfxExtVppAuxData")); break;
                case MFX_EXTBUFF_VPP_SCENE_CHANGE: _tcscat_s(buf, sizeof(buf)/ sizeof(buf[0]), TEXT("MFX_EXTBUFF_VPP_SCENE_CHANGE")); break;
                case MFX_EXTBUFF_VPP_FRAME_RATE_CONVERSION: _tcscat_s(buf, sizeof(buf)/ sizeof(buf[0]), TEXT("mfxExtVPPFrameRateConversion")); break;
                case MFX_EXTBUFF_VPP_COMPOSITE: _tcscat_s(buf, sizeof(buf)/ sizeof(buf[0]), TEXT("mfxExtVPPComposite")); break;
                case MFX_EXTBUFF_VPP_VIDEO_SIGNAL_INFO: _tcscat_s(buf, sizeof(buf)/ sizeof(buf[0]), TEXT("mfxExtVPPVideoSignalInfo")); break;
                case MFX_EXTBUFF_AVC_TEMPORAL_LAYERS: _tcscat_s(buf, sizeof(buf)/ sizeof(buf[0]), TEXT("mfxExtAvcTemporalLayers")); break;
                case MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION: _tcscat_s(buf, sizeof(buf)/ sizeof(buf[0]), TEXT("mfxExtOpaqueSurfaceAlloc")); break;
                case MFX_EXTBUFF_AVC_REFLIST_CTRL: _tcscat_s(buf, sizeof(buf)/ sizeof(buf[0]), TEXT("mfxExtAVCRefListCtrl")); break;
                case MFX_EXTBUFF_VIDEO_SIGNAL_INFO:  _tcscat_s(buf, sizeof(buf)/ sizeof(buf[0]), TEXT("mfxExtVideoSignalInfo")); break;
                case MFX_EXTBUFF_PICTURE_TIMING_SEI: _tcscat_s(buf, sizeof(buf)/ sizeof(buf[0]), TEXT("mfxExtPictureTimingSEI")); break;
                case MFX_EXTBUFF_CODING_OPTION2: _tcscat_s(buf, sizeof(buf)/ sizeof(buf[0]), TEXT("mfxExtCodingOption2")); break;
                case MFX_EXTBUFF_ENCODER_CAPABILITY: _tcscat_s(buf, sizeof(buf)/ sizeof(buf[0]), TEXT("mfxExtEncoderCapability")); break;
                case MFX_EXTBUFF_ENCODER_RESET_OPTION: _tcscat_s(buf, sizeof(buf)/ sizeof(buf[0]), TEXT("mfxExtEncoderResetOption")); break;
                case MFX_EXTBUFF_ENCODED_FRAME_INFO: _tcscat_s(buf, sizeof(buf)/ sizeof(buf[0]), TEXT("mfxExtAVCEncodedFrameInfo")); break;
                case MFX_EXTBUFF_ENCODER_ROI: _tcscat_s(buf, sizeof(buf)/ sizeof(buf[0]), TEXT("mfxExtEncoderROI")); break;
                case MFX_EXTBUFF_LOOKAHEAD_CTRL: _tcscat_s(buf, sizeof(buf)/ sizeof(buf[0]), TEXT("mfxExtLAControl")); break;
                case MFX_EXTBUFF_LOOKAHEAD_STAT: _tcscat_s(buf, sizeof(buf)/ sizeof(buf[0]), TEXT("mfxExtLAFrameStatistics")); break;
                default: 
                    _tcscatformat_s(buf,TEXT("0x%x"),eb->BufferId); break;
            }
            if (i + 1 != n)
            {
                _tcscatformat_s(buf,TEXT(",")); 
            }
        }
        _tcscat_s(buf, sizeof(buf)/ sizeof(buf[0]), TEXT(""));
        dump_format_wprefix(fd,level, 2,prefix, prefix2, buf);

        for (i = 0; i < n && bfs; i++) 
        {
            mfxExtBuffer *eb = bfs[i];
            switch (eb->BufferId) 
            {
                case MFX_EXTBUFF_CODING_OPTION: 
                {
                    mfxExtCodingOption *co=(mfxExtCodingOption *)eb;

                    dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".mfxExtCodingOption.RateDistortionOpt="),TEXT("%d"),  co->RateDistortionOpt);
                    dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".mfxExtCodingOption.MECostType="),TEXT("%d"),  co->MECostType);
                    dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".mfxExtCodingOption.MESearchType="),TEXT("%d"),  co->MESearchType);
                    dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".mfxExtCodingOption.MVSearchWindow="),TEXT("(%d,%d)"),  co->MVSearchWindow.x, co->MVSearchWindow.y);
                    dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".mfxExtCodingOption.EndOfSequence="),TEXT("%d"),  co->EndOfSequence);
                    dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".mfxExtCodingOption.FramePicture="),TEXT("%d"),  co->FramePicture);
                    if (codec==MFX_CODEC_AVC) {
                        dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".mfxExtCodingOption.CAVLC="),TEXT("%d"),  co->CAVLC);
                        dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".mfxExtCodingOption.RecoveryPointSEI="),TEXT("%d"),  co->RecoveryPointSEI);
                        dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".mfxExtCodingOption.ViewOutput="),TEXT("%d"),  co->ViewOutput);
                        dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".mfxExtCodingOption.NalHrdConformance="),TEXT("%d"),  co->NalHrdConformance);
                        dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".mfxExtCodingOption.SingleSeiNalUnit="),TEXT("%d"),  co->SingleSeiNalUnit);
                        dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".mfxExtCodingOption.VuiVclHrdParameters="),TEXT("%d"),  co->VuiVclHrdParameters);
                        dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".mfxExtCodingOption.RefPicListReordering="),TEXT("%d"), co->RefPicListReordering);
                        dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".mfxExtCodingOption.ResetRefList="),TEXT("%d"),  co->ResetRefList);
                        dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".mfxExtCodingOption.RefPicMarkRep="),TEXT("%d"),  co->RefPicMarkRep);
                        dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".mfxExtCodingOption.FieldOutput="),TEXT("%d"),  co->FieldOutput);
                        dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".mfxExtCodingOption.IntraPredBlockSize="),TEXT("%d"), co->IntraPredBlockSize);
                        dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".mfxExtCodingOption.InterPredBlockSize="),TEXT("%d"), co->InterPredBlockSize);
                        dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".mfxExtCodingOption.MVPrecision="),TEXT("%d"),  co->MVPrecision);
                        dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".mfxExtCodingOption.MaxDecFrameBuffering="),TEXT("%d"), co->MaxDecFrameBuffering);
                        dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".mfxExtCodingOption.AUDelimiter="),TEXT("%d"),  co->AUDelimiter);
                        dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".mfxExtCodingOption.EndOfStream="),TEXT("%d"),  co->EndOfStream);
                        dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".mfxExtCodingOption.PicTimingSEI="),TEXT("%d"),  co->PicTimingSEI);
                        dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".mfxExtCodingOption.VuiNalHrdParameters="),TEXT("%d"), co->VuiNalHrdParameters);
                    }
                    break;
                }
                case MFX_EXTBUFF_CODING_OPTION_SPSPPS: 
                {
                    mfxExtCodingOptionSPSPPS *co=(mfxExtCodingOptionSPSPPS *)eb;
                    dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".mfxExtCodingOptionSPSPPS.SPSId="),TEXT("%d"), co->SPSId);
                    RECORD_POINTERS(dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".mfxExtCodingOptionSPSPPS.SPSBuffer=0x"),TEXT("%p"), co->SPSBuffer));
                    dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".mfxExtCodingOptionSPSPPS.SPSBufSize="),TEXT("%d"), co->SPSBufSize);
                    dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".mfxExtCodingOptionSPSPPS.PPSId="),TEXT("%d"), co->PPSId);
                    RECORD_POINTERS(dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".mfxExtCodingOptionSPSPPS.PPSBuffer=0x"),TEXT("%p"), co->PPSBuffer));
                    dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".mfxExtCodingOptionSPSPPS.PPSBufSize="),TEXT("%d"), co->PPSBufSize);
                    break;
                }
                case MFX_EXTBUFF_VPP_DOUSE:
                case MFX_EXTBUFF_VPP_DONOTUSE: 
                {
                    mfxExtVPPDoNotUse *hint=(mfxExtVPPDoNotUse *)eb;
                    const TCHAR doUse[] = TEXT(".mfxExtVPPDoUse.AlgList=");
                    const TCHAR doNotUse[] = TEXT(".mfxExtVPPDoNotUse.AlgList=");

                    _stprintf_s(buf, sizeof(buf)/ sizeof(buf[0]), MFX_EXTBUFF_VPP_DONOTUSE == eb->BufferId ? doNotUse : doUse);

                    for (int j = 0; j < (int)hint->NumAlg; j++) 
                    {
                        std::basic_string<TCHAR> pCode = GET_STRING_FROM_CODE_TABLE_EQUAL(hint->AlgList[j], tbl_vpp_filters);
                        if (pCode == TEXT("UNKNOWN"))
                        {
                            _tcscatformat_s(buf, TEXT("0x%x"),hint->AlgList[j]);
                        }
                        else
                        {
                            _tcscat_s(buf, sizeof(buf)/ sizeof(buf[0]), pCode.c_str() );
                        }
                 
                        if ((mfxU32)j+1 != hint->NumAlg)
                            _tcscat_s(buf, sizeof(buf)/ sizeof(buf[0]), TEXT(","));
                    }
                    dump_format_wprefix(fd,level, 2, prefix, prefix2, buf);
                    break;
                }
                    
            case MFX_EXTBUFF_VPP_DENOISE:
            {
                mfxExtVPPDenoise * pDenose =  (mfxExtVPPDenoise *)eb;
                dump_format_wprefix(fd,level, 3,prefix,prefix2, TEXT(".mfxExtVPPDenoise.DenoiseFactor="),TEXT("%d"), pDenose->DenoiseFactor);
                break;
            }
            case MFX_EXTBUFF_VPP_PROCAMP:
            {
                mfxExtVPPProcAmp * pProcAmp =  (mfxExtVPPProcAmp *)eb;
                dump_format_wprefix(fd,level, 3,prefix,prefix2, TEXT(".mfxExtVPPProcAmp.Brightness="),TEXT("%.4lf"), pProcAmp->Brightness);
                dump_format_wprefix(fd,level, 3,prefix,prefix2, TEXT(".mfxExtVPPProcAmp.Contrast="),TEXT("%.4lf"), pProcAmp->Contrast);
                dump_format_wprefix(fd,level, 3,prefix,prefix2, TEXT(".mfxExtVPPProcAmp.Hue="),TEXT("%.4lf"), pProcAmp->Hue);
                dump_format_wprefix(fd,level, 3,prefix,prefix2, TEXT(".mfxExtVPPProcAmp.Saturation="),TEXT("%.4lf"), pProcAmp->Saturation);
                break;
            }
            case MFX_EXTBUFF_VPP_IMAGE_STABILIZATION:
            {
                mfxExtVPPImageStab * pImStab =  (mfxExtVPPImageStab *)eb;
                dump_format_wprefix(fd,level, 3,prefix,prefix2, TEXT(".mfxExtVPPImageStab.Mode="),TEXT("%d"), pImStab->Mode);
                break;
            }
            case MFX_EXTBUFF_VPP_DETAIL:
            {
                mfxExtVPPDetail * pDetail =  (mfxExtVPPDetail *)eb;
                dump_format_wprefix(fd,level, 3,prefix,prefix2, TEXT(".mfxExtVPPDetail.DetailFactor="),TEXT("%u"), pDetail->DetailFactor);
                break;
            }
            case MFX_EXTBUFF_VPP_AUXDATA:
            {
                mfxExtVppAuxData * pAux =  (mfxExtVppAuxData *)eb;
                dump_format_wprefix(fd,level, 3,prefix,prefix2, TEXT(".mfxExtVppAuxData.SpatialComplexity="),TEXT("%d"), pAux->SpatialComplexity);
                dump_format_wprefix(fd,level, 3,prefix,prefix2, TEXT(".mfxExtVppAuxData.TemporalComplexity="),TEXT("%d"), pAux->TemporalComplexity);
                dump_format_wprefix(fd,level, 3,prefix,prefix2, TEXT(".mfxExtVppAuxData.PicStruct="),TEXT("%d"), pAux->PicStruct);
                dump_format_wprefix(fd,level, 3,prefix,prefix2, TEXT(".mfxExtVppAuxData.SceneChangeRate="),TEXT("%d"), pAux->SceneChangeRate);
                dump_format_wprefix(fd,level, 3,prefix,prefix2, TEXT(".mfxExtVppAuxData.RepeatedFrame="),TEXT("%d"), pAux->RepeatedFrame);
                break;
            }
            case MFX_EXTBUFF_VPP_SCENE_CHANGE:
            {
                dump_format_wprefix(fd,level, 2,prefix,prefix2, TEXT("%s=true"),GET_STRING_FROM_CODE_TABLE_EQUAL(eb->BufferId, tbl_vpp_filters));
                break;
            }
            case MFX_EXTBUFF_VPP_FRAME_RATE_CONVERSION :
            {
                mfxExtVPPFrameRateConversion *frc = (mfxExtVPPFrameRateConversion*)eb;
                dump_format_wprefix(fd,level, 3, prefix, prefix2, TEXT("mfxExtVPPFrameRateConversion.Algorithm="), TEXT("%s"), GET_STRING_FROM_CODE_TABLE_EQUAL(frc->Algorithm, tbl_frc_algm));
                break;
            }
            case MFX_EXTBUFF_VPP_COMPOSITE:
            {
                mfxExtVPPComposite *c = (mfxExtVPPComposite*)eb;
                dump_format_wprefix(fd,level, 3, prefix, prefix2, TEXT(".mfxExtVPPComposite.NumInputStream="),TEXT("%d"), c->NumInputStream);
                dump_format_wprefix(fd,level, 3, prefix, prefix2, TEXT(".mfxExtVPPComposite.Y(R)="),TEXT("%d"), c->R);
                dump_format_wprefix(fd,level, 3, prefix, prefix2, TEXT(".mfxExtVPPComposite.U(G)="),TEXT("%d"), c->G);
                dump_format_wprefix(fd,level, 3, prefix, prefix2, TEXT(".mfxExtVPPComposite.V(B)="),TEXT("%d"), c->B);
                for (int j = 0; j < c->NumInputStream; j++)
                {
                    dump_format_wprefix(fd,level, 3, prefix, prefix2, TEXT(".mfxExtVPPComposite.InputStream"),TEXT("[%d].DstX=%d"),              j, c->InputStream[j].DstX);
                    dump_format_wprefix(fd,level, 3, prefix, prefix2, TEXT(".mfxExtVPPComposite.InputStream"),TEXT("[%d].DstY=%d"),              j, c->InputStream[j].DstY);
                    dump_format_wprefix(fd,level, 3, prefix, prefix2, TEXT(".mfxExtVPPComposite.InputStream"),TEXT("[%d].DstW=%d"),              j, c->InputStream[j].DstW);
                    dump_format_wprefix(fd,level, 3, prefix, prefix2, TEXT(".mfxExtVPPComposite.InputStream"),TEXT("[%d].DstH=%d"),              j, c->InputStream[j].DstH);
                    dump_format_wprefix(fd,level, 3, prefix, prefix2, TEXT(".mfxExtVPPComposite.InputStream"),TEXT("[%d].LumaKeyEnable=%d"),     j, c->InputStream[j].LumaKeyEnable);
                    dump_format_wprefix(fd,level, 3, prefix, prefix2, TEXT(".mfxExtVPPComposite.InputStream"),TEXT("[%d].LumaKeyMin=%d"),        j, c->InputStream[j].LumaKeyMin);
                    dump_format_wprefix(fd,level, 3, prefix, prefix2, TEXT(".mfxExtVPPComposite.InputStream"),TEXT("[%d].LumaKeyMax=%d"),        j, c->InputStream[j].LumaKeyMax);
                    dump_format_wprefix(fd,level, 3, prefix, prefix2, TEXT(".mfxExtVPPComposite.InputStream"),TEXT("[%d].GlobalAlphaEnable=%d"), j, c->InputStream[j].GlobalAlphaEnable);
                    dump_format_wprefix(fd,level, 3, prefix, prefix2, TEXT(".mfxExtVPPComposite.InputStream"),TEXT("[%d].GlobalAlpha=%d"),       j, c->InputStream[j].GlobalAlpha);
                    dump_format_wprefix(fd,level, 3, prefix, prefix2, TEXT(".mfxExtVPPComposite.InputStream"),TEXT("[%d].PixelAlphaEnable=%d"),  j, c->InputStream[j].PixelAlphaEnable);
                }
                break;
            }
            case MFX_EXTBUFF_VPP_VIDEO_SIGNAL_INFO:
            {
                mfxExtVPPVideoSignalInfo *vsi = (mfxExtVPPVideoSignalInfo*)eb;
                dump_format_wprefix(fd,level, 3, prefix, prefix2, TEXT(".mfxExtVPPVideoSignalInfo.In.TransferMatrix="),TEXT("%d"), vsi->In.TransferMatrix);
                dump_format_wprefix(fd,level, 3, prefix, prefix2, TEXT(".mfxExtVPPVideoSignalInfo.In.NominalRange="),TEXT("%d"), vsi->In.NominalRange);
                dump_format_wprefix(fd,level, 3, prefix, prefix2, TEXT(".mfxExtVPPVideoSignalInfo.Out.TransferMatrix="),TEXT("%d"), vsi->Out.TransferMatrix);
                dump_format_wprefix(fd,level, 3, prefix, prefix2, TEXT(".mfxExtVPPVideoSignalInfo.Out.NominalRange="),TEXT("%d"), vsi->Out.NominalRange);
                break;
            }
            case MFX_EXTBUFF_AVC_TEMPORAL_LAYERS:
            {
                mfxExtAvcTemporalLayers *pLayer =(mfxExtAvcTemporalLayers *)eb;

                dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".mfxExtAvcTemporalLayers.BaseLayerPID="),TEXT("%d"),  pLayer->BaseLayerPID);
                std::basic_stringstream<TCHAR> stream;
                for (int j = 0; j < sizeof(pLayer->Layer) / sizeof(pLayer->Layer[0]); j++)
                {
                    stream<<pLayer->Layer[j].Scale;
                    if (j + 1 != sizeof(pLayer->Layer) / sizeof(pLayer->Layer[0]))
                    {
                        stream<<TEXT(", ");
                    }
                }
                dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".mfxExtAvcTemporalLayers.Layer="),TEXT("{%s}"),  stream.str().c_str());
                break;
            }
            case  MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION:
            {
                mfxExtOpaqueSurfaceAlloc *pAlloc = (mfxExtOpaqueSurfaceAlloc *)eb;
                dump_format_wprefix(fd,level, 3, prefix, prefix2, TEXT(".mfxExtOpaqueSurfaceAlloc.In.Surfaces="),TEXT("%p"), pAlloc->In.Surfaces);
                dump_format_wprefix(fd,level, 3, prefix, prefix2, TEXT(".mfxExtOpaqueSurfaceAlloc.In.Type="),TEXT("%s"), GET_STRING_FROM_CODE_TABLE(pAlloc->In.Type,tbl_mem_types));
                dump_format_wprefix(fd,level, 3, prefix, prefix2, TEXT(".mfxExtOpaqueSurfaceAlloc.In.NumSurface="),TEXT("%d"), pAlloc->In.NumSurface);

                dump_format_wprefix(fd,level, 3, prefix, prefix2, TEXT(".mfxExtOpaqueSurfaceAlloc.Out.Surfaces="),TEXT("%p"), pAlloc->Out.Surfaces);
                dump_format_wprefix(fd,level, 3, prefix, prefix2, TEXT(".mfxExtOpaqueSurfaceAlloc.Out.Type="),TEXT("%s"), GET_STRING_FROM_CODE_TABLE(pAlloc->Out.Type,tbl_mem_types));
                dump_format_wprefix(fd,level, 3, prefix, prefix2, TEXT(".mfxExtOpaqueSurfaceAlloc.Out.NumSurface="),TEXT("%d"), pAlloc->Out.NumSurface);
                break;
            }
            case MFX_EXTBUFF_AVC_REFLIST_CTRL :
            {
                mfxExtAVCRefListCtrl *pRefList = (mfxExtAVCRefListCtrl *)eb;

                dump_format_wprefix(fd,level, 3, prefix, prefix2, TEXT(".mfxExtAVCRefListCtrl.NumRefIdxL0Active="),TEXT("%d"), pRefList->NumRefIdxL0Active);
                dump_format_wprefix(fd,level, 3, prefix, prefix2, TEXT(".mfxExtAVCRefListCtrl.NumRefIdxL1Active="),TEXT("%d"), pRefList->NumRefIdxL1Active);

                for (int j = 0; j < sizeof(pRefList->PreferredRefList) / sizeof(pRefList->PreferredRefList[0]); j++)
                {
                    if (MFX_FRAMEORDER_UNKNOWN == pRefList->PreferredRefList[j].FrameOrder)
                        continue;
                    dump_format_wprefix(fd,level, 3, prefix, prefix2, TEXT(".mfxExtAVCRefListCtrl.PreferredRefList"),TEXT("[%d].FrameOrder=%d"), j, pRefList->PreferredRefList[j].FrameOrder);
                    dump_format_wprefix(fd,level, 3, prefix, prefix2, TEXT(".mfxExtAVCRefListCtrl.PreferredRefList"),TEXT("[%d].PicStruct=%s"), j, GET_STRING_FROM_CODE_TABLE(pRefList->PreferredRefList[j].PicStruct, tbl_picstruct));
                    dump_format_wprefix(fd,level, 3, prefix, prefix2, TEXT(".mfxExtAVCRefListCtrl.PreferredRefList"),TEXT("[%d].ViewId=%u"), j, pRefList->PreferredRefList[j].ViewId);
                }
                for (int j = 0; j < sizeof(pRefList->RejectedRefList) / sizeof(pRefList->RejectedRefList[0]); j++)
                {
                    if (MFX_FRAMEORDER_UNKNOWN == pRefList->RejectedRefList[j].FrameOrder)
                        continue;
                    dump_format_wprefix(fd,level, 3, prefix, prefix2, TEXT(".mfxExtAVCRefListCtrl.RejectedRefList"),TEXT("[%d].FrameOrder=%d"), j, pRefList->RejectedRefList[j].FrameOrder);
                    dump_format_wprefix(fd,level, 3, prefix, prefix2, TEXT(".mfxExtAVCRefListCtrl.RejectedRefList"),TEXT("[%d].PicStruct=%s"), j, GET_STRING_FROM_CODE_TABLE(pRefList->RejectedRefList[j].PicStruct, tbl_picstruct));
                    dump_format_wprefix(fd,level, 3, prefix, prefix2, TEXT(".mfxExtAVCRefListCtrl.RejectedRefList"),TEXT("[%d].ViewId=%u"), j, pRefList->RejectedRefList[j].ViewId);
                }
                for (int j = 0; j < sizeof(pRefList->LongTermRefList) / sizeof(pRefList->LongTermRefList[0]); j++)
                {
                    if (MFX_FRAMEORDER_UNKNOWN == pRefList->LongTermRefList[j].FrameOrder)
                        continue;
                    dump_format_wprefix(fd,level, 3, prefix, prefix2, TEXT(".mfxExtAVCRefListCtrl.LongTermRefList"),TEXT("[%d].FrameOrder=%d"), j, pRefList->LongTermRefList[j].FrameOrder);
                    dump_format_wprefix(fd,level, 3, prefix, prefix2, TEXT(".mfxExtAVCRefListCtrl.LongTermRefList"),TEXT("[%d].PicStruct=%s"), j, GET_STRING_FROM_CODE_TABLE(pRefList->LongTermRefList[j].PicStruct, tbl_picstruct));
                    dump_format_wprefix(fd,level, 3, prefix, prefix2, TEXT(".mfxExtAVCRefListCtrl.LongTermRefList"),TEXT("[%d].ViewId=%u"), j, pRefList->LongTermRefList[j].ViewId);
                }
                break;
            }
            case MFX_EXTBUFF_VIDEO_SIGNAL_INFO :
            {
                mfxExtVideoSignalInfo *vs = (mfxExtVideoSignalInfo*)eb;
                dump_format_wprefix(fd,level, 3, prefix, prefix2, TEXT(".mfxExtVideoSignalInfo.LongTermRefList.VideoFormat="),TEXT("%d"), vs->VideoFormat);
                dump_format_wprefix(fd,level, 3, prefix, prefix2, TEXT(".mfxExtVideoSignalInfo.LongTermRefList.VideoFullRange="),TEXT("%d"), vs->VideoFullRange);
                dump_format_wprefix(fd,level, 3, prefix, prefix2, TEXT(".mfxExtVideoSignalInfo.LongTermRefList.ColourDescriptionPresent="),TEXT("%d"), vs->ColourDescriptionPresent);
                dump_format_wprefix(fd,level, 3, prefix, prefix2, TEXT(".mfxExtVideoSignalInfo.LongTermRefList.ColourPrimaries="),TEXT("%d"), vs->ColourPrimaries);
                dump_format_wprefix(fd,level, 3, prefix, prefix2, TEXT(".mfxExtVideoSignalInfo.LongTermRefList.TransferCharacteristics="),TEXT("%d"), vs->TransferCharacteristics);
                dump_format_wprefix(fd,level, 3, prefix, prefix2, TEXT(".mfxExtVideoSignalInfo.LongTermRefList.MatrixCoefficients="),TEXT("%d"), vs->MatrixCoefficients);
                break;
            }

            case MFX_EXTBUFF_PICTURE_TIMING_SEI : 
            {
                mfxExtPictureTimingSEI *ptSei = (mfxExtPictureTimingSEI *)eb;
                const TCHAR prefix3 []= TEXT(".mfxExtPictureTimingSEI.TimeStamp");
                for (int j = 0; j < sizeof(ptSei->TimeStamp) / sizeof(ptSei->TimeStamp[0]); j++)
                {
                    dump_format_wprefix(fd, level, 3, prefix, prefix2, prefix3,TEXT("[%d].ClockTimestampFlag=%d"), j, ptSei->TimeStamp[j].ClockTimestampFlag);
                    dump_format_wprefix(fd, level, 3, prefix, prefix2, prefix3,TEXT("[%d].CtType=%d"), j, ptSei->TimeStamp[j].CtType);
                    dump_format_wprefix(fd, level, 3, prefix, prefix2, prefix3,TEXT("[%d].CountingType=%d"), j, ptSei->TimeStamp[j].CountingType);
                    dump_format_wprefix(fd, level, 3, prefix, prefix2, prefix3,TEXT("[%d].FullTimestampFlag=%d"), j, ptSei->TimeStamp[j].FullTimestampFlag);
                    dump_format_wprefix(fd, level, 3, prefix, prefix2, prefix3,TEXT("[%d].DiscontinuityFlag=%d"), j, ptSei->TimeStamp[j].DiscontinuityFlag);
                    dump_format_wprefix(fd, level, 3, prefix, prefix2, prefix3,TEXT("[%d].CntDroppedFlag=%d"), j, ptSei->TimeStamp[j].CntDroppedFlag);
                    dump_format_wprefix(fd, level, 3, prefix, prefix2, prefix3,TEXT("[%d].NFrames=%d"), j, ptSei->TimeStamp[j].NFrames);
                    dump_format_wprefix(fd, level, 3, prefix, prefix2, prefix3,TEXT("[%d].SecondsFlag=%d"), j, ptSei->TimeStamp[j].SecondsFlag);
                    dump_format_wprefix(fd, level, 3, prefix, prefix2, prefix3,TEXT("[%d].MinutesFlag=%d"), j, ptSei->TimeStamp[j].MinutesFlag);
                    dump_format_wprefix(fd, level, 3, prefix, prefix2, prefix3,TEXT("[%d].HoursFlag=%d"), j, ptSei->TimeStamp[j].HoursFlag);
                    dump_format_wprefix(fd, level, 3, prefix, prefix2, prefix3,TEXT("[%d].SecondsValue=%d"), j, ptSei->TimeStamp[j].SecondsValue);
                    dump_format_wprefix(fd, level, 3, prefix, prefix2, prefix3,TEXT("[%d].MinutesValue=%d"), j, ptSei->TimeStamp[j].MinutesValue);
                    dump_format_wprefix(fd, level, 3, prefix, prefix2, prefix3,TEXT("[%d].HoursValue=%d"), j, ptSei->TimeStamp[j].HoursValue);
                    dump_format_wprefix(fd, level, 3, prefix, prefix2, prefix3,TEXT("[%d].TimeOffset=%d"), j, ptSei->TimeStamp[j].TimeOffset);
                }
                break;
            }

            case MFX_EXTBUFF_CODING_OPTION2: 
            {
                mfxExtCodingOption2 *co2=(mfxExtCodingOption2 *)eb;
                dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".mfxExtCodingOption2.IntRefType="),TEXT("%d"), co2->IntRefType);
                dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".mfxExtCodingOption2.IntRefCycleSize="),TEXT("%d"), co2->IntRefCycleSize);
                dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".mfxExtCodingOption2.IntRefQPDelta="),TEXT("%d"), co2->IntRefQPDelta);
                dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".mfxExtCodingOption2.MaxFrameSize="),TEXT("%d"), co2->MaxFrameSize);
                dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".mfxExtCodingOption2.MaxSliceSize="),TEXT("%d"), co2->MaxSliceSize);
                dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".mfxExtCodingOption2.BitrateLimit="),TEXT("%d"), co2->BitrateLimit);
                dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".mfxExtCodingOption2.MBBRC="),TEXT("%d"), co2->MBBRC);
                dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".mfxExtCodingOption2.ExtBRC="),TEXT("%d"), co2->ExtBRC);
                dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".mfxExtCodingOption2.LookAheadDepth="),TEXT("%d"), co2->LookAheadDepth);
                dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".mfxExtCodingOption2.Trellis="),TEXT("%d"), co2->Trellis);
                dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".mfxExtCodingOption2.RepeatPPS="),TEXT("%d"), co2->RepeatPPS);
                dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".mfxExtCodingOption2.BRefType="),TEXT("%d"), co2->BRefType);
                dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".mfxExtCodingOption2.AdaptiveI="),TEXT("%d"), co2->AdaptiveI);
                dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".mfxExtCodingOption2.AdaptiveB="),TEXT("%d"), co2->AdaptiveB);
                dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".mfxExtCodingOption2.LookAheadDS="),TEXT("%d"), co2->LookAheadDS);
                dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".mfxExtCodingOption2.NumMbPerSlice="),TEXT("%d"), co2->NumMbPerSlice);
                dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".mfxExtCodingOption2.SkipFrame="),TEXT("%d"), co2->SkipFrame);
                dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".mfxExtCodingOption2.MinQPI="),TEXT("%d"), co2->MinQPI);
                dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".mfxExtCodingOption2.MaxQPI="),TEXT("%d"), co2->MaxQPI);
                dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".mfxExtCodingOption2.MinQPP="),TEXT("%d"), co2->MinQPP);
                dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".mfxExtCodingOption2.MaxQPP="),TEXT("%d"), co2->MaxQPP);
                dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".mfxExtCodingOption2.MinQPB="),TEXT("%d"), co2->MinQPB);
                dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".mfxExtCodingOption2.MaxQPB="),TEXT("%d"), co2->MaxQPB);
                dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".mfxExtCodingOption2.FixedFrameRate="),TEXT("%d"), co2->FixedFrameRate);
                dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".mfxExtCodingOption2.DisableDeblockingIdc="),TEXT("%d"), co2->DisableDeblockingIdc);
                dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".mfxExtCodingOption2.DisableVUI="),TEXT("%d"), co2->DisableVUI);
                dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".mfxExtCodingOption2.BufferingPeriodSEI="),TEXT("%d"), co2->BufferingPeriodSEI);
                break;
            }

            case MFX_EXTBUFF_ENCODER_CAPABILITY:
            {
                mfxExtEncoderCapability *ec=(mfxExtEncoderCapability *)eb;
                dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".mfxExtEncoderCapability.MBPerSec="),TEXT("%d"), ec->MBPerSec);
                break;
            }

            case MFX_EXTBUFF_ENCODER_RESET_OPTION:
            {
                mfxExtEncoderResetOption *ero=(mfxExtEncoderResetOption *)eb;
                dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".mfxExtEncoderResetOption.StartNewSequence="),TEXT("%d"), ero->StartNewSequence);
                break;
            }

            case MFX_EXTBUFF_ENCODED_FRAME_INFO:
            {
                mfxExtAVCEncodedFrameInfo *efi=(mfxExtAVCEncodedFrameInfo *)eb;

                dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".mfxExtAVCEncodedFrameInfo.FrameOrder="),TEXT("%d"), efi->FrameOrder);
                dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".mfxExtAVCEncodedFrameInfo.PicStruct="),TEXT("%d"), efi->PicStruct);
                dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".mfxExtAVCEncodedFrameInfo.LongTermIdx="),TEXT("%d"), efi->LongTermIdx);
                dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".mfxExtAVCEncodedFrameInfo.MAD="),TEXT("%d"), efi->MAD);
                dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".mfxExtAVCEncodedFrameInfo.BRCPanicMode="),TEXT("%d"), efi->BRCPanicMode);
                dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".mfxExtAVCEncodedFrameInfo.SecondFieldOffset="),TEXT("%d"), efi->SecondFieldOffset);

                for (int j = 0; j < sizeof(efi->UsedRefListL0) / sizeof(efi->UsedRefListL0[0]); j++)
                {
                    if (MFX_FRAMEORDER_UNKNOWN == efi->UsedRefListL0[j].FrameOrder)
                        continue;
                    dump_format_wprefix(fd,level, 3, prefix, prefix2, TEXT(".mfxExtAVCEncodedFrameInfo.UsedRefListL0"),TEXT("[%d].FrameOrder=%d"), j, efi->UsedRefListL0[j].FrameOrder);
                    dump_format_wprefix(fd,level, 3, prefix, prefix2, TEXT(".mfxExtAVCEncodedFrameInfo.UsedRefListL0"),TEXT("[%d].PicStruct=%s"), j, GET_STRING_FROM_CODE_TABLE(efi->UsedRefListL0[j].PicStruct, tbl_picstruct));
                    dump_format_wprefix(fd,level, 3, prefix, prefix2, TEXT(".mfxExtAVCEncodedFrameInfo.UsedRefListL0"),TEXT("[%d].LongTermIdx=%u"), j, efi->UsedRefListL0[j].LongTermIdx);
                }
                for (int j = 0; j < sizeof(efi->UsedRefListL1) / sizeof(efi->UsedRefListL1[0]); j++)
                {
                    if (MFX_FRAMEORDER_UNKNOWN == efi->UsedRefListL1[j].FrameOrder)
                        continue;
                    dump_format_wprefix(fd,level, 3, prefix, prefix2, TEXT(".mfxExtAVCEncodedFrameInfo.UsedRefListL1"),TEXT("[%d].FrameOrder=%d"), j, efi->UsedRefListL1[j].FrameOrder);
                    dump_format_wprefix(fd,level, 3, prefix, prefix2, TEXT(".mfxExtAVCEncodedFrameInfo.UsedRefListL1"),TEXT("[%d].PicStruct=%s"), j, GET_STRING_FROM_CODE_TABLE(efi->UsedRefListL1[j].PicStruct, tbl_picstruct));
                    dump_format_wprefix(fd,level, 3, prefix, prefix2, TEXT(".mfxExtAVCEncodedFrameInfo.UsedRefListL1"),TEXT("[%d].LongTermIdx=%u"), j, efi->UsedRefListL1[j].LongTermIdx);
                }

                break;
            }

            case MFX_EXTBUFF_ENCODER_ROI:
            {
                mfxExtEncoderROI *eroi=(mfxExtEncoderROI *)eb;

                dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".mfxExtEncoderROI.NumROI="),TEXT("%d"), eroi->NumROI);

                for (int j = 0; j < sizeof(eroi->NumROI); j++)
                {
                    dump_format_wprefix(fd,level, 3, prefix, prefix2, TEXT(".mfxExtEncoderROI.ROI"),TEXT("[%d].Left=%d"), j, eroi->ROI[j].Left);
                    dump_format_wprefix(fd,level, 3, prefix, prefix2, TEXT(".mfxExtEncoderROI.ROI"),TEXT("[%d].Top=%d"), j, eroi->ROI[j].Top);
                    dump_format_wprefix(fd,level, 3, prefix, prefix2, TEXT(".mfxExtEncoderROI.ROI"),TEXT("[%d].Right=%d"), j, eroi->ROI[j].Right);
                    dump_format_wprefix(fd,level, 3, prefix, prefix2, TEXT(".mfxExtEncoderROI.ROI"),TEXT("[%d].Bottom=%d"), j, eroi->ROI[j].Bottom);
                    dump_format_wprefix(fd,level, 3, prefix, prefix2, TEXT(".mfxExtEncoderROI.ROI"),TEXT("[%d].Priority=%d"), j, eroi->ROI[j].Priority);
                }

                break;
            }

            case MFX_EXTBUFF_VPP_DEINTERLACING:
            {
                mfxExtVPPDeinterlacing *vppdi=(mfxExtVPPDeinterlacing *)eb;
                dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".mfxExtVPPDeinterlacing.Mode="),TEXT("%d"), vppdi->Mode);
                break;
            }

            case MFX_EXTBUFF_LOOKAHEAD_CTRL:
            {
                mfxExtLAControl *lac=(mfxExtLAControl *)eb;
                dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".mfxExtLAControl.LookAheadDepth="),TEXT("%d"), lac->LookAheadDepth);
                dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".mfxExtLAControl.DependencyDepth="),TEXT("%d"), lac->DependencyDepth);
                dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".mfxExtLAControl.DownScaleFactor="),TEXT("%d"), lac->DownScaleFactor);
                dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".mfxExtLAControl.NumOutStream="),TEXT("%d"), lac->NumOutStream);
                for (int j = 0; j < sizeof(lac->OutStream) / sizeof(lac->OutStream[0]); j++)
                {
                    if (lac->OutStream[j].Width != 0 && lac->OutStream[j].Height != 0)
                    {
                        dump_format_wprefix(fd,level, 3, prefix, prefix2, TEXT(".mfxExtLAControl.OutStream"),TEXT("[%d].Width=%d"), j, lac->OutStream[j].Width);
                        dump_format_wprefix(fd,level, 3, prefix, prefix2, TEXT(".mfxExtLAControl.OutStream"),TEXT("[%d].Height=%d"), j, lac->OutStream[j].Height);
                    }
                }
                break;
            }

            case MFX_EXTBUFF_LOOKAHEAD_STAT:
            {
                mfxExtLAFrameStatistics *lafs=(mfxExtLAFrameStatistics *)eb;

                dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".mfxExtLAFrameStatistics.NumAlloc="),TEXT("%d"), lafs->NumAlloc);
                dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".mfxExtLAFrameStatistics.NumStream="),TEXT("%d"), lafs->NumStream);
                dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".mfxExtLAFrameStatistics.NumFrame="),TEXT("%d"), lafs->NumFrame);
                if (lafs->FrameStat != NULL)
                {
                    dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".mfxExtLAFrameStatistics.FrameStat.Width="),TEXT("%d"), lafs->FrameStat->Width);
                    dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".mfxExtLAFrameStatistics.FrameStat.Height="),TEXT("%d"), lafs->FrameStat->Height);
                    dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".mfxExtLAFrameStatistics.FrameStat.FrameType="),TEXT("%d"), lafs->FrameStat->FrameType);
                    dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".mfxExtLAFrameStatistics.FrameStat.FrameDisplayOrder="),TEXT("%d"), lafs->FrameStat->FrameDisplayOrder);
                    dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".mfxExtLAFrameStatistics.FrameStat.FrameEncodeOrder="),TEXT("%d"), lafs->FrameStat->FrameEncodeOrder);
                    dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".mfxExtLAFrameStatistics.FrameStat.IntraCost="),TEXT("%d"), lafs->FrameStat->IntraCost);
                    dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".mfxExtLAFrameStatistics.FrameStat.InterCost="),TEXT("%d"), lafs->FrameStat->InterCost);
                    dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".mfxExtLAFrameStatistics.FrameStat.DependencyCost="),TEXT("%d"), lafs->FrameStat->DependencyCost);
                    std::basic_stringstream<TCHAR> stream;
                    for (int j = 0; j < sizeof(lafs->FrameStat->EstimatedRate) / sizeof(lafs->FrameStat->EstimatedRate[0]); j++)
                    {
                        stream<<lafs->FrameStat->EstimatedRate[j];
                        if (j + 1 != sizeof(lafs->FrameStat->EstimatedRate) / sizeof(lafs->FrameStat->EstimatedRate[0]))
                        {
                            stream<<TEXT(", ");
                        }
                    }
                    dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".mfxExtLAFrameStatistics.FrameStat.EstimatedRate=.mfxExtLAFrameStatistics.FrameStat.EstimatedRate="),TEXT("{%s}"),  stream.str().c_str());
                    //mfxFrameSurface1 *OutSurface;
                }
                else
                {
                    dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".mfxExtLAFrameStatistics.FrameStat=NULL"));
                }
                break;
            }

            default:
                dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".BufferId="),TEXT("%d"),  eb->BufferId);
                dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".BufferSz="),TEXT("%d"),  eb->BufferSz);
                break;
            }
        }
    }
    catch (...)
    {
        dump_format_wprefix(fd,level, 2,prefix,prefix2,TEXT("=0x%p (ERROR: Exception during extended Buffer access)"), bfs);
    }
}

void dump_mfxVideoParam(FILE *fd, int level, TCHAR *prefix, Component c, mfxVideoParam *vp, bool bDumpExtBuffer) {
    if (!vp) {
        dump_format_wprefix(fd,level, 2,prefix,TEXT("=NULL"),TEXT(""));
        return;
    }
    dump_format_wprefix(fd,level, 2, prefix, TEXT(".AsyncDepth="), TEXT("%d"), vp->AsyncDepth);

    if (c==DECODE || c==ENCODE) {
        dump_mfxInfoMFX(fd,level,  prefix, TEXT(".mfx"), c, &vp->mfx);
    } else {
        dump_mfxInfoVPP(fd,level,  prefix, TEXT(".vpp"), &vp->vpp);
    }
    dump_format_wprefix(fd,level, 2,prefix,TEXT(".Protected="),TEXT("%d"), vp->Protected);
    dump_format_wprefix(fd,level, 2,prefix,TEXT(".IOPattern="),TEXT("%s"), GET_STRING_FROM_CODE_TABLE(vp->IOPattern, tbl_iopattern));
    dump_format_wprefix(fd,level, 2,prefix,TEXT(".NumExtParam="),TEXT("%d"), vp->NumExtParam);

    if (bDumpExtBuffer)
    {
        dump_ExtBuffers(fd,level, prefix, TEXT(".ExtParam"), (c==VPP?0:vp->mfx.CodecId), vp->ExtParam, vp->NumExtParam);
    }
}

void dump_mfxFrameAllocRequest(FILE *fd, int level, TCHAR *prefix, mfxFrameAllocRequest *request) {
    if (request) {
        dump_mfxFrameInfo(fd,level,  prefix, TEXT(".Info"), TEXT(""), &request->Info);

        dump_format_wprefix(fd,level, 2,prefix,TEXT(".Type="),TEXT("%s"),  GET_STRING_FROM_CODE_TABLE(request->Type,tbl_mem_types));
        dump_format_wprefix(fd,level, 2,prefix,TEXT(".NumFrameMin="),TEXT("%d"),request->NumFrameMin);
        dump_format_wprefix(fd,level, 2,prefix,TEXT(".NumFrameSuggested="),TEXT("%d"),request->NumFrameSuggested);
    } else {
        dump_format_wprefix(fd,level, 2,prefix,TEXT("=NULL"),TEXT(""));
    }
}

void dump_mfxEncodeStat(FILE *fd, int level, TCHAR *prefix, mfxEncodeStat *stat) {
    if (stat) {
        dump_format_wprefix(fd,level, 2,prefix,TEXT(".NumFrame="),TEXT("%d"),stat->NumFrame);
        dump_format_wprefix(fd,level, 2,prefix,TEXT(".NumBit="),TEXT("%d"),stat->NumBit);
        dump_format_wprefix(fd,level, 2,prefix,TEXT(".NumCachedFrame="),TEXT("%d"),stat->NumCachedFrame);
    } else {
        dump_format_wprefix(fd,level, 2,prefix,TEXT("=NULL"),TEXT(""));
    }
}

void dump_mfxDecodeStat(FILE *fd, int level, TCHAR *prefix, mfxDecodeStat *stat) {
    if (stat) {
        dump_format_wprefix(fd,level, 2,prefix,TEXT(".NumFrame="),TEXT("%d"),stat->NumFrame);
        dump_format_wprefix(fd,level, 2,prefix,TEXT(".NumSkippedFrame="),TEXT("%d"),stat->NumSkippedFrame);
        dump_format_wprefix(fd,level, 2,prefix,TEXT(".NumError="),TEXT("%d"),stat->NumError);
        dump_format_wprefix(fd,level, 2,prefix,TEXT(".NumCachedFrame="),TEXT("%d"),stat->NumCachedFrame);
    } else {
        dump_format_wprefix(fd,level, 2,prefix,TEXT("=NULL"),TEXT(""));
    }
}

void dump_mfxVPPStat(FILE *fd, int level, TCHAR *prefix, mfxVPPStat *stat) {
    if (stat) {
        dump_format_wprefix(fd,level, 2,prefix,TEXT(".NumFrame="),TEXT("%d"),stat->NumFrame);
        dump_format_wprefix(fd,level, 2,prefix,TEXT(".NumCachedFrame="),TEXT("%d"),stat->NumCachedFrame);
    } else {
        dump_format_wprefix(fd,level, 2,prefix,TEXT("=NULL"),TEXT(""));
    }
}

void dump_mfxEncodeCtrl(FILE *fd, int level, TCHAR *prefix, mfxEncodeCtrl *ctl) {
    if (ctl) {
        dump_format_wprefix(fd,level, 2,prefix,TEXT(".QP="),TEXT("%d"),ctl->QP);
        dump_format_wprefix(fd,level, 2,prefix,TEXT(".FrameType="),TEXT("%s"),GET_STRING_FROM_CODE_TABLE(ctl->FrameType, tbl_frame_type));
        dump_ExtBuffers(fd, level, prefix, TEXT(".ExtParam"), 0, ctl->ExtParam, ctl->NumExtParam);
        dump_format_wprefix(fd,level, 2,prefix,TEXT(".NumPayload="),TEXT("%d"),ctl->NumPayload);
    } else {
        dump_format_wprefix(fd,level, 2,prefix,TEXT("=NULL"),TEXT(""));
    }
}

void dump_mfxFrameSurface1Raw(FILE *fd, mfxFrameSurface1 *pSurface, mfxFrameAllocator *pAlloc)
{
    // NULL means flushing (end of stream)
    if(NULL == pSurface) return ;
    
    mfxFrameData frame;
    bool bLocked = false;
    if (!pSurface->Data.Y)
    {
        if (NULL == pAlloc || NULL == pSurface->Data.MemId)
            return;
        if (MFX_ERR_NONE != pAlloc->Lock(pAlloc->pthis, pSurface->Data.MemId, &frame))
            return;

        bLocked = true;
    }else
    {
        memcpy_s(&frame, sizeof(mfxFrameData), &pSurface->Data, sizeof(mfxFrameData));
    }

    // Need to write raw surfaces so that ROI can be effectively checked.
    switch (pSurface->Info.FourCC) {
        case MFX_FOURCC_NV12:
        {
            // write Y
            BYTE *p=(BYTE *)frame.Y;
            mfxU32 i;
            for (i=0;i<(mfxU32)pSurface->Info.Height;i++) {
                fwrite(p,1,pSurface->Info.Width,fd);
                p+=frame.Pitch;
            }
            // write UV;
            p=(BYTE *)frame.UV;
            for (i=0;i<(mfxU32)pSurface->Info.Height/2;i++) {
                fwrite(p,1,pSurface->Info.Width,fd);
                p+=frame.Pitch;
            }

            break;
        }

        case MFX_FOURCC_YV12:
        {
            // write Y
            BYTE *p=(BYTE *)frame.Y;
            mfxU32 i;
            for (i=0;i<(mfxU32)pSurface->Info.Height;i++) {
                fwrite(p,1,pSurface->Info.Width,fd);
                p+=frame.Pitch;
            }
            // write V;
            p=(BYTE *)frame.U;
            for (i=0;i<(mfxU32)pSurface->Info.Height/2;i++) {
                fwrite(p,1,pSurface->Info.Width/2,fd);
                p+=frame.Pitch/2;
            }
            // write U;
            p=(BYTE *)frame.V;
            for (i=0;i<(mfxU32)pSurface->Info.Height/2;i++) {
                fwrite(p,1,pSurface->Info.Width/2,fd);
                p+=frame.Pitch/2;
            }
            break;
        }
    }
    if (bLocked){
        pAlloc->Unlock(pAlloc->pthis, pSurface->Data.MemId, &frame);
    }
}

void dump_mfxBitstreamRaw(FILE *fd, mfxBitstream *bs) {
    if (!bs) return;
    if (!bs->Data) return;
    fwrite(bs->Data+bs->DataOffset,1,bs->DataLength,fd);
}

void dump_mfxFrameData(FILE *fd, int level, TCHAR *prefix, TCHAR *prefix2, mfxFrameData *data) {
    if (data) {
        dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".TimeStamp=0x"),TEXT("%x"),data->TimeStamp);
        dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".FrameOrder="),TEXT("%d"),data->FrameOrder);
        dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".Locked="),TEXT("%d"),data->Locked);
        dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".Pitch="),TEXT("%d"),data->Pitch);
        dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".PitchHigh="),TEXT("%d"),data->PitchHigh);
        dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".PitchLow="),TEXT("%d"),data->PitchLow);
        dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".Corrupted="),TEXT("%d"),data->Corrupted);
        RECORD_POINTERS(dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".Y=0x"),TEXT("%p"),data->Y));
        RECORD_POINTERS(dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".U=0x"),TEXT("%p"),data->U));
        RECORD_POINTERS(dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".V=0x"),TEXT("%p"),data->V));
        RECORD_POINTERS(dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".A=0x"),TEXT("%p"),data->A));
        RECORD_POINTERS(dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT(".MemId=0x"),TEXT("%p"),data->MemId));
    } else {
        dump_format_wprefix(fd,level, 3,prefix,prefix2,TEXT("=NULL"),TEXT(""));
    }
}

void dump_mfxFrameSurface1(FILE *fd, int level, TCHAR *prefix,mfxFrameSurface1 *fs) {

    RECORD_POINTERS(dump_format_wprefix(fd,level, 1,prefix,TEXT("=0x%p"), fs));
    if (fs) {
        dump_mfxFrameInfo(fd,level, prefix, TEXT(".Info"),TEXT(""),&fs->Info);
        dump_mfxFrameData(fd,level, prefix, TEXT(".Data"),&fs->Data);
    } 
}

void dump_mfxBitstream(FILE *fd, int level, TCHAR *prefix, mfxBitstream *bs) {
    if (bs) {
        dump_format_wprefix(fd,level, 2,prefix,TEXT(".DecodeTimeStamp=0x"),TEXT("%x"),bs->DecodeTimeStamp);
        dump_format_wprefix(fd,level, 2,prefix,TEXT(".TimeStamp=0x"),TEXT("%x"),bs->TimeStamp);
        RECORD_POINTERS(dump_format_wprefix(fd,level, 2,prefix,TEXT(".Data=0x"),TEXT("%p"), bs->Data));
        dump_format_wprefix(fd,level, 2,prefix,TEXT(".DataOffset="),TEXT("%d"),bs->DataOffset);
        dump_format_wprefix(fd,level, 2,prefix,TEXT(".DataLength="),TEXT("%d"), bs->DataLength);
        dump_format_wprefix(fd,level, 2,prefix,TEXT(".MaxLength="),TEXT("%d"), bs->MaxLength);
        dump_format_wprefix(fd,level, 2,prefix,TEXT(".PicStruct="),TEXT("%s"), GET_STRING_FROM_CODE_TABLE(bs->PicStruct, tbl_picstruct));
        dump_format_wprefix(fd,level, 2,prefix,TEXT(".FrameType="),TEXT("%s"), GET_STRING_FROM_CODE_TABLE(bs->FrameType, tbl_frame_type));
        dump_format_wprefix(fd,level, 2,prefix,TEXT(".DataFlag=0x"),TEXT("%x"), bs->DataFlag);
        dump_format_wprefix(fd,level, 2,prefix,TEXT(".NumExtParam="),TEXT("%d"), bs->NumExtParam);
        if (bs->NumExtParam > 0)
        {
            dump_ExtBuffers(fd,level, prefix, TEXT(".ExtParam"), 0, bs->ExtParam, bs->NumExtParam);
        }
    } else {
        dump_format_wprefix(fd,level, 2,prefix,TEXT("=NULL"),TEXT(""));
    }
}

static TCHAR *GetStatusString(mfxStatus sts) {
    static TCHAR status_string[128];
#define CASENO(XX) case XX: return TEXT(#XX)
    switch (sts) {
    CASENO(MFX_ERR_NONE);
    CASENO(MFX_ERR_UNKNOWN);
    CASENO(MFX_ERR_NULL_PTR);
    CASENO(MFX_ERR_UNSUPPORTED);
    CASENO(MFX_ERR_MEMORY_ALLOC);
    CASENO(MFX_ERR_NOT_ENOUGH_BUFFER);
    CASENO(MFX_ERR_INVALID_HANDLE);
    CASENO(MFX_ERR_LOCK_MEMORY);
    CASENO(MFX_ERR_NOT_INITIALIZED);
    CASENO(MFX_ERR_NOT_FOUND);
    CASENO(MFX_ERR_MORE_DATA);
    CASENO(MFX_ERR_MORE_SURFACE);
    CASENO(MFX_ERR_ABORTED);
    CASENO(MFX_ERR_DEVICE_LOST);
    CASENO(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);
    CASENO(MFX_ERR_INVALID_VIDEO_PARAM);
    CASENO(MFX_ERR_UNDEFINED_BEHAVIOR);
    CASENO(MFX_ERR_DEVICE_FAILED);
    CASENO(MFX_ERR_MORE_BITSTREAM);
    CASENO(MFX_WRN_IN_EXECUTION);
    CASENO(MFX_WRN_DEVICE_BUSY);
    CASENO(MFX_WRN_VIDEO_PARAM_CHANGED);
    CASENO(MFX_WRN_PARTIAL_ACCELERATION);
    CASENO(MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);
    CASENO(MFX_WRN_VALUE_NOT_CHANGED);
    CASENO(MFX_WRN_FILTER_SKIPPED);
    default:
        _stprintf_s(status_string,128,TEXT("UNDEFINED(%d)"),sts);
        return status_string;
    }
}

void dump_mfxStatus(FILE *fd, int level, TCHAR *prefix, mfxStatus sts) {
    dump_format_wprefix(fd,level, 2,prefix,TEXT(".status="),TEXT("%s"),GetStatusString(sts));
}

void dump_StatusMap(FILE *fd, int level, TCHAR *prefix, StatusMap *sm) {
    for (int i=0;;i++) {
        mfxStatus sts; int ctr=0;
        if (!sm->Traverse(i,&sts,&ctr)) break;
        if (ctr>0) dump_format_wprefix(fd,level, 1,prefix,TEXT(": EXTERNAL : %s (%d)"), GetStatusString(sts),ctr);
    }
    for (int i=0;;i++) {
        mfxStatus sts; int ctr=0;
        if (!sm->TraverseInternal(i,&sts,&ctr)) break;
        if (ctr>0) dump_format_wprefix(fd,level, 1,prefix,TEXT(": INTERNAL : %s (%d)"), GetStatusString(sts),ctr);
    }
}

void dump_mfxIMPL(FILE *fd, int level, TCHAR *prefix, mfxIMPL impl) {
    
    std::basic_stringstream<TCHAR> stream;
    stream << GET_STRING_FROM_CODE_TABLE_EQUAL( impl & (MFX_IMPL_VIA_ANY - 1), tbl_impl);
    
    int via_flag = impl & ~(MFX_IMPL_VIA_ANY - 1);

    if (0 != via_flag)
    {
        stream << TEXT("|") << GET_STRING_FROM_CODE_TABLE_EQUAL(via_flag, tbl_impl);
    }
    dump_format_wprefix(fd,level, 1, prefix, TEXT(".impl=%s"), stream.str().c_str());
}

void dump_mfxVersion(FILE *fd, int level, TCHAR *prefix, mfxVersion *ver) {
    if (ver) {
        dump_format_wprefix(fd,level, 2,prefix,TEXT(".ver="),TEXT("%d.%d"), ver->Major, ver->Minor);
    } else {
        dump_format_wprefix(fd,level, 2,prefix,TEXT("=NULL"),TEXT(""));
    }
}

void dump_mfxPriority(FILE *fd, int level, TCHAR *prefix, mfxPriority priority) {
    switch (priority) {
    case MFX_PRIORITY_LOW:
        dump_format_wprefix(fd,level, 2,prefix,TEXT(".priority=LOW"),TEXT(""));
        break;
    case MFX_PRIORITY_NORMAL:
        dump_format_wprefix(fd,level, 2,prefix,TEXT(".priority=NORMAL"),TEXT(""));
        break;
    case MFX_PRIORITY_HIGH:
        dump_format_wprefix(fd,level, 2,prefix,TEXT(".priority=HIGH"),TEXT(""));
        break;
    default:
        dump_format_wprefix(fd,level, 2,prefix,TEXT(".priority=") ,TEXT("%d"),(int)priority);
        break;
    }
}

void dump_format_wprefix(FILE *fd, int level, int nPrefix, TCHAR *format,...)
{
    va_list args;
    va_start(args, format);

    GlobalConfiguration::ThreadData & current_thread_info = gc.GetThreadData();
    TCHAR str [2048] = {0}, *pStr = str;
    static TCHAR stack_indicators[][16] = {
        TEXT(""), TEXT("  "), TEXT("    "), TEXT("      "),
    };

    if (!gc.async_dump)
    {
        //writing thread id firstly
        pStr += _stprintf_s(pStr, sizeof(str)/sizeof(str[0]) - (pStr - str),TEXT("%d:"), current_thread_info.thread_id);

        //writing timestamps
        if(gc.record_timestamp_mode != gc.TIMESTAMP_NONE)
        {
            TCHAR buf[1024];
            _vstprintf_s(buf, format, args);

            if (gc.record_timestamp_mode == gc.TIMESTAMP_UTC ||  gc.record_timestamp_mode == gc.TIMESTAMP_LOCAL)
            {
                SYSTEMTIME time = {0};
                if (gc.record_timestamp_mode == gc.TIMESTAMP_LOCAL)
                    GetLocalTime(&time);
                if (gc.record_timestamp_mode == gc.TIMESTAMP_UTC)
                    GetSystemTime(&time);

                pStr += _stprintf_s(pStr, sizeof(str)/sizeof(str[0]) - (pStr - str),TEXT("%d.%d.%d %d:%d:%d.%d:"), time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond, time.wMilliseconds);
            }else
            {
                LARGE_INTEGER cur;
                QueryPerformanceCounter(&cur);
                double diff = ((double) cur.QuadPart - (double)gc.qpc_start.QuadPart)/(double)(gc.qpc_freq.QuadPart);
                pStr += _stprintf_s(pStr, sizeof(str)/sizeof(str[0]) - (pStr - str), TEXT("%.3f:"), diff);
            }
        }
    }

    //writing prefixes
    GlobalConfiguration::TLSData d;
    memset(&d.prefix, NULL, sizeof(d.prefix));

    //write additional spaces indicated call stack
    int nPrefixIt = 0;
    if (current_thread_info.nStackSize > 1 && 
        (current_thread_info.nStackSize - 1) < sizeof(stack_indicators) / sizeof(stack_indicators[0]))
    {
        if (!gc.async_dump)
        {
            pStr += _stprintf_s(pStr, sizeof(str)/sizeof(str[0]) - (pStr -str), TEXT("%s") , stack_indicators[current_thread_info.nStackSize - 1]);
        }
        else
        {
            d.prefix[nPrefixIt++] = stack_indicators[current_thread_info.nStackSize - 1];
        }
    }

    for(int i = 0; i < nPrefix; i++)
    {
        if (0 == i)
            d.prefix[i + nPrefixIt] = format;
        else
            d.prefix[i + nPrefixIt] = va_arg(args, TCHAR*);

        if (!gc.async_dump)
        {        
            _tcscat_s(str, sizeof(str)/sizeof(str[0]), d.prefix[i+nPrefixIt]);
        }
    }
    //restoring end pointer due to tcscat usage
    pStr = str + _tcslen(str);

    TCHAR *new_format = format;

    //restoring pointer to actual format string
    if (0 != nPrefix)
    {
        new_format = va_arg(args, TCHAR*);
    }

    pStr += _vstprintf_s(pStr, sizeof(str)/sizeof(str[0]) - (pStr -str), new_format, args);

    if(gc.async_dump)
    {
        d.str   = str;
        d.level = level;
        QueryPerformanceCounter(&d.timestamp);
        current_thread_info.messages.push_back(d);
    }
    else if (NULL != fd)
    {
        _ftprintf(fd, _T("%s\n"), str);
    }
}

void dump_mfxENCInput(FILE *fd, int level, TCHAR *prefix, mfxENCInput *in)
{
    if (in) {
        //mfxFrameSurface1 *InSurface;
        dump_format_wprefix(fd,level, 2,prefix,TEXT(".NumFrameL0="),TEXT("%d"),in->NumFrameL0);
        //mfxFrameSurface1 **L0Surface;
        dump_format_wprefix(fd,level, 2,prefix,TEXT(".NumFrameL1="),TEXT("%d"),in->NumFrameL1);
        //mfxFrameSurface1 **L1Surface;
        dump_format_wprefix(fd,level, 2,prefix,TEXT(".NumExtParam="),TEXT("%d"),in->NumExtParam);
        if (in->NumExtParam > 0)
        {
            dump_ExtBuffers(fd,level, prefix, TEXT(".ExtParam"), 0, in->ExtParam, in->NumExtParam);
        }
    } else {
        dump_format_wprefix(fd,level, 2,prefix,TEXT("=NULL"),TEXT(""));
    }
}

void dump_mfxENCOutput(FILE *fd, int level, TCHAR *prefix, mfxENCOutput *out)
{
    if (out) {
        dump_format_wprefix(fd,level, 2,prefix,TEXT(".NumExtParam="),TEXT("%d"),out->NumExtParam);
        if (out->NumExtParam > 0)
        {
            dump_ExtBuffers(fd,level, prefix, TEXT(".ExtParam"), 0, out->ExtParam, out->NumExtParam);
        }
    } else {
        dump_format_wprefix(fd,level, 2,prefix,TEXT("=NULL"),TEXT(""));
    }
}

void DispatcherLogRecorder::Write(int level, int /*opcode*/, const char * msg, va_list argptr)
{
    if (NULL == msg || level == DL_LOADED_LIBRARY)
        return;

    //we have to manually format message since we don't support char->tchar conversions
    char message_formated[1024];
    vsprintf_s(message_formated, sizeof(message_formated)/sizeof(message_formated[0]), msg, argptr);

    //todo: eliminate this
    if (message_formated[strlen(message_formated)-1] == '\n')
    {
        message_formated[strlen(message_formated)-1] = 0;
    }

    TCHAR *pStr = NULL;
#ifdef UNICODE
    TCHAR input_str[1024];
    size_t nCopied;
    mbstowcs_s(&nCopied, input_str
              , sizeof(input_str)/sizeof(input_str[0])
              , message_formated
              , sizeof(input_str)/sizeof(input_str[0]));
    pStr = input_str;
#else
    pStr = message_formated;
#endif

    RECORD_CONFIGURATION({
        dump_format_wprefix(fd, level,  0 , TEXT("%s%s"), GET_STRING_FROM_CODE_TABLE(level, LevelStrings), pStr);
    });
}
