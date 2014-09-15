/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2012-2014 Intel Corporation. All Rights Reserved.
//
*/
#include "mfx_common.h"
#if defined(MFX_ENABLE_VP8_VIDEO_ENCODE_HW) && defined(MFX_VA)

#ifndef _MFX_VP8_ENCODE_DDI_HW_H_
#define _MFX_VP8_ENCODE_DDI_HW_H_

#include <vector>
#include "mfx_vp8_encode_utils_hw.h"
#include "mfx_platform_defs.h"

#if defined (MFX_VA_LINUX)
#include "mfx_vp8_vaapi_ext.h"
#endif

namespace MFX_VP8ENC
{

#if defined (MFX_VA_LINUX)
    typedef struct tagENCODE_CAPS_VP8 
    { 
        union { 
            struct { 
                UINT CodingLimitSet      : 1; 
                UINT Color420Only        : 1; 
                UINT SegmentationAllowed : 1; 
                UINT CoeffPartitionLimit : 2; 
                UINT FrameLevelRateCtrl  : 1; 
                UINT BRCReset            : 1; 
                UINT                     : 25; 
            }; 
            UINT CodingLimits;
        }; 

        union { 
            struct { 
                BYTE EncodeFunc    : 1; 
                BYTE HybridPakFunc : 1; // Hybrid Pak function for BDW 
                BYTE               : 6; 
            }; 
            BYTE CodingFunction; 
        }; 

        UINT MaxPicWidth; 
        UINT MaxPicHeight; 
    } ENCODE_CAPS_VP8;

    typedef struct
    {
        SHORT  x;
        SHORT  y;
    } ENCODE_MV_DATA;
#endif

    typedef VAEncMbDataLayout MBDATA_LAYOUT;

    class DriverEncoder;

    mfxStatus QueryHwCaps(mfxCoreInterface * pCore, ENCODE_CAPS_VP8 & caps);
    mfxStatus CheckVideoParam(mfxVideoParam const & par, ENCODE_CAPS_VP8 const &caps);

    DriverEncoder* CreatePlatformVp8Encoder();

    class TaskHybridDDI;

    class DriverEncoder
    {
    public:

        virtual ~DriverEncoder(){}

        virtual mfxStatus CreateAuxilliaryDevice(
                        mfxCoreInterface * pCore,
                        GUID               guid,
                        mfxU32             width,
                        mfxU32             height) = 0;

        virtual
        mfxStatus CreateAccelerationService(
            mfxVideoParam const & par) = 0;

        virtual
        mfxStatus Reset(
            mfxVideoParam const & par) = 0;

        virtual 
        mfxStatus Register(
            mfxFrameAllocResponse & response,
            D3DDDIFORMAT            type) = 0;

        virtual 
        mfxStatus Execute(
            TaskHybridDDI const &task, 
            mfxHDL surface) = 0;

        virtual
        mfxStatus QueryCompBufferInfo(
            D3DDDIFORMAT           type,
            mfxFrameAllocRequest & request,
            mfxU32 frameWidth,
            mfxU32 frameHeight) = 0;

        virtual
        mfxStatus QueryEncodeCaps(
            ENCODE_CAPS_VP8 & caps) = 0;

        virtual
        mfxStatus QueryMBLayout(
            MBDATA_LAYOUT &layout) = 0;

        virtual
        mfxStatus QueryStatus( 
            Task & task ) = 0;

        virtual
            mfxU32 GetReconSurfFourCC() = 0;

        virtual
        mfxStatus Destroy() = 0;
    };

#if defined (MFX_VA_LINUX)

#include <va/va_enc_vp8.h>
    
#define MFX_DESTROY_VABUFFER(vaBufferId, vaDisplay)    \
do {                                               \
    if ( vaBufferId != VA_INVALID_ID)              \
    {                                              \
        vaDestroyBuffer(vaDisplay, vaBufferId);    \
        vaBufferId = VA_INVALID_ID;                \
    }                                              \
} while (0)

#define D3DDDIFMT_INTELENCODE_BITSTREAMDATA     (D3DDDIFORMAT)164
#define D3DDDIFMT_INTELENCODE_MBDATA            (D3DDDIFORMAT)165
#define D3DDDIFMT_INTELENCODE_SEGMENTMAP        (D3DDDIFORMAT)178
#define D3DDDIFMT_INTELENCODE_COEFFPROB         (D3DDDIFORMAT)179
#define D3DDDIFMT_INTELENCODE_DISTORTIONDATA    (D3DDDIFORMAT)180

