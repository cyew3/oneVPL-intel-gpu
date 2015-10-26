/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014 Intel Corporation. All Rights Reserved.

File Name: mfx_camera_plugin_cpu.h

\* ****************************************************************************** */


#ifndef _MFX_CAMERA_PLUGIN_CPU_H
#define _MFX_CAMERA_PLUGIN_CPU_H


#include "mfx_camera_plugin_utils.h"

#define NUM_CONTROL_POINTS_SKL 64

using namespace MfxCameraPlugin;

typedef struct _CamInfo {
    int cpu_status;

    int InWidth;
    int InHeight;
    int FrameWidth;
    int FrameHeight;
    int bitDepth;
    bool enable_padd;
    int BayerType;
    unsigned short* cpu_Input;
    unsigned short* cpu_PaddedBayerImg;
    unsigned char* cpu_AVG_flag;
    unsigned char* cpu_AVG_new_flag;
    unsigned char* cpu_HV_flag;

    short* cpu_G_h; short* cpu_B_h; short* cpu_R_h;
    short* cpu_G_v; short* cpu_B_v; short* cpu_R_v;
    short* cpu_G_a; short* cpu_B_a; short* cpu_R_a;
    unsigned short* cpu_G_c; unsigned short* cpu_B_c; unsigned short* cpu_R_c;
    short* cpu_G_o; short* cpu_B_o; short* cpu_R_o;

    short* cpu_G_fgc_in;  short* cpu_B_fgc_in;  short* cpu_R_fgc_in;
    short* cpu_G_fgc_out; short* cpu_B_fgc_out; short* cpu_R_fgc_out;
} CamInfo;

class CPUCameraProcessor: public CameraProcessor
{
public:
    CPUCameraProcessor()  {};
    ~CPUCameraProcessor() {

    };

    static mfxStatus Query(mfxVideoParam * /* in */, mfxVideoParam * /* out */)
    {
        return MFX_ERR_NONE;
    }

    virtual mfxStatus Init(mfxVideoParam * /*par*/) { return MFX_ERR_NONE; };
    virtual mfxStatus Init(CameraParams *PipeParams)
    {
        MFX_CHECK_NULL_PTR1(PipeParams);
        mfxStatus sts = MFX_ERR_NONE;
        sts = (mfxStatus)InitCamera_CPU(&m_cmi,
                                        PipeParams->TileInfo.CropW,
                                        PipeParams->TileInfo.CropH,
                                        PipeParams->BitDepth,
                                        ! PipeParams->Caps.bNoPadding,
                                        PipeParams->Caps.BayerPatternType);
        m_Params = *PipeParams;
        return sts;
    }

    virtual mfxStatus Reset(mfxVideoParam *par, CameraParams *PipeParams)
    {
        par; PipeParams;
        MFX_CHECK_NULL_PTR1(PipeParams);

        // TODO: implement reset for SW

        return MFX_ERR_NONE;
    }

    virtual mfxStatus Close()
    {
        FreeCamera_CPU(&m_cmi);
        return MFX_ERR_NONE;
    }

