//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2008-2013 Intel Corporation. All Rights Reserved.
//

#include "mfx_vc1_enc_defs.h"

#if defined (MFX_ENABLE_VC1_VIDEO_ENCODER)
#include "mfx_vc1_enc_common.h"
#include "umc_vc1_enc_tables.h"
#include "mfx_vc1_enc_ex_param_buf.h"
#include "umc_vc1_enc_bit_rate_control.h"
extern Ipp8u BFractionScaleFactor[30][3] =
{
    {1,2,128},
    {1,3,85},
    {2,3,170},
    {1,4,64},
    {3,4,192},
    {1,5,51},
    {2,5,102},
    {0,0,0},
    {0,0,0},
    {0,0,0},
    {0,0,0},
    {0,0,0},
    {0,0,0},
    {0,0,0},
    {0,0,0},
    {0,0,0},
    {3,5,153},
    {4,5,204},
    {1,6,43},
    {5,6,215},
    {1,7,37},
    {2,7,74},
    {3,7,111},
    {4,7,148},
    {5,7,185},
    {6,7,222},
    {1,8,32},
    {3,8,96},
    {5,8,160},
    {7,8,224}
};
mfxStatus  GetBFractionParameters (mfxU8 code,Ipp8u &nom, Ipp8u &denom, Ipp8u &scaleFactor)
{
    MFX_CHECK_COND(code<30)

    nom         = BFractionScaleFactor[code][0];
    denom       = BFractionScaleFactor[code][1];
    scaleFactor = BFractionScaleFactor[code][2];

    MFX_CHECK_COND(denom != 0)
    return MFX_ERR_NONE;
}
mfxStatus  GetBFractionCode       (mfxU8 &code,Ipp8u nom, Ipp8u denom)
{
    MFX_CHECK_COND(denom < 9 && nom < 8 && denom !=0 && nom != 0)
    code = UMC_VC1_ENCODER::BFractionVLC[denom][nom*2] & 0x1F;
    return MFX_ERR_NONE;
}

mfxStatus GetUMCPictureType(mfxU8 FrameType, mfxU8 FrameType2, bool bField, bool bFrame,
                            mfxU8 Pic4MvAllowed, UMC_VC1_ENCODER::ePType* VC1PicType)
{
    if (bField)
    {
        mfxU16  ftype = (FrameType2<<8)|(FrameType);
        switch (ftype)
        {
        case ((MFX_FRAMETYPE_I | MFX_FRAMETYPE_REF)<<8)|(MFX_FRAMETYPE_I | MFX_FRAMETYPE_REF):
            * VC1PicType = UMC_VC1_ENCODER::VC1_ENC_I_I_FIELD;
            return MFX_ERR_NONE;
        case ((MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF)<<8)|(MFX_FRAMETYPE_I | MFX_FRAMETYPE_REF):
            * VC1PicType = UMC_VC1_ENCODER::VC1_ENC_I_P_FIELD;;
            return MFX_ERR_NONE;
        case ((MFX_FRAMETYPE_I | MFX_FRAMETYPE_REF)<<8)|(MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF):
            * VC1PicType = UMC_VC1_ENCODER::VC1_ENC_P_I_FIELD;;
            return MFX_ERR_NONE;
        case ((MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF)<<8)|(MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF):
            * VC1PicType = UMC_VC1_ENCODER::VC1_ENC_P_P_FIELD;;
            return MFX_ERR_NONE;
        case ((MFX_FRAMETYPE_B)<<8)|(MFX_FRAMETYPE_B):
            * VC1PicType = UMC_VC1_ENCODER::VC1_ENC_B_B_FIELD;;
            return MFX_ERR_NONE;
         case ((MFX_FRAMETYPE_I)<<8)|(MFX_FRAMETYPE_B):
            * VC1PicType = UMC_VC1_ENCODER::VC1_ENC_B_BI_FIELD;;
            return MFX_ERR_NONE;
         case ((MFX_FRAMETYPE_B)<<8)|(MFX_FRAMETYPE_I):
            * VC1PicType = UMC_VC1_ENCODER::VC1_ENC_BI_B_FIELD;;
            return MFX_ERR_NONE;
         case ((MFX_FRAMETYPE_I)<<8)|(MFX_FRAMETYPE_I):
            * VC1PicType = UMC_VC1_ENCODER::VC1_ENC_BI_BI_FIELD;;
            return MFX_ERR_NONE;
       default:
           return  MFX_ERR_UNSUPPORTED;
        }

    }
    else if (bFrame)
    {
        return MFX_ERR_UNSUPPORTED;
    }
    else
    {
        switch (FrameType)
        {
        case MFX_FRAMETYPE_I | MFX_FRAMETYPE_REF:
            * VC1PicType = UMC_VC1_ENCODER::VC1_ENC_I_FRAME;
            return MFX_ERR_NONE;
        case MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF:
        case MFX_FRAMETYPE_P:
            //if(!Pic4MvAllowed)
            * VC1PicType = UMC_VC1_ENCODER::VC1_ENC_P_FRAME;
            //else
            //* VC1PicType = UMC_VC1_ENCODER::VC1_ENC_P_FRAME_MIXED;
            return MFX_ERR_NONE;
        case MFX_FRAMETYPE_B:
            * VC1PicType = UMC_VC1_ENCODER::VC1_ENC_B_FRAME;
            return MFX_ERR_NONE;
       case MFX_FRAMETYPE_I:
            * VC1PicType = UMC_VC1_ENCODER::VC1_ENC_BI_FRAME;
            return MFX_ERR_NONE;
       case MFX_FRAMETYPE_S:
            * VC1PicType = UMC_VC1_ENCODER::VC1_ENC_SKIP_FRAME;
            return MFX_ERR_NONE;
       default:
           return  MFX_ERR_UNSUPPORTED;
        }
    }

    return MFX_ERR_UNSUPPORTED;
}
mfxStatus GetMFXPictureType(mfxU8 &FrameType, mfxU8 &FrameType2, bool &bField, bool &bFrame,
                            UMC_VC1_ENCODER::ePType VC1PicType)
{
    bFrame = false;
    switch (VC1PicType)
        {
        case UMC_VC1_ENCODER::VC1_ENC_I_I_FIELD:
            FrameType = MFX_FRAMETYPE_I | MFX_FRAMETYPE_REF;
            FrameType = MFX_FRAMETYPE_I | MFX_FRAMETYPE_REF;
            bField = true;
            return MFX_ERR_NONE;
        case UMC_VC1_ENCODER::VC1_ENC_I_P_FIELD:
            FrameType = MFX_FRAMETYPE_I | MFX_FRAMETYPE_REF;
            FrameType = MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF;
            bField = true;
            return MFX_ERR_NONE;
        case UMC_VC1_ENCODER::VC1_ENC_P_I_FIELD:
            FrameType = MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF;
            FrameType = MFX_FRAMETYPE_I | MFX_FRAMETYPE_REF;
            bField = true;
            return MFX_ERR_NONE;
        case UMC_VC1_ENCODER::VC1_ENC_P_P_FIELD:
            FrameType = MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF;
            FrameType = MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF;
            bField = true;
            return MFX_ERR_NONE;
        case UMC_VC1_ENCODER::VC1_ENC_B_B_FIELD:
            FrameType = MFX_FRAMETYPE_B;
            FrameType = MFX_FRAMETYPE_B;
            bField = true;
            return MFX_ERR_NONE;
        case UMC_VC1_ENCODER::VC1_ENC_B_BI_FIELD:
            FrameType = MFX_FRAMETYPE_B;
            FrameType = MFX_FRAMETYPE_I;
            bField = true;
            return MFX_ERR_NONE;
        case  UMC_VC1_ENCODER::VC1_ENC_BI_B_FIELD:
            FrameType = MFX_FRAMETYPE_I;
            FrameType = MFX_FRAMETYPE_B;
            bField = true;
            return MFX_ERR_NONE;
        case UMC_VC1_ENCODER::VC1_ENC_BI_BI_FIELD:
            FrameType = MFX_FRAMETYPE_I;
            FrameType = MFX_FRAMETYPE_I;
            bField = true;
            return MFX_ERR_NONE;

        case  UMC_VC1_ENCODER::VC1_ENC_I_FRAME:
            FrameType = MFX_FRAMETYPE_I | MFX_FRAMETYPE_REF;
            bField = false;
            return MFX_ERR_NONE;
        case UMC_VC1_ENCODER::VC1_ENC_P_FRAME:
            FrameType = MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF;
            bField = false;
            return MFX_ERR_NONE;
        case UMC_VC1_ENCODER::VC1_ENC_B_FRAME:
            FrameType = MFX_FRAMETYPE_B;
            bField = false;
            return MFX_ERR_NONE;
        case UMC_VC1_ENCODER::VC1_ENC_BI_FRAME:
            FrameType = MFX_FRAMETYPE_I;
            bField = false;
            return MFX_ERR_NONE;
        case UMC_VC1_ENCODER::VC1_ENC_SKIP_FRAME:
            FrameType = MFX_FRAMETYPE_S;
            bField = false;
            return MFX_ERR_NONE;
        default:
            return  MFX_ERR_UNSUPPORTED;
        }

    return MFX_ERR_UNSUPPORTED;
}
/*mfxI16  GetAdditionalFrameIndex (ExtVC1FrameLabel* pS, mfxU8 frameLabel, mfxU8 &reused)
{
    mfxI16  index = -1;
    reused = 0;

    for (mfxI32 i=0; i<pS->numIndexes; i++)
    {
        if (pS->pFrameLabel->frameLabel == frameLabel)
        {
            index  = pS->pFrameLabel[i].additionalIndex;
            reused = pS->pFrameLabel[i].isReused;
            break;
        }
    }
    return index;
}*/

