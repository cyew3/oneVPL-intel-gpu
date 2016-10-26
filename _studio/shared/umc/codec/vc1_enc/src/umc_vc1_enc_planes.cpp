//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2008 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"

#if defined (UMC_ENABLE_VC1_VIDEO_ENCODER)

#include "ippvc.h"
#include "ippi.h"
#include "umc_vc1_enc_def.h"
#include "umc_vc1_enc_common.h"
#include "umc_vc1_enc_debug.h"
#include "umc_vc1_enc_planes.h"


namespace UMC_VC1_ENCODER
{
Ipp32u Frame::CalcAllocatedMemSize(Ipp32u w, Ipp32u h, Ipp32u paddingSize, bool bUdata)
{
    Ipp32u size = 0;
    Ipp32u wMB = (w+15)>>4;
    Ipp32u hMB = (h+15)>>4;

    Ipp32u stepY =  UMC::align_value<Ipp32u>(wMB*VC1_ENC_LUMA_SIZE   + 2*paddingSize);
    Ipp32u stepUV=  UMC::align_value<Ipp32u>(wMB*VC1_ENC_CHROMA_SIZE + paddingSize);


    //Y
    size += UMC::align_value<Ipp32u>(stepY  * (hMB*VC1_ENC_LUMA_SIZE + 2*paddingSize)
                    + 32 + stepY * 4);
    //U
    size += UMC::align_value<Ipp32u>(stepUV  * (hMB*VC1_ENC_CHROMA_SIZE+ paddingSize)
                    + 32 + stepUV * 4);
    //V
    size += UMC::align_value<Ipp32u>(stepUV  * (hMB*VC1_ENC_CHROMA_SIZE+ paddingSize)
                    + 32 + stepUV * 4);
    if (bUdata)
    {
        size += w*h;
    }
    
    return size;
}

UMC::Status Frame::Init(Ipp8u* pBuffer, Ipp32s memSize, Ipp32s w, Ipp32s h, Ipp32u paddingSize, bool bUdata )
{
    Ipp32s wMB = (w+15)>>4;
    Ipp32s hMB = (h+15)>>4;

    Close();

    if(!pBuffer)
        return UMC::UMC_ERR_NULL_PTR;

    if(memSize == 0)
        return UMC::UMC_ERR_NOT_ENOUGH_BUFFER;

    m_widthYPlane = w;
    m_widthUVPlane = w>>1;

    m_heightYPlane = h;
    m_heightUVPlane = h>>1;

    m_paddingSize = paddingSize;

    m_stepY =  UMC::align_value<Ipp32u>(wMB*VC1_ENC_LUMA_SIZE   + 2*m_paddingSize);
    m_stepUV=  UMC::align_value<Ipp32u>(wMB*VC1_ENC_CHROMA_SIZE + m_paddingSize);

    //Y
    m_pYFrame = pBuffer;
    pBuffer += UMC::align_value<Ipp32u>(m_stepY  * (hMB*VC1_ENC_LUMA_SIZE + 2*m_paddingSize)
                    + 32 + m_stepY * 4);
    memSize -= UMC::align_value<Ipp32u>(m_stepY  * (hMB*VC1_ENC_LUMA_SIZE + 2*m_paddingSize)
                    + 32 + m_stepY * 4);

    if(memSize <= 0)
        return UMC::UMC_ERR_NOT_ENOUGH_BUFFER;

    //U
    m_pUFrame = pBuffer;
    pBuffer += UMC::align_value<Ipp32u>(m_stepUV  * (hMB*VC1_ENC_CHROMA_SIZE+ m_paddingSize)
                    + 32 + m_stepUV * 4);
    memSize -= UMC::align_value<Ipp32u>(m_stepUV  * (hMB*VC1_ENC_CHROMA_SIZE+ m_paddingSize)
                    + 32 + m_stepUV * 4);

    if(memSize <= 0)
        return UMC::UMC_ERR_NOT_ENOUGH_BUFFER;


     //V
    m_pVFrame = pBuffer;
    pBuffer += UMC::align_value<Ipp32u>(m_stepUV  * (hMB*VC1_ENC_CHROMA_SIZE+ m_paddingSize)
                    + 32 + m_stepUV * 4);
    memSize -= UMC::align_value<Ipp32u>(m_stepUV  * (hMB*VC1_ENC_CHROMA_SIZE+ m_paddingSize)
                    + 32 + m_stepUV * 4);

    if(memSize < 0)
        return UMC::UMC_ERR_NOT_ENOUGH_BUFFER;

    m_pYPlane = UMC::align_pointer<Ipp8u*>( m_pYFrame + m_stepY*(m_paddingSize + 2) + m_paddingSize);
    m_pUPlane = UMC::align_pointer<Ipp8u*>( m_pUFrame + m_stepUV*((m_paddingSize>>1) + 2) + (m_paddingSize>>1));
    m_pVPlane = UMC::align_pointer<Ipp8u*>( m_pVFrame + m_stepUV*((m_paddingSize>>1) + 2) + (m_paddingSize>>1));

    if (bUdata)
    {
        if (memSize < w*h)
        {
            return UMC::UMC_ERR_NOT_ENOUGH_BUFFER;
        }
        m_UDataBufferSize = w*h;
        m_UDataBuffer = pBuffer;
        pBuffer += m_UDataBufferSize;
        memSize -= m_UDataBufferSize;
    }
    
    return UMC::UMC_OK;
}
UMC::Status FrameNV12::Init(Ipp8u* pBuffer, Ipp32s memSize, Ipp32s w, Ipp32s h, Ipp32u paddingSize, bool bUdata )
{
    Ipp32s wMB = (w+15)>>4;
    Ipp32s hMB = (h+15)>>4;

    Close();

    if(!pBuffer)
        return UMC::UMC_ERR_NULL_PTR;

    if(memSize == 0)
        return UMC::UMC_ERR_NOT_ENOUGH_BUFFER;

    m_widthYPlane = w;
    m_widthUVPlane= w;

    m_heightYPlane = h;
    m_heightUVPlane =h>>1;

    m_paddingSize = paddingSize;

    m_stepY  =  UMC::align_value<Ipp32u>(wMB*VC1_ENC_LUMA_SIZE   + 2*m_paddingSize);
    m_stepUV =  m_stepY;

    //Y
    m_pYFrame = pBuffer;
    pBuffer += UMC::align_value<Ipp32u>(m_stepY  * (hMB*VC1_ENC_LUMA_SIZE + 2*m_paddingSize)
        + 32 + m_stepY * 4);
    memSize -= UMC::align_value<Ipp32u>(m_stepY  * (hMB*VC1_ENC_LUMA_SIZE + 2*m_paddingSize)
        + 32 + m_stepY * 4);

    if(memSize <= 0)
        return UMC::UMC_ERR_NOT_ENOUGH_BUFFER;

    //U
    m_pUFrame = pBuffer;
    pBuffer += UMC::align_value<Ipp32u>(m_stepUV  * (hMB*VC1_ENC_CHROMA_SIZE+ m_paddingSize)
        + 32 + m_stepUV * 4);
    memSize -= UMC::align_value<Ipp32u>(m_stepUV  * (hMB*VC1_ENC_CHROMA_SIZE+ m_paddingSize)
        + 32 + m_stepUV * 4);

    if(memSize <= 0)
        return UMC::UMC_ERR_NOT_ENOUGH_BUFFER;


    //V
    m_pVFrame = m_pUFrame + 1;

    m_pYPlane = UMC::align_pointer<Ipp8u*>( m_pYFrame + m_stepY*(m_paddingSize + 2) + m_paddingSize);
    m_pUPlane = UMC::align_pointer<Ipp8u*>( m_pUFrame + m_stepUV*((m_paddingSize>>1) + 2) + (m_paddingSize));
    m_pVPlane = m_pUPlane + 1;

    if (bUdata)
    {
        if (memSize < w*h)
        {
            return UMC::UMC_ERR_NOT_ENOUGH_BUFFER;
        }
        m_UDataBufferSize = w*h;
        m_UDataBuffer = pBuffer;
        pBuffer += m_UDataBufferSize;
        memSize -= m_UDataBufferSize;
    }

    return UMC::UMC_OK;
}
void Frame::Close()
{
    m_pYFrame = 0;
    m_pUFrame = 0;
    m_pVFrame = 0;
    m_stepY = 0;
    m_stepUV = 0;

    m_pYPlane = 0;
    m_pUPlane = 0;
    m_pVPlane = 0;

    m_widthYPlane = 0;
    m_widthUVPlane = 0;

    m_heightYPlane = 0;
    m_heightUVPlane = 0;

    m_paddingSize = 0;
    m_bTaken = false;
}



UMC::Status Frame::CopyPlane ( Ipp8u* pYPlane, Ipp32u stepY,
                               Ipp8u* pUPlane, Ipp32u stepU,
                               Ipp8u* pVPlane, Ipp32u stepV,
                               ePType pictureType)
{
    IppiSize            sizeLuma        = {m_widthYPlane, m_heightYPlane};
    IppiSize            sizeChroma      = {m_widthUVPlane,m_heightUVPlane};

IPP_STAT_START_TIME(m_IppStat->IppStartTime);
    ippiCopy_8u_C1R(pYPlane,stepY, m_pYPlane,m_stepY, sizeLuma);
    ippiCopy_8u_C1R(pUPlane,stepU,m_pUPlane,m_stepUV, sizeChroma);
    ippiCopy_8u_C1R(pVPlane,stepV,m_pVPlane,m_stepUV, sizeChroma);
IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);

