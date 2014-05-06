/* /////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2012-2014 Intel Corporation. All Rights Reserved.
//
//
//          VP8 encoder part
//
*/
#include "mfx_common.h"

#if defined(MFX_ENABLE_VP8_VIDEO_ENCODE_HW) 
#include "mfx_vp8_encode_hybrid_hw.h"
#include "mfx_vp8_enc_common_hw.h"
#include "mfx_task.h"
#include "vm_time.h"

namespace MFX_VP8ENC
{
    int CheckMBData(sMBData* pData)
    {
        bool Y2block = true;
        int nError = 0;

        nError += !(pData->mb_ref_frame_sel<=3);
        if (pData->mb_ref_frame_sel == 0)
        {
            nError += !(pData->intra_y_mode_partition_type <= 4); 
            nError += !(pData->intra_uv_mode <= 3);
            if (pData->intra_y_mode_partition_type == 4)
            {
                nError += !(pData->intra_b_mode_sub_mv_mode_0 <= 9);
                nError += !(pData->intra_b_mode_sub_mv_mode_1 <= 9);
                nError += !(pData->intra_b_mode_sub_mv_mode_2 <= 9);
                nError += !(pData->intra_b_mode_sub_mv_mode_3 <= 9);
                nError += !(pData->intra_b_mode_sub_mv_mode_4 <= 9);
                nError += !(pData->intra_b_mode_sub_mv_mode_5 <= 9);
                nError += !(pData->intra_b_mode_sub_mv_mode_6 <= 9);
                nError += !(pData->intra_b_mode_sub_mv_mode_7 <= 9);
                nError += !(pData->intra_b_mode_sub_mv_mode_8 <= 9);
                nError += !(pData->intra_b_mode_sub_mv_mode_9 <= 9);
                nError += !(pData->intra_b_mode_sub_mv_mode_10 <= 9);
                nError += !(pData->intra_b_mode_sub_mv_mode_11 <= 9);
                nError += !(pData->intra_b_mode_sub_mv_mode_12 <= 9);
                nError += !(pData->intra_b_mode_sub_mv_mode_13 <= 9);
                nError += !(pData->intra_b_mode_sub_mv_mode_14 <= 9);
                nError += !(pData->intra_b_mode_sub_mv_mode_15 <= 9);
                Y2block = false;
            }
        }
        else
        {
            nError += !(pData->mv_mode <= 4);
            if (pData->mv_mode == 3) //new
            {
                nError += !(pData->MV[15].x <= 1023 && pData->MV[15].x >=-1023);
                nError += !(pData->MV[15].y <= 1023 && pData->MV[15].y >=-1023);
            }
            else if (pData->mv_mode == 4) //split
            {
                Y2block = false;
                nError += !(pData->intra_y_mode_partition_type <=5 && pData->intra_y_mode_partition_type>0);
                nError += !(pData->intra_b_mode_sub_mv_mode_0 <= 4);
                nError += !(pData->intra_b_mode_sub_mv_mode_1 <= 4);
                nError += !(pData->intra_b_mode_sub_mv_mode_2 <= 4);
                nError += !(pData->intra_b_mode_sub_mv_mode_3 <= 4);
                nError += !(pData->intra_b_mode_sub_mv_mode_4 <= 4);
                nError += !(pData->intra_b_mode_sub_mv_mode_5 <= 4);
                nError += !(pData->intra_b_mode_sub_mv_mode_6 <= 4);
                nError += !(pData->intra_b_mode_sub_mv_mode_7 <= 4);
                nError += !(pData->intra_b_mode_sub_mv_mode_8 <= 4);
                nError += !(pData->intra_b_mode_sub_mv_mode_9 <= 4);
                nError += !(pData->intra_b_mode_sub_mv_mode_10 <= 4);
                nError += !(pData->intra_b_mode_sub_mv_mode_11 <= 4);
                nError += !(pData->intra_b_mode_sub_mv_mode_12 <= 4);
                nError += !(pData->intra_b_mode_sub_mv_mode_13 <= 4);
                nError += !(pData->intra_b_mode_sub_mv_mode_14 <= 4);
                nError += !(pData->intra_b_mode_sub_mv_mode_15 <= 4);

                for (int i = 0; i<16; i++)
                {
                    nError += !(pData->MV[i].x <= 1023 && pData->MV[i].x >=-1023);
                    nError += !(pData->MV[i].y <= 1023 && pData->MV[i].y >=-1023);
                }
            }  
        }
        if (Y2block)
        {
            // need to fix driver issue;
            pData->Y1block_0[0] = pData->Y1block_1[0] = pData->Y1block_2[0] = pData->Y1block_3[0] = 
                pData->Y1block_4[0] = pData->Y1block_5[0] = pData->Y1block_6[0] = pData->Y1block_7[0] = 
                pData->Y1block_8[0] = pData->Y1block_9[0] = pData->Y1block_10[0] = pData->Y1block_11[0] =
                pData->Y1block_12[0] = pData->Y1block_13[0] = pData->Y1block_14[0] = pData->Y1block_15[0] = 0;        
        }    
        return nError;    
    }
    void printMBData(sMBData* pData)
    {
        bool bYblock = true;
        static const char MBModes[5][20]      = {"intra", "ref to last", "ref to golden", "ref to alt" , "unknown"};
        static const char IntraModes[6][20]   = {"DC_Pred", "V_Pred", "H_Pred", "TM_Pred","Block_Pred", "unknown"};
        static const char IntraBlocks[11][20] = {"DC", "VE", "HE", "TM", "LD", "RD", "VR", "VL", "HD", "HU", "unknown"};
        static const char InterMV[6][20]       ={"NEARESTMV", "NEARMV", "ZEROMV","NEWMV","SPLITMV","unknown"}; 
        static const char SplitModes [6][20]   ={"16x16","16x8","8x16","8x8","4x4","unknown"};
        static const char Split4x4Modes[5][20]   ={"LEFT","ABOVE","ZERO","NEW","unknown"};
        static const char blocks[25][20]         = {"Y2","Y1(0)","Y1(1)","Y1(2)","Y1(3)","Y1(4)",
            "Y1(5)","Y1(6)","Y1(7)","Y1(8)","Y1(9)",
            "Y1(10)","Y1(11)","Y1(12)","Y1(13)","Y1(14)","Y1(15)"
            "U(0)","U(1)","U(2)","U(3)",
            "V(0)","V(1)","V(2)","V(3)"};

        mfxI16* pBlock[25] = {   pData->Y2block,
            pData->Y1block_0,pData->Y1block_1,pData->Y1block_2,pData->Y1block_3,
            pData->Y1block_4,pData->Y1block_5,pData->Y1block_6,pData->Y1block_7,
            pData->Y1block_8,pData->Y1block_9,pData->Y1block_10,pData->Y1block_11,
            pData->Y1block_12,pData->Y1block_13,pData->Y1block_14,pData->Y1block_15,
            pData->Ublock_0,pData->Ublock_1,pData->Ublock_2,pData->Ublock_3,
            pData->Vblock_0,pData->Vblock_1,pData->Vblock_2,pData->Vblock_3};

        int sub [16]  = { pData->intra_b_mode_sub_mv_mode_0, pData->intra_b_mode_sub_mv_mode_1,pData->intra_b_mode_sub_mv_mode_2,pData->intra_b_mode_sub_mv_mode_3,
            pData->intra_b_mode_sub_mv_mode_4, pData->intra_b_mode_sub_mv_mode_5,pData->intra_b_mode_sub_mv_mode_6,pData->intra_b_mode_sub_mv_mode_7,
            pData->intra_b_mode_sub_mv_mode_8, pData->intra_b_mode_sub_mv_mode_9,pData->intra_b_mode_sub_mv_mode_10,pData->intra_b_mode_sub_mv_mode_11,
            pData->intra_b_mode_sub_mv_mode_12, pData->intra_b_mode_sub_mv_mode_13,pData->intra_b_mode_sub_mv_mode_14,pData->intra_b_mode_sub_mv_mode_15};

        printf("MB %d, %d: type: %s, ", pData->mbx, pData->mby, MBModes[pData->mb_ref_frame_sel < 5 ? pData->mb_ref_frame_sel : 4]);       

        if (pData->mb_ref_frame_sel == 0)
        {
            printf (" luma: %s, chroma: %s\n",   IntraModes[pData->intra_y_mode_partition_type < 6 ? pData->intra_y_mode_partition_type: 5],
                IntraModes[pData->intra_uv_mode < 5 ? pData->intra_uv_mode: 5]);
            if (pData->intra_y_mode_partition_type == 4)
            {
                bYblock = false;
                printf(" luma block types:\n");
                for(int j =0; j<16; j++)
                {
                    printf(" %s", IntraBlocks[sub[j] < 11 ? sub[j] : 10]);
                    if (j%4 == 3) printf("\n");                
                }
            }
        }
        else
        {
            printf(" MV mode %s\n", InterMV[pData->mv_mode <6? pData->mv_mode: 5]);
            if (pData->mv_mode == 3) 
            {   
                printf(" MV (%d, %d) \n", pData->MV[15].x, pData->MV[15].y);            
            }
            else if (pData->mv_mode == 4)
            {
                bYblock = false;
                printf(" Split Mode %s:\n", SplitModes[pData->intra_y_mode_partition_type < 6 ? pData->intra_y_mode_partition_type : 5]);
                for(int j =0; j<16; j++)
                {
                    if (sub[j]==3)
                    {
                        printf(" (%d,%d)", pData->MV[j].x, pData->MV[j].y);
                    }
                    else
                    {
                        printf(" %s,", Split4x4Modes[sub[j] < 5 ? sub[j] : 4]);
                    }
                    if (j%4 == 3) printf("\n");                
                }

            }
        }
        for (int i = bYblock ? 0 : 1;  i<25; i++)
        {
            printf("Block %s coeff:\n", blocks[i]);
            for (int j = 0; j < 16; j++)
            {
                printf("%5d,", pBlock[i][j]);
            }
            printf("\n");
        }
    }

