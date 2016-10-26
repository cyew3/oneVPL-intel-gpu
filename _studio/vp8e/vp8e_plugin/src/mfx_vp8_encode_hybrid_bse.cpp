//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2013-2016 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"
#if defined(MFX_ENABLE_VP8_VIDEO_ENCODE_HW)
#include "mfx_vp8_encode_hybrid_bse.h"
#include "mfx_vp8_encode_utils_hw.h"

namespace MFX_VP8ENC
{
    const I32 vp8_c0_table[256] =
    {
        2047, 2047, 1791, 1641, 1535, 1452, 1385, 1328, 1279, 1235, 1196, 1161, 1129, 1099, 1072, 1046,
        1023, 1000,  979,  959,  940,  922,  905,  889,  873,  858,  843,  829,  816,  803,  790,  778,
        767,  755,  744,  733,  723,  713,  703,  693,  684,  675,  666,  657,  649,  641,  633,  625,
        617,  609,  602,  594,  587,  580,  573,  567,  560,  553,  547,  541,  534,  528,  522,  516,
        511,  505,  499,  494,  488,  483,  477,  472,  467,  462,  457,  452,  447,  442,  437,  433,
        428,  424,  419,  415,  410,  406,  401,  397,  393,  389,  385,  381,  377,  373,  369,  365,
        361,  357,  353,  349,  346,  342,  338,  335,  331,  328,  324,  321,  317,  314,  311,  307,
        304,  301,  297,  294,  291,  288,  285,  281,  278,  275,  272,  269,  266,  263,  260,  257,
        255,  252,  249,  246,  243,  240,  238,  235,  232,  229,  227,  224,  221,  219,  216,  214,
        211,  208,  206,  203,  201,  198,  196,  194,  191,  189,  186,  184,  181,  179,  177,  174,
        172,  170,  168,  165,  163,  161,  159,  156,  154,  152,  150,  148,  145,  143,  141,  139,
        137,  135,  133,  131,  129,  127,  125,  123,  121,  119,  117,  115,  113,  111,  109,  107,
        105,  103,  101,   99,   97,   95,   93,   92,   90,   88,   86,   84,   82,   81,   79,   77,
        75,   73,   72,   70,   68,   66,   65,   63,   61,   60,   58,   56,   55,   53,   51,   50,
        48,   46,   45,   43,   41,   40,   38,   37,   35,   33,   32,   30,   29,   27,   25,   24,
        22,   21,   19,   18,   16,   15,   13,   12,   10,    9,    7,    6,    4,    3,    1,   1
    };

#define vp8_c0(p) (vp8_c0_table[p])
#define vp8_c1(p) (vp8_c0_table[255-p])
#define vp8_cb(p, b) ((b)?vp8_c1(p):vp8_c0(p))

    VPX_FORCEINLINE U8 vp8ProbFromCounters(U32 cnt0, U32 cnt1)
    {
        U32 d = cnt0 + cnt1;
        d = (d) ? (256 * cnt0 + (d>>1)) / d : 128;
        U8  p = (U8)((d<256) ? (d ? d : 1) : 255);
        return p;
    }

    VPX_FORCEINLINE U32 vp8GetBranchCost(Vp8Prob p, U32 c0, U32 c1)
    {
        return (c0*vp8_c0(p))+(c1*vp8_c1(p));
    }

    VPX_FORCEINLINE U32 vp8GetTreeTokenCost(I32 v, const I8 *tree, const U8 *prob, I8 start)
    {
        I8  i = start;
        U8  bit;
        U32 cost = 0;

        do {
            bit = v & 1; v>>=1;
            cost += vp8_cb(prob[i>>1],bit);
            i = tree[i+bit];
        } while(i>0);

        return cost;
    }

    VPX_FORCEINLINE void vp8GetTreeTokenCostArr(const U8* map, const I8 *tree, const U8 *prob, U32 num, U16* cost)
    {
        U32  i ;

        for (i = 0; i < num; i++)
        {
            cost[i] = (U16)vp8GetTreeTokenCost(map[i], tree, prob, 0);
        }

    }

    VPX_FORCEINLINE void vp8GetTreeCountsFromHist(const I8 *tree, const U8 *map, const U8 map_size, U32 *hist, U32 *cnt0, U32 *cnt1)
    {
        I32     i, j, v;
        U8      bit;
        U32    *cnt[2] = {cnt0, cnt1};

        for(j=0; j<map_size; j++) {
            v = map[j]; i = 0;
            do {
                bit = v & 1; v>>=1;
                cnt[bit][i>>1] += hist[j];
                i = tree[i+bit];
            } while(i>0);
        }
    }

    VPX_FORCEINLINE void vp8GetTreeProbFromHist(const I8 *tree, const U8 *map, U32 *hist, Vp8Prob *nProb, const U8 map_size, const U8 prob_size)
    {
        I32     i, j, v;
        U8      bit;
        U32     cnt[16][2]; /*Enough to store bit count accumulators for any tree */

        memset(cnt,0,sizeof(cnt));
        for(j=0; j<map_size; j++) {
            v = map[j]; i = 0;
            do {
                bit = v & 1; v>>=1;
                cnt[i>>1][bit] += hist[j];
                i = tree[i+bit];
            } while(i>0);
        }
        for(j=0; j<prob_size; j++)
            nProb[j] = vp8ProbFromCounters(cnt[j][0],cnt[j][1]);
    }

    Vp8CoreBSE::Vp8CoreBSE()
    {
        mfxI32  i;

        for(i=0; i<VP8_MAX_NUM_OF_PARTITIONS+1; i++) m_pPartitions[i] = NULL;
        for(i=0; i<VP8_MAX_NUM_OF_PARTITIONS; i++) { m_TokensB[i] = m_TokensE[i] = NULL; }

        m_encoded_frame_num = 0;
        m_Mbs1 = NULL;
        m_UVPreds = NULL;
    }

    Vp8CoreBSE::~Vp8CoreBSE()
    {
        if(m_pPartitions[0]) free(m_pPartitions[0]);
        if(m_pPartitions[1]) free(m_pPartitions[1]);

        if(m_TokensB[0]) free(m_TokensB[0]);
        if(m_Mbs1) free(m_Mbs1);
        if (m_UVPreds) free(m_UVPreds);
    }

    mfxStatus Vp8CoreBSE::InitVp8VideoParam(const mfxVideoParam &video)
    {
        mfxExtVP8CodingOption *pExtVP8Opt= GetExtBuffer(video);

        memset(&m_Params,0,sizeof(m_Params));

        m_Params.SrcWidth  = video.mfx.FrameInfo.CropW!=0 ? video.mfx.FrameInfo.CropW: video.mfx.FrameInfo.Width;
        m_Params.SrcHeight = video.mfx.FrameInfo.CropH!=0 ? video.mfx.FrameInfo.CropH: video.mfx.FrameInfo.Height;
        m_Params.fSizeInMBs = ((m_Params.SrcWidth+15)>>4) * ((m_Params.SrcHeight+15)>>4);
        m_Params.NumFramesToEncode = pExtVP8Opt->NumFramesForIVFHeader;
        m_Params.FrameRateNom = video.mfx.FrameInfo.FrameRateExtN;
        m_Params.FrameRateDeNom = video.mfx.FrameInfo.FrameRateExtD;
        m_Params.EnableSeg = (pExtVP8Opt->EnableMultipleSegments==MFX_CODINGOPTION_ON)?1:0;

        m_Params.EnableSkip = 1;
        m_Params.NumTokenPartitions = pExtVP8Opt->NumTokenPartitions; 

        for (mfxU8 i = 0; i < 4; i ++)
        {
            m_Params.KeyFrameQP[i] = video.mfx.QPI + pExtVP8Opt->SegmentQPDelta[i];
            m_Params.PFrameQP[i]   = video.mfx.QPP + pExtVP8Opt->SegmentQPDelta[i];
        }

        m_Params.LoopFilterType = pExtVP8Opt->LoopFilterType;
        m_Params.SharpnessLevel = pExtVP8Opt->SharpnessLevel;

        for (mfxU8 i = 0; i < 4; i ++)
        {
            m_Params.LoopFilterLevel[i] = pExtVP8Opt->LoopFilterLevel[i];
            m_Params.LoopFilterRefTypeDelta[i]  = pExtVP8Opt->LoopFilterRefTypeDelta[i];
            m_Params.LoopFilterMbModeDelta[i]   = pExtVP8Opt->LoopFilterMbModeDelta[i];
        }

        for (mfxU8 i = 0; i < 5; i ++)
        {
            m_Params.CoeffTypeQPDelta[i] = pExtVP8Opt->CoeffTypeQPDelta[i];
        }

        return MFX_ERR_NONE;
    }

    inline U16 vp8CNVToken(I16 v)
    {
        static const U16 cnvt[67*2] = {
            0x903f, 0x903d, 0x903b, 0x9039, 0x9037, 0x9035, 0x9033, 0x9031, 0x902f, 0x902d, 0x902b, 0x9029, 0x9027, 0x9025, 0x9023, 0x9021,
            0x901f, 0x901d, 0x901b, 0x9019, 0x9017, 0x9015, 0x9013, 0x9011, 0x900f, 0x900d, 0x900b, 0x9009, 0x9007, 0x9005, 0x9003, 0x9001,
            0x801f, 0x801d, 0x801b, 0x8019, 0x8017, 0x8015, 0x8013, 0x8011, 0x800f, 0x800d, 0x800b, 0x8009, 0x8007, 0x8005, 0x8003, 0x8001,
            0x700f, 0x700d, 0x700b, 0x7009, 0x7007, 0x7005, 0x7003, 0x7001, 0x6007, 0x6005, 0x6003, 0x6001, 0x5003, 0x5001, 0x4009, 0x3007,
            0x2005, 0x1003, 0x0000, 0x1002, 0x2004, 0x3006, 0x4008, 0x5000, 0x5002, 0x6000, 0x6002, 0x6004, 0x6006, 0x7000, 0x7002, 0x7004,
            0x7006, 0x7008, 0x700a, 0x700c, 0x700e, 0x8000, 0x8002, 0x8004, 0x8006, 0x8008, 0x800a, 0x800c, 0x800e, 0x8010, 0x8012, 0x8014,
            0x8016, 0x8018, 0x801a, 0x801c, 0x801e, 0x9000, 0x9002, 0x9004, 0x9006, 0x9008, 0x900a, 0x900c, 0x900e, 0x9010, 0x9012, 0x9014,
            0x9016, 0x9018, 0x901a, 0x901c, 0x901e, 0x9020, 0x9022, 0x9024, 0x9026, 0x9028, 0x902a, 0x902c, 0x902e, 0x9030, 0x9032, 0x9034,
            0x9036, 0x9038, 0x903a, 0x903c, 0x903e
        };

        if(v<=-67) {
            return (VP8_DCTCAT_6 << 12) | 1 | ((-v-67) << 1);
        } else if(v>=67) {
            return (VP8_DCTCAT_6 << 12) | ((v-67) << 1);
        } else {
            return cnvt[v + 66];
        }
    };

