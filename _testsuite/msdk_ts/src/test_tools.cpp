/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2013-2016 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

//test_tools.cpp - non-testing blocks (memory allocation, file operations, parameters manipulation etc.)
//prefix "t_" for each block
#include "stdafx.h"
#include "test_common.h"
#include "test_trace.h"
#include <string>
#include "test_sample_allocator.h"
#include "mfx_plugin_loader.h"
#include "vm_time.h"



msdk_ts_BLOCK(t_AllocSurfPool){
    mfxFrameAllocRequest& request = var_old<mfxFrameAllocRequest>("request");
    FrameSurfPool& pool = var_new<FrameSurfPool>("surf_pool");
    std::string stype = var_def<const char*>("surface_type", "direct_pointers");
    mfxU16 w = request.Info.Width;
    mfxU16 h = request.Info.Height;

    TRACE_PAR(request);

    if(    stype == "MemId"
        || (request.Type&(MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET|MFX_MEMTYPE_VIDEO_MEMORY_PROCESSOR_TARGET))){
        mfxFrameAllocator*& frame_alloc = var_old<mfxFrameAllocator*>("p_frame_allocator");
        mfxFrameAllocResponse response = {0};
        mfxFrameSurface1 s = {0};

        s.Info = request.Info;
        s.Data.TimeStamp = MFX_TIMESTAMP_UNKNOWN;

        {
            mfxStatus AllocSts = frame_alloc->Alloc(frame_alloc->pthis, &request, &response);
            CHECK_STS(AllocSts, MFX_ERR_NONE);
        }

        for(mfxU16 i = 0; i < response.NumFrameActual; i++){
            if(response.mids)
                s.Data.MemId = response.mids[i];
            pool.push_back(s);
        }
    }else if(stype == "direct_pointers"){
        for(mfxU16 i = 0; i < request.NumFrameSuggested; i++){
            mfxFrameSurface1 s = {0};
            s.Info = request.Info;

            switch(s.Info.FourCC){
                case MFX_FOURCC_NV12:
                    s.Data.Y  = (mfxU8*)alloc(w*h*3/2);
                    s.Data.UV = s.Data.Y+w*h;
                    s.Data.Pitch = w;
                    break;
                case MFX_FOURCC_YV12:
                    s.Data.Y = (mfxU8*)alloc(w*h*3/2);
                    s.Data.U = s.Data.Y+w*h;
                    s.Data.V = s.Data.U+w*h/4;
                    s.Data.Pitch = w;
                    break;
                case MFX_FOURCC_YUY2:
                    s.Data.Y = (mfxU8*)alloc(w*h*2);
                    s.Data.U = s.Data.Y+1;
                    s.Data.V = s.Data.Y+3;
                    s.Data.Pitch = w*2;
                    break;
                case MFX_FOURCC_RGB4:
                    s.Data.B = (mfxU8*)alloc(w*h*4);
                    s.Data.G = s.Data.B+1;
                    s.Data.R = s.Data.B+2;
                    s.Data.A = s.Data.B+3;
                    s.Data.Pitch = w*4;
                    break;
                default: CHECK(0, "unsupported FourCC");
            }
            s.Data.TimeStamp = MFX_TIMESTAMP_UNKNOWN;
            pool.push_back(s);
        }
    }

    return msdk_ts::resOK;
}

msdk_ts_BLOCK(t_AllocOpaqueSurfPool){
    mfxFrameAllocRequest& request = var_old<mfxFrameAllocRequest>("request");
    mfxExtOpaqueSurfaceAlloc& osa = var_new<mfxExtOpaqueSurfaceAlloc>("opaque_pool");
    const char* type = var_old<const char*>("type");

    osa.Header.BufferId = MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION;
    osa.Header.BufferSz = sizeof(mfxExtOpaqueSurfaceAlloc);
    if (strcmp(type, "in") == 0){
        osa.In.NumSurface = request.NumFrameSuggested;
        osa.In.Surfaces = (mfxFrameSurface1**) alloc(request.NumFrameSuggested * sizeof(mfxFrameSurface1*));
        for(mfxU16 i = 0; i < request.NumFrameSuggested; i++){
            osa.In.Surfaces[i] = (mfxFrameSurface1*)alloc(sizeof(mfxFrameSurface1));
            osa.In.Surfaces[i]->Info = request.Info;
            osa.In.Surfaces[i]->Data.Pitch = request.Info.Width;
            osa.In.Surfaces[i]->Data.TimeStamp = MFX_TIMESTAMP_UNKNOWN;
        }
        osa.In.Type = MFX_MEMTYPE_FROM_ENCODE|MFX_MEMTYPE_OPAQUE_FRAME|MFX_MEMTYPE_SYSTEM_MEMORY;
        return msdk_ts::resOK;
    }
    else if (strcmp(type, "out") == 0){
        osa.Out.NumSurface = request.NumFrameSuggested;
        osa.Out.Surfaces = (mfxFrameSurface1**) alloc(request.NumFrameSuggested * sizeof(mfxFrameSurface1*));
        for(mfxU16 i = 0; i < request.NumFrameSuggested; i++){
            osa.Out.Surfaces[i] = (mfxFrameSurface1*)alloc(sizeof(mfxFrameSurface1));
            osa.Out.Surfaces[i]->Info = request.Info;
            osa.Out.Surfaces[i]->Data.Pitch = request.Info.Width;
            osa.Out.Surfaces[i]->Data.TimeStamp = MFX_TIMESTAMP_UNKNOWN;
        }
        osa.Out.Type = MFX_MEMTYPE_FROM_DECODE|MFX_MEMTYPE_OPAQUE_FRAME|MFX_MEMTYPE_SYSTEM_MEMORY;
        return msdk_ts::resOK;
    }
    else
        CHECK(0, "FAILED: bad input value for surface type parameter");
}