    virtual mfxStatus AsyncRoutine(AsyncParams *pParam)
    {
        UMC::AutomaticUMCMutex guard(m_guard);

        mfxFrameSurface1 *surfIn  = pParam->surf_in;
        mfxFrameSurface1 *surfOut = pParam->surf_out;

        /* input data  in surfIn->Data.Y16 (16-bit)
         * output data in surfOut->Data.V16 (16-bit) or Data.B (8-bit)
         * params in surfIn->Info
         */
        if (surfOut->Data.MemId)
            m_core->LockExternalFrame(surfOut->Data.MemId,  &surfOut->Data);

        // Step 1. Padding
        // TODO: Add flipping for GB, GR bayers
        if(m_cmi.enable_padd)
        {
            for (int i = 0; i < m_cmi.InHeight; i++)
                for (int j = 0; j < m_cmi.InWidth; j++)
                    m_cmi.cpu_Input[i*m_cmi.InWidth + j] = (short)surfIn->Data.Y16[i*(surfIn->Data.Pitch>>1) + j];
            m_cmi.cpu_status = CPU_Padding_16bpp(m_cmi.cpu_Input, m_cmi.cpu_PaddedBayerImg, m_cmi.InWidth, m_cmi.InHeight, m_cmi.bitDepth);

            if(m_Params.Caps.BayerPatternType == GRBG || m_Params.Caps.BayerPatternType == GBRG)
            {
                m_cmi.cpu_status = CPU_Bufferflip(m_cmi.cpu_PaddedBayerImg, m_cmi.FrameWidth, m_cmi.FrameHeight, m_cmi.bitDepth, false);
            }
        }
        else
        {
            for (int i = 0; i < m_cmi.FrameHeight; i++)
                for (int j = 0; j < m_cmi.FrameWidth; j++)
                    m_cmi.cpu_PaddedBayerImg[i*m_cmi.FrameWidth + j] = (surfIn->Data.Y16[i*(surfIn->Data.Pitch>>1) + j] >> (16 - m_cmi.bitDepth));
        }

        // Step 2. Black level correction
        if(m_Params.Caps.bBlackLevelCorrection)
        {
            m_cmi.cpu_status = CPU_BLC(m_cmi.cpu_PaddedBayerImg, m_cmi.FrameWidth, m_cmi.FrameHeight,
                             m_cmi.bitDepth, m_cmi.BayerType,
                             (short)pParam->BlackLevelParams.BlueLevel,        (short)pParam->BlackLevelParams.GreenTopLevel,
                             (short)pParam->BlackLevelParams.GreenBottomLevel, (short)pParam->BlackLevelParams.RedLevel,
                             false);
        }

        // Step 3. White balance
        if(m_Params.Caps.bWhiteBalance)
        {
            m_cmi.cpu_status = CPU_WB (m_cmi.cpu_PaddedBayerImg, m_cmi.FrameWidth, m_cmi.FrameHeight,
                                       m_cmi.bitDepth, m_cmi.BayerType,
                                       (float)pParam->WBparams.BlueCorrection,        (float)pParam->WBparams.GreenTopCorrection,
                                       (float)pParam->WBparams.GreenBottomCorrection, (float)pParam->WBparams.RedCorrection,
                                       false);
        }

        // Step 4. Demosaic
        m_cmi.cpu_status = Demosaic_CPU(&m_cmi, surfIn->Data.Y16, surfIn->Data.Pitch);

        if (m_Params.Caps.BayerPatternType == RGGB || m_Params.Caps.BayerPatternType == GBRG)
        {
            /* swap R and B buffers */
            m_cmi.cpu_R_fgc_in = m_cmi.cpu_B_o;
            m_cmi.cpu_G_fgc_in = m_cmi.cpu_G_o;
            m_cmi.cpu_B_fgc_in = m_cmi.cpu_R_o;
        }
        else
        {
            m_cmi.cpu_R_fgc_in = m_cmi.cpu_R_o;
            m_cmi.cpu_G_fgc_in = m_cmi.cpu_G_o;
            m_cmi.cpu_B_fgc_in = m_cmi.cpu_B_o;
        }

        // Step 5. CMM
        if (m_Params.Caps.bColorConversionMatrix)
        {
            m_cmi.cpu_status = CPU_CCM( (unsigned short*)m_cmi.cpu_R_fgc_in, (unsigned short*)m_cmi.cpu_G_fgc_in, (unsigned short*)m_cmi.cpu_B_fgc_in,
                m_Params.TileInfo.CropW, m_Params.TileInfo.CropH, m_Params.BitDepth, pParam->CCMParams.CCM);
        }

        // Step 6. Gamma correction
        if (m_Params.Caps.bForwardGammaCorrection)
        {
            m_cmi.cpu_status = CPU_Gamma_SKL( (unsigned short*)m_cmi.cpu_R_fgc_in, (unsigned short*)m_cmi.cpu_G_fgc_in, (unsigned short*)m_cmi.cpu_B_fgc_in,
                                             m_Params.TileInfo.CropW, m_Params.TileInfo.CropH, m_Params.BitDepth,
                                             pParam->GammaParams.Segment);

        }
        if (m_Params.TileInfo.FourCC == MFX_FOURCC_ARGB16)
            m_cmi.cpu_status = CPU_ARGB16Interleave(surfOut->Data.V16, surfOut->Info.CropW, surfOut->Info.CropH, surfOut->Data.Pitch,
                                                    m_Params.BitDepth, m_Params.Caps.BayerPatternType,
                                                    m_cmi.cpu_R_fgc_in, m_cmi.cpu_G_fgc_in, m_cmi.cpu_B_fgc_in);
        else
            m_cmi.cpu_status = CPU_ARGB8Interleave(surfOut->Data.B, surfOut->Info.CropW, surfOut->Info.CropH, surfOut->Data.Pitch,
                                                   m_Params.BitDepth, m_Params.Caps.BayerPatternType,
                                                   m_cmi.cpu_R_fgc_in, m_cmi.cpu_G_fgc_in, m_cmi.cpu_B_fgc_in);

        if (surfOut->Data.MemId)
            m_core->UnlockExternalFrame(surfOut->Data.MemId,  &surfOut->Data);

        return (mfxStatus)m_cmi.cpu_status;
    }
    virtual mfxStatus CompleteRoutine(AsyncParams * /*pParams*/)
    {
        return MFX_ERR_NONE;
    }

protected:
    virtual mfxStatus CheckIOPattern(mfxU16  /*IOPattern*/) { return MFX_ERR_NONE; };