    mfxStatus Vp8CoreBSE::Init(const mfxVideoParam &video)
    {
        static const U8 p_shift[9] = {0, 0, 4, 2, 6, 1, 3, 5, 7 };
        I32         i;
        mfxStatus   st = MFX_ERR_NONE;

        st = InitVp8VideoParam(video);
        if(st != MFX_ERR_NONE) return st;

        m_Mbs1 = (TrMbCodeVp8*)malloc(sizeof(TrMbCodeVp8)*m_Params.fSizeInMBs);
        if(!m_Mbs1) return MFX_ERR_MEMORY_ALLOC;

        U32 wInMB = (m_Params.SrcWidth+15)>>4;
        U32 hInMB = (m_Params.SrcHeight+15)>>4;
        U32 mxPartH = (hInMB + 7) >> 3;
        U32 mxPartS = mxPartH*wInMB*2048;
        U32 mxToknS = mxPartH*wInMB*800;

        m_pPartitions[0] = (mfxU8*)malloc(2048 + m_Params.fSizeInMBs * 68); // 2048 (header) + 68 bytes per 16x16 block
        if(m_pPartitions[0]==NULL) return MFX_ERR_MEMORY_ALLOC;
        vp8EncodeInit(&(m_BoolEncStates[0]),(U8*)m_pPartitions[0]);

        m_pPartitions[1] = (mfxU8*)malloc(8*mxPartS); // 2048 bytes per 16x16 block for coefficients
        if(m_pPartitions[1]==NULL) return MFX_ERR_MEMORY_ALLOC;
        vp8EncodeInit(&(m_BoolEncStates[1]),(U8*)m_pPartitions[1]);

        m_TokensE[0] = m_TokensB[0] = (mfxU16*)malloc(8 * mxToknS * sizeof(mfxU16)); // 800 words per 16x16 block for coefficients
        if(m_TokensB[0]==NULL) return MFX_ERR_MEMORY_ALLOC;

        for(i=2;i<VP8_MAX_NUM_OF_PARTITIONS+1;i++) {
            m_pPartitions[i] = m_pPartitions[1] + p_shift[i]*mxPartS;
            vp8EncodeInit(&(m_BoolEncStates[i]),(U8*)m_pPartitions[i]);
        }
        for(i=1;i<VP8_MAX_NUM_OF_PARTITIONS;i++) {
            m_TokensE[i] = m_TokensB[i] = m_TokensB[0] + p_shift[i+1]*mxToknS;
        }        

        m_externalMb1.RefFrameMode = VP8_INTRA_FRAME;
        m_externalMb1.Y4x4Modes0 = m_externalMb1.Y4x4Modes1 = m_externalMb1.Y4x4Modes2 = m_externalMb1.Y4x4Modes3 = 0;
        m_externalMb1.nonZeroCoeff = 0;
        m_externalMb1.Y2AboveNotZero = m_externalMb1.Y2LeftNotZero = 0;

        m_UVPreds = (MbUVPred*)malloc(sizeof(MbUVPred)*m_Params.fSizeInMBs);
        if(!m_UVPreds) return MFX_ERR_MEMORY_ALLOC;

        return MFX_ERR_NONE;
    }

    // [SE] Reset supports BSP mode only (not CPU PAK)
    mfxStatus Vp8CoreBSE::Reset(const mfxVideoParam &video)
    {
        I32         i;
        mfxStatus   st = MFX_ERR_NONE;

        st = InitVp8VideoParam(video);
        if(st != MFX_ERR_NONE) return st;

        U32 wInMB = (m_Params.SrcWidth+15)>>4;
        U32 hInMB = (m_Params.SrcHeight+15)>>4;
        U32 mxPartH = (hInMB + 7) >> 3;
        U32 mxPartS = mxPartH*wInMB*2048;
        U32 mxToknS = mxPartH*wInMB*800;

        m_externalMb1.RefFrameMode = VP8_INTRA_FRAME;
        m_externalMb1.Y4x4Modes0 = m_externalMb1.Y4x4Modes1 = m_externalMb1.Y4x4Modes2 = m_externalMb1.Y4x4Modes3 = 0;
        m_externalMb1.nonZeroCoeff = 0;
        m_externalMb1.Y2AboveNotZero = m_externalMb1.Y2LeftNotZero = 0;

        return MFX_ERR_NONE;
    }

    mfxStatus Vp8CoreBSE::SetNextFrame(mfxFrameSurface1 * /*pSurface*/, bool /*bExternal*/, const sFrameParams &frameParams, mfxU32 frameNum)
    {
        mfxStatus  sts = MFX_ERR_NONE;

        m_encoded_frame_num = frameNum;

        for(int i=0;i<VP8_MAX_NUM_OF_PARTITIONS+1;i++) 
        {
            vp8EncodeInit(&(m_BoolEncStates[i]), m_pPartitions[i]);
        }

        DetermineFrameType(frameParams);
        PrepareFrameControls(frameParams);

        // [SE] Use LoopFilterLevel provided by application
        /*I16 dc_idx = m_ctrl.qIndex + m_ctrl.qDelta[VP8_Y_DC-1];
        dc_idx = (dc_idx<0)?0:(dc_idx>VP8_MAX_QP)?VP8_MAX_QP:dc_idx;
        m_ctrl.LoopFilterLevel = ( dc_idx ) / 2;*/

        return sts;
    }
    void Vp8CoreBSE::DetermineFrameType(const sFrameParams &frameParams)
    {
        m_ctrl.FrameType = (frameParams.bIntra) ? 0 : 1;

        if(m_ctrl.FrameType) 
        {
            m_ctrl.GoldenCopyFlag  = frameParams.copyToGoldRef;
            m_ctrl.AltRefCopyFlag  = frameParams.copyToAltRef;
            m_ctrl.LastFrameUpdate = frameParams.bLastRef;
            m_ctrl.SignBiasGolden  = 0;
            m_ctrl.SignBiasAltRef  = 0;

            m_fRefBias[0] = 0;
            m_fRefBias[1] = (U8)(m_ctrl.SignBiasAltRef ^ m_ctrl.SignBiasGolden);
            m_fRefBias[2] = (U8)(m_ctrl.SignBiasAltRef);
            m_fRefBias[3] = (U8)(m_ctrl.SignBiasGolden);
        }    
    }

    void Vp8CoreBSE::PrepareFrameControls(const sFrameParams &frameParams)
    {
        I32 i, j;

        m_ctrl.Version          = m_Params.Version;
        m_ctrl.ShowFrame        = 1;
        m_ctrl.HorSizeCode      = 0;
        m_ctrl.VerSizeCode      = 0;
        m_ctrl.PartitionNumL2   = m_Params.NumTokenPartitions;
        m_ctrl.ColorSpace       = 0;
        m_ctrl.ClampType        = 0;
        m_ctrl.SegOn            = m_Params.EnableSeg;

        m_ctrl.qIndex = (U8)(m_ctrl.FrameType ? m_Params.PFrameQP[0] : m_Params.KeyFrameQP[0]);
        m_ctrl.qDelta[0] = (I8)(m_Params.CoeffTypeQPDelta[0]);
        m_ctrl.qDelta[1] = (I8)(m_Params.CoeffTypeQPDelta[1]);
        m_ctrl.qDelta[2] = (I8)(m_Params.CoeffTypeQPDelta[2]);
        m_ctrl.qDelta[3] = (I8)(m_Params.CoeffTypeQPDelta[3]);
        m_ctrl.qDelta[4] = (I8)(m_Params.CoeffTypeQPDelta[4]);

        if(!m_ctrl.FrameType) {
            for(j=0;j<VP8_NUM_OF_MB_FEATURES;j++)
                for(i=0;i<VP8_MAX_NUM_OF_SEGMENTS;i++)
                    m_ctrl.segFeatureData[j][i] = 0;
            m_ctrl.SegFeatMode = 0;
            for(i=0;i<VP8_NUM_OF_REF_FRAMES;i++)
                m_ctrl.refLFDeltas[i] = 0;
            for(i=0;i<VP8_NUM_OF_MODE_LF_DELTAS;i++)
                m_ctrl.modeLFDeltas[i] = 0;
        }

        if(m_ctrl.SegOn) {
            U32 *QP = m_ctrl.FrameType ? m_Params.PFrameQP : m_Params.KeyFrameQP;
            m_ctrl.SegFeatUpdate = 0;
            for(i=1;i<VP8_MAX_NUM_OF_SEGMENTS;i++)
                if((QP[i]!=QP[0]) || (frameParams.LFLevel[i]!=frameParams.LFLevel[0])) {
                    m_ctrl.SegFeatUpdate = 1; break;
                }
                if(m_ctrl.SegFeatUpdate) {
                    m_ctrl.SegFeatMode = 0;
                    for(i=0;i<VP8_MAX_NUM_OF_SEGMENTS;i++)
                        m_ctrl.segFeatureData[0][i] = (I8)(QP[i] - QP[0]);
                    for(i=0;i<VP8_MAX_NUM_OF_SEGMENTS;i++)
                        m_ctrl.segFeatureData[1][i] = (I8)(frameParams.LFLevel[i] - frameParams.LFLevel[0]);
                }
                m_ctrl.MBSegUpdate  = 1;
        }

        m_ctrl.LoopFilterType   = frameParams.LFType;
        m_ctrl.LoopFilterLevel  = frameParams.LFLevel[0];
        m_ctrl.SharpnessLevel   = frameParams.Sharpness;

        m_ctrl.LoopFilterAdjOn = ((m_Params.LoopFilterRefTypeDelta[0]|m_Params.LoopFilterRefTypeDelta[1]|m_Params.LoopFilterRefTypeDelta[2]|m_Params.LoopFilterRefTypeDelta[3]|
            m_Params.LoopFilterMbModeDelta[0]|m_Params.LoopFilterMbModeDelta[1]|m_Params.LoopFilterMbModeDelta[2]|m_Params.LoopFilterMbModeDelta[3]) != 0);

        if(m_ctrl.LoopFilterAdjOn)
        {
            for(i=0;i<VP8_NUM_OF_REF_FRAMES;i++) {
                m_ctrl.refLFDeltas_pf[i] = m_ctrl.refLFDeltas[i];
                m_ctrl.refLFDeltas[i] = (I8)(m_Params.LoopFilterRefTypeDelta[i]);
            }
            for(i=0;i<VP8_NUM_OF_MODE_LF_DELTAS;i++) {
                m_ctrl.modeLFDeltas_pf[i] = m_ctrl.modeLFDeltas[i];
                m_ctrl.modeLFDeltas[i] = (I8)(m_Params.LoopFilterMbModeDelta[i]);
            }
        }

        m_ctrl.RefreshEntropyP  = 1;
        m_ctrl.MbNoCoeffSkip = m_Params.EnableSkip;
    }

#define CHECK(cond) if (!(cond)) return MFX_ERR_UNDEFINED_BEHAVIOR;
#define CHECK_STS(sts) if ((sts) != UMC_OK) return MFX_ERR_UNDEFINED_BEHAVIOR;

    inline void AddSeqHeader(unsigned int width,
        unsigned int   height,
        unsigned int   FrameRateN,
        unsigned int   FrameRateD,
        unsigned int   numFramesInFile,
        unsigned char *pBitstream)
    {
        U32   ivf_file_header[8]  = {0x46494B44, 0x00200000, 0x30385056, width + (height << 16), FrameRateN, FrameRateD, numFramesInFile, 0x00000000};

        memcpy(pBitstream, ivf_file_header, sizeof (ivf_file_header));
    }

    inline void AddPictureHeader(unsigned char *pBitstream)
    {
        U32 ivf_frame_header[3] = {0x00000000, 0x00000000, 0x00000000};

        memcpy(pBitstream, ivf_frame_header, sizeof (ivf_frame_header));
    }

    inline void UpdatePictureHeader(unsigned int frameLen,
        unsigned int frameNum,
        unsigned char *pPictureHeader)
    {
        U32 ivf_frame_header[3] = {frameLen, frameNum << 1, 0x00000000};

        memcpy(pPictureHeader, ivf_frame_header, sizeof (ivf_frame_header));
    }

