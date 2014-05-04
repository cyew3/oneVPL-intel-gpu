/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2009-2012 Intel Corporation. All Rights Reserved.

File Name: scan.cpp

\* ****************************************************************************** */
#include "msdka_serial.h"

#pragma warning(disable: 4244)

void scan_PicStruct(TCHAR *line, mfxU16 *ps) {
    if (NULL == ps)
        return;
    *ps=MFX_PICSTRUCT_UNKNOWN;
    if (_tcsstr(line,TEXT("PROGRESSIVE"))) *ps|=MFX_PICSTRUCT_PROGRESSIVE;
    if (_tcsstr(line,TEXT("FIELD_TFF"))) *ps|=MFX_PICSTRUCT_FIELD_TFF;
    if (_tcsstr(line,TEXT("FIELD_BFF"))) *ps|=MFX_PICSTRUCT_FIELD_BFF;
    if (_tcsstr(line,TEXT("FIELD_REPEATED"))) *ps|=MFX_PICSTRUCT_FIELD_REPEATED;
    if (_tcsstr(line,TEXT("FRAME_DOUBLING"))) *ps|=MFX_PICSTRUCT_FRAME_DOUBLING;
    if (_tcsstr(line,TEXT("FRAME_TRIPLING"))) *ps|=MFX_PICSTRUCT_FRAME_TRIPLING;
}

void scan_mfxFrameInfo(TCHAR *line, mfxFrameInfo *fi) {
    if (NULL == fi)
        return;
    if (_tcsstr(line,TEXT(".FourCC=NV12"))) fi->FourCC=MFX_FOURCC_NV12;
    if (_tcsstr(line,TEXT(".FourCC=RGB3"))) fi->FourCC=MFX_FOURCC_RGB3;
    if (_tcsstr(line,TEXT(".FourCC=RGB4"))) fi->FourCC=MFX_FOURCC_RGB4;
    if (_tcsstr(line,TEXT(".FourCC=YV12"))) fi->FourCC=MFX_FOURCC_YV12;
    if (_tcsstr(line,TEXT(".FourCC=YUY2"))) fi->FourCC=MFX_FOURCC_YUY2;

    int var1, var2, var3, var4;

    if (_stscanf_s(line,TEXT(".Resolution=(%d,%d)"), &var1, &var2)>=2) fi->Width=(mfxU16)var1, fi->Height=(mfxU16)var2;
    if (_stscanf_s(line,TEXT(".Crop=(%d,%d,%d,%d)"), &var1, &var2, &var3, &var4)>=4) fi->CropX=(mfxU16)var1,fi->CropY=(mfxU16)var2,fi->CropW=(mfxU16)var3,fi->CropH=(mfxU16)var4;
    if (_stscanf_s(line,TEXT(".FrameRate=%d/%d"), &var1, &var2)>=2) fi->FrameRateExtN=(mfxU16)var1, fi->FrameRateExtD=(mfxU16)var2;
    if (_stscanf_s(line,TEXT(".AspectRatio=%d:%d"), &var1, &var2)>=2) fi->AspectRatioW=(mfxU16)var1, fi->AspectRatioH=(mfxU16)var2;
    if (!_tcsncmp(line,TEXT(".PicStruct="),11)) scan_PicStruct(line+11,&fi->PicStruct);
    if (_tcsstr(line,TEXT(".ChromaFormat=MONO"))) fi->ChromaFormat=MFX_CHROMAFORMAT_MONOCHROME;
    if (_tcsstr(line,TEXT(".ChromaFormat=4:2:0"))) fi->ChromaFormat=MFX_CHROMAFORMAT_YUV420;
    if (_tcsstr(line,TEXT(".ChromaFormat=4:2:0"))) fi->ChromaFormat=MFX_CHROMAFORMAT_YUV422;
    if (_tcsstr(line,TEXT(".ChromaFormat=4:2:0"))) fi->ChromaFormat=MFX_CHROMAFORMAT_YUV444;
}