msdk_ts_BLOCK(t_AllocVPPSurfPools){
    msdk_ts::test_result res = msdk_ts::resFAIL;

    var_old_or_new<mfxFrameAllocRequest>("request") = var_old<mfxFrameAllocRequest*>("vpp_request")[0];
    res = t_AllocSurfPool();
    if(res) return res;
    m_var[key("vpp_surf_pool_in")] = m_var["surf_pool"];

    var_old_or_new<mfxFrameAllocRequest>("request") = var_old<mfxFrameAllocRequest*>("vpp_request")[1];
    res = t_AllocSurfPool();
    if(res) return res;
    m_var[key("vpp_surf_pool_out")] = m_var["surf_pool"];

    return msdk_ts::resOK;
}

msdk_ts_BLOCK(t_GetFreeSurf){
    FrameSurfPool& pool = var_old<FrameSurfPool>("surf_pool");
    mfxFrameSurface1*& pSurf = var_def<mfxFrameSurface1*>("p_free_surf", NULL);
    
    mfxU32 timeout = 100;
    mfxU32 step = 5;

    while(timeout)
    {
        for(FrameSurfPool::iterator it = pool.begin(); it != pool.end(); it++){
            if(!it->Data.Locked){
                pSurf = &(*it);
                m_var[key("free_surf")] = pSurf;
                return msdk_ts::resOK;
            }
        }
        timeout -= step;
        vm_time_sleep(step);
    }
    CHECK(0, "All surfaces are locked");
    return msdk_ts::resFAIL;
}

msdk_ts_BLOCK(t_GetFreeVPPInSurf){
    if(!is_defined("vpp_surf_pool_in" )) return resINVPAR;
    m_var["surf_pool"] = m_var["vpp_surf_pool_in"];
    return t_GetFreeSurf();
}

msdk_ts_BLOCK(t_GetFreeVPPOutSurf){
    msdk_ts::test_result res = msdk_ts::resFAIL;

    if(!is_defined("vpp_surf_pool_out")) return resINVPAR;

    m_var[key("surf_pool")] = m_var["vpp_surf_pool_out"];
    res = t_GetFreeSurf();
    if(res) return res;
    var_old_or_new<mfxFrameSurface1*> ("p_vpp_out_surf") = var_old<mfxFrameSurface1*>("p_free_surf");
    m_var[key("vpp_out_surf")] = m_var["free_surf"];

    return res;
}

msdk_ts_BLOCK(t_ReadRawFrame){
    mfxFrameSurface1*& s = var_old<mfxFrameSurface1*>("p_free_surf");
    VMFileHolder& f  = var_old_or_new<VMFileHolder>("file_reader");
    mfxFrameAllocator* pframe_alloc = 0;

    if(!f.is_open()){
        const char* fname = var_old<const char*>("file_name");
        CHECK(f.open(fname, VM_STRING("rb")) , "Can't open file: " << fname);
    }
    if(!f.eof()){
        CHECK(s, "zero surface");

        if(NULL != s->Data.MemId){
            pframe_alloc = var_old<mfxFrameAllocator*>("p_frame_allocator");
            pframe_alloc->Lock(pframe_alloc->pthis, s->Data.MemId, &(s->Data));
        }

        switch(s->Info.FourCC){
            case MFX_FOURCC_NV12:
            case MFX_FOURCC_YV12:
                f.read(s->Data.Y, (s->Info.Width * s->Info.Height * 3 / 2));
                break;
            case MFX_FOURCC_YUY2:
                f.read(s->Data.Y, (s->Info.Width * s->Info.Height * 2));
                break;
            case MFX_FOURCC_RGB4:
                f.read(s->Data.B, (s->Info.Width * s->Info.Height * 4));
                break;
            default: CHECK(0, "unsupported FourCC");
        }

        if(pframe_alloc){
            pframe_alloc->Unlock(pframe_alloc->pthis, s->Data.MemId, &(s->Data));
        }
    }
    if(f.eof()){ 
        s = NULL;
        var_old_or_new<bool>("eof") = true;
    }

    return msdk_ts::resOK;
}

msdk_ts_BLOCK(t_WriteRawFrame){
    mfxFrameSurface1*& s = var_old<mfxFrameSurface1*>("p_out_surf");
    VMFileHolder& f  = var_old_or_new<VMFileHolder>("file_writer");
    mfxStatus& mfxRes = var_old<mfxStatus> ("mfxRes");
    mfxFrameAllocator* pframe_alloc = 0;

    if(!f.is_open()){
        CHECK(f.open(var_old<const char*>("output_file_name"), VM_STRING("wb")) , "Can't open file");
    }
    CHECK(s, "zero surface");
    if(NULL != s->Data.MemId){
        pframe_alloc = var_old<mfxFrameAllocator*>("p_frame_allocator");
        pframe_alloc->Lock(pframe_alloc->pthis, s->Data.MemId, &(s->Data));
    }
    switch(s->Info.FourCC){
        case MFX_FOURCC_NV12:
        case MFX_FOURCC_YV12:
            f.write(s->Data.Y, (s->Info.Width * s->Info.Height * 3 / 2));
            break;
        case MFX_FOURCC_YUY2:
            f.write(s->Data.Y, (s->Info.Width * s->Info.Height * 2));
            break;
        case MFX_FOURCC_RGB4:
            f.write(s->Data.B, (s->Info.Width * s->Info.Height * 4));
            break;
        default: CHECK(0, "unsupported FourCC");
    }
    if(pframe_alloc){
        pframe_alloc->Unlock(pframe_alloc->pthis, s->Data.MemId, &(s->Data));
    }

    return msdk_ts::resOK;
}

