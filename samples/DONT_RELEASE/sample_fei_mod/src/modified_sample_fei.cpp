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

mfxStatus PakOneStreamoutFrame(mfxU32 m_numOfFields, iTask *eTask)
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
            sts = RepackStremoutMB2PakMB(m_pExtBufDecodeStreamout->MB + fieldId*feiEncMBCode->NumMBAlloc + i, feiEncMBCode->MB + i);
            MSDK_BREAK_ON_ERROR(sts);

            sts = RepackStreamoutMV(m_pExtBufDecodeStreamout->MB + fieldId*feiEncMBCode->NumMBAlloc + i, feiEncMV->MB + i);
            MSDK_BREAK_ON_ERROR(sts);
        }
    }

    return sts;
}

inline mfxStatus RepackStremoutMB2PakMB(mfxFeiDecStreamOutMBCtrl* dsoMB, mfxFeiPakMBCtrl* pakMB)
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
    pakMB->QpPrimeY            = dsoMB->QpPrimeY;
    pakMB->IsLastMB            = dsoMB->IsLastMB;
    pakMB->Direct8x8Pattern    = 0;//dsoMB->Direct8x8Pattern;
    pakMB->MbSkipConvDisable   = 1;

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
    /*else
    {
        for (int i=0; i<2; ++i)
            for (int j=0; j<4; ++j)
                pakMB->InterMB.RefIdx[i][j] &= 0x7f;
    }*/

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