void scan_mfxInfoMFX(TCHAR *line, Component c, mfxInfoMFX *im) {
    if (NULL == im)
        return;

    int var;

    if (!_tcsncmp(line,TEXT(".FrameInfo"),10)) scan_mfxFrameInfo(line+10, &im->FrameInfo);
    if (_tcsstr(line,TEXT(".CodecId=AVC"))) im->CodecId=MFX_CODEC_AVC;
    if (_tcsstr(line,TEXT(".CodecId=MPEG2"))) im->CodecId=MFX_CODEC_MPEG2;
    if (_tcsstr(line,TEXT(".CodecId=VC1"))) im->CodecId=MFX_CODEC_VC1;
    if (_stscanf_s(line,TEXT(".CodecProfile=%d"), &var)>0) im->CodecProfile=(mfxU16)var;
    if (_stscanf_s(line,TEXT(".CodecLevel=%d"), &var)>0) im->CodecLevel=(mfxU16)var;
    if (_stscanf_s(line,TEXT(".NumThread=%d"), &var)>0) im->NumThread=(mfxU16)var;

    if (c==ENCODE) {
        if (_stscanf_s(line,TEXT(".TargetUsage=%d"), &var)>0) im->TargetUsage=(mfxU16)var;
        if (_stscanf_s(line,TEXT(".GopPicSize=%d"), &var)>0) im->GopPicSize=(mfxU16)var;
        if (_stscanf_s(line,TEXT(".GopRefDist=%d"), &var)>0) im->GopRefDist=(mfxU16)var;
        if (_stscanf_s(line,TEXT(".GopOptFlag=%d"), &var)>0) im->GopOptFlag=(mfxU16)var;
        if (_stscanf_s(line,TEXT(".IdrInterval=%d"), &var)>0) im->IdrInterval=(mfxU16)var;
        if (_stscanf_s(line,TEXT(".RateControlMethod=%d"), &var)>0) im->RateControlMethod=(mfxU16)var;
        if (_stscanf_s(line,TEXT(".InitialDelayInKB=%d"), &var)>0) im->InitialDelayInKB=(mfxU16)var;
        if (_stscanf_s(line,TEXT(".BufferSizeInKB=%d"), &var)>0) im->BufferSizeInKB=(mfxU16)var;
        if (_stscanf_s(line,TEXT(".TargetKbps=%d"), &var)>0) im->TargetKbps=(mfxU16)var;
        if (_stscanf_s(line,TEXT(".MaxKbps=%d"), &var)>0) im->MaxKbps=(mfxU16)var;
        if (_stscanf_s(line,TEXT(".NumSlice=%d"), &var)>0) im->NumSlice=(mfxU16)var;
        if (_stscanf_s(line,TEXT(".EncodedOrder=%d"), &var)>0) im->EncodedOrder=var;
    } else {
        if (_stscanf_s(line,TEXT(".DecodedOrder=%d\n"), &var)>0) im->DecodedOrder=(mfxU16)var;
        if (_stscanf_s(line,TEXT(".ExtendedPicStruct=%d\n"), &var)>0) im->ExtendedPicStruct=(mfxU16)var;
        if (im->CodecId==MFX_CODEC_AVC)
            if (_stscanf_s(line,TEXT(".SliceGroupsPresent=%d\n"), &var)>0) im->SliceGroupsPresent=(mfxU16)var;
    }
}

void scan_do_not_use_hint(TCHAR *line,mfxExtVPPDoNotUse *hint,ExtendedBufferOverride *ebo) {
    if (NULL == hint || NULL == ebo)
        return;
    mfxU32 *AlgList=(mfxU32 *)new mfxU32[hint->NumAlg+2];
    mfxU32 NumAlg=0;

    /* scan available hints */
    for (;line[0];line++) {
        if (!_tcsncmp(line,TEXT("DENOISE"),7)) AlgList[NumAlg++]=MFX_EXTBUFF_VPP_DENOISE;
        if (!_tcsncmp(line,TEXT("SCENE_ANALYSIS"),14)) AlgList[NumAlg++]=MFX_EXTBUFF_VPP_SCENE_ANALYSIS;
        line=_tcsstr(line,TEXT(","));
        if (!line) break;
    }

    /* add any unlisted hints */
    for (int i=0;i<(int)hint->NumAlg;i++) {
        if (hint->AlgList[i]==MFX_EXTBUFF_VPP_DENOISE) continue;
        if (hint->AlgList[i]==MFX_EXTBUFF_VPP_SCENE_ANALYSIS) continue;
        AlgList[NumAlg++]=hint->AlgList[i];
    }
    ebo->pNumAlg=&hint->NumAlg;
    ebo->NumAlg_old=hint->NumAlg;
    hint->NumAlg=NumAlg;

    ebo->pAlgList=&hint->AlgList;
    ebo->AlgList_old=hint->AlgList;
    hint->AlgList=AlgList;
}