msdk_ts_BLOCK(t_Alloc_mfxBitstream){
    mfxVideoParam& p = var_old<mfxVideoParam>("mfxvp_in");
    mfxBitstream& bs = var_new<mfxBitstream>("bitstream");
    memset(&bs, 0, sizeof(bs));
    mfxU32 size = (p.mfx.BufferSizeInKB*1000*MAX(1, p.mfx.BRCParamMultiplier));

    if(p.mfx.CodecId == MFX_CODEC_JPEG){
        size = MAX((p.mfx.FrameInfo.Width*p.mfx.FrameInfo.Height), 1000000);
    }

    bs.MaxLength = size*MAX(p.AsyncDepth, 1);
    bs.Data = (mfxU8*)alloc(bs.MaxLength);

    return msdk_ts::resOK;
}

msdk_ts_BLOCK(t_ResetBitstream){
    VMFileHolder& f  = var_old_or_new<VMFileHolder>("bs_in");
    f.close();
    return t_Read_mfxBitstream();
}

msdk_ts_BLOCK(t_Read_mfxBitstream){
    VMFileHolder& f  = var_old_or_new<VMFileHolder>("bs_in");
    mfxBitstream& bs = var_old<mfxBitstream>("bitstream");
    mfxStatus& mfxRes = var_old<mfxStatus> ("mfxRes");
    mfxU32& LengthOfReadData = var_old_or_new<mfxU32>("LengthOfReadData");
    const char* file_name = var_old<const char*>("file_name");

    if(!f.is_open()){
        std::cout << "Opening file: " << file_name << "\n";
        CHECK(f.open(file_name, VM_STRING("rb")) , "Can't open file");
        f.file_size = 1;
        f.file_attr = 1;

        f.getinfo(file_name);
        mfxU32 toRead = MIN( static_cast<mfxU32>(f.file_size), bs.MaxLength - bs.DataOffset);
        CHECK( !((mfxU32)f.read(bs.Data + bs.DataOffset, toRead) != toRead), "Failed to read file");

        LengthOfReadData = toRead;
        bs.DataLength = LengthOfReadData;
        bs.TimeStamp = -1;
        if(f.eof()){ 
            var_old_or_new<bool>("eof") = true;
            bs.DataFlag = MFX_BITSTREAM_EOS;
        }
        return msdk_ts::resOK;
    }

    if (mfxRes == MFX_ERR_MORE_DATA){
        if (bs.DataOffset == 0)
            return msdk_ts::resOK;
        if (bs.DataOffset > LengthOfReadData)
            return msdk_ts::resFAIL;

        mfxU64 pos = f.ftell();
        mfxU32 tail = LengthOfReadData - bs.DataOffset;
        memmove(bs.Data, bs.Data + bs.DataOffset, tail);
        mfxU32 toRead = MIN(static_cast<mfxU32>(f.file_size - pos), bs.MaxLength - tail);
        if ((mfxU32)f.read(bs.Data + tail, toRead) != toRead)
        {
            bs.DataLength = tail;
            f.fseek(pos, VM_FILE_SEEK_SET);
            CHECK(0, "Failed to updated bitstream data");
            return msdk_ts::resFAIL;
        }

        if(f.eof()){ 
            var_old_or_new<bool>("eof") = true;
            bs.DataFlag = MFX_BITSTREAM_EOS;
        }
        LengthOfReadData = tail + toRead;
        bs.DataLength = LengthOfReadData;
        bs.DataOffset = 0;
        bs.TimeStamp = -1;
    }
    return msdk_ts::resOK;
}

msdk_ts_BLOCK(t_SetAVCEncDefaultParam){
    mfxVideoParam& param = var_old_or_new<mfxVideoParam>("mfxvp_in");
    memset(&param, 0, sizeof(mfxVideoParam));

    std::cout << "WARNING: this block is depricated !!!\n";
    
    param.mfx.CodecId = MFX_CODEC_AVC;
    param.mfx.CodecProfile = MFX_PROFILE_AVC_HIGH;
    param.mfx.CodecLevel = MFX_LEVEL_AVC_41;
    param.mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
    param.mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
    param.mfx.FrameInfo.Width  = param.mfx.FrameInfo.CropW = 352;
    param.mfx.FrameInfo.Height = param.mfx.FrameInfo.CropH = 288;
    param.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
    param.mfx.FrameInfo.FrameRateExtN = 30;
    param.mfx.FrameInfo.FrameRateExtD = 1;
    param.mfx.FrameInfo.AspectRatioW = 1;
    param.mfx.FrameInfo.AspectRatioH = 1;
    param.mfx.RateControlMethod = MFX_RATECONTROL_CBR;
    param.mfx.TargetKbps = 5000;
    param.mfx.GopRefDist = 2;
    param.mfx.TargetUsage = MFX_TARGETUSAGE_BALANCED;
    param.IOPattern = MFX_IOPATTERN_IN_SYSTEM_MEMORY;

    return msdk_ts::resOK;
}

msdk_ts_BLOCK(t_SetVPPDefaultParam){
    mfxVideoParam& param = var_old_or_new<mfxVideoParam>("mfxvp_in");
    memset(&param, 0, sizeof(mfxVideoParam));

    std::cout << "WARNING: this block is deprecated !!!\n";

    param.IOPattern = MFX_IOPATTERN_IN_SYSTEM_MEMORY | MFX_IOPATTERN_OUT_SYSTEM_MEMORY;
    param.vpp.In.FourCC = MFX_FOURCC_YV12;
    param.vpp.Out.FourCC = MFX_FOURCC_NV12;
    param.vpp.In.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
    param.vpp.Out.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
    param.vpp.In.FrameRateExtN = 30;
    param.vpp.In.FrameRateExtD = 1;
    param.vpp.Out.FrameRateExtN = 30;
    param.vpp.Out.FrameRateExtD = 1;
    param.vpp.In.Width = 720;
    param.vpp.In.Height = 480;
    param.vpp.Out.Width = 720;
    param.vpp.Out.Height = 576;

    return msdk_ts::resOK;
}


