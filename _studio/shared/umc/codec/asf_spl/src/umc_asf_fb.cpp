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

#include "umc_asf_fb.h"
#include "umc_linear_buffer.h"
#include "umc_sample_buffer.h"
#include "mfx_utils.h"

namespace UMC
{

#define ASF_ALIGN_VALUE     128

ASFFrameBuffer::ASFFrameBuffer()
{
    m_pCurFrame = new SampleInfo;
    m_pCurFrame->m_dTime = 0;
    m_pCurFrame->m_dTimeAux = 0;
    m_pCurFrame->m_lDataSize = 0;
    m_pCurFrame->m_pbData = NULL;
}

ASFFrameBuffer::~ASFFrameBuffer()
{
    delete m_pCurFrame;
}

Status ASFFrameBuffer::LockInputBuffer(MediaData* in)
{
    std::unique_lock<std::mutex> guard(m_synchro);
    size_t lFreeSize;
    bool bAtEnd = false;

    // check error(s)
    if (NULL == in)
        return UMC_ERR_NULL_PTR;
    if (NULL == m_pbFree)
        return UMC_ERR_NOT_INITIALIZED;

    // get free size
    if (m_pbFree >= m_pbBuffer + (m_lBufferSize - m_lFreeSize))
    {
        lFreeSize = m_pbBuffer + m_lBufferSize - m_pbFree;
        bAtEnd = true;
    }
    else
        lFreeSize = m_lFreeSize;

    // check free size
    if (lFreeSize < m_lInputSize + ASF_ALIGN_VALUE * 2 + sizeof(SampleInfo))
    {
        if (false == bAtEnd)
            return UMC_ERR_NOT_ENOUGH_BUFFER;
        // free space at end is too small
        else
        {
            // when used data is present,
            // concatenate dummy bytes to the last sample info
            if (m_pSamples)
            {
                SampleInfo *pTemp;

                // find last sample info
                pTemp = m_pSamples;
                while (pTemp->m_pNext)
                    pTemp = pTemp->m_pNext;
                pTemp->m_lBufferSize += lFreeSize;

                // update variable(s)
                m_pbFree = m_pbBuffer;
                m_lFreeSize -= lFreeSize;

                // need to call Unlock to avoid double locking of
                // the mutex
                guard.unlock();
                // and call again to lock space at the
                // beginning of the buffer
                return LockInputBuffer(in);
            }
            // when no used data,
            // simply move pointer(s)
            else
            {
                m_pbFree = m_pbBuffer;
                m_pbUsed = m_pbBuffer;
                lFreeSize = m_lFreeSize;
            }
        }
    }

    // set free pointer
    in->SetBufferPointer(m_pbFree + m_pCurFrame->m_lDataSize, lFreeSize - m_pCurFrame->m_lDataSize - ASF_ALIGN_VALUE - sizeof(SampleInfo));
    in->SetDataSize(0);
    return UMC_OK;

} // Status SampleBuffer::LockInputBuffer(MediaData* in)

Status ASFFrameBuffer::UnLockInputBuffer(MediaData *in, Status frameStat)
{
    std::lock_guard<std::mutex> guard(m_synchro);
    size_t lFreeSize = 0;
    SampleInfo *pTemp = NULL;
    Ipp8u *pb = NULL;
    Ipp64f startPTS = 0, endPTS = 0;

    // check for last frame
    if (UMC_ERR_END_OF_STREAM == frameStat) {
      m_bEndOfStream = true;
      return UMC_OK;
    }

    /*** start to add new frame ***/

    if (NULL == in) {
      return UMC_ERR_NULL_PTR;
    }
    if (NULL == m_pbFree) {
      return UMC_ERR_NOT_INITIALIZED;
    }

    // when no data is given
    if (0 == in->GetDataSize())
      return UMC_OK;

    // get free size
    if (m_pbFree >= m_pbBuffer + (m_lBufferSize - m_lFreeSize)) {
      lFreeSize = m_pbBuffer + m_lBufferSize - m_pbFree;
    } else {
      lFreeSize = m_lFreeSize;
    }

    // check free size
    if (lFreeSize < m_lInputSize)
      return UMC_ERR_NOT_ENOUGH_BUFFER;

    // check used data
    if (in->GetDataSize() + ASF_ALIGN_VALUE * 2 + sizeof(SampleInfo) > lFreeSize)
      return UMC_ERR_FAILED;

    // renew current data size
    m_pCurFrame->m_lDataSize += in->GetDataSize();
    in->GetTime(startPTS, endPTS);
    m_pCurFrame->m_dTime = startPTS;
    m_pCurFrame->m_dTimeAux = endPTS;
    m_pCurFrame->m_FrameType = in->GetFrameType();

    if (frameStat == UMC_ERR_NOT_ENOUGH_DATA)
    {
        return UMC_OK;
    }

    // get new sample info
    pb = align_pointer<Ipp8u *> (m_pbFree + m_pCurFrame->m_lDataSize, ASF_ALIGN_VALUE);
    pTemp = reinterpret_cast<SampleInfo *> (pb);

    // fill sample info
    pTemp->m_pbData = m_pbFree;
    pTemp->m_FrameType = m_pCurFrame->m_FrameType;
    pTemp->m_dTime = m_pCurFrame->m_dTime;
    pTemp->m_dTimeAux = m_pCurFrame->m_dTimeAux;
    pTemp->m_lBufferSize = mfx::align2_value(pb + sizeof(SampleInfo) - m_pbFree, ASF_ALIGN_VALUE);
    pTemp->m_lDataSize = m_pCurFrame->m_lDataSize;
    pTemp->m_pNext = NULL;

    // add sample to end of queue
    if (m_pSamples)
    {
        SampleInfo *pWork = m_pSamples;

        while (pWork->m_pNext)
            pWork = pWork->m_pNext;

        pWork->m_pNext = pTemp;
    }
    else
        m_pSamples = pTemp;

    // update variable(s)
    m_pbFree += pTemp->m_lBufferSize;
    if (m_pbBuffer + m_lBufferSize == m_pbFree)
        m_pbFree = m_pbBuffer;

    m_lFreeSize -= pTemp->m_lBufferSize;
    m_lUsedSize += pTemp->m_lDataSize;

    m_pCurFrame->m_dTime = 0;
    m_pCurFrame->m_dTimeAux = 0;
    m_pCurFrame->m_lDataSize = 0;
    m_pCurFrame->m_pbData = NULL;

  return UMC_OK;
}

} // namespace UMC
