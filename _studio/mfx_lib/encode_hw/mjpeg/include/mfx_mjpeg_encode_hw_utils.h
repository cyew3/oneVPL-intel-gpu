/* /////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2008-2014 Intel Corporation. All Rights Reserved.
//
//
*/

#include "mfx_common.h"
#if defined (MFX_ENABLE_MJPEG_VIDEO_ENCODE) && defined(MFX_VA_WIN)

#ifndef __MFX_MJPEG_ENCODE_HW_UTILS_H__
#define __MFX_MJPEG_ENCODE_HW_UTILS_H__
#include <d3d9.h>
#include <dxva.h>
#include <dxva2api.h>

#define D3DDDIFORMAT        D3DFORMAT
#define DXVADDI_VIDEODESC   DXVA2_VideoDesc

#include "encoder_ddi.hpp"
#include "mfxstructures.h"

#include <vector>
#include <list>
#include <assert.h>

#include "auxiliary_device.h"
#include "mfxstructures.h"
#include "mfxjpeg.h"

#define MFX_CHECK_WITH_ASSERT(EXPR, ERR) { assert(EXPR); MFX_CHECK(EXPR, ERR); }

namespace MfxHwMJpegEncode
{
    #define  JPEG_VIDEO_SURFACE_NUM    4
    #define  JPEG_DDITASK_MAX_NUM      32

    enum
    {
        MFX_MEMTYPE_D3D_INT = MFX_MEMTYPE_FROM_ENCODE | MFX_MEMTYPE_DXVA2_DECODER_TARGET | MFX_MEMTYPE_INTERNAL_FRAME,
        MFX_MEMTYPE_D3D_EXT = MFX_MEMTYPE_FROM_ENCODE | MFX_MEMTYPE_DXVA2_DECODER_TARGET | MFX_MEMTYPE_EXTERNAL_FRAME
    };

    mfxStatus QueryHwCaps(
        eMFXVAType         va_type,
        mfxU32             adapterNum,
        ENCODE_CAPS_JPEG & hwCaps);

    bool IsJpegParamExtBufferIdSupported(
        mfxU32 id);

    mfxStatus CheckExtBufferId(
        mfxVideoParam const & par);

    mfxStatus CheckJpegParam(
        mfxVideoParam          & par,
        ENCODE_CAPS_JPEG const & hwCaps,
        bool                     setExtAlloc);

    mfxStatus FastCopyFrameBufferSys2Vid(
        VideoCORE    * core,
        mfxMemId       vidMemId,
        mfxFrameData & sysSurf,
        mfxFrameInfo & frmInfo
        );

    struct ExecuteBuffers
    {
        ExecuteBuffers()
        {
            memset(&m_pps, 0, sizeof(m_pps));
            memset(&m_payload, 0, sizeof(m_payload));

            m_payload_data_present = false;
        }

        mfxStatus Init(mfxVideoParam const *par);
        void      Close();

        ENCODE_CAPS_JPEG     m_caps;

        ENCODE_SET_PICTURE_PARAMETERS_JPEG           m_pps;
        std::vector<ENCODE_SET_SCAN_PARAMETERS_JPEG> m_scan_list;
        std::vector<ENCODE_QUANT_TABLE_JPEG>         m_dqt_list;
        std::vector<ENCODE_HUFFMAN_TABLE_JPEG>       m_dht_list;

        struct {
            mfxU8 * data;
            mfxU16  reserved;
            mfxU16  size;
        } m_payload;

        bool m_payload_data_present;
    };

    typedef struct {
        mfxFrameSurface1 * surface;              // input raw surface
        mfxBitstream     * bs;                   // output bitstream
        mfxU32             m_idx;                // index of raw surface
        mfxU32             m_idxBS;              // index of bitstream surface (always equal as m_idx for now)
        mfxL32             lInUse;               // 0: free, 1: used.
        mfxU32             m_statusReportNumber;
        mfxU32             m_bsDataLength;       // output bitstream length
        ExecuteBuffers   * m_pDdiData;
    } DdiTask;

    class TaskManager
    {
    public:
        TaskManager();
        ~TaskManager();

        mfxStatus Init(mfxU32 maxTaskNum);
        mfxStatus Reset();
        mfxStatus Close();

        mfxStatus AssignTask(DdiTask *& newTask);
        mfxStatus RemoveTask(DdiTask & task);
    private:
        DdiTask * m_pTaskList;
        mfxU32    m_TaskNum;
    };

    class CachedFeedback
    {
    public:
        typedef ENCODE_QUERY_STATUS_PARAMS Feedback;
        typedef std::vector<Feedback> FeedbackStorage;

        void Reset(mfxU32 cacheSize);

        void Update(FeedbackStorage const & update);

        const Feedback * Hit(mfxU32 feedbackNumber) const;

        void Remove(mfxU32 feedbackNumber);

    private:
        FeedbackStorage m_cache;
    };

}; // namespace MfxHwMJpegEncode

#endif // __MFX_MJPEG_ENCODE_HW_UTILS_H__
#endif // MFX_ENABLE_MJPEG_VIDEO_ENCODE
