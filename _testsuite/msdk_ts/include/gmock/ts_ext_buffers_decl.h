/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2016 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

EXTBUF(mfxExtCodingOption           , MFX_EXTBUFF_CODING_OPTION             )
EXTBUF(mfxExtCodingOptionSPSPPS     , MFX_EXTBUFF_CODING_OPTION_SPSPPS      )
EXTBUF(mfxExtVPPDoNotUse            , MFX_EXTBUFF_VPP_DONOTUSE              )
EXTBUF(mfxExtVppAuxData             , MFX_EXTBUFF_VPP_AUXDATA               )
EXTBUF(mfxExtVPPDenoise             , MFX_EXTBUFF_VPP_DENOISE               )
EXTBUF(mfxExtVPPProcAmp             , MFX_EXTBUFF_VPP_PROCAMP               )
EXTBUF(mfxExtVPPDetail              , MFX_EXTBUFF_VPP_DETAIL                )
EXTBUF(mfxExtVideoSignalInfo        , MFX_EXTBUFF_VIDEO_SIGNAL_INFO         )
EXTBUF(mfxExtVPPDoUse               , MFX_EXTBUFF_VPP_DOUSE                 )
EXTBUF(mfxExtOpaqueSurfaceAlloc     , MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION )
EXTBUF(mfxExtAVCRefListCtrl         , MFX_EXTBUFF_AVC_REFLIST_CTRL          )
EXTBUF(mfxExtVPPFrameRateConversion , MFX_EXTBUFF_VPP_FRAME_RATE_CONVERSION )
EXTBUF(mfxExtPictureTimingSEI       , MFX_EXTBUFF_PICTURE_TIMING_SEI        )
EXTBUF(mfxExtAvcTemporalLayers      , MFX_EXTBUFF_AVC_TEMPORAL_LAYERS       )
EXTBUF(mfxExtCodingOption2          , MFX_EXTBUFF_CODING_OPTION2            )
EXTBUF(mfxExtVPPImageStab           , MFX_EXTBUFF_VPP_IMAGE_STABILIZATION   )
EXTBUF(mfxExtEncoderCapability      , MFX_EXTBUFF_ENCODER_CAPABILITY        )
EXTBUF(mfxExtEncoderResetOption     , MFX_EXTBUFF_ENCODER_RESET_OPTION      )
EXTBUF(mfxExtAVCEncodedFrameInfo    , MFX_EXTBUFF_ENCODED_FRAME_INFO        )
EXTBUF(mfxExtVPPComposite           , MFX_EXTBUFF_VPP_COMPOSITE             )
EXTBUF(mfxExtVPPVideoSignalInfo     , MFX_EXTBUFF_VPP_VIDEO_SIGNAL_INFO     )
EXTBUF(mfxExtEncoderROI             , MFX_EXTBUFF_ENCODER_ROI               )
EXTBUF(mfxExtVPPDeinterlacing       , MFX_EXTBUFF_VPP_DEINTERLACING         )
EXTBUF(mfxExtVP8CodingOption        , MFX_EXTBUFF_VP8_CODING_OPTION         )
EXTBUF(mfxExtLAControl              , MFX_EXTBUFF_LOOKAHEAD_CTRL            )
EXTBUF(mfxExtVPPFieldProcessing     , MFX_EXTBUFF_VPP_FIELD_PROCESSING      )
// FEI
EXTBUF(mfxExtFeiParam               , MFX_EXTBUFF_FEI_PARAM                 )
EXTBUF(mfxExtFeiEncFrameCtrl        , MFX_EXTBUFF_FEI_ENC_CTRL              )
EXTBUF(mfxExtFeiEncMVPredictors     , MFX_EXTBUFF_FEI_ENC_MV_PRED           )
EXTBUF(mfxExtFeiEncMBCtrl           , MFX_EXTBUFF_FEI_ENC_MB                )
EXTBUF(mfxExtFeiEncMV               , MFX_EXTBUFF_FEI_ENC_MV                )
EXTBUF(mfxExtFeiEncMBStat           , MFX_EXTBUFF_FEI_ENC_MB_STAT           )
EXTBUF(mfxExtFeiPakMBCtrl           , MFX_EXTBUFF_FEI_PAK_CTRL              )
EXTBUF(mfxExtFEIH265Param           , MFX_EXTBUFF_FEI_H265_PARAM            )
EXTBUF(mfxExtFEIH265Input           , MFX_EXTBUFF_FEI_H265_INPUT            )
EXTBUF(mfxExtFEIH265Output          , MFX_EXTBUFF_FEI_H265_OUTPUT           )
// end of FEI
// Camera
EXTBUF(mfxExtCamTotalColorControl   , MFX_EXTBUF_CAM_TOTAL_COLOR_CONTROL     )
EXTBUF(mfxExtCamCscYuvRgb           , MFX_EXTBUF_CAM_CSC_YUV_RGB             )
EXTBUF(mfxExtCamGammaCorrection     , MFX_EXTBUF_CAM_GAMMA_CORRECTION        )
EXTBUF(mfxExtCamWhiteBalance        , MFX_EXTBUF_CAM_WHITE_BALANCE           )
EXTBUF(mfxExtCamHotPixelRemoval     , MFX_EXTBUF_CAM_HOT_PIXEL_REMOVAL       )
EXTBUF(mfxExtCamBlackLevelCorrection, MFX_EXTBUF_CAM_BLACK_LEVEL_CORRECTION  )
EXTBUF(mfxExtCamVignetteCorrection  , MFX_EXTBUF_CAM_VIGNETTE_CORRECTION     )
EXTBUF(mfxExtCamBayerDenoise        , MFX_EXTBUF_CAM_BAYER_DENOISE           )
EXTBUF(mfxExtCamColorCorrection3x3  , MFX_EXTBUF_CAM_COLOR_CORRECTION_3X3    )
EXTBUF(mfxExtCamPadding             , MFX_EXTBUF_CAM_PADDING                 )
EXTBUF(mfxExtCamPipeControl         , MFX_EXTBUF_CAM_PIPECONTROL             )
// end of Camera
EXTBUF(mfxExtAVCRefLists            , MFX_EXTBUFF_AVC_REFLISTS               )
EXTBUF(mfxExtCodingOption3          , MFX_EXTBUFF_CODING_OPTION3             )
EXTBUF(mfxExtMBQP                   , MFX_EXTBUFF_MBQP                       )

