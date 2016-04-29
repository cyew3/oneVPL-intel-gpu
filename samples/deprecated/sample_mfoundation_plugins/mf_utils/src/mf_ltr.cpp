/******************************************************************************\
Copyright (c) 2005-2015, Intel Corporation
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

This sample was distributed or derived from the Intel's Media Samples package.
The original version of this sample may be obtained from https://software.intel.com/en-us/intel-media-server-studio
or https://software.intel.com/en-us/media-client-solutions-support.
\**********************************************************************************/

#include "mf_ltr.h"
#include "mf_utils.h"

using namespace std;

static const mfxU16 MFX_FRAMETYPE_IPB     = MFX_FRAMETYPE_I | MFX_FRAMETYPE_P | MFX_FRAMETYPE_B;
static const mfxU16 MFX_FRAMETYPE_IREFIDR = MFX_FRAMETYPE_I | MFX_FRAMETYPE_REF | MFX_FRAMETYPE_IDR;

void FrameTypeGenerator::Init(mfxU16 GopOptFlag, mfxU16 GopPicSize, mfxU16 GopRefDist, mfxU16 IdrInterval)
{
    m_gopOptFlag = GopOptFlag;
    m_gopPicSize = MAX(GopPicSize, 1);
    m_gopRefDist = MAX(GopRefDist, 1);
    m_idrDist    = m_gopPicSize * (IdrInterval + 1);
    m_frameOrder = 0;
    m_init = true;
}

/*------------------------------------------------------------------------------*/

mfxU16 ExtendFrameType(mfxU32 type)
{
    mfxU16 ffType = mfxU16(type & 0xff); // 1st field
    mfxU16 sfType = ffType;              // 2nd field

    if (sfType & MFX_FRAMETYPE_IDR)
    {
        sfType &= ~MFX_FRAMETYPE_IDR;
    }

    if (sfType & MFX_FRAMETYPE_I)
    {
        sfType &= ~MFX_FRAMETYPE_I;
        sfType |= MFX_FRAMETYPE_P;
    }

    return ffType | (sfType << 8);
}

/*------------------------------------------------------------------------------*/

mfxU16 FrameTypeGenerator::CurrentFrameType()const
{
    if (m_init && m_cachedFrameType == MFX_FRAMETYPE_UNKNOWN)
    {
        if (m_frameOrder == 0)
        {

            return m_cachedFrameType = ExtendFrameType(MFX_FRAMETYPE_I | MFX_FRAMETYPE_REF | MFX_FRAMETYPE_IDR);
        }

        if (m_frameOrder % m_gopPicSize == 0)
        {
            return m_cachedFrameType = ExtendFrameType(MFX_FRAMETYPE_I | MFX_FRAMETYPE_REF);
        }

        if (m_frameOrder % m_gopPicSize % m_gopRefDist == 0)
        {
            return m_cachedFrameType = ExtendFrameType(MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF);
        }

        if ((m_gopOptFlag & MFX_GOP_STRICT) == 0)
        {
            if ((m_frameOrder + 1) % m_gopPicSize == 0 && (m_gopOptFlag & MFX_GOP_CLOSED) ||
                (m_frameOrder + 1) % m_idrDist == 0)
            {
                // switch last B frame to P frame
                return m_cachedFrameType = ExtendFrameType(MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF);
            }
        }

        return m_cachedFrameType = ExtendFrameType(MFX_FRAMETYPE_B);
    }
    else if (m_cachedFrameType != MFX_FRAMETYPE_UNKNOWN)
        return m_cachedFrameType;
    else
        return MFX_FRAMETYPE_IDR; // after initialization the first frame will be IDR

}

/*------------------------------------------------------------------------------*/

void FrameTypeGenerator::Next()
{
    if (!m_init)
        return;
    m_frameOrder = (m_frameOrder + 1) % m_idrDist;
    m_cachedFrameType = MFX_FRAMETYPE_UNKNOWN;
}

