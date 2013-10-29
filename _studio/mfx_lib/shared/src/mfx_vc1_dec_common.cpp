/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2004-2013 Intel Corporation. All Rights Reserved.
//
*/

#include "mfx_common.h"

#if defined (MFX_ENABLE_VC1_VIDEO_BSD) || defined (MFX_ENABLE_VC1_VIDEO_DEC) || defined (MFX_ENABLE_VC1_VIDEO_DECODE)

#include "mfx_vc1_dec_common.h"
#include "umc_vc1_common.h"

#include "mfxpcp.h"

#include "mfx_common_int.h"

mfxStatus MFXVC1DecCommon::ConvertMfxToCodecParams(mfxVideoParam *par, UMC::BaseCodecParams* pVideoParams)
{
    UMC::VideoDecoderParams *init = DynamicCast<UMC::VideoDecoderParams, UMC::BaseCodecParams>(pVideoParams);
    if (init)
    {

        init->info.clip_info.height = par->mfx.FrameInfo.Height;
        init->info.clip_info.width = par->mfx.FrameInfo.Width;

        //init->lFlags |= UMC::FLAG_VDEC_REORDER;

        if (MFX_PROFILE_VC1_ADVANCED == par->mfx.CodecProfile)
        {
            init->info.stream_subtype = UMC::VC1_VIDEO_VC1;
            init->info.stream_type = UMC::VC1_VIDEO;
        }
        else if ((MFX_PROFILE_VC1_MAIN == par->mfx.CodecProfile)||
            (MFX_PROFILE_VC1_SIMPLE == par->mfx.CodecProfile))

        {
            init->info.stream_subtype = UMC::VC1_VIDEO_RCV;
        }
        return MFX_ERR_NONE;
    }
    else
        return MFX_ERR_UNKNOWN;
}
mfxStatus MFXVC1DecCommon::Query(VideoCORE* core, mfxVideoParam *in, mfxVideoParam *out)
{
    mfxStatus sts = MFX_ERR_NONE;
    memset(&out->vpp, 0, sizeof(mfxInfoVPP));
    memset(&out->mfx, 0, sizeof(mfxInfoMFX));

    if (in)
    {
        //out->Version = in->Version;
        //out->mfx.CodingLimits = in->mfx.CodingLimits;
        // frame info
        {
            // mfxFrameInfo
            if (in->mfx.FrameInfo.FourCC == MFX_FOURCC_NV12)
                out->mfx.FrameInfo.FourCC = in->mfx.FrameInfo.FourCC;
            else
                sts = MFX_ERR_UNSUPPORTED;

            if ((in->mfx.FrameInfo.Width % 16 == 0)/*&&
                (in->mfx.FrameInfo.Width <= 4096)*/)
                out->mfx.FrameInfo.Width = in->mfx.FrameInfo.Width;
            else
                sts = MFX_ERR_UNSUPPORTED;

            if ((in->mfx.FrameInfo.Height % 16 == 0)/*&&
                (in->mfx.FrameInfo.Height <= 4096)*/)
                out->mfx.FrameInfo.Height = in->mfx.FrameInfo.Height;
            else
                sts = MFX_ERR_UNSUPPORTED;

            if (in->mfx.FrameInfo.CropX <= out->mfx.FrameInfo.Width)
                out->mfx.FrameInfo.CropX = in->mfx.FrameInfo.CropX;
            else
                sts = MFX_ERR_UNSUPPORTED;

            if (in->mfx.FrameInfo.CropY <= out->mfx.FrameInfo.Height)
                out->mfx.FrameInfo.CropY = in->mfx.FrameInfo.CropY;
            else
                sts = MFX_ERR_UNSUPPORTED;

            if (out->mfx.FrameInfo.CropX + in->mfx.FrameInfo.CropW <= out->mfx.FrameInfo.Width)
                out->mfx.FrameInfo.CropW = in->mfx.FrameInfo.CropW;
            else
                sts = MFX_ERR_UNSUPPORTED;

            if (out->mfx.FrameInfo.CropY + in->mfx.FrameInfo.CropH <= out->mfx.FrameInfo.Height)
                out->mfx.FrameInfo.CropH = in->mfx.FrameInfo.CropH;
            else
                sts = MFX_ERR_UNSUPPORTED;

            out->mfx.FrameInfo.FrameRateExtN = in->mfx.FrameInfo.FrameRateExtN;
            out->mfx.FrameInfo.FrameRateExtD = in->mfx.FrameInfo.FrameRateExtD;

            out->mfx.FrameInfo.AspectRatioW = in->mfx.FrameInfo.AspectRatioW;
            out->mfx.FrameInfo.AspectRatioH = in->mfx.FrameInfo.AspectRatioH;

            //out->mfx.FrameInfo.ChromaDelta = in->mfx.FrameInfo.ChromaDelta;

            switch (in->mfx.FrameInfo.PicStruct)
            {
            case MFX_PICSTRUCT_UNKNOWN:
            case MFX_PICSTRUCT_PROGRESSIVE:
            case MFX_PICSTRUCT_FIELD_TFF:
            case MFX_PICSTRUCT_FIELD_BFF:
            case MFX_PICSTRUCT_FIELD_REPEATED:
            case MFX_PICSTRUCT_FRAME_DOUBLING:
            case MFX_PICSTRUCT_FRAME_TRIPLING:
                out->mfx.FrameInfo.PicStruct = in->mfx.FrameInfo.PicStruct;
                break;
            }

            if ((in->IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY) || (in->IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY) ||
                (in->IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY))
            {
                Ipp32u mask = in->IOPattern & 0xf0;
                if (mask != MFX_IOPATTERN_OUT_VIDEO_MEMORY || mask != MFX_IOPATTERN_OUT_SYSTEM_MEMORY || mask != MFX_IOPATTERN_OUT_OPAQUE_MEMORY)
                    out->IOPattern = in->IOPattern;
            }


            //if (in->mfx.FrameInfo.Step == 1)
            //    out->mfx.FrameInfo.Step = in->mfx.FrameInfo.Step;

            if (in->Protected)
            {
                out->Protected = in->Protected;
                if (core->GetHWType() == MFX_HW_UNKNOWN || !IS_PROTECTION_ANY(in->Protected))
                {
                    sts = MFX_ERR_UNSUPPORTED;
                    out->Protected = 0;
                }

                if (!(in->IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY))
                    sts = MFX_ERR_UNSUPPORTED;

                if (in->NumExtParam > 1)
                    sts = MFX_ERR_UNSUPPORTED;

                mfxExtPAVPOption * pavpOptIn = (mfxExtPAVPOption*)GetExtendedBuffer(in->ExtParam, in->NumExtParam, MFX_EXTBUFF_PAVP_OPTION);
                mfxExtPAVPOption * pavpOptOut = (mfxExtPAVPOption*)GetExtendedBuffer(out->ExtParam, out->NumExtParam, MFX_EXTBUFF_PAVP_OPTION);

                if (IS_PROTECTION_PAVP_ANY(in->Protected) && (!pavpOptIn && in->NumExtParam == 1))
                    sts = MFX_ERR_UNSUPPORTED;

                if (pavpOptIn && pavpOptOut)
                {
                    pavpOptOut->Header.BufferId = pavpOptIn->Header.BufferId;
                    pavpOptOut->Header.BufferSz = pavpOptIn->Header.BufferSz;

                    switch(pavpOptIn->CounterType)
                    {
                    case MFX_PAVP_CTR_TYPE_A:
                    case MFX_PAVP_CTR_TYPE_B:
                    case MFX_PAVP_CTR_TYPE_C:
                        pavpOptOut->CounterType = pavpOptIn->CounterType;
                        break;
                    default:
                        pavpOptOut->CounterType = 0;
                        break;
                    }

                    if (pavpOptIn->EncryptionType != MFX_PAVP_AES128_CBC || pavpOptIn->EncryptionType != MFX_PAVP_AES128_CTR)
                    {
                        pavpOptOut->EncryptionType = pavpOptIn->EncryptionType;
                        if(pavpOptIn->EncryptionType == MFX_PAVP_AES128_CBC)
                        {
                            pavpOptOut->EncryptionType = 0;
                            sts=MFX_ERR_UNSUPPORTED;
                        }

                    }
                    else
                    {
                        pavpOptOut->EncryptionType = 0;
                    }

                    pavpOptOut->CounterIncrement = 0;
                    memset(&pavpOptOut->CipherCounter, 0, sizeof(mfxAES128CipherCounter));
                    memset(pavpOptOut->reserved, 0, sizeof(pavpOptOut->reserved));
                }
                else
                {
                    if (pavpOptOut)
                    {
                        mfxExtBuffer header = pavpOptOut->Header;
                        memset(pavpOptOut, 0, sizeof(mfxExtPAVPOption));
                        pavpOptOut->Header = header;
                    }
                }
            }
            else
            {
                mfxStatus stsExt = CheckDecodersExtendedBuffers(in);
                if (stsExt < MFX_ERR_NONE)
                    sts = MFX_ERR_UNSUPPORTED;

             }

            if (in->mfx.FrameInfo.ChromaFormat == MFX_CHROMAFORMAT_YUV420)
                out->mfx.FrameInfo.ChromaFormat = in->mfx.FrameInfo.ChromaFormat;
            else
                sts = MFX_ERR_UNSUPPORTED;
        }

        if (MFX_CODEC_VC1== in->mfx.CodecId)
            out->mfx.CodecId = MFX_CODEC_VC1;

        if ((MFX_PROFILE_VC1_SIMPLE == in->mfx.CodecProfile)||
            (MFX_PROFILE_VC1_MAIN == in->mfx.CodecProfile)||
            (MFX_PROFILE_VC1_ADVANCED == in->mfx.CodecProfile))
            out->mfx.CodecProfile = in->mfx.CodecProfile;


        // simple/main profiles
        if ((MFX_LEVEL_VC1_LOW == in->mfx.CodecLevel)||
            (MFX_LEVEL_VC1_MEDIAN == in->mfx.CodecLevel)||
            (MFX_LEVEL_VC1_HIGH == in->mfx.CodecLevel))
            out->mfx.CodecLevel = in->mfx.CodecLevel;

        //advanced profile
        if ((MFX_LEVEL_VC1_0 == in->mfx.CodecLevel)||
            (MFX_LEVEL_VC1_1 == in->mfx.CodecLevel)||
            (MFX_LEVEL_VC1_2 == in->mfx.CodecLevel)||
            (MFX_LEVEL_VC1_3 == in->mfx.CodecLevel)||
            (MFX_LEVEL_VC1_4 == in->mfx.CodecLevel))
            out->mfx.CodecLevel = in->mfx.CodecLevel;



        if (in->mfx.DecodedOrder)
            return MFX_ERR_UNSUPPORTED;

        if (in->mfx.NumThread)
            out->mfx.NumThread = in->mfx.NumThread;

        if ((in->IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY) ||
            (in->IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY) ||
            (in->IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY))
            out->IOPattern = in->IOPattern;
        else if (MFX_PLATFORM_SOFTWARE == core->GetPlatformType())
            out->IOPattern = MFX_IOPATTERN_OUT_SYSTEM_MEMORY;
        else
            out->IOPattern = MFX_IOPATTERN_OUT_VIDEO_MEMORY;

        if (in->AsyncDepth < MFX_MAX_ASYNC_DEPTH_VALUE)
            out->AsyncDepth = in->AsyncDepth;

        if (in->mfx.ExtendedPicStruct)
        {
            if (in->mfx.ExtendedPicStruct == 1)
                out->mfx.ExtendedPicStruct = in->mfx.ExtendedPicStruct;
            else
                sts = MFX_ERR_UNSUPPORTED;
        }

        out->mfx.TimeStampCalc = in->mfx.TimeStampCalc;
        out->mfx.SliceGroupsPresent = 0;
    }
    else
    {
        memset(&out->mfx.FrameInfo, 1, sizeof(mfxFrameInfo));
        out->mfx.CodecId = MFX_CODEC_VC1;
        out->mfx.CodecProfile = 1;
        out->mfx.CodecLevel = 1;
        //out->mfx.NumThread = 1;
        out->mfx.NumSlice = 1;
        //out->mfx.CodingLimits = 1;
        out->mfx.ExtendedPicStruct = 1;
        out->mfx.TimeStampCalc = 1;
        out->mfx.SliceGroupsPresent = 0;

        // mfxFrameInfo
        out->mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
        out->mfx.FrameInfo.Width = 16;
        out->mfx.FrameInfo.Height = 16;

        out->mfx.FrameInfo.CropX = 1;
        out->mfx.FrameInfo.CropY = 1;
        out->mfx.FrameInfo.CropW = 1;
        out->mfx.FrameInfo.CropH = 1;
        out->AsyncDepth = 1;
        out->mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
        out->mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
        if (MFX_PLATFORM_SOFTWARE == core->GetPlatformType())
            out->IOPattern = MFX_IOPATTERN_OUT_SYSTEM_MEMORY;
        else
            out->IOPattern = MFX_IOPATTERN_OUT_VIDEO_MEMORY;
    }
    return sts;
}
mfxStatus MFXVC1DecCommon::ParseSeqHeader(mfxBitstream *bs, 
                                          mfxVideoParam *par, 
                                          mfxExtVideoSignalInfo *pVideoSignal,
                                          mfxExtCodingOptionSPSPPS *pSPS)
{
    mfxStatus       MFXSts = MFX_ERR_NONE;

    if (bs->DataLength < 4)
        return MFX_ERR_MORE_DATA;

    // allocated bitstream should be enoudh to parse sequnce header
    mfxU8 pt_data[1024];

    mfxU32* pbs;
    mfxI32  bitOffset = 31;
    mfxI32 SHLen = 0; //in bits
    mfxU32 tempData = GetDWord_s(bs->Data);

    mfxBitstream bs_out = {};

    bool NeedToMapFR = false;
    mfxU32 frCode = 0;

    mfxU16  TargetKbps    = par->mfx.TargetKbps;

    memset(par, 0, sizeof(mfxVideoParam));
    par->mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
    par->mfx.CodecId = MFX_CODEC_VC1;

    par->mfx.TargetKbps = TargetKbps;
    
    // simple profile checking
    // may be need to update
    if (*(bs->Data+3) == 0xC5)
    {
        par->mfx.CodecProfile =  MFX_PROFILE_VC1_MAIN;
    }
    else // probably advanced profile
    {
        bs_out.Data = pt_data;
        bs_out.MaxLength = 1024;
        MFXSts = PrepareSeqHeader(bs, &bs_out);
        if (MFX_ERR_NONE != MFXSts)
        {
            return MFX_ERR_NOT_INITIALIZED;
        }
        if (pSPS)
        {
            if (pSPS->SPSBufSize < bs_out.DataLength)
                return MFX_ERR_NOT_ENOUGH_BUFFER;
            
            memcpy_s(pSPS->SPSBuffer, pSPS->SPSBufSize, bs_out.Data, bs_out.DataLength);
            pSPS->SPSBufSize = (mfxU16)bs_out.DataLength;
        }
        par->mfx.CodecProfile = MFX_PROFILE_VC1_ADVANCED;
    }
    par->mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
    mfxExtVideoSignalInfo  videoSignal = {};
    videoSignal.ColourPrimaries = 1;
    videoSignal.TransferCharacteristics = 1;
    videoSignal.MatrixCoefficients = 6;

    if (MFX_PROFILE_VC1_ADVANCED == par->mfx.CodecProfile)
    {
        SHLen =bs_out.DataLength*8;
        // first 19 bytes are needed only
        mfxU8* pTemp = bs_out.Data + 4; // skip start code

        VC1CheckDataLen(SHLen,32);
        
        //MFX_INTERNAL_CPY(pTemp,bs_out.Data + 4,19);
        UMC::VC1Common::SwapData(pTemp,19);

        pbs = (mfxU32*)pTemp;

        //PROFILE
        VC1CheckDataLen(SHLen,2);
        VC1GetNBits(pbs, bitOffset, 2, tempData);
        

        if(tempData == VC1_PROFILE_ADVANCED)            //PROFILE
        {
            //LEVEL
           VC1CheckDataLen(SHLen,3);
            VC1GetNBits(pbs, bitOffset, 3, tempData);
           
            //CHROMAFORMAT
            VC1CheckDataLen(SHLen,2);
            VC1GetNBits(pbs, bitOffset, 2,tempData);

            //FRMRTQ_POSTPROC
            VC1CheckDataLen(SHLen,3);
            VC1GetNBits(pbs, bitOffset, 3, tempData);

            frCode = tempData;
            //BITRTQ_POSTPROC. BitRate
            VC1CheckDataLen(SHLen,5);
            VC1GetNBits(pbs, bitOffset, 5, tempData);

            par->mfx.TargetKbps = (mfxU16)tempData;

            //for advanced profile
            if ((frCode == 0)
                && (par->mfx.TargetKbps == 31))
            {
                //Post processing indicators for Frame Rate and Bit Rate are undefined
                par->mfx.TargetKbps = 0;
            }
            else if ((frCode == 0)
                && (par->mfx.TargetKbps == 30))
            {
                if (par->mfx.FrameInfo.FrameRateExtN && par->mfx.FrameInfo.FrameRateExtD)
                {
                    mfxF64 frate = (mfxF64)par->mfx.FrameInfo.FrameRateExtN / par->mfx.FrameInfo.FrameRateExtD;
                    if (frate < 1 || frate > 3) // we need to set frame rate is around 2
                    {
                        par->mfx.FrameInfo.FrameRateExtN = 2;
                        par->mfx.FrameInfo.FrameRateExtD = 1;
                    }
                }
                else
                {
                    par->mfx.FrameInfo.FrameRateExtN = 2;
                    par->mfx.FrameInfo.FrameRateExtD = 1;
                }
                if (par->mfx.TargetKbps)
                {
                    if (par->mfx.TargetKbps < 1952)
                        par->mfx.TargetKbps = 1952;
                }
                else
                    par->mfx.TargetKbps = 1952;
            }
            else if ((frCode == 1)
                && (par->mfx.TargetKbps == 31))
            {
                if (par->mfx.FrameInfo.FrameRateExtN && par->mfx.FrameInfo.FrameRateExtD)
                {
                    mfxF64 frate = (mfxF64)par->mfx.FrameInfo.FrameRateExtN / par->mfx.FrameInfo.FrameRateExtD;
                    if (frate < 5 || frate > 6) // we need to set frame rate is around 1
                    {
                        par->mfx.FrameInfo.FrameRateExtN = 6;
                        par->mfx.FrameInfo.FrameRateExtD = 1;
                    }
                }
                else
                {
                    par->mfx.FrameInfo.FrameRateExtN = 6;
                    par->mfx.FrameInfo.FrameRateExtD = 1;
                }
                if (par->mfx.TargetKbps)
                {
                    if (par->mfx.TargetKbps < 2016)
                        par->mfx.TargetKbps = 2016;
                }
                else
                    par->mfx.TargetKbps = 2016;
            }
            else
            {
                if (7 == frCode)
                {
                    if (par->mfx.FrameInfo.FrameRateExtN && par->mfx.FrameInfo.FrameRateExtD)
                    {

                        mfxF64 frate = (mfxF64)par->mfx.FrameInfo.FrameRateExtN / par->mfx.FrameInfo.FrameRateExtD;
                        if (frate < 30) // we need to set frame rate is around or more than 30
                        {
                            par->mfx.FrameInfo.FrameRateExtN = 30;
                            par->mfx.FrameInfo.FrameRateExtD = 1;
                        }
                    }
                    else
                    {
                        par->mfx.FrameInfo.FrameRateExtN = 30;
                        par->mfx.FrameInfo.FrameRateExtD = 1;
                    }
                }
                else
                {

                    par->mfx.FrameInfo.FrameRateExtN = 2 + frCode*4;
                    par->mfx.FrameInfo.FrameRateExtD = 1;
                }


                if (par->mfx.TargetKbps == 31)
                    par->mfx.TargetKbps = 2016;
                else
                    par->mfx.TargetKbps = (32 + par->mfx.TargetKbps * 64);
            }


            //POSTPROCFLAG
            VC1CheckDataLen(SHLen,1);
            VC1GetNBits(pbs, bitOffset, 1, tempData);

            //MAX_CODED_WIDTH
            VC1CheckDataLen(SHLen,12);
            VC1GetNBits(pbs, bitOffset, 12, tempData);

            par->mfx.FrameInfo.Width = (mfxU16)tempData;
            par->mfx.FrameInfo.Width = (par->mfx.FrameInfo.Width +1)*2;

            //MAX_CODED_HEIGHT
            VC1CheckDataLen(SHLen,12);
            VC1GetNBits(pbs, bitOffset, 12, tempData);
            
            par->mfx.FrameInfo.Height = (mfxU16)tempData;
            par->mfx.FrameInfo.Height = (par->mfx.FrameInfo.Height + 1)*2;

            par->mfx.FrameInfo.CropH = par->mfx.FrameInfo.Height;
            par->mfx.FrameInfo.CropW = par->mfx.FrameInfo.Width;

            //PULLDOWN
            VC1CheckDataLen(SHLen,1);
            VC1GetNBits(pbs, bitOffset, 1, tempData);

            //INTERLACE
            VC1CheckDataLen(SHLen,1);
            VC1GetNBits(pbs, bitOffset, 1, tempData);

            if(tempData)
                par->mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_UNKNOWN; // TBD video_info->interlace_type = INTERLEAVED_TOP_FIELD_FIRST;
            else
                par->mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;


            if (MFX_PICSTRUCT_PROGRESSIVE == par->mfx.FrameInfo.PicStruct)
            {
                // sizes should be align on 16 or 32 in case of fields
                // !!!!!!!!!!!!!!!!!!!!!TBD. What is aligh in case of fields
                par->mfx.FrameInfo.Height = UMC::align_value<mfxU16>(par->mfx.FrameInfo.Height, 16);
                par->mfx.FrameInfo.Width = UMC::align_value<mfxU16>(par->mfx.FrameInfo.Width , 16);
            }
            else
            {
                // sizes should be align on 16 or 32 in case of fields
                par->mfx.FrameInfo.Height = UMC::align_value<mfxU16>(par->mfx.FrameInfo.Height, 32);
                par->mfx.FrameInfo.Width = UMC::align_value<mfxU16>(par->mfx.FrameInfo.Width, 16);
            }


            //TFCNTRFLAG
            VC1CheckDataLen(SHLen,1);
            VC1GetNBits(pbs, bitOffset, 1, tempData);

            //FINTERPFLAG
            VC1CheckDataLen(SHLen,1);
            VC1GetNBits(pbs, bitOffset, 1, tempData);

            //reserved
            VC1CheckDataLen(SHLen,2);
            VC1GetNBits(pbs, bitOffset, 2, tempData);

            //DISPLAY_EXT
            VC1CheckDataLen(SHLen,1);
            VC1GetNBits(pbs, bitOffset, 1, tempData);

            if(tempData)   //DISPLAY_EXT
            {
                //DISP_HORIZ_SIZE
                VC1CheckDataLen(SHLen,14);
                VC1GetNBits(pbs, bitOffset, 14,tempData);
                //TBD!!!!! Need to compare the real size with reference
                //par->mfx.FrameInfo.CropW = tempData + 1;

                //DISP_VERT_SIZE
                VC1CheckDataLen(SHLen,14);
                VC1GetNBits(pbs, bitOffset, 14,tempData);
                //TBD!!!!! Need to compare the real size with reference
                //par->mfx.FrameInfo.CropH = tempData + 1;

                //ASPECT_RATIO_FLAG
                VC1CheckDataLen(SHLen,1);
                VC1GetNBits(pbs, bitOffset, 1,tempData);

                if(tempData)    //ASPECT_RATIO_FLAG
                {
                    //ASPECT_RATIO
                    VC1CheckDataLen(SHLen,4);
                    VC1GetNBits(pbs, bitOffset, 4,tempData);

                    if(tempData == 15)
                    {
                        //ASPECT_HORIZ_SIZE
                        VC1CheckDataLen(SHLen,8);
                        VC1GetNBits(pbs, bitOffset, 8, tempData);
                        par->mfx.FrameInfo.AspectRatioH = (mfxU16)tempData;


                        //ASPECT_VERT_SIZE
                        VC1CheckDataLen(SHLen,9);
                        VC1GetNBits(pbs, bitOffset, 8, tempData);
                        par->mfx.FrameInfo.AspectRatioW = (mfxU16)tempData;
                    }
                }

                //FRAMERATE_FLAG
                VC1CheckDataLen(SHLen,1);
                VC1GetNBits(pbs, bitOffset, 1,tempData);

                if(tempData)       //FRAMERATE_FLAG
                {
                    //FRAMERATEIND
                    VC1CheckDataLen(SHLen,1);
                    VC1GetNBits(pbs, bitOffset, 1,tempData);

                    if(!tempData)      //FRAMERATEIND
                    {
                        NeedToMapFR = true;
                        //FRAMERATENR
                        VC1CheckDataLen(SHLen,8);
                        VC1GetNBits(pbs, bitOffset, 8,tempData);
                        par->mfx.FrameInfo.FrameRateExtN = tempData;
                        //FRAMERATEDR
                        VC1CheckDataLen(SHLen,4);
                        VC1GetNBits(pbs, bitOffset, 4,tempData);
                        par->mfx.FrameInfo.FrameRateExtD = tempData;

                    }
                    else
                    {
                        //FRAMERATEEXP
                        VC1CheckDataLen(SHLen,16);
                        VC1GetNBits(pbs, bitOffset, 16,tempData);
                        //par->mfx.FrameInfo.FrameRateCode = mfxU16((tempData + 1)/32.0);
                    }
                }
                //COLOR_FORMAT_FLAG
                VC1CheckDataLen(SHLen,1);
                VC1GetNBits(pbs, bitOffset, 1,tempData);

                if(tempData)       //COLOR_FORMAT_FLAG
                {
                    videoSignal.ColourDescriptionPresent = 1;
                    //COLOR_PRIM
                    VC1CheckDataLen(SHLen,8);
                    VC1GetNBits(pbs, bitOffset, 8,tempData);
                    videoSignal.ColourPrimaries = (mfxU16)tempData;
                    //TRANSFER_CHAR
                    VC1CheckDataLen(SHLen,8);
                    VC1GetNBits(pbs, bitOffset, 8, tempData);
                    videoSignal.TransferCharacteristics = (mfxU16)tempData;
                    //MATRIX_COEF
                    VC1CheckDataLen(SHLen,8);
                    VC1GetNBits(pbs, bitOffset, 8, tempData);
                    videoSignal.MatrixCoefficients = (mfxU16)tempData;
                }
            }

            ////HRD_PARAM_FLAG
            VC1CheckDataLen(SHLen,1);
            VC1GetNBits(pbs, bitOffset, 1, tempData);

            if(tempData)    //HRD_PARAM_FLAG
            {
                mfxU32 hrd_num;
                //HRD_NUM_LEAKY_BUCKETS
                VC1CheckDataLen(SHLen,5);
                VC1GetNBits(pbs, bitOffset, 5,hrd_num);

                //BIT_RATE_EXPONENT
                VC1CheckDataLen(SHLen,4);
                VC1GetNBits(pbs, bitOffset, 4,tempData);

                //BUFFER_SIZE_EXPONENT
                VC1CheckDataLen(SHLen,4);
                VC1GetNBits(pbs, bitOffset, 4,tempData);

                //!!!!!!
                for(mfxU8 i=0; i < hrd_num; i++)//HRD_NUM_LEAKY_BUCKETS
                {
                    //HRD_RATE[i]
                    VC1CheckDataLen(SHLen,16);
                    VC1GetNBits(pbs, bitOffset, 16,tempData);

                    //HRD_BUFFER[i]
                    VC1CheckDataLen(SHLen,16);
                    VC1GetNBits(pbs, bitOffset, 16,tempData);
                }
            }
        }
        // calculate frame rate according to MFX
        if (NeedToMapFR)
        {
            MapFrameRateIntoMfx(par->mfx.FrameInfo.FrameRateExtN,
                                par->mfx.FrameInfo.FrameRateExtD,
                                (Ipp16u)frCode);
            //frCode = MFX_FRAMERATE_24;
        }

    } // Advanced profile
    else
    {
        mfxU32 size = GetDWord_s(bs->Data + 4);
        if (bs->DataLength < size + 16)
        {
            return MFX_ERR_MORE_DATA;
        }
        if (pSPS)
        {
            if (pSPS->SPSBufSize < (size + 16))
                return MFX_ERR_NOT_ENOUGH_BUFFER;

            memcpy_s(pSPS->SPSBuffer, pSPS->SPSBufSize, bs->Data, size + 16);
            pSPS->SPSBufSize = (mfxU16)(size + 16);
        }
        //Simple/Main profile
        par->mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
        par->mfx.FrameInfo.Height = (mfxU16)GetDWord_s(bs->Data + 8 + size);
        par->mfx.FrameInfo.Width = (mfxU16)GetDWord_s(bs->Data + 12 + size);

        // there is no coded cropping
        par->mfx.FrameInfo.CropH = par->mfx.FrameInfo.Height;
        par->mfx.FrameInfo.CropW = par->mfx.FrameInfo.Width;

        // sizes should be aligned
        par->mfx.FrameInfo.Height = UMC::align_value<mfxU16>(par->mfx.FrameInfo.Height, 16);
        par->mfx.FrameInfo.Width = UMC::align_value<mfxU16>(par->mfx.FrameInfo.Width , 16);



        mfxU32 tDWord = GetDWord_s(bs->Data + size);
        pbs = &tDWord;
        UMC::VC1Common::SwapData((mfxU8*)pbs,4);

        //PROFILE
        VC1GetNBits(pbs, bitOffset, 2, tempData);

        //LEVEL
        VC1GetNBits(pbs, bitOffset, 2, tempData);

        //FRMRTQ_POSTPROC
        VC1GetNBits(pbs, bitOffset, 3, tempData);
        frCode = tempData;
        if (7 == frCode)
        {
            if (par->mfx.FrameInfo.FrameRateExtN && par->mfx.FrameInfo.FrameRateExtD)
            {

                mfxF64 frate = (mfxF64)par->mfx.FrameInfo.FrameRateExtN / par->mfx.FrameInfo.FrameRateExtD;
                if (frate < 30) // we need to set frame rate is around or more than 30
                {
                    par->mfx.FrameInfo.FrameRateExtN = 30;
                    par->mfx.FrameInfo.FrameRateExtD = 1;
                }
            }
            else
            {
                par->mfx.FrameInfo.FrameRateExtN = 30;
                par->mfx.FrameInfo.FrameRateExtD = 1;
            }
        }
        else
        {

            par->mfx.FrameInfo.FrameRateExtN = 2 + frCode*4;
            par->mfx.FrameInfo.FrameRateExtD = 1;
        }

        //BITRTQ_POSTPROC
        VC1GetNBits(pbs, bitOffset, 5, tempData);
        par->mfx.TargetKbps = (mfxU16)tempData;

        if (par->mfx.TargetKbps == 31)
            par->mfx.TargetKbps = 2016;
        else
            par->mfx.TargetKbps = (32 + par->mfx.TargetKbps * 64);

        //LOOPFILTER
        VC1GetNBits(pbs, bitOffset, 1, tempData);

        //reserved
        VC1GetNBits(pbs, bitOffset, 1, tempData);

        //MULTIRES
        VC1GetNBits(pbs, bitOffset, 1, tempData);

        //reserved
        VC1GetNBits(pbs, bitOffset, 1, tempData);

        //FASTUVMC
        VC1GetNBits(pbs, bitOffset, 1, tempData);

        //EXTENDED_MV
        VC1GetNBits(pbs, bitOffset, 1, tempData);

        //DQUANT
        VC1GetNBits(pbs, bitOffset, 2, tempData);

        //VSTRANSFORM
        VC1GetNBits(pbs, bitOffset, 1, tempData);

        //reserved
        VC1GetNBits(pbs, bitOffset, 1, tempData);

        //OVERLAP
        VC1GetNBits(pbs, bitOffset, 1, tempData);

        //SYNCMARKER
        VC1GetNBits(pbs, bitOffset, 1, tempData);

        //RANGERED
        VC1GetNBits(pbs, bitOffset, 1, tempData);

        //MAXBFRAMES
        VC1GetNBits(pbs, bitOffset, 3, tempData);

        //QUANTIZER
        VC1GetNBits(pbs, bitOffset, 2, tempData);

        //FINTERPFLAG
        VC1GetNBits(pbs, bitOffset, 1, tempData);
    } // SM profile

    if (pVideoSignal)
    {
        memcpy_s(pVideoSignal, sizeof(videoSignal), &videoSignal, sizeof(videoSignal));
    }
    return MFXSts;

}

