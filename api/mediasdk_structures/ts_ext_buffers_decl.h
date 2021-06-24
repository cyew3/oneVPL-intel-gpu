// Copyright (c) 2018-2021 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#if defined(__MFXSTRUCTURES_H__)
EXTBUF(mfxExtCodingOption                , MFX_EXTBUFF_CODING_OPTION                   )
EXTBUF(mfxExtCodingOptionSPSPPS          , MFX_EXTBUFF_CODING_OPTION_SPSPPS            )
EXTBUF(mfxExtCodingOptionVPS             , MFX_EXTBUFF_CODING_OPTION_VPS               )
EXTBUF(mfxExtVPPDoNotUse                 , MFX_EXTBUFF_VPP_DONOTUSE                    )
EXTBUF(mfxExtVppAuxData                  , MFX_EXTBUFF_VPP_AUXDATA                     )
EXTBUF(mfxExtVPPDenoise                  , MFX_EXTBUFF_VPP_DENOISE                     )
EXTBUF(mfxExtVPPProcAmp                  , MFX_EXTBUFF_VPP_PROCAMP                     )
EXTBUF(mfxExtVPPDetail                   , MFX_EXTBUFF_VPP_DETAIL                      )
EXTBUF(mfxExtVideoSignalInfo             , MFX_EXTBUFF_VIDEO_SIGNAL_INFO               )
EXTBUF(mfxExtVPPDoUse                    , MFX_EXTBUFF_VPP_DOUSE                       )
#if defined (MFX_ENABLE_OPAQUE_MEMORY)
EXTBUF(mfxExtOpaqueSurfaceAlloc          , MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION       )
#endif
EXTBUF(mfxExtAVCRefListCtrl              , MFX_EXTBUFF_AVC_REFLIST_CTRL                )
EXTBUF(mfxExtVPPFrameRateConversion      , MFX_EXTBUFF_VPP_FRAME_RATE_CONVERSION       )
EXTBUF(mfxExtPictureTimingSEI            , MFX_EXTBUFF_PICTURE_TIMING_SEI              )
EXTBUF(mfxExtAvcTemporalLayers           , MFX_EXTBUFF_AVC_TEMPORAL_LAYERS             )
EXTBUF(mfxExtCodingOption2               , MFX_EXTBUFF_CODING_OPTION2                  )
EXTBUF(mfxExtVPPImageStab                , MFX_EXTBUFF_VPP_IMAGE_STABILIZATION         )
EXTBUF(mfxExtEncoderCapability           , MFX_EXTBUFF_ENCODER_CAPABILITY              )
EXTBUF(mfxExtEncoderResetOption          , MFX_EXTBUFF_ENCODER_RESET_OPTION            )
EXTBUF(mfxExtAVCEncodedFrameInfo         , MFX_EXTBUFF_ENCODED_FRAME_INFO              )
EXTBUF(mfxExtVPPComposite                , MFX_EXTBUFF_VPP_COMPOSITE                   )
EXTBUF(mfxExtVPPVideoSignalInfo          , MFX_EXTBUFF_VPP_VIDEO_SIGNAL_INFO           )
EXTBUF(mfxExtEncoderROI                  , MFX_EXTBUFF_ENCODER_ROI                     )
EXTBUF(mfxExtVPPDeinterlacing            , MFX_EXTBUFF_VPP_DEINTERLACING               )
EXTBUF(mfxExtVP8CodingOption             , MFX_EXTBUFF_VP8_CODING_OPTION               )
EXTBUF(mfxExtVPPFieldProcessing          , MFX_EXTBUFF_VPP_FIELD_PROCESSING            )
EXTBUF(mfxExtContentLightLevelInfo       , MFX_EXTBUFF_CONTENT_LIGHT_LEVEL_INFO        )
EXTBUF(mfxExtMasteringDisplayColourVolume, MFX_EXTBUFF_MASTERING_DISPLAY_COLOUR_VOLUME )
#if (MFX_VERSION >= 1026)
EXTBUF(mfxExtVppMctf                     , MFX_EXTBUFF_VPP_MCTF                        )
#endif
EXTBUF(mfxExtColorConversion             , MFX_EXTBUFF_VPP_COLOR_CONVERSION            )
EXTBUF(mfxExtAVCRefLists                 , MFX_EXTBUFF_AVC_REFLISTS               )
EXTBUF(mfxExtCodingOption3               , MFX_EXTBUFF_CODING_OPTION3             )
EXTBUF(mfxExtMBQP                        , MFX_EXTBUFF_MBQP                       )
EXTBUF(mfxExtMBForceIntra                , MFX_EXTBUFF_MB_FORCE_INTRA             )
EXTBUF(mfxExtChromaLocInfo               , MFX_EXTBUFF_CHROMA_LOC_INFO            )
EXTBUF(mfxExtDecodedFrameInfo            , MFX_EXTBUFF_DECODED_FRAME_INFO         )
EXTBUF(mfxExtDecodeErrorReport           , MFX_EXTBUFF_DECODE_ERROR_REPORT        )
EXTBUF(mfxExtVPPRotation                 , MFX_EXTBUFF_VPP_ROTATION)
EXTBUF(mfxExtVPPMirroring                , MFX_EXTBUFF_VPP_MIRRORING)
EXTBUF(mfxExtMVCSeqDesc                  , MFX_EXTBUFF_MVC_SEQ_DESC)
EXTBUF(mfxExtMBDisableSkipMap            , MFX_EXTBUFF_MB_DISABLE_SKIP_MAP)
EXTBUF(mfxExtDirtyRect                   , MFX_EXTBUFF_DIRTY_RECTANGLES          )
EXTBUF(mfxExtMoveRect                    , MFX_EXTBUFF_MOVING_RECTANGLES         )
#if (MFX_VERSION >= 1026)
EXTBUF(mfxExtVP9Segmentation             , MFX_EXTBUFF_VP9_SEGMENTATION          )
EXTBUF(mfxExtVP9TemporalLayers           , MFX_EXTBUFF_VP9_TEMPORAL_LAYERS       )
EXTBUF(mfxExtVP9Param                    , MFX_EXTBUFF_VP9_PARAM                 )
#endif
EXTBUF(mfxExtHEVCParam                   , MFX_EXTBUFF_HEVC_PARAM                )
EXTBUF(mfxExtHEVCTiles                   , MFX_EXTBUFF_HEVC_TILES                )
EXTBUF(mfxExtPredWeightTable             , MFX_EXTBUFF_PRED_WEIGHT_TABLE         )
EXTBUF(mfxExtEncodedUnitsInfo            , MFX_EXTBUFF_ENCODED_UNITS_INFO        )
#if (MFX_VERSION >= 1031)
EXTBUF(mfxExtPartialBitstreamParam       , MFX_EXTBUFF_PARTIAL_BITSTREAM_PARAM   )
#endif
EXTBUF(mfxExtEncoderIPCMArea             , MFX_EXTBUFF_ENCODER_IPCM_AREA               )
EXTBUF(mfxExtInsertHeaders               , MFX_EXTBUFF_INSERT_HEADERS                  )

