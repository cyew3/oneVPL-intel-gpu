// Copyright (c) 2012-2018 Intel Corporation
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

#include "mfx_common.h"

#if defined(MFX_ENABLE_VPP) && defined(MFX_ENABLE_IMAGE_STABILIZATION_VPP)

#ifndef __MFX_IMAGE_STABILIZATION_VPP_H
#define __MFX_IMAGE_STABILIZATION_VPP_H

#include "mfxvideo++int.h"

#include "mfx_vpp_defs.h"
#include "mfx_vpp_base.h"
#include <vector>

#define VPP_IS_IN_NUM_FRAMES_REQUIRED  (6+1)
#define VPP_IS_OUT_NUM_FRAMES_REQUIRED (1)

class VideoVME;

class MFXVideoVPPImgStab : public FilterVPP
{
public:

    static mfxU8 GetInFramesCountExt( void ) { return VPP_IS_IN_NUM_FRAMES_REQUIRED; };
    static mfxU8 GetOutFramesCountExt(void) { return VPP_IS_OUT_NUM_FRAMES_REQUIRED; };

    // this function is used by VPP Pipeline Query to correct application request
    static mfxStatus Query( mfxExtBuffer* pHint );

#if !defined (MFX_ENABLE_HW_ONLY_VPP)
    MFXVideoVPPImgStab(VideoCORE *core, mfxStatus* sts);
    virtual ~MFXVideoVPPImgStab();

    // VideoVPP
    virtual mfxStatus RunFrameVPP(mfxFrameSurface1 *in, mfxFrameSurface1 *out, InternalParam* pParam);

    // VideoBase methods
    virtual mfxStatus Close(void);
    virtual mfxStatus Init(mfxFrameInfo* In, mfxFrameInfo* Out);

    // tuning parameters
    virtual mfxStatus SetParam( mfxExtBuffer* pHint );
    virtual mfxStatus Reset(mfxVideoParam *par);

    virtual mfxU8 GetInFramesCount( void ){ return GetInFramesCountExt(); };
    virtual mfxU8 GetOutFramesCount( void ){ return GetOutFramesCountExt(); };

    // work buffer management
    virtual mfxStatus GetBufferSize( mfxU32* pBufferSize );
    virtual mfxStatus SetBuffer( mfxU8* pBuffer );

    virtual mfxStatus CheckProduceOutput(mfxFrameSurface1 *in, mfxFrameSurface1 *out );
    virtual bool      IsReadyOutput( mfxRequestType requestType );

protected:
    //mfxStatus PassThrough(mfxFrameSurface1 *in, mfxFrameSurface1 *out);

private:
    static const mfxU32 MSF_TAP = 11;
    static const mfxU32 MSF_TAP_HALF = 5;

    class Config
    {
    public:
        mfxF64 m_sigma;
        mfxU32 m_DownWidth;
        mfxU32 m_DownHeight;
        mfxU32 m_ME_Range_X;
        mfxU32 m_ME_Range_Y;
        mfxU32 m_MinCostThreshold;
        mfxU32 m_BlockSize;
        mfxF64 m_cf;
        mfxU32 m_MotionModel;
        mfxU32 m_FrameDelay;

        Config(void);

    } m_config;

    /* work buffer is required by ippiResizeSqrPixel */
    mfxU8*        m_pWorkBuf;
    mfxU16        m_mode;//upscale, cropping

    // aux counters
    bool m_firstFrame;
    mfxU32  m_frameCount;

    //algorithm params 
    mfxU32        m_width;
    mfxU32        m_height;
    mfxU32        m_nCrpX;
    mfxU32        m_nCrpY;
    mfxU32        m_destWidth;
    mfxU32        m_destHeight;
    mfxU32        m_scale_h;
    mfxU32        m_scale_v;
    mfxU32        m_downWidth;
    mfxU32        m_downHeight;
    mfxU32        m_widthScale;
    mfxU32        m_heightScale;

    typedef struct
    {
        int x;
        int y;
    }Point;

    typedef struct
    {
        double a;
        double b;
        double c;
        double d;
    }Global_MV;

    class SizeInfo
    {
    public:

        SizeInfo()
            : m_workBufSize(0)
            , m_refSizeY(0)
            , m_curSizeY(0)
            , m_preOutSizeY(0)
            , m_preOutSizeUV(0)
        {
        }

        mfxU32 m_workBufSize;
        mfxU32 m_refSizeY;
        mfxU32 m_curSizeY;
        mfxU32 m_preOutSizeY;
        mfxU32 m_preOutSizeUV;

        mfxU32 GetTotalSize(void)
        {         
            return (m_curSizeY + m_refSizeY + m_workBufSize + m_preOutSizeY + m_preOutSizeUV);
        }
    };

    std::vector<Global_MV> m_GMVArray;
    std::vector<Global_MV> m_CMVArray;
    Global_MV mGMV, mCMV;

    std::vector<Point> mBlock;
    Point mblk;
    std::vector<Point> mMV;

    // resources
    mfxFrameSurface1    m_ref;
    mfxFrameSurface1    m_cur;
    mfxFrameSurface1*   m_FrameBuf[6];
    mfxFrameSurface1    m_preOut;// for upscale mode

    mfxF64        m_motionSmoothFilter[MSF_TAP];

    VideoVME* m_vme;

    void InitMotionSmoothFilter(mfxF64 sigma);

    mfxStatus DoImageStabilization(
        mfxFrameSurface1* in, 
        mfxFrameSurface1* out);

    mfxStatus FrameDownSampling(
        mfxFrameSurface1* in, 
        mfxFrameSurface1* out);

    void MotionCompensation(
        int fn,
        mfxFrameSurface1* cur,
        mfxFrameSurface1* out,
        int width, 
        int height, 
        int frame_delay);

    Global_MV GetFilteredMP(
        int fn, 
        int frm_delay);

    void WarpFrame(
        mfxFrameSurface1* cur,
        mfxFrameSurface1* out,
        Global_MV mWrpMV,
        int width,
        int height,
        int fn);

    mfxStatus GetBufferSizeEx( 
        SizeInfo & sizeInf );

    void BlockMotionSearch( void );

    void SetUsefulBlock(void);

    Point GetPredictMV( void );

    void GlobalMPGeneration( void );

    void AccumulatedGlobalMP( void );

    void PostProcess(mfxFrameSurface1* out);

    mfxStatus DoUpscale( 
        mfxFrameSurface1* in, 
        mfxFrameSurface1* out );

    mfxStatus ResizeNV12( 
        mfxFrameSurface1* in,
        mfxFrameSurface1* out);

    void VME_IS(
        mfxU8 *src, 
        mfxU8 *ref, 
        int width, 
        int height, 
        Point tmpbk, 
        Point *tmpMV, 
        int *distortion);
#endif
};

#endif // __MFX_IMAGE_STABILIZATION_VPP_H

#endif // MFX_ENABLE_VPP
/* EOF */