    mfxStatus Vp8CoreBSE::RunBSP(bool bIVFHeaders, bool bSeqHeader, mfxBitstream * pBitstream, TaskHybridDDI *pTask, MBDATA_LAYOUT const & layout, mfxCoreInterface * pCore)
    {
        mfxStatus   sts = MFX_ERR_NONE;
        mfxU8      *pPictureHeader = 0;
        mfxU32      IVFHeaderSize = 0;
        if (bIVFHeaders)
            IVFHeaderSize = bSeqHeader ? 32 + 12 : 12;

        CHECK(pBitstream->DataLength + pBitstream->DataOffset + IVFHeaderSize  < pBitstream->MaxLength);

        m_pBitstream = pBitstream;
        if (bIVFHeaders)
        {
            pPictureHeader = pBitstream->Data + pBitstream->DataLength + pBitstream->DataOffset;
            if (bSeqHeader) {
                AddSeqHeader(m_Params.SrcWidth , m_Params.SrcHeight, m_Params.FrameRateNom, m_Params.FrameRateDeNom, m_Params.NumFramesToEncode, pPictureHeader);
                pBitstream->DataLength += 32;
            }
            pPictureHeader = pBitstream->Data + pBitstream->DataLength + pBitstream->DataOffset;
            AddPictureHeader(pPictureHeader);
            pBitstream->DataLength += 12;
        }

        memset(&m_cnt1, 0, sizeof(m_cnt1));

        TokenizeAndCnt(pTask, layout, pCore);
        UpdateEntropyModel();
        CalculateCosts();
        EncodeTokenPartitions();
        EncodeFirstPartition();
        sts = OutputBitstream();

        if(sts == MFX_ERR_NONE && bIVFHeaders)
            UpdatePictureHeader(pBitstream->DataLength-IVFHeaderSize, m_encoded_frame_num, pPictureHeader);

        return sts;   
    }

    void Vp8CoreBSE::EncodeFirstPartition(void)
    {
        I32     i, j, k, l;

        /* Encode the frame header */
        if(!m_ctrl.FrameType)
        {
            vp8EncodeBitEP(m_BoolEncStates, m_ctrl.ColorSpace);         /* Color space type */
            vp8EncodeBitEP(m_BoolEncStates, m_ctrl.ClampType);          /* Clamping type */
        }
        vp8EncodeBitEP(m_BoolEncStates, m_ctrl.SegOn);                  /* Segmentation enabled */
        if(m_ctrl.SegOn)
        {
            vp8EncodeBitEP(m_BoolEncStates, m_ctrl.MBSegUpdate);
            vp8EncodeBitEP(m_BoolEncStates, m_ctrl.SegFeatUpdate);
            if(m_ctrl.SegFeatUpdate)
            {
                vp8EncodeBitEP(m_BoolEncStates, m_ctrl.SegFeatMode);    /* Should be selected automaticaly */
                if(m_ctrl.SegFeatMode) {
                    /* In absolute mode absence of feature means that the feature equalts to zero */
                    for(i=0;i<VP8_MAX_NUM_OF_SEGMENTS;i++)
                        vp8EncodeValueIfNE(m_BoolEncStates, m_ctrl.segFeatureData[0][i], 0, 7, true, 0x80);
                    for(i=0;i<VP8_MAX_NUM_OF_SEGMENTS;i++)
                        vp8EncodeValueIfNE(m_BoolEncStates, m_ctrl.segFeatureData[1][i], 0, 6, true, 0x80);
                } else {
                    for(i=0;i<VP8_MAX_NUM_OF_SEGMENTS;i++)
                        vp8EncodeDeltaIfNE(m_BoolEncStates, m_ctrl.segFeatureData[0][i], 0, 7);
                    for(i=0;i<VP8_MAX_NUM_OF_SEGMENTS;i++)
                        vp8EncodeDeltaIfNE(m_BoolEncStates, m_ctrl.segFeatureData[1][i], 0, 6);
                }
            }
            if(m_ctrl.MBSegUpdate)
                for(i=0;i<VP8_NUM_OF_SEGMENT_TREE_PROBS;i++)
                    vp8EncodeValueIfNE(m_BoolEncStates, m_ctrl.segTreeProbs[i], segTreeProbsC[i], 8, false, 0x80);
        }
        vp8EncodeBitEP(m_BoolEncStates, m_ctrl.LoopFilterType);
        vp8EncodeValue(m_BoolEncStates, m_ctrl.LoopFilterLevel, 6);
        vp8EncodeValue(m_BoolEncStates, m_ctrl.SharpnessLevel, 3);
        vp8EncodeBitEP(m_BoolEncStates, m_ctrl.LoopFilterAdjOn);
        if(m_ctrl.LoopFilterAdjOn) /* Loop filter deltas are set to previous value if no update */
        {
            vp8EncodeBitEP(m_BoolEncStates, m_ctrl.LoopFilterAdjOn);
            for(i=0;i<VP8_NUM_OF_REF_FRAMES;i++)
                vp8EncodeValueIfNE(m_BoolEncStates, m_ctrl.refLFDeltas[i], m_ctrl.refLFDeltas_pf[i], 6, true, 0x80);
            for(i=0;i<VP8_NUM_OF_MODE_LF_DELTAS;i++)
                vp8EncodeValueIfNE(m_BoolEncStates, m_ctrl.modeLFDeltas[i], m_ctrl.modeLFDeltas_pf[i], 6, true, 0x80);
        }
        vp8EncodeValue(m_BoolEncStates, m_ctrl.PartitionNumL2, 2);
        vp8EncodeValue(m_BoolEncStates, m_ctrl.qIndex, 7);
        vp8EncodeDeltaIfNE(m_BoolEncStates, m_ctrl.qDelta[VP8_Y_DC-1],  0, 4);
        vp8EncodeDeltaIfNE(m_BoolEncStates, m_ctrl.qDelta[VP8_Y2_DC-1], 0, 4);
        vp8EncodeDeltaIfNE(m_BoolEncStates, m_ctrl.qDelta[VP8_Y2_AC-1], 0, 4);
        vp8EncodeDeltaIfNE(m_BoolEncStates, m_ctrl.qDelta[VP8_UV_DC-1], 0, 4);
        vp8EncodeDeltaIfNE(m_BoolEncStates, m_ctrl.qDelta[VP8_UV_AC-1], 0, 4);
        if(!m_ctrl.FrameType) {
            vp8EncodeBitEP(m_BoolEncStates, m_ctrl.RefreshEntropyP);
        } else {
            vp8EncodeBitEP(m_BoolEncStates, (m_ctrl.GoldenCopyFlag == 3));
            vp8EncodeBitEP(m_BoolEncStates, (m_ctrl.AltRefCopyFlag == 3));
            if(m_ctrl.GoldenCopyFlag!=3)
                vp8EncodeValue(m_BoolEncStates, m_ctrl.GoldenCopyFlag, 2);
            if(m_ctrl.AltRefCopyFlag!=3)
                vp8EncodeValue(m_BoolEncStates, m_ctrl.AltRefCopyFlag, 2);
            vp8EncodeBitEP(m_BoolEncStates, m_ctrl.SignBiasGolden);
            vp8EncodeBitEP(m_BoolEncStates, m_ctrl.SignBiasAltRef);
            vp8EncodeBitEP(m_BoolEncStates, m_ctrl.RefreshEntropyP);
            vp8EncodeBitEP(m_BoolEncStates, m_ctrl.LastFrameUpdate);
        }

        for (i = 0; i < VP8_NUM_COEFF_PLANES; i++)
            for (j = 0; j < VP8_NUM_COEFF_BANDS; j++)
                for (k = 0; k < VP8_NUM_LOCAL_COMPLEXITIES; k++)
                    for (l = 0; l < VP8_NUM_COEFF_NODES; l++)
                        vp8EncodeValueIfNE(m_BoolEncStates, m_ctrl.coeff_probs[i][j][k][l], m_ctrl.coeff_probs_pf[i][j][k][l], 8, false, vp8_coeff_update_probs[i][j][k][l]);

        vp8EncodeBitEP(m_BoolEncStates, m_ctrl.MbNoCoeffSkip);
        if(m_ctrl.MbNoCoeffSkip)
            vp8EncodeValue(m_BoolEncStates, m_ctrl.prSkipFalse, 8);

        if(m_ctrl.FrameType) {
            vp8EncodeValue(m_BoolEncStates, m_ctrl.prIntra, 8);
            vp8EncodeValue(m_BoolEncStates, m_ctrl.prLast, 8);
            vp8EncodeValue(m_BoolEncStates, m_ctrl.prGolden, 8);
            vp8EncodeBitEP(m_BoolEncStates, m_ctrl.Intra16PUpdate);
            if(m_ctrl.Intra16PUpdate) {
                for(i=0;i<VP8_NUM_MB_MODES_Y-1;i++)
                    vp8EncodeValue(m_BoolEncStates, m_ctrl.mbModeProbY[i], 8);
            }
            vp8EncodeBitEP(m_BoolEncStates, m_ctrl.IntraUVPUpdate);
            if(m_ctrl.IntraUVPUpdate) {
                for(i=0;i<VP8_NUM_MB_MODES_UV-1;i++)
                    vp8EncodeValue(m_BoolEncStates, m_ctrl.mbModeProbUV[i], 8);
            }
            for(i=0;i<2;i++)
                for(j=0;j<VP8_NUM_MV_PROBS;j++)
                    if(m_ctrl.mvContexts[i][j] != m_ctrl.mvContexts_pf[i][j])
                        vp8EncodeValueIfNE(m_BoolEncStates, m_ctrl.mvContexts[i][j]>>1, -1, 7, false, vp8_mv_update_probs[i][j]); /* possible last bit difference */
                    else
                        vp8EncodeValueIfNE(m_BoolEncStates, m_ctrl.mvContexts[i][j]>>1, m_ctrl.mvContexts_pf[i][j]>>1, 7, false, vp8_mv_update_probs[i][j]);
            EncodeFrameMBs();
        } else {
            EncodeKeyFrameMB();
        }

        vp8EncodeFlush(&(m_BoolEncStates[0]));
    }

    void Vp8CoreBSE::EncodeTokenPartitions(void)
    {
        I32     i, maxPartId   = (1<<m_ctrl.PartitionNumL2);

        for(i=0; i<maxPartId; i++) {
            vp8BoolEncState *e = &(m_BoolEncStates[i+1]);
            U16             *t = m_TokensB[i], *tend = m_TokensE[i];
            U16             v, ctx;
            U8              lZ, *prob;

            while(t < tend) {
                ctx  = *t++;
                prob = ((U8*)m_ctrl.coeff_probs) + (ctx & 0x7F) * 11;
                
                if(ctx >> 8) {
                    vp8EncodeEobToken(e, prob);
                } else {
                    v  = *t++;
                    lZ = (U8)(ctx >> 7);
                    vp8EncodeToken(e, v, lZ, prob);
                }
            }
            vp8EncodeFlush(e);
        }
    }

    enum { CNT_ZERO, CNT_NEAREST, CNT_NEAR, CNT_SPLITMV };