#if (MFX_VERSION >= MFX_VERSION_NEXT)
EXTBUF(mfxExtDPB                         , MFX_EXTBUFF_DPB)
#endif
#if (MFX_VERSION >= MFX_VERSION_NEXT) && !defined(STRIP_EMBARGO)
EXTBUF(mfxExtAV1Param                    , MFX_EXTBUFF_AV1_PARAM)
EXTBUF(mfxExtAV1AuxData                  , MFX_EXTBUFF_AV1_AUXDATA)
EXTBUF(mfxExtAV1Segmentation             , MFX_EXTBUFF_AV1_SEGMENTATION)
#endif
#endif //defined(__MFXSTRUCTURES_H__)

#if defined(__MFXFEIH265_H__) && !defined(OPEN_SOURCE)
EXTBUF(mfxExtFEIH265Param                , MFX_EXTBUFF_FEI_H265_PARAM            )
EXTBUF(mfxExtFEIH265Input                , MFX_EXTBUFF_FEI_H265_INPUT            )
EXTBUF(mfxExtFEIH265Output               , MFX_EXTBUFF_FEI_H265_OUTPUT           )
#endif //__MFXFEIH265_H__ && !OPEN_SOURCE

#if defined(__MFXCOMMON_H__)
// Threading API
EXTBUF(mfxExtThreadsParam                , MFX_EXTBUFF_THREADS_PARAM)
#endif //defined(__MFXCOMMON_H__)

