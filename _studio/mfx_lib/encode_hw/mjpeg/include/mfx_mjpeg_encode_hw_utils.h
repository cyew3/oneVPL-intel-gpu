/* /////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2008-2014 Intel Corporation. All Rights Reserved.
//
*/

#ifndef __MFX_MJPEG_ENCODE_HW_UTILS_H__
#define __MFX_MJPEG_ENCODE_HW_UTILS_H__

#include "mfx_common.h"

#if defined (MFX_ENABLE_MJPEG_VIDEO_ENCODE) && defined (MFX_VA)

#if defined (MFX_VA_WIN)
#include <d3d9.h>
#include <dxva.h>
#include <dxva2api.h>
#include "encoding_ddi.h"
#include "encoder_ddi.hpp"
#endif

#include "mfxstructures.h"

#include <vector>
#include <memory>
#include <assert.h>

#include "vm_interlocked.h"

#include "mfxstructures.h"
#include "mfxjpeg.h"

#define MFX_CHECK_WITH_ASSERT(EXPR, ERR) { assert(EXPR); MFX_CHECK(EXPR, ERR); }

namespace MfxHwMJpegEncode
{
    #define  JPEG_VIDEO_SURFACE_NUM    4
    #define  JPEG_DDITASK_MAX_NUM      32

    enum
    {
        MFX_MEMTYPE_VIDEO_INT = MFX_MEMTYPE_FROM_ENCODE | MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET | MFX_MEMTYPE_INTERNAL_FRAME,
        MFX_MEMTYPE_VIDEO_EXT = MFX_MEMTYPE_FROM_ENCODE | MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET | MFX_MEMTYPE_EXTERNAL_FRAME
    };

    typedef struct {
        mfxU32    Baseline;
        mfxU32    Sequential;
        mfxU32    Huffman;

        mfxU32    NonInterleaved;
        mfxU32    Interleaved;

        mfxU32    MaxPicWidth;
        mfxU32    MaxPicHeight;

        mfxU32    SampleBitDepth;
        mfxU32    MaxNumComponent;
        mfxU32    MaxNumScan;
        mfxU32    MaxNumHuffTable; 
        mfxU32    MaxNumQuantTable;
    } JpegEncCaps;

    mfxStatus QueryHwCaps(
        eMFXVAType    va_type,
        mfxU32        adapterNum,
        JpegEncCaps & hwCaps);

    bool IsJpegParamExtBufferIdSupported(
        mfxU32 id);

    mfxStatus CheckExtBufferId(
        mfxVideoParam const & par);

    mfxStatus CheckJpegParam(
        mfxVideoParam     & par,
        JpegEncCaps const & hwCaps);

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
#if defined (MFX_VA_WIN)
            memset(&m_pps, 0, sizeof(m_pps));
#endif
            memset(&m_payload, 0, sizeof(m_payload));

            m_payload_data_present = false;
        }

        mfxStatus Init(mfxVideoParam const *par);
        void      Close();

#if defined (MFX_VA_WIN)
        ENCODE_SET_PICTURE_PARAMETERS_JPEG           m_pps;
        std::vector<ENCODE_SET_SCAN_PARAMETERS_JPEG> m_scan_list;
        std::vector<ENCODE_QUANT_TABLE_JPEG>         m_dqt_list;
        std::vector<ENCODE_HUFFMAN_TABLE_JPEG>       m_dht_list;

#elif defined (MFX_VA_LINUX)
        // ToDo: structures for Linux

#endif

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
        mfxU32             lInUse;               // 0: free, 1: used.
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

}; // namespace MfxHwMJpegEncode

#endif // #if defined (MFX_ENABLE_MJPEG_VIDEO_ENCODE) && defined (MFX_VA)
#endif // __MFX_MJPEG_ENCODE_HW_UTILS_H__