mfxStatus    GetUMCProfile(mfxU8 MFXprofile, Ipp32s* UMCprofile)
{
    mfxStatus MFXSts = MFX_ERR_NONE;

    switch(MFXprofile)
    {
        case MFX_PROFILE_VC1_ADVANCED:
            *UMCprofile = UMC_VC1_ENCODER::VC1_ENC_PROFILE_A;
            break;
        case MFX_PROFILE_VC1_MAIN:
            *UMCprofile = UMC_VC1_ENCODER::VC1_ENC_PROFILE_M;
            break;
        case MFX_PROFILE_VC1_SIMPLE:
            *UMCprofile = UMC_VC1_ENCODER::VC1_ENC_PROFILE_S;
            break;
        default:
            assert(0);
            MFXSts = MFX_ERR_UNSUPPORTED;
            break;
    }

    return MFXSts;
}

mfxStatus  GetUMCLevel(mfxU8 MFXprofile, mfxU8 MFXlevel, Ipp32s* UMClevel)
{
    mfxStatus       MFXSts = MFX_ERR_NONE;

    switch (MFXprofile)
    {
        case MFX_PROFILE_VC1_ADVANCED:
                switch(MFXlevel)
                {
                    case MFX_LEVEL_VC1_4:
                        *UMClevel = UMC_VC1_ENCODER::VC1_ENC_LEVEL_4;
                        break;
                    case MFX_LEVEL_VC1_3:
                        *UMClevel = UMC_VC1_ENCODER::VC1_ENC_LEVEL_3;
                        break;
                    case MFX_LEVEL_VC1_2:
                        *UMClevel = UMC_VC1_ENCODER::VC1_ENC_LEVEL_2;
                        break;
                    case MFX_LEVEL_VC1_1:
                        *UMClevel = UMC_VC1_ENCODER::VC1_ENC_LEVEL_1;
                        break;
                    case MFX_LEVEL_VC1_0:
                        *UMClevel = UMC_VC1_ENCODER::VC1_ENC_LEVEL_0;
                        break;
                    default:
                        assert(0);
                        MFXSts = MFX_ERR_UNSUPPORTED;
                        break;
                }
            break;
        case MFX_PROFILE_VC1_MAIN:
                switch(MFXlevel)
                {
                    case MFX_LEVEL_VC1_HIGH:
                        *UMClevel = UMC_VC1_ENCODER::VC1_ENC_LEVEL_H;
                        break;
                    case MFX_LEVEL_VC1_MEDIAN:
                        *UMClevel = UMC_VC1_ENCODER::VC1_ENC_LEVEL_M;
                        break;
                    case MFX_LEVEL_VC1_LOW:
                        *UMClevel = UMC_VC1_ENCODER::VC1_ENC_LEVEL_S;
                        break;
                    default:
                        assert(0);
                        MFXSts = MFX_ERR_UNSUPPORTED;
                        break;
                }
            break;
        case MFX_PROFILE_VC1_SIMPLE:
                switch(MFXlevel)
                {
                    case MFX_LEVEL_VC1_MEDIAN:
                        *UMClevel = UMC_VC1_ENCODER::VC1_ENC_LEVEL_M;
                        break;
                    case MFX_LEVEL_VC1_LOW:
                        *UMClevel = UMC_VC1_ENCODER::VC1_ENC_LEVEL_S;
                        break;
                    default:
                        assert(0);
                        MFXSts = MFX_ERR_UNSUPPORTED;
                        break;
                }
            break;
        default:
            assert(0);
            MFXSts = MFX_ERR_UNSUPPORTED;
            break;
    }
    return MFXSts;
}