    VPX_FORCEINLINE void Vp8GetNearMVs(TrMbCodeVp8 *pMb, I32 fWidthInMBs, MvTypeVp8 *nearMVs, I32 *mvCnt, U8 *refBias)
    {
        TrMbCodeVp8 *aMb, *lMb, *alMb;
        MvTypeVp8    tMv;
        I32          idx;

        mvCnt[0] = mvCnt[1] = mvCnt[2] = mvCnt[3] = 0;
        nearMVs[0].s32 = nearMVs[1].s32 = nearMVs[2].s32 = nearMVs[3].s32 = 0;

        if(!pMb->MbYcnt) {
            if(pMb->MbXcnt) {
                /* Only left neighbour present */
                lMb = pMb-1;
                if (lMb->RefFrameMode != VP8_INTRA_FRAME)
                {
                    tMv = (lMb->MbMode==VP8_MV_SPLIT) ? lMb->NewMV4x4[15] : lMb->NewMV16;
                    if (tMv.s32)
                    {
                        if(refBias[lMb->RefFrameMode^pMb->RefFrameMode]) {
                            nearMVs[CNT_NEAREST].MV.mvx = -tMv.MV.mvx;
                            nearMVs[CNT_NEAREST].MV.mvy = -tMv.MV.mvy;
                        } else {
                            nearMVs[CNT_NEAREST].s32 = tMv.s32;
                        }
                        nearMVs[CNT_ZERO].s32 = nearMVs[CNT_NEAREST].s32;
                        mvCnt[CNT_NEAREST] = 2;
                    }
                    else {
                        mvCnt[CNT_ZERO] = 2;
                    }
                    mvCnt[CNT_SPLITMV] = (lMb->MbMode == VP8_MV_SPLIT) ? 2 : 0;
                }
            }
        } else {
            if(!pMb->MbXcnt) {
                /* Only top neighbour present */
                aMb = pMb - fWidthInMBs;
                if (aMb->RefFrameMode != VP8_INTRA_FRAME)
                {
                    tMv = (aMb->MbMode==VP8_MV_SPLIT) ? aMb->NewMV4x4[15] : aMb->NewMV16;
                    if (tMv.s32)
                    {
                        if(refBias[aMb->RefFrameMode^pMb->RefFrameMode]) {
                            nearMVs[CNT_NEAREST].MV.mvx = -tMv.MV.mvx;
                            nearMVs[CNT_NEAREST].MV.mvy = -tMv.MV.mvy;
                        } else {
                            nearMVs[CNT_NEAREST].s32 = tMv.s32;
                        }
                        nearMVs[CNT_ZERO].s32 = nearMVs[CNT_NEAREST].s32;
                        mvCnt[CNT_NEAREST] = 2;
                    }
                    else {
                        mvCnt[CNT_ZERO] = 2;
                    }
                    mvCnt[CNT_SPLITMV] = (aMb->MbMode == VP8_MV_SPLIT) ? 2 : 0;
                }
            } else {
                /* All three neighbours present */
                idx  = 0;
                lMb  = pMb - 1;
                aMb  = pMb - fWidthInMBs;
                alMb = aMb - 1;

                if (aMb->RefFrameMode != VP8_INTRA_FRAME)
                {
                    tMv = (aMb->MbMode==VP8_MV_SPLIT) ? aMb->NewMV4x4[15] : aMb->NewMV16;
                    if (tMv.s32)
                    {
                        if(refBias[aMb->RefFrameMode^pMb->RefFrameMode]) {
                            nearMVs[CNT_NEAREST].MV.mvx = -tMv.MV.mvx;
                            nearMVs[CNT_NEAREST].MV.mvy = -tMv.MV.mvy;
                        } else {
                            nearMVs[CNT_NEAREST].s32 = tMv.s32;
                        }

                        mvCnt[CNT_NEAREST] = 2;
                        idx = 1;
                    }
                    else {
                        mvCnt[CNT_ZERO] = 2;
                    }
                }

                if (lMb->RefFrameMode != VP8_INTRA_FRAME)
                {
                    tMv = (lMb->MbMode==VP8_MV_SPLIT) ? lMb->NewMV4x4[15] : lMb->NewMV16;
                    if (tMv.s32)
                    {
                        if(refBias[lMb->RefFrameMode^pMb->RefFrameMode]) {
                            tMv.MV.mvx = -tMv.MV.mvx;
                            tMv.MV.mvy = -tMv.MV.mvy;
                        }
                        if(tMv.s32 != nearMVs[CNT_NEAREST].s32) 
                            nearMVs[++idx].s32 = tMv.s32;
                        mvCnt[idx] += 2;
                    }
                    else {
                        mvCnt[CNT_ZERO] += 2;
                    }
                }

                if(alMb->RefFrameMode != VP8_INTRA_FRAME)
                {
                    tMv = (alMb->MbMode==VP8_MV_SPLIT) ? alMb->NewMV4x4[15] : alMb->NewMV16;
                    if (tMv.s32)
                    {
                        if(refBias[alMb->RefFrameMode^pMb->RefFrameMode]) {
                            tMv.MV.mvx = -tMv.MV.mvx;
                            tMv.MV.mvy = -tMv.MV.mvy;
                        }
                        if(tMv.s32 != nearMVs[idx].s32) 
                            nearMVs[++idx].s32 = tMv.s32;
                        mvCnt[idx]++;
                    }
                    else {
                        mvCnt[CNT_ZERO]++;
                    }
                }

                if(mvCnt[CNT_SPLITMV] && (nearMVs[CNT_NEAREST].s32 == nearMVs[CNT_SPLITMV].s32))
                    mvCnt[CNT_NEAREST]++;

                if(mvCnt[CNT_NEAR] > mvCnt[CNT_NEAREST])
                {
                    I32 tmp;

                    tmp = nearMVs[CNT_NEAR].s32;
                    nearMVs[CNT_NEAR].s32    = nearMVs[CNT_NEAREST].s32;
                    nearMVs[CNT_NEAREST].s32 = tmp;

                    tmp = mvCnt[CNT_NEAR];
                    mvCnt[CNT_NEAR]    = mvCnt[CNT_NEAREST];
                    mvCnt[CNT_NEAREST] = tmp;
                }

                if (mvCnt[CNT_NEAREST] >= mvCnt[CNT_ZERO])
                    nearMVs[CNT_ZERO].s32 = nearMVs[CNT_NEAREST].s32;

                mvCnt[CNT_SPLITMV] = (((lMb->RefFrameMode != VP8_INTRA_FRAME)&&(lMb->MbMode == VP8_MV_SPLIT)) ? 2 : 0) +
                    (((aMb->RefFrameMode != VP8_INTRA_FRAME)&&(aMb->MbMode == VP8_MV_SPLIT)) ? 2 : 0) +
                    (((alMb->RefFrameMode != VP8_INTRA_FRAME)&&(alMb->MbMode == VP8_MV_SPLIT)) ? 1 : 0);
            }
        }
    }

    void Vp8CoreBSE::EncodeFrameMBs(void)
    {
        U32         i, j, mRowCnt;
        TrMbCodeVp8 *cMb,*aMb,*lMb;
        MvTypeVp8   nearMvs[4];
        I32         nearCnt[4], mPitch;
        Vp8Prob     mvModeProbs[4];
        I16         limT, limL, limB, limR;

        mPitch  = m_Mbs1[m_Params.fSizeInMBs-1].MbXcnt + 1;
        mRowCnt = m_Mbs1[m_Params.fSizeInMBs-1].MbYcnt + 1;
        cMb     = m_Mbs1;
        for(i=0; i<m_Params.fSizeInMBs; i++)
        {
            lMb = (cMb->MbXcnt)? cMb - 1 : &m_externalMb1;
            aMb = (cMb->MbYcnt)? cMb - mPitch : &m_externalMb1;
            if(m_ctrl.SegOn && m_ctrl.MBSegUpdate)
            {
                switch(cMb->SegmentId){
                case 1:
                    vp8EncodeBit(m_BoolEncStates, 0, m_ctrl.segTreeProbs[0]);
                    vp8EncodeBit(m_BoolEncStates, 1, m_ctrl.segTreeProbs[1]);
                    break;
                case 2:
                    vp8EncodeBit(m_BoolEncStates, 1, m_ctrl.segTreeProbs[0]);
                    vp8EncodeBit(m_BoolEncStates, 0, m_ctrl.segTreeProbs[2]);
                    break;
                case 3:
                    vp8EncodeBit(m_BoolEncStates, 1, m_ctrl.segTreeProbs[0]);
                    vp8EncodeBit(m_BoolEncStates, 1, m_ctrl.segTreeProbs[2]);
                    break;
                case 0:
                default:
                    vp8EncodeBit(m_BoolEncStates, 0, m_ctrl.segTreeProbs[0]);
                    vp8EncodeBit(m_BoolEncStates, 0, m_ctrl.segTreeProbs[1]);
                }
            }
            if(m_ctrl.MbNoCoeffSkip)
                vp8EncodeBit(m_BoolEncStates, cMb->CoeffSkip, m_ctrl.prSkipFalse);
            if(cMb->RefFrameMode==VP8_INTRA_FRAME) {
                vp8EncodeBit(m_BoolEncStates, 0, m_ctrl.prIntra);
                vp8EncodeTreeToken(m_BoolEncStates, vp8_mb_mode_y_map[cMb->MbMode], vp8_mb_mode_y_tree, m_ctrl.mbModeProbY, 0);
                if(cMb->MbMode==VP8_MB_B_PRED) {
                    for(j=0;j<16;j+=4) {
                        vp8EncodeTreeToken(m_BoolEncStates, vp8_block_mode_map[(cMb->Y4x4Modes0>>j) & 0x0F], vp8_block_mode_tree, vp8_block_mode_probs, 0);
                    }
                    for(j=0;j<16;j+=4) {
                        vp8EncodeTreeToken(m_BoolEncStates, vp8_block_mode_map[(cMb->Y4x4Modes1>>j) & 0x0F], vp8_block_mode_tree, vp8_block_mode_probs, 0);
                    }
                    for(j=0;j<16;j+=4) {
                        vp8EncodeTreeToken(m_BoolEncStates, vp8_block_mode_map[(cMb->Y4x4Modes2>>j) & 0x0F], vp8_block_mode_tree, vp8_block_mode_probs, 0);
                    }
                    for(j=0;j<16;j+=4) {
                        vp8EncodeTreeToken(m_BoolEncStates, vp8_block_mode_map[(cMb->Y4x4Modes3>>j) & 0x0F], vp8_block_mode_tree, vp8_block_mode_probs, 0);
                    }
                }
                vp8EncodeTreeToken(m_BoolEncStates, vp8_mb_mode_uv_map[cMb->MbSubmode], vp8_mb_mode_uv_tree, m_ctrl.mbModeProbUV, 0);
            } else {
                vp8EncodeBit(m_BoolEncStates, 1, m_ctrl.prIntra);
                if(cMb->RefFrameMode==VP8_LAST_FRAME) {
                    vp8EncodeBit(m_BoolEncStates, 0, m_ctrl.prLast);
                } else {
                    vp8EncodeBit(m_BoolEncStates, 1, m_ctrl.prLast);
                    vp8EncodeBit(m_BoolEncStates, (cMb->RefFrameMode==VP8_GOLDEN_FRAME) ? 0 : 1, m_ctrl.prGolden);
                }
                limT = -(((I16)cMb->MbYcnt + 1) << 7);
                limL = -(((I16)cMb->MbXcnt + 1) << 7);
                limB = (I16)(mRowCnt - cMb->MbYcnt) << 7;
                limR = (I16)(mPitch - cMb->MbXcnt) << 7;
                Vp8GetNearMVs(cMb, mPitch, nearMvs, nearCnt, m_fRefBias);
                clampMvComponent(nearMvs[0], limT, limB, limL, limR);
                clampMvComponent(nearMvs[1], limT, limB, limL, limR);
                clampMvComponent(nearMvs[2], limT, limB, limL, limR);
                for (j = 0; j < 4; j++)
                    mvModeProbs[j] = vp8_mvmode_contexts[nearCnt[j]][j];
                vp8EncodeTreeToken(m_BoolEncStates, vp8_mvmode_map[cMb->MbMode],vp8_mvmode_tree, mvModeProbs, 0);
                switch(cMb->MbMode) {
                case VP8_MV_NEW:
                    vp8EncodeMvComponent(m_BoolEncStates, (cMb->NewMV16.MV.mvy-nearMvs[0].MV.mvy)>>1, m_ctrl.mvContexts[0]);
                    vp8EncodeMvComponent(m_BoolEncStates, (cMb->NewMV16.MV.mvx-nearMvs[0].MV.mvx)>>1, m_ctrl.mvContexts[1]);
                    break;
                case VP8_MV_SPLIT:
                    {
                        I8          j = 0, bl = 0;
                        U8          offset, num_blocks, smode, cntx;
                        MvTypeVp8   mvl, mvu;

                        vp8EncodeTreeToken(m_BoolEncStates, vp8_mv_partitions_map[cMb->MbSubmode],vp8_mv_partitions_tree, vp8_mv_partitions_probs, 0);
                        switch(cMb->MbSubmode) {
                        case VP8_MV_TOP_BOTTOM:
                            num_blocks = 2; offset = 8; break;
                        case VP8_MV_LEFT_RIGHT:
                            num_blocks = 2; offset = 2; break;
                        case VP8_MV_QUARTERS:
                            num_blocks = 4; offset = 2; break;
                        case VP8_MV_16:
                        default:
                            num_blocks = 16; offset = 1; break;
                        };
                        for (bl = 0, j = 0; j < num_blocks; j++)
                        {
                            if (bl < 4) {
                                mvu.s32 = (aMb->RefFrameMode) ? ((aMb->MbMode==VP8_MV_SPLIT) ? aMb->NewMV4x4[bl+12].s32 : aMb->NewMV16.s32) : 0;
                            } else
                                mvu.s32 = cMb->NewMV4x4[bl - 4].s32;
                            if (bl & 3) {
                                mvl.s32 = cMb->NewMV4x4[bl - 1].s32;
                            } else {
                                mvl.s32 = (lMb->RefFrameMode) ? ((lMb->MbMode==VP8_MV_SPLIT) ? lMb->NewMV4x4[bl+3].s32 : lMb->NewMV16.s32) : 0;
                            }
                            if (mvl.s32 == mvu.s32)
                                cntx = mvl.s32 ? 3 : 4;
                            else
                                cntx = (mvl.s32 ? 0 : 1) + (mvu.s32 ? 0 : 2);
                            smode = (cMb->InterSplitModes >> (bl*2)) & 0x03;
                            vp8EncodeTreeToken(m_BoolEncStates, vp8_block_mv_map[smode],vp8_block_mv_tree, vp8_block_mv_probs[cntx], 0);
                            if(smode==VP8_B_MV_NEW) {
                                vp8EncodeMvComponent(m_BoolEncStates, (cMb->NewMV4x4[static_cast<int>(bl)].MV.mvy-nearMvs[0].MV.mvy)>>1, m_ctrl.mvContexts[0]);
                                vp8EncodeMvComponent(m_BoolEncStates, (cMb->NewMV4x4[static_cast<int>(bl)].MV.mvx-nearMvs[0].MV.mvx)>>1, m_ctrl.mvContexts[1]);
                            }
                            if (cMb->MbSubmode == VP8_MV_QUARTERS && j==1) bl+=4;
                            bl += offset;
                        }
                    }
                    break;
                default:
                    break;
                }
            }
            cMb++;
        }
    }