    enum {
        MFX_FOURCC_VP8_NV12    = MFX_MAKEFOURCC('V','P','8','N'),
        MFX_FOURCC_VP8_MBDATA  = MFX_MAKEFOURCC('V','P','8','M'),
        MFX_FOURCC_VP8_SEGMAP  = MFX_MAKEFOURCC('V','P','8','S'),
    };

    /* Convert MediaSDK into DDI */

    void FillSpsBuffer(mfxVideoParam const & par, 
        VAEncSequenceParameterBufferVP8 & sps);

    mfxStatus FillPpsBuffer(Task const &TaskHybridDDI, mfxVideoParam const & par, 
        VAEncPictureParameterBufferVP8 & pps);

    mfxStatus FillQuantBuffer(Task const &TaskHybridDDI, mfxVideoParam const & par,
        VAQMatrixBufferVP8 & quant);

    struct sDDI_MBDATA
    { 
        /* DWORD  0 */

        mfxU32 mbx                              :   10;
        mfxU32 mby                              :   10;

        mfxU32 mb_luma_coeff_skip               :   1;
        /* 
        0: some of the Luma Quantized coefficient are non-zero in a Macroblock
        1: all the luma coefficients in a Macroblock is zero 
        */

        mfxU32 is_inter_mb                      :  1;
        /* 
        0: MB should be intra coded
        1: MB should be inter coded 
        */

        mfxU32 mb_ref_frame_sel                 :  2;
        /*  
        0: Intra Frame
        1: Last Frame
        2: Golden Frame
        3: Alternate Reference frame 
        */

        mfxU32 mv_mode                          : 3;
        /*  
        0:NEARESTMV
        1:NEARMV
        2:ZEROMV
        3:NEWMV
        4:SPLITMV */

        mfxU32 intra_y_mode_partition_type      : 3;
        
        /* for Intra Luma 16x16           
        0: DC prediction
        1: Vertical prediction
        2: Horizontal Prediction
        3: True motion prediction
        4: Block Prediction*/

        /*for inter MB
        0: 16x16
        1: 16x8
        2: 8x16
        3: 8x8
        4:  4x4*/

        mfxU32 intra_uv_mode                    : 2;  
        /*  
        0: DC prediction
        1: Vertical prediction
        2: Horizontal Prediction
        3: True motion prediction*/        

         
         /* DWORD 1*/

         mfxU32 intra_b_mode_sub_mv_mode_0      :    4;
         mfxU32 intra_b_mode_sub_mv_mode_1      :    4;
         mfxU32 intra_b_mode_sub_mv_mode_2      :    4;
         mfxU32 intra_b_mode_sub_mv_mode_3      :    4;
         mfxU32 intra_b_mode_sub_mv_mode_4      :    4;
         mfxU32 intra_b_mode_sub_mv_mode_5      :    4;
         mfxU32 intra_b_mode_sub_mv_mode_6      :    4;
         mfxU32 intra_b_mode_sub_mv_mode_7      :    4;

          /* DWORD 2*/

         mfxU32 intra_b_mode_sub_mv_mode_8      :    4;
         mfxU32 intra_b_mode_sub_mv_mode_9      :    4;
         mfxU32 intra_b_mode_sub_mv_mode_10     :    4;
         mfxU32 intra_b_mode_sub_mv_mode_11     :    4;
         mfxU32 intra_b_mode_sub_mv_mode_12     :    4;
         mfxU32 intra_b_mode_sub_mv_mode_13     :    4;
         mfxU32 intra_b_mode_sub_mv_mode_14     :    4;
         mfxU32 intra_b_mode_sub_mv_mode_15     :    4;


         /* Intra Luma 4x4 mode in intra macroblock for  block  (intra_b_mode)
            
            0:B_DC_PRED
            1:B_TM_PRED
            2:B_VE_PRED
            3:B_HE_PRED
            4:B_LD_PRED
            5:B_RD_PRED
            6:B_VR_PRED
            7:B_VL_PRED
            8:B_HD_PRED
            9:B_HU_PRED

         sub block motion vector mode in inter macroblock for block(sub_mv_mode)
            
            0: LEFT4x4_MV
            1: ABOVE4x4_MV
            2: ZERO4x4_MV
            3: NEW4x4_MV*/

         mfxU32 Padding;