    mfxStatus HybridPakDDIImpl::Init(mfxVideoParam * par)
    {

        mfxStatus sts  = MFX_ERR_NONE;
        mfxStatus sts1 = MFX_ERR_NONE; // to save warnings ater parameters checking

#if defined (VP8_HYBRID_DUMP_WRITE)
        m_bse_dump = fopen("dump_file","wb");
#elif defined (VP8_HYBRID_DUMP_READ)
        m_bse_dump = fopen("dump_file_vidyo1_QP_30","rb");
#endif

#if defined (VP8_HYBRID_DUMP_BS_INFO)
        m_bs_info = fopen("bs_info","w");
#endif

#if defined (VP8_HYBRID_TIMING)
        m_timings = fopen("timings.txt","a");
        m_timingsPerFrame = fopen("timingsPerFrame.txt", "a");
#endif

        m_video = *par;
        mfxExtCodingOptionVP8* pExtOpt    = GetExtBuffer(m_video);
        {
            mfxExtOpaqueSurfaceAlloc   * pExtOpaque = GetExtBuffer(m_video);
            mfxExtCodingOptionVP8Param * pVP8Par    = GetExtBuffer(m_video);

            MFX_CHECK(CheckFrameSize(par->mfx.FrameInfo.Width, par->mfx.FrameInfo.Height),MFX_ERR_INVALID_VIDEO_PARAM);

            sts1 = MFX_VP8ENC::CheckParametersAndSetDefault(par,&m_video, pExtOpt, pVP8Par, pExtOpaque, m_core->IsExternalFrameAllocator(),false);
            MFX_CHECK(sts1 >=0, sts1);
        }
        m_ddi.reset(CreatePlatformVp8Encoder( m_core));
        MFX_CHECK(m_ddi.get() != 0, MFX_WRN_PARTIAL_ACCELERATION);

        sts = m_ddi->CreateAuxilliaryDevice(m_core,DXVA2_Intel_Encode_VP8, 
            m_video.mfx.FrameInfo.Width, m_video.mfx.FrameInfo.Height);
        MFX_CHECK(sts == MFX_ERR_NONE, MFX_WRN_PARTIAL_ACCELERATION);

        ENCODE_CAPS_VP8 caps = {0};
        sts = m_ddi->QueryEncodeCaps(caps);
        if (sts != MFX_ERR_NONE)
            return MFX_WRN_PARTIAL_ACCELERATION;

        sts = CheckVideoParam(m_video, caps);
        MFX_CHECK(sts>=0,sts);

#if !defined (VP8_HYBRID_DUMP_READ)
        sts = m_ddi->CreateAccelerationService(m_video);
        MFX_CHECK_STS(sts);
#endif

        mfxFrameAllocRequest reqMB     = { 0 };
        mfxFrameAllocRequest reqDist   = { 0 };
        mfxFrameAllocRequest reqSegMap = { 0 };

        // on Linux we should allocate recon surfaces first, and then create encoding context and use it for allocation of other buffers
        // initialize task manager, including allocation of recon surfaces chain
        sts = m_taskManager.Init(m_core,&m_video,m_ddi->GetReconSurfFourCC());
        MFX_CHECK_STS(sts);

#if !defined (VP8_HYBRID_DUMP_READ)
        // encoding device/context is created inside this Register() call
        sts = m_ddi->Register(m_taskManager.GetRecFramesForReg(), D3DDDIFMT_NV12);
        MFX_CHECK_STS(sts);

        sts = m_ddi->QueryCompBufferInfo(D3DDDIFMT_INTELENCODE_MBDATA, reqMB, m_video.mfx.FrameInfo.Width, m_video.mfx.FrameInfo.Height);
        MFX_CHECK_STS(sts);
        sts = m_ddi->QueryCompBufferInfo(D3DDDIFMT_INTELENCODE_DISTORTIONDATA, reqDist, m_video.mfx.FrameInfo.Width, m_video.mfx.FrameInfo.Height);
        if (sts == MFX_ERR_NONE)
            reqDist.NumFrameMin = reqDist.NumFrameSuggested = (mfxU16)CalcNumSurfRecon(m_video);
        sts = m_ddi->QueryCompBufferInfo(D3DDDIFMT_INTELENCODE_SEGMENTMAP, reqSegMap, m_video.mfx.FrameInfo.Width, m_video.mfx.FrameInfo.Height);
        if (sts == MFX_ERR_NONE && pExtOpt->EnableMultipleSegments == MFX_CODINGOPTION_ON)
            reqSegMap.NumFrameMin = reqSegMap.NumFrameSuggested = (mfxU16)CalcNumSurfRecon(m_video);
#endif // !VP8_HYBRID_DUMP_READ

#if defined (VP8_HYBRID_DUMP_WRITE_STRUCTS) // dump of MB data to file
        fwrite(&(reqMB.Info), 1, sizeof(mfxFrameInfo), m_bse_dump);
#elif defined (VP8_HYBRID_DUMP_READ) // read of MB data from file
        fread(&reqMB.Info, 1, sizeof(reqMB.Info), m_bse_dump);
#endif

        sts = m_taskManager.AllocInternalResources(m_core,reqMB,reqDist,reqSegMap);
        MFX_CHECK_STS(sts);

        // [SE] Hack/WA for Windows hybrid VP8 HSW driver
#if defined (MFX_VA_WIN)
        memcpy(&(m_ddi->hackResponse), &(m_taskManager.m_MBDataDDI_hw.m_response), sizeof(mfxFrameAllocRequest));
#endif

#if !defined (VP8_HYBRID_DUMP_READ)
        sts = m_ddi->Register(m_taskManager.GetMBFramesForReg(), D3DDDIFMT_INTELENCODE_MBDATA);
        MFX_CHECK_STS(sts);
        if (reqDist.NumFrameMin)
        {
            sts = m_ddi->Register(m_taskManager.GetDistFramesForReg(), D3DDDIFMT_INTELENCODE_DISTORTIONDATA);
            MFX_CHECK_STS(sts);
        }
        if (reqSegMap.NumFrameMin)
        {
            sts = m_ddi->Register(m_taskManager.GetSegMapFramesForReg(), D3DDDIFMT_INTELENCODE_SEGMENTMAP);
            MFX_CHECK_STS(sts);
        }
#endif // !VP8_HYBRID_DUMP_READ

        sts = m_BSE.Init(m_video);
        MFX_CHECK_STS(sts);

#if defined (VP8_HYBRID_TIMING)
        memset(&m_I,0,sizeof(m_I));
        memset(&m_P,0,sizeof(m_I));
        cpuFreq = 3500;
        //TICK(cpuFreq)
        //Sleep(1000);
        //TOCK(cpuFreq);
        //cpuFreq = cpuFreq / 1000000;
#endif // VP8_HYBRID_TIMING

        return sts1;
    }