    m_bTaken      = true;
    m_pictureType = pictureType;
    return UMC::UMC_OK;

}
UMC::Status FrameNV12::CopyPlane (  Ipp8u* pYPlane, Ipp32u stepY,
                                    Ipp8u* pUPlane, Ipp32u stepU,
                                    Ipp8u* pVPlane, Ipp32u stepV,
                                    ePType pictureType)
{
    IppiSize            sizeLuma        = {m_widthYPlane, m_heightYPlane};
    IppiSize            sizeChroma      = {m_widthUVPlane,m_heightUVPlane};

    IPP_STAT_START_TIME(m_IppStat->IppStartTime);
    ippiCopy_8u_C1R(pYPlane,stepY, m_pYPlane,m_stepY, sizeLuma);
    ippiCopy_8u_C1R(pUPlane,stepU,m_pUPlane,m_stepUV, sizeChroma);
    IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);

    m_bTaken      = true;
    m_pictureType = pictureType;
    return UMC::UMC_OK;

}
UMC::Status Frame::CopyPlane ( Frame * fr)
{
    IppiSize            sizeLuma        = {m_widthYPlane, m_heightYPlane};
    IppiSize            sizeChroma      = {m_widthUVPlane,m_heightUVPlane};

IPP_STAT_START_TIME(m_IppStat->IppStartTime);
    ippiCopy_8u_C1R(fr->m_pYPlane,fr->m_stepY, m_pYPlane,m_stepY, sizeLuma);
    ippiCopy_8u_C1R(fr->m_pUPlane,fr->m_stepUV,m_pUPlane,m_stepUV, sizeChroma);
    ippiCopy_8u_C1R(fr->m_pVPlane,fr->m_stepUV,m_pVPlane,m_stepUV, sizeChroma);
IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);

    m_bTaken      = true;
    m_pictureType = fr->m_pictureType;
    return UMC::UMC_OK;
}
UMC::Status FrameNV12::CopyPlane ( Frame * fr)
{
    IppiSize            sizeLuma        = {m_widthYPlane, m_heightYPlane};
    IppiSize            sizeChroma      = {m_widthUVPlane,m_heightUVPlane};

    IPP_STAT_START_TIME(m_IppStat->IppStartTime);
    ippiCopy_8u_C1R(fr->GetYPlane(),fr->GetYStep(), m_pYPlane,m_stepY, sizeLuma);
    ippiCopy_8u_C1R(fr->GetUPlane(),fr->GetUStep(), m_pUPlane,m_stepUV, sizeChroma);
    IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);

    m_bTaken      = true;
    m_pictureType = fr->GetPictureType();
    return UMC::UMC_OK;
}

