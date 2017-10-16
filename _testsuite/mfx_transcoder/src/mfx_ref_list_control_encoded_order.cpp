/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#include "mfx_pipeline_defs.h"
#include "mfx_extended_buffer.h"
#include "mfx_serializer.h"
#include "mfx_ref_list_control_encoded_order.h"

RefListControlEncodedOrder::RefListControlEncodedOrder (std::auto_ptr<IVideoEncode>& pTarget, const vm_char* par_file)
    : InterfaceProxy<IVideoEncode>(pTarget)
    , m_file_name(par_file)
{
}

RefListControlEncodedOrder::~RefListControlEncodedOrder()
{
    if(m_file.is_open())
        m_file.close();
}

mfxStatus RefListControlEncodedOrder::Init(mfxVideoParam * pInit)
{
    mfxStatus sts = MFX_ERR_NONE;
    mfxVideoParam getVParams = {};
    MFX_CHECK_POINTER(pInit);
    MFX_CHECK_STS(InterfaceProxy<IVideoEncode>::Init(pInit));

    getVParams.NumExtParam = pInit->NumExtParam;
    getVParams.ExtParam = pInit->ExtParam;
    MFX_CHECK_STS(GetVideoParam(&getVParams));

    m_file.open(m_file_name, std::ifstream::in);

    if (!m_file.is_open())
        sts = MFX_ERR_NULL_PTR;

    return sts;
}

mfxStatus RefListControlEncodedOrder::EncodeFrameAsync(mfxEncodeCtrl *ctrl, mfxFrameSurface1 *surface, mfxBitstream *bs, mfxSyncPoint *syncp)
{
    mfxFrameSurface1* pSurfToEncode = 0;
    mfxU16 frameTypeToEncode = 0;
    mfxStatus sts_reorder = MFX_ERR_NONE;
    mfxStatus sts_enc = MFX_ERR_NONE;

    if (nullptr == ctrl)
    {
        if (nullptr == m_controls[surface])
        {
            m_controls[surface] = new mfxEncodeCtrl;
            memset(m_controls[surface], 0, sizeof(mfxEncodeCtrl));
        }
        ctrl = m_controls[surface];
    }

    mfxExtAVCRefLists * refLists = NULL;
    if (ctrl->ExtParam)
    {
        for (mfxU32 i = 0; i < ctrl->NumExtParam; ++i)
        {
            if (ctrl->ExtParam[i] && ctrl->ExtParam[i]->BufferId == MFX_EXTBUFF_AVC_REFLISTS)
            {
                refLists = (mfxExtAVCRefLists *)ctrl->ExtParam[i];
                break;
            }
        }
    }

    // attach own mfxExtAVCRefLists
    if (nullptr == refLists)
    {
        if (nullptr == m_refLists[surface])
        {
            m_refLists[surface] = new mfxExtAVCRefLists;
            mfx_init_ext_buffer(*m_refLists[surface]);
        }
        refLists = m_refLists[surface];

        // recreate extbuffers array
        ExtBuffersVector * ppExtParams;
        if (NULL == (ppExtParams = m_extBuffers[surface]))
        {
           ppExtParams = new ExtBuffersVector;
            m_extBuffers[surface] = ppExtParams;
        }

        ppExtParams->reserve(ctrl->NumExtParam + 1);
        for (mfxU32 i = 0; i < ctrl->NumExtParam; i++)
        {
            // copy old buffers
            ppExtParams->push_back(ctrl->ExtParam[i]);
        }
        // copy own buffer
        ppExtParams->push_back((mfxExtBuffer*)refLists);

        ctrl->ExtParam = ppExtParams->data();
        ctrl->NumExtParam = (mfxU16)ppExtParams->size();
    }

    MFX_CHECK_STS_SKIP(sts_reorder = ReorderFrame(surface, &pSurfToEncode, &frameTypeToEncode, refLists), MFX_ERR_MORE_DATA);

    if (pSurfToEncode)
    {
        PipelineTrace ((VM_STRING("\nFrameOrder %d:\n"), pSurfToEncode->Data.FrameOrder));
        PipelineTrace ((VM_STRING("%s\n"), Serialize(*refLists).c_str()));
    }

    if (sts_reorder == MFX_ERR_NONE)
    {
        ctrl->FrameType = frameTypeToEncode;

        MFX_CHECK_STS_SKIP(sts_enc = InterfaceProxy<IVideoEncode>::EncodeFrameAsync(ctrl, pSurfToEncode, bs, syncp), MFX_ERR_MORE_DATA);
    }
    else if (surface == 0)
    {
        MFX_CHECK_STS_SKIP(sts_enc = InterfaceProxy<IVideoEncode>::EncodeFrameAsync(0, 0, bs, syncp), MFX_ERR_MORE_DATA);
    }

    //pull out frames from reorderer as well
    if (NULL == surface && MFX_ERR_MORE_DATA == sts_enc && MFX_ERR_NONE == sts_reorder)
    {
        sts_enc = MFX_ERR_NONE;
    }

    return sts_enc;
}

