//* ////////////////////////////////////////////////////////////////////////////// */
//*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2010-2013 Intel Corporation. All Rights Reserved.
//
//
//*/


#include "vc1_spl_header_parser.h"
#include <assert.h>
#include <string.h>

namespace ProtectedLibrary
{

    void SwapData(mfxU8 *src, mfxU8 *dst, mfxU32 dataSize)
    {
        mfxU32 i = 0;
        mfxU32 zeroNum = 0;
        mfxU32 counter = 0;
        mfxU32* pDst = (mfxU32*)dst;
        mfxU8* pSwbuff = dst;
        mfxU8* pSrc = src;
        mfxU32  iCur = 0;

        for(i = 0; i < dataSize; i++)
        {
            if((zeroNum == 2) && (*pSrc == 0x03))
            {
                zeroNum = 0;
                pSrc++;
            }
            else
            {
                 if(*pSrc==0)
                    zeroNum++;
                 else
                    zeroNum = 0;

                 *pSwbuff = *pSrc;
                pSwbuff++;
                pSrc++;
            }



        }

        for(i = 0; i < dataSize+4; i++)
        {
            if (4 == counter)
            {
                counter = 0;
                *pDst = iCur;
                pDst++;
                iCur = 0;
            }

            if (0 == counter)
                iCur = dst[i];
            iCur <<= 8;
            iCur |= dst[i];
            ++counter;
        }
    }

    VC1_SPL_Headers::VC1_SPL_Headers()
    {
        Init();
    }

    VC1_SPL_Headers::~VC1_SPL_Headers()
    {
    }

    mfxStatus VC1_SPL_Headers::Init()
    {
        SHInit = 0;
        memset(&SH,0,sizeof(VC1SequenceHeader));
        memset(&PicH,0,sizeof(VC1PictureLayerHeader));

        m_bs.pBitstream = NULL;
        m_bs.bitOffset  = 0;

        return MFX_ERR_NONE;
    }



    mfxStatus VC1_SPL_Headers::Close()
    {
        return MFX_ERR_NONE;
    }

    mfxStatus VC1_SPL_Headers::Reset()
    {
        SHInit = 0;
        memset(&SH,0,sizeof(VC1SequenceHeader));
        memset(&PicH,0,sizeof(VC1PictureLayerHeader));
        return MFX_ERR_NONE;
    }

    mfxStatus VC1_SPL_Headers::ParseSeqHeader(mfxU32* bs, mfxU32* len, mfxU32 dataSize)
    {
        mfxU32 reserved = 0;
        mfxU32 i = 0;
        mfxU32 tempValue = 0;

        SHInit = 0;
        mfxU32* pBeging = PrepareData(bs, dataSize);

        VC1_SPL_GET_BITS(2, SH.PROFILE);

        if(SH.PROFILE == VC1_PROFILE_ADVANCED)
        {
            VC1_SPL_GET_BITS(3, SH.LEVEL);
            VC1_SPL_GET_BITS(2,tempValue);     //CHROMAFORMAT
            VC1_SPL_GET_BITS(3, SH.FRMRTQ_POSTPROC);
            VC1_SPL_GET_BITS(5, SH.BITRTQ_POSTPROC);
            VC1_SPL_GET_BITS(1, SH.POSTPROCFLAG);
            VC1_SPL_GET_BITS(12, SH.MAX_CODED_WIDTH);
            VC1_SPL_GET_BITS(12, SH.MAX_CODED_HEIGHT);
            VC1_SPL_GET_BITS(1, SH.PULLDOWN);
            VC1_SPL_GET_BITS(1, SH.INTERLACE);
            VC1_SPL_GET_BITS(1, SH.TFCNTRFLAG);
            VC1_SPL_GET_BITS(1, SH.FINTERPFLAG);
            VC1_SPL_GET_BITS(2, reserved);
            VC1_SPL_GET_BITS(1, tempValue);//DISPLAY_EXT

            SH.widthMB = (mfxU16)(((SH.MAX_CODED_WIDTH+1)*2+15)/16);
            SH.heightMB = (mfxU16)(((SH.MAX_CODED_HEIGHT+1)*2+15)/16);
            m_pBitplane.resize(SH.heightMB*SH.widthMB*VC1_MAX_BITPANE_CHUNCKS);
            SH.pBitplane.m_databits = &m_pBitplane[0];


            if(tempValue)//DISPLAY_EXT
            {
                VC1_SPL_GET_BITS(14,tempValue);     //DISP_HORIZ_SIZE
                VC1_SPL_GET_BITS(14,tempValue);     //DISP_VERT_SIZE

                VC1_SPL_GET_BITS(1,tempValue); //ASPECT_RATIO_FLAG

                if(tempValue)
                {
                    VC1_SPL_GET_BITS(4,tempValue);        //ASPECT_RATIO

                    if(tempValue==15)
                    {
                        VC1_SPL_GET_BITS(8, SH.AspectRatioW);        //ASPECT_HORIZ_SIZE
                        VC1_SPL_GET_BITS(8, SH.AspectRatioH);      //ASPECT_VERT_SIZE
                    }
                    else
                    {
                        SH.AspectRatioW = 0;
                        SH.AspectRatioH = 0;
                    }
                }

                VC1_SPL_GET_BITS(1,tempValue);      //FRAMERATE_FLAG

                if(tempValue)       //FRAMERATE_FLAG
                {
                    VC1_SPL_GET_BITS(1,tempValue);    //FRAMERATEIND

                    if(!tempValue)      //FRAMERATEIND
                    {
                        VC1_SPL_GET_BITS(8, SH.FRAMERATENR);      //FRAMERATENR

                        VC1_SPL_GET_BITS(4,SH.FRAMERATEDR);  //FRAMERATEDR
                    }
                    else
                    {
                        VC1_SPL_GET_BITS(16,tempValue);     //FRAMERATEEXP
                    }

                }

                VC1_SPL_GET_BITS(1,tempValue);      //COLOR_FORMAT_FLAG

                if(tempValue)       //COLOR_FORMAT_FLAG
                {
                    VC1_SPL_GET_BITS(8,tempValue);        //COLOR_PRIM
                    VC1_SPL_GET_BITS(8,tempValue);        //TRANSFER_CHAR
                    VC1_SPL_GET_BITS(8,tempValue);        //MATRIX_COEF
                }

                VC1_SPL_GET_BITS(1, SH.HRD_PARAM_FLAG);
                if(SH.HRD_PARAM_FLAG)
                {
                    VC1_SPL_GET_BITS(5,SH.HRD_NUM_LEAKY_BUCKETS);
                    VC1_SPL_GET_BITS(4,tempValue);//BIT_RATE_EXPONENT
                    VC1_SPL_GET_BITS(4,tempValue);//BUFFER_SIZE_EXPONENT

                    for(i=0; i<SH.HRD_NUM_LEAKY_BUCKETS; i++)
                    {
                        VC1_SPL_GET_BITS(16,tempValue);//HRD_RATE[i]
                        VC1_SPL_GET_BITS(16,tempValue);//HRD_BUFFER[i]
                    }
                }

            }

            *len = (mfxU32)(sizeof(mfxU32)*(m_bs.pBitstream - pBeging));

            SHInit = 1;
        }
        else
        {
            return MFX_ERR_UNSUPPORTED;
        }

        return MFX_ERR_NONE;
    }

