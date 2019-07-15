/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011-2019 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */


#pragma once

#include "mfx_iyuv_source.h"
#include "mfx_extended_buffer.h"
#include "mfx_iproxy.h"
//this wrapper can only be used for YUV decoder, or plain decoder without MVC
//reports mvc structures back to application according to init parameters
class MVCDecoder
    : public InterfaceProxy<IYUVSource>
{
public:
    MVCDecoder( bool bGenerateViewIds
              , mfxVideoParam &frameParam
              , std::unique_ptr<IYUVSource> &&pTarget);


    virtual mfxStatus DecodeFrameAsync( mfxBitstream2 &bs,
                                        mfxFrameSurface1 *surface_work,
                                        mfxFrameSurface1 **surface_out,
                                        mfxSyncPoint *syncp);
    virtual mfxStatus DecodeHeader(mfxBitstream *bs, mfxVideoParam *par);


protected:

    virtual mfxStatus DecodeHeaderTarget(mfxBitstream *bs, mfxVideoParam *par);

    MFXExtBufferVector                m_extParams;
    MFXExtBufferPtr<mfxExtMVCSeqDesc> m_sequence;
    mfxU32                            m_nCurrentViewId;
    mfxVideoParam                     m_vParam;
    bool                              m_bGenerateViewsId;
};
