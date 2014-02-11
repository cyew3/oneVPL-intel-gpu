/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2013-2014 Intel Corporation. All Rights Reserved.
//
*/

#ifndef _MFX_VP8_ENCODE_HYBRID_PAK_H_
#define _MFX_VP8_ENCODE_HYBRID_PAK_H_

#include "mfx_common.h"
#include "mfx_vp8_encode_utils_hw.h"
#include "mfx_vp8_encode_utils_hybrid_hw.h"

#if defined(MFX_ENABLE_VP8_VIDEO_ENCODE_HW) && defined(MFX_VA)

#if defined VP8_HYBRID_CPUPAK

namespace MFX_VP8ENC
{
    typedef struct {
        U8          mbType;
        I8          mbSubId;    /* -1 - 16x16 mode, 0-15 - 4x4 mode, sublock Id */
        I32         pitchRec;
        PixType    *pYRec;
        PixType    *pURec;
        PixType    *pVRec;
        I32         pitchSrc;
        PixType    *pYSrc;
        PixType    *pUSrc;
        PixType    *pVSrc;
        CoeffsType *pCoeffs;
    } FwdTransIf;

    class Vp8PakMfdVft
    {
    public:
        Vp8PakMfdVft(){};
        virtual ~Vp8PakMfdVft(){};

        void doProcess(FwdTransIf &mB);
        void doProcessSkip(FwdTransIf &mB);
    };

    typedef struct {
        U8          mbType;
        I8          mbSubId;    /* -1 - 16x16 mode, 0-15 - 4x4 mode, sublock Id */
        I32         pitchRec;
        PixType    *pYRec;
        PixType    *pURec;
        PixType    *pVRec;
        I32         pitchDst;
        PixType    *pYDst;
        PixType    *pUDst;
        PixType    *pVDst;
        CoeffsType *pCoeffs;
    } InvTransIf;

    class Vp8PakMfdVit
    {
    public:
        Vp8PakMfdVit(){};
        virtual ~Vp8PakMfdVit(){};

        void doProcess(InvTransIf &mB);
        void doProcessSkip(InvTransIf &mB);
    };

    typedef struct
    {
        U8          LfType;
        U8          LfLevel;
        U8          LfLimit;

        U8          fType;
        U8          MbXcnt;
        U8          MbYcnt;
        U8          notSkipBlockFilter;

        I32         pitch;
        PixType    *pYRec;
        PixType    *pURec;
        PixType    *pVRec;
    } LoopFilterIf;

    class Vp8PakMfdLf
    {
    public:
        Vp8PakMfdLf(){};
        virtual ~Vp8PakMfdLf(){};

        void doProcess(LoopFilterIf &mB);
    };

    typedef struct {
        U8          RefFrameMode;
        U8          MbXcnt;
        U8          MbYcnt;

        I32         pitchRec;
        PixType    *pYRec;
        PixType    *pURec;
        PixType    *pVRec;

        PixType    *aRow[3];
        PixType    *lRow[3];
        union {
            struct {
                U8  MbYIntraMode;
                U8  MbUVIntraMode;
                U8  Mb4x4YIntraModes[16];
                U8  notLastInRow;
                I8  subBlockId;
            };
            struct {
                I32         pitchRef;
                PixType    *pYRef;
                PixType    *pURef;
                PixType    *pVRef;
                U8          filterType;
                U8          MbInterMode;
                U8          MbSplitMode;
                I32         MvLimits[4];
                MvTypeVp8   MbMv;
                MvTypeVp8   Mb4x4MVs[16];
            };
        } PredictionCtrl;
    } PredictionIf;

    class Vp8PakMfdVp
    {
    public:
        Vp8PakMfdVp(){};
        virtual ~Vp8PakMfdVp(){};

        void doProcess(PredictionIf &mB);
    };

