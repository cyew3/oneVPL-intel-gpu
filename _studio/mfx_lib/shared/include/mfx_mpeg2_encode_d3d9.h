//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2011-2012 Intel Corporation. All Rights Reserved.
//

#ifndef __MFX_MPEG2_ENCODE_D3D9__H
#define __MFX_MPEG2_ENCODE_D3D9__H

#include "mfx_common.h"

#if defined(MFX_VA)
#if defined(MFX_ENABLE_MPEG2_VIDEO_ENCODE) || defined(MFX_ENABLE_MPEG2_VIDEO_ENC)

#include <vector>
#include <assert.h>

#include "encoding_ddi.h"
#include "auxiliary_device.h"

#include "mfx_ext_buffers.h"
#include "mfxpcp.h"

#include "mfx_mpeg2_enc_common_hw.h"
#include "mfx_mpeg2_encode_interface.h"

namespace MfxHwMpeg2Encode
{
    class D3D9Encoder : public DriverEncoder
    {
    public:
        explicit D3D9Encoder(VideoCORE* core);

        virtual ~D3D9Encoder();
        virtual mfxStatus QueryEncodeCaps(ENCODE_CAPS & caps);
        
        virtual mfxStatus Init(ExecuteBuffers* pExecuteBuffers, mfxU32 numRefFrames, mfxU32 funcId);                 

        virtual mfxStatus Execute(ExecuteBuffers* pExecuteBuffers, mfxU8* pUserData = 0, mfxU32 userDataLen = 0);                

        virtual mfxStatus Close();

        virtual bool      IsFullEncode() const { return m_bENC_PAK; }

        virtual mfxStatus RegisterRefFrames(const mfxFrameAllocResponse* pResponse);

        virtual mfxStatus FillMBBufferPointer(ExecuteBuffers* pExecuteBuffers);

        virtual mfxStatus FillBSBuffer(mfxU32 nFeedback,mfxU32 nBitstream, mfxBitstream* pBitstream, Encryption *pEncrypt);

        virtual mfxStatus SetFrames (ExecuteBuffers* pExecuteBuffers);
    
    private:

        // Declare private copy constructor to avoid accidental assignment
        // and klocwork complaining.
        D3D9Encoder(const D3D9Encoder &);
        D3D9Encoder & operator = (const D3D9Encoder &);

        mfxStatus Init_MPEG2_ENC(ExecuteBuffers* pExecuteBuffers, mfxU32 numRefFrames);         
        mfxStatus Init_MPEG2_ENCODE (ExecuteBuffers* pExecuteBuffers, mfxU32 numRefFrames);

        mfxStatus Execute_ENC    (ExecuteBuffers* pExecuteBuffers, mfxU8 *pUserData = 0, mfxU32 userDataLen = 0);        
        mfxStatus Execute_ENCODE (ExecuteBuffers* pExecuteBuffers, mfxU8 *pUserData, mfxU32 userDataLen);                

        mfxStatus QueryCompBufferInfo(D3DDDIFORMAT type,mfxFrameAllocRequest* pRequest);
        mfxStatus CreateCompBuffers  (ExecuteBuffers* pExecuteBuffers, mfxU32 numRefFrames);
        mfxStatus CreateMBDataBuffer  (mfxU32 numRefFrames);
        mfxStatus CreateBSBuffer      (mfxU32 numRefFrames);
        //mfxStatus CreateBSBuffer      (mfxU32 numRefFrames);
        mfxStatus GetBuffersInfo();
        mfxStatus QueryMbDataLayout();
        mfxStatus Init(const GUID& guid, ENCODE_FUNC func, ExecuteBuffers* pExecuteBuffers);
        mfxStatus Execute(ExecuteBuffers* pExecuteBuffers, mfxU32 func, mfxU8* pUserData, mfxU32 userDataLen);        
        mfxStatus Register (const mfxFrameAllocResponse* pResponse, D3DDDIFORMAT type);
        mfxI32    GetRecFrameIndex (mfxMemId memID);
        mfxI32    GetRawFrameIndex (mfxMemId memIDe, bool bAddFrames);    


        VideoCORE*                          m_core;
        AuxiliaryDevice*                    m_pDevice;

        std::vector<ENCODE_COMP_BUFFER_INFO> m_compBufInfo;
        std::vector<D3DDDIFORMAT> m_uncompBufInfo;
        ENCODE_MBDATA_LAYOUT m_layout;
        mfxFeedback                             m_feedback;
    
        mfxFrameAllocResponse               m_allocResponseMB;
        mfxFrameAllocResponse               m_allocResponseBS;
        mfxRecFrames                        m_recFrames;
        mfxRawFrames                        m_rawFrames;
        bool                                m_bENC_PAK;

#ifdef MPEG2_ENC_HW_PERF
        vm_time lock_MB_data_time[3];
        vm_time copy_MB_data_time[3];
#endif 
    };
};

#endif
#endif //#if defined (MFX_ENABLE_H264_VIDEO_ENCODE) && defined (MFX_VA_WIN)
#endif //#ifndef __MFX_MPEG2_ENCODE_D3D9__H
/* EOF */
