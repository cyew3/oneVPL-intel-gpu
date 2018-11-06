/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2009-2014 Intel Corporation. All Rights Reserved.

File Name: core.cpp

\* ****************************************************************************** */
#include "configuration.h"
#include "msdka_import.h"
#include "msdka_serial.h"
#include "etw_event.h"

StatusMap sm_encode_sync, sm_decode_sync, sm_vpp_sync;

mfxStatus MFXVideoCORE_SetBufferAllocator(mfxSession session, mfxBufferAllocator *allocator) {
    StackIncreaser si;
    RECORD_CONFIGURATION({
        RECORD_POINTERS(dump_format_wprefix(fd, level, 1,TEXT("core.SetBufferAllocator.allocator=0x"),TEXT("%p"), allocator));
    });
    AnalyzerBufferAllocator *aba=new AnalyzerBufferAllocator(allocator);
    return MFX_CALL(MFXVideoCORE_SetBufferAllocator,(((AnalyzerSession *)session)->session, (mfxBufferAllocator *)aba));
}

mfxStatus MFXVideoCORE_SetFrameAllocator(mfxSession session, mfxFrameAllocator *allocator) {
    StackIncreaser si;
    RECORD_CONFIGURATION({
        RECORD_POINTERS(dump_format_wprefix(fd, level, 1,TEXT("core.SetFrameAllocator.allocator=0x"),TEXT("%p"), allocator));
    });
    AnalyzerFrameAllocator *afa=new AnalyzerFrameAllocator(allocator);
    mfxStatus sts = MFX_CALL(MFXVideoCORE_SetFrameAllocator,(((AnalyzerSession *)session)->session, (mfxFrameAllocator *)afa));
    ((AnalyzerSession *)session)->pAlloc = afa;
    RECORD_CONFIGURATION({
        dump_mfxStatus(fd, level, TEXT("core.SetFrameAllocator"),sts);
    });

    return sts;

}

mfxStatus MFXVideoCORE_SetHandle(mfxSession session, mfxHandleType type, mfxHDL hdl) {
    StackIncreaser si;
    RECORD_CONFIGURATION({
        RECORD_POINTERS(dump_format_wprefix(fd, level, 1,TEXT("core.SetHandle.type="),TEXT("%d:0x%p"), type, hdl));
    });
    mfxStatus sts = MFX_CALL(MFXVideoCORE_SetHandle,(((AnalyzerSession *)session)->session, type, hdl));

    RECORD_CONFIGURATION({
        dump_mfxStatus(fd, level, TEXT("core.SetHandle"),sts);
    });
    return sts;
}

mfxStatus MFXVideoCORE_GetHandle(mfxSession session, mfxHandleType type, mfxHDL *hdl) {
    StackIncreaser si;
    mfxStatus sts=MFX_CALL(MFXVideoCORE_GetHandle,(((AnalyzerSession *)session)->session, type, hdl));
    RECORD_CONFIGURATION({
        RECORD_POINTERS(dump_format_wprefix(fd, level, 1,TEXT("core.GetHandle.type="),TEXT("%d:0x%p"), type, *hdl));
    });
    return sts;
}

