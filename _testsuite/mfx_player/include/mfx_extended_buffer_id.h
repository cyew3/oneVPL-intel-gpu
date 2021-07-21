/******************************************************************************* *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011-2021 Intel Corporation. All Rights Reserved.

File Name: mfx_extended_buffer_id.h

*******************************************************************************/

#pragma once
#include "mfxpavp.h"
#include "mfxdeprecated.h"
#if defined(MFX_ENABLE_USER_ENCTOOLS) && defined(MFX_ENABLE_ENCTOOLS)
#include "mfxenctools-int.h"
#endif

#include "mfx_extended_buffer.h"

#ifndef INCLUDED_FROM_MFX_EXTENDED_BUFFER_H
    #error "mfx_extended_buffer_id.h should be included only by mfx_extenedd_buffer.h"
#endif

//////////////////////////////////////////////////////////////////////////
//Factory that holds registered prototypes for mfxextbuffers

class MFXExtBufferFactory 
    : public MFXNTSSingleton<MFXExtBufferFactory>
{
    typedef  std::map<mfxU32, MFXExtBufferPtrBase *> ContainerType ;
    ContainerType m_registered;
public:
    ~MFXExtBufferFactory ()
    {
        for_each(m_registered.begin()
            , m_registered.end()
            , second_of_t<deleter<MFXExtBufferPtrBase *> >());
    }
    template <class T>
    void RegisterBuffer()
    {
        //buffer already registered
        if(m_registered.find(BufferIdOf<T>::id) != m_registered.end())
            return;

        m_registered[BufferIdOf<T>::id] = new MFXExtBufferPtr<T>();
    }

    MFXExtBufferPtrBase* CreateBuffer(mfxU32 bufferId)
    {
        ContainerType::iterator it;
        if (m_registered.end() == (it = m_registered.find(bufferId)))
        {
            //TODO: default buffer may be?
            return NULL;
        }
        return it->second->Clone();
    }

};

#define DECL_BUFFER_TYPE(buffer_class, buffer_id)\
template <> struct BufferIdOf<buffer_class> \
{\
    enum {id = buffer_id} ;\
};\
namespace {\
    struct buffer_class##_Registrator\
    {\
        buffer_class##_Registrator()\
        {\
            MFXExtBufferFactory::Instance().RegisterBuffer<buffer_class>();\
        }\
    } register_buffer_##buffer_id;\
}

//

#define MFX_EXTBUFF_DEC_ADAPTIVE_PLAYBACK MFX_MAKEFOURCC('A','P','B','K')

typedef struct {
    mfxExtBuffer    Header;
} mfxExtAdaptivePlayback;

