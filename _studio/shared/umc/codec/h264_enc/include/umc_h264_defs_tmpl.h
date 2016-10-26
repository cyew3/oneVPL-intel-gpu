//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2012 Intel Corporation. All Rights Reserved.
//

#if PIXBITS == 8

#define PIXTYPE Ipp8u
#define COEFFSTYPE Ipp16s
#define H264ENC_MAKE_NAME(NAME) NAME##_8u16s

#elif PIXBITS == 16

#define PIXTYPE Ipp16u
#define COEFFSTYPE Ipp32s
#define H264ENC_MAKE_NAME(NAME) NAME##_16u32s

#else

void H264EncoderFakeFunction() { UNSUPPORTED_PIXBITS; }

#endif //PIXBITS

#define T_RLE_DataType H264ENC_MAKE_NAME(T_RLE_Data)
#define MBDissectionType H264ENC_MAKE_NAME(MBDissection)
#define H264CurrentMacroblockDescriptorType H264ENC_MAKE_NAME(H264CurrentMacroblockDescriptor)
#define ME_InfType H264ENC_MAKE_NAME(ME_Inf)

typedef struct H264ENC_MAKE_NAME(sT_RLE_Data)
{
    // Note: uNumCoeffs and uTotalZeros are not redundant because
    // this struct covers blocks with 4, 15 and 16 possible coded coeffs.
    Ipp8u   uTrailing_Ones;         // Up to 3 trailing ones are allowed (not in iLevels below)
    Ipp8u   uTrailing_One_Signs;    // Packed into up to 3 lsb, (1==neg)
    Ipp8u   uNumCoeffs;             // Total Number of non-zero coeffs (including Trailing Ones)
    Ipp8u   uTotalZeros;            // Total Number of zero coeffs
    COEFFSTYPE iLevels[64];         // Up to 64 Coded coeffs are possible, in reverse zig zag order
    Ipp8u   uRuns[64];              // Up to 64 Runs are recorded, including Trailing Ones, in rev zig zag order

} T_RLE_DataType;

struct __ALIGN8 CabacBlock4x4
{
    Ipp16s coeff[16];

    Ipp8u uBlockType;
    Ipp8u uNumSigCoeffs;
    Ipp8u uLastCoeff;
    Ipp8u uFirstCoeff;
    Ipp8u uFirstSignificant;
    Ipp8u uLastSignificant;
    Ipp8s CtxBlockCat;
    Ipp8s pad;
};

struct __ALIGN8 CabacBlock8x8
{
    Ipp16s coeff[64];

    Ipp8u uBlockType;
    Ipp8u uNumSigCoeffs;
    Ipp8u uLastCoeff;
    Ipp8u uFirstCoeff;
    Ipp8u uFirstSignificant;
    Ipp8u uLastSignificant;
    Ipp8s CtxBlockCat;
    Ipp8s pad;
};

struct T_Block_CABAC_Data
{
    union
    {
        CabacBlock4x4 lumaBlock4x4[16]; // 16 4x4 blocks
        CabacBlock8x8 lumaBlock8x8[4];  // or 4 8x8 blocks
    };

    CabacBlock4x4 chromaBlock[8];  // 8 4x4 chroma blocks for 4:2:0, 16 - 4:2:2, 32 - 4:4:4
    CabacBlock4x4 dcBlock[3];      // 3 4x4 blocks for DC coeffs (luma + 2 chroma)
};

typedef struct H264ENC_MAKE_NAME(sMBDissection)
{
    PIXTYPE* prediction;
    PIXTYPE* reconstruct;
    COEFFSTYPE* transform;
    COEFFSTYPE* recTransform;

} MBDissectionType;