    mfxStatus VC1_SPL_Headers::ParseEntryPoint(mfxU32* bs, mfxU32* len, mfxU32 dataSize)
    {
        mfxU32 i = 0;
        mfxU32 tempValue = 0;
        mfxU32* pBeging = PrepareData(bs, dataSize);

        VC1_SPL_GET_BITS(1, SH.BROKEN_LINK);
        VC1_SPL_GET_BITS(1, SH.CLOSED_ENTRY);
        VC1_SPL_GET_BITS(1, SH.PANSCAN_FLAG);
        VC1_SPL_GET_BITS(1, SH.REFDIST_FLAG);
        VC1_SPL_GET_BITS(1, SH.LOOPFILTER);
        VC1_SPL_GET_BITS(1, SH.FASTUVMC);
        VC1_SPL_GET_BITS(1, SH.EXTENDED_MV);
        VC1_SPL_GET_BITS(2, SH.DQUANT);
        VC1_SPL_GET_BITS(1, SH.VSTRANSFORM);
        VC1_SPL_GET_BITS(1, SH.OVERLAP);
        VC1_SPL_GET_BITS(2, SH.QUANTIZER);

        if (SH.CLOSED_ENTRY)
            SH.BROKEN_LINK = 0;

        if(SH.HRD_PARAM_FLAG == 1)
        {
            for(i=0; i < SH.HRD_NUM_LEAKY_BUCKETS;i++)
            {
                VC1_SPL_GET_BITS(8, tempValue);       //m_hrd_buffer_fullness.HRD_FULLNESS[i]
            }
        }

        VC1_SPL_GET_BITS(1, tempValue);    //CODED_SIZE_FLAG
        if (tempValue == 1)
        {
            VC1_SPL_GET_BITS(12, SH.CODED_WIDTH);
            VC1_SPL_GET_BITS(12, SH.CODED_HEIGHT);
        }

        if (SH.EXTENDED_MV == 1)
        {
            VC1_SPL_GET_BITS(1, SH.EXTENDED_DMV);
        }

        VC1_SPL_GET_BITS(1, SH.RANGE_MAPY_FLAG);   //RANGE_MAPY_FLAG
        if (SH.RANGE_MAPY_FLAG == 1)
        {
            VC1_SPL_GET_BITS(3,SH.RANGE_MAPY);
        }
        else
            SH.RANGE_MAPY = -1;

        VC1_SPL_GET_BITS(1, SH.RANGE_MAPUV_FLAG); //RANGE_MAPUV_FLAG

        if (SH.RANGE_MAPUV_FLAG == 1)
        {
            VC1_SPL_GET_BITS(3,SH.RANGE_MAPUV);
        }
        else
            SH.RANGE_MAPUV = -1;

        *len = (mfxU32)(sizeof(mfxU32)*(m_bs.pBitstream - pBeging));

        return MFX_ERR_NONE;
    }



    mfxStatus VC1_SPL_Headers::ParsePictureHeader(mfxU32* bs, mfxU32* len, mfxU32 dataSize)
    {
        mfxStatus mfxSts = MFX_ERR_NONE;

        mfxU32* pBeging = PrepareData(bs, dataSize);

        PicH.CurrField = 0;

        mfxSts = DecodePictHeaderParams();

        // we have to analyze whole picture header including start code
        *len = GetPicHeaderSize((mfxU8*)bs,
            (mfxU32)((mfxU32)sizeof(mfxU32)*(m_bs.pBitstream - pBeging))); // start code

        if (*len < 4)
            return MFX_ERR_UNKNOWN;

        return mfxSts;
    }

static const mfxU8 MapPQIndToQuant_Impl[] =
{
    0,
    1, 2, 3, 4, 5, 6, 7, 8,
    6, 7, 8, 9, 10,11,12,13,
    14,15,16,17,18,19,20,21,
    22,23,24,25,27,29,31
};
    mfxStatus VC1_SPL_Headers::DecodePictHeaderParams()
    {
        mfxU32 i = 0;
        mfxU32 tempValue = 0;
        mfxU32 RFF = 0;
        mfxU32 number_of_pan_scan_window = 0;
        mfxU32 RPTFRM = 0;

        if(SH.INTERLACE)
        {
            VC1_SPL_GET_BITS(1, PicH.FCM);
            if(PicH.FCM)
            {
                VC1_SPL_GET_BITS(1, PicH.FCM);
                if(PicH.FCM)
                    PicH.FCM = VC1_FieldInterlace;
                else
                    PicH.FCM = VC1_FrameInterlace;
            }
            else
                PicH.FCM = VC1_Progressive;
        }


        if(PicH.FCM != VC1_FieldInterlace)
        {
            VC1_SPL_GET_BITS(1, PicH.PTYPE);
            if(PicH.PTYPE)
            {
                VC1_SPL_GET_BITS(1, PicH.PTYPE);
                if(PicH.PTYPE)
                {
                    VC1_SPL_GET_BITS(1, PicH.PTYPE);
                    if(PicH.PTYPE)
                    {
                        VC1_SPL_GET_BITS(1, PicH.PTYPE);
                        if(PicH.PTYPE)
                            PicH.PTYPE = VC1_SKIPPED_FRAME;
                        else
                            PicH.PTYPE = VC1_BI_FRAME;
                    }
                    else
                        PicH.PTYPE = VC1_I_FRAME;
                }
                else
                    PicH.PTYPE = VC1_B_FRAME;
            }
            else
                PicH.PTYPE = VC1_P_FRAME;


            if(!(PicH.PTYPE == VC1_SKIPPED_FRAME)&&SH.TFCNTRFLAG)
                    VC1_SPL_GET_BITS(8, tempValue);       //TFCNTR

            if(SH.PULLDOWN)
            {
                if(!(SH.INTERLACE))
                {
                    VC1_SPL_GET_BITS(2, RPTFRM);
                }
                else
                {
                    mfxU32 tff;
                    VC1_SPL_GET_BITS(1, tff);
                    PicH.TFF = (mfxU8)tff;
                    VC1_SPL_GET_BITS(1, RFF);
                    PicH.RFF = (mfxU8)RFF;
                }
            }

            if(SH.PANSCAN_FLAG)
            {
                VC1_SPL_GET_BITS(1,tempValue);       //PS_PRESENT

                if(tempValue)       //PS_PRESENT
                {
                    if(SH.INTERLACE)
                    {
                        if(SH.PULLDOWN)
                            number_of_pan_scan_window = 2 + RFF;
                        else
                            number_of_pan_scan_window = 2;
                    }
                    else
                    {
                        if(SH.PULLDOWN)
                            number_of_pan_scan_window = 1 + RPTFRM;
                        else
                            number_of_pan_scan_window = 1;
                    }

                    for (i = 0; i< number_of_pan_scan_window; i++)
                    {
                        VC1_SPL_GET_BITS(18,tempValue);
                        VC1_SPL_GET_BITS(18,tempValue);
                        VC1_SPL_GET_BITS(14,tempValue);
                        VC1_SPL_GET_BITS(14,tempValue);
                    }
                }
            }

            if(!(PicH.PTYPE == VC1_SKIPPED_FRAME))
            {
                VC1_SPL_GET_BITS(1,PicH.RNDCTRL);

                if((SH.INTERLACE) || (PicH.FCM != VC1_Progressive))
                    VC1_SPL_GET_BITS(1,tempValue);//UVSAMP

                if(SH.FINTERPFLAG && (PicH.FCM == VC1_Progressive) )
                    VC1_SPL_GET_BITS(1,tempValue);

                if(PicH.PTYPE == VC1_B_FRAME && (PicH.FCM == VC1_Progressive) )
                {
                    VC1_SPL_GET_BITS(3,tempValue);
                    if(tempValue == 7)
                        VC1_SPL_GET_BITS(4,tempValue);
                }

                VC1_SPL_GET_BITS(5,PicH.PQINDEX);
                //CalculatePQuant
                {
                    PicH.PQUANT = PicH.PQINDEX;
                    PicH.QuantizationType = VC1_QUANTIZER_UNIFORM;

                    if(SH.QUANTIZER == 0)
                    {
                        if(PicH.PQINDEX < 9)
                        {
                            PicH.QuantizationType = VC1_QUANTIZER_UNIFORM;
                        }
                        else
                        {
                            PicH.QuantizationType = VC1_QUANTIZER_NONUNIFORM;
                            PicH.PQUANT = MapPQIndToQuant_Impl[PicH.PQINDEX];
                        }
                    }
                    else //01 or 10 or 11 binary
                    {
                        if(SH.QUANTIZER == 2)
                        {
                            PicH.QuantizationType = VC1_QUANTIZER_NONUNIFORM;
                        }
                    }
                }

                if(PicH.PQINDEX<=8)
                {
                    VC1_SPL_GET_BITS(1,PicH.HALFQP);
                }
                else
                    PicH.HALFQP = 0;


                if(SH.QUANTIZER == 1)
                    VC1_SPL_GET_BITS(1,PicH.PQUANTIZER);    //PQUANTIZER

                if(SH.POSTPROCFLAG)
                    VC1_SPL_GET_BITS(2,tempValue);        //POSTPROC
            }
        }
        else
        {
            DecodePictHeaderParams_InterlaceFieldPicture();
        }

        if(PicH.FCM == 0)
        {
            if(PicH.PTYPE == 0){DecodePictHeaderParams_ProgressiveIpicture_Adv();}
            else if (PicH.PTYPE == 1){DecodePictHeaderParams_ProgressivePpicture_Adv();}
            else if (PicH.PTYPE == 2){DecodePictHeaderParams_ProgressiveBpicture_Adv();}
            else if (PicH.PTYPE == 3){DecodePictHeaderParams_ProgressiveIpicture_Adv();}
            else if (PicH.PTYPE == 4){DecodeSkippicture();}
            else{return MFX_ERR_UNSUPPORTED;}
        }
        else if(PicH.FCM == 1)
        {
            if(PicH.PTYPE == 0){DecodePictHeaderParams_InterlaceIpicture_Adv();}
            else if (PicH.PTYPE == 1){DecodePictHeaderParams_InterlacePpicture_Adv();}
            else if (PicH.PTYPE == 2){DecodePictHeaderParams_InterlaceBpicture_Adv();}
            else if (PicH.PTYPE == 3){DecodePictHeaderParams_InterlaceIpicture_Adv();}
            else if (PicH.PTYPE == 4){DecodeSkippicture();}
            else{return MFX_ERR_UNSUPPORTED;}
        }
        else if(PicH.FCM == 2)
        {
            if(PicH.PTYPE == 0){DecodeFieldHeaderParams_InterlaceFieldIpicture_Adv();}
            else if (PicH.PTYPE == 1){DecodeFieldHeaderParams_InterlaceFieldPpicture_Adv();}
            else if (PicH.PTYPE == 2){DecodeFieldHeaderParams_InterlaceFieldBpicture_Adv();}
            else if (PicH.PTYPE == 3){DecodeFieldHeaderParams_InterlaceFieldIpicture_Adv();}
            else if (PicH.PTYPE == 4){DecodeSkippicture();}
            else{return MFX_ERR_UNSUPPORTED;}
        }
        else
        {
            return MFX_ERR_UNSUPPORTED;
        }
        return MFX_ERR_NONE;
    }

