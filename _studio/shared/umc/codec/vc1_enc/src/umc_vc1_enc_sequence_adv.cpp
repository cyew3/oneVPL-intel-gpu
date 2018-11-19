// Copyright (c) 2008-2018 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "umc_defs.h"

#if defined (UMC_ENABLE_VC1_VIDEO_ENCODER)

#include "umc_vc1_enc_sequence_adv.h"
namespace UMC_VC1_ENCODER
{
UMC::Status VC1EncoderSequenceADV::WriteSeqHeader(VC1EncoderBitStreamAdv* pCodedSH)
{
    UMC::Status     ret     =   UMC::UMC_OK;
    uint32_t          coded_w =   ((m_uiPictureWidth ) >>1) - 1;
    uint32_t          coded_h =   ((m_uiPictureHeight )>>1) - 1;


    if (!pCodedSH)
        return UMC::UMC_ERR_NULL_PTR;


    ret = pCodedSH->PutStartCode(0x0000010F);
    if (ret != UMC::UMC_OK) return ret;


    ret = pCodedSH->PutBits(0x03,                  2);  //profile
    if (ret != UMC::UMC_OK) return ret;
    ret = pCodedSH->PutBits(m_uiLevel,             3);
    if (ret != UMC::UMC_OK) return ret;
    ret = pCodedSH->PutBits(0x01,                  2);  //chroma format
    if (ret != UMC::UMC_OK) return ret;

    ret = pCodedSH->PutBits(m_uiFrameRateQ        , 3);
    if (ret != UMC::UMC_OK) return ret;
    ret = pCodedSH->PutBits(m_uiBitRateQ          , 5);
    if (ret != UMC::UMC_OK) return ret;

    ret = pCodedSH->PutBits(m_bPostProc           , 1);
    if (ret != UMC::UMC_OK) return ret;

    ret = pCodedSH->PutBits(coded_w               ,  12);
    if (ret != UMC::UMC_OK) return ret;

    ret = pCodedSH->PutBits(coded_h               ,  12);
    if (ret != UMC::UMC_OK) return ret;

    ret = pCodedSH->PutBits(m_bPullDown           , 1);
    if (ret != UMC::UMC_OK) return ret;

    ret = pCodedSH->PutBits(m_bInterlace          , 1);
    if (ret != UMC::UMC_OK) return ret;

    ret = pCodedSH->PutBits(m_bFrameCounter        , 1);
    if (ret != UMC::UMC_OK) return ret;

    ret = pCodedSH->PutBits( m_bFrameInterpolation  , 1);
    if (ret != UMC::UMC_OK) return ret;

    ret = pCodedSH->PutBits(0x03                   , 2);     // reserved
    if (ret != UMC::UMC_OK) return ret;

    ret = pCodedSH->PutBits(m_bDisplayExtention     , 1);
    if (ret != UMC::UMC_OK) return ret;

    assert (!m_bDisplayExtention);

    ret = pCodedSH->PutBits(m_bHRDParams            , 1);
    if (ret != UMC::UMC_OK) return ret;
    assert (!m_bHRDParams);

    ret = pCodedSH->AddLastBits();
    if (ret != UMC::UMC_OK) return ret;

    return WriteEntryPoint(pCodedSH);

}
UMC::Status VC1EncoderSequenceADV::WriteEntryPoint(VC1EncoderBitStreamAdv* pCodedSH)
{
    UMC::Status     ret     =   UMC::UMC_OK;
    uint32_t          coded_w =   (m_uiPictureWidth - 2) >>2;
    uint32_t          coded_h =   (m_uiPictureHeight - 2)>>2;

    //m_uiNumberOfFrames
    //m_bRangeRedution
    //m_bMultiRes
    //m_bSyncMarkers
    //m_uiMaxBFrames


    if (!pCodedSH)
        return UMC::UMC_ERR_NULL_PTR;


    ret = pCodedSH->PutStartCode(0x0000010E);
    if (ret != UMC::UMC_OK) return ret;


    ret = pCodedSH->PutBits(m_bBrokenLink               , 1);
    if (ret != UMC::UMC_OK) return ret;
    ret = pCodedSH->PutBits(m_bClosedEnryPoint          , 1);
    if (ret != UMC::UMC_OK) return ret;
    ret = pCodedSH->PutBits(m_bPanScan                  , 1);
    if (ret != UMC::UMC_OK) return ret;
    ret = pCodedSH->PutBits(m_bReferenceFrameDistance   , 1);
    if (ret != UMC::UMC_OK) return ret;
    ret = pCodedSH->PutBits(m_bLoopFilter         , 1);
    if (ret != UMC::UMC_OK) return ret;
    ret = pCodedSH->PutBits(m_bFastUVMC            ,1);
    if (ret != UMC::UMC_OK) return ret;
    ret = pCodedSH->PutBits(m_bExtendedMV          ,1);
    if (ret != UMC::UMC_OK) return ret;
    ret = pCodedSH->PutBits(m_uiDQuant             ,2);
    if (ret != UMC::UMC_OK) return ret;
    ret = pCodedSH->PutBits(m_bVSTransform         ,1);
    if (ret != UMC::UMC_OK) return ret;
    ret = pCodedSH->PutBits(m_bOverlap             ,1);
    if (ret != UMC::UMC_OK) return ret;

    ret = pCodedSH->PutBits(m_uiQuantazer          ,2);
    if (ret != UMC::UMC_OK) return ret;

    assert (!m_bHRDParams);

    ret = pCodedSH->PutBits(m_bSizeInEntryPoint        ,  1);
    if (ret != UMC::UMC_OK) return ret;

    if (m_bSizeInEntryPoint)
    {
        ret = pCodedSH->PutBits(coded_w               ,  12);
        if (ret != UMC::UMC_OK) return ret;

        ret = pCodedSH->PutBits(coded_h               ,  12);
        if (ret != UMC::UMC_OK) return ret;
    }
    if (m_bExtendedMV)
    {
        ret = pCodedSH->PutBits( m_bExtendedDMV        ,  1);
        if (ret != UMC::UMC_OK) return ret;
    }

    ret = pCodedSH->PutBits(m_iRangeMapY>=0 ,1);
    if (ret != UMC::UMC_OK) return ret;

    if (m_iRangeMapY>=0)
    {
        ret = pCodedSH->PutBits(m_iRangeMapY          ,3);
        if (ret != UMC::UMC_OK) return ret;
    }
    ret = pCodedSH->PutBits(m_iRangeMapUV>=0 ,1);
    if (ret != UMC::UMC_OK) return ret;

    if (m_iRangeMapUV>=0)
    {
        ret = pCodedSH->PutBits(m_iRangeMapUV          ,3);
        if (ret != UMC::UMC_OK) return ret;
    }

    ret = pCodedSH->AddLastBits();
    if (ret != UMC::UMC_OK) return ret;

    return ret;

}



UMC::Status VC1EncoderSequenceADV::CheckParameters(vm_char* pLastError)
{
    UMC::Status ret = UMC::UMC_OK;

    if (!m_uiPictureWidth || !m_uiPictureHeight)
    {
        vm_string_sprintf(pLastError,VM_STRING("Error: seq. header parameter: %d x %d\n"), m_uiPictureWidth, m_uiPictureHeight);
        return UMC::UMC_ERR_NOT_INITIALIZED;
    }

    if (m_uiFrameRateQ >7)
    {
        vm_string_sprintf(pLastError,VM_STRING("Warning: seq. header parameter: FrameRateQ (%d)> 7\n"),m_uiFrameRateQ);
        m_uiFrameRateQ = 7;
    }
    if (m_uiBitRateQ >31)
    {
        vm_string_sprintf(pLastError,VM_STRING("Warning: seq. header parameter: BitRateQ  (%d)> 31\n"),m_uiBitRateQ);
        m_uiBitRateQ = 31;
    }

    if (m_uiDQuant>2)
    {
          vm_string_sprintf(pLastError,VM_STRING("Warning: seq. header parameter: m_uiDQuant (%d) >2 \n"),m_uiDQuant);
          m_uiDQuant = 2;
    }

    if (m_uiQuantazer!= VC1_ENC_QTYPE_IMPL &&
        m_uiQuantazer!= VC1_ENC_QTYPE_EXPL &&
        m_uiQuantazer!= VC1_ENC_QTYPE_UF   &&
        m_uiQuantazer!= VC1_ENC_QTYPE_NUF)
    {
       vm_string_sprintf(pLastError,VM_STRING("Warning: seq. header parameter: Quantazer (%d) > 7\n"),m_uiQuantazer);
       m_uiQuantazer = VC1_ENC_QTYPE_UF;
    }
    if (m_uiLevel !=VC1_ENC_LEVEL_0 &&
        m_uiLevel !=VC1_ENC_LEVEL_1 &&
        m_uiLevel !=VC1_ENC_LEVEL_2 &&
        m_uiLevel !=VC1_ENC_LEVEL_3 &&
        m_uiLevel !=VC1_ENC_LEVEL_4  )
    {
        vm_string_sprintf(pLastError,VM_STRING("Error: seq. header parameter: level\n"));
        return UMC::UMC_ERR_NOT_INITIALIZED;
    }
    if ( m_uiPictureWidth %2 != 0 || m_uiPictureHeight%2 != 0 )
    {
        vm_string_sprintf(pLastError,VM_STRING("Error: seq. header parameter: picture size\n"));
        return UMC::UMC_ERR_NOT_INITIALIZED;
    }
    if (m_bClosedEnryPoint && m_bBrokenLink)
    {
       vm_string_sprintf(pLastError,VM_STRING("Warning: seq. header parameter: BrokenLink && ClosedEnryPoint\n"));
       m_bBrokenLink  = false;
    }
    if (m_iRangeMapY > 7)
    {
        vm_string_sprintf(pLastError,VM_STRING("Error: seq. header parameter: RangeMapY > 7\n"));
        return UMC::UMC_ERR_NOT_INITIALIZED;
    }
    if (m_iRangeMapUV > 7)
    {
        vm_string_sprintf(pLastError,VM_STRING("Error: seq. header parameter: RangeMapUV > 7\n"));
        return UMC::UMC_ERR_NOT_INITIALIZED;
    }
    return ret;

}
UMC::Status  VC1EncoderSequenceADV::Init(UMC::VC1EncoderParams* pParams)
{
    if (!pParams)
        return UMC::UMC_ERR_NULL_PTR;

    m_uiPictureWidth    = pParams->info.clip_info.width;
    m_uiPictureHeight   = pParams->info.clip_info.height;

    m_bVSTransform      =   pParams->m_bVSTransform;        // variable-size transform
    m_bLoopFilter       =   pParams->m_bDeblocking;
    m_bInterlace        =   pParams->m_bInterlace;

    m_bFrameInterpolation       =  false;

    m_uiFrameRateQ      =   0;                  // [0, 7]
    m_uiBitRateQ        =   0;                  // [0,31]
    m_bOverlap          =   (pParams->m_uiOverlapSmoothing != 0);

    switch(pParams->m_uiQuantType)
    {
        case 0:
            m_uiQuantazer = VC1_ENC_QTYPE_IMPL;
            break;
        case 1:
            m_uiQuantazer = VC1_ENC_QTYPE_EXPL;
            break;
        case 2:
            m_uiQuantazer = VC1_ENC_QTYPE_NUF;
            break;
        case 3:
            m_uiQuantazer =   VC1_ENC_QTYPE_UF; // [0,3] - quantizer specification
            break;
        default:
            assert(0);
            break;
    }

    m_uiLevel           =   VC1_ENC_LEVEL_4;    //VC1_ENC_LEVEL_0, VC1_ENC_LEVEL_1,
                                                //VC1_ENC_LEVEL_2, VC1_ENC_LEVEL_3, VC1_ENC_LEVEL_4,
    m_bConstBitRate     =   false;
    m_uiHRDBufferSize   =   1;                  // buffersize in milliseconds [1,65536]
    m_uiHRDFrameRate    =   1;                  // rate: bits per seconds [1,65536]
    m_uiFrameRate       =   30;                 // 0xffffffff - if unknown


    m_bFastUVMC         =   pParams->m_bFastUVMC;
    m_bExtendedMV       =   false;
    m_uiDQuant          =   0;


    // only for advance profile

    m_bClosedEnryPoint          = true;  // true -false
    m_bBrokenLink               = false; // if !m_bClosedEnryPoint -> true or false
    m_bPanScan                  = false;
    m_bReferenceFrameDistance   = false;
    m_bExtendedDMV              = false;
    m_bSizeInEntryPoint         = false;
    m_iRangeMapY                = -1;   // [0,7]
    m_iRangeMapUV               = -1;   // [0,7]
    m_bPullDown                 = false;
    m_bFrameCounter             = false;
    m_bDisplayExtention         = false;
    m_bHRDParams                = false;
    m_bPostProc                 = false;

    return UMC::UMC_OK;
}

}
#endif //defined (UMC_ENABLE_VC1_VIDEO_ENCODER)
