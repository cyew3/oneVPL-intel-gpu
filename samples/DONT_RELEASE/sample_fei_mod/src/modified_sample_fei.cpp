/*********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement.
This sample was distributed or derived from the Intel's Media Samples package.
The original version of this sample may be obtained from https://software.intel.com/en-us/intel-media-server-studio
or https://software.intel.com/en-us/media-client-solutions-support.
Copyright(c) 2005-2016 Intel Corporation. All Rights Reserved.

**********************************************************************************/

#include "modified_sample_fei.h"

static mfxU32 mv_data_length_offset = 0;
static const int mb_type_remap[26] = {0, 21, 22, 23, 24, 21, 22, 23, 24, 21, 22, 23, 24, 21, 22, 23, 24, 21, 22, 23, 24, 21, 22, 23, 24, 25};
static const int intra_16x16[26]   = {2,  0,  1,  2,  3,  0,  1,  2,  3,  0,  1,  2,  3,  0,  1,  2,  3,  0,  1,  2,  3,  0,  1,  2,  3,  2};

mfxStatus PakOneStreamoutFrame(mfxU32 m_numOfFields, iTask *eTask, mfxU8 QP, std::list<iTask*> *pTaskList)
{
    MSDK_CHECK_POINTER(eTask,                  MFX_ERR_NULL_PTR);
    MSDK_CHECK_POINTER(eTask->inPAK.InSurface, MFX_ERR_NULL_PTR);

    mfxExtFeiDecStreamOut* m_pExtBufDecodeStreamout = NULL;
    for (int i = 0; i < eTask->inPAK.InSurface->Data.NumExtParam; i++)
        if (eTask->inPAK.InSurface->Data.ExtParam[i]->BufferId == MFX_EXTBUFF_FEI_DEC_STREAM_OUT)
        {
            m_pExtBufDecodeStreamout = (mfxExtFeiDecStreamOut*)eTask->inPAK.InSurface->Data.ExtParam[i];
            break;
        }
    MSDK_CHECK_POINTER(m_pExtBufDecodeStreamout, MFX_ERR_NULL_PTR);

    mfxStatus sts = MFX_ERR_NONE;

    mfxExtFeiPakMBCtrl* feiEncMBCode = NULL;
    mfxExtFeiEncMV*     feiEncMV     = NULL;

    for (mfxU32 fieldId = 0; fieldId < m_numOfFields; fieldId++)
    {
        MSDK_BREAK_ON_ERROR(sts);

        mv_data_length_offset = 0;

        /* get mfxExtFeiPakMBCtrl buffer */
        feiEncMBCode = (mfxExtFeiPakMBCtrl*)getBufById(&eTask->bufs->PB_bufs.out, MFX_EXTBUFF_FEI_PAK_CTRL, fieldId);
        MSDK_CHECK_POINTER(feiEncMBCode, MFX_ERR_NULL_PTR);

        /* get mfxExtFeiEncMV buffer */
        feiEncMV = (mfxExtFeiEncMV*)getBufById(&eTask->bufs->PB_bufs.out, MFX_EXTBUFF_FEI_ENC_MV, fieldId);
        MSDK_CHECK_POINTER(feiEncMV, MFX_ERR_NULL_PTR);

        /* repack streamout output to PAK input */
        for (mfxU32 i = 0; i < feiEncMBCode->NumMBAlloc; i++){

            /* temporary, this flag is not set at all by driver */
            (m_pExtBufDecodeStreamout->MB + fieldId*feiEncMBCode->NumMBAlloc + i)->IsLastMB = (i == (feiEncMBCode->NumMBAlloc-1));

            /* NOTE: streamout holds data for both fields in MB array (first NumMBAlloc for first field data, second NumMBAlloc for second field) */
            sts = RepackStremoutMB2PakMB(m_pExtBufDecodeStreamout->MB + fieldId*feiEncMBCode->NumMBAlloc + i, feiEncMBCode->MB + i, QP);
            MSDK_BREAK_ON_ERROR(sts);

            sts = RepackStreamoutMV(m_pExtBufDecodeStreamout->MB + fieldId*feiEncMBCode->NumMBAlloc + i, feiEncMV->MB + i);
            MSDK_BREAK_ON_ERROR(sts);
        }
        sts = ResetDirect (eTask, pTaskList);
    }

    return sts;
}