DECL_BUFFER_TYPE(mfxExtAdaptivePlayback, MFX_EXTBUFF_DEC_ADAPTIVE_PLAYBACK);
DECL_BUFFER_TYPE(mfxExtVPPDoUse, MFX_EXTBUFF_VPP_DOUSE);
DECL_BUFFER_TYPE(mfxExtVPPDoNotUse, MFX_EXTBUFF_VPP_DONOTUSE);
DECL_BUFFER_TYPE(mfxExtVppAuxData, MFX_EXTBUFF_VPP_AUXDATA);
DECL_BUFFER_TYPE(mfxExtVPPDenoise, MFX_EXTBUFF_VPP_DENOISE);
DECL_BUFFER_TYPE(mfxExtVPPDetail, MFX_EXTBUFF_VPP_DETAIL);
DECL_BUFFER_TYPE(mfxExtVPPRotation, MFX_EXTBUFF_VPP_ROTATION);
DECL_BUFFER_TYPE(mfxExtVPPMirroring, MFX_EXTBUFF_VPP_MIRRORING);
DECL_BUFFER_TYPE(mfxExtVPPVideoSignalInfo, MFX_EXTBUFF_VPP_VIDEO_SIGNAL_INFO);
DECL_BUFFER_TYPE(mfxExtVPPScaling, MFX_EXTBUFF_VPP_SCALING);
DECL_BUFFER_TYPE(mfxExtColorConversion, MFX_EXTBUFF_VPP_COLOR_CONVERSION);
DECL_BUFFER_TYPE(mfxExtVPPProcAmp, MFX_EXTBUFF_VPP_PROCAMP);
DECL_BUFFER_TYPE(mfxExtVPPFieldProcessing, MFX_EXTBUFF_VPP_FIELD_PROCESSING);
DECL_BUFFER_TYPE(mfxExtVPPDeinterlacing, MFX_EXTBUFF_VPP_DEINTERLACING);
DECL_BUFFER_TYPE(mfxExtVPPDenoise2, MFX_EXTBUFF_VPP_DENOISE2);
DECL_BUFFER_TYPE(mfxExtMVCSeqDesc, MFX_EXTBUFF_MVC_SEQ_DESC);
DECL_BUFFER_TYPE(mfxExtCodingOption, MFX_EXTBUFF_CODING_OPTION);
DECL_BUFFER_TYPE(mfxExtCodingOption2, MFX_EXTBUFF_CODING_OPTION2);
DECL_BUFFER_TYPE(mfxExtCodingOption3,           MFX_EXTBUFF_CODING_OPTION3      );
DECL_BUFFER_TYPE(mfxExtCodingOptionDDI, MFX_EXTBUFF_DDI);
#if defined(MFX_ENABLE_USER_ENCTOOLS) && defined(MFX_ENABLE_ENCTOOLS)
DECL_BUFFER_TYPE(mfxExtEncToolsConfig, MFX_EXTBUFF_ENCTOOLS_CONFIG);
#endif
DECL_BUFFER_TYPE(mfxExtCodingOptionHEVC, MFX_EXTBUFF_HEVCENC);
DECL_BUFFER_TYPE(mfxExtCodingOptionAV1E, MFX_EXTBUFF_AV1ENC);
#if (MFX_VERSION >= MFX_VERSION_NEXT)
DECL_BUFFER_TYPE(mfxExtAV1BitstreamParam, MFX_EXTBUFF_AV1_BITSTREAM_PARAM);
DECL_BUFFER_TYPE(mfxExtAV1ResolutionParam, MFX_EXTBUFF_AV1_RESOLUTION_PARAM);
DECL_BUFFER_TYPE(mfxExtAV1TileParam, MFX_EXTBUFF_AV1_TILE_PARAM);
DECL_BUFFER_TYPE(mfxExtAV1Param, MFX_EXTBUFF_AV1_PARAM);
DECL_BUFFER_TYPE(mfxExtAV1AuxData, MFX_EXTBUFF_AV1_AUXDATA);
DECL_BUFFER_TYPE(mfxExtAV1Segmentation, MFX_EXTBUFF_AV1_SEGMENTATION);
#endif
DECL_BUFFER_TYPE(mfxExtHEVCTiles, MFX_EXTBUFF_HEVC_TILES);
DECL_BUFFER_TYPE(mfxExtHEVCParam, MFX_EXTBUFF_HEVC_PARAM);
DECL_BUFFER_TYPE(mfxExtVP8CodingOption, MFX_EXTBUFF_VP8_CODING_OPTION);
DECL_BUFFER_TYPE(mfxExtVP9Param, MFX_EXTBUFF_VP9_PARAM);
DECL_BUFFER_TYPE(mfxExtVP9Segmentation, MFX_EXTBUFF_VP9_SEGMENTATION);
DECL_BUFFER_TYPE(mfxExtEncoderROI, MFX_EXTBUFF_ENCODER_ROI);
DECL_BUFFER_TYPE(mfxExtDirtyRect, MFX_EXTBUFF_DIRTY_RECTANGLES);
DECL_BUFFER_TYPE(mfxExtMoveRect, MFX_EXTBUFF_MOVING_RECTANGLES);
DECL_BUFFER_TYPE(mfxExtEncoderCapability, MFX_EXTBUFF_ENCODER_CAPABILITY);
DECL_BUFFER_TYPE(mfxExtEncoderResetOption, MFX_EXTBUFF_ENCODER_RESET_OPTION);
DECL_BUFFER_TYPE(mfxExtMultiFrameParam, MFX_EXTBUFF_MULTI_FRAME_PARAM);

