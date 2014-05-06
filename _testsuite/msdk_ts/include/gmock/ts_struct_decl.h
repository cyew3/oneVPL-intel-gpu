

STRUCT(mfxI16Pair,
    FIELD_T(mfxI16, x)
    FIELD_T(mfxI16, y)
)

STRUCT(mfxHDLPair,
    FIELD_T(mfxHDL, first )
    FIELD_T(mfxHDL, second)
)

STRUCT(mfxExtBuffer,
    FIELD_T(mfx4CC, BufferId)
    FIELD_T(mfxU32, BufferSz)
)

STRUCT(mfxVersion,
    FIELD_T(mfxU16, Minor  )
    FIELD_T(mfxU16, Major  )
    FIELD_T(mfxU32, Version)
)

STRUCT(mfxBitstream,
    FIELD_T(mfxEncryptedData*, EncryptedData  )
    FIELD_T(mfxExtBuffer **  , ExtParam       )
    FIELD_T(mfxU16           , NumExtParam    )
    FIELD_T(mfxI64           , DecodeTimeStamp)
    FIELD_T(mfxU64           , TimeStamp      )
    FIELD_T(mfxU8*           , Data           )
    FIELD_T(mfxU32           , DataOffset     )
    FIELD_T(mfxU32           , DataLength     )
    FIELD_T(mfxU32           , MaxLength      )
    FIELD_T(mfxU16           , PicStruct      )
    FIELD_T(mfxU16           , FrameType      )
    FIELD_T(mfxU16           , DataFlag       )
)

STRUCT(mfxFrameId,
    FIELD_T(mfxU16, TemporalId  )
    FIELD_T(mfxU16, PriorityId  )
    FIELD_T(mfxU16, DependencyId)
    FIELD_T(mfxU16, QualityId   )
    FIELD_T(mfxU16, ViewId      )
)

STRUCT(mfxFrameInfo,
    FIELD_S(mfxFrameId, FrameId)
    FIELD_T(mfxU16, BitDepthLuma  )
    FIELD_T(mfxU16, BitDepthChroma)
    FIELD_T(mfxU16, Shift         )
    FIELD_T(mfx4CC, FourCC        )
    FIELD_T(mfxU16, Width         )
    FIELD_T(mfxU16, Height        )
    FIELD_T(mfxU16, CropX         )
    FIELD_T(mfxU16, CropY         )
    FIELD_T(mfxU16, CropW         )
    FIELD_T(mfxU16, CropH         )
    FIELD_T(mfxU32, FrameRateExtN )
    FIELD_T(mfxU32, FrameRateExtD )
    FIELD_T(mfxU16, AspectRatioW  )
    FIELD_T(mfxU16, AspectRatioH  )
    FIELD_T(mfxU16, PicStruct     )
    FIELD_T(mfxU16, ChromaFormat  )
)

STRUCT(mfxFrameData,
    FIELD_T(mfxU16  , PitchHigh  )
    FIELD_T(mfxU64  , TimeStamp  )
    FIELD_T(mfxU32  , FrameOrder )
    FIELD_T(mfxU16  , Locked     )
    FIELD_T(mfxU16  , Pitch      )
    FIELD_T(mfxU16  , PitchLow   )
    FIELD_T(mfxU8 * , Y          )
    FIELD_T(mfxU16* , Y16        )
    FIELD_T(mfxU8 * , R          )
    FIELD_T(mfxU8 * , UV         )
    FIELD_T(mfxU8 * , VU         )
    FIELD_T(mfxU8 * , CbCr       )
    FIELD_T(mfxU8 * , CrCb       )
    FIELD_T(mfxU8 * , Cb         )
    FIELD_T(mfxU8 * , U          )
    FIELD_T(mfxU16* , U16        )
    FIELD_T(mfxU8 * , G          )
    FIELD_T(mfxU8 * , Cr         )
    FIELD_T(mfxU8 * , V          )
    FIELD_T(mfxU16* , V16        )
    FIELD_T(mfxU8 * , B          )
    FIELD_T(mfxU8 * , A          )
    FIELD_T(mfxMemId,  MemId     )
    FIELD_T(mfxU16  , Corrupted  )
    FIELD_T(mfxU16  , DataFlag   )
)

STRUCT(mfxFrameSurface1,
    FIELD_S(mfxFrameInfo, Info)
    FIELD_S(mfxFrameData, Data)
)