    mfxStatus VC1_SPL_Headers::DecodePictHeaderParams_InterlaceFieldPicture()
    {
        mfxU32 i = 0;
        mfxU32 tempValue;
        mfxU32 RFF = 0;
        mfxU32 number_of_pan_scan_window;
        PicH.RFF = 0;


        VC1_SPL_GET_BITS(3, tempValue);
        switch(tempValue)
        {
        case 0:
            PicH.PTypeField1 = VC1_I_FRAME;
            PicH.PTypeField2 = VC1_I_FRAME;
            break;
        case 1:
            PicH.PTypeField1 = VC1_I_FRAME;
            PicH.PTypeField2 = VC1_P_FRAME;
            break;
        case 2:
            PicH.PTypeField1 = VC1_P_FRAME;
            PicH.PTypeField2 = VC1_I_FRAME;
            break;
        case 3:
            PicH.PTypeField1 = VC1_P_FRAME;
            PicH.PTypeField2 = VC1_P_FRAME;
            break;
        case 4:
            PicH.PTypeField1 = VC1_B_FRAME;
            PicH.PTypeField2 = VC1_B_FRAME;
            break;
        case 5:
            PicH.PTypeField1 = VC1_B_FRAME;
            PicH.PTypeField2 = VC1_BI_FRAME;
            break;
        case 6:
            PicH.PTypeField1 = VC1_BI_FRAME;
            PicH.PTypeField2 = VC1_B_FRAME;
            break;
        case 7:
            PicH.PTypeField1 = VC1_BI_FRAME;
            PicH.PTypeField2 = VC1_BI_FRAME;
            break;
        default:
            assert(0);
            break;
        }

        if(SH.TFCNTRFLAG)
            VC1_SPL_GET_BITS(8, tempValue);       //TFCNTR

        if(SH.PULLDOWN)
        {
            if(!(SH.INTERLACE))
            {
                VC1_SPL_GET_BITS(2,tempValue);//RPTFRM
            }
            else
            {
                mfxU32 tff;
                VC1_SPL_GET_BITS(1, tff);
                PicH.TFF = (mfxU8)tff;
                VC1_SPL_GET_BITS(1, RFF);
                PicH.RFF = (mfxU8)RFF;
            }
        } else
            PicH.TFF = 1;

        if(SH.PANSCAN_FLAG)
        {
            VC1_SPL_GET_BITS(1,tempValue);       //PS_PRESENT

            if(tempValue)       //PS_PRESENT
            {
                if(SH.PULLDOWN)
                    number_of_pan_scan_window = 2 + RFF;
                else
                    number_of_pan_scan_window = 2;

                for (i = 0; i<number_of_pan_scan_window; i++)
                {
                    VC1_SPL_GET_BITS(18,tempValue);
                    VC1_SPL_GET_BITS(18,tempValue);
                    VC1_SPL_GET_BITS(14,tempValue);
                    VC1_SPL_GET_BITS(14,tempValue);
                }
            }
        }
        VC1_SPL_GET_BITS(1,PicH.RNDCTRL);

        VC1_SPL_GET_BITS(1,tempValue);//UVSAMP

        if(SH.REFDIST_FLAG == 1 &&
            (PicH.PTypeField1 < VC1_B_FRAME &&
            PicH.PTypeField2 < VC1_B_FRAME))
        {
            mfxU32 tmp = 0;
            VC1_SPL_GET_BITS(2,tmp);
            if(tmp == 0)
            {
                PicH.REFDIST = 0;
            }
            else if (tmp == 1)
            {
                PicH.REFDIST = 1;
            }
            else if (tmp == 2)
            {
                PicH.REFDIST = 2;
            }
            else
            {
                tmp = 1;
                PicH.REFDIST = 2;
                while(tmp!=0)
                {
                    VC1_SPL_GET_BITS(1,tmp);
                    PicH.REFDIST++;
                }
            }
        }
        else if(SH.REFDIST_FLAG == 0)
        {
            PicH.REFDIST = 0;
        }
        else
            PicH.REFDIST = 0;

        if( (PicH.PTypeField1 >= VC1_B_FRAME &&
            PicH.PTypeField2 >= VC1_B_FRAME) )
        {
                    VC1_SPL_GET_BITS(3,tempValue);
                    if(tempValue == 7)
                        VC1_SPL_GET_BITS(4,tempValue);
        }

        if(PicH.CurrField == 0)
        {
            PicH.PTYPE = PicH.PTypeField1;
            PicH.BottomField = (mfxU8)(1 - PicH.TFF);
        }
        else
        {
            PicH.BottomField = (mfxU8)(PicH.TFF);
            PicH.PTYPE = PicH.PTypeField2;
        }
        return MFX_ERR_NONE;
    }


    mfxStatus VC1_SPL_Headers::ParseSliceHeader(mfxU32* bs, mfxU32* len, mfxU32 dataSize)
    {
        mfxStatus mfxSts = MFX_ERR_NONE;
        mfxU32 temp = 0;

        mfxU32* pBeging = PrepareData(bs, dataSize);

        VC1_SPL_GET_BITS(9,temp);
        VC1_SPL_GET_BITS(1,temp);


        if(temp)
            mfxSts = DecodePictHeaderParams();

        *len = GetPicHeaderSize((mfxU8*)bs,
            (mfxU32)((mfxU32)sizeof(mfxU32)*(m_bs.pBitstream - pBeging)));

        if (*len < 4)
            return MFX_ERR_UNKNOWN;

        return MFX_ERR_NONE;
    }