/*------------------------------------------------------------------------------*/


auto_ptr<MFLongTermReference> MFLongTermReferenceFactory::CreateLongTermReference(MFParamsManager *pManager)
{
    try
    {
        auto_ptr<MFLongTermReference> ltr(new MFLongTermReference (pManager, new FrameTypeGenerator));
        return ltr;
    }
    catch(...)
    {
        return auto_ptr<MFLongTermReference>(NULL);
    }
}

/*------------------------------------------------------------------------------*/

MFLongTermReference::MFLongTermReference( MFParamsManager *pManager, FrameTypeGenerator *gen) :
    MFParamsWorker(pManager),
    m_frameTypeGen(gen),
    m_nBufferControlSize(0),
    m_nBufferControlMode(MF_LTR_BC_MODE_TRUST_UNTIL),
    m_nNumRefFrame(0),
    m_ForcedFrameType(MFX_FRAMETYPE_UNKNOWN),
    m_nFrameOrder(0),
    m_prevFrameOrder((mfxU32)MFX_FRAMEORDER_UNKNOWN),
    m_MarkFrameIdx(MF_INVALID_IDX),
    m_bSaveConstraints(false),
    m_bToCleanSTRList(false),
    m_tempLayerType(mtNotifyTemporalLayerIsBase)
{
    Zero(m_cachedExtAVCRefListCtrl);
}

/*------------------------------------------------------------------------------*/

MFLongTermReference::~MFLongTermReference(void)
{

}

/*------------------------------------------------------------------------------*/
//assume "The first field, Bits[0..15], The second field, Bits[16..31]" is little endian

HRESULT MFLongTermReference::GetBufferControl(ULONG &nValue) const
{
    nValue = GetBufferControlSize() + (GetBufferControlMode() << 16);
    return S_OK;
}

/*------------------------------------------------------------------------------*/

HRESULT MFLongTermReference::SetBufferControl(ULONG nValue)
{
    HRESULT hr = S_OK;
    const UINT defaultValue = 0;

    if (nValue != defaultValue)
    {
        UINT16 nBufferControlSize = nValue & MF_INVALID_IDX;
        if ( nBufferControlSize > GetMaxBufferControlSize())
        {
            hr = E_INVALIDARG;
        }
        UINT16 nBufferControlMode = nValue >> 16;
        if (SUCCEEDED(hr) && nBufferControlMode != MF_LTR_BC_MODE_TRUST_UNTIL)
        {
            hr = E_INVALIDARG;
        }
        if (SUCCEEDED(hr))
        {
            m_nBufferControlSize = nBufferControlSize;
            m_nBufferControlMode = nBufferControlMode;
        }
    }
    return hr;
}

/*------------------------------------------------------------------------------*/

HRESULT MFLongTermReference::MarkFrame(ULONG nLtrIndex)
{
    HRESULT hr = S_OK;

    mfxU32 frameType = m_frameTypeGen->CurrentFrameType();
    if (m_ForcedFrameType != MFX_FRAMETYPE_UNKNOWN)
    {
        if ((m_ForcedFrameType & MFX_FRAMETYPE_IPB) == MFX_FRAMETYPE_I)
        {
            frameType = MFX_FRAMETYPE_IREFIDR;
        }
        m_ForcedFrameType = MFX_FRAMETYPE_UNKNOWN;
    }

    if (nLtrIndex >= m_nBufferControlSize || (MFX_FRAMETYPE_IDR & frameType && 0 != nLtrIndex))
    {
        hr = E_INVALIDARG;
        m_MarkFrameIdx = MF_INVALID_IDX;
    }
    else
        m_MarkFrameIdx = (LongTermFrameIdx)nLtrIndex;

    return hr;
}

/*------------------------------------------------------------------------------*/