STRUCT(mfxInfoMFX,
    FIELD_S(mfxFrameInfo, FrameInfo)
    FIELD_T(mfxU16, BRCParamMultiplier)
    FIELD_T(mfx4CC, CodecId           )
    FIELD_T(mfxU16, CodecProfile      )
    FIELD_T(mfxU16, CodecLevel        )
    FIELD_T(mfxU16, NumThread         )
    FIELD_T(mfxU16, TargetUsage       )
    FIELD_T(mfxU16, GopPicSize        )
    FIELD_T(mfxU16, GopRefDist        )
    FIELD_T(mfxU16, GopOptFlag        )
    FIELD_T(mfxU16, IdrInterval       )
    FIELD_T(mfxU16, RateControlMethod )
    FIELD_T(mfxU16, InitialDelayInKB  )
    FIELD_T(mfxU16, QPI               )
    FIELD_T(mfxU16, Accuracy          )
    FIELD_T(mfxU16, BufferSizeInKB    )
    FIELD_T(mfxU16, TargetKbps        )
    FIELD_T(mfxU16, QPP               )
    FIELD_T(mfxU16, ICQQuality        )
    FIELD_T(mfxU16, MaxKbps           )
    FIELD_T(mfxU16, QPB               )
    FIELD_T(mfxU16, Convergence       )
    FIELD_T(mfxU16, NumSlice          )
    FIELD_T(mfxU16, NumRefFrame       )
    FIELD_T(mfxU16, EncodedOrder      )
    FIELD_T(mfxU16, DecodedOrder      )
    FIELD_T(mfxU16, ExtendedPicStruct )
    FIELD_T(mfxU16, TimeStampCalc     )
    FIELD_T(mfxU16, SliceGroupsPresent)
    FIELD_T(mfxU16, JPEGChromaFormat  )
    FIELD_T(mfxU16, Rotation          )
    FIELD_T(mfxU16, JPEGColorFormat   )
    FIELD_T(mfxU16, InterleavedDec    )
    FIELD_T(mfxU16, Interleaved       )
    FIELD_T(mfxU16, Quality           )
    FIELD_T(mfxU16, RestartInterval   )
)

STRUCT(mfxInfoVPP,
    FIELD_S(mfxFrameInfo, In)
    FIELD_S(mfxFrameInfo, Out)
)

STRUCT(mfxVideoParam,
    FIELD_S(mfxInfoMFX, mfx)
    FIELD_S(mfxInfoVPP, vpp)
    FIELD_T(mfxU16        , AsyncDepth  )
    FIELD_T(mfxU16        , Protected   )
    FIELD_T(mfxU16        , IOPattern   )
    FIELD_T(mfxExtBuffer**, ExtParam    )
    FIELD_T(mfxU16        , NumExtParam )
)

STRUCT(mfxExtCodingOption,
    FIELD_S(mfxExtBuffer, Header               )
    FIELD_T(mfxU16      , reserved1            )
    FIELD_T(mfxU16      , RateDistortionOpt    )
    FIELD_T(mfxU16      , MECostType           )
    FIELD_T(mfxU16      , MESearchType         )
    FIELD_S(mfxI16Pair  , MVSearchWindow       )
    FIELD_T(mfxU16      , EndOfSequence        )
    FIELD_T(mfxU16      , FramePicture         )
    FIELD_T(mfxU16      , CAVLC                )
    FIELD_T(mfxU16      , RecoveryPointSEI     )
    FIELD_T(mfxU16      , ViewOutput           )
    FIELD_T(mfxU16      , NalHrdConformance    )
    FIELD_T(mfxU16      , SingleSeiNalUnit     )
    FIELD_T(mfxU16      , VuiVclHrdParameters  )
    FIELD_T(mfxU16      , RefPicListReordering )
    FIELD_T(mfxU16      , ResetRefList         )
    FIELD_T(mfxU16      , RefPicMarkRep        )
    FIELD_T(mfxU16      , FieldOutput          )
    FIELD_T(mfxU16      , IntraPredBlockSize   )
    FIELD_T(mfxU16      , InterPredBlockSize   )
    FIELD_T(mfxU16      , MVPrecision          )
    FIELD_T(mfxU16      , MaxDecFrameBuffering )
    FIELD_T(mfxU16      , AUDelimiter          )
    FIELD_T(mfxU16      , EndOfStream          )
    FIELD_T(mfxU16      , PicTimingSEI         )
    FIELD_T(mfxU16      , VuiNalHrdParameters  )
)