    void Vp8CoreBSE::EncodeKeyFrameMB(void)
    {
        I32         i, mPitch;
        TrMbCodeVp8 *cMb,*aMb,*lMb;

        mPitch  = m_Mbs1[m_Params.fSizeInMBs-1].MbXcnt + 1;
        cMb     = m_Mbs1;
        for(i=0; i<(I32)m_Params.fSizeInMBs; i++)
        {
            lMb = (cMb->MbXcnt)? cMb - 1 : &m_externalMb1;
            aMb = (cMb->MbYcnt)? cMb - mPitch : &m_externalMb1;
            if(m_ctrl.SegOn && m_ctrl.MBSegUpdate)
            {
                switch(cMb->SegmentId){
                case 1:
                    vp8EncodeBit(m_BoolEncStates, 0, m_ctrl.segTreeProbs[0]);
                    vp8EncodeBit(m_BoolEncStates, 1, m_ctrl.segTreeProbs[1]);
                    break;
                case 2:
                    vp8EncodeBit(m_BoolEncStates, 1, m_ctrl.segTreeProbs[0]);
                    vp8EncodeBit(m_BoolEncStates, 0, m_ctrl.segTreeProbs[2]);
                    break;
                case 3:
                    vp8EncodeBit(m_BoolEncStates, 1, m_ctrl.segTreeProbs[0]);
                    vp8EncodeBit(m_BoolEncStates, 1, m_ctrl.segTreeProbs[2]);
                    break;
                case 0:
                default:
                    vp8EncodeBit(m_BoolEncStates, 0, m_ctrl.segTreeProbs[0]);
                    vp8EncodeBit(m_BoolEncStates, 0, m_ctrl.segTreeProbs[1]);
                }
            }
            if(m_ctrl.MbNoCoeffSkip)
                vp8EncodeBit(m_BoolEncStates, cMb->CoeffSkip, m_ctrl.prSkipFalse);

            vp8EncodeTreeToken(m_BoolEncStates, vp8_kf_mb_mode_y_map[cMb->MbMode], vp8_kf_mb_mode_y_tree, vp8_kf_mb_mode_y_probs, 0);

            if(cMb->MbMode==VP8_MB_B_PRED)
            {
                U32     j, aSubMbModes,lSubMbModes, cSubMbModes, a, l, c;

                aSubMbModes = aMb->Y4x4Modes3; lSubMbModes = (cMb->Y4x4Modes0 << 4) | (lMb->Y4x4Modes0 >> 12); cSubMbModes = cMb->Y4x4Modes0;
                for(j=0;j<4;j++) {
                    a = aSubMbModes & 0xF; l = lSubMbModes & 0xF; c = cSubMbModes & 0xF;
                    vp8EncodeTreeToken(m_BoolEncStates, vp8_block_mode_map[c], vp8_block_mode_tree, vp8_kf_block_mode_probs[a][l],0);
                    aSubMbModes >>= 4; lSubMbModes >>= 4; cSubMbModes >>= 4;
                }
                aSubMbModes = cMb->Y4x4Modes0; lSubMbModes = (cMb->Y4x4Modes1 << 4) | (lMb->Y4x4Modes1 >> 12); cSubMbModes = cMb->Y4x4Modes1;
                for(j=0;j<4;j++) {
                    a = aSubMbModes & 0xF; l = lSubMbModes & 0xF; c = cSubMbModes & 0xF;
                    vp8EncodeTreeToken(m_BoolEncStates, vp8_block_mode_map[c], vp8_block_mode_tree, vp8_kf_block_mode_probs[a][l],0);
                    aSubMbModes >>= 4; lSubMbModes >>= 4; cSubMbModes >>= 4;
                }
                aSubMbModes = cMb->Y4x4Modes1; lSubMbModes = (cMb->Y4x4Modes2 << 4) | (lMb->Y4x4Modes2 >> 12); cSubMbModes = cMb->Y4x4Modes2;
                for(j=0;j<4;j++) {
                    a = aSubMbModes & 0xF; l = lSubMbModes & 0xF; c = cSubMbModes & 0xF;
                    vp8EncodeTreeToken(m_BoolEncStates, vp8_block_mode_map[c], vp8_block_mode_tree, vp8_kf_block_mode_probs[a][l],0);
                    aSubMbModes >>= 4; lSubMbModes >>= 4; cSubMbModes >>= 4;
                }
                aSubMbModes = cMb->Y4x4Modes2; lSubMbModes = (cMb->Y4x4Modes3 << 4) | (lMb->Y4x4Modes3 >> 12); cSubMbModes = cMb->Y4x4Modes3;
                for(j=0;j<4;j++) {
                    a = aSubMbModes & 0xF; l = lSubMbModes & 0xF; c = cSubMbModes & 0xF;
                    vp8EncodeTreeToken(m_BoolEncStates, vp8_block_mode_map[c], vp8_block_mode_tree, vp8_kf_block_mode_probs[a][l],0);
                    aSubMbModes >>= 4; lSubMbModes >>= 4; cSubMbModes >>= 4;
                }
            }
            vp8EncodeTreeToken(m_BoolEncStates, vp8_mb_mode_uv_map[cMb->MbSubmode], vp8_mb_mode_uv_tree, vp8_kf_mb_mode_uv_probs,0);
            cMb++;
        }
    }

    VPX_FORCEINLINE void vp8UpdateMVCounters(Vp8MvCounters *cnt, I16 v)
    {
        U16     absv = (v<0)?-v:v;
        I32     i;

        if(absv<VP8_MV_LONG-1) {
            cnt->evnCnt[0][VP8_MV_IS_SHORT]++;
            cnt->absValHist[absv]++;
        } else {
            cnt->evnCnt[1][VP8_MV_IS_SHORT]++;
            for(i=0;i<3;i++)
                cnt->evnCnt[(absv >> i) & 1][VP8_MV_LONG+i]++;
            for (i = VP8_MV_LONG_BITS - 1; i > 3; i--)
                cnt->evnCnt[(absv >> i) & 1][VP8_MV_LONG+i]++;
            if (absv & 0xFFF0)
                cnt->evnCnt[(absv >> 3) & 1][VP8_MV_LONG+3]++;
        }
        if(absv!=0) 
            cnt->evnCnt[(absv!=v)][VP8_MV_SIGN]++;
    }

    VPX_FORCEINLINE void vp8UpdateEobCounters(Vp8Counters *cnt, U8 ctx1, U8 ctx2, U8 ctx3)
    {
        cnt->mbTokenHist[ctx1][ctx2][ctx3][VP8_DCTEOB]++;
    }