    mfxStatus VC1_SPL_Headers::ParseFieldHeader(mfxU32* bs, mfxU32* len, mfxU32 dataSize)
    {
        mfxStatus mfxSts = MFX_ERR_NONE;
        mfxU32* pBeging = PrepareData(bs, dataSize);

        PicH.CurrField = 1;
        PicH.PTYPE = PicH.PTypeField2;

        if(PicH.PTYPE == 0){DecodeFieldHeaderParams_InterlaceFieldIpicture_Adv();}
        else if (PicH.PTYPE == 1){mfxSts = DecodeFieldHeaderParams_InterlaceFieldPpicture_Adv();}
        else if (PicH.PTYPE == 2){mfxSts = DecodeFieldHeaderParams_InterlaceFieldBpicture_Adv();}
        else if (PicH.PTYPE == 3){mfxSts = DecodeFieldHeaderParams_InterlaceFieldIpicture_Adv();}
        else if (PicH.PTYPE == 4){mfxSts = DecodeSkippicture();}
        else{mfxSts = MFX_ERR_UNSUPPORTED;}

        *len = GetPicHeaderSize((mfxU8*)bs,
            (mfxU32)((mfxU32)sizeof(mfxU32)*(m_bs.pBitstream - pBeging)));

        if (*len < 4)
            return MFX_ERR_UNKNOWN;

        return mfxSts;
    }

    mfxU32* VC1_SPL_Headers::PrepareData(mfxU32* bs,mfxU32 dataSize)
    {
        if (m_pSwapBuffer.size() <= dataSize + 8)
            m_pSwapBuffer.resize(dataSize + 8);

        m_bs.pBitstream = (mfxU32*)(&m_pSwapBuffer[0]);
        m_bs.bitOffset  = 31;

        memset(m_bs.pBitstream,0,m_pSwapBuffer.size());

        SwapData((mfxU8 *)bs, (mfxU8*)m_bs.pBitstream, dataSize);
        m_bs.pBitstream = (mfxU32*)(&m_pSwapBuffer[0] + 4);

        return (m_bs.pBitstream - 1);
    }

    static const mfxU32 bc_lut_1[] = {4,0,1,3};
    static const mfxU32 bc_lut_2[] = {0,1,2,3};
    static const mfxU32 bc_lut_4[] = {0,1,2};
    static const mfxU32 bc_lut_5[] = {0,0,1};

    mfxStatus VC1_SPL_Headers::DecodePictHeaderParams_ProgressiveIpicture_Adv()
    {
        DecodeBitplane(&PicH.ACPRED, SH.widthMB,  SH.heightMB,0);

        if( (SH.OVERLAP==1) && (PicH.PQUANT<=8) )
        {
            VC1_SPL_GET_BITS(1,PicH.CONDOVER);
            if(PicH.CONDOVER)
            {
                VC1_SPL_GET_BITS(1,PicH.CONDOVER);
                if(!PicH.CONDOVER)
                    PicH.CONDOVER = VC1_COND_OVER_FLAG_ALL;
                else
                {
                    PicH.CONDOVER = VC1_COND_OVER_FLAG_SOME;
                    DecodeBitplane(&PicH.OVERFLAGS,SH.widthMB, SH.heightMB,0);
                }
            }
            else
                PicH.CONDOVER = VC1_COND_OVER_FLAG_NONE;
        }

        VC1_SPL_GET_BITS(1,PicH.TRANSACFRM);
        if(PicH.TRANSACFRM)
        {
            VC1_SPL_GET_BITS(1,PicH.TRANSACFRM);
            PicH.TRANSACFRM++;
        }

        VC1_SPL_GET_BITS(1,PicH.TRANSACFRM2);
        if(PicH.TRANSACFRM2)
        {
            VC1_SPL_GET_BITS(1,PicH.TRANSACFRM2);
            PicH.TRANSACFRM2++;
        }

        VC1_SPL_GET_BITS(1,PicH.TRANSDCTAB);        //TRANSDCTAB

        return(VOPDQuant());
    }


    mfxStatus VC1_SPL_Headers::DecodePictHeaderParams_InterlaceIpicture_Adv()
    {
        DecodeBitplane(&PicH.FIELDTX, SH.widthMB,  SH.heightMB,0);

        DecodeBitplane(&PicH.ACPRED, SH.widthMB,  SH.heightMB,0);

        if( (SH.OVERLAP==1) && (PicH.PQUANT<=8) )
        {
            VC1_SPL_GET_BITS(1,PicH.CONDOVER);
            if(PicH.CONDOVER)
            {
                VC1_SPL_GET_BITS(1,PicH.CONDOVER);
                if(!PicH.CONDOVER)
                    PicH.CONDOVER = VC1_COND_OVER_FLAG_ALL;
                else
                {
                    PicH.CONDOVER = VC1_COND_OVER_FLAG_SOME;
                    DecodeBitplane(&PicH.OVERFLAGS, SH.widthMB, SH.heightMB,0);
                }
            }
            else
                PicH.CONDOVER = VC1_COND_OVER_FLAG_NONE;
        }

        VC1_SPL_GET_BITS(1,PicH.TRANSACFRM);
        if(PicH.TRANSACFRM)
        {
            VC1_SPL_GET_BITS(1,PicH.TRANSACFRM);
            PicH.TRANSACFRM++;
        }
        VC1_SPL_GET_BITS(1,PicH.TRANSACFRM2);
        if(PicH.TRANSACFRM2)
        {
            VC1_SPL_GET_BITS(1,PicH.TRANSACFRM2);
            PicH.TRANSACFRM2++;
        }

        VC1_SPL_GET_BITS(1,PicH.TRANSDCTAB);        //TRANSDCTAB

        return(VOPDQuant());
    }

    mfxStatus VC1_SPL_Headers::DecodeFieldHeaderParams_InterlaceFieldIpicture_Adv()
    {
        mfxU32 tempValue = 0;

        VC1_SPL_GET_BITS(5,PicH.PQINDEX);
                //CalculatePQuant
                {
                    PicH.PQUANT = PicH.PQINDEX;
                    PicH.QuantizationType = VC1_QUANTIZER_UNIFORM;

                    if(SH.QUANTIZER == 0)
                    {
                        if(PicH.PQINDEX < 9)
                        {
                            PicH.QuantizationType = VC1_QUANTIZER_UNIFORM;
                        }
                        else
                        {
                            PicH.QuantizationType = VC1_QUANTIZER_NONUNIFORM;
                            PicH.PQUANT = MapPQIndToQuant_Impl[PicH.PQINDEX];
                        }
                    }
                    else //01 or 10 or 11 binary
                    {
                        if(SH.QUANTIZER == 2)
                        {
                            PicH.QuantizationType = VC1_QUANTIZER_NONUNIFORM;
                        }
                    }
                }

        if(PicH.PQINDEX<=8)
        {
            VC1_SPL_GET_BITS(1,PicH.HALFQP);
        }
        else
            PicH.HALFQP = 0;


        if(SH.QUANTIZER == 1)
            VC1_SPL_GET_BITS(1,PicH.PQUANTIZER);    //PQUANTIZER

        if(SH.POSTPROCFLAG)
            VC1_SPL_GET_BITS(2,tempValue);        //POSTPROC

        if (PicH.CurrField == 0)
            DecodeBitplane(&PicH.ACPRED,SH.widthMB, SH.heightMB/2,0);
        else
            DecodeBitplane(&PicH.ACPRED,  SH.widthMB,SH.heightMB/2,SH.widthMB * SH.heightMB/2);


        if( (SH.OVERLAP==1) && (PicH.PQUANT<=8) )
        {
            VC1_SPL_GET_BITS(1,PicH.CONDOVER);
            if(PicH.CONDOVER)
            {
                VC1_SPL_GET_BITS(1,PicH.CONDOVER);
                if(!PicH.CONDOVER)
                    PicH.CONDOVER = VC1_COND_OVER_FLAG_ALL;
                else
                {
                    PicH.CONDOVER = VC1_COND_OVER_FLAG_SOME;

                    if (PicH.CurrField == 0)
                        DecodeBitplane(&PicH.OVERFLAGS,SH.widthMB, SH.heightMB/2,0);
                    else
                        DecodeBitplane(&PicH.OVERFLAGS,SH.widthMB, SH.heightMB/2, SH.widthMB*SH.heightMB/2);
                }
            }
            else
                PicH.CONDOVER = VC1_COND_OVER_FLAG_NONE;
        }

        VC1_SPL_GET_BITS(1,PicH.TRANSACFRM);
        if(PicH.TRANSACFRM)
        {
            VC1_SPL_GET_BITS(1,PicH.TRANSACFRM);
            PicH.TRANSACFRM++;
        }

        VC1_SPL_GET_BITS(1,PicH.TRANSACFRM2);
        if(PicH.TRANSACFRM2)
        {
            VC1_SPL_GET_BITS(1,PicH.TRANSACFRM2);
            PicH.TRANSACFRM2++;
        }

        VC1_SPL_GET_BITS(1,PicH.TRANSDCTAB);        //TRANSDCTAB

        return VOPDQuant();
    }