mfxStatus MFXVC1DecCommon::PrepareSeqHeader(mfxBitstream *bs_in, mfxBitstream *bs_out)
{
    mfxU32 readBufSize = bs_in->DataLength;
    mfxU8* readBuf = bs_in->Data + bs_in->DataOffset;
    mfxU8* readPos = readBuf;
    mfxU32 readDataSize = 0;

    mfxI32 size = 0;
    mfxU32 a = 0x0000FF00 | (*readPos);
    mfxU32 b = 0xFFFFFFFF;

    bool isFindFirstSC = false;

    while(readPos < (readBuf + readBufSize))
    {
        //find sequence of 0x000001 or 0x000003
        while(!( b == 0x00000001 || b == 0x00000003 )
            &&(++readPos < (readBuf + readBufSize)))
        {
            a = (a<<8)| (mfxI32)(*readPos);
            b = a & 0x00FFFFFF;
        }

        //check end of read buffer
        if(readPos < (readBuf + readBufSize - 1))
        {
            if(*readPos == 0x01)
            {
                if((*(readPos + 1) == VC1_SequenceHeader) && !isFindFirstSC)
                {
                    readPos++;
                    a = 0x00000100 |(mfxI32)(*readPos);
                    b = a & 0x00FFFFFF;
                    isFindFirstSC = true;
                    size = (mfxU32)(readPos - readBuf - readDataSize - 3);
                    readDataSize = readDataSize + size;
                    bs_in->DataOffset += size;
                    bs_in->DataLength -= size;

                }
                else if (isFindFirstSC) // SH start code already find. Complete unit
                {
                    //end of SH
                    readPos = readPos - 2;
                    Ipp8u* ptr = readPos - 1;
                    //trim zero bytes
                    while ( (*ptr==0) && (ptr > readBuf) )
                        ptr--;
                    size = (Ipp32u)(ptr - readBuf - readDataSize +1);
                    if(size + bs_out->DataOffset > bs_out->MaxLength)
                        return MFX_ERR_NOT_ENOUGH_BUFFER;

                    ippsCopy_8u(readBuf + readDataSize, bs_out->Data + bs_out->DataOffset, size);
                    bs_out->DataLength = size;
                    return MFX_ERR_NONE;
                }
                else
                {
                    readPos++;
                    a = 0x00000100 |(mfxI32)(*readPos);
                    b = a & 0x00FFFFFF;
                }
            }
            else
            {
                if (isFindFirstSC)
                {
                    size = (Ipp32u)(readPos - readBuf - readDataSize);

                    if(size + bs_out->DataOffset > bs_out->MaxLength)
                        return MFX_ERR_NOT_ENOUGH_BUFFER;

                    ippsCopy_8u(readBuf + readDataSize, bs_out->Data + bs_out->DataOffset, size);
                    bs_out->DataOffset +=  size;
                    readDataSize = readDataSize + size + 1;
                }
                readPos++;
                a = (a<<8)| (mfxI32)(*readPos);
                b = a & 0x00FFFFFF;

            }
        }
        else if (!isFindFirstSC)
        {
            bs_in->DataOffset +=  bs_in->DataLength - 4;
            bs_in->DataLength = 4;
            return MFX_ERR_MORE_DATA;
        }
        else
        {
            //not found next SC
            if(bs_in->DataFlag & MFX_BITSTREAM_COMPLETE_FRAME ||
                bs_in->DataFlag & MFX_BITSTREAM_EOS)
            {
                if (isFindFirstSC) // SH start code already find. Complete unit
                {
                    //end of SH
                    readPos = readPos - 2;
                    Ipp8u* ptr = readPos - 1;
                    //trim zero bytes
                    while ( (*ptr==0) && (ptr > readBuf) )
                        ptr--;
                    size = (Ipp32u)(ptr - readBuf - readDataSize +1);
                    if(size + bs_out->DataOffset > bs_out->MaxLength)
                        return MFX_ERR_NOT_ENOUGH_BUFFER;

                    ippsCopy_8u(readBuf + readDataSize, bs_out->Data + bs_out->DataOffset, size);
                    bs_out->DataLength = size;
                    return MFX_ERR_NONE;
                }
                else
                {
                    bs_in->DataOffset +=  bs_in->DataLength - 4;
                    bs_in->DataLength = 4;
                    return MFX_ERR_MORE_DATA;                
                }
            }
            else
                return MFX_ERR_MORE_DATA;
        }
    }

    // move pointers and wait for next data
    if (!isFindFirstSC)
    {
        bs_in->DataOffset +=  bs_in->DataLength - 4;
        bs_in->DataLength = 4;
    }
    return MFX_ERR_MORE_DATA;
}
//PRO mode
#if defined (MFX_ENABLE_VC1_VIDEO_BSD) || defined (MFX_ENABLE_VC1_VIDEO_DEC)
mfxStatus MFXVC1DecCommon::FillmfxVideoParams(VC1Context* pContext, mfxVideoParam* pVideoParams)
{
    MFX_CHECK_NULL_PTR1(pContext);

    pVideoParams->mfx.CodecId = MFX_CODEC_VC1;
    pVideoParams->mfx.FrameInfo.Height = 2*(pContext->m_seqLayerHeader->MAX_CODED_HEIGHT + 1);
    pVideoParams->mfx.FrameInfo.Width = 2*(pContext->m_seqLayerHeader->MAX_CODED_WIDTH + 1);
     // Pic Struct Define
//    m_VideoParams.mfx.ColorFormat = MFX_CHROMAFORMAT_YUV420;
    {
        Ipp32s   ProfileTable[4] = {MFX_PROFILE_VC1_SIMPLE,
                                    MFX_PROFILE_VC1_MAIN,
                                    -1,
                                    MFX_PROFILE_VC1_ADVANCED};
        pVideoParams->mfx.CodecProfile = ProfileTable[pContext->m_seqLayerHeader->PROFILE];
    }
    // LEVEL - TBD
    //pVideoParams->mfx.Log2MaxFrameOrder = 16;
    //ConstructNFrame!!!
    return MFX_ERR_NONE;
}