msdk_ts_BLOCK(t_ProcessEncodeFrameAsyncStatus){
    mfxStatus& mfxRes       = var_old<mfxStatus>("mfxRes");
    mfxStatus& expectedRes  = var_def<mfxStatus>("expectedRes", MFX_ERR_NONE);
    bool& break_on_expected = var_def<bool>("break_on_expected", false);
    mfxU16& async = var_def<mfxU16>("async", 1);

    if(break_on_expected && mfxRes == expectedRes)
        return msdk_ts::loopBREAK;
    switch(mfxRes){
        case MFX_ERR_NONE: 
            if(++var_def<mfxU16>("__pefas_async_cnt", 0)%async)
                return msdk_ts::loopCONTINUE;
            return msdk_ts::resOK;
        case MFX_ERR_MORE_DATA:
            if(var_def<bool>("eof", false))
                return msdk_ts::loopBREAK;
            return msdk_ts::loopCONTINUE;
        default: CHECK(0, "Returned status is wrong: " << mfxRes);
    }

    return msdk_ts::resOK;
}

msdk_ts_BLOCK(t_ProcessRunFrameVPPAsyncStatus){
    mfxStatus& mfxRes       = var_old<mfxStatus>("mfxRes");
    mfxStatus& expectedRes  = var_def<mfxStatus>("expectedRes", MFX_ERR_NONE);
    bool& break_on_expected = var_def<bool>("break_on_expected", false);
    mfxU16& async = var_def<mfxU16>("async", 1);

    if(break_on_expected && mfxRes == expectedRes)
        return msdk_ts::loopBREAK;

    switch(mfxRes){
        case MFX_ERR_NONE: 
        case MFX_ERR_MORE_SURFACE: 
            if(++var_def<mfxU16>("__prfvas_async_cnt", 0)%async){
                if(t_GetFreeVPPOutSurf()) return msdk_ts::resFAIL;
                return msdk_ts::loopCONTINUE;
            }
            return msdk_ts::resOK;
        case MFX_ERR_MORE_DATA:
            if(var_def<bool>("eof", false))
                return msdk_ts::loopBREAK;
            if(t_GetFreeVPPInSurf())  return msdk_ts::resFAIL;
            if(t_ReadRawFrame())      return msdk_ts::resFAIL;
            return msdk_ts::loopCONTINUE;
        default: CHECK(0, "Returned status is wrong: " << mfxRes);
    }

    return msdk_ts::resOK;
}

msdk_ts_BLOCK(t_ProcessDecodeFrameAsyncStatus){
    mfxStatus& mfxRes       = var_old<mfxStatus>("mfxRes");
    mfxStatus& expectedRes  = var_def<mfxStatus>("expectedRes", MFX_ERR_NONE);
    bool& break_on_expected = var_def<bool>("break_on_expected", false);
    mfxU16& async = var_def<mfxU16>("async", 1);

    if(break_on_expected && mfxRes == expectedRes)
        return msdk_ts::loopBREAK;
    switch(mfxRes){
        case MFX_ERR_NONE: 
            if(++var_def<mfxU16>("__pdfas_async_cnt", 0)%async)
                return msdk_ts::loopCONTINUE;
            return msdk_ts::resOK;
        case MFX_ERR_MORE_DATA:
            if(var_def<bool>("eof", false))
                return msdk_ts::loopBREAK;
            return msdk_ts::loopCONTINUE;
        case MFX_ERR_MORE_SURFACE:
            if(var_def<bool>("eof", false))
                return msdk_ts::loopBREAK;
            return msdk_ts::loopCONTINUE;
        case MFX_WRN_VIDEO_PARAM_CHANGED:
            if(var_def<bool>("eof", false))
                return msdk_ts::loopBREAK;
            return msdk_ts::loopCONTINUE;
        case MFX_WRN_DEVICE_BUSY:
            if(var_def<bool>("eof", false))
                return msdk_ts::loopBREAK;
            return msdk_ts::loopCONTINUE;
        default: CHECK(0, "Returned status is wrong.\n Returned status is: " << mfxRes 
                       << ". Expected MFX_ERR_NONE, MFX_ERR_MORE_DATA, MFX_ERR_MORE_SURFACE, MFX_WRN_DEVICE_BUSY, or MFX_WRN_VIDEO_PARAM_CHANGED in this block");
    }

    return msdk_ts::resOK;
}

msdk_ts_BLOCK(t_inc){
    switch(var_def<mfxU8>("inc_size", 4)){
        case 1: var_old<mfxU8 >("inc_x")++; break;
        case 2: var_old<mfxU16>("inc_x")++; break;
        case 4: var_old<mfxU32>("inc_x")++; break;
        case 8: var_old<mfxU64>("inc_x")++; break;
    }
    return msdk_ts::resOK;
}

#define DECL_OPERATION_BLOCK(name, op)\
    msdk_ts_BLOCK(t_##name){\
        switch(var_def<mfxU8>("size", 4)){\
            case 1: var_old<mfxU8 >("x") op##= var_old<mfxU8 >("y"); break;\
            case 2: var_old<mfxU16>("x") op##= var_old<mfxU16>("y"); break;\
            case 4: var_old<mfxU32>("x") op##= var_old<mfxU32>("y"); break;\
            case 8: var_old<mfxU64>("x") op##= var_old<mfxU64>("y"); break;\
        }\
        return msdk_ts::resOK;\
    }

DECL_OPERATION_BLOCK(sum, +);
DECL_OPERATION_BLOCK(sub, -);
DECL_OPERATION_BLOCK(mul, *);
DECL_OPERATION_BLOCK(div, /);
DECL_OPERATION_BLOCK(mod, %);
DECL_OPERATION_BLOCK(and, &);
DECL_OPERATION_BLOCK(or , |);
DECL_OPERATION_BLOCK(xor, ^);
DECL_OPERATION_BLOCK(shl, <<);
DECL_OPERATION_BLOCK(shr, >>);
DECL_OPERATION_BLOCK(set, );

msdk_ts_BLOCK(t_Wipe_mfxBitstream){
    mfxBitstream& bs = var_old<mfxBitstream>("bitstream");
    bs.DataLength = 0;
    bs.DataOffset = 0;
    return msdk_ts::resOK;
}