    mfxStatus VC1_SPL_Headers::DecodePictHeaderParams_ProgressivePpicture_Adv()
    {
        mfxStatus mfxSts = MFX_ERR_NONE;
        MVRangeDecode();

        if(PicH.PQUANT > 12)
        {
            mfxI32 bit_count = 1;
            VC1_SPL_GET_BITS(1, PicH.MVMODE);
            while((PicH.MVMODE == 0) && (bit_count < 4))
            {
                VC1_SPL_GET_BITS(1, PicH.MVMODE);
                bit_count++;
            }
            if (bit_count < 4)
                PicH.MVMODE = VC1_LUT_SET(bit_count,bc_lut_1);
            else
                if(PicH.MVMODE == 0)
                    PicH.MVMODE = VC1_MVMODE_MIXED_MV;
                else
                {
                    PicH.MVMODE = VC1_MVMODE_INTENSCOMP;
                     mfxI32 bit_count = 1;
                    VC1_SPL_GET_BITS(1, PicH.MVMODE);
                    while((PicH.MVMODE == 0) && (bit_count < 3))
                    {
                        VC1_SPL_GET_BITS(1, PicH.MVMODE);
                        bit_count++;
                    }
                    if (bit_count < 3)
                        PicH.MVMODE = VC1_LUT_SET(bit_count,bc_lut_5);
                    else
                        if(PicH.MVMODE == 0)
                            PicH.MVMODE = VC1_MVMODE_MIXED_MV;
                        else
                            PicH.MVMODE = VC1_MVMODE_HPEL_1MV;
                    VC1_SPL_GET_BITS(6, PicH.LUMSCALE);

                    VC1_SPL_GET_BITS(6, PicH.LUMSHIFT);
                }
        }
        else
        {
            mfxI32 bit_count = 1;
            VC1_SPL_GET_BITS(1, PicH.MVMODE);
            while((PicH.MVMODE == 0) && (bit_count < 4))
            {
                VC1_SPL_GET_BITS(1, PicH.MVMODE);
                bit_count++;
            }
            if (bit_count < 4)
                PicH.MVMODE = VC1_LUT_SET(bit_count,bc_lut_2);
            else
                if(PicH.MVMODE == 0)
                    PicH.MVMODE = VC1_MVMODE_HPELBI_1MV;
                else
                {
                    PicH.MVMODE = VC1_MVMODE_INTENSCOMP;
                    mfxI32 bit_count = 1;
                    VC1_SPL_GET_BITS(1, PicH.MVMODE);
                    while((PicH.MVMODE == 0) && (bit_count < 3))
                    {
                        VC1_SPL_GET_BITS(1, PicH.MVMODE);
                        bit_count++;
                    }
                    if (bit_count < 3)
                        PicH.MVMODE = VC1_LUT_SET(bit_count,bc_lut_4);
                    else
                        if(PicH.MVMODE == 0)
                            PicH.MVMODE = VC1_MVMODE_HPELBI_1MV;
                        else
                            PicH.MVMODE = VC1_MVMODE_HPEL_1MV;

                    VC1_SPL_GET_BITS(6, PicH.LUMSCALE);
                    VC1_SPL_GET_BITS(6, PicH.LUMSHIFT);
                }
        }

        if(PicH.MVMODE == VC1_MVMODE_MIXED_MV)
        {
            DecodeBitplane(&PicH.MVTYPEMB, SH.widthMB, SH.heightMB,0);
        }

        DecodeBitplane(&PicH.SKIPMB, SH.widthMB, SH.heightMB,0);

        VC1_SPL_GET_BITS(2, PicH.MVTAB);       //MVTAB
        VC1_SPL_GET_BITS(2, PicH.CBPTAB);       //CBPTAB

        mfxSts = VOPDQuant();

        if(SH.VSTRANSFORM)
        {
            VC1_SPL_GET_BITS(1, PicH.TTMBF);

            if(PicH.TTMBF)
            {
                VC1_SPL_GET_BITS(2, PicH.TTFRM_ORIG);
                PicH.TTFRM = 1 << PicH.TTFRM_ORIG;
            }
        }

        VC1_SPL_GET_BITS(1, PicH.TRANSACFRM);//TRANSACFRM

        if(PicH.TRANSACFRM == 1)
        {
            VC1_SPL_GET_BITS(1, PicH.TRANSACFRM);
            PicH.TRANSACFRM++;
        }
        PicH.TRANSACFRM2 = PicH.TRANSACFRM;

        VC1_SPL_GET_BITS(1, PicH.TRANSDCTAB);       //TRANSDCTAB

        return mfxSts;
    }

    mfxStatus VC1_SPL_Headers::DecodePictHeaderParams_InterlacePpicture_Adv()
    {
        mfxU32 tempValue = 0;
        mfxStatus mfxSts = MFX_ERR_NONE;

        MVRangeDecode();

        if(SH.EXTENDED_DMV == 1)
        {
            VC1_SPL_GET_BITS(1, PicH.DMVRANGE);
            if(PicH.DMVRANGE==0)
                PicH.DMVRANGE = VC1_DMVRANGE_NONE;
            else
            {
                VC1_SPL_GET_BITS(1, PicH.DMVRANGE);
                if(PicH.DMVRANGE==0)
                    PicH.DMVRANGE = VC1_DMVRANGE_HORIZONTAL_RANGE;
                else
                {
                    VC1_SPL_GET_BITS(1, PicH.DMVRANGE);
                    if(PicH.DMVRANGE==0)
                        PicH.DMVRANGE = VC1_DMVRANGE_VERTICAL_RANGE;
                    else
                        PicH.DMVRANGE = VC1_DMVRANGE_HORIZONTAL_VERTICAL_RANGE;
                }
            }
        }

        VC1_SPL_GET_BITS(1, PicH.MV4SWITCH);
        if(PicH.MV4SWITCH)
            PicH.MVMODE = VC1_MVMODE_MIXED_MV;
        else
            PicH.MVMODE = VC1_MVMODE_1MV;

        VC1_SPL_GET_BITS(1, tempValue);       //INTCOMP

        if(tempValue)       //INTCOM
        {
            VC1_SPL_GET_BITS(6, PicH.LUMSCALE);
            VC1_SPL_GET_BITS(6, PicH.LUMSHIFT);
        }

        DecodeBitplane(&PicH.SKIPMB, SH.widthMB, SH.heightMB,0);

        VC1_SPL_GET_BITS(2, PicH.MBMODETAB);       //MBMODETAB
        VC1_SPL_GET_BITS(2,PicH.MVTAB);       //MVTAB
        VC1_SPL_GET_BITS(3,PicH.CBPTAB);       //CBPTAB
        VC1_SPL_GET_BITS(2, PicH.MV2BPTAB);       //MV2BPTAB

        if(PicH.MV4SWITCH)
            VC1_SPL_GET_BITS(2, PicH.MV4BPTAB)        //MV4BPTAB;

        mfxSts = VOPDQuant();

        if(SH.VSTRANSFORM == 1)
        {
            VC1_SPL_GET_BITS(1, PicH.TTMBF);

            if(PicH.TTMBF)
            {
                VC1_SPL_GET_BITS(2, PicH.TTFRM_ORIG);
                PicH.TTFRM = 1 << PicH.TTFRM_ORIG;
            }
            else
                PicH.TTFRM = VC1_BLK_INTER;
        }
        else
            PicH.TTFRM = VC1_BLK_INTER8X8;

        VC1_SPL_GET_BITS(1, PicH.TRANSACFRM);//TRANSACFRM
        if(PicH.TRANSACFRM == 1)
        {
            VC1_SPL_GET_BITS(1, PicH.TRANSACFRM);
            PicH.TRANSACFRM++;
        }

        VC1_SPL_GET_BITS(1, PicH.TRANSDCTAB);       //TRANSDCTAB

        return mfxSts;
    }