inline mfxStatus RepackStremoutMB2PakMB(mfxFeiDecStreamOutMBCtrl* dsoMB, mfxFeiPakMBCtrl* pakMB, mfxU8 QP)
{
    MSDK_CHECK_POINTER(dsoMB, MFX_ERR_NULL_PTR);
    MSDK_CHECK_POINTER(pakMB, MFX_ERR_NULL_PTR);

    /* fill header */
    pakMB->Header              = MFX_PAK_OBJECT_HEADER;
    pakMB->MVDataLength        = dsoMB->IntraMbFlag? 0 : 128;
    pakMB->MVDataOffset        = dsoMB->IntraMbFlag? 0 : mv_data_length_offset;
    mv_data_length_offset     += 128;

    pakMB->Reserved03 = dsoMB->IntraMbFlag? 0x800: 0x806;

    pakMB->InterMbMode         = dsoMB->InterMbMode;
    pakMB->MBSkipFlag          = 0;//dsoMB->MBSkipFlag;
    pakMB->IntraMbMode         = dsoMB->IntraMbMode;
    pakMB->FieldMbPolarityFlag = dsoMB->FieldMbPolarityFlag;
    pakMB->IntraMbFlag         = dsoMB->IntraMbFlag;
    pakMB->MbType              = pakMB->IntraMbFlag? mb_type_remap[dsoMB->MbType]: dsoMB->MbType;
    pakMB->FieldMbFlag         = dsoMB->FieldMbFlag;
    pakMB->Transform8x8Flag    = dsoMB->Transform8x8Flag;
    pakMB->DcBlockCodedCrFlag  = 1;//dsoMB->DcBlockCodedCrFlag;
    pakMB->DcBlockCodedCbFlag  = 1;//dsoMB->DcBlockCodedCbFlag;
    pakMB->DcBlockCodedYFlag   = 1;//dsoMB->DcBlockCodedYFlag;
    pakMB->HorzOrigin          = dsoMB->HorzOrigin;
    pakMB->VertOrigin          = dsoMB->VertOrigin;
    pakMB->CbpY                = 0xffff;//dsoMB->CbpY;
    pakMB->CbpCb               = 0xf;//dsoMB->CbpCb;
    pakMB->CbpCr               = 0xf;//dsoMB->CbpCr;
    pakMB->QpPrimeY            = QP;//dsoMB->QpPrimeY;
    pakMB->IsLastMB            = dsoMB->IsLastMB;
    pakMB->Direct8x8Pattern    = 0;//dsoMB->Direct8x8Pattern;
    pakMB->MbSkipConvDisable   = 0;

    memcpy(&pakMB->InterMB, &dsoMB->InterMB, sizeof(pakMB->InterMB)); // this part is common

    if (pakMB->IntraMbFlag){
        if (dsoMB->MbType){
            pakMB->IntraMB.LumaIntraPredModes[0] =
            pakMB->IntraMB.LumaIntraPredModes[1] =
            pakMB->IntraMB.LumaIntraPredModes[2] =
            pakMB->IntraMB.LumaIntraPredModes[3] = intra_16x16[dsoMB->MbType] * 0x1111;
        }
        else{
            if (pakMB->Transform8x8Flag){
                pakMB->IntraMB.LumaIntraPredModes[0] =  (dsoMB->IntraMB.LumaIntraPredModes[0]&0x000f)      * 0x1111;
                pakMB->IntraMB.LumaIntraPredModes[1] = ((dsoMB->IntraMB.LumaIntraPredModes[0]&0x00f0)>>4)  * 0x1111;
                pakMB->IntraMB.LumaIntraPredModes[2] = ((dsoMB->IntraMB.LumaIntraPredModes[0]&0x0f00)>>8)  * 0x1111;
                pakMB->IntraMB.LumaIntraPredModes[3] = ((dsoMB->IntraMB.LumaIntraPredModes[0]&0xf000)>>12) * 0x1111;
            }
            /*
             * this is already done during memcpy
            else
            {
                pakMB->IntraMB.LumaIntraPredModes[0] = dsoMB->IntraMB.LumaIntraPredModes[0];
                pakMB->IntraMB.LumaIntraPredModes[1] = dsoMB->IntraMB.LumaIntraPredModes[1];
                pakMB->IntraMB.LumaIntraPredModes[2] = dsoMB->IntraMB.LumaIntraPredModes[2];
                pakMB->IntraMB.LumaIntraPredModes[3] = dsoMB->IntraMB.LumaIntraPredModes[3];
            }*/
        }
    }
    else
    {
        pakMB->InterMB.SubMbShapes = 0;
//        for (int i=0; i<2; ++i) // this operation gives error!!!
//            // assume DSO SubMbPredModes filled per subMb, not per prediction unit
//            for (int j=0; j<4; ++j) {
//                if (((pakMB->InterMB.SubMbPredModes>>2*j)&3) == (1-i)) // 0 for 1 and 1 for 0 means direction is unused
//                    pakMB->InterMB.RefIdx[i][j] = 255;
//            }
    }

    pakMB->TargetSizeInWord = 0xff;
    pakMB->MaxSizeInWord    = 0xff;

    pakMB->reserved2[4]     = pakMB->IsLastMB ? 0x5000000 : 0; /* end of slice */

    return MFX_ERR_NONE;
}