msdk_ts_BLOCK(t_Dump_mfxBitstream){
    mfxBitstream& bs = var_old<mfxBitstream>("bitstream");
    VMFileHolder& f  = var_old_or_new<VMFileHolder>("dump_file");

    if(!f.is_open()){
        CHECK(f.open(var_old<const char*>("dump_file_name"), VM_STRING("wb")) , "Can't open file");
    }
    f.write(bs.Data+bs.DataOffset, bs.DataLength);

    return msdk_ts::resOK;
}

#define EB_ALLOC_STEP 10
msdk_ts_BLOCK(t_AttachBuffers){
    mfxVideoParam& p = var_old<mfxVideoParam>("mfxvp");
    std::string bufs = var_old<char*>("ext_bufs_str");
    char str[256] = {0};
    size_t n_alloc = 0;

    if(!p.ExtParam)p.ExtParam = alloc_array<mfxExtBuffer*>(EB_ALLOC_STEP);
    n_alloc = m_mem[p.ExtParam]->size();

    while(!bufs.empty()){
        std::string::size_type size = bufs.copy(str, bufs.find(","));
        str[size] = '\0';
        bufs.erase(0, size+1);
        if(n_alloc < (size_t)p.NumExtParam+1){
            mfxExtBuffer** tmp = p.ExtParam;
            p.ExtParam = alloc_array<mfxExtBuffer*>(n_alloc += EB_ALLOC_STEP);
            memcpy(p.ExtParam, tmp, p.NumExtParam*sizeof(mfxExtBuffer*));
        }
        p.ExtParam[p.NumExtParam++] = &var_old<mfxExtBuffer>(str);
    }

    return msdk_ts::resOK;
}

msdk_ts_BLOCK(t_ParseH264AU){
    BS_H264_parser& p = var_old_or_new<BS_H264_parser>("h264_parser");
    mfxBitstream& bs = var_old<mfxBitstream>("bitstream");
    BS_H264_au*& pAU = var_def<BS_H264_au*>("p_h264_au_hdr", NULL);
    BSErr sts = BS_ERR_NONE;

    sts = p.set_buffer(bs.Data+bs.DataOffset, bs.DataLength);
    CHECK_STS(sts, BS_ERR_NONE);

    sts = p.parse_next_au(pAU);
    CHECK_STS(sts, BS_ERR_NONE);

    bs.DataOffset += (Bs32u)p.get_offset();
    if(pAU) m_var["h264_au_hdr"] = pAU;

    return msdk_ts::resOK;
}


msdk_ts_BLOCK(t_ParseHEVCAU){
    BS_HEVC_parser& p = var_old_or_new<BS_HEVC_parser>("hevc_parser");
    mfxBitstream& bs = var_old<mfxBitstream>("bitstream");
    BS_HEVC::AU*& pAU = var_def<BS_HEVC::AU*>("p_hevcau_hdr", NULL);
    BSErr sts = BS_ERR_NONE;

    sts = p.set_buffer(bs.Data+bs.DataOffset, bs.DataLength);
    CHECK_STS(sts, BS_ERR_NONE);

    sts = p.parse_next_au(pAU);
    CHECK_STS(sts, BS_ERR_NONE);

    bs.DataOffset += (Bs32u)p.get_offset();
    if(pAU) m_var["hevc_au_hdr"] = pAU;

    return msdk_ts::resOK;
}

msdk_ts_BLOCK(t_NewBsParser){
    Bs32u& id   = var_old<mfxU32>("parser_id");
    Bs32u& mode = var_def<mfxU32>("parser_mode", 0);
    BS_parser* parser = NULL;
    char* f = NULL;

    switch(id){
    case MFX_CODEC_AVC: 
        parser = &var_new<BS_H264_parser>("bs_parser", new BS_H264_parser(mode));
        break;
    case MFX_CODEC_MPEG2: 
        parser = &var_new<BS_MPEG2_parser>("bs_parser", new BS_MPEG2_parser(mode));
        break;
    case MFX_CODEC_VC1: 
        parser = &var_new<BS_VC1_parser>("bs_parser");
        break;
    case MFX_CODEC_VP8: 
        parser = &var_new<BS_VP8_parser>("bs_parser");
        break;
    case MFX_CODEC_HEVC: 
        parser = &var_new<BS_HEVC_parser>("bs_parser", new BS_HEVC_parser(mode));
        break;
    case MFX_CODEC_JPEG: 
        parser = &var_new<BS_JPEG_parser>("bs_parser");
        break;
    default: 
        RETERR(resFAIL, "Unknown parser_id: " << id);
    }

    var_old_or_new<BS_parser*>("p_bs_parser") = parser;

    if((f = var_def<char*>("bs_file", NULL))){
        if(parser->open(f)){
            RETERR(resFAIL, "can't open file '" << f << "'");
        }
    }
    parser->set_trace_level(var_def<Bs32u>("bs_trace_level", -1));

    return msdk_ts::resOK;
}

msdk_ts_BLOCK(t_ParseNextUnit){
    BS_parser* parser = var_old<BS_parser*>("p_bs_parser");
    void*& hdr = var_def<void*>("bs_header", NULL);
    mfxBitstream* pBS = NULL;
    BSErr sts = BS_ERR_NONE;

    if(!parser)
        return msdk_ts::resFAIL;

    if(!var_def<char*>("bs_file", NULL)){
        pBS = &var_old<mfxBitstream>("bitstream");

        sts = parser->set_buffer(pBS->Data+pBS->DataOffset, pBS->DataLength);
        CHECK_STS(sts, BS_ERR_NONE);
    }

    sts = parser->parse_next_unit();
    if(sts == BS_ERR_MORE_DATA){
        hdr = NULL;
        return msdk_ts::resOK;
    }
    CHECK_STS(sts, BS_ERR_NONE);

    if(pBS){
        pBS->DataOffset += (Bs32u)parser->get_offset();
    }

    hdr = parser->get_header();

    return msdk_ts::resOK;
}