mfxStatus     GetMFXProfile(Ipp8u UMCprofile, mfxU8* MFXprofile )
{
    mfxStatus       MFXSts = MFX_ERR_NONE;

    switch(UMCprofile)
    {
        case UMC_VC1_ENCODER::VC1_ENC_PROFILE_S:
            *MFXprofile = MFX_PROFILE_VC1_SIMPLE;
            break;
        case UMC_VC1_ENCODER::VC1_ENC_PROFILE_M:
            *MFXprofile = MFX_PROFILE_VC1_MAIN;
            break;
        case UMC_VC1_ENCODER::VC1_ENC_PROFILE_A:
            *MFXprofile = MFX_PROFILE_VC1_ADVANCED;
            break;
        default:
            assert(0);
            MFXSts = MFX_ERR_UNSUPPORTED;
            break;
    }

    return MFXSts;
}

mfxStatus     GetMFXLevel(Ipp8u UMCprofile, Ipp8u UMClevel, mfxU8* MFXlevel)
{
    mfxStatus       MFXSts = MFX_ERR_NONE;

    switch (UMCprofile)
    {
        case UMC_VC1_ENCODER::VC1_ENC_PROFILE_A:
                switch(UMClevel)
                {
                    case UMC_VC1_ENCODER::VC1_ENC_LEVEL_4:
                        *MFXlevel = MFX_LEVEL_VC1_4;
                        break;
                    case UMC_VC1_ENCODER::VC1_ENC_LEVEL_3:
                        *MFXlevel = MFX_LEVEL_VC1_3;
                        break;
                    case UMC_VC1_ENCODER::VC1_ENC_LEVEL_2:
                        *MFXlevel = MFX_LEVEL_VC1_2;
                        break;
                    case UMC_VC1_ENCODER::VC1_ENC_LEVEL_1:
                        *MFXlevel = MFX_LEVEL_VC1_1;
                        break;
                    case UMC_VC1_ENCODER::VC1_ENC_LEVEL_0:
                        *MFXlevel = MFX_LEVEL_VC1_0;
                        break;
                    default:
                        assert(0);
                        MFXSts = MFX_ERR_UNSUPPORTED;
                        break;
                }
            break;
        case UMC_VC1_ENCODER::VC1_ENC_PROFILE_M:
                switch(UMClevel)
                {
                    case UMC_VC1_ENCODER::VC1_ENC_LEVEL_H:
                        *MFXlevel = MFX_LEVEL_VC1_HIGH;
                        break;
                    case UMC_VC1_ENCODER::VC1_ENC_LEVEL_M:
                        *MFXlevel = MFX_LEVEL_VC1_MEDIAN;
                        break;
                    case UMC_VC1_ENCODER::VC1_ENC_LEVEL_S:
                        *MFXlevel = MFX_LEVEL_VC1_LOW;
                        break;
                    default:
                        assert(0);
                        MFXSts = MFX_ERR_UNSUPPORTED;
                        break;
                }
            break;
        case UMC_VC1_ENCODER::VC1_ENC_PROFILE_S:
                switch(UMClevel)
                {
                    case UMC_VC1_ENCODER::VC1_ENC_LEVEL_M:
                        *MFXlevel = MFX_LEVEL_VC1_MEDIAN;
                        break;
                    case UMC_VC1_ENCODER::VC1_ENC_LEVEL_S:
                        *MFXlevel = MFX_LEVEL_VC1_LOW;
                        break;
                    default:
                        assert(0);
                        MFXSts = MFX_ERR_UNSUPPORTED;
                        break;
                }
            break;
        default:
            assert(0);
            MFXSts = MFX_ERR_UNSUPPORTED;
            break;
    }

    return MFXSts;
}

Ipp32u CalculateUMC_HRD_BufferSize(mfxU32 MFXSize)
{
    return (MFXSize*MFX_BIT_IN_KB);
}

Ipp32u CalculateUMC_HRD_InitDelay(mfxU32 MFXDelay)
{
    return (MFXDelay*MFX_BIT_IN_KB);
}

mfxU32 CalculateMFX_HRD_BufferSize(Ipp32u UMCSize)
{
    return (UMCSize/(MFX_BIT_IN_KB));
}

mfxU32 CalculateMFX_HRD_InitDelay(Ipp32u UMCDelay)
{
    return (UMCDelay/(MFX_BIT_IN_KB));
}





UMC_VC1_ENCODER::eReferenceFieldType GetRefFieldFlag(mfxU8 NumRefPic, mfxU8 RefFieldPicFlag, mfxU8 bSecond)
{
    UMC_VC1_ENCODER::eReferenceFieldType type;
    if(NumRefPic == 0)
    {
       type = (RefFieldPicFlag)?UMC_VC1_ENCODER::VC1_ENC_REF_FIELD_SECOND:UMC_VC1_ENCODER::VC1_ENC_REF_FIELD_FIRST;
    }
    else
    {
        type = UMC_VC1_ENCODER::VC1_ENC_REF_FIELD_BOTH;
    }
    return type;
}