inline mfxStatus RepackStreamoutMV(mfxFeiDecStreamOutMBCtrl* dsoMB, mfxExtFeiEncMV::mfxExtFeiEncMVMB* encMB)
{

    for (int i = 0; i < 16; i++){
        encMB->MV[i][0] = dsoMB->MV[i>>2][0];
        encMB->MV[i][1] = dsoMB->MV[i>>2][1];
    }

    return MFX_ERR_NONE;
}


//
// To recompute and reset direct
//

#define I_SLICE(SliceType) ((SliceType) == 2 || (SliceType) == 7)
#define P_SLICE(SliceType) ((SliceType) == 0 || (SliceType) == 5)
#define B_SLICE(SliceType) ((SliceType) == 1 || (SliceType) == 6)

#define MIN(a,b)    (((a) < (b)) ? (a) : (b))

const mfxU8 rasterToZ[16] = {0,1,4,5,2,3,6,7, 8,9,12,13,10,11,14,15}; // the same for reverse

const mfxU8 sbDirect[2][16] = {
    {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15},
    {0,0,3,3,0,0,3,3,12,12,15,15,12,12,15,15}
}; // raster idx [direct_inference_8x8][raster 4x4 block]

const mfxI16Pair zeroMv = {0,0};

#define MV_NEIGHBOUR(umb, neib, nbl) \
    mfxI32 pmode = (mbCode->MB[umb].InterMB.SubMbPredModes >> (nbl*2)) & 3; \
    if (pmode == 2 || pmode == list) { \
        mvn[neib] = &mvs->MB[umb].MV[nbl*5][list]; \
        isRefDif[neib] = (ref_idx != mbCode->MB[umb].InterMB.RefIdx[list][nbl]); \
    }

