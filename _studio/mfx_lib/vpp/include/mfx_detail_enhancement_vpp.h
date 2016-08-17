/* /////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2010-2016 Intel Corporation. All Rights Reserved.
//
//
//          Detail Enhancement algorithm of Video Pre\Post Processing
//
*/

#include "mfx_common.h"

#if defined (MFX_ENABLE_VPP)

#ifndef __MFX_DETAIL_ENHANCEMENT_VPP_H
#define __MFX_DETAIL_ENHANCEMENT_VPP_H

#include "mfxvideo++int.h"

#include "mfx_vpp_defs.h"
#include "mfx_vpp_base.h"

#define VPP_DETAIL_ENHANCEMENT_IN_NUM_FRAMES_REQUIRED  (1)
#define VPP_DETAIL_ENHANCEMENT_OUT_NUM_FRAMES_REQUIRED (1)

class MFXVideoVPPDetailEnhancement : public FilterVPP{
public:

    static mfxU8 GetInFramesCountExt( void ) { return VPP_DETAIL_ENHANCEMENT_IN_NUM_FRAMES_REQUIRED; };
    static mfxU8 GetOutFramesCountExt(void) { return VPP_DETAIL_ENHANCEMENT_OUT_NUM_FRAMES_REQUIRED; };

    // this function is used by VPP Pipeline Query to correct application request
    static mfxStatus Query( mfxExtBuffer* pHint );

#if !defined (MFX_ENABLE_HW_ONLY_VPP)

    MFXVideoVPPDetailEnhancement(VideoCORE *core, mfxStatus* sts);
    virtual ~MFXVideoVPPDetailEnhancement();

    // VideoVPP
    virtual mfxStatus RunFrameVPP(mfxFrameSurface1 *in, mfxFrameSurface1 *out, InternalParam* pParam);

    // VideoBase methods
    virtual mfxStatus Close(void);
    virtual mfxStatus Init(mfxFrameInfo* In, mfxFrameInfo* Out);
    virtual mfxStatus Reset(mfxVideoParam *par);

    // tuning parameters
    virtual mfxStatus SetParam( mfxExtBuffer* pHint );

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
    Ipp8u     m_divTable[256];

    int       m_gainFactor;                     //strength of sharpening
    int       m_internalGainFactor;             //real strength of sharpening

    int       m_strongSobel;                    //strong edge threshold
    int       m_weakSobel;                      //no edge threshold

    int       m_strongWeight;                   //strength of sharpening for strong edge
    int       m_regularWeight;                  //strength of sharpening for regular edge
    int       m_weakWeight;                     //strength of sharpening for weak edge

    // internal buffers
    Ipp8u*    m_pSobelBuffer;
    int       m_sobelPitch;

    Ipp8u*    m_pDiffBuffer;
    int       m_diffPitch;

    Ipp8u*    m_pExtBuffer;
    int       m_extPitch;

    // iin case of default params filter can be excluded from pipeline to high performance
    bool      m_isFilterActive;

    mfxStatus DetailFilterCore( Ipp8u* pSrc, int srcPitch,
                                Ipp8u* pDst, int dstPitch,
                                Ipp8u* pSobel, int sobelPitch,
                                Ipp8u* pDiff,  int diffPitch,
                                IppiSize size);
#endif
};

#endif // __MFX_DETAIL_ENHANCEMENT_VPP_H

#endif // MFX_ENABLE_VPP
/* EOF */