mfxStatus RefListControlEncodedOrder::Close()
{
    mfxStatus sts = InterfaceProxy<IVideoEncode>::Close();

    for(auto e : m_controls) { delete e.second; }
    m_controls.clear();

    for(auto e : m_refLists) { delete e.second; }
    m_refLists.clear();

    for(auto e : m_extBuffers) { delete e.second; }
    m_extBuffers.clear();

    return sts;
}

mfxStatus RefListControlEncodedOrder::ReorderFrame(mfxFrameSurface1 * in, mfxFrameSurface1 ** out, mfxU16 * frameType, mfxExtAVCRefLists * pRefLists)
{
    mfxStatus sts = MFX_ERR_NONE;

    if (m_nextFrame.order < 0)
    {
        const char separator_char = '|';
        char separator;

        std::string str;
        getline(m_file, str);
        std::stringstream ss(str);

        ss >> m_nextFrame.order >> std::hex >> m_nextFrame.type >> m_nextFrame.picStruct >> std::dec;
        if (ss.fail())
        {
            if (!m_file.eof())
            {
                PipelineTrace((VM_STRING("\nERROR: Invalid par file\n")));
                return MFX_ERR_UNDEFINED_BEHAVIOR;
            }
            else if (NULL == in) // not an error
                return MFX_ERR_MORE_DATA;
            else
            {
                PipelineTrace((VM_STRING("\nERROR: Unexpected end of par file\n")));
                return MFX_ERR_UNDEFINED_BEHAVIOR;
            }
        }

        m_nextFrame.refLists.NumRefIdxL0Active = 0;
        m_nextFrame.refLists.NumRefIdxL1Active = 0;

        for (mfxU32 direction = 0; direction < 2; ++direction)
        {
            ss >> separator;
            if (ss.fail() || separator != separator_char)
                return MFX_ERR_UNDEFINED_BEHAVIOR;

            mfxU16 size = 0;
            for (mfxI32 poc, i = 0; i < 8 && !(ss >> poc).fail(); i++)
            {
                mfxExtAVCRefLists::mfxRefPic & ref = direction ? m_nextFrame.refLists.RefPicList1[size] : m_nextFrame.refLists.RefPicList0[size];
                if (poc >= 0)
                {
                    ref.FrameOrder = poc;
                    size++;
                }
            }

            if (direction == 0)
                m_nextFrame.refLists.NumRefIdxL0Active = size;
            else
                m_nextFrame.refLists.NumRefIdxL1Active = size;
        }
    }

    if (in != nullptr)
    {
        m_cachedFrames[in->Data.FrameOrder] = in;
        IncreaseReference(&in->Data);
    }

    if (m_cachedFrames.find(m_nextFrame.order) != m_cachedFrames.end())
    {
        *out = m_cachedFrames[m_nextFrame.order];
        (*out)->Info.PicStruct = m_nextFrame.picStruct;
        *frameType = m_nextFrame.type;
        *pRefLists = m_nextFrame.refLists;

        m_cachedFrames.erase(m_nextFrame.order);
        DecreaseReference(&(*out)->Data);

        m_nextFrame.order = -1;
    }
    else
        sts = MFX_ERR_MORE_DATA;

    return sts;
}