mfxStatus MFXVC1DecCommon::FillmfxPictureParams(VC1Context* pContext, mfxFrameParam* pFrameParam)
{
    MFX_CHECK_NULL_PTR1(pContext);
    MFX_CHECK_NULL_PTR1(pFrameParam);

    // enum. Our
    //{
    //    VC1_MVMODE_HPELBI_1MV    = 0,    //0000      1 MV Half-pel bilinear
    //    VC1_MVMODE_1MV           = 1,    //1         1 MV
    //    VC1_MVMODE_MIXED_MV      = 2,    //01        Mixed MV
    //    VC1_MVMODE_HPEL_1MV      = 3,    //001       1 MV Half-pel
    //    VC1_MVMODE_INTENSCOMP    = 4,    //0001      Intensity Compensation
    //};
    // standard MFX
    //    UnifiedMvMode Description
    //    00b Mixed-MV
    //    01b 1-MV
    //    10b 1-MV half-pel
    //    11b 1-MV half-pel bilinear
    //
    static Ipp32u LTMVMode[4] = {3, 1, 0, 2};
    pFrameParam->VC1.UnifiedMvMode = LTMVMode[pContext->m_picLayerHeader->MVMODE];

    // as same as DXVA2
    if (VC1_PROFILE_ADVANCED == pContext->m_seqLayerHeader->PROFILE)
    {
        pFrameParam->VC1.IsWMVA = 1;
        pFrameParam->VC1.IsWMV9 = 0;
        if (VC1_FieldInterlace == pContext->m_picLayerHeader->FCM)
            pFrameParam->VC1.FrameHinMbMinus1 = (WORD)pContext->m_seqLayerHeader->MAX_CODED_HEIGHT;
        else
            pFrameParam->VC1.FrameHinMbMinus1 = (WORD)(2*(pContext->m_seqLayerHeader->MAX_CODED_HEIGHT + 1) -1);

         pFrameParam->VC1.FrameWinMbMinus1 = (WORD)(2*(pContext->m_seqLayerHeader->MAX_CODED_WIDTH + 1) - 1);
    }
    else
    {
        pFrameParam->VC1.IsWMVA = 0;
        pFrameParam->VC1.IsWMV9 = 1;
        pFrameParam->VC1.IntraResidUnsigned = 1;
        /* Height in MBs minus 1 */
        pFrameParam->VC1.FrameHinMbMinus1 = (WORD)(pContext->m_seqLayerHeader->heightMB - 1);
        /* Width in MBs minus 1 */
        pFrameParam->VC1.FrameWinMbMinus1 = (WORD)(pContext->m_seqLayerHeader->widthMB - 1);
    }

    pFrameParam->VC1.NoResidDiffs = 1;
    pFrameParam->VC1.ScanMethod = MFX_SCANMETHOD_ARBITRARY;

    if (VC1_PROFILE_ADVANCED == pContext->m_seqLayerHeader->PROFILE)
    {
        // interlace fields
        if (VC1_FieldInterlace == pContext->m_picLayerHeader->FCM)
        {
            pFrameParam->VC1.PicExtrapolation = 0x2;
            if (pContext->m_picLayerHeader->CurrField == 0)
            {
                if (pContext->m_seqLayerHeader->PULLDOWN)
                {
                    if (pContext->m_picLayerHeader->TFF)
                        pFrameParam->VC1.BottomFieldFlag= 0;
                    else
                        pFrameParam->VC1.BottomFieldFlag = 1;
                }
                else
                    pFrameParam->VC1.BottomFieldFlag= 0;
            }
            else
            {
                if (pContext->m_picLayerHeader->TFF)
                    pFrameParam->VC1.BottomFieldFlag= 0;
                else
                    pFrameParam->VC1.BottomFieldFlag = 1;
            }
        }
        else  // frames
        {
            if (VC1_Progressive == pContext->m_picLayerHeader->FCM)
                pFrameParam->VC1.PicExtrapolation = 0; //Progressive
            else
                pFrameParam->VC1.PicExtrapolation = 1; //Interlace
        }
    }
    else
    {
        // all frames are progressive
        pFrameParam->VC1.PicExtrapolation = 0;
        pFrameParam->VC1.BottomFieldFlag= 0;
    }

    pFrameParam->VC1.ChromaFormatIdc = MFX_CHROMAFORMAT_YUV420;

    pFrameParam->VC1.SecondFieldFlag = pContext->m_picLayerHeader->CurrField;
    pFrameParam->VC1.IntraPicFlag= 0;


    if (pContext->m_picLayerHeader->CurrField == 0) //first field or progressive picture
    {
        if (VC1_I_FRAME == pContext->m_picLayerHeader->PTYPE)
        {
            pFrameParam->VC1.IntraPicFlag = 1;
            pFrameParam->VC1.FrameType = MFX_FRAMETYPE_I;
            pFrameParam->VC1.FrameType |= MFX_FRAMETYPE_REF;
        }
        else if (VC1_BI_FRAME == pContext->m_picLayerHeader->PTYPE)
        {
            pFrameParam->VC1.FrameType = MFX_FRAMETYPE_I;
        }
        else if (VC1_B_FRAME == pContext->m_picLayerHeader->PTYPE)
        {
            pFrameParam->VC1.BackwardPredFlag = 1;
            pFrameParam->VC1.FrameType = MFX_FRAMETYPE_B;
        }
        else if (VC1_P_FRAME == pContext->m_picLayerHeader->PTYPE)
        {
            pFrameParam->VC1.FrameType = MFX_FRAMETYPE_P;
            pFrameParam->VC1.FrameType |= MFX_FRAMETYPE_REF;
        }
        else if (VC1_SKIPPED_FRAME == pContext->m_picLayerHeader->PTYPE)
        {
            pFrameParam->VC1.FrameType = MFX_FRAMETYPE_S;
            pFrameParam->VC1.FrameType |= MFX_FRAMETYPE_REF;
        }
    }
    else
    {
        if (VC1_I_FRAME == pContext->m_picLayerHeader->PTYPE)
        {
            pFrameParam->VC1.IntraPicFlag = 1;
            pFrameParam->VC1.FrameType2nd = MFX_FRAMETYPE_I;
            pFrameParam->VC1.FrameType2nd |= MFX_FRAMETYPE_REF;
        }
        else if (VC1_BI_FRAME == pContext->m_picLayerHeader->PTYPE)
        {
            pFrameParam->VC1.FrameType2nd = MFX_FRAMETYPE_I;
        }
        else if (VC1_B_FRAME == pContext->m_picLayerHeader->PTYPE)
        {
            pFrameParam->VC1.BackwardPredFlag = 1;
            pFrameParam->VC1.FrameType2nd = MFX_FRAMETYPE_B;
        }
        else if (VC1_P_FRAME == pContext->m_picLayerHeader->PTYPE)
        {
            pFrameParam->VC1.FrameType2nd = MFX_FRAMETYPE_P;
            pFrameParam->VC1.FrameType2nd |= MFX_FRAMETYPE_REF;
        }
    }

    pFrameParam->VC1.BrokenLinkFlag = pContext->m_seqLayerHeader->BROKEN_LINK;
    pFrameParam->VC1.CloseEntryFlag = pContext->m_seqLayerHeader->CLOSED_ENTRY;

    //???
    //IntraResidUnsigned;
    //FrameMbsOnlyFlag;

    pFrameParam->VC1.Pic4MvAllowed = 0;
    if (VC1_FrameInterlace == pContext->m_picLayerHeader->FCM &&
        1 == pContext->m_picLayerHeader->MV4SWITCH &&
        VC1_P_FRAME == pContext->m_picLayerHeader->PTYPE)
    {
        pFrameParam->VC1.Pic4MvAllowed = 1;
    }
    else if (VC1_IS_PRED(pContext->m_picLayerHeader->PTYPE))
    {
        if (VC1_MVMODE_MIXED_MV == pContext->m_picLayerHeader->MVMODE)
            pFrameParam->VC1.Pic4MvAllowed = 1;
    }

    //???
    //PicResampling;
    //ScanMethod
    pFrameParam->VC1.ScanFixed = 1;
    pFrameParam->VC1.ScanMethod = 0; //????????????

    //if (VC1_PROFILE_ADVANCED == pContext->m_seqLayerHeader->PROFILE)
        pFrameParam->VC1.RoundControl = (BYTE)(pContext->m_picLayerHeader->RNDCTRL);
    //else
    //    pFrameParam->VC1.RoundControl = (BYTE)(pContext->m_seqLayerHeader->RNDCTRL);

    //????
    //SpatialRes8;
    //ReferenceIndexed;

    pFrameParam->VC1.PicDeblocked = 0;
    //if (VC1_PROFILE_ADVANCED == pContext->m_seqLayerHeader->PROFILE)
    {
        pFrameParam->VC1.PicDeblocked = bit_set(pFrameParam->VC1.PicDeblocked, 2, 1, pContext->m_seqLayerHeader->LOOPFILTER);
        pFrameParam->VC1.PicDeblocked = bit_set(pFrameParam->VC1.PicDeblocked, 3, 1, pContext->m_picLayerHeader->POSTPROC); // TBD
    }
    if (VC1_B_FRAME != pContext->m_picLayerHeader->PTYPE)
        pFrameParam->VC1.PicDeblocked = bit_set(pFrameParam->VC1.PicDeblocked, 5, 1, pContext->m_seqLayerHeader->OVERLAP); // overlap

    //PicDeblockConfined
    //RangeMapping params
    pFrameParam->VC1.RangeMap = 0;

    if (VC1_PROFILE_ADVANCED == pContext->m_seqLayerHeader->PROFILE)
    {
        if (pContext->m_seqLayerHeader->RANGE_MAPY_FLAG)
        {
            pFrameParam->VC1.RangeMap = bit_set(pFrameParam->VC1.RangeMap, 7, 1, pContext->m_seqLayerHeader->RANGE_MAPY_FLAG);
            pFrameParam->VC1.RangeMap = bit_set(pFrameParam->VC1.RangeMap, 4, 3, pContext->m_seqLayerHeader->RANGE_MAPY);

        }
        if (pContext->m_seqLayerHeader->RANGE_MAPUV_FLAG)
        {
            pFrameParam->VC1.RangeMap = bit_set(pFrameParam->VC1.RangeMap, 3, 1,pContext->m_seqLayerHeader->RANGE_MAPUV_FLAG);
            pFrameParam->VC1.RangeMap = bit_set(pFrameParam->VC1.RangeMap, 0, 3, pContext->m_seqLayerHeader->RANGE_MAPUV);
        }
    }


    pFrameParam->VC1.LumaScale = pContext->m_picLayerHeader->LUMSCALE;
    pFrameParam->VC1.LumaScale2 = pContext->m_picLayerHeader->LUMSCALE1;

    pFrameParam->VC1.LumaShift = pContext->m_picLayerHeader->LUMSHIFT;
    pFrameParam->VC1.LumaShift2 = pContext->m_picLayerHeader->LUMSHIFT1;


    pFrameParam->VC1.RawCodingFlag = 0;

    if (VC1_IS_BITPLANE_RAW_MODE(&pContext->m_picLayerHeader->MVTYPEMB))
        pFrameParam->VC1.RawCodingFlag |= 1;
    if (VC1_IS_BITPLANE_RAW_MODE(&pContext->m_picLayerHeader->m_DirectMB))
        pFrameParam->VC1.RawCodingFlag |= 1 << 1;
    if (VC1_IS_BITPLANE_RAW_MODE(&pContext->m_picLayerHeader->SKIPMB))
        pFrameParam->VC1.RawCodingFlag |= 1 << 2;
    if (VC1_IS_BITPLANE_RAW_MODE(&pContext->m_picLayerHeader->FIELDTX))
        pFrameParam->VC1.RawCodingFlag |= 1 << 3;
    if (VC1_IS_BITPLANE_RAW_MODE(&pContext->m_picLayerHeader->FORWARDMB))
        pFrameParam->VC1.RawCodingFlag |= 1 << 4;
    if (VC1_IS_BITPLANE_RAW_MODE(&pContext->m_picLayerHeader->ACPRED))
        pFrameParam->VC1.RawCodingFlag |= 1 << 5;
    if (VC1_IS_BITPLANE_RAW_MODE(&pContext->m_picLayerHeader->OVERFLAGS))
        pFrameParam->VC1.RawCodingFlag |= 1 << 6;


    pFrameParam->VC1.TTFRM = pContext->m_picLayerHeader->TTFRM_ORIG;
    pFrameParam->VC1.TTMBF = pContext->m_picLayerHeader->TTMBF;
    pFrameParam->VC1.PQuant = pContext->m_picLayerHeader->PQUANT;
    //need to replace with BFRACTION_ORIG
    pFrameParam->VC1.CodedBfraction = pContext->m_picLayerHeader->BFRACTION;

    pFrameParam->VC1.PQuantUniform = 1 - pContext->m_picLayerHeader->QuantizationType;


    //TBD- need to translate into AltPQuantConfig
    pFrameParam->VC1.AltPQuantConfig = pContext->m_picLayerHeader->m_PQuant_mode;

    pFrameParam->VC1.HalfQP = pContext->m_picLayerHeader->HALFQP;
    pFrameParam->VC1.AltPQuant = pContext->m_picLayerHeader->m_AltPQuant;

    pFrameParam->VC1.CBPTable = pContext->m_picLayerHeader->CBPTAB;
    pFrameParam->VC1.TransDCTable = pContext->m_picLayerHeader->TRANSDCTAB;
    pFrameParam->VC1.TransACTable = pContext->m_picLayerHeader->TRANSACFRM;

    if ((VC1_I_FRAME == pContext->m_picLayerHeader->PTYPE)||
        (VC1_BI_FRAME == pContext->m_picLayerHeader->PTYPE))
    {
        pFrameParam->VC1.TransACTable2 = pContext->m_picLayerHeader->TRANSACFRM2;
    }
    else
    {
        pFrameParam->VC1.MbModeTable = pContext->m_picLayerHeader->MBMODETAB;
        pFrameParam->VC1.MvTable = pContext->m_picLayerHeader->MVTAB;
        pFrameParam->VC1.TwoMVBPTable = pContext->m_picLayerHeader->MV2BPTAB;
        pFrameParam->VC1.FourMVBPTable = pContext->m_picLayerHeader->MV4BPTAB;
    }

    pFrameParam->VC1.RefDistance = pContext->m_picLayerHeader->REFDIST;
    pFrameParam->VC1.NumRefPic = pContext->m_picLayerHeader->NUMREF;
    pFrameParam->VC1.RefFieldPicFlag = pContext->m_picLayerHeader->REFFIELD;

    pFrameParam->VC1.FastUVMCflag = pContext->m_seqLayerHeader->FASTUVMC;
    pFrameParam->VC1.FourMvSwitch = pContext->m_picLayerHeader->MV4SWITCH;

    if (pContext->m_bIntensityCompensation)
    {
        static Ipp32u IntCompTtable[3] = {1, 3, 2};
        pFrameParam->VC1.IntCompField = IntCompTtable[pContext->m_picLayerHeader->INTCOMFIELD];
    }
    else
    {
        pFrameParam->VC1.IntCompField = 0;
    }

    //????? May be need to translate
    pFrameParam->VC1.ExtendedMvRange = pContext->m_picLayerHeader->MVRANGE;
    pFrameParam->VC1.ExtendedDMvRange = pContext->m_picLayerHeader->DMVRANGE;

    return MFX_ERR_NONE;
}


