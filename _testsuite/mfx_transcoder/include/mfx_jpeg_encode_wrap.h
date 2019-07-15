/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2012-2019 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#pragma once

//jpeg encoder has a lot of specific features
// for example it doesnot support several extended buffers like extcoding options
// that automatically attaches in case of interlacing

#include "mfx_ivideo_encode.h"
#include "mfx_extended_buffer.h"
#include <limits>

class MFXJpegEncWrap
    : public InterfaceProxy<IVideoEncode>

{
    typedef InterfaceProxy<IVideoEncode> base;
public:
    MFXJpegEncWrap(std::unique_ptr<IVideoEncode> &&pEncode)
        : base(std::move(pEncode))
    {
    }

    mfxStatus Query(mfxVideoParam *in, mfxVideoParam *out)
    {
        if (NULL == in || NULL == in->ExtParam)
            return base::Query(in, out);

        push_ext_co(in);
        push_ext_co(out);

        mfxStatus sts = base::Query(in, out);

        pop_ext_co(out);
        pop_ext_co(in);

        return sts;
    }

    mfxStatus Init(mfxVideoParam *par)
    {
        if (NULL == par)
            return base::Init(par);

        push_ext_co(par);
        mfxStatus sts = base::Init(par);
        pop_ext_co(par);

        return sts;
    }

    mfxStatus GetVideoParam(mfxVideoParam *par)
    {
        if (NULL == par)
            return base::GetVideoParam(par);

        push_ext_co(par);
        mfxStatus sts = base::GetVideoParam(par);
        pop_ext_co(par);

        //adjusting buffersize since jpeg encoder wont provide it see mediasdk man for formula description
        if (NULL != par && MFX_ERR_NONE <= sts)
        {
            mfxU32 nPlaneSize_p_1023 = 1023 + (std::min)(
                GetMinPlaneSize(par->mfx.FrameInfo), (std::numeric_limits<mfxU32>::max)() - 1023);
            par->mfx.BufferSizeInKB = (mfxU16)(std::min)(
                (mfxU32)(std::numeric_limits<mfxU16>::max)(), 4 + nPlaneSize_p_1023 / 1024);
        }

        return sts;
    }

protected:
    //record this for restoring original buffers in videoparams
    struct ExtBufferRestore
    {
        mfxExtBuffer **pBufOrig;
        mfxU16         numOrig;
        std::vector <mfxExtBuffer*> bufReplaced;
    };
    std::vector<ExtBufferRestore> m_savedBuffers;

    //removing encoding option if only picstruct specified, since it gets attached automatically
    void push_ext_co(mfxVideoParam *in)
    {
        if (NULL == in)
            return;
        ExtBufferRestore restoreRec;
        restoreRec.pBufOrig = in->ExtParam;
        restoreRec.numOrig  = in->NumExtParam;

        for (size_t i =0; i < in->NumExtParam; i++)
        {
            bool bCopy = true;
            if (MFX_EXTBUFF_CODING_OPTION == in->ExtParam[i]->BufferId)
            {
                //check this buffer for picstruct only
                mfxExtCodingOption * pOption = reinterpret_cast<mfxExtCodingOption*>(in->ExtParam[i]);
                mfxExtCodingOption zero;
                mfxU16 FramePictureFlag = pOption->FramePicture;

                mfx_init_ext_buffer(zero);
                pOption->FramePicture = MFX_CODINGOPTION_UNKNOWN;
                
                //differences only in framepicture, do not pass it to encoder
                if (0 == memcmp(pOption, &zero, sizeof(mfxExtCodingOption)))
                {
                    bCopy = false;
                }
                pOption->FramePicture = FramePictureFlag;
            }
            if (bCopy)
            {
                restoreRec.bufReplaced.push_back(in->ExtParam[i]);
            }
        }
        m_savedBuffers.push_back(restoreRec);

        //moving pointers to new buffer
        std::vector <mfxExtBuffer*> &refReplaced = m_savedBuffers.back().bufReplaced;
        in->NumExtParam = (mfxU16)refReplaced.size();
        in->ExtParam = refReplaced.empty() ? NULL :&refReplaced.front();
        
    }

    void pop_ext_co(mfxVideoParam *in)
    {
        if (m_savedBuffers.empty() || NULL == in)
            return;
        in->ExtParam = m_savedBuffers.back().pBufOrig;
        in->NumExtParam = m_savedBuffers.back().numOrig;
        m_savedBuffers.pop_back();
    }

};