mfxStatus GetSavedMVFrame (mfxFrameCUC *cuc, mfxI16Pair ** pSavedMV)
{
    mfxFrameParamVC1* pFrameParam = &cuc->FrameParam->VC1;

    Ipp32u       h = (pFrameParam->FrameHinMbMinus1 + 1);
    Ipp32u       w = (pFrameParam->FrameWinMbMinus1 + 1);

    mfxI16 n = GetExBufferIndex(cuc, MFX_CUC_VC1_MVDATA);
    MFX_CHECK_COND (n>=0)

    mfxExtVc1MvData* pMVs = (mfxExtVc1MvData*)(cuc->ExtBuffer[n]);
    MFX_CHECK_COND (pMVs->NumMv >= w*h)
    *pSavedMV = pMVs->Mv ;

    return MFX_ERR_NONE;
}

mfxStatus GetMBRoundControl (mfxFrameCUC *cuc, mfxI8** pRC)
{
    mfxFrameParamVC1* pFrameParam = &cuc->FrameParam->VC1;

    Ipp32u       h = (pFrameParam->FrameHinMbMinus1 + 1);
    Ipp32u       w = (pFrameParam->FrameWinMbMinus1 + 1);

    * pRC = 0;
    mfxI16 n = GetExBufferIndex(cuc,MFX_CUC_VC1_RNDCTRL);
    MFX_CHECK_COND (n>=0)

    mfxExtVc1TrellisRndCtrl* pExtRC = (mfxExtVc1TrellisRndCtrl*)(cuc->ExtBuffer[n]);
    MFX_CHECK_COND (pExtRC->NumMb >= w*h)

    *pRC = pExtRC->RndCtrl;

    return MFX_ERR_NONE;
}
mfxStatus GetSavedMVField (mfxFrameCUC *cuc, mfxI16Pair ** pSavedMV, Ipp8u** pDirection)
{
    mfxFrameParamVC1* pFrameParam = &cuc->FrameParam->VC1;

    Ipp32u       h = (pFrameParam->FrameHinMbMinus1 + 1);
    Ipp32u       w = (pFrameParam->FrameWinMbMinus1 + 1);

    bool bBottom = pFrameParam->BottomFieldFlag;

    mfxI16        n = GetExBufferIndex(cuc,  MFX_CUC_VC1_MVDATA);
    MFX_CHECK_COND (n>=0)

    mfxExtVc1MvData* pMVs = (mfxExtVc1MvData*)(cuc->ExtBuffer[n]);
    MFX_CHECK_COND (pMVs->NumMv >= w*h)
    * pSavedMV = pMVs->Mv;

    n = GetExBufferIndex(cuc, MFX_CUC_VC1_MVDIR);
    MFX_CHECK_COND (n>=0)

    mfxExtVc1MvDir* pRefType = (mfxExtVc1MvDir*)(cuc->ExtBuffer[n]);
    MFX_CHECK_COND (pRefType->NumMv >= w*h)
    * pDirection = pRefType->MvDir;
    return MFX_ERR_NONE;
}