    mfxStatus VC1_SPL_Headers::DecodeFieldHeaderParams_InterlaceFieldPpicture_Adv()
    {
        mfxStatus mfxSts = MFX_ERR_NONE;
        mfxU32 tempValue = 0;

        VC1_SPL_GET_BITS(5,PicH.PQINDEX);
        //CalculatePQuant
        {
            PicH.PQUANT = PicH.PQINDEX;
            PicH.QuantizationType = VC1_QUANTIZER_UNIFORM;

            if(SH.QUANTIZER == 0)
            {
                if(PicH.PQINDEX < 9)
                {
                    PicH.QuantizationType = VC1_QUANTIZER_UNIFORM;
                }
                else
                {
                    PicH.QuantizationType = VC1_QUANTIZER_NONUNIFORM;
                    PicH.PQUANT = MapPQIndToQuant_Impl[PicH.PQINDEX];
                }
            }
            else //01 or 10 or 11 binary
            {
                if(SH.QUANTIZER == 2)
                {
                    PicH.QuantizationType = VC1_QUANTIZER_NONUNIFORM;
                }
            }
        }

        if(PicH.PQINDEX<=8)
        {
            VC1_SPL_GET_BITS(1,PicH.HALFQP);
        }
        else
            PicH.HALFQP = 0;


        if(SH.QUANTIZER == 1)
            VC1_SPL_GET_BITS(1,PicH.PQUANTIZER);    //PQUANTIZER

        if(SH.POSTPROCFLAG)
            VC1_SPL_GET_BITS(2,tempValue);        //POSTPROC

        VC1_SPL_GET_BITS(1,PicH.NUMREF);

        if(!PicH.NUMREF)
            VC1_SPL_GET_BITS(1,PicH.REFFIELD);

        MVRangeDecode();

        DMVRangeDecode();

        PicH.INTCOMFIELD = 0;

        if(PicH.PQUANT > 12)
        {
            mfxI32 bit_count = 1;
            VC1_SPL_GET_BITS(1, PicH.MVMODE);
            while((PicH.MVMODE == 0) && (bit_count < 4))
            {
                VC1_SPL_GET_BITS(1, PicH.MVMODE);
                bit_count++;
            }
            if (bit_count < 4)
                PicH.MVMODE = VC1_LUT_SET(bit_count,bc_lut_1);
            else
                if(PicH.MVMODE == 0)
                    PicH.MVMODE = VC1_MVMODE_MIXED_MV;
                else
                {
                    PicH.MVMODE = VC1_MVMODE_INTENSCOMP;

                    {
                        mfxI32 bit_count = 1;
                        VC1_SPL_GET_BITS(1, PicH.MVMODE);
                        while((PicH.MVMODE == 0) && (bit_count < 3))
                        {
                            VC1_SPL_GET_BITS(1, PicH.MVMODE);
                            bit_count++;
                        }
                        if (bit_count < 3)
                            PicH.MVMODE = VC1_LUT_SET(bit_count,bc_lut_5);
                        else
                            if(PicH.MVMODE == 0)
                                PicH.MVMODE = VC1_MVMODE_MIXED_MV;
                            else
                                PicH.MVMODE = VC1_MVMODE_HPEL_1MV;

                        VC1_SPL_GET_BITS(1, PicH.INTCOMFIELD);
                        if(PicH.INTCOMFIELD == 1)
                            PicH.INTCOMFIELD = VC1_INTCOMP_BOTH_FIELD;
                        else
                        {
                            VC1_SPL_GET_BITS(1, PicH.INTCOMFIELD);
                            if(PicH.INTCOMFIELD == 1)
                                PicH.INTCOMFIELD = VC1_INTCOMP_BOTTOM_FIELD;
                            else
                                PicH.INTCOMFIELD = VC1_INTCOMP_TOP_FIELD;
                        }

                        if(VC1_IS_INT_TOP_FIELD(PicH.INTCOMFIELD))
                        {
                            VC1_SPL_GET_BITS(6, PicH.LUMSCALE);
                            VC1_SPL_GET_BITS(6, PicH.LUMSHIFT);
                        }

                        if(VC1_IS_INT_BOTTOM_FIELD(PicH.INTCOMFIELD) )
                        {
                            VC1_SPL_GET_BITS(6, PicH.LUMSCALE);
                            VC1_SPL_GET_BITS(6, PicH.LUMSHIFT);
                        }
                        PicH.MVMODE2 = PicH.MVMODE;
                    }
                }
        }
        else
        {
            mfxI32 bit_count = 1;
            VC1_SPL_GET_BITS(1, PicH.MVMODE);
            while((PicH.MVMODE == 0) && (bit_count < 4))
            {
                VC1_SPL_GET_BITS(1, PicH.MVMODE);
                bit_count++;
            }
            if (bit_count < 4)
                PicH.MVMODE = VC1_LUT_SET(bit_count,bc_lut_2);
            else
                if(PicH.MVMODE == 0)
                    PicH.MVMODE = VC1_MVMODE_HPELBI_1MV;
                else
                {
                    PicH.MVMODE = VC1_MVMODE_INTENSCOMP;
                    {
                        mfxI32 bit_count = 1;
                        VC1_SPL_GET_BITS(1, PicH.MVMODE);
                        while((PicH.MVMODE == 0) && (bit_count < 3))
                        {
                            VC1_SPL_GET_BITS(1, PicH.MVMODE);
                            bit_count++;
                        }
                        if (bit_count < 3)
                            PicH.MVMODE = VC1_LUT_SET(bit_count,bc_lut_4);
                        else
                            if(PicH.MVMODE == 0)
                                PicH.MVMODE = VC1_MVMODE_HPELBI_1MV;
                            else
                                PicH.MVMODE = VC1_MVMODE_HPEL_1MV;

                        VC1_SPL_GET_BITS(1, PicH.INTCOMFIELD);
                        if(PicH.INTCOMFIELD == 1)
                            PicH.INTCOMFIELD = VC1_INTCOMP_BOTH_FIELD;
                        else
                        {
                            VC1_SPL_GET_BITS(1, PicH.INTCOMFIELD);
                            if(PicH.INTCOMFIELD == 1)
                                PicH.INTCOMFIELD = VC1_INTCOMP_BOTTOM_FIELD;
                            else
                                PicH.INTCOMFIELD = VC1_INTCOMP_TOP_FIELD;
                        }

                        if(VC1_IS_INT_TOP_FIELD(PicH.INTCOMFIELD))
                        {
                            VC1_SPL_GET_BITS(6, PicH.LUMSCALE);
                            VC1_SPL_GET_BITS(6, PicH.LUMSHIFT);
                        }

                        if(VC1_IS_INT_BOTTOM_FIELD(PicH.INTCOMFIELD) )
                        {
                            VC1_SPL_GET_BITS(6, PicH.LUMSCALE);
                            VC1_SPL_GET_BITS(6, PicH.LUMSHIFT);
                        }

                        PicH.MVMODE2 = PicH.MVMODE;
                    }
                }
        }

        VC1_SPL_GET_BITS(3, PicH.MBMODETAB);       //MBMODETAB

        if(PicH.NUMREF)
        {
            VC1_SPL_GET_BITS(3, PicH.MVTAB);       //MVTAB
        }
        else
            VC1_SPL_GET_BITS(2, PicH.MVTAB);       //MVTAB

        VC1_SPL_GET_BITS(3, PicH.CBPTAB);       //CBPTAB

        if(PicH.MVMODE == VC1_MVMODE_MIXED_MV)
            VC1_SPL_GET_BITS(2, PicH.MV4BPTAB)        //MV4BPTAB;

        mfxSts = VOPDQuant();

        if(SH.VSTRANSFORM == 1)
        {
            VC1_SPL_GET_BITS(1, PicH.TTMBF);

            if(PicH.TTMBF)
            {
                VC1_SPL_GET_BITS(2, PicH.TTFRM_ORIG);
                PicH.TTFRM = 1 << PicH.TTFRM_ORIG;
            }
            else
                PicH.TTFRM = VC1_BLK_INTER;
        }
        else
        {
            PicH.TTFRM = VC1_BLK_INTER8X8;
        }

        VC1_SPL_GET_BITS(1, PicH.TRANSACFRM);//TRANSACFRM
        if(PicH.TRANSACFRM == 1)
        {
            VC1_SPL_GET_BITS(1, PicH.TRANSACFRM);
            PicH.TRANSACFRM++;
        }

        VC1_SPL_GET_BITS(1, PicH.TRANSDCTAB);       //TRANSDCTAB

        return mfxSts;
    }