msdk_ts_BLOCK(t_BsLock){
    BS_parser* parser = var_old<BS_parser*>("p_bs_parser");
    void*& hdr = var_def<void*>("bs_header", NULL);

    if(parser && !parser->lock(hdr))
        return msdk_ts::resOK;

    return msdk_ts::resFAIL;
}

msdk_ts_BLOCK(t_BsUnlock){
    BS_parser* parser = var_old<BS_parser*>("p_bs_parser");
    void*& hdr = var_def<void*>("bs_header", NULL);

    if(parser && !parser->unlock(hdr))
        return msdk_ts::resOK;

    return msdk_ts::resFAIL;
}

msdk_ts_BLOCK(t_PackHEVCNALU){
    mfxBitstream& bs = var_old<mfxBitstream>("bitstream");
    BS_HEVC::NALU*& p = var_def<BS_HEVC::NALU*>("p_hevc_nalu_hdr", NULL);
    BSErr sts = BS_ERR_NONE;

    bs.DataLength = bs.MaxLength - bs.DataOffset;

    sts = BS_HEVC_Pack(bs.Data + bs.DataOffset, bs.DataLength, p);
    CHECK_STS(sts, BS_ERR_NONE);
    
    return msdk_ts::resOK;
}


msdk_ts_BLOCK(t_ParseMPEG2){
    BS_MPEG2_parser& p = var_old_or_new<BS_MPEG2_parser>("mpeg2_parser");
    mfxBitstream& bs = var_old<mfxBitstream>("bitstream");
    BSErr& sts = var_def<BSErr>("bs_sts", BS_ERR_NONE);

    sts = p.set_buffer(bs.Data+bs.DataOffset, bs.DataLength);
    CHECK_STS(sts, BS_ERR_NONE);

    sts = p.parse_next_unit();

    switch(sts){
        case BS_ERR_NONE:
        case BS_ERR_NOT_IMPLEMENTED:
            bs.DataOffset += (Bs32u)p.get_offset();
            m_var["mpeg2_hdr"] = p.get_header();
        case BS_ERR_MORE_DATA:
            return msdk_ts::resOK;
        default: break;
    }
    return msdk_ts::resFAIL;
}

msdk_ts_BLOCK(t_DefineAllocator){
    std::string platform = var_def<const char*>("platform", "");
    bool        is_d3d11 = false;

    if(platform.find("d3d11") != platform.npos)
        is_d3d11 = true;

    frame_allocator & alloc = var_new<frame_allocator>(
        "global_allocator_instance", 
        new frame_allocator(
            getAllocatorType(var_def<const char*> ("alloc_type", (const char*)(is_d3d11 ? "d3d11" : "hw"))),
            getAllocMode    (var_def<const char*> ("alloc_mode", "max")),
            getLockMode     (var_def<const char*> ("lock_mode" , "valid")),
            getOpaqueMode   (var_def<const char*> ("opaque_alloc_mode", "error"))
        )
    );

    if(!alloc.valid())
        return msdk_ts::resFAIL;

    mfxFrameAllocator& redirector = var_new<mfxFrameAllocator>("frame_allocator");
    mfxFrameAllocator*& frame_alloc = var_new<mfxFrameAllocator*>("p_frame_allocator");
    frame_alloc = &redirector;

    memset(redirector.reserved, 0, sizeof(redirector.reserved));
    redirector.pthis  = &alloc;
    redirector.Alloc  = &frame_allocator::AllocFrame;
    redirector.Lock   = &frame_allocator::LockFrame;
    redirector.Unlock = &frame_allocator::UnLockFrame;
    redirector.GetHDL = &frame_allocator::GetHDL;
    redirector.Free   = &frame_allocator::Free;

    return msdk_ts::resOK;
}

msdk_ts_BLOCK(t_AllocGetDevHdl){
    mfxHandleType& type = var_old_or_new<mfxHandleType>("hdl_type");
    mfxHDL&         hdl = var_old_or_new<mfxHDL>       ("hdl");
    frame_allocator& allocator = var_old<frame_allocator>("global_allocator_instance");
    
    if(!allocator.get_hdl(type, hdl))
        return msdk_ts::resFAIL;
    
    return msdk_ts::resOK;
}

msdk_ts_BLOCK(t_CntAllocatedSurf){
    var_old_or_new<mfxU32>("surf_cnt") = var_old<frame_allocator>("global_allocator_instance").cnt_surf();
    return msdk_ts::resOK;
}

msdk_ts_BLOCK(t_ResetAllocator){
    var_old<frame_allocator>("global_allocator_instance").reset(
            getAllocMode  (var_old<char*> ("alloc_mode")),
            getLockMode   (var_old<char*> ("lock_mode")),
            getOpaqueMode (var_old<char*> ("opaque_alloc_mode"))
        );
    return msdk_ts::resOK;
}