bool isReference(UMC_VC1_ENCODER::ePType UMCtype)
{
    if (UMCtype == UMC_VC1_ENCODER::VC1_ENC_B_FRAME ||
        UMCtype == UMC_VC1_ENCODER::VC1_ENC_BI_FRAME ||
        UMCtype == UMC_VC1_ENCODER::VC1_ENC_B_B_FIELD ||
        UMCtype == UMC_VC1_ENCODER::VC1_ENC_B_BI_FIELD ||
        UMCtype == UMC_VC1_ENCODER::VC1_ENC_BI_B_FIELD ||
        UMCtype == UMC_VC1_ENCODER::VC1_ENC_BI_BI_FIELD)
    {
        return false;
    }
    return true;
}
bool isIntra(UMC_VC1_ENCODER::ePType UMCtype)
{
    if (UMCtype == UMC_VC1_ENCODER::VC1_ENC_I_FRAME ||
        UMCtype == UMC_VC1_ENCODER::VC1_ENC_I_I_FIELD ||
        UMCtype == UMC_VC1_ENCODER::VC1_ENC_BI_FRAME ||
        UMCtype == UMC_VC1_ENCODER::VC1_ENC_BI_BI_FIELD)
    {
        return true;
    }
    else
    {
        return false;
    }

}
bool isPredicted(UMC_VC1_ENCODER::ePType UMCtype)
{
    if (isIntra(UMCtype))
    {
        return false;
    }
    else
    {
        return isBPredicted(UMCtype)? false : true;
    }

}
bool isBPredicted(UMC_VC1_ENCODER::ePType UMCtype)
{
    if (UMCtype == UMC_VC1_ENCODER::VC1_ENC_B_FRAME ||
        UMCtype == UMC_VC1_ENCODER::VC1_ENC_B_B_FIELD ||
        UMCtype == UMC_VC1_ENCODER::VC1_ENC_B_BI_FIELD ||
        UMCtype == UMC_VC1_ENCODER::VC1_ENC_BI_B_FIELD)
    {
        return true;
    }
    else
    {
       return false;
    }
}
bool isField(UMC_VC1_ENCODER::ePType UMCtype)
{
 if (UMCtype == UMC_VC1_ENCODER::VC1_ENC_I_FRAME||
     UMCtype == UMC_VC1_ENCODER::VC1_ENC_P_FRAME ||
     //UMCtype == UMC_VC1_ENCODER::VC1_ENC_P_FRAME_MIXED||
     UMCtype == UMC_VC1_ENCODER::VC1_ENC_B_FRAME||
     UMCtype == UMC_VC1_ENCODER::VC1_ENC_BI_FRAME||
     UMCtype == UMC_VC1_ENCODER::VC1_ENC_SKIP_FRAME)
    {
        return false;
    }
    else
    {
       return true;
    }
}
mfxStatus SetFrameYV12 (mfxU8               index,
                        mfxFrameSurface *   pFrameSurface,
                        mfxVideoParam*      pVideoParam,
                        UMC_VC1_ENCODER::ePType pictureType,
                        UMC_VC1_ENCODER::Frame * pFrame,
                        bool bField)
{
    MFX_CHECK_COND (index< pFrameSurface->Info.NumFrameData);

    Ipp8u* pYPlane = pFrameSurface->Data[index]->Y + GetLumaShift(&pFrameSurface->Info,pFrameSurface->Data[index]);
    Ipp32u stepY   = pFrameSurface->Data[index]->Pitch;
    Ipp8u* pUPlane = pFrameSurface->Data[index]->U + GetChromaShiftYV12(&pFrameSurface->Info, pFrameSurface->Data[index]);
    Ipp32u stepUV  = pFrameSurface->Data[index]->Pitch>>1;
    Ipp8u* pVPlane = pFrameSurface->Data[index]->V + GetChromaShiftYV12(&pFrameSurface->Info, pFrameSurface->Data[index]);

    Ipp32u Width   = pVideoParam->mfx.FrameInfo.Width;
    Ipp32u Height  = pVideoParam->mfx.FrameInfo.Height;

    Ipp32u Width16 = ((Width + 15)>>4)<<4;
    Ipp32u Height16= (bField)? ((Height + 31)>>5)<<5:((Height + 15)>>4)<<4;

    Ipp32u WidthUV  = Width  >> 1;
    Ipp32u HeightUV = Height >> 1;

    mfxI32 padL=0,padR=0, padT=0, padB=0;

    GetPaddingSize (&pFrameSurface->Info,Width16,Height16, padT,padL, padB,padR);

    mfxI32 padding = min4(padT,padL, padB,padR);
    padding = (padding >= 32)? 32 : 0;

    if ((padding >0 || (Width16 - Width)> 0 || (Height16 - Height)>0)&& !pFrameSurface->Data[index]->Paired )
    {
        if (!bField)
        {
            PadFrameProgressiveYV12 ( pYPlane, pUPlane, pVPlane,
                                      Width,  Height, stepY,
                                      padding, padding, padding + (Height16-Height),
                                      padding + (Width16 - Width));
        }
        else
        {
            PadFrameFieldYV12    (pYPlane, pUPlane, pVPlane,
                                  Width,  Height, stepY,
                                  padding, padding, padding + (Height16-Height),
                                  padding + (Width16 - Width));
        }
    }

    pFrame->Init(   pYPlane, stepY,
                    pUPlane, pVPlane, stepUV,
                    Width, WidthUV,
                    Height,HeightUV,
                    padding,
                    pictureType) ;

    return MFX_ERR_NONE;
}
mfxStatus SetFrameNV12 (mfxU8               index,
                        mfxFrameSurface *   pFrameSurface,
                        mfxVideoParam*      pVideoParam,
                        UMC_VC1_ENCODER::ePType pictureType,
                        UMC_VC1_ENCODER::Frame * pFrame,
                        bool bField)
{
    MFX_CHECK_COND (index< pFrameSurface->Info.NumFrameData);

    Ipp8u* pYPlane = pFrameSurface->Data[index]->Y + GetLumaShift(&pFrameSurface->Info,pFrameSurface->Data[index]);
    Ipp32u stepY   = pFrameSurface->Data[index]->Pitch;
    Ipp8u* pUPlane = pFrameSurface->Data[index]->U + GetChromaShiftNV12(&pFrameSurface->Info, pFrameSurface->Data[index]);
    Ipp32u stepUV  = pFrameSurface->Data[index]->Pitch;
    Ipp8u* pVPlane = pFrameSurface->Data[index]->V + GetChromaShiftNV12(&pFrameSurface->Info, pFrameSurface->Data[index]);

    Ipp32u Width   = pVideoParam->mfx.FrameInfo.Width;
    Ipp32u Height  = pVideoParam->mfx.FrameInfo.Height;

    Ipp32u Width16 = ((Width + 15)>>4)<<4;
    Ipp32u Height16= (bField)? ((Height + 31)>>5)<<5:((Height + 15)>>4)<<4;

    Ipp32u WidthUV = Width  >> 1;
    Ipp32u HeightUV= Height >> 1;

    mfxI32 padL = 0, padR = 0, padT = 0, padB = 0;

    GetPaddingSize (&pFrameSurface->Info,Width16,Height16, padT,padL, padB,padR);
    mfxI32 padding = min4(padT,padL, padB,padR);

    padding = (padding >= 32)? 32 : 0;

    if ((padding >0 || (Width16 - Width)> 0 || (Height16 - Height)>0)&& !pFrameSurface->Data[index]->Paired )
    {
        if (!bField)
        {
            PadFrameProgressiveNV12(pYPlane, pUPlane, pVPlane,
                                    Width,  Height, stepY,
                                    padding, padding, padding + (Height16-Height),
                                    padding + (Width16 - Width));
        }
        else
        {
            PadFrameFieldNV12(  pYPlane, pUPlane, pVPlane,
                                Width,  Height, stepY,
                                padding, padding, padding + (Height16-Height),
                                padding + (Width16 - Width));
        }
    }

    pFrame->Init(   pYPlane, stepY,
                    pUPlane, pVPlane, stepUV,
                    Width, WidthUV,
                    Height,HeightUV,
                    padding,
                    pictureType) ;

    return MFX_ERR_NONE;
}

//--------------Padding-------------------------------------------------------
IppStatus   _own_ippiReplicateBorder_8u_C1R  (  Ipp8u * pSrc,  int srcStep,
                                              IppiSize srcRoiSize, IppiSize dstRoiSize,
                                              int topBorderWidth,  int leftBorderWidth)
{
    // upper
    Ipp8u* pBlock = pSrc;
    for (int i = 0; i < topBorderWidth; i++)
    {
        MFX_INTERNAL_CPY(pBlock - srcStep, pBlock, srcRoiSize.width);
        pBlock -= srcStep;
    }

    // bottom
    pBlock = pSrc + (srcRoiSize.height  - 1)*srcStep;

    for (int i = 0; i < dstRoiSize.height - srcRoiSize.height - topBorderWidth; i++ )
    {
        MFX_INTERNAL_CPY(pBlock + srcStep, pBlock, srcRoiSize.width);
        pBlock += srcStep;
    }

    // left
    pBlock = pSrc - srcStep * topBorderWidth;

    for (int i = 0; i < dstRoiSize.height; i++)
    {
        memset(pBlock-leftBorderWidth,pBlock[0],leftBorderWidth);
        pBlock += srcStep;
    }

    //right

    pBlock = pSrc - srcStep * topBorderWidth + srcRoiSize.width - 1;
    Ipp32s rightY = dstRoiSize.width - srcRoiSize.width - leftBorderWidth;
    rightY = (rightY>0)? rightY : 0;

    for (int i = 0; i < dstRoiSize.height; i++)
    {
        memset(pBlock+1,pBlock[0],rightY);
        pBlock += srcStep;
    }
    return ippStsNoErr;
}

