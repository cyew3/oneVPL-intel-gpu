/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011-2019 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#pragma once

#include "mfx_iyuv_source.h"
#include "mfx_ibitstream_writer.h"
#include "mfx_state_generic.h"
#include "mfx_surfaces_pool.h"
#include "mfx_iproxy.h"
#include "mfx_frame_allocator_rw.h"

//TODO: switch to another way of holding settings and logics
//creating an external event listener for video params after decode
class DecodeContext
{
public :
    std::unique_ptr<IYUVSource> pSource;
    //allocator already set to session and initialized
    MFXFrameAllocatorRW * pAlloc;
    //session life time managed by object accepted context
    mfxSession          session;

    mfxFrameAllocRequest request;
    //codecid for example is necessary for mediasdk decode
    mfxVideoParam       sInitialParams;
    //sync primitive
    ITime             * pTime;
    //
    bool                bCompleteFrame;

    DecodeContext()
        : pAlloc{}
        , session{}
        , pTime{}
        , bCompleteFrame{} {}

    DecodeContext(DecodeContext &&ctx) = default;
};

class BitstreamDecoder
    : public GenericStateContext<mfxBitstream2 &>
    , public InterfaceProxy<IYUVSource>
    , public IBitstreamWriter
{
    typedef InterfaceProxy<IYUVSource> base;
public:
    //todo: here a room for dynamic initializing
    explicit BitstreamDecoder(DecodeContext &&ctx);
    virtual ~BitstreamDecoder();

    //need bitstream only on input
    virtual mfxStatus Write(mfxBitstream * pBs);

    virtual mfxStatus Init(mfxVideoParam *par);
    virtual mfxStatus DecodeFrameAsync(mfxBitstream2 & bs, mfxFrameSurface1 *surface_work, mfxFrameSurface1 **surface_out, mfxSyncPoint *syncp);
    virtual mfxStatus DecodeHeader(mfxBitstream *bs, mfxVideoParam *par) ;

protected:
    virtual mfxStatus  DecodeFrameAsyncInternal(mfxBitstream2 &bs, mfxFrameSurface1 *surface_work, mfxFrameSurface1 **surface_out, mfxSyncPoint *syncp);
    DecodeContext      m_Ctx;
    SurfacesPool       m_surfaces;
    mfxBitstream2      m_bsRemained;
    std::vector<mfxU8> m_bsData;
    std::auto_ptr<MFXFrameAllocator> m_Alloc;
};

//decode from bitstream only has 2 states for now
class DecodeState_DecodeHeader
    : public GenericState<BitstreamDecoder, mfxBitstream2 &>
{
protected:
    mfxStatus HandleParticular(BitstreamDecoder *pSource, mfxBitstream2 &pData);
};

class DecodeState_Init
    : public GenericState<BitstreamDecoder, mfxBitstream2 &>
{
protected:
    mfxStatus HandleParticular(BitstreamDecoder *pSource, mfxBitstream2 &pData);
};

class DecodeState_DecodeFrameAsync
    : public GenericState<BitstreamDecoder, mfxBitstream2 &>
{
protected:
    mfxStatus HandleParticular(BitstreamDecoder *pSource, mfxBitstream2 &pData);
};
