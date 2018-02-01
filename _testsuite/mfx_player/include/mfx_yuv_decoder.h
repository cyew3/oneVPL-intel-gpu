/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2008-2018 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#ifndef __MFX_YUV_DECODER_H
#define __MFX_YUV_DECODER_H

#include "mfx_decoder.h"
#include "mfx_extended_buffer.h"
#include "mfx_ibitstream_converter.h"
#include "mfx_ifactory.h"

class OutlineReader;

class MFXYUVDecoder : public IYUVSource
{
public:
    MFXYUVDecoder(IVideoSession* session,
                  mfxVideoParam &frameParam,
                  mfxF64 dFramerate,
                  mfxU32 nInFourCC,
                  IMFXPipelineFactory * pFactory,
                  const vm_char * outlineInput);
    virtual ~MFXYUVDecoder();

    virtual mfxStatus Init(mfxVideoParam *par);
    virtual mfxStatus Close();

    virtual mfxStatus Query(mfxVideoParam *in, mfxVideoParam *out);
    virtual mfxStatus QueryIOSurf(mfxVideoParam *par, mfxFrameAllocRequest *request);
    virtual mfxStatus GetDecodeStat(mfxDecodeStat *stat);

    virtual mfxStatus DecodeFrameAsync(
        mfxBitstream2 &bs,
        mfxFrameSurface1 *surface_work,
        mfxFrameSurface1 **surface_out,
        mfxSyncPoint *syncp);
    virtual mfxStatus DecodeHeader(mfxBitstream *bs, mfxVideoParam *par);
    virtual mfxStatus SyncOperation(mfxSyncPoint /*syncp*/, mfxU32 /*wait*/) { return MFX_ERR_NONE; }
    virtual mfxStatus Reset(mfxVideoParam* /*par*/) ;
    virtual mfxStatus GetVideoParam(mfxVideoParam* par);
    virtual mfxStatus SetSkipMode(mfxSkipMode /*mode*/) { return MFX_ERR_NONE; }
    mfxStatus GetInfoFromOutline(mfxVideoParam* par);

protected:
    //decoder is very spefic when it works in this mode
    bool IsMVCProfile();
    //organizes yuv bitstream into yuv frames according to color format
    mfxStatus ConstructFrame(mfxBitstream * in, mfxBitstream * out);
    mfxStatus DecodeFrame(mfxBitstream * bs, mfxFrameSurface1 *surface);
//    mfxStatus DecodeFrameInternal(mfxBitstream * bs, mfxFrameSurface1 *surface);
    
    //analog of fread function
    mfxU32    ReadBs(mfxU8 * pData, int nBlockSize, mfxU32 nSize, mfxBitstream * pBs);

    IVideoSession* m_session;
    mfxVideoParam           m_vParam;
    mfxU16                  m_yuvWidth;
    mfxU16                  m_yuvHeight;
    mfxBitstream            m_internalBS;//rudiment bs
    mfxSyncPoint            m_syncPoint;
    mfxU32                  m_nFrames;
    //fourcc of bitstream
    //mfxU32                  m_nInFourCC;
    //mfxU32                  m_nOutFourCC;
    const vm_char            *m_pOutlineFileName;
    std::auto_ptr<OutlineReader> m_pOutlineReader;
    std::auto_ptr<IBitstreamConverter> m_pConverter;
};

#endif//__MFX_YUV_DECODER_H