IppStatus   _own_ippiReplicateBorder_16u_C1R  ( Ipp16u * pSrc,  int srcStep,
                                               IppiSize srcRoiSize, IppiSize dstRoiSize,
                                               int topBorderWidth,  int leftBorderWidth)
{
    // upper
    Ipp8u* pBlock = (Ipp8u*) pSrc;
    for (int i = 0; i < topBorderWidth; i++)
    {
        MFX_INTERNAL_CPY(pBlock - srcStep, pBlock, srcRoiSize.width*sizeof(Ipp16u));
        pBlock -= srcStep;
    }

    // bottom
    pBlock = (Ipp8u*)pSrc + (srcRoiSize.height  - 1)*srcStep;

    for (int i = 0; i < dstRoiSize.height - srcRoiSize.height - topBorderWidth; i++ )
    {
        MFX_INTERNAL_CPY(pBlock + srcStep, pBlock, srcRoiSize.width*sizeof(Ipp16u));
        pBlock += srcStep;
    }

    // left
    pBlock = (Ipp8u*)(pSrc) - srcStep * topBorderWidth;

    for (int i = 0; i < dstRoiSize.height; i++)
    {
        ippsSet_16s (*(Ipp16s*)pBlock, (Ipp16s*)pBlock-leftBorderWidth,leftBorderWidth);
        pBlock += srcStep;
    }

    //right

    pBlock = (Ipp8u*)(pSrc + srcRoiSize.width - 1) - srcStep * topBorderWidth ;
    Ipp32s rightY = dstRoiSize.width - srcRoiSize.width - leftBorderWidth;
    rightY = (rightY>0)? rightY : 0;

    for (int i = 0; i < dstRoiSize.height; i++)
    {
        ippsSet_16s (*(Ipp16s*)pBlock, ((Ipp16s*)pBlock)+ 1,rightY);
        pBlock += srcStep;
    }
    return ippStsNoErr;
}

void ReplicateBorderChroma_YV12 (Ipp8u* pU,Ipp8u* pV, Ipp32u stepUV,IppiSize srcRoiSizeUV, IppiSize dstRoiSizeUV,
                                        int topBorderWidth,  int leftBorderWidth)
{
    _own_ippiReplicateBorder_8u_C1R  (pU, stepUV, srcRoiSizeUV,dstRoiSizeUV,topBorderWidth,leftBorderWidth);
    _own_ippiReplicateBorder_8u_C1R  (pV, stepUV, srcRoiSizeUV,dstRoiSizeUV,topBorderWidth,leftBorderWidth);
}

void ReplicateBorderChroma_NV12 (Ipp8u* pUV,Ipp8u* /*pV*/, Ipp32u stepUV,IppiSize srcRoiSizeUV, IppiSize dstRoiSizeUV,
                                 int topBorderWidth,  int leftBorderWidth)
{
    _own_ippiReplicateBorder_16u_C1R  ((Ipp16u*)pUV, stepUV, srcRoiSizeUV,dstRoiSizeUV,topBorderWidth,leftBorderWidth);
}






mfxStatus   CheckCucSeqHeaderAdv(ExtVC1SequenceParams * m_pSH, UMC_VC1_ENCODER::VC1EncoderSequenceADV*   m_pVC1SH)
{
    mfxStatus MFXSts  = MFX_ERR_NONE;

    //assert(0);

    return MFXSts;
}

mfxStatus   CheckCucSeqHeaderSM (ExtVC1SequenceParams * m_pSH, UMC_VC1_ENCODER::VC1EncoderSequenceSM*    m_pVC1SH)
{
    mfxStatus MFXSts  = MFX_ERR_NONE;

    //assert(0);

    return MFXSts;
}

mfxStatus GetMFXTransformType(Ipp32s UMCTSType, mfxU8* MFXLuma, mfxU8* MFXU, mfxU8* MFXV)
{
    mfxStatus MFXSts  = MFX_ERR_NONE;

    *MFXLuma = UMCTSType & 0xFF; //Transform type for luma:   ((Y3<<6)| (Y2<<4)| (Y1<<2) | Y0)
                                                                         //Values are on the table on p. 89
    *MFXU  = (UMCTSType >> 4*2) & 0x3;
    *MFXV  = (UMCTSType >> 5*2) & 0x3;

    return MFXSts;
}

mfxStatus GetMFXTransformType(UMC_VC1_ENCODER::eTransformType* tsType, mfxU8* MFXLuma, mfxU8* MFXU, mfxU8* MFXV)
{
    mfxStatus MFXSts  = MFX_ERR_NONE;

    *MFXLuma = (tsType[0] <<6) | (tsType[1] <<4) | (tsType[2] <<2) | (tsType[3]); //Transform type for luma:   ((Y3<<6)| (Y2<<4)| (Y1<<2) | Y0)
                                                                         //Values are on the table on p. 89
    *MFXU  = tsType[4];
    *MFXV  = tsType[5];

    return MFXSts;
}

mfxStatus GetUMCTransformType(mfxU8 MFXLuma, mfxU8 MFXU, mfxU8 MFXV, Ipp32s* UMCTSType)
{
    mfxStatus MFXSts  = MFX_ERR_NONE;

    *UMCTSType = MFXLuma | (MFXU << 2*4) | (MFXV << 2*5);

    return MFXSts;
}

mfxStatus GetUMCTransformType(mfxU8 MFXLuma, mfxU8 MFXU, mfxU8 MFXV, UMC_VC1_ENCODER::eTransformType* tsType)
{
    mfxStatus MFXSts  = MFX_ERR_NONE;

    tsType[0] = (UMC_VC1_ENCODER::eTransformType)((MFXLuma>>6) & 0x3);
    tsType[1] = (UMC_VC1_ENCODER::eTransformType)((MFXLuma>>4) & 0x3);
    tsType[2] = (UMC_VC1_ENCODER::eTransformType)((MFXLuma>>2) & 0x3);
    tsType[3] = (UMC_VC1_ENCODER::eTransformType)((MFXLuma) & 0x3);

    tsType[4] = (UMC_VC1_ENCODER::eTransformType)(MFXU);
    tsType[5] = (UMC_VC1_ENCODER::eTransformType)(MFXV);

    return MFXSts;
}