    mfxStatus HybridPakDDIImpl::Reset(mfxVideoParam * par)
    {
        mfxStatus sts  = MFX_ERR_NONE;
        mfxStatus sts1 = MFX_ERR_NONE;

        //printf("HybridPakDDIImpl::Reset\n");

        MFX_CHECK_NULL_PTR1(par);
        MFX_CHECK(par->IOPattern == m_video.IOPattern, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);

        m_video     = *par;

        {
            mfxExtCodingOptionVP8*       pExtOpt = GetExtBuffer(m_video);
            mfxExtCodingOptionVP8Param*  pVP8Par = GetExtBuffer(m_video);
            mfxExtOpaqueSurfaceAlloc*    pExtOpaque = GetExtBuffer(m_video);

            sts1 = MFX_VP8ENC::CheckParametersAndSetDefault(par,&m_video, pExtOpt, pVP8Par,pExtOpaque,m_core->IsExternalFrameAllocator(),true);
            MFX_CHECK(sts1>=0, sts1);
        }
        sts = m_taskManager.Reset(&m_video);
        MFX_CHECK_STS(sts);

        sts = m_ddi->Reset(*par);
        MFX_CHECK_STS(sts);

        return sts1;
    } 

    mfxStatus HybridPakDDIImpl::GetVideoParam(mfxVideoParam * par)
    {
        MFX_CHECK_NULL_PTR1(par);
        return MFX_VP8ENC::GetVideoParam(par,&m_video);
    } 