    int InitCamera_CPU(CamInfo *dmi, int InWidth, int InHeight, int BitDepth, bool enable_padd, int BayerType);
    void FreeCamera_CPU(CamInfo *dmi);

    int CPU_Padding_16bpp(unsigned short* bayer_original, //input  of Padding module, bayer image of 16bpp format
                          unsigned short* bayer_input,    //output of Padding module, Paddded bayer image of 16bpp format
                          int width,                      //input framewidth
                          int height,                     //input frameheight
                          int bitDepth);

    int CPU_BLC(unsigned short* Bayer,
                int PaddedWidth, int PaddedHeight, int bitDepth, int BayerType,
                short B_amount, short Gtop_amount, short Gbot_amount, short R_amount, bool firstflag);

    int CPU_WB(unsigned short* Bayer,
               int PaddedWidth, int Paddedheight, int bitDepth, int BayerType,
               float B_scale, float Gtop_scale, float Gbot_scale, float R_scale, bool firstflag);

    int Demosaic_CPU(CamInfo *dmi, unsigned short *BayerImg, mfxU16 pitch);

    int CPU_CCM(unsigned short* R_i, unsigned short* G_i, unsigned short* B_i,
                int framewidth, int frameheight, int bitDepth, mfxF64 CCM[3][3]);

    int CPU_Gamma_SKL(unsigned short* R_i, unsigned short* G_i, unsigned short* B_i,
                      int framewidth, int frameheight, int bitDepth,
                      mfxCamFwdGammaSegment* gamma_segment);          // Pointer to 64 Words

    int CPU_ARGB8Interleave(unsigned char* ARGB8, int OutWidth, int OutHeight, int OutPitch, int BitDepth, int BayerType,
                            short* R, short* G, short* B);

    int CPU_ARGB16Interleave(unsigned short* ARGB16, int OutWidth, int OutHeight, int OutPitch, int BitDepth, int BayerType,
                             short* R, short* G, short* B);
    int CPU_Bufferflip(unsigned short* buffer,
                       int width, int height, int bitdepth, bool firstflag);

    int CPU_VIG(unsigned short* Bayer,
            unsigned short* Mask,
            int PaddedWidth, int PaddedHeight, int bitDepth, bool firstflag);

    int CPU_GoodPixelCheck(unsigned short *pSrcBayer16bpp, int m_Width, int m_Height,
                       unsigned char *pDstFlag8bpp, int Diff_Prec, bool , int bitshiftamount, bool firstflag);

    int CPU_RestoreG(unsigned short *m_Pin,         //Pointer to Bayer data
                 int m_Width, int m_Height,
                  short *m_Pout_H__m_G,
                 short *m_Pout_V__m_G,
                 short *m_Pout_G__m_G,
                 int Max_Intensity);

    int CPU_RestoreBandR(unsigned short *m_Pin,         //Pointer to Bayer data
                     int m_Width, int m_Height,
                     short *m_Pout_H__m_G, short *m_Pout_V__m_G, short *m_Pout_G__m_G,  //Input,  G_H, G_V, G_A component
                     short *m_Pout_H__m_B, short *m_Pout_V__m_B, short *m_Pout_G__m_B,  //Output, B_H, B_V, B_A    component
                     short *m_Pout_H__m_R, short *m_Pout_V__m_R, short *m_Pout_G__m_R,  //Output, R_H, R_V, R_A component
                     int Max_Intensity);

    int CPU_SAD (short *m_Pout_H__m_G, short *m_Pout_V__m_G, //Input, G_H, G_V component
             short *m_Pout_H__m_B, short *m_Pout_V__m_B, //Input, B_H, B_V component
             short *m_Pout_H__m_R, short *m_Pout_V__m_R,
             int m_Width, int m_Height,
             short *m_Pout_O__m_G,
             short *m_Pout_O__m_B,
             short *m_Pout_O__m_R);

    int CPU_Decide_Average(short *R_A8,       // Restored average version of R component
                       short *G_A8,          // Restored average version of G component
                       short *B_A8,       // Restored average version of B component
                       int m_Width, int m_Height,
                       unsigned char *A,
                       unsigned char *updated_A,
                       short *m_Pout_O__m_R,
                       short *m_Pout_O__m_G,
                       short *m_Pout_O__m_B,
                       int bitdepth);

private:
    UMC::Mutex             m_guard;
    CamInfo                m_cmi;
    CameraParams           m_Params;
};

#endif  /* _MFX_CAMERA_PLUGIN_CPU_H */