mfxStatus MFXVideoCORE_SyncOperation(mfxSession session, mfxSyncPoint syncp, mfxU32 wait) {
    StackIncreaser si;
    AnalyzerSession *as=(AnalyzerSession *)session;
    AnalyzerSyncPoint *sp=(AnalyzerSyncPoint *)syncp;
    if (!as || !sp || !as->session) return MFX_ERR_NULL_PTR;
    if (!sp->sync_point) {
        // already synced
        return sp->sts;
    }
    
    MPA_TRACE((sp->c == ENCODE ? TRACER_TASK_PREFIX"SyncOperation(ENCODE)" : (sp->c == DECODE? TRACER_TASK_PREFIX"SyncOperation(DECODE)": TRACER_TASK_PREFIX"SyncOperation(VPP)")));

    if (!sp->copy_data_from_stub){
        if ((gc.decode.skip_sync_operation && sp->c==DECODE) || 
            (gc.vpp.skip_sync_operation && sp->c==VPP)) {
            sp->sts=MFX_CALL(MFXVideoCORE_SyncOperation,(as->session, sp->sync_point, 0));
            if (sp->sts==MFX_WRN_IN_EXECUTION) sp->sts=MFX_ERR_NONE;
        } else {
            sp->sts=MFX_CALL(MFXVideoCORE_SyncOperation,(as->session, sp->sync_point, wait));
        }
    } else {
        if (sp->c == ENCODE){ 
            if (sp->output.bitstream){
                

                mfxBitstream & ref_bs = as->encode.first_frame;
                if (sp->output.bitstream->MaxLength < ref_bs.DataLength){
                    RECORD_CONFIGURATION_PER_FRAME({
                        dump_format_wprefix(fd, level, 1,TEXT("SyncOperation() ERROR: Cannot Copy from stub buffer to target buffer , processing aborted"),TEXT(""));
                    });
                    sp->sts = MFX_ERR_NOT_ENOUGH_BUFFER;
                } else {
                    //work in non buffered mode -> input frame always present
                    memcpy_s(sp->output.bitstream->Data, sp->output.bitstream->DataLength, ref_bs.Data, ref_bs.DataLength);
                    sp->output.bitstream->DataLength = ref_bs.DataLength;
                    sp->output.bitstream->TimeStamp = sp->input.frame->Data.TimeStamp;
                    sp->sts = MFX_ERR_NONE;
                }
            }
        } else {
            RECORD_CONFIGURATION_PER_FRAME({
                dump_format_wprefix(fd, level, 1,TEXT("SyncOperation() ERROR: Unsupported copy from stub for component = %s"), TEXT("%s"), sp->c == DECODE ? "DECODE" : "VPP");
            });
                
        }
    }

    if (gc.debug_view) {
        switch (sp->c) {
        case DECODE: OutputDebugString(TEXT("SDK: SyncOperation(D)")); break;
        case ENCODE: OutputDebugString(TEXT("SDK: SyncOperation(E)")); break;
        case VPP:    OutputDebugString(TEXT("SDK: SyncOperation(V)")); break;
        }
    }

    switch (sp->c) 
    {
        case DECODE: 
        {
            sm_decode_sync.Count(sp->sts); 
            gc.RegisterSurface(sp->c, SyncOperation_Output, sp->output.psurface_out);
            break;
        }
        case ENCODE: 
        {
            sm_encode_sync.Count(sp->sts); 
            gc.RegisterSurface(sp->c, SyncOperation_Input, sp->input.frame);
            break;
        }
        case VPP: 
        {
            sm_vpp_sync.Count(sp->sts); 
            gc.RegisterSurface(sp->c, SyncOperation_Input, sp->input.frame);
            gc.RegisterSurface(sp->c, SyncOperation_Output, sp->output.out);
            break;
        }
    }
    

    if (sp->sts!=MFX_WRN_IN_EXECUTION) {
        RECORD_CONFIGURATION_PER_FRAME({
            switch (sp->c) {
            case DECODE:
                RECORD_POINTERS(dump_format_wprefix(fd, level, 1,TEXT("decode.SyncOperation.syncp=0x"),TEXT("%p"), sp->sync_point));
                if (sp->output.psurface_out) {
                    RECORD_POINTERS(dump_format_wprefix(fd, level, 1,TEXT("decode.SyncOperation.surface_out=0x"),TEXT("%p"),sp->output.psurface_out));
                } else {
                    dump_format_wprefix(fd, level, 1,TEXT("decode.SyncOperation.surface_out=NULL"),TEXT(""));
                }
                if (wait == INFINITE)
                {
                    dump_format_wprefix(fd, level, 1,TEXT("decode.SyncOperation.wait=INFINITE"),TEXT(""));
                }else
                {
                    dump_format_wprefix(fd, level, 1,TEXT("decode.SyncOperation.wait="),TEXT("%u"),wait);
                }
                dump_mfxStatus(fd, level, TEXT("decode.SyncOperation"),sp->sts);
                break;
            case ENCODE:
                RECORD_POINTERS(dump_format_wprefix(fd, level, 1,TEXT("encode.SyncOperation.syncp=0x"),TEXT("%p"), sp->sync_point));
                dump_mfxBitstream(fd, level, TEXT("encode.SyncOperation.bs(out)"),sp->output.bitstream);
                //dump_format_wprefix(fd, level, 1,TEXT("encode.SyncOperation.wait="),TEXT("%u"),wait);
                
                if (wait == INFINITE)
                {
                    dump_format_wprefix(fd, level, 1,TEXT("encode.SyncOperation.wait=INFINITE"),TEXT(""));
                }else
                {
                    dump_format_wprefix(fd, level, 1,TEXT("encode.SyncOperation.wait="),TEXT("%u"),wait);
                }
                dump_mfxStatus(fd, level, TEXT("encode.SyncOperation"),sp->sts);

                break;
            case VPP:
                RECORD_POINTERS(dump_format_wprefix(fd, level, 1,TEXT("vpp.SyncOperation.syncp=0x"),TEXT("%p"), sp->sync_point));
                dump_mfxFrameSurface1(fd, level, TEXT("vpp.SyncOperation.out"),sp->output.out);
                dump_format_wprefix(fd, level, 1,TEXT("vpp.SyncOperation.wait="),TEXT("%u"),wait);
                if (wait == INFINITE)
                {
                    dump_format_wprefix(fd, level, 1,TEXT("vpp.SyncOperation.wait=INFINITE"),TEXT(""));
                }else
                {
                    dump_format_wprefix(fd, level, 1,TEXT("vpp.SyncOperation.wait="),TEXT("%u"),wait);
                }
                dump_mfxStatus(fd, level, TEXT("vpp.SyncOperation"),sp->sts);

                break;
            case VPPEX:
                RECORD_POINTERS(dump_format_wprefix(fd, level, 1,TEXT("vpp.SyncOperation.syncp=0x"),TEXT("%p"), sp->sync_point));
                dump_mfxFrameSurface1(fd, level, TEXT("*(vpp.SyncOperation.ppOut)"),*(sp->output.ppOut));
                dump_format_wprefix(fd, level, 1,TEXT("vpp.SyncOperation.wait="),TEXT("%u"),wait);
                if (wait == INFINITE)
                {
                    dump_format_wprefix(fd, level, 1,TEXT("vpp.SyncOperation.wait=INFINITE"),TEXT(""));
                }else
                {
                    dump_format_wprefix(fd, level, 1,TEXT("vpp.SyncOperation.wait="),TEXT("%u"),wait);
                }
                dump_mfxStatus(fd, level, TEXT("vpp.SyncOperation"),sp->sts);

                break;
            };
        });
    }

/* It's possible that SyncOperation be called very frequently in a busy loop. 
   Logging this level of information severally impact execution
  else
    {
        RECORD_CONFIGURATION_PER_FRAME({
            switch (sp->c) {
                case DECODE:
                    dump_mfxStatus(fd, level, TEXT("decode.SyncOperation"),sp->sts);
                break;
                case ENCODE:
                    dump_mfxStatus(fd, level, TEXT("encode.SyncOperation"),sp->sts);
                break;
                case VPP:
                    dump_mfxStatus(fd, level, TEXT("vpp.SyncOperation"),sp->sts);
                break;
            };
        });
    }
*/

    if (sp->sts==MFX_ERR_NONE) {
        AnalyzerSession::Statistics *stat=0;

        switch (sp->c) {
        case DECODE:
            stat=&as->decode;
            RECORD_RAW_BITS(DECODE|DIR_INPUT,stat->frames_sync,{
                if (sp->output.psurface_out) dump_mfxFrameSurface1Raw(fd2,sp->output.psurface_out, as->pAlloc );
            });
            break;
        case ENCODE:
            stat=&as->encode;
            RECORD_RAW_BITS_IF_NOFLAG(ENCODE|DIR_INPUT|FORCE_NO_SYNC,stat->frames_sync,{
                dump_mfxFrameSurface1Raw(fd2,sp->input.frame, as->pAlloc );
            });
            RECORD_RAW_BITS(ENCODE|DIR_OUTPUT,stat->frames_sync,{
                dump_mfxBitstreamRaw(fd2,sp->output.bitstream);
            });
            //we in stub mode and receive first frame from encode, lets skip all new encode tasks
            if (gc.encode.stub_mode && 
                NULL == as->encode.first_frame.Data && 
                sp->output.bitstream && 
                sp->output.bitstream->DataLength)
            {
                as->encode.bsData.insert(as->encode.bsData.begin(), sp->output.bitstream->Data, sp->output.bitstream->Data + sp->output.bitstream->DataLength);
                memcpy_s(&as->encode.first_frame, sizeof (mfxBitstream), sp->output.bitstream, sizeof (mfxBitstream));
                as->encode.first_frame.Data = &as->encode.bsData.front();
            }
            break;
        case VPP:
            stat=&as->vpp;
            RECORD_RAW_BITS_IF_NOFLAG(VPP|DIR_INPUT|FORCE_NO_SYNC,stat->frames_sync,{
                dump_mfxFrameSurface1Raw(fd2,sp->input.frame, as->pAlloc);
            });
            RECORD_RAW_BITS(VPP|DIR_OUTPUT,stat->frames_sync,{
                dump_mfxFrameSurface1Raw(fd2,sp->output.out, as->pAlloc);
            });
            break;
        }
        if (NULL != stat)
        {
            stat->frames_sync++;
            LARGE_INTEGER ts1;
            QueryPerformanceCounter(&ts1);
            stat->exec_time+=((mfxI64)ts1.QuadPart-sp->start_tick);
        }
    }

    if (sp->sts!=MFX_WRN_IN_EXECUTION) {
        sp->sync_point=0;
    }
    return sp->sts;
}