    mfxStatus HybridPakDDIImpl::EncodeFrameCheck( mfxEncodeCtrl *ctrl,
        mfxFrameSurface1 *surface,
        mfxBitstream *bs,
        mfxFrameSurface1 ** /*reordered_surface*/,
        mfxEncodeInternalParams * /*pInternalParams*/,
        MFX_ENTRY_POINT pEntryPoints[],
        mfxU32 &numEntryPoints)


    {
        Task* pTask = 0;
        mfxStatus sts  = MFX_ERR_NONE;

        mfxStatus checkSts = CheckEncodeFrameParam(
            m_video,
            ctrl,
            surface,
            bs,
            m_core->IsExternalFrameAllocator());

        MFX_CHECK(checkSts >= MFX_ERR_NONE, checkSts);

        {
            UMC::AutomaticUMCMutex guard(m_taskMutex);
            sts = m_taskManager.InitTask(surface,bs,pTask);
            MFX_CHECK_STS(sts);
        }

        pEntryPoints[0].pState               = pEntryPoints[1].pState               = this;
        pEntryPoints[0].pCompleteProc        = pEntryPoints[1].pCompleteProc        = 0;
        pEntryPoints[0].pGetSubTaskProc      = pEntryPoints[1].pGetSubTaskProc      = 0;
        pEntryPoints[0].pCompleteSubTaskProc = pEntryPoints[1].pCompleteSubTaskProc = 0;
        pEntryPoints[0].requiredNumThreads   = pEntryPoints[1].requiredNumThreads   = 1;
        pEntryPoints[0].pParam               = pEntryPoints[1].pParam               = pTask;

        pEntryPoints[0].pRoutineName        = "Submit Frame";
        pEntryPoints[0].pRoutine            = TaskRoutineSubmit;
        pEntryPoints[1].pRoutineName        = "Query Frame";
        pEntryPoints[1].pRoutine            = TaskRoutineQuery;

        numEntryPoints = 2;

        return checkSts;
    }