void scan_ExtBuffers(TCHAR *line, int codec, mfxExtBuffer **bfs, int n,ExtendedBufferOverride *ebo) {
    if (NULL == bfs || NULL == ebo)
        return;
    int var, var2;
    for (int i=0;i<n;i++) {
        mfxExtBuffer *eb=bfs[i];
        switch (eb->BufferId) {
        case MFX_EXTBUFF_CODING_OPTION: {
                mfxExtCodingOption *co=(mfxExtCodingOption *)eb;
                if (_stscanf_s(line,TEXT(".mfxExtCodingOption.RateDistortionOpt=%d"),&var)>=1) co->RateDistortionOpt=(mfxU16)var;
                if (_stscanf_s(line,TEXT(".mfxExtCodingOption.MECostType=%d"),&var)>=1) co->MECostType=(mfxU16)var;
                if (_stscanf_s(line,TEXT(".mfxExtCodingOption.MESearchType=%d"),&var)>=1) co->MESearchType=(mfxU16)var;
                if (_stscanf_s(line,TEXT(".mfxExtCodingOption.MVSearchWindow=(%d,%d)"),&var,&var2)>=2) co->MVSearchWindow.x=(mfxU16)var, co->MVSearchWindow.y=(mfxU16)var2;
                if (_stscanf_s(line,TEXT(".mfxExtCodingOption.EndOfSequence=%d"),&var)>=1) co->EndOfSequence=(mfxU16)var;
                if (_stscanf_s(line,TEXT(".mfxExtCodingOption.FramePicture=%d"),&var)>=1) co->FramePicture=(mfxU16)var;
                if (codec==MFX_CODEC_AVC) {
                    if (_stscanf_s(line,TEXT(".mfxExtCodingOption.CAVLC=%d"),&var)>=1) co->CAVLC=(mfxU16)var;
                    if (_stscanf_s(line,TEXT(".mfxExtCodingOption.RecoveryPointSEI=%d"),&var)>=1) co->RecoveryPointSEI=(mfxU16)var;
                    if (_stscanf_s(line,TEXT(".mfxExtCodingOption.ViewOutput=%d"),&var)>=1) co->ViewOutput=(mfxU16)var;
                    if (_stscanf_s(line,TEXT(".mfxExtCodingOption.RefPicListReordering=%d"),&var)>=1) co->RefPicListReordering=(mfxU16)var;
                    if (_stscanf_s(line,TEXT(".mfxExtCodingOption.ResetRefList=%d"),&var)>=1) co->ResetRefList=(mfxU16)var;
                    if (_stscanf_s(line,TEXT(".mfxExtCodingOption.IntraPredBlockSize=%d"),&var)>=1) co->IntraPredBlockSize=(mfxU16)var;
                    if (_stscanf_s(line,TEXT(".mfxExtCodingOption.InterPredBlockSize=%d"),&var)>=1) co->InterPredBlockSize=(mfxU16)var;
                    if (_stscanf_s(line,TEXT(".mfxExtCodingOption.MVPrecision=%d"),&var)>=1) co->MVPrecision=(mfxU16)var;
                    if (_stscanf_s(line,TEXT(".mfxExtCodingOption.MaxDecFrameBuffering=%d"),&var)>=1) co->MaxDecFrameBuffering=(mfxU16)var;
                    if (_stscanf_s(line,TEXT(".mfxExtCodingOption.AUDelimiter=%d"),&var)>=1) co->AUDelimiter=(mfxU16)var;
                    if (_stscanf_s(line,TEXT(".mfxExtCodingOption.EndOfStream=%d"),&var)>=1) co->EndOfStream=(mfxU16)var;
                    if (_stscanf_s(line,TEXT(".mfxExtCodingOption.PicTimingSEI=%d"),&var)>=1) co->PicTimingSEI=(mfxU16)var;
                    if (_stscanf_s(line,TEXT(".mfxExtCodingOption.VuiNalHrdParameters=%d"),&var)>=1) co->VuiNalHrdParameters=(mfxU16)var;
                }
                break;
            }
        case MFX_EXTBUFF_CODING_OPTION_SPSPPS: {
                mfxExtCodingOptionSPSPPS *co=(mfxExtCodingOptionSPSPPS *)eb;
                if (_stscanf_s(line,TEXT(".mfxExtCodingOptionSPSPPS.SPSId=%d"),&var)>=1) co->SPSId=(mfxU16)var;
                if (_stscanf_s(line,TEXT(".mfxExtCodingOptionSPSPPS.PPSId=%d"),&var)>=1) co->PPSId=(mfxU16)var;
                break;
            }
        case MFX_EXTBUFF_VPP_DONOTUSE: {
                mfxExtVPPDoNotUse *hint=(mfxExtVPPDoNotUse *)eb;
                if (!_tcsncmp(line,TEXT(".mfxExtVPPDoNotUse.AlgList="),27))
                    scan_do_not_use_hint(line+27,hint,ebo);
                break;
            }
        case MFX_EXTBUFF_VPP_IMAGE_STABILIZATION: {
                mfxExtVPPImageStab *is=(mfxExtVPPImageStab *)eb;
                if (_stscanf_s(line,TEXT(".mfxExtVPPImageStab.Mode=%d"),&var)>=1) is->Mode=(mfxU16)var;
                break;
            }

        case MFX_EXTBUFF_CODING_OPTION2: {
                mfxExtCodingOption2 *co2=(mfxExtCodingOption2 *)eb;
                if (_stscanf_s(line,TEXT(".mfxExtCodingOption2.IntRefType=%d"),&var)>=1) co2->IntRefType=(mfxU16)var;
                if (_stscanf_s(line,TEXT(".mfxExtCodingOption2.IntRefCycleSize=%d"),&var)>=1) co2->IntRefCycleSize=(mfxU16)var;
                if (_stscanf_s(line,TEXT(".mfxExtCodingOption2.IntRefQPDelta=%d"),&var)>=1) co2->IntRefQPDelta=(mfxI16)var;
                if (_stscanf_s(line,TEXT(".mfxExtCodingOption2.MaxFrameSize=%d"),&var)>=1) co2->MaxFrameSize=(mfxU32)var;
                if (_stscanf_s(line,TEXT(".mfxExtCodingOption2.BitrateLimit=%d"),&var)>=1) co2->BitrateLimit=(mfxU16)var;
                if (_stscanf_s(line,TEXT(".mfxExtCodingOption2.MBBRC=%d"),&var)>=1) co2->MBBRC=(mfxU16)var;
                if (_stscanf_s(line,TEXT(".mfxExtCodingOption2.ExtBRC=%d"),&var)>=1) co2->ExtBRC=(mfxU16)var;
                break;
            }
        }
    }
}

