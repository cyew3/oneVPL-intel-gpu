// Copyright (c) 2008-2019 Intel Corporation
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

#include "ippvc.h"
#include "ippi.h"
#include "umc_vc1_enc_def.h"
#include "umc_vc1_enc_common.h"
#include "umc_vc1_enc_debug.h"
#include "umc_vc1_enc_planes.h"


namespace UMC_VC1_ENCODER
{
uint32_t Frame::CalcAllocatedMemSize(uint32_t w, uint32_t h, uint32_t paddingSize, bool bUdata)
{
    uint32_t size = 0;
    uint32_t wMB = (w+15)>>4;
    uint32_t hMB = (h+15)>>4;

    uint32_t stepY =  mfx::align_value(wMB*VC1_ENC_LUMA_SIZE   + 2*paddingSize);
    uint32_t stepUV=  mfx::align_value(wMB*VC1_ENC_CHROMA_SIZE + paddingSize);


    //Y
    size += mfx::align_value(stepY  * (hMB*VC1_ENC_LUMA_SIZE + 2*paddingSize)
                    + 32 + stepY * 4);
    //U
    size += mfx::align_value(stepUV  * (hMB*VC1_ENC_CHROMA_SIZE+ paddingSize)
                    + 32 + stepUV * 4);
    //V
    size += mfx::align_value(stepUV  * (hMB*VC1_ENC_CHROMA_SIZE+ paddingSize)
                    + 32 + stepUV * 4);
    if (bUdata)
    {
        size += w*h;
    }
    
    return size;
}

UMC::Status Frame::Init(uint8_t* pBuffer, int32_t memSize, int32_t w, int32_t h, uint32_t paddingSize, bool bUdata )
{
    int32_t wMB = (w+15)>>4;
    int32_t hMB = (h+15)>>4;

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

    m_stepY =  mfx::align_value(wMB*VC1_ENC_LUMA_SIZE   + 2*m_paddingSize);
    m_stepUV=  mfx::align_value(wMB*VC1_ENC_CHROMA_SIZE + m_paddingSize);

    //Y
    m_pYFrame = pBuffer;
    pBuffer += mfx::align_value(m_stepY  * (hMB*VC1_ENC_LUMA_SIZE + 2*m_paddingSize)
                    + 32 + m_stepY * 4);
    memSize -= mfx::align_value(m_stepY  * (hMB*VC1_ENC_LUMA_SIZE + 2*m_paddingSize)
                    + 32 + m_stepY * 4);

    if(memSize <= 0)
        return UMC::UMC_ERR_NOT_ENOUGH_BUFFER;

    //U
    m_pUFrame = pBuffer;
    pBuffer += mfx::align_value(m_stepUV  * (hMB*VC1_ENC_CHROMA_SIZE+ m_paddingSize)
                    + 32 + m_stepUV * 4);
    memSize -= mfx::align_value(m_stepUV  * (hMB*VC1_ENC_CHROMA_SIZE+ m_paddingSize)
                    + 32 + m_stepUV * 4);

    if(memSize <= 0)
        return UMC::UMC_ERR_NOT_ENOUGH_BUFFER;


     //V
    m_pVFrame = pBuffer;
    pBuffer += mfx::align_value(m_stepUV  * (hMB*VC1_ENC_CHROMA_SIZE+ m_paddingSize)
                    + 32 + m_stepUV * 4);
    memSize -= mfx::align_value(m_stepUV  * (hMB*VC1_ENC_CHROMA_SIZE+ m_paddingSize)
                    + 32 + m_stepUV * 4);

    if(memSize < 0)
        return UMC::UMC_ERR_NOT_ENOUGH_BUFFER;

    m_pYPlane = UMC::align_pointer<uint8_t*>( m_pYFrame + m_stepY*(m_paddingSize + 2) + m_paddingSize);
    m_pUPlane = UMC::align_pointer<uint8_t*>( m_pUFrame + m_stepUV*((m_paddingSize>>1) + 2) + (m_paddingSize>>1));
    m_pVPlane = UMC::align_pointer<uint8_t*>( m_pVFrame + m_stepUV*((m_paddingSize>>1) + 2) + (m_paddingSize>>1));

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
UMC::Status FrameNV12::Init(uint8_t* pBuffer, int32_t memSize, int32_t w, int32_t h, uint32_t paddingSize, bool bUdata )
{
    int32_t wMB = (w+15)>>4;
    int32_t hMB = (h+15)>>4;

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

    m_stepY  =  mfx::align_value(wMB*VC1_ENC_LUMA_SIZE   + 2*m_paddingSize);
    m_stepUV =  m_stepY;

    //Y
    m_pYFrame = pBuffer;
    pBuffer += mfx::align_value(m_stepY  * (hMB*VC1_ENC_LUMA_SIZE + 2*m_paddingSize)
        + 32 + m_stepY * 4);
    memSize -= mfx::align_value(m_stepY  * (hMB*VC1_ENC_LUMA_SIZE + 2*m_paddingSize)
        + 32 + m_stepY * 4);

    if(memSize <= 0)
        return UMC::UMC_ERR_NOT_ENOUGH_BUFFER;

    //U
    m_pUFrame = pBuffer;
    pBuffer += mfx::align_value(m_stepUV  * (hMB*VC1_ENC_CHROMA_SIZE+ m_paddingSize)
        + 32 + m_stepUV * 4);
    memSize -= mfx::align_value(m_stepUV  * (hMB*VC1_ENC_CHROMA_SIZE+ m_paddingSize)
        + 32 + m_stepUV * 4);

    if(memSize <= 0)
        return UMC::UMC_ERR_NOT_ENOUGH_BUFFER;


    //V
    m_pVFrame = m_pUFrame + 1;

    m_pYPlane = UMC::align_pointer<uint8_t*>( m_pYFrame + m_stepY*(m_paddingSize + 2) + m_paddingSize);
    m_pUPlane = UMC::align_pointer<uint8_t*>( m_pUFrame + m_stepUV*((m_paddingSize>>1) + 2) + (m_paddingSize));
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



UMC::Status Frame::CopyPlane ( uint8_t* pYPlane, uint32_t stepY,
                               uint8_t* pUPlane, uint32_t stepU,
                               uint8_t* pVPlane, uint32_t stepV,
                               ePType pictureType)
{
    mfxSize            sizeLuma        = {m_widthYPlane, m_heightYPlane};
    mfxSize            sizeChroma      = {m_widthUVPlane,m_heightUVPlane};

IPP_STAT_START_TIME(m_IppStat->IppStartTime);
    ippiCopy_8u_C1R(pYPlane,stepY, m_pYPlane,m_stepY, sizeLuma);
    ippiCopy_8u_C1R(pUPlane,stepU,m_pUPlane,m_stepUV, sizeChroma);
    ippiCopy_8u_C1R(pVPlane,stepV,m_pVPlane,m_stepUV, sizeChroma);
IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);

    m_bTaken      = true;
    m_pictureType = pictureType;
    return UMC::UMC_OK;

}
UMC::Status FrameNV12::CopyPlane (  uint8_t* pYPlane, uint32_t stepY,
                                    uint8_t* pUPlane, uint32_t stepU,
                                    uint8_t* pVPlane, uint32_t stepV,
                                    ePType pictureType)
{
    mfxSize            sizeLuma        = {m_widthYPlane, m_heightYPlane};
    mfxSize            sizeChroma      = {m_widthUVPlane,m_heightUVPlane};

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
    mfxSize            sizeLuma        = {m_widthYPlane, m_heightYPlane};
    mfxSize            sizeChroma      = {m_widthUVPlane,m_heightUVPlane};

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
    mfxSize            sizeLuma        = {m_widthYPlane, m_heightYPlane};
    mfxSize            sizeChroma      = {m_widthUVPlane,m_heightUVPlane};

    IPP_STAT_START_TIME(m_IppStat->IppStartTime);
    ippiCopy_8u_C1R(fr->GetYPlane(),fr->GetYStep(), m_pYPlane,m_stepY, sizeLuma);
    ippiCopy_8u_C1R(fr->GetUPlane(),fr->GetUStep(), m_pUPlane,m_stepUV, sizeChroma);
    IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);

    m_bTaken      = true;
    m_pictureType = fr->GetPictureType();
    return UMC::UMC_OK;
}

//////////////////////////////////////////////////////////////////////////////////////
uint32_t BufferedFrames::CalcAllocatedMemSize(uint32_t w, uint32_t h, uint32_t paddingSize, uint32_t n)
{
    uint32_t memSize = 0;
    memSize += mfx::align_value(n*sizeof(Frame));
    memSize += n*mfx::align_value(Frame::CalcAllocatedMemSize(w, h, paddingSize));

    return memSize;
}

UMC::Status BufferedFrames::Init (uint8_t* pBuffer, uint32_t memSize, uint32_t w, uint32_t h, uint32_t paddingSize, uint32_t n)
{
    uint32_t i;
    UMC::Status err = UMC::UMC_OK;

    Close();

    if(!pBuffer)
        return UMC::UMC_ERR_NULL_PTR;

    if(memSize == 0)
        return UMC::UMC_ERR_NOT_ENOUGH_BUFFER;

    m_pFrames = new (pBuffer) Frame[n];
    if (!m_pFrames)
        return UMC::UMC_ERR_ALLOC;

    pBuffer += mfx::align_value(n * sizeof(Frame));
    memSize -= mfx::align_value(n * sizeof(Frame));

    if(memSize < 0)
        return UMC::UMC_ERR_NOT_ENOUGH_BUFFER;

    m_bufferSize    = n;

    uint32_t frameSize = mfx::align_value(Frame::CalcAllocatedMemSize(w,h,paddingSize));
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
UMC::Status   BufferedFrames::SaveFrame (   uint8_t* pYPlane, uint32_t stepY,
                                            uint8_t* pUPlane, uint32_t stepU,
                                            uint8_t* pVPlane, uint32_t stepV )
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
    uint32_t index = 0;
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