         mfxI16 Y1block_0[16];
         mfxI16 Y1block_1[16];
         mfxI16 Y1block_2[16];
         mfxI16 Y1block_3[16];
         mfxI16 Y1block_4[16];
         mfxI16 Y1block_5[16];
         mfxI16 Y1block_6[16];
         mfxI16 Y1block_7[16];
         mfxI16 Y1block_8[16];
         mfxI16 Y1block_9[16];
         mfxI16 Y1block_10[16];
         mfxI16 Y1block_11[16];
         mfxI16 Y1block_12[16];
         mfxI16 Y1block_13[16];
         mfxI16 Y1block_14[16];
         mfxI16 Y1block_15[16];

         mfxI16 Ublock_0[16];
         mfxI16 Ublock_1[16];
         mfxI16 Ublock_2[16];
         mfxI16 Ublock_3[16];


         mfxI16 Vblock_0[16];
         mfxI16 Vblock_1[16];
         mfxI16 Vblock_2[16];
         mfxI16 Vblock_3[16];

         mfxI16 Y2block[16];
    };
    typedef struct
    {
        VASurfaceID surface;
        mfxU32 number;
        mfxU32 idxBs;

    } ExtVASurface;

    class VAAPIEncoder : public DriverEncoder
    {
    public:
        VAAPIEncoder();

        virtual
        ~VAAPIEncoder();

        virtual
        mfxStatus CreateAuxilliaryDevice(
            mfxCoreInterface* core,
            GUID       guid,
            mfxU32     width,
            mfxU32     height);

        virtual
        mfxStatus CreateAccelerationService(
            mfxVideoParam const & par);

        virtual
        mfxStatus Reset(
            mfxVideoParam const & par);

        // empty  for Lin
        virtual
        mfxStatus Register(
            mfxMemId memId,
            D3DDDIFORMAT type);

        // 2 -> 1
        virtual
        mfxStatus Register(
            mfxFrameAllocResponse& response,
            D3DDDIFORMAT type);

        // (mfxExecuteBuffers& data)
        virtual
        mfxStatus Execute(
            TaskHybridDDI const &task, 
            mfxHDL surface);

        // recomendation from HW
        virtual
        mfxStatus QueryCompBufferInfo(
            D3DDDIFORMAT type,
            mfxFrameAllocRequest& request,
            mfxU32 frameWidth,
            mfxU32 frameHeight);

        virtual
        mfxStatus QueryEncodeCaps(
            ENCODE_CAPS_VP8& caps);

        virtual
        mfxStatus QueryMBLayout(
            MBDATA_LAYOUT &layout);

        virtual
        mfxStatus QueryStatus(
            Task & task);

        virtual
            mfxU32 GetReconSurfFourCC();

        virtual
        mfxStatus Destroy();

    private:
        VAAPIEncoder(const VAAPIEncoder&); // no implementation
        VAAPIEncoder& operator=(const VAAPIEncoder&); // no implementation

        mfxCoreInterface * m_pmfxCore;
        VP8MfxParam   m_video;
        MBDATA_LAYOUT m_layout;

        // encoder specific. can be encapsulated by auxDevice class
        VADisplay    m_vaDisplay;
        VAContextID  m_vaContextEncode;
        VAConfigID   m_vaConfig;

        // encode params (extended structures)
        VAEncSequenceParameterBufferVP8        m_sps;
        VAEncPictureParameterBufferVP8         m_pps;
        VAQMatrixBufferVP8                     m_quant;
        VAEncMiscParameterVP8HybridFrameUpdate m_frmUpdate;
        VAEncMiscParameterVP8SegmentMapParams  m_segMapPar;
        VAEncMiscParameterFrameRate            m_frameRate;

        // encode buffer to send vaRender()
        VABufferID m_spsBufferId;
        VABufferID m_ppsBufferId;
        VABufferID m_qMatrixBufferId;
        VABufferID m_frmUpdateBufferId;
        VABufferID m_segMapParBufferId;
        VABufferID m_frameRateBufferId;
        VABufferID m_rateCtrlBufferId;
        VABufferID m_hrdBufferId;
      
        std::vector<ExtVASurface> m_feedbackCache;
        std::vector<ExtVASurface> m_mbDataQueue;
        std::vector<ExtVASurface> m_reconQueue;
        std::vector<ExtVASurface> m_segMapQueue;
        
        static const mfxU32 MAX_CONFIG_BUFFERS_COUNT = 9; // sps, pps, quant, seg_map, per segment par, frame update data, frame rate, rate ctrl, hrd

        mfxU32 m_width;
        mfxU32 m_height;
        ENCODE_CAPS_VP8 m_caps;
        UMC::Mutex                      m_guard;
    };
#endif


}
#endif
#endif