    mfxStatus VC1_SPL_Headers::DecodePictHeaderParams_ProgressiveBpicture_Adv()
    {
        mfxStatus mfxSts = MFX_ERR_NONE;
        MVRangeDecode();

        VC1_SPL_GET_BITS(1, PicH.MVMODE);
        PicH.MVMODE =(PicH.MVMODE==1)? VC1_MVMODE_1MV:VC1_MVMODE_HPELBI_1MV;

        DecodeBitplane(&PicH.m_DirectMB, SH.widthMB, SH.heightMB,0);
        DecodeBitplane(&PicH.SKIPMB, SH.widthMB, SH.heightMB,0);

        VC1_SPL_GET_BITS(2, PicH.MVTAB);       //MVTAB
        VC1_SPL_GET_BITS(2,PicH.CBPTAB);       //CBPTAB

        mfxSts = VOPDQuant();

        if(SH.VSTRANSFORM)
        {
            VC1_SPL_GET_BITS(1, PicH.TTMBF);

            if(PicH.TTMBF)
            {
                VC1_SPL_GET_BITS(2, PicH.TTFRM_ORIG);
                PicH.TTFRM = 1 << PicH.TTFRM_ORIG;
            }
            else
                PicH.TTFRM = VC1_BLK_INTER;
        }
        else
            PicH.TTFRM = VC1_BLK_INTER8X8;


        VC1_SPL_GET_BITS(1, PicH.TRANSACFRM);//TRANSACFRM
        if(PicH.TRANSACFRM == 1)
        {
            VC1_SPL_GET_BITS(1, PicH.TRANSACFRM);
            PicH.TRANSACFRM++;
        }

        VC1_SPL_GET_BITS(1, PicH.TRANSDCTAB);       //TRANSDCTAB

        return mfxSts;
    }


    mfxStatus VC1_SPL_Headers::DecodePictHeaderParams_InterlaceBpicture_Adv()
    {
        mfxStatus mfxSts = MFX_ERR_NONE;
        mfxU32 tempValue = 0;

        {
            VC1_SPL_GET_BITS(3,tempValue);
            if(tempValue == 7)
               VC1_SPL_GET_BITS(4,tempValue);
        }
        MVRangeDecode();

        if(SH.EXTENDED_DMV == 1)
        {
            VC1_SPL_GET_BITS(1, PicH.DMVRANGE);
            if(PicH.DMVRANGE==0)
                PicH.DMVRANGE = VC1_DMVRANGE_NONE;
            else
            {
                VC1_SPL_GET_BITS(1, PicH.DMVRANGE);
                if(PicH.DMVRANGE==0)
                   PicH.DMVRANGE = VC1_DMVRANGE_HORIZONTAL_RANGE;
                else
                {
                    VC1_SPL_GET_BITS(1, PicH.DMVRANGE);
                    if(PicH.DMVRANGE==0)
                        PicH.DMVRANGE = VC1_DMVRANGE_VERTICAL_RANGE;
                    else
                        PicH.DMVRANGE = VC1_DMVRANGE_HORIZONTAL_VERTICAL_RANGE;
                }
            }
        }

        VC1_SPL_GET_BITS(1, tempValue);       //INTCOMP

        DecodeBitplane(&PicH.m_DirectMB,SH.widthMB, SH.heightMB,0);

        DecodeBitplane(&PicH.SKIPMB,SH.widthMB, SH.heightMB,0);

        PicH.MVMODE = VC1_MVMODE_1MV;

        VC1_SPL_GET_BITS(2, PicH.MBMODETAB);       //MBMODETAB

        VC1_SPL_GET_BITS(2, PicH.MVTAB);       //MVTAB

        VC1_SPL_GET_BITS(3,PicH.CBPTAB);       //CBPTAB

        VC1_SPL_GET_BITS(2, PicH.MV2BPTAB);       //MV2BPTAB

        VC1_SPL_GET_BITS(2, PicH.MV4BPTAB)        //MV4BPTAB;

        mfxSts = VOPDQuant();

        if(SH.VSTRANSFORM)
        {
            VC1_SPL_GET_BITS(1, PicH.TTMBF);

            if(PicH.TTMBF)
            {
                VC1_SPL_GET_BITS(2, PicH.TTFRM_ORIG);
                PicH.TTFRM = 1 << PicH.TTFRM_ORIG;
            }
            else
                PicH.TTFRM = VC1_BLK_INTER;
        }
        else
            PicH.TTFRM = VC1_BLK_INTER8X8;

        VC1_SPL_GET_BITS(1, PicH.TRANSACFRM);//TRANSACFRM
        if(PicH.TRANSACFRM == 1)
        {
            VC1_SPL_GET_BITS(1, PicH.TRANSACFRM);
            PicH.TRANSACFRM++;
        }

        VC1_SPL_GET_BITS(1, PicH.TRANSDCTAB);       //TRANSDCTAB

        return mfxSts;
    }