    void Vp8CoreBSE::UpdateEntropyModel(void)
    {
        U32             oldCost, newCost;
        I32             i, j, k, l;
        Vp8Prob         p, nProb[VP8_NUM_MB_MODES_Y - 1];
        U32             tc[2][VP8_NUM_COEFF_NODES];
        Vp8Counters     &m_cnt = m_cnt1;


        if(m_ctrl.MbNoCoeffSkip)
            m_ctrl.prSkipFalse = vp8ProbFromCounters(m_Params.fSizeInMBs-m_cnt.mbSkipFalse, m_cnt.mbSkipFalse);

        if(m_ctrl.SegOn) {
            if(m_ctrl.MBSegUpdate) {
                m_ctrl.segTreeProbs[0] = vp8ProbFromCounters(m_cnt.mbSegHist[0]+m_cnt.mbSegHist[1],m_cnt.mbSegHist[2]+m_cnt.mbSegHist[3]);
                m_ctrl.segTreeProbs[1] = vp8ProbFromCounters(m_cnt.mbSegHist[0],m_cnt.mbSegHist[1]);
                m_ctrl.segTreeProbs[2] = vp8ProbFromCounters(m_cnt.mbSegHist[2],m_cnt.mbSegHist[3]);
            } else {
                for(i=0;i<VP8_NUM_OF_SEGMENT_TREE_PROBS;i++)
                    m_ctrl.segTreeProbs[i] = segTreeProbsC[i];
            }
        }

        if(m_ctrl.FrameType) {
            m_ctrl.prIntra  = vp8ProbFromCounters(m_cnt.mbRefHist[0],m_cnt.mbRefHist[1]+m_cnt.mbRefHist[2]+m_cnt.mbRefHist[3]);
            m_ctrl.prLast   = vp8ProbFromCounters(m_cnt.mbRefHist[1],m_cnt.mbRefHist[2]+m_cnt.mbRefHist[3]);
            m_ctrl.prGolden = vp8ProbFromCounters(m_cnt.mbRefHist[2],m_cnt.mbRefHist[3]);
        }

        if(m_ctrl.FrameType) {
            if(m_ctrl.RefreshEntropyP) {
                memmove(m_ctrl.coeff_probs_pf,m_ctrl.coeff_probs,sizeof(m_ctrl.coeff_probs));
                memmove(m_ctrl.mvContexts_pf,m_ctrl.mvContexts,sizeof(m_ctrl.mvContexts));
                memmove(m_ctrl.mbModeProbY_pf,m_ctrl.mbModeProbY,sizeof(m_ctrl.mbModeProbY));
                memmove(m_ctrl.mbModeProbUV_pf,m_ctrl.mbModeProbUV,sizeof(m_ctrl.mbModeProbUV));
            }

            for (oldCost = 0, i = 0; i < VP8_NUM_MB_MODES_Y; i++)
                oldCost += m_cnt.mbModeYHist[i] * vp8GetTreeTokenCost(vp8_mb_mode_y_map[i], vp8_mb_mode_y_tree, m_ctrl.mbModeProbY, 0);
            vp8GetTreeProbFromHist(vp8_mb_mode_y_tree, vp8_mb_mode_y_map, m_cnt.mbModeYHist, nProb, VP8_NUM_MB_MODES_Y, VP8_NUM_MB_MODES_Y - 1);
            for (newCost = 0, i = 0; i < VP8_NUM_MB_MODES_Y; i++)
                newCost += m_cnt.mbModeYHist[i] * vp8GetTreeTokenCost(vp8_mb_mode_y_map[i], vp8_mb_mode_y_tree, nProb, 0);
            newCost += (VP8_NUM_MB_MODES_Y-1) * 8 * 255;

            m_ctrl.Intra16PUpdate = (newCost < oldCost);
            memmove(m_ctrl.mbModeProbY, (m_ctrl.Intra16PUpdate) ? nProb : m_ctrl.mbModeProbY_pf, sizeof(m_ctrl.mbModeProbY));

            for (oldCost = 0, i = 0; i < VP8_NUM_MB_MODES_UV; i++)
                oldCost += m_cnt.mbModeUVHist[i] * vp8GetTreeTokenCost(vp8_mb_mode_uv_map[i], vp8_mb_mode_uv_tree, m_ctrl.mbModeProbUV, 0);
            vp8GetTreeProbFromHist(vp8_mb_mode_uv_tree, vp8_mb_mode_uv_map, m_cnt.mbModeUVHist, nProb, VP8_NUM_MB_MODES_UV, VP8_NUM_MB_MODES_UV - 1);
            for (newCost = 0, i = 0; i < VP8_NUM_MB_MODES_UV; i++)
                newCost += m_cnt.mbModeUVHist[i] * vp8GetTreeTokenCost(vp8_mb_mode_uv_map[i], vp8_mb_mode_uv_tree, nProb, 0);
            newCost += (VP8_NUM_MB_MODES_UV-1) * 8 * 255;

            m_ctrl.IntraUVPUpdate = (newCost < oldCost);
            memmove(m_ctrl.mbModeProbUV, (m_ctrl.IntraUVPUpdate) ? nProb : m_ctrl.mbModeProbUV_pf, sizeof(m_ctrl.mbModeProbUV));

            for (i = 0; i < 2; i++) {
                vp8GetTreeCountsFromHist(vp8_short_mv_tree, vp8_short_mv_map,
                    VP8_MV_LONG - VP8_MV_SHORT + 1, m_cnt.mbMVs[i].absValHist, m_cnt.mbMVs[i].evnCnt[0]+2, m_cnt.mbMVs[i].evnCnt[1]+2);
                for (j = 0; j < VP8_NUM_MV_PROBS; j++) {
                    oldCost  = vp8GetBranchCost(m_ctrl.mvContexts_pf[i][j],m_cnt.mbMVs[i].evnCnt[0][j],m_cnt.mbMVs[i].evnCnt[1][j]);
                    nProb[0] = vp8ProbFromCounters(m_cnt.mbMVs[i].evnCnt[0][j],m_cnt.mbMVs[i].evnCnt[1][j]) & ~0x01;
                    newCost  = vp8GetBranchCost(nProb[0],m_cnt.mbMVs[i].evnCnt[0][j],m_cnt.mbMVs[i].evnCnt[1][j]);
                    newCost += 7*255 + vp8_c1(vp8_mv_update_probs[i][j]) - vp8_c0(vp8_mv_update_probs[i][j]);
                    m_ctrl.mvContexts[i][j] = (newCost < oldCost) ? nProb[0] : m_ctrl.mvContexts_pf[i][j];
                }
            }
        } else
        {
            memmove(m_ctrl.coeff_probs,vp8_default_coeff_probs,sizeof(m_ctrl.coeff_probs));
            memmove(m_ctrl.coeff_probs_pf,vp8_default_coeff_probs,sizeof(m_ctrl.coeff_probs));
            memmove(m_ctrl.mvContexts,vp8_default_mv_contexts,sizeof(m_ctrl.mvContexts));
            memmove(m_ctrl.mvContexts_pf,vp8_default_mv_contexts,sizeof(m_ctrl.mvContexts));
            memmove(m_ctrl.mbModeProbY,vp8_mb_mode_y_probs,sizeof(m_ctrl.mbModeProbY));
            memmove(m_ctrl.mbModeProbY_pf,vp8_mb_mode_y_probs,sizeof(m_ctrl.mbModeProbY));
            memmove(m_ctrl.mbModeProbUV,vp8_mb_mode_uv_probs,sizeof(m_ctrl.mbModeProbUV));
            memmove(m_ctrl.mbModeProbUV_pf,vp8_mb_mode_uv_probs,sizeof(m_ctrl.mbModeProbUV));
        }

        for (i = 0; i < VP8_NUM_COEFF_PLANES; i++)
            for (j = 0; j < VP8_NUM_COEFF_BANDS; j++)
                for (k = 0; k < VP8_NUM_LOCAL_COMPLEXITIES; k++)
                {
                    memset(tc,0,sizeof(tc));
                    vp8GetTreeCountsFromHist(vp8_token_tree, vp8_token_map, VP8_NUM_DCT_TOKENS, m_cnt.mbTokenHist[i][j][k], tc[0], tc[1]);
                    tc[1][0] -= m_cnt.mbTokenHist[i][j][k][12];
                    for (l = 0; l < VP8_NUM_COEFF_NODES; l++)
                    {
                        oldCost  = vp8GetBranchCost(m_ctrl.coeff_probs[i][j][k][l], tc[0][l], tc[1][l]) >> 8;
                        p = vp8ProbFromCounters(tc[0][l], tc[1][l]);
                        newCost  = vp8GetBranchCost(p, tc[0][l], tc[1][l]) >> 8;
                        newCost += 8 + ((vp8_c1(vp8_coeff_update_probs[i][j][k][l]) - vp8_c0(vp8_coeff_update_probs[i][j][k][l]))>>8);
                        if(newCost < oldCost) {
                            m_ctrl.coeff_probs[i][j][k][l] = p;
                        }
                    }
                }
    }

    void Vp8CoreBSE::CalculateCosts(void)
    {

        U16     IntraModeCostLuma[VP8_NUM_MB_MODES_Y];
        U16     IntraSubModeCost[VP8_NUM_INTRA_BLOCK_MODES];
        U16     MvPartitionCost[VP8_MV_NUM_PARTITIONS];
        U16     SubMvRefCost[VP8_NUM_B_MV_MODES];

        Zero(m_costs);

        if (m_ctrl.FrameType == 0)
        {
            m_costs.IntraModeCost[MODE_INTRA_16x16] = mbmodecost[VP8_MB_DC_PRED];
            m_costs.IntraModeCost[MODE_INTRA_4x4]   = mbmodecost[VP8_MB_B_PRED] + 3645;
            m_costs.IntraNonDCPenalty16x16 = (899 - mbmodecost[VP8_MB_DC_PRED]);
            m_costs.IntraNonDCPenalty4x4 = 1157;
        }
        else
        {
            m_costs.RefFrameCost[VP8_INTRA_FRAME]  = U16(vp8_c0(m_ctrl.prIntra));
            m_costs.RefFrameCost[VP8_LAST_FRAME]   = U16(vp8_c1(m_ctrl.prIntra) + vp8_c0(m_ctrl.prLast));
            m_costs.RefFrameCost[VP8_GOLDEN_FRAME] = U16(vp8_c1(m_ctrl.prIntra) + vp8_c1(m_ctrl.prLast) + vp8_c0(m_ctrl.prGolden));
            m_costs.RefFrameCost[VP8_ALTREF_FRAME] = U16(vp8_c1(m_ctrl.prIntra) + vp8_c1(m_ctrl.prLast) + vp8_c1(m_ctrl.prGolden));

            vp8GetTreeTokenCostArr(vp8_mb_mode_y_map, vp8_mb_mode_y_tree, m_ctrl.mbModeProbY, VP8_NUM_MB_MODES_Y, IntraModeCostLuma);
            vp8GetTreeTokenCostArr(vp8_block_mode_map, vp8_block_mode_tree, vp8_block_mode_probs, VP8_NUM_INTRA_BLOCK_MODES, IntraSubModeCost);
            vp8GetTreeTokenCostArr(vp8_mv_partitions_map, vp8_mv_partitions_tree, vp8_mv_partitions_probs, VP8_MV_NUM_PARTITIONS, MvPartitionCost);
            vp8GetTreeTokenCostArr(vp8_block_mv_map, vp8_block_mv_tree, vp8_block_mv_default_prob, VP8_NUM_B_MV_MODES, SubMvRefCost);

            m_costs.IntraModeCost[MODE_INTRA_16x16] = IntraModeCostLuma[VP8_MB_DC_PRED];
            m_costs.IntraModeCost[MODE_INTRA_4x4] = IntraModeCostLuma[VP8_MB_B_PRED] + IntraSubModeCost[VP8_B_DC_PRED]*16;
            m_costs.IntraNonDCPenalty16x16 = IntraModeCostLuma[VP8_MB_V_PRED] - IntraModeCostLuma[VP8_MB_DC_PRED];
            m_costs.IntraNonDCPenalty4x4 = IntraSubModeCost[VP8_B_HE_PRED] - IntraModeCostLuma[VP8_B_DC_PRED];
        
            m_costs.InterModeCost[MODE_INTER_16x16] =0;
            m_costs.InterModeCost[MODE_INTER_16x8] = MvPartitionCost[VP8_MV_TOP_BOTTOM] +  SubMvRefCost[VP8_B_MV_NEW]
                                                + ((SubMvRefCost[VP8_B_MV_LEFT] + SubMvRefCost[VP8_B_MV_ABOVE])>>1);
            m_costs.InterModeCost[MODE_INTER_8x8] = (MvPartitionCost[VP8_MV_QUARTERS]
                                                + ((SubMvRefCost[VP8_B_MV_LEFT] + SubMvRefCost[VP8_B_MV_ABOVE])>>1)
                                                + 3 * SubMvRefCost[VP8_B_MV_NEW])/4;
            m_costs.InterModeCost[MODE_INTER_4x4] = (MvPartitionCost[VP8_MV_16] +  + 16 * SubMvRefCost[VP8_B_MV_NEW])/4;
        }
    }

