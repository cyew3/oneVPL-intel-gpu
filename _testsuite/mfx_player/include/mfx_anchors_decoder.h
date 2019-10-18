/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2019 Intel Corporation. All Rights Reserved.

File Name: mfx_preload_decoder.h

\* ****************************************************************************** */

#if (MFX_VERSION >= MFX_VERSION_NEXT)

#ifndef MFX_ANCHOR_FRAMES_DECODER
#define MFX_ANCHOR_FRAMES_DECODER


#include "mfx_decoder.h"
#include "mfx_loop_decoder.h"
#include "mfx_iproxy.h"
#include "mfx_bitsream_buffer.h"
#include "mfx_buffered_bs_reader.h"

class MFXAV1AnchorsDecoder
    : public InterfaceProxy<IYUVSource>
{
public:
    // anchors from separate YUV file
    MFXAV1AnchorsDecoder(
        IVideoSession* session,
        std::unique_ptr<IYUVSource> &&pTarget,
        mfxVideoParam &frameParam,
        IMFXPipelineFactory * pFactory,
        const vm_char *  anchorsFileInput,
        mfxU32 anchorFramesNum);

    // anchors from stream (first frames are anchors)
    MFXAV1AnchorsDecoder(
        IVideoSession* session,
        std::unique_ptr<IYUVSource> &&pTarget,
        mfxVideoParam &frameParam,
        IMFXPipelineFactory * pFactory,
        mfxU32 anchorFramesNum);

    virtual mfxStatus Init(mfxVideoParam *par);
    virtual mfxStatus Close();
    virtual mfxStatus QueryIOSurf(mfxVideoParam *par, mfxFrameAllocRequest *request);

    virtual mfxStatus DecodeFrameAsync(mfxBitstream2 &bs, mfxFrameSurface1 *surface_work, mfxFrameSurface1 **surface_out, mfxSyncPoint *syncp);

protected:

    mfxSession m_session;

    mfxU32                                      m_anchors_source;
    std::unique_ptr<IYUVSource>                 m_pAnchorsYUVSource;

    // preloaded (anchor for AV1) frames decoder - should be limited by anchor frames num
    mfxU32                                      m_PreloadedFramesNum;
    mfxU32                                      m_PreloadedFramesDecoded;
    std::vector<mfxFrameSurface1 *>             m_anchorSurfaces;

    vm_char                                     m_srcFile[MAX_FILE_PATH];
    MFXBistreamBuffer                           m_bitstreamBuf;
    IBitstreamReader                            *m_pSpl;

    bool                                        bCompleteFrame;
    mfxU32                                      m_nMaxAsync;

    typedef std::pair <mfxSyncPoint, SrfEncCtl> SyncPair;
    std::list<SyncPair>                         m_SyncPoints;

    mfxStatus PreloadFrames(mfxVideoParam &par);
    mfxStatus RunDecode(mfxBitstream2 & bs);
    mfxStatus CheckExitingCondition();
    mfxStatus FindFreeSurface(mfxFrameSurface1 **sfc);
    mfxStatus CreateSplitter(mfxFrameInfo &frameInfo);

};

#endif // MFX_ANCHOR_FRAMES_DECODER
#endif // (MFX_VERSION >= MFX_VERSION_NEXT)