void scan_mfxInfoVPP(TCHAR *line, mfxInfoVPP *iv) {
    if (NULL == iv)
        return;
    if (!_tcsncmp(line,TEXT(".In"),3)) scan_mfxFrameInfo(line+3, &iv->In);
    if (!_tcsncmp(line,TEXT(".Out"),4)) scan_mfxFrameInfo(line+4, &iv->Out);
}

mfxExtBuffer *find_extended_buffer(mfxVideoParam *vp,mfxU32 id,mfxU32 sz) {
    /* check if the buffer is already available, if so, copy it */
    for (int j=0;j<(int)vp->NumExtParam;j++) {
        if (vp->ExtParam[j]->BufferId!=id) continue;
        return vp->ExtParam[j];
    }
    /* If not exist, create one */
    mfxExtBuffer *buf=(mfxExtBuffer *)new mfxU8[sz];
    memset(buf,0,sz);
    buf->BufferId=id;
    buf->BufferSz=sz;
    return buf;
}

void override_extended_buffers(TCHAR *line,mfxVideoParam *vp,ExtendedBufferOverride *ebo) {

    mfxExtBuffer** ExtParam=(mfxExtBuffer **)new mfxExtBuffer*[vp->NumExtParam+3];
    int NumExtParam=0;
    for (;line[0];line++) {
        if (!_tcsncmp(line,TEXT("mfxExtCodingOption"),18))
            ExtParam[NumExtParam++]=find_extended_buffer(vp,MFX_EXTBUFF_CODING_OPTION,sizeof(mfxExtCodingOption));
        if (!_tcsncmp(line,TEXT("mfxExtCodingOptionSPSPPS"),24))
            ExtParam[NumExtParam++]=find_extended_buffer(vp,MFX_EXTBUFF_CODING_OPTION_SPSPPS,sizeof(mfxExtCodingOptionSPSPPS));
        if (!_tcsncmp(line,TEXT("mfxExtVPPDoNotUse"),17))
            ExtParam[NumExtParam++]=find_extended_buffer(vp,MFX_EXTBUFF_VPP_DONOTUSE,sizeof(mfxExtVPPDoNotUse));
        if (!_tcsncmp(line,TEXT("mfxExtVPPImageStab"),18))
            ExtParam[NumExtParam++]=find_extended_buffer(vp,MFX_EXTBUFF_VPP_IMAGE_STABILIZATION,sizeof(mfxExtVPPImageStab));
        if (!_tcsncmp(line,TEXT("mfxExtCodingOption2"),19))
            ExtParam[NumExtParam++]=find_extended_buffer(vp,MFX_EXTBUFF_CODING_OPTION2,sizeof(mfxExtCodingOption2));
        line=_tcsstr(line,TEXT(","));
        if (!line) break;
    }
    /* Add any unknown buffers */
    for (int j=0;j<vp->NumExtParam;j++) {
        if (vp->ExtParam[j]->BufferId==MFX_EXTBUFF_CODING_OPTION) continue;
        if (vp->ExtParam[j]->BufferId==MFX_EXTBUFF_CODING_OPTION_SPSPPS) continue;
        if (vp->ExtParam[j]->BufferId==MFX_EXTBUFF_VPP_DONOTUSE) continue;
        if (vp->ExtParam[j]->BufferId==MFX_EXTBUFF_VPP_IMAGE_STABILIZATION) continue;
        if (vp->ExtParam[j]->BufferId==MFX_EXTBUFF_CODING_OPTION2) continue;
        ExtParam[NumExtParam++]=vp->ExtParam[j];
    }

    ebo->ExtParam_old=vp->ExtParam;
    ebo->NumExtParam_old=vp->NumExtParam;
    vp->NumExtParam=(mfxU16)NumExtParam;
    vp->ExtParam=ExtParam;
}