mfxStatus GetMFXIntraPattern(UMC::MeMB *pMECurrMB, mfxU8* SubMbPredMode)
{
    mfxStatus MFXSts  = MFX_ERR_NONE;

    mfxU8  intraPattern = (mfxU8)((UMC::ME_MbIntra == pMECurrMB->BlockType[0])<<VC_ENC_PATTERN_POS(0)|
                                  (UMC::ME_MbIntra == pMECurrMB->BlockType[1])<<VC_ENC_PATTERN_POS(1)|
                                  (UMC::ME_MbIntra == pMECurrMB->BlockType[2])<<VC_ENC_PATTERN_POS(2)|
                                  (UMC::ME_MbIntra == pMECurrMB->BlockType[3])<<VC_ENC_PATTERN_POS(3));

    *SubMbPredMode = intraPattern;
    return MFXSts;
}

mfxStatus GetUMCIntraPattern(mfxU8 SubMbPredMode, UMC::MeMB *pMECurrMB)
{
    mfxStatus MFXSts  = MFX_ERR_NONE;


    pMECurrMB->BlockType[0] = ((SubMbPredMode & (1<<VC_ENC_PATTERN_POS(0)))!=0)? UMC::ME_MbIntra : UMC::ME_MbFrw;
    pMECurrMB->BlockType[1] = ((SubMbPredMode & (1<<VC_ENC_PATTERN_POS(1)))!=0)? UMC::ME_MbIntra : UMC::ME_MbFrw;
    pMECurrMB->BlockType[2] = ((SubMbPredMode & (1<<VC_ENC_PATTERN_POS(2)))!=0)? UMC::ME_MbIntra : UMC::ME_MbFrw;
    pMECurrMB->BlockType[3] = ((SubMbPredMode & (1<<VC_ENC_PATTERN_POS(3)))!=0)? UMC::ME_MbIntra : UMC::ME_MbFrw;

    return MFXSts;
}

mfxStatus GetMFXHybrid(UMC_VC1_ENCODER::VC1EncoderCodedMB*  pCompressedMB, mfxU8* pMFXHybrid)
{
    mfxStatus MFXSts  = MFX_ERR_NONE;
    mfxU8 hybrid = 0;

    hybrid = (pCompressedMB->GetHybrid(0)<<(3*2)) | (pCompressedMB->GetHybrid(1)<<(2*2)) |
             (pCompressedMB->GetHybrid(2)<<(1*2)) | (pCompressedMB->GetHybrid(3));

    *pMFXHybrid = hybrid;

    return MFXSts;
}
mfxStatus GetUMCHybrid(mfxU8 pMFXHybrid, UMC_VC1_ENCODER::VC1EncoderCodedMB*  pCompressedMB)
{
    mfxStatus MFXSts  = MFX_ERR_NONE;

    mfxU8 hybrid = pMFXHybrid;

    pCompressedMB->SetHybrid((hybrid>>(3*2))& 0x3, 0);
    pCompressedMB->SetHybrid((hybrid>>(2*2))& 0x3, 1);
    pCompressedMB->SetHybrid((hybrid>>(1*2))& 0x3, 2);
    pCompressedMB->SetHybrid((hybrid)& 0x3, 3);

    return MFXSts;
}

mfxStatus GetMFX_CBPCY(Ipp8u UMC_CPBCY, mfxU16* CBPCY_Y, mfxU16* CBPCY_U, mfxU16* CBPCY_V)
{
    mfxStatus MFXSts  = MFX_ERR_NONE;

    *CBPCY_Y = UMC_CPBCY>>2;
    *CBPCY_U = (UMC_CPBCY>>1) & 0x1;
    *CBPCY_V = UMC_CPBCY & 0x1;

    return MFXSts;
}

Ipp8u GetUMC_CBPCY(mfxU16 CBPCY_Y, mfxU16 CBPCY_U, mfxU16 CBPCY_V)
{
    Ipp8u UMC_CPBCY = 0;

    UMC_CPBCY = (CBPCY_Y << 2) | (CBPCY_U <<1) | CBPCY_V;

    return UMC_CPBCY;
}

mfxStatus GetFirstLastMB (mfxFrameCUC *cuc, Ipp32s &firstMB, Ipp32s &lastMB, bool oneSlice)
{
    mfxU16 Id = (oneSlice)? 0:cuc->SliceId;
    mfxSliceParamVC1 *pSlice = &cuc->SliceParam[Id].VC1;
    MFX_CHECK_NULL_PTR1(pSlice)

    firstMB = pSlice->FirstMbX + pSlice->FirstMbY*(cuc->FrameParam->VC1.FrameWinMbMinus1 + 1);
    lastMB = firstMB + pSlice->NumMb - 1;

    return MFX_ERR_NONE;
}

mfxStatus GetSliceRows (mfxFrameCUC *cuc, mfxI32 &firstRow, mfxI32 &nRows, bool oneSlice)
{
    mfxU16 Id = (oneSlice)? 0:cuc->SliceId;

    mfxSliceParamVC1 *pSlice = &cuc->SliceParam[Id].VC1;
    MFX_CHECK_NULL_PTR1(pSlice)

    firstRow = pSlice->FirstMbY;
    nRows = pSlice->NumMb/(cuc->FrameParam->VC1.FrameWinMbMinus1 + 1);

    return MFX_ERR_NONE;
}