STRUCT(mfxExtCodingOption2,
    FIELD_S(mfxExtBuffer, Header          )
    FIELD_T(mfxU16      , IntRefType      )
    FIELD_T(mfxU16      , IntRefCycleSize )
    FIELD_T(mfxI16      , IntRefQPDelta   )
    FIELD_T(mfxU32      , MaxFrameSize    )
    FIELD_T(mfxU16      , BitrateLimit    )
    FIELD_T(mfxU16      , MBBRC           )
    FIELD_T(mfxU16      , ExtBRC          )
    FIELD_T(mfxU16      , LookAheadDepth  )
    FIELD_T(mfxU16      , Trellis         )
    FIELD_T(mfxU16      , RepeatPPS       )
    FIELD_T(mfxU16      , BRefType        )
    FIELD_T(mfxU16      , AdaptiveI       )
    FIELD_T(mfxU16      , AdaptiveB       )
    FIELD_T(mfxU16      , LookAheadDS     )
    FIELD_T(mfxU16      , NumMbPerSlice   )
    FIELD_T(mfxU16      , SkipFrame       )
    FIELD_T(mfxU8       , MinQPI          )
    FIELD_T(mfxU8       , MaxQPI          )
    FIELD_T(mfxU8       , MinQPP          )
    FIELD_T(mfxU8       , MaxQPP          )
    FIELD_T(mfxU8       , MinQPB          )
    FIELD_T(mfxU8       , MaxQPB          )
    FIELD_T(mfxU16      , FixedFrameRate       )
    FIELD_T(mfxU16      , DisableDeblockingIdc )
)

STRUCT(mfxExtVPPDoNotUse,
    FIELD_S(mfxExtBuffer, Header)
    FIELD_T(mfxU32 , NumAlg)
    FIELD_T(mfxU32*, AlgList)
)

STRUCT(mfxExtVPPDenoise,
    FIELD_S(mfxExtBuffer, Header)
    FIELD_T(mfxU16, DenoiseFactor)
)

STRUCT(mfxExtVPPDetail,
    FIELD_S(mfxExtBuffer, Header)
    FIELD_T(mfxU16, DetailFactor)
)

STRUCT(mfxExtVPPProcAmp,
    FIELD_S(mfxExtBuffer, Header)
    FIELD_T(mfxF64, Brightness  )
    FIELD_T(mfxF64, Contrast    )
    FIELD_T(mfxF64, Hue         )
    FIELD_T(mfxF64, Saturation  )
)

STRUCT(mfxEncodeStat,
    FIELD_T(mfxU32, NumFrame            )
    FIELD_T(mfxU64, NumBit              )
    FIELD_T(mfxU32, NumCachedFrame      )
    FIELD_T(mfxU32, NumFrameCountAsync  )
)

STRUCT(mfxDecodeStat,
    FIELD_T(mfxU32, NumFrame        )
    FIELD_T(mfxU32, NumSkippedFrame )
    FIELD_T(mfxU32, NumError        )
    FIELD_T(mfxU32, NumCachedFrame  )
)

STRUCT(mfxVPPStat,
    FIELD_T(mfxU32, NumFrame        )
    FIELD_T(mfxU32, NumCachedFrame  )
)

STRUCT(mfxExtVppAuxData,
    FIELD_S(mfxExtBuffer, Header)
    FIELD_T(mfxU16, PicStruct       )
    FIELD_T(mfxU16, SceneChangeRate )
    FIELD_T(mfxU16, RepeatedFrame   )
)

STRUCT(mfxPayload,
    FIELD_T(mfxU8*, Data    )
    FIELD_T(mfxU32, NumBit  )
    FIELD_T(mfxU16, Type    )
    FIELD_T(mfxU16, BufSize )
)

STRUCT(mfxEncodeCtrl,
    FIELD_S(mfxExtBuffer  , Header     )
    FIELD_T(mfxU16        , SkipFrame  )
    FIELD_T(mfxU16        , QP         )
    FIELD_T(mfxU16        , FrameType  )
    FIELD_T(mfxU16        , NumExtParam)
    FIELD_T(mfxU16        , NumPayload )
    FIELD_T(mfxExtBuffer**, ExtParam   )
    FIELD_T(mfxPayload**  , Payload    )
)

STRUCT(mfxFrameAllocRequest,
    FIELD_S(mfxFrameInfo, Info)
    FIELD_T(mfxU16, Type              )
    FIELD_T(mfxU16, NumFrameMin       )
    FIELD_T(mfxU16, NumFrameSuggested )
    )