    mfxStatus HybridPakDDIImpl::TaskRoutineSubmit(  void * state,
        void * param,
        mfxU32 /*threadNumber*/,
        mfxU32 /*callNumber*/)
    {
        HybridPakDDIImpl &    impl = *(HybridPakDDIImpl *) state;
        Task *          task =  (Task *)param;

        mfxStatus sts = impl.SubmitFrame(task);
        MFX_CHECK_STS(sts);

        return MFX_TASK_DONE;
    }

    mfxStatus HybridPakDDIImpl::TaskRoutineQuery(  void * state,
        void * param,
        mfxU32 /*threadNumber*/,
        mfxU32 /*callNumber*/)
    {
        HybridPakDDIImpl &    impl = *(HybridPakDDIImpl *) state;
        Task *          task =  (Task *)param;

        mfxStatus sts = impl.QueryFrame(task);
        MFX_CHECK_STS(sts);

        return MFX_TASK_DONE;
    }

    mfxStatus HybridPakDDIImpl::SubmitFrame(Task *pTaskInput)
    { 
        TaskHybridDDI       *pTask = (TaskHybridDDI*)pTaskInput;
        mfxStatus           sts = MFX_ERR_NONE;
        sFrameParams        frameParams={0};
        mfxFrameSurface1    *pSurface=0;
        bool                bExternalSurface = true;

#if defined (VP8_HYBRID_TIMING)
        mfxU8 frmTimingIdx = pTask->m_frameOrder % 2;

        m_bsp[frmTimingIdx]            = 0;
        m_fullFrame[frmTimingIdx]      = 0;
        m_ddi->submit[frmTimingIdx]  = 0;
        m_ddi->hwAsync[frmTimingIdx] = 0;

        TICK(m_fullFrame[frmTimingIdx])
#endif // VP8_HYBRID_TIMING

        mfxHDLPair surfacePair = {0}; // D3D11
        mfxHDL     surfaceHDL = 0;// others
        mfxHDL *pSurfaceHdl = (mfxHDL *)&surfaceHDL;

        if (MFX_HW_D3D11 == m_core->GetVAType())
            pSurfaceHdl = (mfxHDL *)&surfacePair;

        {
            UMC::AutomaticUMCMutex guard(m_taskMutex);
            sts = SetFramesParams(&m_video,pTask->m_frameOrder, &frameParams);
            MFX_CHECK_STS(sts);
            sts = m_taskManager.SubmitTask(pTask,&frameParams);
            MFX_CHECK_STS(sts);
        }

        sts = pTask->GetInputSurface(pSurface, bExternalSurface);
        MFX_CHECK_STS(sts);

        sts = bExternalSurface ?
            m_core->GetExternalFrameHDL(pSurface->Data.MemId, pSurfaceHdl):
            m_core->GetFrameHDL(pSurface->Data.MemId, pSurfaceHdl);
        MFX_CHECK_STS(sts);

#if !defined (VP8_HYBRID_DUMP_READ)
       if (MFX_HW_D3D11 == m_core->GetVAType())
        {
            MFX_CHECK(surfacePair.first != 0, MFX_ERR_UNDEFINED_BEHAVIOR);
            sts = m_ddi->Execute(*pTask, (mfxHDL)pSurfaceHdl);
        }
        else if (MFX_HW_D3D9 == m_core->GetVAType())
        {
            MFX_CHECK(surfaceHDL != 0, MFX_ERR_UNDEFINED_BEHAVIOR);
            sts = m_ddi->Execute(*pTask, surfaceHDL);
        }
        else if (MFX_HW_VAAPI == m_core->GetVAType())
        {
            MFX_CHECK(surfaceHDL != 0, MFX_ERR_UNDEFINED_BEHAVIOR);
            sts = m_ddi->Execute(*pTask,  surfaceHDL);
        }
        MFX_CHECK_STS(sts);
#endif
        return sts;
    }