DECL_BUFFER_TYPE(mfxExtAVCEncodedFrameInfo, MFX_EXTBUFF_ENCODED_FRAME_INFO);
#ifdef MFX_UNDOCUMENTED_DUMP_FILES
DECL_BUFFER_TYPE(mfxExtDumpFiles, MFX_EXTBUFF_DUMP);
#endif
DECL_BUFFER_TYPE(mfxExtMVCTargetViews, MFX_EXTBUFF_MVC_TARGET_VIEWS);
DECL_BUFFER_TYPE(mfxExtVideoSignalInfo, MFX_EXTBUFF_VIDEO_SIGNAL_INFO);
DECL_BUFFER_TYPE(mfxExtAVCRefListCtrl, MFX_EXTBUFF_AVC_REFLIST_CTRL);
DECL_BUFFER_TYPE(mfxExtVPPFrameRateConversion, MFX_EXTBUFF_VPP_FRAME_RATE_CONVERSION);
DECL_BUFFER_TYPE(mfxExtVPPImageStab, MFX_EXTBUFF_VPP_IMAGE_STABILIZATION);
DECL_BUFFER_TYPE(mfxExtCodingOptionSPSPPS, MFX_EXTBUFF_CODING_OPTION_SPSPPS);
DECL_BUFFER_TYPE(mfxExtAvcTemporalLayers, MFX_EXTBUFF_AVC_TEMPORAL_LAYERS);
DECL_BUFFER_TYPE(mfxExtVP9TemporalLayers, MFX_EXTBUFF_VP9_TEMPORAL_LAYERS);
DECL_BUFFER_TYPE(mfxExtSVCRateControl, MFX_EXTBUFF_SVC_RATE_CONTROL);
DECL_BUFFER_TYPE(mfxExtSVCSeqDesc, MFX_EXTBUFF_SVC_SEQ_DESC);
DECL_BUFFER_TYPE(mfxExtSvcTargetLayer, MFX_EXTBUFF_SVC_TARGET_LAYER);
#if (MFX_VERSION >= MFX_VERSION_NEXT)
DECL_BUFFER_TYPE(mfxExtTemporalLayers, MFX_EXTBUFF_TEMPORAL_LAYERS);
DECL_BUFFER_TYPE(mfxExtAV1LargeScaleTileParam, MFX_EXTBUFF_AV1_LST_PARAM);
#endif

DECL_BUFFER_TYPE(mfxExtPAVPOption, MFX_EXTBUFF_PAVP_OPTION);
DECL_BUFFER_TYPE(mfxExtDecVideoProcessing, MFX_EXTBUFF_DEC_VIDEO_PROCESSING);
DECL_BUFFER_TYPE(mfxExtAVCRefLists, MFX_EXTBUFF_AVC_REFLISTS);
DECL_BUFFER_TYPE(mfxExtPredWeightTable, MFX_EXTBUFF_PRED_WEIGHT_TABLE);

#ifdef MFX_EXTBUFF_FORCE_PRIVATE_DDI_ENABLE
DECL_BUFFER_TYPE(mfxExtForcePrivateDDI, MFX_EXTBUFF_FORCE_PRIVATE_DDI);
#endif

//init helper
template <class T>
T& mfx_init_ext_buffer(T & buffer)
{
    memset(&buffer, 0, sizeof(T));
    reinterpret_cast<mfxExtBuffer*>(&buffer)->BufferId = BufferIdOf<T>::id;
    reinterpret_cast<mfxExtBuffer*>(&buffer)->BufferSz = sizeof(T);
    return buffer;
}

template <class T>
void mfx_set_ext_buffer_header(T & buffer)
{
    reinterpret_cast<mfxExtBuffer*>(&buffer)->BufferId = BufferIdOf<T>::id;
    reinterpret_cast<mfxExtBuffer*>(&buffer)->BufferSz = sizeof(T);
}