    typedef struct
    {
        U8          mbType;
        U8          segId;
        I8          mbSubId;                /* -1 - 16x16 mode, 0-15 - 4x4 mode, sublock Id */
        CoeffsType *pInvTransformResult;    /* Input / output of inv quantization */
        CoeffsType *pY2mbCoeffs;            /* Quantized Y2 coeffs output */
        CoeffsType *pmbCoeffs;              /* Quantized YUV coeffs output */
    } QrcIf;

    typedef struct
    {
        struct {
            U16 fwd_mul[VP8_NUM_COEFF_QUANTIZERS];
            U16 inv_mul[VP8_NUM_COEFF_QUANTIZERS];
            U16 rshift[VP8_NUM_COEFF_QUANTIZERS];
        } fQuants[VP8_MAX_NUM_OF_SEGMENTS];
    } PakMfdVqStateVp8;

    class Vp8PakMfdVq
    {
    public:
        Vp8PakMfdVq(){};
        virtual ~Vp8PakMfdVq(){};

        void LoadState(PakMfdVqStateVp8 *in_hState)
        {
            m_VqState = *in_hState;
        };

        void doProcess(QrcIf &mB);
    private:
        PakMfdVqStateVp8    m_VqState;
    };

    struct PakFrameInfVp8
    {
        U8  fType;
        U8  fSegCtrl;
        U8  fInterCtrl;
        U16 fWidth;
        U16 fHeight;

        U8  fLfType;
        U8  fLfEnabled; /* defined by frame LF strength */

        U8  fLfLevel[VP8_NUM_OF_REF_FRAMES][VP8_NUM_MV_MODES+2][VP8_MAX_NUM_OF_SEGMENTS];
        U8  fLfLimit[VP8_NUM_OF_REF_FRAMES][VP8_NUM_MV_MODES+2][VP8_MAX_NUM_OF_SEGMENTS];
        struct {
            U16 fwd_mul[VP8_NUM_COEFF_QUANTIZERS];
            U16 inv_mul[VP8_NUM_COEFF_QUANTIZERS];
            U16 rshift[VP8_NUM_COEFF_QUANTIZERS];
        } fQuants[VP8_MAX_NUM_OF_SEGMENTS];
    };

    typedef struct {
        U32 width;
        U32 height;
    } BlockSize;

    struct VidSurfaceState
    {
        U32 pitchPixels; 
        PixType* pYPlane;
        PixType* pUPlane;
        PixType* pVPlane;
        VidSurfaceState() : pYPlane(0), pUPlane(0), pVPlane(0) {};
    };

    struct PakSurfaceStateVp8
    {
        VidSurfaceState CurSurfState;           // Current Frame Surface
        VidSurfaceState LrfSurfState;           // Last Reference Frame Surface
        VidSurfaceState GrfSurfState;           // Gold Reference Frame Surface
        VidSurfaceState ArfSurfState;           // Alt Reference Frame Surface
        VidSurfaceState PreILDBReconSurfState;
        VidSurfaceState ReconSurfState;         // Reconstructed Frame Surface
    };

    struct PakMfdStateVp8
    {
        PakSurfaceStateVp8  pakSurfaceState;
        PakFrameInfVp8      pakFrameInfState;
    };

    class Vp8PakMfd
    {
    public:
        Vp8PakMfd();
        ~Vp8PakMfd();

        void LoadState(PakMfdStateVp8 *in_hState);
        void EncAndRecMB(TrMbCodeVp8 &mB, CoeffsType* coeffs);

    protected:
        CoeffsType      m_buffer[25*16];
        PixType         m_reconBuf[384];
        PixType        *m_aRow[3], *m_lRow[3];

    private:
        PakMfdStateVp8  m_mfdState;

        Vp8PakMfdVp     m_Pred;
        Vp8PakMfdVft    m_FwdT;
        Vp8PakMfdVit    m_InvT;
        Vp8PakMfdVq     m_Qnt;
        Vp8PakMfdLf     m_LF;
    };
}

#endif
#endif
#endif