void scan_mfxVideoParam(TCHAR *line, Component c, mfxVideoParam *vp, ExtendedBufferOverride *ebo) {
    if (NULL == vp || NULL == ebo)
        return;
    int var;
    if (_stscanf_s(line,TEXT(".AsyncDepth=%d"),&var)>0) vp->AsyncDepth=(mfxU16)var;
    if (_stscanf_s(line,TEXT(".Protected=%d"),&var)>0) vp->Protected=(mfxU16)var;
    if (_stscanf_s(line,TEXT(".IOPattern=%d"),&var)>0) vp->IOPattern=(mfxU16)var;

    if (c==VPP) {
        if (!_tcsncmp(line,TEXT(".vpp"),4)) scan_mfxInfoVPP(line+4, &vp->vpp);
    } else {
        if (!_tcsncmp(line,TEXT(".mfx"),4)) scan_mfxInfoMFX(line+4, c, &vp->mfx);
    }

    if (!_tcsncmp(line,TEXT(".ExtParam="),10))
        override_extended_buffers(line+10,vp,ebo);
    else if (!_tcsncmp(line,TEXT(".ExtParam"),9))
        scan_ExtBuffers(line+9,vp->mfx.CodecId,vp->ExtParam,vp->NumExtParam,ebo);
}

void scan_mfxFrameSurface1(TCHAR *line, mfxFrameSurface1 *fs) {
    if (NULL == fs)
        return;
    if (!_tcsncmp(line,TEXT(".Info"),5)) scan_mfxFrameInfo(line+5,&fs->Info);
}