STRUCT(mfxFrameAllocResponse,
    FIELD_T(mfxMemId*, mids           )
    FIELD_T(mfxU16   , NumFrameActual )
)

STRUCT(mfxExtCodingOptionSPSPPS,
    FIELD_S(mfxExtBuffer, Header    )
    FIELD_T(mfxU8*      , SPSBuffer )
    FIELD_T(mfxU8*      , PPSBuffer )
    FIELD_T(mfxU16      , SPSBufSize)
    FIELD_T(mfxU16      , PPSBufSize)
    FIELD_T(mfxU16      , SPSId     )
    FIELD_T(mfxU16      , PPSId     )
)

STRUCT(mfxExtVideoSignalInfo,
    FIELD_S(mfxExtBuffer, Header    )
    FIELD_T(mfxU16, VideoFormat             )
    FIELD_T(mfxU16, VideoFullRange          )
    FIELD_T(mfxU16, ColourDescriptionPresent)
    FIELD_T(mfxU16, ColourPrimaries         )
    FIELD_T(mfxU16, TransferCharacteristics )
    FIELD_T(mfxU16, MatrixCoefficients      )
)

STRUCT(mfxExtVPPDoUse,
    FIELD_S(mfxExtBuffer, Header)
    FIELD_T(mfxU32 , NumAlg)
    FIELD_T(mfxU32*, AlgList)
)

STRUCT(mfxExtOpaqueSurfaceAlloc_InOut,
    FIELD_T(mfxFrameSurface1 ** , Surfaces  )
    FIELD_T(mfxU16              , Type      )
    FIELD_T(mfxU16              , NumSurface)
)

STRUCT(mfxExtOpaqueSurfaceAlloc,
    FIELD_S(mfxExtBuffer, Header)
    FIELD_S(mfxExtOpaqueSurfaceAlloc_InOut, In)
    FIELD_S(mfxExtOpaqueSurfaceAlloc_InOut, Out)
)

STRUCT(mfxExtAVCRefListCtrl_Entry,
    FIELD_T(mfxU32, FrameOrder  )
    FIELD_T(mfxU16, PicStruct   )
    FIELD_T(mfxU16, ViewId      )
    FIELD_T(mfxU16, LongTermIdx )
)

STRUCT(mfxExtAVCRefListCtrl,
    FIELD_S(mfxExtBuffer, Header)
    FIELD_T(mfxU16, NumRefIdxL0Active)
    FIELD_T(mfxU16, NumRefIdxL1Active)
    FIELD_S(mfxExtAVCRefListCtrl_Entry, PreferredRefList)
    FIELD_S(mfxExtAVCRefListCtrl_Entry, RejectedRefList )
    FIELD_S(mfxExtAVCRefListCtrl_Entry, LongTermRefList )
    FIELD_T(mfxU16, ApplyLongTermIdx )
)

STRUCT(mfxExtVPPFrameRateConversion,
    FIELD_S(mfxExtBuffer, Header)
    FIELD_T(mfxU16, Algorithm)
)

STRUCT(mfxExtVPPImageStab,
    FIELD_S(mfxExtBuffer, Header)
    FIELD_T(mfxU16, Mode)
)

STRUCT(mfxExtPictureTimingSEI_TimeStamp,
    FIELD_T(mfxU16, ClockTimestampFlag)
    FIELD_T(mfxU16, CtType            )
    FIELD_T(mfxU16, NuitFieldBasedFlag)
    FIELD_T(mfxU16, CountingType      )
    FIELD_T(mfxU16, FullTimestampFlag )
    FIELD_T(mfxU16, DiscontinuityFlag )
    FIELD_T(mfxU16, CntDroppedFlag    )
    FIELD_T(mfxU16, NFrames           )
    FIELD_T(mfxU16, SecondsFlag       )
    FIELD_T(mfxU16, MinutesFlag       )
    FIELD_T(mfxU16, HoursFlag         )
    FIELD_T(mfxU16, SecondsValue      )
    FIELD_T(mfxU16, MinutesValue      )
    FIELD_T(mfxU16, HoursValue        )
    FIELD_T(mfxU32, TimeOffset        )
)

STRUCT(mfxExtPictureTimingSEI,
    FIELD_S(mfxExtBuffer, Header)
    FIELD_S(mfxExtPictureTimingSEI_TimeStamp, TimeStamp)
)