// base MFX tasking functionality
VC1TaskMfxBase::VC1TaskMfxBase():m_MBStartRow(0),
                                       m_MBEndRow(0),
                                       m_MBRowsToDecode(0),
                                       m_bIsFirstInSlice(0),
                                       m_bIsLastInSlice(false),
                                       m_bIsReady(false),
                                       m_bIsDone(false),
                                       m_bIsFullMode(false),
                                       m_ThreadOwner(-1),
                                       m_pContext(0),
                                       m_pCUC(0)
{
}

VC1TaskMfxBase::~VC1TaskMfxBase()
{
}

mfxStatus VC1TaskMfxBase::Init(MfxVC1ThreadUnitParams*  pUnitParams,
                               MfxVC1ThreadUnitParams*  pPrevUnitParams,
                               Ipp32s                   threadOwn,
                               bool                     isReadyToProcess)
{

    m_MBStartRow  = pUnitParams->MBStartRow;
    m_MBEndRow = pUnitParams->MBEndRow;
    m_MBRowsToDecode = m_MBEndRow - m_MBStartRow;
    m_bIsFirstInSlice = pUnitParams->isFirstInSlice;
    m_bIsLastInSlice = pUnitParams->isLastInSlice;

    m_bIsReady = isReadyToProcess;
    m_bIsDone = false;

    m_bIsFullMode = pUnitParams->isFullMode;

    m_ThreadOwner = threadOwn;
    m_pCUC = pUnitParams->pCUC;
    m_pContext = pUnitParams->pContext;
    m_pContext->m_pCurrMB = &m_pContext->m_MBs[m_MBStartRow*m_pContext->m_seqLayerHeader->widthMB];

    return MFX_ERR_NONE;
}

UMC::Status MfxVC1TaskProcessor::process()
{
    VC1TaskMfxBase* pSampleTask;
    while(pSampleTask = m_pStore->GetNextStaticTask(0))
    {
        MFX_CHECK_UMC_STS(pSampleTask->ProcessJob());
        pSampleTask->SetTaskAsDone();
    }
    return UMC::UMC_OK;

}
UMC::Status MfxVC1TaskProcessor::processMainThread()
{
    VC1TaskMfxBase* pSampleTask;
    while(!m_pStore->IsFuncComplte()) // May be we already complete processing
    {
        while(pSampleTask = m_pStore->GetNextStaticTask(0))
        {
            MFX_CHECK_UMC_STS(pSampleTask->ProcessJob());
            pSampleTask->SetTaskAsDone();
        }
    }
    return UMC::UMC_OK;
}

//--------------------
#endif
#endif