//////////////////////////////////////////////////////////////////////////////////////
Ipp32u BufferedFrames::CalcAllocatedMemSize(Ipp32u w, Ipp32u h, Ipp32u paddingSize, Ipp32u n)
{
    Ipp32u memSize = 0;
    memSize += UMC::align_value<Ipp32u>(n*sizeof(Frame));
    memSize += n*UMC::align_value<Ipp32u>(Frame::CalcAllocatedMemSize(w, h, paddingSize));

    return memSize;
}

UMC::Status BufferedFrames::Init (Ipp8u* pBuffer, Ipp32u memSize, Ipp32u w, Ipp32u h, Ipp32u paddingSize, Ipp32u n)
{
    Ipp32u i;
    UMC::Status err = UMC::UMC_OK;

    Close();

    if(!pBuffer)
        return UMC::UMC_ERR_NULL_PTR;

    if(memSize == 0)
        return UMC::UMC_ERR_NOT_ENOUGH_BUFFER;

    m_pFrames = new (pBuffer) Frame[n];
    if (!m_pFrames)
        return UMC::UMC_ERR_ALLOC;

    pBuffer += UMC::align_value<Ipp32u>(n * sizeof(Frame));
    memSize -= UMC::align_value<Ipp32u>(n * sizeof(Frame));

    if(memSize < 0)
        return UMC::UMC_ERR_NOT_ENOUGH_BUFFER;

    m_bufferSize    = n;

    Ipp32u frameSize = UMC::align_value<Ipp32u>(Frame::CalcAllocatedMemSize(w,h,paddingSize));
    for (i = 0; i < n ; i++)
    {
        err = m_pFrames[i].Init (pBuffer, memSize, w,h,paddingSize);
        if (err != UMC::UMC_OK)
            return err;
        pBuffer += frameSize;
        memSize -= frameSize;
        if(memSize < 0)
            return UMC::UMC_ERR_NOT_ENOUGH_BUFFER;
    }
    return err;
}
void BufferedFrames::Close ()
{
    if (m_pFrames)
    {
        delete [] m_pFrames;
        m_pFrames = 0;
    }
    m_bufferSize        = 0;
    m_nBuffered         = 0;
    m_currFrameIndex    = 0;
    m_bClosed           = false;
}
UMC::Status   BufferedFrames::SaveFrame (   Ipp8u* pYPlane, Ipp32u stepY,
                                            Ipp8u* pUPlane, Ipp32u stepU,
                                            Ipp8u* pVPlane, Ipp32u stepV )
{
    if (m_currFrameIndex)
    {
        // frames from prev. seq. - ERROR;
        return UMC::UMC_ERR_FAILED;
    }

    if (m_nBuffered < m_bufferSize )
    {
        m_pFrames[m_nBuffered].CopyPlane(pYPlane,stepY,pUPlane,stepU,pVPlane,stepV);
        m_bClosed = false;
        m_nBuffered++;
        return UMC::UMC_OK;
    }
    else
    {
        return UMC::UMC_ERR_NOT_ENOUGH_BUFFER;
    }
};
UMC::Status   BufferedFrames::GetFrame (Frame** currFrame)
{
    *currFrame = 0;
    if (!m_bClosed)
    {
        //sequence isn't closed - ERROR;
        return UMC::UMC_ERR_FAILED;
    }
    if (m_nBuffered)
    {
        *currFrame = &(m_pFrames[m_currFrameIndex]);
        m_nBuffered --;
        m_currFrameIndex = (m_nBuffered) ? (m_currFrameIndex++) : 0;
        return UMC::UMC_OK;
    }
    else
    {
        return UMC::UMC_ERR_NOT_ENOUGH_DATA;
    }
}
UMC::Status BufferedFrames::GetReferenceFrame (Frame** currFrame)
{
    Ipp32u index = 0;
    *currFrame = 0;

    if (m_bClosed)
    {
        //sequence is closed - ERROR;
        return UMC::UMC_ERR_FAILED;
    }
    if (m_nBuffered)
    {
        index = m_currFrameIndex + m_nBuffered - 1;
        *currFrame = &(m_pFrames[m_currFrameIndex]);
        m_bClosed = true;
        m_nBuffered -- ;
        m_currFrameIndex = (m_nBuffered) ? m_currFrameIndex : 0;
        return UMC::UMC_OK;
    }
    else
    {
        return UMC::UMC_ERR_NOT_ENOUGH_DATA;
    }
}
UMC::Status  BufferedFrames::ReferenceFrame()
{
    if (m_currFrameIndex)
    {
         return UMC::UMC_ERR_FAILED;
    }
    if (m_nBuffered)
    {
        m_bClosed = true;
    }
    return UMC::UMC_OK;
}

}
#endif // defined (UMC_ENABLE_VC1_VIDEO_ENCODER)