STRUCT(mfxExtAvcTemporalLayers_Layer,
    FIELD_T(mfxU16, Scale)
)

STRUCT(mfxExtAvcTemporalLayers,
    FIELD_S(mfxExtBuffer, Header)
    FIELD_T(mfxU16, BaseLayerPID)
    FIELD_S(mfxExtAvcTemporalLayers_Layer, Layer)
)

STRUCT(mfxExtEncoderCapability,
    FIELD_S(mfxExtBuffer, Header)
    FIELD_T(mfxU32, MBPerSec)
)

STRUCT(mfxExtEncoderResetOption,
    FIELD_S(mfxExtBuffer, Header)
    FIELD_T(mfxU16, StartNewSequence)
)

STRUCT(mfxExtAVCEncodedFrameInfo_RefList,
    FIELD_T(mfxU32, FrameOrder  )
    FIELD_T(mfxU16, PicStruct   )
    FIELD_T(mfxU16, LongTermIdx )
)

STRUCT(mfxExtAVCEncodedFrameInfo,
    FIELD_S(mfxExtBuffer, Header)
    FIELD_T(mfxU32, FrameOrder  )
    FIELD_T(mfxU16, PicStruct   )
    FIELD_T(mfxU16, LongTermIdx )
    FIELD_T(mfxU32, MAD         )
    FIELD_T(mfxU16, BRCPanicMode)
    FIELD_T(mfxU16, QP          )
    FIELD_T(mfxU32, SecondFieldOffset  )
    FIELD_S(mfxExtAVCEncodedFrameInfo_RefList, UsedRefListL0)
    FIELD_S(mfxExtAVCEncodedFrameInfo_RefList, UsedRefListL1)
)

STRUCT(mfxVPPCompInputStream,
    FIELD_T(mfxU32, DstX             )
    FIELD_T(mfxU32, DstY             )
    FIELD_T(mfxU32, DstW             )
    FIELD_T(mfxU32, DstH             )
    FIELD_T(mfxU16, LumaKeyEnable    )
    FIELD_T(mfxU16, LumaKeyMin       )
    FIELD_T(mfxU16, LumaKeyMax       )
    FIELD_T(mfxU16, GlobalAlphaEnable)
    FIELD_T(mfxU16, GlobalAlpha      )
    FIELD_T(mfxU16, PixelAlphaEnable )
)

STRUCT(mfxExtVPPComposite,
    FIELD_S(mfxExtBuffer, Header)
    FIELD_T(mfxU16, Y)
    FIELD_T(mfxU16, U)
    FIELD_T(mfxU16, V)
    FIELD_T(mfxU16, R)
    FIELD_T(mfxU16, G)
    FIELD_T(mfxU16, B)
    FIELD_T(mfxU16, NumInputStream)
    FIELD_T(mfxVPPCompInputStream*, InputStream)
)

STRUCT(mfxExtVPPVideoSignalInfo_InOut,
    FIELD_T(mfxU16, TransferMatrix )
    FIELD_T(mfxU16, NominalRange   )
)

STRUCT(mfxExtVPPVideoSignalInfo,
    FIELD_S(mfxExtBuffer, Header)
    FIELD_S(mfxExtVPPVideoSignalInfo_InOut, In )
    FIELD_S(mfxExtVPPVideoSignalInfo_InOut, Out)
)

STRUCT(mfxExtEncoderROI_Entry,
    FIELD_T(mfxU32, Left    )
    FIELD_T(mfxU32, Top     )
    FIELD_T(mfxU32, Right   )
    FIELD_T(mfxU32, Bottom  )
    FIELD_T(mfxI16, Priority)
)

STRUCT(mfxExtEncoderROI,
    FIELD_S(mfxExtBuffer, Header)
    FIELD_T(mfxI16, NumROI)
    FIELD_S(mfxExtEncoderROI_Entry, ROI)
)

STRUCT(mfxExtVPPDeinterlacing,
    FIELD_S(mfxExtBuffer, Header)
    FIELD_T(mfxU16, Mode)
)

STRUCT(mfxExtCodingOptionVP8,
    FIELD_S(mfxExtBuffer, Header)
    FIELD_T(mfxU16, EnableAutoAltRef       )
    FIELD_T(mfxU16, TokenPartitions        )
    FIELD_T(mfxU16, EnableMultipleSegments )
)

STRUCT(mfxPluginUID,
    FIELD_T(mfxU8, Data)
)