HRESULT MFLongTermReference::UseFrame(ULONG nUseLTRFrames)
{
    // validate argument first
    HRESULT hr = S_OK;
    UINT16 nLTRFrames = nUseLTRFrames & MF_INVALID_IDX;
    if (0 == nLTRFrames)
    {
        hr = E_INVALIDARG;
    }
    UINT16 nConstraints = nUseLTRFrames >> 16;
    if (SUCCEEDED(hr) && (MF_LTR_USE_CONSTRAINTS_NO != nConstraints && MF_LTR_USE_CONSTRAINTS_YES != nConstraints))
    {
        hr = E_INVALIDARG;
    }

    bitset<16> bitMap(nLTRFrames);
    if (SUCCEEDED(hr))
    {
        for (mfxU8 i = 0; i < 16; i++)
        {
            if (!bitMap[i]) continue;

            RefList::iterator it = m_LTRFrames.find(i);
            if (it == m_LTRFrames.end())
            {
                hr = E_INVALIDARG;
                break;
            }
        }
    }

    //proceed if argument is valid, don't modify state instead
    if (SUCCEEDED(hr))
    {
        m_preferredIdxs.clear();
        m_bToCleanSTRList = m_bSaveConstraints = nConstraints ? MF_LTR_USE_CONSTRAINTS_YES : MF_LTR_USE_CONSTRAINTS_NO;

        for (mfxU8 i = 0; i < 16; i++)
        {
            if (!bitMap[i]) continue;

            RefList::iterator it = m_LTRFrames.find(i);
            if (it != m_LTRFrames.end())
            {
                try
                {
                    m_preferredIdxs.insert(i);
                }
                catch(...)
                {
                    hr = E_OUTOFMEMORY;
                }
            }
        }
    }
    return hr;
}

/*------------------------------------------------------------------------------*/

HRESULT MFLongTermReference::HandleMessage(MFParamsManagerMessage *pMessage, MFParamsMessagePeer *pSender)
{
    UNREFERENCED_PARAMETER(pSender);
    HRESULT hr = E_NOTIMPL;
    MFParamsManagerMessageType messageType = pMessage->GetType();
    switch (messageType)
    {
        case mtNotifyTemporalLayerIsBase:
        case mtNotifyTemporalLayerIsMax:
        case mtNotifyTemporalLayerIsNeitherBaseNorMax:
            m_tempLayerType = messageType;
            hr = S_OK;
    };
    return hr;
}

/*------------------------------------------------------------------------------*/
/*
HRESULT MFLongTermReference::UpdateVideoParam(mfxVideoParam &videoParams, MFExtBufCollection &arrExtBuf) const
{
    mfxExtAVCEncodedFrameInfo *pAvcEncodedFrameInfo = NULL;
    HRESULT hr = PrepareExtBuf(videoParams, arrExtBuf, pAvcEncodedFrameInfo);  //to check that MSDK can support reporting by mfxExtAVCEncodedFrameInfo

    return hr;
}
*/
/*------------------------------------------------------------------------------*/

void MFLongTermReference::IncrementFrameOrder()
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_LTR);
    m_frameTypeGen->Next();
    m_nFrameOrder++;
    MFX_LTRACE_D(MF_TL_LTR, m_nFrameOrder);
}

/*------------------------------------------------------------------------------*/

void MFLongTermReference::ResetFrameOrder(void)
{
    m_LTRFrames.clear();
    m_MarkFrameIdx = MF_INVALID_IDX;
    m_preferredIdxs.clear();
    m_nFrameOrder = 0;
    m_prevFrameOrder = (mfxU32)MFX_FRAMEORDER_UNKNOWN;
    Zero(m_cachedExtAVCRefListCtrl);
    m_frameTypeGen->ResetFrameOrder();
    m_ForcedFrameType = MFX_FRAMETYPE_UNKNOWN;
    m_bToCleanSTRList = false;
    m_bSaveConstraints = MF_LTR_USE_CONSTRAINTS_NO;
    m_STRFrames.clear();
}

/*------------------------------------------------------------------------------*/