const unsigned long CRC32::table[256] = {
  0x00000000, 0x77073096, 0xEE0E612C, 0x990951BA, 0x076DC419, 0x706AF48F, 0xE963A535, 0x9E6495A3,
  0x0EDB8832, 0x79DCB8A4, 0xE0D5E91E, 0x97D2D988, 0x09B64C2B, 0x7EB17CBD, 0xE7B82D07, 0x90BF1D91,
  0x1DB71064, 0x6AB020F2, 0xF3B97148, 0x84BE41DE, 0x1ADAD47D, 0x6DDDE4EB, 0xF4D4B551, 0x83D385C7,
  0x136C9856, 0x646BA8C0, 0xFD62F97A, 0x8A65C9EC, 0x14015C4F, 0x63066CD9, 0xFA0F3D63, 0x8D080DF5,
  0x3B6E20C8, 0x4C69105E, 0xD56041E4, 0xA2677172, 0x3C03E4D1, 0x4B04D447, 0xD20D85FD, 0xA50AB56B,
  0x35B5A8FA, 0x42B2986C, 0xDBBBC9D6, 0xACBCF940, 0x32D86CE3, 0x45DF5C75, 0xDCD60DCF, 0xABD13D59,
  0x26D930AC, 0x51DE003A, 0xC8D75180, 0xBFD06116, 0x21B4F4B5, 0x56B3C423, 0xCFBA9599, 0xB8BDA50F,
  0x2802B89E, 0x5F058808, 0xC60CD9B2, 0xB10BE924, 0x2F6F7C87, 0x58684C11, 0xC1611DAB, 0xB6662D3D,
  0x76DC4190, 0x01DB7106, 0x98D220BC, 0xEFD5102A, 0x71B18589, 0x06B6B51F, 0x9FBFE4A5, 0xE8B8D433,
  0x7807C9A2, 0x0F00F934, 0x9609A88E, 0xE10E9818, 0x7F6A0DBB, 0x086D3D2D, 0x91646C97, 0xE6635C01,
  0x6B6B51F4, 0x1C6C6162, 0x856530D8, 0xF262004E, 0x6C0695ED, 0x1B01A57B, 0x8208F4C1, 0xF50FC457,
  0x65B0D9C6, 0x12B7E950, 0x8BBEB8EA, 0xFCB9887C, 0x62DD1DDF, 0x15DA2D49, 0x8CD37CF3, 0xFBD44C65,
  0x4DB26158, 0x3AB551CE, 0xA3BC0074, 0xD4BB30E2, 0x4ADFA541, 0x3DD895D7, 0xA4D1C46D, 0xD3D6F4FB,
  0x4369E96A, 0x346ED9FC, 0xAD678846, 0xDA60B8D0, 0x44042D73, 0x33031DE5, 0xAA0A4C5F, 0xDD0D7CC9,
  0x5005713C, 0x270241AA, 0xBE0B1010, 0xC90C2086, 0x5768B525, 0x206F85B3, 0xB966D409, 0xCE61E49F,
  0x5EDEF90E, 0x29D9C998, 0xB0D09822, 0xC7D7A8B4, 0x59B33D17, 0x2EB40D81, 0xB7BD5C3B, 0xC0BA6CAD,
  0xEDB88320, 0x9ABFB3B6, 0x03B6E20C, 0x74B1D29A, 0xEAD54739, 0x9DD277AF, 0x04DB2615, 0x73DC1683,
  0xE3630B12, 0x94643B84, 0x0D6D6A3E, 0x7A6A5AA8, 0xE40ECF0B, 0x9309FF9D, 0x0A00AE27, 0x7D079EB1,
  0xF00F9344, 0x8708A3D2, 0x1E01F268, 0x6906C2FE, 0xF762575D, 0x806567CB, 0x196C3671, 0x6E6B06E7,
  0xFED41B76, 0x89D32BE0, 0x10DA7A5A, 0x67DD4ACC, 0xF9B9DF6F, 0x8EBEEFF9, 0x17B7BE43, 0x60B08ED5,
  0xD6D6A3E8, 0xA1D1937E, 0x38D8C2C4, 0x4FDFF252, 0xD1BB67F1, 0xA6BC5767, 0x3FB506DD, 0x48B2364B,
  0xD80D2BDA, 0xAF0A1B4C, 0x36034AF6, 0x41047A60, 0xDF60EFC3, 0xA867DF55, 0x316E8EEF, 0x4669BE79,
  0xCB61B38C, 0xBC66831A, 0x256FD2A0, 0x5268E236, 0xCC0C7795, 0xBB0B4703, 0x220216B9, 0x5505262F,
  0xC5BA3BBE, 0xB2BD0B28, 0x2BB45A92, 0x5CB36A04, 0xC2D7FFA7, 0xB5D0CF31, 0x2CD99E8B, 0x5BDEAE1D,
  0x9B64C2B0, 0xEC63F226, 0x756AA39C, 0x026D930A, 0x9C0906A9, 0xEB0E363F, 0x72076785, 0x05005713,
  0x95BF4A82, 0xE2B87A14, 0x7BB12BAE, 0x0CB61B38, 0x92D28E9B, 0xE5D5BE0D, 0x7CDCEFB7, 0x0BDBDF21,
  0x86D3D2D4, 0xF1D4E242, 0x68DDB3F8, 0x1FDA836E, 0x81BE16CD, 0xF6B9265B, 0x6FB077E1, 0x18B74777,
  0x88085AE6, 0xFF0F6A70, 0x66063BCA, 0x11010B5C, 0x8F659EFF, 0xF862AE69, 0x616BFFD3, 0x166CCF45,
  0xA00AE278, 0xD70DD2EE, 0x4E048354, 0x3903B3C2, 0xA7672661, 0xD06016F7, 0x4969474D, 0x3E6E77DB,
  0xAED16A4A, 0xD9D65ADC, 0x40DF0B66, 0x37D83BF0, 0xA9BCAE53, 0xDEBB9EC5, 0x47B2CF7F, 0x30B5FFE9,
  0xBDBDF21C, 0xCABAC28A, 0x53B39330, 0x24B4A3A6, 0xBAD03605, 0xCDD70693, 0x54DE5729, 0x23D967BF,
  0xB3667A2E, 0xC4614AB8, 0x5D681B02, 0x2A6F2B94, 0xB40BBE37, 0xC30C8EA1, 0x5A05DF1B, 0x2D02EF8D
};

void CRC32::update(unsigned char *buf, int len) {
    for (int i = 0; i < len; i++)
        value = table[(value ^ buf[i]) & 0xFF] ^ (value >> 8);
    value ^= 0xFFFFFFFFL;
};

#define MAX_HEAPS       64
#define HEAP_STATE_SIZE  8

struct heap_state{
    mfxU32 crc; 
    mfxU32 nHeaps;
    mfxU64 size;
};
static heap_state heap_state_default[HEAP_STATE_SIZE];