typedef struct H264ENC_MAKE_NAME(sH264CurrentMacroblockDescriptor)
{
    Ipp32u   uMB, uMBpair;  //MacroBlock address
    Ipp16u   uMBx, uMBy;
    PIXTYPE* mbPtr;  //Pointer to macroblock data in original frame
    Ipp32s   mbPitchPixels;
    Ipp32s   lambda;
    Ipp32s   chroma_format_idc;  //Current chroma mode
    Ipp32s   lumaQP;
    Ipp32s   lumaQP51;
    Ipp32s   chromaQP;
    H264MacroblockLocalInfo  *LocalMacroblockInfo;
    H264MacroblockLocalInfo  *LocalMacroblockPairInfo;
    H264MacroblockGlobalInfo *GlobalMacroblockInfo;
    H264MacroblockGlobalInfo *GlobalMacroblockPairInfo;
    H264MacroblockCoeffsInfo *MacroblockCoeffsInfo;
    Ipp32u      m_uIntraCBP4x4;
    Ipp32s      m_iNumCoeffs4x4[16];
    Ipp32u      m_iLastCoeff4x4[16];
    Ipp32u      m_uIntraCBP8x8;
    Ipp32s      m_iNumCoeffs8x8[16];
    Ipp32u      m_iLastCoeff8x8[16];
    T_AIMode *intra_types;
    T_AIMode intra_types_save[16];
    MBDissectionType mb4x4;
    MBDissectionType mb8x8;
    MBDissectionType mb16x16;
    MBDissectionType mbInter;
    MBDissectionType mbChromaInter;
    MBDissectionType mbChromaIntra;

    MBDissection_8u16s mbBaseMode;
    MBDissection_8u16s mbChromaBaseMode;
    Ipp16s      *m_pResPredDiff;
    Ipp16s      *m_pResPredDiffC;
    Ipp16s      *m_pTCoeffsRef;
//    Ipp16s      *m_pSCoeffsRef;
//    Ipp16s      *m_pTCoeffsTmp;
    Ipp32s      m_iNumCoeffsBaseMode[16];
    Ipp32u      m_iLastCoeffBaseMode[16];
    Ipp32u      m_uIntraCBPBaseMode;

    H264MacroblockMVs *MVs[4];         //MV L0,L1, MVDeltas 0,1
    H264MacroblockRefIdxs *RefIdxs[2]; //RefIdx L0, L1
    H264MacroblockNeighboursInfo MacroblockNeighbours; //mb neighbouring info
    H264MacroblockNeighboursInfo IntraNeighbours;      //mb neighbours, usable for intra pred (if constrained_intra_pred)
    H264BlockNeighboursInfo BlockNeighbours; //block neighbouring info (if mbaff turned off remained static)
    T_Block_CABAC_Data *cabac_data;
} H264CurrentMacroblockDescriptorType;

typedef struct H264ENC_MAKE_NAME(sME_Inf)
{
    PIXTYPE *pCur;
    PIXTYPE *pCurU;
    PIXTYPE *pCurV;
    PIXTYPE *pRef;
#ifdef NO_PADDING
#if PIXBITS == 8
    IppVCInterpolateBlock_8u interpolateInfo;
    IppVCInterpolateBlock_8u interpolateInfoChroma;
#elif PIXBITS == 16
    IppVCInterpolateBlock_16u interpolateInfo;
    IppVCInterpolateBlock_16u interpolateInfoChroma;
#endif //PIXBITS
#endif // NO_PADDING
    PIXTYPE *pRefU;
    PIXTYPE *pRefV;
#ifdef FRAME_INTERPOLATION
    Ipp32s planeSize;
#endif
    Ipp32s pitchPixels;
    Ipp32s bit_depth_luma;
    Ipp32s bit_depth_chroma;
    Ipp16s chroma_mvy_offset;
    Ipp32s chroma_format_idc;
    IppiSize block;
    H264MotionVector candMV[ME_MAX_CANDIDATES];
    H264MotionVector predictedMV;
    H264MotionVector bestMV;
    H264MotionVector bestMV_Int;
    Ipp32s candNum;
    Ipp16s *pRDQM;
    Ipp32s xMin, xMax, yMin, yMax, rX, rY;
    Ipp32s flags;
    Ipp32s searchAlgo;
    Ipp32s threshold;
    Ipp32s bestSAD;

    // for SVC MV prediction
    Ipp32s useScPredMV;
    Ipp32s useScPredMVAlways;
    H264MotionVector scPredMV;
    H264MotionVector bestPredMV;

    // for SVC residual prediction
    Ipp32s residualPredictionFlag;
    Ipp16s *pResY;
    Ipp16s *pResU;
    Ipp16s *pResV;
//    Ipp32s resPitchPixels;

} ME_InfType;

void H264ENC_MAKE_NAME(InterpolateLuma)(
    const PIXTYPE* src,
    Ipp32s src_pitch,
    PIXTYPE* dst,
    Ipp32s dst_pitch,
    Ipp32s xh,
    Ipp32s yh,
    IppiSize sz,
    Ipp32s bit_depth,
    Ipp32s planeSize,
    const PIXTYPE **pI/* = NULL*/,
    Ipp32s *sI/* = NULL*/);

#undef T_RLE_DataType
#undef MBDissectionType
#undef H264CurrentMacroblockDescriptorType
#undef ME_InfType
#undef H264ENC_MAKE_NAME
#undef COEFFSTYPE
#undef PIXTYPE