void MVPrediction(mfxI32 uMB, mfxI32 wmb, mfxI32 uMBx, mfxI32 uMBy, mfxU8 list, mfxU8 ref_idx,
    const mfxExtFeiEncMV* mvs, const mfxExtFeiPakMBCtrl* mbCode, mfxI16Pair *mvPred)
{
    bool isRefDif[3] = {1,1,1};
    bool hasB = 0, hasC = 0;
    const mfxI16Pair *mvn[3] = {&zeroMv, &zeroMv, &zeroMv};
    // only for 16x16
    if (uMBx > 0 && !mbCode->MB[uMB-1].IntraMbFlag) { // A
        MV_NEIGHBOUR(uMB-1, 0, 1)
    }
    if (uMBy > 0) {
        hasB = 1;
        if (!mbCode->MB[uMB-wmb].IntraMbFlag) { // B
            MV_NEIGHBOUR(uMB-wmb, 1, 2)
        }
        if (uMBx < wmb-1) {
            hasC = 1;
            if (!mbCode->MB[uMB-wmb+1].IntraMbFlag) { // C
                MV_NEIGHBOUR(uMB-wmb+1, 2, 2)
            }
        } else if (uMBx > 0) {
            hasC = 1;
            if (!mbCode->MB[uMB-wmb-1].IntraMbFlag) { // D
                MV_NEIGHBOUR(uMB-wmb-1, 2, 3)
            }
        }
    }

    if (!hasB && !hasC)
        *mvPred = *(mvn[0]);
    else if (isRefDif[0] + isRefDif[1] + isRefDif[2] == 2) {
        if (!isRefDif[0]) *mvPred = *(mvn[0]);
        else if (!isRefDif[1]) *mvPred = *(mvn[1]);
        else *mvPred = *(mvn[2]);
    } else {
        mvPred->x = MIN(mvn[0]->x, mvn[1]->x) ^ MIN(mvn[0]->x, mvn[2]->x) ^ MIN(mvn[2]->x, mvn[1]->x);
        mvPred->y = MIN(mvn[0]->y, mvn[1]->y) ^ MIN(mvn[0]->y, mvn[2]->y) ^ MIN(mvn[2]->y, mvn[1]->y);
    }
}

// for B 16x16
void RefPrediction(mfxI32 uMB, mfxI32 wmb, mfxI32 uMBx, mfxI32 uMBy,
    const mfxExtFeiPakMBCtrl* mbCode, mfxU8* refs)
{
    mfxI32 pmode;
    mfxU8 ref[2][3] = {{255,255,255}, {255,255,255}};
    // only for 16x16
    if (uMBx > 0 && !mbCode->MB[uMB-1].IntraMbFlag) {
        pmode = (mbCode->MB[uMB-1].InterMB.SubMbPredModes >> 2) & 3;
        if (pmode == 2 || pmode == 0) ref[0][0] = mbCode->MB[uMB-1].InterMB.RefIdx[0][1];
        if (pmode == 2 || pmode == 1) ref[1][0] = mbCode->MB[uMB-1].InterMB.RefIdx[1][1];
    }
    if (uMBy > 0 && !mbCode->MB[uMB-wmb].IntraMbFlag) {
        pmode = (mbCode->MB[uMB-wmb].InterMB.SubMbPredModes >> 4) & 3;
        if (pmode == 2 || pmode == 0) ref[0][1] = mbCode->MB[uMB-wmb].InterMB.RefIdx[0][2];
        if (pmode == 2 || pmode == 1) ref[1][1] = mbCode->MB[uMB-wmb].InterMB.RefIdx[1][2];
    }
    if (uMBy > 0) {
        if (uMBx < wmb-1) {
            if (!mbCode->MB[uMB-wmb+1].IntraMbFlag) { // C
                pmode = (mbCode->MB[uMB-wmb+1].InterMB.SubMbPredModes >> 4) & 3;
                if (pmode == 2 || pmode == 0) ref[0][2] = mbCode->MB[uMB-wmb+1].InterMB.RefIdx[0][2];
                if (pmode == 2 || pmode == 1) ref[1][2] = mbCode->MB[uMB-wmb+1].InterMB.RefIdx[1][2];
            }
        } else if (uMBx > 0 && !mbCode->MB[uMB-wmb-1].IntraMbFlag) { // D
            pmode = (mbCode->MB[uMB-wmb-1].InterMB.SubMbPredModes >> 6) & 3;
            if (pmode == 2 || pmode == 0) ref[0][2] = mbCode->MB[uMB-wmb-1].InterMB.RefIdx[0][3];
            if (pmode == 2 || pmode == 1) ref[1][2] = mbCode->MB[uMB-wmb-1].InterMB.RefIdx[1][3];
        }
    }
    refs[0] = MIN(MIN(ref[0][0], ref[0][1]), ref[0][2]);
    refs[1] = MIN(MIN(ref[1][0], ref[1][1]), ref[1][2]);
}



