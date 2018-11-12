// Copyright (c) 2003-2018 Intel Corporation
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
#if defined (UMC_ENABLE_H264_SPLITTER)

#include "umc_h264_spl.h"

namespace UMC
{

H264Splitter::H264Splitter()
{
    m_bEndOfSourceSeen = false;
    m_bEndOfAUSeen = false;
}

H264Splitter::~H264Splitter()
{
    Close();
}

void H264Splitter::Init()
{
    m_bEndOfSourceSeen = false;
    m_bEndOfAUSeen = false;
    m_dataFile.SetBufferPointer(0, 0);
    m_dataFile.SetDataSize(0);
    m_streamAU.Init();
    m_streamNALU.Init();
}

Status H264Splitter::Init(SplitterParams &initParams)
{
    // check incompatible flag(s)
    if (0 == (initParams.m_lFlags & VIDEO_SPLITTER))
        return UMC_ERR_INVALID_STREAM;

    // check parameter(s)
    if (NULL == initParams.m_pDataReader)
        return UMC_ERR_NULL_PTR;

    Close();
    Init();

    m_pDataReader = initParams.m_pDataReader;

    return UMC_OK;
}

Status H264Splitter::Close()
{
    m_pDataReader = 0;
    return UMC_OK;
}

Status H264Splitter::LoadByteData(MediaData & p_data)
{
    if (p_data.GetDataSize() > sizeof(m_aFileBuffer))
    {
        VM_ASSERT(false);
        throw "abnormal behavior";
    }

    size_t tail_size = p_data.GetDataSize();

    Ipp32u iRead = (Ipp32u)(sizeof(m_aFileBuffer) - tail_size);
    memcpy(m_aFileBuffer, p_data.GetDataPointer(), tail_size);
    Status umsRes = m_pDataReader->ReadData(m_aFileBuffer + tail_size, &iRead);

    p_data.SetBufferPointer(m_aFileBuffer, iRead + tail_size);
    p_data.SetDataSize(iRead + tail_size);

    if (umsRes != UMC_OK && !iRead)
    {
        return UMC_ERR_END_OF_STREAM;
    }

    return UMC_OK;
}

Status H264Splitter::GetNALUData(MediaData & p_data)
{
    if(m_bEndOfSourceSeen)
    {
        return UMC_ERR_END_OF_STREAM;
    }

    m_streamNALU.UnLockOutputData();

    for(;;)
    {
        Status status = m_streamNALU.PutData(m_dataFile);
        if( UMC_OK == status )
        {
            m_streamNALU.LockOutputData(p_data);
            return status;
        } else if( UMC_ERR_NOT_ENOUGH_DATA == status)
        {
            if (!m_bEndOfSourceSeen)
            {
                //if (m_dataFile.GetDataSize() == 0)
                {
                    if (UMC_OK != LoadByteData(m_dataFile))
                    {
                        m_bEndOfSourceSeen = true;
                        m_streamNALU.EndOfStream(m_dataFile);
                        m_streamNALU.LockOutputData(p_data);
                        return p_data.GetDataSize() == 0 ? UMC_ERR_END_OF_STREAM : UMC_OK;
                    }
                }
            }
            continue;
        } else {
            // Error or other event seen
            return status;
        }
    }
}

bool H264Splitter::HasMoreBinData()
{
    return m_dataFile.GetDataSize() > 0 ? true : false;
}

Status H264Splitter::GetNextData(MediaData *pFrame, Ipp32u )
{
    if(m_bEndOfAUSeen){
        pFrame->SetBufferPointer(0, 0);
        pFrame->SetDataSize(0);
        return UMC_ERR_END_OF_STREAM;
    }

    for(;;)
    {
        MediaData dataNALU;
        Status status = GetNALUData(dataNALU);
        if(UMC_OK == status){
            Status statusAU = m_streamAU.PutData(dataNALU);
            if(UMC_OK == statusAU){
                m_streamAU.LockOutputData(*pFrame);
                return UMC_OK;
            }
        } else {
            m_streamAU.Swap();
            m_streamAU.LockOutputData(*pFrame);
            m_bEndOfAUSeen = true;
            return pFrame->GetDataSize() == 0 ? UMC_ERR_END_OF_STREAM : UMC_OK;
        }
    }
}
}

#endif // UMC_ENABLE_H264_SPLITTER