void MFLongTermReference::ForceFrameType(mfxU16 nFrameType)
{
    ATLASSERT(MFX_FRAMETYPE_IREFIDR == nFrameType);

    if ((nFrameType & MFX_FRAMETYPE_IPB) == MFX_FRAMETYPE_I)
    {
        m_ForcedFrameType = nFrameType;
        m_frameTypeGen->ResetFrameOrder();
    }
};

/*------------------------------------------------------------------------------*/

HRESULT MFLongTermReference::GenerateFrameEncodeCtrlExtParam(mfxEncodeCtrl &encodeCtrl)
{
    HRESULT hr = S_OK;

    if (IsEnabled())
    {
        CHECK_EXPRESSION(0 == encodeCtrl.NumExtParam, E_FAIL);
        CHECK_EXPRESSION(NULL == encodeCtrl.ExtParam, E_FAIL);

        SAFE_NEW(encodeCtrl.ExtParam, (mfxExtBuffer*));
        if (NULL != encodeCtrl.ExtParam)
        {
            encodeCtrl.NumExtParam = 1;
            mfxExtAVCRefListCtrl *pExtAVCRefListCtrl = NULL;
            SAFE_NEW(pExtAVCRefListCtrl, mfxExtAVCRefListCtrl);
            *encodeCtrl.ExtParam = (mfxExtBuffer*)pExtAVCRefListCtrl;

            if (NULL != *encodeCtrl.ExtParam)
            {
                if (m_prevFrameOrder == m_nFrameOrder)
                {
                    *pExtAVCRefListCtrl = m_cachedExtAVCRefListCtrl;
                }
                else
                    hr = GenerateExtAVCRefList(*pExtAVCRefListCtrl);
                PrintInfo();
                encodeCtrl.NumExtParam = 1;
                pExtAVCRefListCtrl->Header.BufferId = MFX_EXTBUFF_AVC_REFLIST_CTRL;
                pExtAVCRefListCtrl->Header.BufferSz = sizeof(mfxExtAVCRefListCtrl);
            }
            else
            {
                SAFE_DELETE(encodeCtrl.ExtParam);
                hr = E_OUTOFMEMORY;
            }
        }
        else
            hr = E_OUTOFMEMORY;
    }
    else
        hr = E_UNEXPECTED;

    return hr;
}

/*------------------------------------------------------------------------------*/