msdk_ts_BLOCK(t_GetHeapState){
    heap_state* state = var_def<heap_state*>("heap_state", heap_state_default);
    mfxU32& idx = var_def<mfxU32>("heap_state_idx", -1);
    CRC32   crc;
    mfxU64 sz = 0;

    if(++idx >= var_def<mfxU32>("heap_state_size", HEAP_STATE_SIZE))
        idx = 0;

#if defined(_WIN32) || defined(_WIN64)
    PROCESS_HEAP_ENTRY Entry = {0};
    DWORD              nHeaps = 0;
    HANDLE             hdl[MAX_HEAPS];
    HANDLE             hHeap = NULL;

    nHeaps = GetProcessHeaps(0, NULL);
    if(!nHeaps || nHeaps > MAX_HEAPS)
        return msdk_ts::resFAIL;
    
    nHeaps = GetProcessHeaps((DWORD)MAX_HEAPS, hdl);
    if(nHeaps > MAX_HEAPS || !nHeaps)
        return msdk_ts::resFAIL;

    for(DWORD i = 0; i < nHeaps; i++){
        hHeap = hdl[i];

        if(!HeapLock(hHeap))
            return msdk_ts::resFAIL;

        Entry.lpData = NULL;
        while(HeapWalk(hHeap, &Entry) != FALSE){

            if((Entry.wFlags & PROCESS_HEAP_ENTRY_BUSY) == 0)
                continue;
            
            sz += Entry.cbData;
            crc.update((unsigned char*)&Entry, sizeof(Entry));
        }

        if (GetLastError() != ERROR_NO_MORE_ITEMS)
            return msdk_ts::resFAIL;
        
        if (!HeapUnlock(hHeap))
            return msdk_ts::resFAIL;
    }

    state[idx].crc     = crc.get();
    state[idx].nHeaps  = nHeaps;
    state[idx].size    = sz;

#else
    std::cout << "Implemented for Windows only" << std::endl;
    return msdk_ts::resFAIL;
#endif
    
    return msdk_ts::resOK;
}


msdk_ts_BLOCK(t_DefineBufferAllocator){
    var_new<mfxBufferAllocator*>("p_buf_allocator") = &(var_new<mfxBufferAllocator>("buf_allocator") = var_new<buffer_allocator>("global_buffer_allocator_instance"));

    return msdk_ts::resOK;
}

msdk_ts_BLOCK(t_GetAllocatedBuffers){
    var_old_or_new<buffer_allocator::buffer*>("buffers") = var_old<buffer_allocator>("global_buffer_allocator_instance").GetBuffers().data();
    var_old_or_new<mfxU32>("num_buffers") = (mfxU32)var_old<buffer_allocator>("global_buffer_allocator_instance").GetBuffers().size();

    return msdk_ts::resOK;
}

#if (defined(LINUX32) || defined(LINUX64))
#if defined(LIBVA_SUPPORT)
msdk_ts_BLOCK(t_GetVA){
    if (!is_defined("va_holder"))
    {
        var_new<CLibVA>("va_holder", CreateLibVA());
    }
    CLibVA& clib_va = var_old<CLibVA>("va_holder");


	mfxHDL& va_display = var_old_or_new<mfxHDL>("va_display");

    va_display = clib_va.GetVADisplay();
    CHECK(va_display, "Failed to Initialize libVA");

    return msdk_ts::resOK;
}
#endif //defined(LIBVA_SUPPORT)
#endif //(defined(LINUX32) || defined(LINUX64))
#ifdef __MFXPLUGIN_H__
msdk_ts_BLOCK(t_LoadDLLPlugin){
    mfxU32&     type     = var_def<mfxU32>     ("type", MFX_PLUGINTYPE_VIDEO_DECODE);
    char*       dll_name = var_old<char*>      ("dll_name");
    mfxPlugin*& pPlugin  = var_def<mfxPlugin*> ("p_plugin", NULL);
    mfxPluginUID*& PluginUid = var_def<mfxPluginUID*> ("p_plugin_uid", NULL);

    vm_char* file_name = 0;
    vm_char* temp_fn = 0;
    std::cout << "  Library to load (linking explicitly without dispatcher): " << dll_name << std::endl;

    if(sizeof(char) == sizeof(vm_char))
        file_name = reinterpret_cast<vm_char*>( dll_name );
    else //char to TCHAR conversion
    {
        size_t i;
        temp_fn = new vm_char[strlen(dll_name)+1];
        for(i = 0; dll_name[i]; i++)
            temp_fn[i] = dll_name[i];
        temp_fn[i] = 0;
        file_name = temp_fn;
    }

    if(MFX_PLUGINTYPE_VIDEO_GENERAL == type)
    {
        PluginLoader<MFXGenericPlugin>& Loader = var_new<PluginLoader<MFXGenericPlugin> >("plugin_loader", new PluginLoader<MFXGenericPlugin>(file_name) );
        pPlugin = Loader.plugin;
    } 
    else if ((MFX_PLUGINTYPE_VIDEO_DECODE == type) || (MFX_PLUGINTYPE_VIDEO_ENCODE == type) || (MFX_PLUGINTYPE_VIDEO_VPP == type))
    {
        PluginLoader<MFXCodecPlugin>& Loader = var_new<PluginLoader<MFXCodecPlugin> >("plugin_loader", new PluginLoader<MFXCodecPlugin>(file_name, *PluginUid) );
        pPlugin = Loader.plugin;
    } 
    else
    {
        std::cout << "Wrong plugin type" << std::endl;
        return msdk_ts::resFAIL;
    }
    if(temp_fn)
        delete[] temp_fn;
    if(NULL == pPlugin)
    {
        std::cout << "Failed to load dll" << std::endl;
        return msdk_ts::resFAIL;
    }
    return msdk_ts::resOK;
}
#endif //#ifdef __MFXPLUGIN_H__