#pragma once

#include "ts_common.h"
#include <vector>
#include <algorithm>

template<class T> struct tsExtBufTypeToId {};

#define BIND_EXTBUF_TYPE_TO_ID(TYPE, ID) template<> struct tsExtBufTypeToId<TYPE> { enum { id = ID }; }
    BIND_EXTBUF_TYPE_TO_ID(mfxExtCodingOption           , MFX_EXTBUFF_CODING_OPTION             );
    BIND_EXTBUF_TYPE_TO_ID(mfxExtCodingOptionSPSPPS     , MFX_EXTBUFF_CODING_OPTION_SPSPPS      );
    BIND_EXTBUF_TYPE_TO_ID(mfxExtVPPDoNotUse            , MFX_EXTBUFF_VPP_DONOTUSE              );
    BIND_EXTBUF_TYPE_TO_ID(mfxExtVppAuxData             , MFX_EXTBUFF_VPP_AUXDATA               );
    BIND_EXTBUF_TYPE_TO_ID(mfxExtVPPDenoise             , MFX_EXTBUFF_VPP_DENOISE               );
    BIND_EXTBUF_TYPE_TO_ID(mfxExtVPPProcAmp             , MFX_EXTBUFF_VPP_PROCAMP               );
    BIND_EXTBUF_TYPE_TO_ID(mfxExtVPPDetail              , MFX_EXTBUFF_VPP_DETAIL                );
    BIND_EXTBUF_TYPE_TO_ID(mfxExtVideoSignalInfo        , MFX_EXTBUFF_VIDEO_SIGNAL_INFO         );
    BIND_EXTBUF_TYPE_TO_ID(mfxExtVPPDoUse               , MFX_EXTBUFF_VPP_DOUSE                 );
    BIND_EXTBUF_TYPE_TO_ID(mfxExtOpaqueSurfaceAlloc     , MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION );
    BIND_EXTBUF_TYPE_TO_ID(mfxExtAVCRefListCtrl         , MFX_EXTBUFF_AVC_REFLIST_CTRL          );
    BIND_EXTBUF_TYPE_TO_ID(mfxExtVPPFrameRateConversion , MFX_EXTBUFF_VPP_FRAME_RATE_CONVERSION );
    BIND_EXTBUF_TYPE_TO_ID(mfxExtPictureTimingSEI       , MFX_EXTBUFF_PICTURE_TIMING_SEI        );
    BIND_EXTBUF_TYPE_TO_ID(mfxExtAvcTemporalLayers      , MFX_EXTBUFF_AVC_TEMPORAL_LAYERS       );
    BIND_EXTBUF_TYPE_TO_ID(mfxExtCodingOption2          , MFX_EXTBUFF_CODING_OPTION2            );
    BIND_EXTBUF_TYPE_TO_ID(mfxExtVPPImageStab           , MFX_EXTBUFF_VPP_IMAGE_STABILIZATION   );
    BIND_EXTBUF_TYPE_TO_ID(mfxExtEncoderCapability      , MFX_EXTBUFF_ENCODER_CAPABILITY        );
    BIND_EXTBUF_TYPE_TO_ID(mfxExtEncoderResetOption     , MFX_EXTBUFF_ENCODER_RESET_OPTION      );
    BIND_EXTBUF_TYPE_TO_ID(mfxExtAVCEncodedFrameInfo    , MFX_EXTBUFF_ENCODED_FRAME_INFO        );
    BIND_EXTBUF_TYPE_TO_ID(mfxExtVPPComposite           , MFX_EXTBUFF_VPP_COMPOSITE             );
    BIND_EXTBUF_TYPE_TO_ID(mfxExtVPPVideoSignalInfo     , MFX_EXTBUFF_VPP_VIDEO_SIGNAL_INFO     );
    BIND_EXTBUF_TYPE_TO_ID(mfxExtEncoderROI             , MFX_EXTBUFF_ENCODER_ROI               );
    BIND_EXTBUF_TYPE_TO_ID(mfxExtVPPDeinterlacing       , MFX_EXTBUFF_VPP_DEINTERLACING         );
    BIND_EXTBUF_TYPE_TO_ID(mfxExtCodingOptionVP8        , MFX_EXTBUFF_VP8_EX_CODING_OPT         );
    BIND_EXTBUF_TYPE_TO_ID(mfxExtMVCSeqDesc             , MFX_EXTBUFF_MVC_SEQ_DESC              );
    BIND_EXTBUF_TYPE_TO_ID(mfxExtMVCTargetViews         , MFX_EXTBUFF_MVC_TARGET_VIEWS          );
    BIND_EXTBUF_TYPE_TO_ID(mfxExtJPEGQuantTables        , MFX_EXTBUFF_JPEG_QT                   );
    BIND_EXTBUF_TYPE_TO_ID(mfxExtJPEGHuffmanTables      , MFX_EXTBUFF_JPEG_HUFFMAN              );
    BIND_EXTBUF_TYPE_TO_ID(mfxExtPAVPOption             , MFX_EXTBUFF_PAVP_OPTION               );
#undef BIND_EXTBUF_TYPE_TO_ID

struct tsCmpExtBufById
{
    mfxU32 m_id;

    tsCmpExtBufById(mfxU32 id)
        : m_id(id)
    {
    };

    bool operator () (mfxExtBuffer* b)
    { 
        return  (b && b->BufferId == m_id);
    };
};

template <typename TB> class tsExtBufType : public TB
{
private:
    typedef std::vector<mfxExtBuffer*> EBStorage;
    typedef EBStorage::iterator        EBIterator;

    EBStorage m_buf;

    void RefreshBuffers()
    {
        NumExtParam = (mfxU32)m_buf.size();
        ExtParam = NumExtParam ? m_buf.data() : 0;
    }

public:
    tsExtBufType()
    {
        TB& base = *this;
        memset(&base, 0, sizeof(TB));
    }

    ~tsExtBufType()
    {
        for(EBIterator it = m_buf.begin(); it != m_buf.end(); it++ )
        {
            delete [] (mfxU8*)(*it);
        }
    }

    mfxExtBuffer* GetExtBuffer(mfxU32 id)
    {
        EBIterator it = std::find_if(m_buf.begin(), m_buf.end(), tsCmpExtBufById(id));
        if(it != m_buf.end())
        {
            return *it;
        }
        return 0;
    }

    void AddExtBuffer(mfxU32 id, mfxU32 size)
    {
        m_buf.push_back( (mfxExtBuffer*)new mfxU8[size] );
        mfxExtBuffer& eb = *m_buf.back();

        memset(&eb, 0, size);
        eb.BufferId = id;
        eb.BufferSz = size;

        RefreshBuffers();
    }

    void RemoveExtBuffer(mfxU32 id)
    {
        EBIterator it = std::find_if(m_buf.begin(), m_buf.end(), tsCmpExtBufById(id));
        if(it != m_buf.end())
        {
            delete [] (mfxU8*)(*it);
            m_buf.erase(it);
            RefreshBuffers();
        }
    }

    template <typename T> operator T&()
    {
        mfxU32 id = tsExtBufTypeToId<T>::id;
        mfxExtBuffer * p = GetExtBuffer(id);
        
        if(!p)
        {
            AddExtBuffer(id, sizeof(T));
            p = GetExtBuffer(id);
        }

        return *(T*)p;
    }

    template <typename T> operator T*()
    {
        return (T*)GetExtBuffer(tsExtBufTypeToId<T>::id);
    }
};