#if defined(MFX_ENABLE_VP9_VIDEO_DECODE) && defined(__MFXVP9_H__)
#if (MFX_VERSION >= MFX_VERSION_NEXT)
EXTBUF(mfxExtVP9DecodedFrameInfo         , MFX_EXTBUFF_VP9_DECODED_FRAME_INFO    )
#endif
#endif //MFX_ENABLE_VP9_VIDEO_DECODE && __MFXVP9_H__

#if defined(__MFXBRC_H__)
EXTBUF(mfxExtBRC, MFX_EXTBUFF_BRC)
#endif // defined(__MFXBRC_H__)

#if defined(__MFXPAVP_H__)
#if !defined(OPEN_SOURCE)
EXTBUF(mfxExtPAVPOption                  , MFX_EXTBUFF_PAVP_OPTION               )
#endif

#if (MFX_VERSION >= 1030)
EXTBUF(mfxExtCencParam                   , MFX_EXTBUFF_CENC_PARAM                )
#endif
#endif // defined(__MFXPCP_H__)

#if defined(__MFXSCD_H__)
EXTBUF(mfxExtSCD, MFX_EXTBUFF_SCD)
#endif // defined(__MFXSCD_H__)

#ifdef __MFXWIDI_H__
EXTBUF(mfxExtAVCEncoderWiDiUsage         , MFX_EXTBUFF_ENCODER_WIDI_USAGE        )
#endif //#ifdef

#ifdef __MFXLA_H__
EXTBUF(mfxExtLAControl                   , MFX_EXTBUFF_LOOKAHEAD_CTRL            )
EXTBUF(mfxExtLAFrameStatistics           , MFX_EXTBUFF_LOOKAHEAD_STAT            )
#endif //__MFXLA_H__

#if defined(__MFX_EXT_BUFFERS_H__)
EXTBUF(mfxExtCodingOptionDDI             , MFX_EXTBUFF_DDI                       )
#if defined(MFX_UNDOCUMENTED_DUMP_FILES)
EXTBUF(mfxExtDumpFiles, MFX_EXTBUFF_DUMP)
#endif //MFX_UNDOCUMENTED_DUMP_FILES
#if defined (MFX_ENABLE_LP_LOOKAHEAD) || defined(MFX_ENABLE_ENCTOOLS_LPLA)
EXTBUF(mfxExtLplaParam                   , MFX_EXTBUFF_LP_LOOKAHEAD              )
#endif
#if defined(MFX_ENABLE_ENCTOOLS_LPLA)
EXTBUF(mfxExtLpLaStatus                  , MFX_EXTBUFF_LPLA_STATUS               )
#endif

EXTBUF(mfxExtHyperModeParam               , MFX_EXTBUFF_HYPER_MODE_PARAM    	 )

#if defined(MFX_ENABLE_ENCTOOLS)
EXTBUF(mfxExtEncToolsConfig              , MFX_EXTBUFF_ENCTOOLS_CONFIG           )
#endif

#endif // defined(__MFX_EXT_BUFFERS_H__)