// pTaskList for CEncodingPipeline::m_inputTasks;
mfxStatus ResetDirect (struct iTask * task, std::list<iTask*> *pTaskList)
{
    if (!task || !pTaskList)
        return MFX_ERR_NULL_PTR;
    if (!task->in.InSurface)
        return MFX_ERR_NULL_PTR;
    mfxFrameInfo* fi = &task->in.InSurface->Info;

    mfxI32 wmb = (fi->Width+15)>>4;
    mfxI32 hmb = (fi->Height+15)>>4;

    const mfxExtFeiSPS* sps = (mfxExtFeiSPS*)GetExtBuffer(task->outPAK.ExtParam, task->outPAK.NumExtParam, MFX_EXTBUFF_FEI_SPS);
    const mfxExtFeiPPS* pps = (mfxExtFeiPPS*)GetExtBuffer(task->outPAK.ExtParam, task->outPAK.NumExtParam, MFX_EXTBUFF_FEI_PPS);
    const mfxExtFeiSliceHeader* sliceHeader = (mfxExtFeiSliceHeader*)GetExtBuffer(task->outPAK.ExtParam, task->outPAK.NumExtParam, MFX_EXTBUFF_FEI_SLICE);

    const mfxExtFeiEncMV* mvs = ( mfxExtFeiEncMV*)GetExtBuffer(task->inPAK.ExtParam, task->inPAK.NumExtParam, MFX_EXTBUFF_FEI_ENC_MV);
    const mfxExtFeiPakMBCtrl* mbCode = (mfxExtFeiPakMBCtrl*)GetExtBuffer(task->inPAK.ExtParam, task->inPAK.NumExtParam, MFX_EXTBUFF_FEI_PAK_CTRL);

    if (sliceHeader && sliceHeader->NumSlice == 0)
        return MFX_ERR_INVALID_VIDEO_PARAM;

    const mfxExtFeiEncMV* refmvs = 0;
    const mfxExtFeiPakMBCtrl* refmbCode = 0;


    if (mvs && mbCode && sliceHeader && pps)
    for (mfxI32 uMB = 0, uMBy = 0, slice = -1, nextSliceMB = 0; uMBy < hmb; uMBy++) for (mfxI32 uMBx = 0; uMBx < wmb; uMBx++, uMB++) {
        while (nextSliceMB <= uMB && slice+1 < sliceHeader->NumSlice) {
            slice++; // next or first slice
            nextSliceMB = sliceHeader->Slice[slice].MBAaddress + sliceHeader->Slice[slice].NumMBs;

            if (!B_SLICE(sliceHeader->Slice[slice].SliceType)) break;

            int ridx = sliceHeader->Slice[slice].RefL1[0].Index;
            int fidx = pps->ReferenceFrames[ridx];
            mfxFrameSurface1 *refSurface = task->inPAK.L0Surface[fidx];

            // find iTask of L1[0]
            struct iTask * reftask = 0;
            for(std::list<iTask*>::iterator it = pTaskList->begin(); it != pTaskList->end(); ++it) {
                //if ((*it)->outPAK.OutSurface->Data.FrameOrder != refSurface->Data.FrameOrder)
                if ((*it)->outPAK.OutSurface != refSurface)
                    continue;
                reftask = *it;
                break;
            }
            if (reftask) {
                refmvs = ( mfxExtFeiEncMV*)GetExtBuffer(reftask->inPAK.ExtParam, reftask->inPAK.NumExtParam, MFX_EXTBUFF_FEI_ENC_MV);
                refmbCode = (mfxExtFeiPakMBCtrl*)GetExtBuffer(reftask->inPAK.ExtParam, reftask->inPAK.NumExtParam, MFX_EXTBUFF_FEI_PAK_CTRL);
                //if (!refmbCode && !refmvs) return MFX_ERR_INVALID_VIDEO_PARAM; // need it?
            }
        }
        if (slice >= sliceHeader->NumSlice) return MFX_ERR_INVALID_VIDEO_PARAM;
        if (!B_SLICE(sliceHeader->Slice[slice].SliceType)) continue;

        //AvcRefine::MBParams *mbs = &refine->mbs[uMB];
        //memset(mbs, 0, sizeof(*mbs)); // will be filled in RefineAfterPak()

        // Inter prediction
        if (mbCode->MB[uMB].IntraMbFlag == 0) {

            mfxI16Pair mvSkip[2];
            mfxU8 refSkip[2];
            mfxU8 canSkip; // true if MV==skipMV ref==0 and 16x16 (==directMV and 8x8 for B)
            mfxU8 canSkipSb[4]; // true if MV==directMV for 8x8 block
            mfxI16Pair mvDirect[2][16]; // [list][raster sb]
            mfxU8 refDirect[2][16];
            int smodes = mbCode->MB[uMB].InterMB.SubMbPredModes;

            // predicted ref/MV to check if it is true skip
            mvSkip[0] = mvSkip[1] = zeroMv;
            refSkip[0] = 0;
            refSkip[1] = P_SLICE(sliceHeader->Slice[slice].SliceType) ? 255 :0;
// no need unless fixed
//            if (B_SLICE(sliceHeader->Slice[slice].SliceType) /*&& !mbCode->MB[uMB].IntraMbFlag*/) { // expand SubMbPredModes
//                int newsmodes = smodes;
//                switch (mbCode->MB[uMB].InterMbMode) {
//                    case 0:
//                        newsmodes = (smodes&3)*0x55;
//                        break;
//                    case 1:
//                        newsmodes = (smodes&0x3)*0x5 + (smodes&0xC)*0x14;
//                        break; //unexpected order!!!
//                    case 2:
//                        newsmodes = (smodes&15)*0x11;
//                        break;
//                    default: break;
//                }
//                if (smodes != newsmodes)
//                    smodes = newsmodes; // place for breakpoint
//            }
            // fill unused refidx unless fixed
            for (int sb=0; sb<4; sb++) {
                mfxI32 modes = (smodes >> sb*2) & 3;
                if (modes == 0) mbCode->MB[uMB].InterMB.RefIdx[1][sb] = 255;
                else if (modes == 1) mbCode->MB[uMB].InterMB.RefIdx[0][sb] = 255;
            }

            if (P_SLICE(sliceHeader->Slice[slice].SliceType)) {
                if (uMBx > 0 && uMBy > 0 &&
                    (mbCode->MB[uMB-1].IntraMbFlag || mbCode->MB[uMB-1].InterMB.RefIdx[0][1] != 0 ||
                        (mvs->MB[uMB-1].MV[5][0].x | mvs->MB[uMB-1].MV[5][0].y) != 0) &&
                    (mbCode->MB[uMB-wmb].IntraMbFlag || mbCode->MB[uMB-wmb].InterMB.RefIdx[0][2] != 0 ||
                        (mvs->MB[uMB-wmb].MV[10][0].x | mvs->MB[uMB-wmb].MV[10][0].y) != 0) )
                {
                    MVPrediction(uMB, wmb, uMBx, uMBy, 0, 0, mvs, mbCode, &mvSkip[0]);
                    refSkip[1] = 255;
                    canSkip = (mbCode->MB[uMB].InterMbMode == 0 && mbCode->MB[uMB].InterMB.RefIdx[0][0] == 0 &&
                        mvs->MB[uMB].MV[0][0].x == mvSkip[0].x && mvs->MB[uMB].MV[0][0].y == mvSkip[0].y);
                }
            } else { // B-slice
//continue;
                RefPrediction(uMB, wmb, uMBx, uMBy, mbCode, refSkip);
                if (refSkip[0] < 32) MVPrediction(uMB, wmb, uMBx, uMBy, 0, refSkip[0], mvs, mbCode, &mvSkip[0]);
                if (refSkip[1] < 32) MVPrediction(uMB, wmb, uMBx, uMBy, 1, refSkip[1], mvs, mbCode, &mvSkip[1]);

                // Direct predictors, frame only (mb_col == uMB etc)
                mfxI32 noRefs = refSkip[0] >= 32 && refSkip[1] >= 32;
                canSkip = 1;
                for (mfxI32 sb = 0; sb < 4; sb++) {
                    canSkipSb[sb] = 1;
                    for (mfxI32 ypos=0; ypos < 2; ypos++) for (mfxI32 xpos=0; xpos < 2; xpos++) { // 4 4x4 blocks
                        mfxI32 zeroPred[2] = {0, 0};
                        mfxI32 bl = (ypos + (sb&2))*4 + (sb&1)*2 + xpos; // raster 4x4 block
                        mfxI32 sbColz = rasterToZ[sbDirect[sps->Direct8x8InferenceFlag][bl]];


                        //H264EncoderFrame* frm = refine->refList[1][0];

                        if (!noRefs && refmbCode && !refmbCode->MB[uMB].IntraMbFlag && refmvs /*&& is_l1_pic_short_term*/) // no long term
                        {
                            mfxU8 ref_col = refmbCode->MB[uMB].InterMB.RefIdx[0][sb];
                            const mfxI16Pair *mv_col;
                            if (ref_col <= 32)
                                mv_col = &refmvs->MB[uMB].MV[sbColz][0];
                            else {
                                ref_col = refmbCode->MB[uMB].InterMB.RefIdx[1][sb];
                                mv_col = &refmvs->MB[uMB].MV[sbColz][1];
                            }
                            if (ref_col == 0 &&
                                mv_col->x >= -1 && mv_col->x <= 1 &&
                                mv_col->y >= -1 && mv_col->y <= 1)
                            {
                                zeroPred[0] = (refSkip[0] == 0);
                                zeroPred[1] = (refSkip[1] == 0);
                                //refine->sbTypeDirect[sb] = 3; // 4x4
                            }
                        }
                        mvDirect[0][bl] = (refSkip[0] < 32 && !zeroPred[0]) ? mvSkip[0] : zeroMv;
                        refDirect[0][bl] = noRefs ? 0 : refSkip[0];
                        mvDirect[1][bl] = (refSkip[1] < 32 && !zeroPred[1]) ? mvSkip[1] : zeroMv;
                        refDirect[1][bl] = noRefs ? 0 : refSkip[1];
                        if (canSkipSb[sb]) {
                            mfxI32 blz = sb*4 + ypos*2 + xpos;
                            mfxI32 modes = (smodes >> sb*2) & 3;
                            for (int l=0; l<2; l++) {
                                mfxI32 sameMV = ((refDirect[l][bl]>=32 && modes == 1-l) || // both unused
                                    //((l+1) & (modes+1)) && // direction is used (unused RefIdx is 0-filled
                                    (mbCode->MB[uMB].InterMB.RefIdx[l][sb] == refDirect[l][bl] && // same ref
                                    mvs->MB[uMB].MV[blz][l].x == mvDirect[l][bl].x && mvs->MB[uMB].MV[blz][l].y == mvDirect[l][bl].y)); // same MV
                                canSkipSb[sb] &= sameMV;
                            }
                        }
                    }
                    canSkip &= canSkipSb[sb];
                }

            } // direct predictors for B

            mfxFeiPakMBCtrl& mb = mbCode->MB[uMB];
            mfxExtFeiEncMV::mfxExtFeiEncMVMB& mv = mvs->MB[uMB];
            if ( mb.InterMbMode == 3 && mb.InterMB.SubMbShapes != 0) {
                for (int bl = 0; bl < 4; bl++)
                    for (int list = 0; list < 2; list++)
                        for (int sb = 0; sb < 4; sb ++)
                            mv.MV[bl*4+sb][list] = mv.MV[bl*4][list];
            }

            mb.InterMB.SubMbShapes = 0;

            // mark directs
            mb.Direct8x8Pattern = 0;
            if (B_SLICE(sliceHeader->Slice[slice].SliceType)) {
                if (canSkip) { // to be direct
                    mb.MbType = 22;
                    mb.InterMbMode = 3;
                    mb.Direct8x8Pattern = 15;
                } else if (mb.InterMbMode == 3) for (int bl = 0; bl < 4; bl++) {
                    if (canSkipSb[bl])
                        mb.Direct8x8Pattern |= 1 << bl;
                }
            }

            // TODO: try recombination, at least after sub-shapes changed to 0, better to count coded MV
            if (mb.InterMbMode != 0 && mb.Direct8x8Pattern != 15) {
                mfxU32 newmode = mb.InterMbMode;
                int difx0 = (mb.InterMB.RefIdx[0][0] != mb.InterMB.RefIdx[0][1]) || (mv.MV[0][0].x != mv.MV[4][0].x) ||  (mv.MV[0][0].y != mv.MV[4][0].y);
                int dify0 = (mb.InterMB.RefIdx[0][0] != mb.InterMB.RefIdx[0][2]) || (mv.MV[0][0].x != mv.MV[8][0].x) ||  (mv.MV[0][0].y != mv.MV[8][0].y);
                int difx1 = (mb.InterMB.RefIdx[0][2] != mb.InterMB.RefIdx[0][3]) || (mv.MV[8][0].x != mv.MV[12][0].x) ||  (mv.MV[8][0].y != mv.MV[12][0].y);
                int dify1 = (mb.InterMB.RefIdx[0][1] != mb.InterMB.RefIdx[0][3]) || (mv.MV[4][0].x != mv.MV[12][0].x) ||  (mv.MV[4][0].y != mv.MV[12][0].y);
                if (B_SLICE(sliceHeader->Slice[slice].SliceType)) {
                    difx0 |= (mb.InterMB.RefIdx[1][0] != mb.InterMB.RefIdx[1][1]) || (mv.MV[0][1].x != mv.MV[4][1].x) ||  (mv.MV[0][1].y != mv.MV[4][1].y);
                    dify0 |= (mb.InterMB.RefIdx[1][0] != mb.InterMB.RefIdx[1][2]) || (mv.MV[0][1].x != mv.MV[8][1].x) ||  (mv.MV[0][1].y != mv.MV[8][1].y);
                    difx1 |= (mb.InterMB.RefIdx[1][2] != mb.InterMB.RefIdx[1][3]) || (mv.MV[8][1].x != mv.MV[12][1].x) ||  (mv.MV[8][1].y != mv.MV[12][1].y);
                    dify1 |= (mb.InterMB.RefIdx[1][1] != mb.InterMB.RefIdx[1][3]) || (mv.MV[4][1].x != mv.MV[12][1].x) ||  (mv.MV[4][1].y != mv.MV[12][1].y);
                }
                if (!difx0 && !difx1)
                    if (!dify0 && !dify1) newmode = 0; //16x16
                    else newmode = 1; //16x8
                else if (!dify0 && !dify1) newmode = 2; //8x16
                if (newmode != mb.InterMbMode) {
                    mb.Direct8x8Pattern = 0; // anyway not 8x8
                    if (P_SLICE(sliceHeader->Slice[slice].SliceType) || (mb.InterMB.RefIdx[1][0]>=32 && mb.InterMB.RefIdx[1][3]>=32)) // L0 only
                        mb.MbType = newmode==0 ? 1 : newmode + 3; // 012 -> 145
                    else { // B-modes
                        int p0 = (mb.InterMB.RefIdx[0][0]<32) + 2*(mb.InterMB.RefIdx[1][0]<32);
                        int p1 = (mb.InterMB.RefIdx[0][3]<32) + 2*(mb.InterMB.RefIdx[1][3]<32);
                        if (newmode==0)
                            mb.MbType = p0;
                        else if (p0==1)
                            mb.MbType = p1 * 4 + newmode-1;
                        else if (p0==3)
                            mb.MbType = p1 * 2 + newmode-1 + 14;
                        else // p0==2
                            mb.MbType = ((p1==1) ? 10 : ((p1==2) ? 6 : 14)) + newmode-1;
                    }
                }
            }

        } // inter

    } // MB loop

    return MFX_ERR_NONE;
}