    mfxStatus VC1_SPL_Headers::DecodeFieldHeaderParams_InterlaceFieldBpicture_Adv ()
    {
        mfxU32 tempValue = 0;
        mfxStatus mfxSts = MFX_ERR_NONE;

        PicH.NUMREF = 1;

        VC1_SPL_GET_BITS(5,PicH.PQINDEX);
        //CalculatePQuant
        {
            PicH.PQUANT = PicH.PQINDEX;
            PicH.QuantizationType = VC1_QUANTIZER_UNIFORM;

            if(SH.QUANTIZER == 0)
            {
                if(PicH.PQINDEX < 9)
                {
                    PicH.QuantizationType = VC1_QUANTIZER_UNIFORM;
                }
                else
                {
                    PicH.QuantizationType = VC1_QUANTIZER_NONUNIFORM;
                    PicH.PQUANT = MapPQIndToQuant_Impl[PicH.PQINDEX];
                }
            }
            else //01 or 10 or 11 binary
            {
                if(SH.QUANTIZER == 2)
                {
                    PicH.QuantizationType = VC1_QUANTIZER_NONUNIFORM;
                }
            }
        }

        if(PicH.PQINDEX<=8)
        {
            VC1_SPL_GET_BITS(1,PicH.HALFQP);
        }
        else
            PicH.HALFQP = 0;


        if(SH.QUANTIZER == 1)
            VC1_SPL_GET_BITS(1,PicH.PQUANTIZER);    //PQUANTIZER

        if(SH.POSTPROCFLAG)
            VC1_SPL_GET_BITS(2,tempValue);        //POSTPROC

        MVRangeDecode();

        DMVRangeDecode();

        if(PicH.PQUANT > 12)
        {
            mfxI32 bit_count = 1;
            VC1_SPL_GET_BITS(1, PicH.MVMODE);
            while((PicH.MVMODE == 0) && (bit_count < 3))
            {
                VC1_SPL_GET_BITS(1, PicH.MVMODE);
                bit_count++;
            }
            if (bit_count < 3)
                PicH.MVMODE = VC1_LUT_SET(bit_count,bc_lut_1);
            else
                if(PicH.MVMODE == 0)
                    PicH.MVMODE = VC1_MVMODE_MIXED_MV;
                else
                    PicH.MVMODE = VC1_MVMODE_HPEL_1MV;
        }
        else
        {
            mfxI32 bit_count = 1;
            VC1_SPL_GET_BITS(1, PicH.MVMODE);
            while((PicH.MVMODE == 0) && (bit_count < 3))
            {
                VC1_SPL_GET_BITS(1, PicH.MVMODE);
                bit_count++;
            }
            if (bit_count < 3)
                PicH.MVMODE = VC1_LUT_SET(bit_count,bc_lut_2);
            else
                if(PicH.MVMODE == 0)
                    PicH.MVMODE = VC1_MVMODE_HPELBI_1MV;
                else
                    PicH.MVMODE = VC1_MVMODE_HPEL_1MV;
        }

        //FORWARDMB
        if (PicH.CurrField == 0)
            DecodeBitplane(&PicH.FORWARDMB,SH.widthMB, SH.heightMB/2,0);
        else
            DecodeBitplane(&PicH.FORWARDMB,SH.widthMB, SH.heightMB/2, SH.widthMB * SH.heightMB/2);

        VC1_SPL_GET_BITS(3, PicH.MBMODETAB);       //MBMODETAB
        VC1_SPL_GET_BITS(3, PicH.MVTAB);       //MVTAB
        VC1_SPL_GET_BITS(3, PicH.CBPTAB);       //CBPTAB

        if(PicH.MVMODE == VC1_MVMODE_MIXED_MV)
            VC1_SPL_GET_BITS(2, PicH.MV4BPTAB)        //MV4BPTAB;

        mfxSts = VOPDQuant();

        if(SH.VSTRANSFORM == 1)
        {
            VC1_SPL_GET_BITS(1, PicH.TTMBF);

            if(PicH.TTMBF)
            {
                VC1_SPL_GET_BITS(2, PicH.TTFRM_ORIG);
                PicH.TTFRM = 1 << PicH.TTFRM_ORIG;
            }
            else
                PicH.TTFRM = VC1_BLK_INTER;
        }
        else
            PicH.TTFRM = VC1_BLK_INTER8X8;

        VC1_SPL_GET_BITS(1, PicH.TRANSACFRM);//TRANSACFRM
        if(PicH.TRANSACFRM == 1)
        {
            VC1_SPL_GET_BITS(1, PicH.TRANSACFRM);
            PicH.TRANSACFRM++;
        }

        VC1_SPL_GET_BITS(1, PicH.TRANSDCTAB);       //TRANSDCTAB

        return mfxSts;
    }

    mfxStatus VC1_SPL_Headers::DecodeSkippicture()
    {
        return MFX_ERR_NONE;
    }

    mfxStatus VC1_SPL_Headers::VOPDQuant()
    {
        mfxU32 tempValue = 0;
        mfxU32 DQUANT  = SH.DQUANT;

        if(DQUANT == 1)
        {
            VC1_SPL_GET_BITS(1, PicH.m_DQuantFRM);

            if(PicH.m_DQuantFRM == 1)
            {
                VC1_SPL_GET_BITS(2,PicH.m_DQProfile);
                switch (PicH.m_DQProfile)
                {
                case VC1_DQPROFILE_ALL4EDGES:
                    PicH.m_PQuant_mode = VC1_ALTPQUANT_EDGES;
                    break;
                case VC1_DQPROFILE_SNGLEDGES:
                    {
                        VC1_SPL_GET_BITS(2, PicH.DQSBEdge);
                        PicH.m_PQuant_mode = 1<<PicH.DQSBEdge;
                        break;
                    }
                case VC1_DQPROFILE_DBLEDGES:
                    {
                        VC1_SPL_GET_BITS(2, PicH.DQSBEdge);
                        PicH.m_PQuant_mode = (PicH.DQSBEdge>1)?VC1_ALTPQUANT_BOTTOM:VC1_ALTPQUANT_TOP;
                        PicH.m_PQuant_mode |= ((PicH.DQSBEdge%3)? VC1_ALTPQUANT_RIGTHT:VC1_ALTPQUANT_LEFT);
                        break;
                    }
                case VC1_DQPROFILE_ALLMBLKS:
                    {
                        VC1_SPL_GET_BITS(1, PicH.m_DQBILevel);
                        PicH.m_PQuant_mode = (PicH.m_DQBILevel)? VC1_ALTPQUANT_MB_LEVEL:VC1_ALTPQUANT_ANY_VALUE;
                        break;
                    }
                }
            }
            else
                PicH.m_PQuant_mode=VC1_ALTPQUANT_NO;
        }
        else if (DQUANT == 2)
        {
            PicH.m_PQuant_mode = VC1_ALTPQUANT_ALL;
            PicH.m_DQuantFRM = 1;
        }
        else
            PicH.m_PQuant_mode=VC1_ALTPQUANT_NO;
        if (PicH.m_DQuantFRM)
        {
            if(DQUANT==2 || !(PicH.m_DQProfile == VC1_DQPROFILE_ALLMBLKS
                && PicH.m_DQBILevel == 0))
            {
                VC1_SPL_GET_BITS(3, tempValue); //PQDIFF

                if(tempValue == 7) // escape
                {
                    VC1_SPL_GET_BITS(5, tempValue);       //m_ABSPQ
                    PicH.m_AltPQuant = tempValue;       //m_ABSPQ
                }
                else
                    PicH.m_AltPQuant = PicH.PQUANT + tempValue + 1;
               }
        }
        return MFX_ERR_NONE;
    }

       mfxU32 VC1_SPL_Headers::GetPicHeaderSize(mfxU8* pOriginalData, mfxU32 Offset)
        {
            mfxU32 i = 0;
            mfxU32 numZero = 0;
            mfxU8* ptr = pOriginalData;

            for(i = 0; i < Offset; i++)
            {
                if(*ptr == 0)
                    numZero++;
                else
                    numZero = 0;

                ptr++;

                if((numZero) == 2 && (*ptr == 0x03))
                {
                    if(*(ptr+1) < 4)
                    {
                        ptr++;
                    }
                    numZero = 0;
                }
            }

            if((numZero == 1) && (*ptr == 0) && (*(ptr+1) == 0x03) && (*(ptr+2) < 4))
            {
                ptr++;
            }

            return (mfxU32)(ptr - pOriginalData);
        }
    mfxStatus VC1_SPL_Headers::GetPicType(SliceTypeCode *type){
        if(PicH.PTYPE == VC1_I_FRAME || PicH.PTYPE == VC1_BI_FRAME)
            *type = TYPE_I;
        else if(PicH.PTYPE == VC1_P_FRAME)
            *type = TYPE_P;
        else if(PicH.PTYPE == VC1_B_FRAME)
            *type = TYPE_B;
        else if(PicH.PTYPE == VC1_SKIPPED_FRAME)
            *type = TYPE_SKIP;
        else
            return MFX_ERR_UNDEFINED_BEHAVIOR;

        return MFX_ERR_NONE;
    }
}