EXTBUF(mfxExtChromaLocInfo          , MFX_EXTBUFF_CHROMA_LOC_INFO            )
EXTBUF(mfxExtDecodedFrameInfo       , MFX_EXTBUFF_DECODED_FRAME_INFO         )

// Threading API
EXTBUF(mfxExtThreadsParam           , MFX_EXTBUFF_THREADS_PARAM)

EXTBUF(mfxExtVPPRotation            , MFX_EXTBUFF_VPP_ROTATION)
EXTBUF(mfxExtMVCSeqDesc             , MFX_EXTBUFF_MVC_SEQ_DESC              )
//EXTBUF(mfxExtMVCTargetViews         , MFX_EXTBUFF_MVC_TARGET_VIEWS          )
//EXTBUF(mfxExtJPEGQuantTables        , MFX_EXTBUFF_JPEG_QT                   )
//EXTBUF(mfxExtJPEGHuffmanTables      , MFX_EXTBUFF_JPEG_HUFFMAN              )
//EXTBUF(mfxExtPAVPOption             , MFX_EXTBUFF_PAVP_OPTION               )
EXTBUF(mfxExtMBDisableSkipMap       , MFX_EXTBUFF_MB_DISABLE_SKIP_MAP)

//Screen capture
EXTBUF(mfxExtScreenCaptureParam     , MFX_EXTBUFF_SCREEN_CAPTURE_PARAM      )
EXTBUF(mfxExtDirtyRect              , MFX_EXTBUFF_DIRTY_RECTANGLES          )
EXTBUF(mfxExtMoveRect               , MFX_EXTBUFF_MOVING_RECTANGLES         )

EXTBUF(mfxExtCodingOptionVP9        , MFX_EXTBUFF_CODING_OPTION_VP9         )
EXTBUF(mfxExtVP9DecodedFrameInfo    , MFX_EXTBUFF_VP9_DECODED_FRAME_INFO    )