    mfxStatus Vp8CoreBSE::OutputBitstream(void)
    {
        I32     i;
        U32     size, bs_size;
        U8     *ptr;
        U32     p0Size = (U32)(m_BoolEncStates[0].pData - m_pPartitions[0]);

        ptr  = (U8*)m_pBitstream->Data + m_pBitstream->DataOffset + m_pBitstream->DataLength;
        size = m_pBitstream->MaxLength - (m_pBitstream->DataOffset + m_pBitstream->DataLength);

        bs_size  = p0Size + ((m_ctrl.FrameType)? 3 : 10);
        bs_size += ((1<<m_ctrl.PartitionNumL2) - 1) * 3;
        for(i=1; i < (1<<m_ctrl.PartitionNumL2)+1; i++)
            bs_size += (U32)(m_BoolEncStates[i].pData - m_pPartitions[i]);

        if(size < bs_size)
            return MFX_ERR_NOT_ENOUGH_BUFFER;

        /* Uncompressed chunk */
        *((U32*)ptr) = m_ctrl.FrameType | (m_ctrl.Version << 1) | (m_ctrl.ShowFrame << 4) | (p0Size << 5); ptr+=3;
        if(!m_ctrl.FrameType) {
            *((U32*)ptr) = 0x2a019d; ptr+=3;
            *((U16*)ptr) = (U16)(m_ctrl.HorSizeCode << 14 | m_Params.SrcWidth); ptr +=2;
            *((U16*)ptr) = (U16)(m_ctrl.VerSizeCode << 14 | m_Params.SrcHeight); ptr +=2;
        }
        /* First partition */
        memmove(ptr, m_pPartitions[0], p0Size);
        ptr += p0Size;
        /* Token partition displacements */
        for(i=1; i < (1<<m_ctrl.PartitionNumL2); i++) {
            *((U32*)ptr) = (U32)(m_BoolEncStates[i].pData - m_pPartitions[i]); ptr +=3;
        }
        /* Token partitions */
        for(i=1; i < (1<<m_ctrl.PartitionNumL2) + 1; i++) {
            memmove(ptr, m_pPartitions[i], (m_BoolEncStates[i].pData - m_pPartitions[i]));
            ptr += (U32)(m_BoolEncStates[i].pData - m_pPartitions[i]);
        }

        m_pBitstream->DataLength = (U32)(ptr - (U8*)m_pBitstream->Data) - m_pBitstream->DataOffset;

        return MFX_ERR_NONE;
    }

    VPX_FORCEINLINE U16 U162bMode(U16 mode) {
        switch(mode) {
        case 0: return VP8_B_DC_PRED;
        case 2: return VP8_B_VE_PRED;
        case 3: return VP8_B_HE_PRED;
        case 1: return VP8_B_TM_PRED;
        case 4: return VP8_B_LD_PRED;
        case 5: return VP8_B_RD_PRED;
        case 6: return VP8_B_VR_PRED;
        case 7: return VP8_B_VL_PRED;
        case 8: return VP8_B_HD_PRED;
        default:
        case 9: return VP8_B_HU_PRED;
        }
    }

    VPX_FORCEINLINE void MbHLD2trMbCode(HLDMbDataVp8 *pS, HLDMvDataVp8 *pSm, TrMbCodeVp8* pMb)
    {
        mfxU8   sbModes[16];
        sbModes[ 0] = (pS->data[1] >>  0) & 0xF;
        sbModes[ 1] = (pS->data[1] >>  4) & 0xF;
        sbModes[ 2] = (pS->data[1] >>  8) & 0xF;
        sbModes[ 3] = (pS->data[1] >> 12) & 0xF;
        sbModes[ 4] = (pS->data[1] >> 16) & 0xF;
        sbModes[ 5] = (pS->data[1] >> 20) & 0xF;
        sbModes[ 6] = (pS->data[1] >> 24) & 0xF;
        sbModes[ 7] = (pS->data[1] >> 28) & 0xF;

        sbModes[ 8] = (pS->data[2] >>  0) & 0xF;
        sbModes[ 9] = (pS->data[2] >>  4) & 0xF;
        sbModes[10] = (pS->data[2] >>  8) & 0xF;
        sbModes[11] = (pS->data[2] >> 12) & 0xF;
        sbModes[12] = (pS->data[2] >> 16) & 0xF;
        sbModes[13] = (pS->data[2] >> 20) & 0xF;
        sbModes[14] = (pS->data[2] >> 24) & 0xF;
        sbModes[15] = (pS->data[2] >> 28) & 0xF;

        pMb->DWord0       = 0;
        pMb->nonZeroCoeff = 0;
        pMb->MbXcnt       = (pS->data[0]) & 0x3FF;
        pMb->MbYcnt       = (pS->data[0] >> 10) & 0x3FF;
        pMb->CoeffSkip    = (pS->data[0] >> 20) & 0x1;
        pMb->RefFrameMode = ((pS->data[0] >> 21) & 0x1) ? ((pS->data[0] >> 22) & 0x3) : VP8_INTRA_FRAME;

        if(pMb->RefFrameMode) {
            pMb->MbMode = (pS->data[0] >> 24) & 0x07;
            if(pMb->MbMode==VP8_MV_SPLIT) {
                pMb->MbSubmode = ((pS->data[0] >> 27) & 0x7) - 1;
                switch(pMb->MbSubmode) {
                case VP8_MV_TOP_BOTTOM:
                    sbModes[ 7] = sbModes[ 6] = sbModes[ 5] = sbModes[ 4] = sbModes[ 3] = sbModes[ 2] = sbModes[ 1] = sbModes[ 0];
                    sbModes[15] = sbModes[14] = sbModes[13] = sbModes[12] = sbModes[11] = sbModes[10] = sbModes[ 9] = sbModes[ 8];
                    break;
                case VP8_MV_LEFT_RIGHT:
                    sbModes[13] = sbModes[12] = sbModes[ 9] = sbModes[ 8] = sbModes[ 5] = sbModes[ 4] = sbModes[ 1] = sbModes[ 0];
                    sbModes[15] = sbModes[14] = sbModes[11] = sbModes[10] = sbModes[ 7] = sbModes[ 6] = sbModes[ 3] = sbModes[ 2];
                    break;
                case VP8_MV_QUARTERS:
                    sbModes[13] = sbModes[12] = sbModes[ 9] = sbModes[ 8];  sbModes[ 5] = sbModes[ 4] = sbModes[ 1] = sbModes[ 0];
                    sbModes[15] = sbModes[14] = sbModes[11] = sbModes[10];  sbModes[ 7] = sbModes[ 6] = sbModes[ 3] = sbModes[ 2];
                    break;
                default:
                    break;
                }
                pMb->InterSplitModes = 0;
                for(I32 k=15;k>=0;k--) {
                    pMb->InterSplitModes <<= 2;
                    pMb->InterSplitModes |= sbModes[k];
                    pMb->NewMV4x4[k].s32 = pSm->MV[k].s32;
                }
            } else {
                pMb->NewMV16.s32 = pSm->MV[15].s32;            /* VP8_MV_NEW vector placeholder */
            }
        } else {
            pMb->MbMode = (pS->data[0] >> 27) & 0x07;
            if(pMb->MbMode == VP8_MB_B_PRED) {
                pMb->Y4x4Modes0 = (U162bMode(sbModes[ 3])<<12)|(U162bMode(sbModes[ 2])<<8)|(U162bMode(sbModes[ 1])<<4)|(U162bMode(sbModes[ 0]));
                pMb->Y4x4Modes1 = (U162bMode(sbModes[ 7])<<12)|(U162bMode(sbModes[ 6])<<8)|(U162bMode(sbModes[ 5])<<4)|(U162bMode(sbModes[ 4]));
                pMb->Y4x4Modes2 = (U162bMode(sbModes[11])<<12)|(U162bMode(sbModes[10])<<8)|(U162bMode(sbModes[ 9])<<4)|(U162bMode(sbModes[ 8]));
                pMb->Y4x4Modes3 = (U162bMode(sbModes[15])<<12)|(U162bMode(sbModes[14])<<8)|(U162bMode(sbModes[13])<<4)|(U162bMode(sbModes[12]));
            } else {
                pMb->Y4x4Modes0 = (U16)((pMb->MbMode<<12)|(pMb->MbMode<<8)|(pMb->MbMode<<4)|(pMb->MbMode));
                pMb->Y4x4Modes1 = (U16)((pMb->MbMode<<12)|(pMb->MbMode<<8)|(pMb->MbMode<<4)|(pMb->MbMode));
                pMb->Y4x4Modes2 = (U16)((pMb->MbMode<<12)|(pMb->MbMode<<8)|(pMb->MbMode<<4)|(pMb->MbMode));
                pMb->Y4x4Modes3 = (U16)((pMb->MbMode<<12)|(pMb->MbMode<<8)|(pMb->MbMode<<4)|(pMb->MbMode));
            }
            pMb->MbSubmode =(pS->data[0] >> 30) & 0x3;
        }
    }

    VPX_FORCEINLINE U32 un_zigzag_and_last_nonzero_coeff_sse( const I16* src, I16* dst )
    {
#define pshufb_word_lower(a) (a)*2, (a)*2+1
#define pshufb_word_upper(a) (a-8)*2, (a-8)*2+1
        //        const U8 vp8_zigzag_scan[16] = { 0, 1, 4, 8, 5, 2, 3, 6, 9, 12, 13, 10, 7, 11, 14, 15 };
        __m128i v_shuf0_7 = _mm_setr_epi8(
            pshufb_word_lower(0),
            pshufb_word_lower(1),
            pshufb_word_lower(4),
            -1, -1, // pshufb_word_lower(8),
            pshufb_word_lower(5),
            pshufb_word_lower(2),
            pshufb_word_lower(3),
            pshufb_word_lower(6) );

        __m128i v_shuf8_15 = _mm_setr_epi8(
            pshufb_word_upper(9),
            pshufb_word_upper(12),
            pshufb_word_upper(13),
            pshufb_word_upper(10),
            -1, -1, // pshufb_word_upper(7),
            pshufb_word_upper(11),
            pshufb_word_upper(14),
            pshufb_word_upper(15) );
#undef pshufb_word_lower
#undef pshufb_word_upper

        __m128i v_coeffs0_7  = _mm_loadu_si128( (const __m128i*)src );
        __m128i v_coeffs8_15 = _mm_loadu_si128( (const __m128i*)(src+8) );

        v_coeffs0_7  = _mm_shuffle_epi8( v_coeffs0_7, v_shuf0_7  );
        v_coeffs8_15 = _mm_shuffle_epi8( v_coeffs8_15, v_shuf8_15);

        v_coeffs0_7  = _mm_insert_epi16( v_coeffs0_7,  *(const int*)(src+8), 3 ); // *(const int*) required for CL to generate a LD+OP form
        v_coeffs8_15 = _mm_insert_epi16( v_coeffs8_15, *(const int*)(src+7), 4 );

        _mm_storeu_si128( (__m128i*)dst,       v_coeffs0_7  );
        _mm_storeu_si128( (__m128i*)(dst + 8), v_coeffs8_15 );

        v_coeffs0_7  = _mm_cmpeq_epi16( v_coeffs0_7,  _mm_setzero_si128() );
        v_coeffs8_15 = _mm_cmpeq_epi16( v_coeffs8_15, _mm_setzero_si128() );

        U32 mask = _mm_movemask_epi8( v_coeffs0_7 );
        mask |= _mm_movemask_epi8( v_coeffs8_15 ) << 16;

        return mask;
    }