HRESULT MFLongTermReference::GenerateExtAVCRefList(mfxExtAVCRefListCtrl& extAvcRefListCtrl)
{
    mfxU16 longTermIdx = MF_INVALID_IDX; //0xffff means output frame is a short term reference frame or a non-reference frame.

    vector<LongTermRefFrame> preferredRefList , rejectedRefList, longTermRefList;

    Zero(extAvcRefListCtrl);
    for (mfxU32 i = 0; i < _countof(extAvcRefListCtrl.PreferredRefList); i++)
    {
        extAvcRefListCtrl.PreferredRefList[i].FrameOrder = (mfxU32)MFX_FRAMEORDER_UNKNOWN;
    }
    for (mfxU32 i = 0; i < _countof(extAvcRefListCtrl.RejectedRefList); i++)
    {
        extAvcRefListCtrl.RejectedRefList[i].FrameOrder = (mfxU32)MFX_FRAMEORDER_UNKNOWN;
    }
    for (mfxU32 i = 0; i < _countof(extAvcRefListCtrl.LongTermRefList); i++)
    {
        extAvcRefListCtrl.LongTermRefList[i].FrameOrder = (mfxU32)MFX_FRAMEORDER_UNKNOWN;
    }

    mfxU32 frameType = m_frameTypeGen->CurrentFrameType();
    if (m_ForcedFrameType != MFX_FRAMETYPE_UNKNOWN)
    {
        if ((m_ForcedFrameType & MFX_FRAMETYPE_IPB) == MFX_FRAMETYPE_I)
        {
            frameType = MFX_FRAMETYPE_IREFIDR;
        }
        m_ForcedFrameType = MFX_FRAMETYPE_UNKNOWN;
    }
    if (mtNotifyTemporalLayerIsMax == m_tempLayerType)
        frameType &= ~MFX_FRAMETYPE_REF;

    //no support of B frames
    if (frameType & MFX_FRAMETYPE_B)
        return E_FAIL;

    if (m_nNumRefFrame <= m_nBufferControlSize)
        return E_FAIL;

    if (frameType & MFX_FRAMETYPE_IDR)
    {
        m_bSaveConstraints = MF_LTR_USE_CONSTRAINTS_NO; //IDR frame will clean DPB so additional limitations don't make sense
        m_bToCleanSTRList = false;
        m_STRFrames.clear();
        m_preferredIdxs.clear();
    }

    ExecuteUseCmd(preferredRefList, rejectedRefList);
    extAvcRefListCtrl.NumRefIdxL0Active = (mfxU16)preferredRefList.size();

    extAvcRefListCtrl.ApplyLongTermIdx = (mfxU16) 1; // 0 - don't use LongTermIdx, 1 - use LongTermIdx
    UpdateLTRList(frameType, longTermRefList, longTermIdx);
    UpdateSTRList(frameType, longTermIdx != MF_INVALID_IDX);

    m_prevFrameOrder = m_nFrameOrder;
    m_cachedExtAVCRefListCtrl = extAvcRefListCtrl;

    if (0 != preferredRefList.size())
        memcpy_s(extAvcRefListCtrl.PreferredRefList, sizeof(extAvcRefListCtrl.PreferredRefList), &preferredRefList.front(), preferredRefList.size()*sizeof(LongTermRefFrame));
    if (0 != rejectedRefList.size())
        memcpy_s(extAvcRefListCtrl.RejectedRefList, sizeof(extAvcRefListCtrl.RejectedRefList), &rejectedRefList.front(), rejectedRefList.size()*sizeof(LongTermRefFrame));
    if (0 != longTermRefList.size())
        memcpy_s(extAvcRefListCtrl.LongTermRefList, sizeof(extAvcRefListCtrl.LongTermRefList), &longTermRefList.front(), longTermRefList.size()*sizeof(LongTermRefFrame));

    return S_OK;
}

/*------------------------------------------------------------------------------*/

void MFLongTermReference::ExecuteUseCmd(vector<LongTermRefFrame> & preferredList, vector<LongTermRefFrame> & removeFromDPB)
{
    if (m_bToCleanSTRList)
    {
        LongTermRefFrame frame;
        for (set<mfxU32>::iterator it = m_STRFrames.begin(); it != m_STRFrames.end(); it++)
        {
            frame.FrameOrder = *it;
            frame.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
            frame.ViewId = 0;
            removeFromDPB.push_back(frame);
        }
        m_STRFrames.clear();
        m_bToCleanSTRList = false;
    }

    LongTermRefFrame frame;
    if(m_preferredIdxs.size())
    {
        RefList::iterator it;
        for (set<mfxU16>::iterator idx = m_preferredIdxs.begin(); idx != m_preferredIdxs.end(); idx++)
        {
            it = m_LTRFrames.find(*idx);
            if (it != m_LTRFrames.end())
            {
                frame.FrameOrder = it->second.FrameOrder;
                frame.PicStruct = it->second.PicStruct;
                frame.ViewId = 0;
                preferredList.push_back(frame);
            }
        }
        if (!m_bSaveConstraints) m_preferredIdxs.clear();
    }
}

/*------------------------------------------------------------------------------*/