void*  GetExBufferIndex (mfxVideoParam *par, mfxU32 cucID)
{
    void* ptr = NULL;

    if (par->VideoParamSz >= sizeof (mfxVideoParam2) + sizeof (ExtVC1SequenceParams))
    {
        mfxVideoParam2 * par2 = (mfxVideoParam2 *)par;
        if (par2->ExtBuffer)
        {
            for (mfxI32 i=0; i < par2->NumExtBuffer; i ++)
            {
                if (((mfxU32*)(par2->ExtBuffer[i]))[0] == cucID)
                {
                    ptr = par2->ExtBuffer[i];
                    break;
                }
            }
        }
    }
    return ptr;
}
void CorrectMfxVideoParams(mfxVideoParam *out)
{
    mfxStatus                 MFXSts      = MFX_ERR_NONE;
    Ipp32s                    umcProfile  = 0;
    Ipp32s                    umcLevel    = 0;
    Ipp32s                    umcLevel1   = 0;
    Ipp32u                    bitrate     = CalculateUMCBitrate(out->mfx.TargetKbps);
    Ipp32u                    w           = out->mfx.FrameInfo.Width;
    Ipp32u                    h           = out->mfx.FrameInfo.Height;

    // check profile and level
    MFXSts = GetUMCProfile(out->mfx.CodecProfile, &umcProfile);
    if(MFXSts != MFX_ERR_NONE)
    {
        umcProfile = UMC_VC1_ENCODER::VC1_ENC_PROFILE_A;
    }
    GetMFXProfile(umcProfile, &out->mfx.CodecProfile);

    MFXSts = GetUMCLevel(out->mfx.CodecProfile, out->mfx.CodecLevel, &umcLevel);
    if(MFXSts != MFX_ERR_NONE)
    {
        umcLevel = 0;
    }
    umcLevel1 = UMC_VC1_ENCODER::VC1BitRateControl::GetLevel(umcProfile,bitrate, w, h);
    if (umcLevel1 < umcLevel)
    {
        umcLevel = umcLevel1;
    }
    GetMFXLevel(umcProfile,umcLevel,&out->mfx.CodecLevel);

    out->mfx.FrameInfo.ChromaFormat  = MFX_CHROMAFORMAT_YUV420;

    if (out->mfx.FrameInfo.FourCC != MFX_FOURCC_YV12 || out->mfx.FrameInfo.FourCC != MFX_FOURCC_NV12)
    {
        out->mfx.FrameInfo.FourCC = MFX_FOURCC_YV12;
    }
    if (out->mfx.FrameInfo.Width == 0)
    {
        out->mfx.FrameInfo.Width = 720;
    }
    if (out->mfx.FrameInfo.Height == 0)
    {
        out->mfx.FrameInfo.Height = 480;
    }

    if (out->mfx.FrameInfo.Width > 4080)
    {
        out->mfx.FrameInfo.Width = 4080;
    }
    if (out->mfx.FrameInfo.Height > 4080)
    {
        out->mfx.FrameInfo.Height = 4080;
    }

    out->mfx.FrameInfo.PicStruct = 0;
    out->mfx.FrameInfo.AspectRatio = 0;

    if (out->mfx.FrameInfo.FrameRateCode == 15)
    {
        out->mfx.FrameInfo.FrameRateCode = MFX_FRAMERATE_239;
    }
    out->mfx.FrameInfo.CropX =  0;
    out->mfx.FrameInfo.CropY =  0;
    out->mfx.FrameInfo.CropW =  out->mfx.FrameInfo.Width;
    out->mfx.FrameInfo.CropH =  out->mfx.FrameInfo.Height;


    out->mfx.BufferSizeInKB = 0;

    if (out->mfx.GopPicSize == 0)
    {
        out->mfx.GopPicSize = 1;
    }
    if (out->mfx.GopPicSize<out->mfx.GopRefDist)
    {
        out->mfx.GopRefDist = out->mfx.GopPicSize;
    }
    if (out->mfx.GopRefDist>7)
    {
        out->mfx.GopRefDist = 7;
    }
    if ((out->mfx.TargetUsage & 0x0f)>MFX_TARGETUSAGE_BEST_SPEED  ||
        (out->mfx.TargetUsage & 0xf0)>MFX_TARGETUSAGE_MULTIPASS)
    {
        out->mfx.TargetUsage = 3;
    }

    out->mfx.CodecId = MFX_MAKEFOURCC ('V','C','1',' ');
    out->mfx.RateControlMethod = 0;
    out->mfx.InitialDelayInKB = 0;
    out->mfx.BufferSizeInKB = 0;
    out->mfx.MaxKbps = 0;
    out->mfx.NumThread = 0;

    // --------- codec limitation -------------------//

    out->mfx.CodingLimitSet = 0;
    out->mfx.CAVLC = 1;
    out->mfx.Color420Only   = 1;


    if (umcProfile == UMC_VC1_ENCODER::VC1_ENC_PROFILE_M)
    {
        out->mfx.OneSliceOnly   = 1;
        out->mfx.GopRefDist     = (out->mfx.GopRefDist>7)? 7:out->mfx.GopRefDist;
    }
    if (umcProfile == UMC_VC1_ENCODER::VC1_ENC_PROFILE_S)
    {
        out->mfx.OneSliceOnly   = 1;
        out->mfx.SliceIPOnly    = 1;
        out->mfx.GopRefDist     = 1;
    }
    out->mfx.RowSliceOnly = 1;
}
void SetMfxVideoParams(mfxVideoParam *out, bool bAdvance)
{
    if (bAdvance)
    {
        out->mfx.CodecProfile = MFX_PROFILE_VC1_ADVANCED;
        out->mfx.CodecLevel = MFX_LEVEL_VC1_4;
    }
    else
    {
        out->mfx.CodecProfile = MFX_PROFILE_VC1_MAIN;
        out->mfx.CodecLevel   = MFX_LEVEL_VC1_HIGH;
    }
    out->mfx.FrameInfo.Width = 720;
    out->mfx.FrameInfo.Height = 480;

    out->mfx.FrameInfo.CropW =  out->mfx.FrameInfo.Width;
    out->mfx.FrameInfo.CropH =  out->mfx.FrameInfo.Height;

    out->mfx.FrameInfo.ChromaFormat  = MFX_CHROMAFORMAT_YUV420;
    out->mfx.FrameInfo.FrameRateCode = MFX_FRAMERATE_239;
    out->mfx.FrameInfo.FrameRateExtN = 1;
    out->mfx.FrameInfo.FrameRateExtD = 1;


    out->mfx.GopPicSize = 15;
    out->mfx.GopRefDist = 5;
    out->mfx.TargetUsage = 3;
    out->mfx.NumSlice = 1;
    out->mfx.TargetKbps = 2000;

    CorrectMfxVideoParams(out);
}
#endif //MFX_ENABLE_VC1_VIDEO_ENCODER
