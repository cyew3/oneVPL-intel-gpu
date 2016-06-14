/* /////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2008-2016 Intel Corporation. All Rights Reserved.
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
#elif defined (MFX_VA_LINUX)
#include <va/va.h>
#include <va/va_enc_jpeg.h>

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

    typedef struct {
        mfxU16 header;
        mfxU8  lenH;
        mfxU8  lenL;
        mfxU8  s[5];
        mfxU8  versionH;
        mfxU8  versionL;
        mfxU8  units;
        mfxU16 xDensity;
        mfxU16 yDensity;
        mfxU8  xThumbnails;
        mfxU8  yThumbnails;
    } JpegApp0Data;

    typedef struct {
        mfxU16 header;
        mfxU8  lenH;
        mfxU8  lenL;
        mfxU8  s[5];
        mfxU8  versionH;
        mfxU8  versionL;
        mfxU8  flags0H;
        mfxU8  flags0L;
        mfxU8  flags1H;
        mfxU8  flags1L;
        mfxU8  transform;
    } JpegApp14Data;

    typedef struct {
        mfxU8 * data;
        mfxU32  length;
        mfxU32  maxLength;
    } JpegPayload;

    mfxStatus QueryHwCaps(
        VideoCORE * core,
        JpegEncCaps & hwCaps);

    bool IsJpegParamExtBufferIdSupported(
        mfxU32 id);

    mfxStatus CheckExtBufferId(
        mfxVideoParam const & par);

    mfxStatus CheckJpegParam(
        VideoCORE         * core,
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
            memset(&m_pps, 0, sizeof(m_pps));
            memset(&m_payload_base, 0, sizeof(m_payload_base));
            memset(&m_app0_data, 0, sizeof(m_app0_data));
            memset(&m_app14_data, 0, sizeof(m_app14_data));
        }

        mfxStatus Init(mfxVideoParam const *par, mfxEncodeCtrl const * ctrl, JpegEncCaps const * hwCaps);
        void      Close();

#if defined (MFX_VA_WIN)
        ENCODE_SET_PICTURE_PARAMETERS_JPEG           m_pps;
        std::vector<ENCODE_SET_SCAN_PARAMETERS_JPEG> m_scan_list;
        std::vector<ENCODE_QUANT_TABLE_JPEG>         m_dqt_list;
        std::vector<ENCODE_HUFFMAN_TABLE_JPEG>       m_dht_list;

#elif defined (MFX_VA_LINUX)
        VAEncPictureParameterBufferJPEG               m_pps;
        std::vector<VAEncSliceParameterBufferJPEG>    m_scan_list;
        std::vector<VAQMatrixBufferJPEG>              m_dqt_list;
        std::vector<VAHuffmanTableBufferJPEGBaseline> m_dht_list;

#endif
        std::vector<JpegPayload>                      m_payload_list;
        JpegPayload                                   m_payload_base;
        JpegApp0Data                                  m_app0_data;
        JpegApp14Data                                 m_app14_data;
    };

    typedef struct {
        mfxFrameSurface1 * surface;              // input raw surface
        mfxBitstream     * bs;                   // output bitstream
        mfxU32             m_idx;                // index of raw surface
        mfxU32             m_idxBS;              // index of bitstream surface (always equal as m_idx for now)
        mfxU32             lInUse;               // 0: free, 1: used.
        mfxU32             m_statusReportNumber;
        mfxU32             m_bsDataLength;       // output bitstream length
        bool               m_cleanDdiData;
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
