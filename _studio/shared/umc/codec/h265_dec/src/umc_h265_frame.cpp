// Copyright (c) 2012-2019 Intel Corporation
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
#ifdef MFX_ENABLE_H265_VIDEO_DECODE

#include <algorithm>
#include "umc_h265_frame.h"
#include "umc_h265_task_supplier.h"
#include "umc_h265_debug.h"

#ifndef MFX_VA
#include "umc_h265_ipplevel.h"
#endif


namespace UMC_HEVC_DECODER
{

H265DecoderFrame::H265DecoderFrame(UMC::MemoryAllocator *pMemoryAllocator, Heap_Objects * pObjHeap)
    : H265DecYUVBufferPadded(pMemoryAllocator)
    , m_ErrorType(0)
    , m_pPreviousFrame(nullptr)
    , m_pFutureFrame(nullptr)
    , m_dFrameTime(-1.0)
    , m_isOriginalPTS(false)
    , post_procces_complete(false)
    , m_index(-1)
    , m_UID(-1)
    , m_pSlicesInfo(nullptr)
    , m_pObjHeap(pObjHeap)
{
    m_isShortTermRef = false;
    m_isLongTermRef = false;
    m_RefPicListResetCount = 0;
    m_PicOrderCnt = 0;

#ifndef MFX_VA
    m_cuOffsetY = 0;
    m_cuOffsetC = 0;
    m_buOffsetY = 0;
    m_buOffsetC = 0;
    m_CodingData = NULL;
#endif
    // set memory managment tools
    m_pMemoryAllocator = pMemoryAllocator;

    ResetRefCounter();

    m_pSlicesInfo = new H265DecoderFrameInfo(this);

    m_Flags.isFull = 0;
    m_Flags.isDecoded = 0;
    m_Flags.isDecodingStarted = 0;
    m_Flags.isDecodingCompleted = 0;
    m_isDisplayable = 0;
    m_wasOutputted = 0;
    m_wasDisplayed = 0;
    m_maxUIDWhenWasDisplayed = 0;
    m_crop_flag = 0;
    m_crop_left = 0;
    m_crop_top = 0;
    m_crop_bottom = 0;
    m_pic_output = 0;
    m_FrameType = UMC::NONE_PICTURE;
    m_aspect_width = 0;
    m_MemID = 0;
    m_aspect_height = 0;
    m_isUsedAsReference = 0;
    m_crop_right = 0;
    m_DisplayPictureStruct_H265 = DPS_FRAME_H265;
    m_frameOrder = 0;
    prepared = false;
}

H265DecoderFrame::~H265DecoderFrame()
{
    if (m_pSlicesInfo)
    {
        delete m_pSlicesInfo;
        m_pSlicesInfo = 0;
    }

    // Just to be safe.
    m_pPreviousFrame = 0;
    m_pFutureFrame = 0;
    Reset();
    deallocate();
#ifndef MFX_VA
    deallocateCodingData();
#endif
}

// Add target frame to the list of reference frames
void H265DecoderFrame::AddReferenceFrame(H265DecoderFrame * frm)
{
    if (!frm || frm == this)
        return;

    if (std::find(m_references.begin(), m_references.end(), frm) != m_references.end())
        return;

    frm->IncrementReference();
    m_references.push_back(frm);
}

// Clear all references to other frames
void H265DecoderFrame::FreeReferenceFrames()
{
    ReferenceList::iterator iter = m_references.begin();
    ReferenceList::iterator end_iter = m_references.end();

    for (; iter != end_iter; ++iter)
    {
        RefCounter *reference = *iter;
        reference->DecrementReference();
    }

    m_references.clear();
}

// Reinitialize frame structure before reusing frame
void H265DecoderFrame::Reset()
{
    if (m_pSlicesInfo)
        m_pSlicesInfo->Reset();

    ResetRefCounter();

    m_isShortTermRef = false;
    m_isLongTermRef = false;

    post_procces_complete = false;

    m_RefPicListResetCount = 0;
    m_PicOrderCnt = 0;

    m_Flags.isFull = 0;
    m_Flags.isDecoded = 0;
    m_Flags.isDecodingStarted = 0;
    m_Flags.isDecodingCompleted = 0;
    m_isDisplayable = 0;
    m_wasOutputted = 0;
    m_wasDisplayed = 0;
    m_pic_output = true;
    m_maxUIDWhenWasDisplayed = 0;

    m_dFrameTime = -1;
    m_isOriginalPTS = false;

    m_isUsedAsReference = false;

    m_UserData.Reset();

    m_ErrorType = 0;
    m_UID = -1;
    m_index = -1;

    m_MemID = MID_INVALID;

    prepared = false;

    FreeReferenceFrames();

    deallocate();

    m_refPicList.clear();
}

// Returns whether frame has all slices found
bool H265DecoderFrame::IsFullFrame() const
{
    return (m_Flags.isFull == 1);
}

// Set frame flag denoting that all slices for it were found
void H265DecoderFrame::SetFullFrame(bool isFull)
{
    m_Flags.isFull = (uint8_t) (isFull ? 1 : 0);
}

// Returns whether frame has been decoded
bool H265DecoderFrame::IsDecoded() const
{
    return m_Flags.isDecoded == 1;
}

// Clean up data after decoding is done
void H265DecoderFrame::FreeResources()
{
    FreeReferenceFrames();

    if (m_pSlicesInfo && IsDecoded())
        m_pSlicesInfo->Free();
}

// Flag frame as completely decoded
void H265DecoderFrame::CompleteDecoding()
{
    m_Flags.isDecodingCompleted = 1;
    UpdateErrorWithRefFrameStatus();
}

// Check reference frames for error status and flag this frame if error is found
void H265DecoderFrame::UpdateErrorWithRefFrameStatus()
{
    if (CheckReferenceFrameError())
    {
        SetErrorFlagged(UMC::ERROR_FRAME_REFERENCE_FRAME);
    }
}

// Delete unneeded references and set flags after decoding is done
void H265DecoderFrame::OnDecodingCompleted()
{
    UpdateErrorWithRefFrameStatus();

    m_Flags.isDecoded = 1;
    DEBUG_PRINT1((VM_STRING("On decoding complete decrement for POC %d, reference = %d\n"), m_PicOrderCnt, m_refCounter));
    DecrementReference();

    FreeResources();
}

// Mark frame as short term reference frame
void H265DecoderFrame::SetisShortTermRef(bool isRef)
{
    if (isRef)
    {
        if (!isShortTermRef() && !isLongTermRef())
            IncrementReference();

        m_isShortTermRef = true;
    }
    else
    {
        bool wasRef = isShortTermRef() != 0;

        m_isShortTermRef = false;

        if (wasRef && !isShortTermRef() && !isLongTermRef())
        {
            DecrementReference();
            DEBUG_PRINT1((VM_STRING("On was short term ref decrement for POC %d, reference = %d\n"), m_PicOrderCnt, m_refCounter));
        }
    }
}

// Mark frame as long term reference frame
void H265DecoderFrame::SetisLongTermRef(bool isRef)
{
    if (isRef)
    {
        if (!isShortTermRef() && !isLongTermRef())
            IncrementReference();

        m_isLongTermRef = true;
    }
    else
    {
        bool wasRef = isLongTermRef() != 0;

        m_isLongTermRef = false;

        if (wasRef && !isShortTermRef() && !isLongTermRef())
        {
            DEBUG_PRINT1((VM_STRING("On was long term reft decrement for POC %d, reference = %d\n"), m_PicOrderCnt, m_refCounter));
            DecrementReference();
        }
    }
}

// Flag frame after it was output
void H265DecoderFrame::setWasOutputted()
{
    m_wasOutputted = 1;
}

// Free resources if possible
void H265DecoderFrame::Free()
{
    if (wasDisplayed() && wasOutputted())
        Reset();
}

void H265DecoderFrame::AddSlice(H265Slice * pSlice)
{
    int32_t iSliceNumber = m_pSlicesInfo->GetSliceCount() + 1;

    pSlice->SetSliceNumber(iSliceNumber);
    pSlice->m_pCurrentFrame = this;
    m_pSlicesInfo->AddSlice(pSlice);

    m_refPicList.resize(pSlice->GetSliceNum() + 1);
}

bool H265DecoderFrame::CheckReferenceFrameError()
{
    uint32_t checkedErrorMask = UMC::ERROR_FRAME_MINOR | UMC::ERROR_FRAME_MAJOR | UMC::ERROR_FRAME_REFERENCE_FRAME;
    for (size_t i = 0; i < m_refPicList.size(); i ++)
    {
        H265DecoderRefPicList* list = &m_refPicList[i].m_refPicList[REF_PIC_LIST_0];
        for (size_t k = 0; list->m_refPicList[k].refFrame; k++)
        {
            if (list->m_refPicList[k].refFrame->GetError() & checkedErrorMask)
                return true;
        }

        list = &m_refPicList[i].m_refPicList[REF_PIC_LIST_1];
        for (size_t k = 0; list->m_refPicList[k].refFrame; k++)
        {
            if (list->m_refPicList[k].refFrame->GetError() & checkedErrorMask)
                return true;
        }
    }

    return false;
}

#ifndef MFX_VA
// Fill frame planes with default values
void H265DecoderFrame::DefaultFill(bool isChromaOnly, uint8_t defaultValue)
{
    try
    {
        mfxSize roi;

        if (!isChromaOnly)
        {
            roi = m_lumaSize;

            if (m_pYPlane)
                SetPlane(defaultValue, m_pYPlane, pitch_luma(), roi);
        }

        roi = m_chromaSize;

        if (m_pUVPlane) // NV12
        {
            roi.width *= 2;
            SetPlane(defaultValue, m_pUVPlane, pitch_chroma(), roi);
        }
        else
        {
            if (m_pUPlane)
                SetPlane(defaultValue, m_pUPlane, pitch_chroma(), roi);
            if (m_pVPlane)
                SetPlane(defaultValue, m_pVPlane, pitch_chroma(), roi);
        }
    } catch(...)
    {
        // nothing to do
        //VM_ASSERT(false);
    }
}

// Allocate and initialize frame array of CTBs and SAO parameters
void H265DecoderFrame::allocateCodingData(const H265SeqParamSet* sps, const H265PicParamSet *pps)
{
    if (!m_CodingData)
    {
        m_CodingData = new H265FrameCodingData();
    }

    uint32_t MaxCUSize = sps->MaxCUSize;

    uint32_t MaxCUDepth   = sps->MaxCUDepth;

    uint32_t widthInCU = (m_lumaSize.width % MaxCUSize) ? m_lumaSize.width / MaxCUSize + 1 : m_lumaSize.width / MaxCUSize;
    uint32_t heightInCU = (m_lumaSize.height % MaxCUSize) ? m_lumaSize.height / MaxCUSize + 1 : m_lumaSize.height / MaxCUSize;

    m_CodingData->m_partitionInfo.Init(sps);

    if (m_CodingData->m_MaxCUWidth != MaxCUSize ||
        m_CodingData->m_WidthInCU != widthInCU  || m_CodingData->m_HeightInCU != heightInCU || m_CodingData->m_MaxCUDepth != MaxCUDepth)
    {
        m_CodingData->destroy();
        m_CodingData->create(m_lumaSize.width, m_lumaSize.height, sps);

        delete[] m_cuOffsetY;

        uint32_t pixelSize = (sps->need16bitOutput) ? 2 : 1;
        int32_t NumCUInWidth = m_CodingData->m_WidthInCU;
        int32_t NumCUInHeight = m_CodingData->m_HeightInCU;

        uint32_t buOffsetSize = (1 << (2 * MaxCUDepth));
        int32_t accumulateSum = 2*NumCUInWidth * NumCUInHeight + 2*buOffsetSize;

        // Initialize CU offset tables
        m_cuOffsetY = h265_new_array_throw<int32_t>(accumulateSum);
        m_cuOffsetC = m_cuOffsetY + NumCUInWidth * NumCUInHeight;

        for (int32_t cuRow = 0; cuRow < NumCUInHeight; cuRow++)
        {
            for (int32_t cuCol = 0; cuCol < NumCUInWidth; cuCol++)
            {
                m_cuOffsetY[cuRow * NumCUInWidth + cuCol] = m_pitch_luma * cuRow * MaxCUSize + cuCol * MaxCUSize;
                m_cuOffsetC[cuRow * NumCUInWidth + cuCol] = m_pitch_chroma * cuRow * (MaxCUSize / sps->SubHeightC()) + cuCol * MaxCUSize;

                m_cuOffsetY[cuRow * NumCUInWidth + cuCol] *= pixelSize;
                m_cuOffsetC[cuRow * NumCUInWidth + cuCol] *= pixelSize;
            }
        }

        // Initialize partition offsets tables
        m_buOffsetY = m_cuOffsetC + NumCUInWidth * NumCUInHeight;
        m_buOffsetC = m_buOffsetY + buOffsetSize;
        for (int32_t buRow = 0; buRow < (1 << MaxCUDepth); buRow++)
        {
            for (int32_t buCol = 0; buCol < (1 << MaxCUDepth); buCol++)
            {
                int32_t buRowOffset = buRow * (MaxCUSize >> MaxCUDepth);
                m_buOffsetY[(buRow << MaxCUDepth) + buCol] = m_pitch_luma * buRowOffset + buCol * (MaxCUSize  >> MaxCUDepth);
                m_buOffsetC[(buRow << MaxCUDepth) + buCol] = m_pitch_chroma * buRowOffset / sps->SubHeightC() + buCol * (MaxCUSize >> MaxCUDepth);

                m_buOffsetY[(buRow << MaxCUDepth) + buCol] *= pixelSize;
                m_buOffsetC[(buRow << MaxCUDepth) + buCol] *= pixelSize;
            }
        }
    }

    m_CodingData->m_CUOrderMap = const_cast<uint32_t*>(&pps->m_CtbAddrTStoRS[0]);
    m_CodingData->m_InverseCUOrderMap = const_cast<uint32_t*>(&pps->m_CtbAddrRStoTS[0]);
    m_CodingData->m_TileIdxMap = const_cast<uint32_t*>(&pps->m_TileIdx[0]);

    m_CodingData->initSAO(sps);
}

// Free array of CTBs
void H265DecoderFrame::deallocateCodingData()
{
    delete m_CodingData;
    m_CodingData = 0;

    delete [] m_cuOffsetY;
    m_cuOffsetY = 0;
}

// Returns a CTB by its raster address
H265CodingUnit* H265DecoderFrame::getCU(uint32_t CUaddr) const
{
    return m_CodingData->getCU(CUaddr);
}

// Returns number of CTBs in frame
uint32_t H265DecoderFrame::getNumCUsInFrame() const
{
    return m_CodingData->m_NumCUsInFrame;
}

// Returns number of minimal partitions in CTB width or height
uint32_t H265DecoderFrame::getNumPartInCUSize() const
{
    return m_CodingData->m_NumPartInWidth;
}

// Returns number of CTBs in frame width
uint32_t H265DecoderFrame::getFrameWidthInCU() const
{
    return m_CodingData->m_WidthInCU;
}

// Returns number of CTBs in frame height
uint32_t H265DecoderFrame::getFrameHeightInCU() const
{
    return m_CodingData->m_HeightInCU;
}

//  Access starting position of original picture for specific coding unit (CU)
PlanePtrY H265DecoderFrame::GetLumaAddr(int32_t CUAddr) const
{
    return m_pYPlane + m_cuOffsetY[CUAddr];
}

//  Access starting position of original picture for specific coding unit (CU)
PlanePtrUV H265DecoderFrame::GetCbCrAddr(int32_t CUAddr) const
{
    // Chroma offset is already multiplied to chroma pitch (double for NV12)
    return m_pUVPlane + m_cuOffsetC[CUAddr];
}

//  Access starting position of original picture for specific coding unit (CU) and partition unit (PU)
// ML: OPT: TODO: Make these functions available for inlining
PlanePtrY H265DecoderFrame::GetLumaAddr(int32_t CUAddr, uint32_t AbsZorderIdx) const
{
    return m_pYPlane + m_cuOffsetY[CUAddr] + m_buOffsetY[getCD()->m_partitionInfo.m_zscanToRaster[AbsZorderIdx]];
}

//  Access starting position of original picture for specific coding unit (CU) and partition unit (PU)
PlanePtrUV H265DecoderFrame::GetCbCrAddr(int32_t CUAddr, uint32_t AbsZorderIdx) const
{
    // Chroma offset is already multiplied to chroma pitch (double for NV12)
    return m_pUVPlane + m_cuOffsetC[CUAddr] + m_buOffsetC[getCD()->m_partitionInfo.m_zscanToRaster[AbsZorderIdx]];
}
#endif

} // end namespace UMC_HEVC_DECODER
#endif // MFX_ENABLE_H265_VIDEO_DECODE