void scan_mfxStatus(TCHAR *line, StatusOverride *so) {
    if (NULL == so)
        return;
    if (_tcsncmp(line,TEXT(".status="),8)) return;
    TCHAR *line2=_tcsstr(line,TEXT(" if "));
    if (line2) { *line2=0; line2++; }

#define SCANSTATUS(XX) \
        if (_tcsstr(line,TEXT(#XX))) { so->then_forced=true; so->then_status=XX; }  \
        if (line2) if (_tcsstr(line2,TEXT(#XX))) { so->if_forced=true; so->if_status=XX; }

    SCANSTATUS(MFX_ERR_NONE);
    SCANSTATUS(MFX_ERR_UNKNOWN);
    SCANSTATUS(MFX_ERR_NULL_PTR);
    SCANSTATUS(MFX_ERR_UNSUPPORTED);
    SCANSTATUS(MFX_ERR_MEMORY_ALLOC);
    SCANSTATUS(MFX_ERR_NOT_ENOUGH_BUFFER);
    SCANSTATUS(MFX_ERR_INVALID_HANDLE);
    SCANSTATUS(MFX_ERR_LOCK_MEMORY);
    SCANSTATUS(MFX_ERR_NOT_INITIALIZED);
    SCANSTATUS(MFX_ERR_NOT_FOUND);
    SCANSTATUS(MFX_ERR_MORE_DATA);
    SCANSTATUS(MFX_ERR_MORE_SURFACE);
    SCANSTATUS(MFX_ERR_ABORTED);
    SCANSTATUS(MFX_ERR_DEVICE_LOST);
    SCANSTATUS(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);
    SCANSTATUS(MFX_ERR_INVALID_VIDEO_PARAM);
    SCANSTATUS(MFX_ERR_UNDEFINED_BEHAVIOR);
    SCANSTATUS(MFX_ERR_DEVICE_FAILED);
    SCANSTATUS(MFX_WRN_IN_EXECUTION);
    SCANSTATUS(MFX_WRN_DEVICE_BUSY);
    SCANSTATUS(MFX_WRN_VIDEO_PARAM_CHANGED);
    SCANSTATUS(MFX_WRN_PARTIAL_ACCELERATION);
    SCANSTATUS(MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);
    SCANSTATUS(MFX_WRN_VALUE_NOT_CHANGED);
    SCANSTATUS(MFX_WRN_FILTER_SKIPPED);

    if (line2) *(line2-1)=TEXT(' ');
}

void scan_mfxIMPL(TCHAR *line,mfxIMPL *impl) {
    if (NULL == impl)
        return;
    if (_tcsstr(line,TEXT(".impl=AUTO"))) *impl=MFX_IMPL_AUTO;
    if (_tcsstr(line,TEXT(".impl=SOFTWARE"))) *impl=MFX_IMPL_SOFTWARE;
    if (_tcsstr(line,TEXT(".impl=HARDWARE"))) *impl=MFX_IMPL_HARDWARE;
}

void scan_mfxVersion(TCHAR *line,mfxVersion *pVer) {
    if (NULL == pVer)
        return;
    if (_tcsncmp(line,TEXT(".ver="),5)) return;
    line+=5;
    TCHAR *line2=_tcsstr(line,TEXT("."));
    if (line2) {
        int var2=0;
        _stscanf_s(line2+1,TEXT("%d"), &var2);
        pVer->Minor=(mfxU16)var2;
    }
    int var1=0;
    _stscanf_s(line,TEXT("%d"),&var1);
    pVer->Major=(mfxU16)var1;
}

void scan_mfxPriority(TCHAR *line,mfxPriority *pPty) {
    if (NULL == pPty)
        return;
    if (_tcsstr(line,TEXT(".priority=LOW"))) *pPty=MFX_PRIORITY_LOW;
    if (_tcsstr(line,TEXT(".priority=NORMAL"))) *pPty=MFX_PRIORITY_NORMAL;
    if (_tcsstr(line,TEXT(".priority=HIGH"))) *pPty=MFX_PRIORITY_HIGH;
}