    void Vp8CoreBSE::EncodeTokens( const CoeffsType un_zigzag_coeffs[], 
        I32 firstCoeff, I32 lastCoeff, U8 cntx1, U8 cntx3, U8 part )
    {
        U8 prevZero = 0;
        U8 cntx1_precomp = cntx1*VP8_NUM_COEFF_BANDS*VP8_NUM_LOCAL_COMPLEXITIES;

        I32 k = firstCoeff;
        for ( ; k <= lastCoeff; k++)
        {
            U16 tvalue = vp8CNVToken(un_zigzag_coeffs[k]);
            U16 cat = (tvalue >> 12);
            U16 ctx = cntx1_precomp + vp8_coefBands[k]*VP8_NUM_LOCAL_COMPLEXITIES + cntx3;

            *m_TokensE[part]++ = ctx + prevZero;
            *m_TokensE[part]++ = tvalue;
            //printf("tk = %d, ctx = %d\n", tvalue, ctx + prevZero);
            U32* hist = (U32*)(m_cnt1.mbTokenHist) + (ctx<<4);

            hist[cat]++;
            if(prevZero) hist[VP8_DCTEOB+1]++;

            switch(cat){
            case 0:
                prevZero = 128; cntx3 = 0; break;
            case 1:
                prevZero = 0; cntx3 = 1; break;
            default:
                prevZero = 0; cntx3 = 2; break;
            }
        }

        if (lastCoeff!=15) {
            *m_TokensE[part]++ = cntx1_precomp + vp8_coefBands[k]*VP8_NUM_LOCAL_COMPLEXITIES + cntx3 + 256;
            vp8UpdateEobCounters(&m_cnt1, cntx1, vp8_coefBands[k], cntx3);
        }
    }
    void Vp8CoreBSE::TokenizeAndCnt(TaskHybridDDI *pTask, MBDATA_LAYOUT const & layout, mfxCoreInterface * pCore)
    {
        I32          i, j, partId, mPitch, maxPartId, mRowCnt, cnzMsk;
        U32         *pCoeff32, isZeroY2;
        U8           cntx3;
        TrMbCodeVp8 *cMb,*aMb,*lMb;
        MvTypeVp8    nearMvs[4];
        I32          nearCnt[4];
        I16          limT, limL, limB, limR;
        U32          aCntx3, lCntx3;

        FrameLocker lockSrc0(pCore, pTask->ddi_frames.m_pMB_hw->pSurface->Data, false);

        HLDMbDataVp8 *pHLDMbData = (HLDMbDataVp8 *)(pTask->ddi_frames.m_pMB_hw->pSurface->Data.Y + layout.MB_CODE_offset);
        HLDMvDataVp8 *pHLDMvData = (HLDMvDataVp8 *)(pTask->ddi_frames.m_pMB_hw->pSurface->Data.Y + layout.MV_offset);

        // get QP and loop_filter_levels provided by BRC
        ReadBRCStatusReport(pTask->ddi_frames.m_pMB_hw->pSurface->Data.Y,
                            m_ctrl,
                            layout);

        // save QP value returned by driver
        pTask->m_pRecFrame->QP = m_ctrl.qIndex;

        mPitch      = ((pHLDMbData[m_Params.fSizeInMBs-1].data[0]) & 0x3FF) + 1;
        mRowCnt     = ((pHLDMbData[m_Params.fSizeInMBs-1].data[0]>>10) & 0x3FF) + 1;
        cMb         = m_Mbs1;
        partId      = 0;
        maxPartId   = (1<<m_ctrl.PartitionNumL2);

        for(i=0; i<maxPartId; i++) m_TokensE[i] = m_TokensB[i];

        for(i=0; i<(I32)m_Params.fSizeInMBs; i++)
        {
            // Initialized truncated MB data
            MbHLD2trMbCode(pHLDMbData, pHLDMvData, cMb);
            partId = (cMb->MbYcnt&(~(0xFFFFFFFF<<m_ctrl.PartitionNumL2)));

            // Gather counters
            m_cnt1.mbRefHist[cMb->RefFrameMode]++;
            if(m_ctrl.SegOn && m_ctrl.MBSegUpdate)
                m_cnt1.mbSegHist[cMb->SegmentId]++;
            if(cMb->RefFrameMode) {
                limT = -(((I16)cMb->MbYcnt + 1) << 7);
                limL = -(((I16)cMb->MbXcnt + 1) << 7);
                limB = (I16)(mRowCnt - cMb->MbYcnt) << 7;
                limR = (I16)(mPitch - cMb->MbXcnt) << 7;
                Vp8GetNearMVs(cMb, mPitch, nearMvs, nearCnt, m_fRefBias);
                clampMvComponent(nearMvs[0], limT, limB, limL, limR);

                switch(cMb->MbMode) {
                case VP8_MV_NEW:
                    vp8UpdateMVCounters(m_cnt1.mbMVs,  (cMb->NewMV16.MV.mvy - nearMvs[0].MV.mvy)>>1);
                    vp8UpdateMVCounters(m_cnt1.mbMVs+1,(cMb->NewMV16.MV.mvx - nearMvs[0].MV.mvx)>>1);
                    break;
                case VP8_MV_SPLIT:
                    {
                        I8 j = 0, bl = 0, offset, num_blocks, smode;

                        switch(cMb->MbSubmode) {
                        case VP8_MV_TOP_BOTTOM:
                            num_blocks = 2; offset = 8; break;
                        case VP8_MV_LEFT_RIGHT:
                            num_blocks = 2; offset = 2; break;
                        case VP8_MV_QUARTERS:
                            num_blocks = 4; offset = 2; break;
                        case VP8_MV_16:
                        default:
                            num_blocks = 16; offset = 1; break;
                        };
                        for (bl = 0, j = 0; j < num_blocks; j++)
                        {
                            smode = (cMb->InterSplitModes >> (bl*2)) & 0x03;
                            if(smode==VP8_B_MV_NEW) {
                                vp8UpdateMVCounters(m_cnt1.mbMVs,  (cMb->NewMV4x4[static_cast<int>(bl)].MV.mvy - nearMvs[0].MV.mvy)>>1);
                                vp8UpdateMVCounters(m_cnt1.mbMVs+1,(cMb->NewMV4x4[static_cast<int>(bl)].MV.mvx - nearMvs[0].MV.mvx)>>1);
                            }
                            if (cMb->MbSubmode == VP8_MV_QUARTERS && j==1) bl+=4;
                            bl += offset;
                        }
                    }
                    break;
                default:
                    break;
                }
            } else {
                m_cnt1.mbModeYHist[cMb->MbMode]++;
                m_cnt1.mbModeUVHist[cMb->MbSubmode]++;
            }
            if(cMb->CoeffSkip) m_cnt1.mbSkipFalse++;

            // Check for skip and set Y2 contexts
            lMb = (cMb->MbXcnt)? cMb - 1 : &m_externalMb1;
            aMb = (cMb->MbYcnt)? cMb - mPitch : &m_externalMb1;
            isZeroY2 = 0;
            if(cMb->MbMode!=VP8_MB_B_PRED) {
                if(!cMb->CoeffSkip) {
                    pCoeff32 = (U32*)&(pHLDMbData->mbCoeff[24]);
                    for(j=0; j<8; j++)
                        isZeroY2 |= pCoeff32[j];
                }
                cMb->Y2AboveNotZero = cMb->Y2LeftNotZero = (isZeroY2!=0);
            } else {
                cMb->Y2AboveNotZero = aMb->Y2AboveNotZero;
                cMb->Y2LeftNotZero  = lMb->Y2LeftNotZero;
            }

            // Forced skip
            if(cMb->CoeffSkip && m_ctrl.MbNoCoeffSkip) {
                cMb->nonZeroCoeff = 0;
                cMb++; pHLDMbData++; pHLDMvData++;
                continue;
            }

            CoeffsType  un_zigzag_coeffs[16];
            unsigned long mask, index;

            aCntx3 = ((aMb->nonZeroCoeff>>12) & 0x0000000F) | ((aMb->nonZeroCoeff>>2) & 0x00330000);
            lCntx3 = ((lMb->nonZeroCoeff>> 3) & 0x00001111) | ((lMb->nonZeroCoeff>>1) & 0x00550000);
            cnzMsk = 0;

            if(cMb->MbMode!=VP8_MB_B_PRED) {
                cntx3 = (U8)(aMb->Y2AboveNotZero + lMb->Y2LeftNotZero);
                mask = un_zigzag_and_last_nonzero_coeff_sse( pHLDMbData->mbCoeff[24], un_zigzag_coeffs);
                if ( ! _BitScanReverse( &index, (mask ^ -1) ) ) {
                    *m_TokensE[partId]++ = (VP8_NUM_COEFF_BANDS*VP8_NUM_LOCAL_COMPLEXITIES+cntx3) + 256;
                    vp8UpdateEobCounters(&m_cnt1, 1, 0, cntx3);
                } else {
                    EncodeTokens( un_zigzag_coeffs, 0, (I32)index >> 1, 1, cntx3, (U8)partId );
                }

                for(j=0;j<16;j++) {
                    cntx3 = ((aCntx3 >> j)&0x1) + ((lCntx3 >> j)&0x1);

                    mask = un_zigzag_and_last_nonzero_coeff_sse( pHLDMbData->mbCoeff[j], un_zigzag_coeffs) | 0x3;
                    if ( ! _BitScanReverse( &index, (mask ^ -1) ) ) {
                        *m_TokensE[partId]++ = (VP8_NUM_LOCAL_COMPLEXITIES+cntx3) + 256;
                        vp8UpdateEobCounters(&m_cnt1, 0, 1, cntx3);
                    } else {
                        EncodeTokens( un_zigzag_coeffs, 1, (I32)index >> 1, 0, cntx3, (U8)partId );
                        cnzMsk |= (0x1 << j);
                        aCntx3 |= ((0x1<<(j+4)) & 0x0000FFF0);
                        lCntx3 |= ((0x1<<(j+1)) & 0x0000EEEE);
                    }
                }
            } else {
                for(j=0;j<16;j++) {
                    cntx3 = ((aCntx3 >> j)&0x1) + ((lCntx3 >> j)&0x1);

                    mask = un_zigzag_and_last_nonzero_coeff_sse( pHLDMbData->mbCoeff[j], un_zigzag_coeffs);
                    if ( ! _BitScanReverse( &index, (mask ^ -1) ) ) {
                        *m_TokensE[partId]++ = ((3*VP8_NUM_COEFF_BANDS*VP8_NUM_LOCAL_COMPLEXITIES+cntx3) ) + 256;
                        vp8UpdateEobCounters(&m_cnt1, 3, vp8_coefBands[0], cntx3);
                    } else {
                        EncodeTokens( un_zigzag_coeffs, 0, (I32)index >> 1, 3, cntx3, (U8)partId );
                        cnzMsk |= (0x1 << j);
                        aCntx3 |= ((0x1<<(j+4)) & 0x0000FFF0);
                        lCntx3 |= ((0x1<<(j+1)) & 0x0000EEEE);
                    }
                }
            }

            for(j=16;j<24;j++) {
                cntx3 = ((aCntx3 >> j)&0x1) + ((lCntx3 >> j)&0x1);

                mask = un_zigzag_and_last_nonzero_coeff_sse( pHLDMbData->mbCoeff[j], un_zigzag_coeffs);
                if ( ! _BitScanReverse( &index, (mask ^ -1) ) ) {
                    *m_TokensE[partId]++ = ((2*VP8_NUM_COEFF_BANDS*VP8_NUM_LOCAL_COMPLEXITIES+cntx3)) + 256;
                    vp8UpdateEobCounters(&m_cnt1, 2, vp8_coefBands[0], cntx3);
                } else {
                    EncodeTokens( un_zigzag_coeffs, 0, (I32)index >> 1, 2, cntx3, (U8)partId );
                    cnzMsk |= (0x1 << j);
                    aCntx3 |= ((0x1<<(j+2)) & 0x00CC0000);
                    lCntx3 |= ((0x1<<(j+1)) & 0x00AA0000);
                }
            }

            cMb->nonZeroCoeff = cnzMsk;
            cMb++; pHLDMbData++; pHLDMvData++;
        }
    }

    VP8HybridCosts Vp8CoreBSE::GetUpdatedCosts()
    {
        return m_costs;
    }
}
#endif // MFX_ENABLE_VP8_VIDEO_ENCODE_HW