    mfxStatus HybridPakDDIImpl::QueryFrame(Task *pTaskInput)
    { 
        TaskHybridDDI       *pTask = (TaskHybridDDI*)pTaskInput;
        mfxStatus           sts = MFX_ERR_NONE;

#if defined (VP8_HYBRID_TIMING)
        mfxU8 frmTimingIdx = pTask->m_frameOrder % 2;
#endif // VP8_HYBRID_TIMING

        MBDATA_LAYOUT layout={0};
#if !defined (VP8_HYBRID_DUMP_READ)
        while ((sts = m_ddi->QueryStatus(*pTask) )== MFX_WRN_DEVICE_BUSY)
        {
            vm_time_sleep(0);
        }
        MFX_CHECK_STS(sts);
#if defined (VP8_HYBRID_TIMING)
        TOCK(m_ddi->hwAsync[frmTimingIdx])
#endif // VP8_HYBRID_TIMING

        MFX_CHECK_STS(m_ddi->QueryMBLayout(layout));
#endif

#if defined (VP8_HYBRID_TIMING)
        TICK(m_bsp[frmTimingIdx])
#endif // VP8_HYBRID_TIMING

#if defined (VP8_HYBRID_DUMP_WRITE) // dump of MB data to file
#if defined (VP8_HYBRID_DUMP_WRITE_STRUCTS)
        fwrite(&layout, 1, sizeof(layout), m_bse_dump);
#endif
        m_bse_dump_size = pTask->ddi_frames.m_pMB_hw->pSurface->Info.Width * pTask->ddi_frames.m_pMB_hw->pSurface->Info.Height;
        fwrite(&m_bse_dump_size, sizeof(m_bse_dump_size), 1, m_bse_dump);
#elif defined (VP8_HYBRID_DUMP_READ)
        fread(&layout, 1, sizeof(layout), m_bse_dump);
        fread(&m_bse_dump_size, sizeof(m_bse_dump_size), 1, m_bse_dump);
#endif

        sts = m_BSE.SetNextFrame(0, 0, pTask->m_sFrameParams,pTask->m_frameOrder);
        MFX_CHECK_STS(sts);

        mfxExtCodingOptionVP8Param * extOptVP8 = GetExtBuffer(m_video);
        bool bInsertIVF = (extOptVP8->WriteIVFHeaders != MFX_CODINGOPTION_OFF);
        bool bInsertSH  = bInsertIVF && pTask->m_frameOrder==0;

        m_BSE.RunBSP(bInsertIVF, bInsertSH, pTask->m_pBitsteam, (TaskHybridDDI *)pTask, layout, m_core
#if defined (VP8_HYBRID_DUMP_READ) || defined (VP8_HYBRID_DUMP_WRITE)
            , m_bse_dump
            , m_bse_dump_size
#endif
            );

        pTask->m_pBitsteam->TimeStamp = pTask->m_timeStamp;
        pTask->m_pBitsteam->FrameType = mfxU16(pTask->m_sFrameParams.bIntra ? MFX_FRAMETYPE_I : MFX_FRAMETYPE_P);

#if defined (VP8_HYBRID_DUMP_BS_INFO)
        fprintf(m_bs_info, "%u,%lu\n", pTask->m_sFrameParams.bIntra, pTask->m_pBitsteam->DataLength);
#endif

        sts = pTask->CompleteTask();

#if defined (VP8_HYBRID_TIMING)
        TOCK(m_bsp[frmTimingIdx])
        TOCK(m_fullFrame[frmTimingIdx])
        if (pTask->m_frameOrder == 0)
        {
            if (m_timings)
            {
                fprintf(m_timings, "\n ,full frame, submit, hw async, bsp\n");
                fprintf(m_timings, "1st I frame,");
                FPRINT(cpuFreq, m_fullFrame[frmTimingIdx], m_timings)
                FPRINT(cpuFreq, m_ddi->submit[frmTimingIdx], m_timings)
                FPRINT(cpuFreq, m_ddi->hwAsync[frmTimingIdx], m_timings)
                FPRINT(cpuFreq, m_bsp[frmTimingIdx], m_timings)
            }
            if (m_timingsPerFrame)
            {
                fprintf(m_timingsPerFrame, "\n ,full frame, submit, hw async, bsp\n");
                fprintf(m_timingsPerFrame, "I %d (%d),", pTask->m_frameOrder, m_I.numFrames);
            }
        }
        else
        {
            if (pTask->m_sFrameParams.bIntra)
            {
                // Intra
                ACCUM(cpuFreq, m_fullFrame[frmTimingIdx], m_I.fullFrameMcs)
                ACCUM(cpuFreq, m_ddi->submit[frmTimingIdx], m_I.submitFrameMcs)
                ACCUM(cpuFreq, m_ddi->hwAsync[frmTimingIdx], m_I.hwAsyncMcs)
                ACCUM(cpuFreq, m_bsp[frmTimingIdx], m_I.bspMcs)
                if (m_timingsPerFrame)
                    fprintf(m_timingsPerFrame, "I %d (%d),", pTask->m_frameOrder, m_I.numFrames);
                m_I.numFrames ++;
            }
            else
            {
                ACCUM(cpuFreq, m_fullFrame[frmTimingIdx], m_P.fullFrameMcs)
                ACCUM(cpuFreq, m_ddi->submit[frmTimingIdx], m_P.submitFrameMcs)
                ACCUM(cpuFreq, m_ddi->hwAsync[frmTimingIdx], m_P.hwAsyncMcs)
                ACCUM(cpuFreq, m_bsp[frmTimingIdx], m_P.bspMcs)
                if (m_timingsPerFrame)
                    fprintf(m_timingsPerFrame, "P %d (%d),", pTask->m_frameOrder, m_P.numFrames); 
                m_P.numFrames ++;
            }
        }
        if (m_timingsPerFrame)
        {
            FPRINT(cpuFreq, m_fullFrame[frmTimingIdx], m_timingsPerFrame)
            FPRINT(cpuFreq, m_ddi->submit[frmTimingIdx], m_timingsPerFrame)
            FPRINT(cpuFreq, m_ddi->hwAsync[frmTimingIdx], m_timingsPerFrame)
            FPRINT(cpuFreq, m_bsp[frmTimingIdx], m_timingsPerFrame)
        fprintf(m_timingsPerFrame, "\n");
        }
#endif // VP8_HYBRID_TIMING

        MFX_CHECK_STS(sts);

        {
            VP8HybridCosts updatedCosts = m_BSE.GetUpdatedCosts();
            UMC::AutomaticUMCMutex guard(m_taskMutex);
            m_taskManager.CacheInfoFromPak(*pTask,updatedCosts);
            pTask->FreeTask();
        }

        return sts;
    }
}

#endif // #if defined(MFX_ENABLE_VP8_VIDEO_ENCODE)
/* EOF */