void MFLongTermReference::UpdateLTRList(mfxU32 frameType, vector<LongTermRefFrame> & addToDPB, mfxU16 & idx)
{
    if (frameType & MFX_FRAMETYPE_IDR)
        m_LTRFrames.clear();

    if ((frameType & MFX_FRAMETYPE_REF) == 0 || (m_tempLayerType != mtNotifyTemporalLayerIsBase))
        return;

    LongTermRefFrame frame;
    if (frameType & MFX_FRAMETYPE_IDR)
    {
        if (m_MarkFrameIdx != MF_INVALID_IDX)
            m_MarkFrameIdx = MF_INVALID_IDX;

        idx = 0;
        m_LTRFrames[idx] = RefFrame(m_nFrameOrder, MFX_PICSTRUCT_PROGRESSIVE);

        frame.FrameOrder = m_nFrameOrder;
        frame.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
        frame.ViewId = 0;
        frame.LongTermIdx = idx;
        addToDPB.push_back(frame);
    }
    else if (frameType & MFX_FRAMETYPE_REF)
    {
        if (m_LTRFrames.size() < m_nBufferControlSize) //auto mode: we mark n-frames from the base layer after each IDR as LTR , where n = m_nBufferControlSize
        {
            // The Mark command is optional here. If it's passed we can consider that it wants to set specific idx for the current frame, even the frame is going to be marked without the command
            if (m_MarkFrameIdx != MF_INVALID_IDX)
            {
                idx = m_MarkFrameIdx;
                m_MarkFrameIdx = MF_INVALID_IDX;
            }
            else
            {
                idx = FindMinFreeIdx(m_LTRFrames);
            }

            m_LTRFrames[idx] = RefFrame(m_nFrameOrder, MFX_PICSTRUCT_PROGRESSIVE);

            frame.FrameOrder = m_nFrameOrder;
            frame.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
            frame.ViewId = 0;
            frame.LongTermIdx = idx;
            addToDPB.push_back(frame);
        }
        else
        {
            if (m_MarkFrameIdx != MF_INVALID_IDX)
            {
                idx = m_MarkFrameIdx;
                m_MarkFrameIdx = MF_INVALID_IDX;

                m_LTRFrames[idx] = RefFrame(m_nFrameOrder, MFX_PICSTRUCT_PROGRESSIVE);

                frame.FrameOrder = m_nFrameOrder;
                frame.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
                frame.ViewId = 0;
                frame.LongTermIdx = idx;
                addToDPB.push_back(frame);
            }
        }
    }
}

/*------------------------------------------------------------------------------*/

void MFLongTermReference::UpdateSTRList(mfxU32 frameType, bool isLTR)
{
    if ((frameType & MFX_FRAMETYPE_REF) && !isLTR)
    {
        mfxU16 maxNumShortTerm = m_nNumRefFrame - m_nBufferControlSize;

        if (m_STRFrames.size() == maxNumShortTerm)
        {
            m_STRFrames.erase(min_element(m_STRFrames.begin(), m_STRFrames.end()));
        }
        m_STRFrames.insert(m_nFrameOrder);
    }
}

/*------------------------------------------------------------------------------*/

mfxU16 MFLongTermReference::FindMinFreeIdx(RefList const & refPicList) const
{
    mfxU16 minIdx = 0;
    for(mfxU8 i = 0; i < 16; i++ )
    {
        if (refPicList.find(i) == refPicList.end())
        {
            minIdx = i;
            break;
        }
    }
    return minIdx;
}

/*------------------------------------------------------------------------------*/

void MFLongTermReference::PrintInfo() const
{
    MFX_AUTO_LTRACE_FUNC(MF_TL_LTR);

    MFX_LTRACE_I(MF_TL_LTR, m_tempLayerType);
    for (RefList::const_iterator it = m_LTRFrames.begin(); it != m_LTRFrames.end(); ++it)
    {
        MFX_LTRACE_2(MF_TL_LTR, "LTR",
            "[%02d]: %2d", it->first, it->second.FrameOrder);
    }

    for (set<mfxU32>::const_iterator it = m_STRFrames.begin(); it != m_STRFrames.end(); ++it)
    {
        MFX_LTRACE_1(MF_TL_LTR, "STR:",
            "%7d", *it);
    }
}
