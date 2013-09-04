/* /////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2008-2013 Intel Corporation. All Rights Reserved.
//
//
*/

#include "mfx_common.h"
#if defined (MFX_ENABLE_MJPEG_VIDEO_ENCODE) && defined(MFX_VA_WIN)

#include "mfx_mjpeg_encode_hw_utils.h"
#include "libmfx_core_factory.h"
#include "libmfx_core_interface.h"
#include "jpegbase.h"
#include "mfx_enc_common.h"
#include "mfx_mjpeg_encode_interface.h"

using namespace MfxHwMJpegEncode;


mfxStatus MfxHwMJpegEncode::QueryHwCaps(eMFXVAType va_type, mfxU32 adapterNum, ENCODE_CAPS_JPEG & hwCaps)
{
    //Should be replaced with once quering capabs as other encoders do

    // FIXME: remove this when driver starts returning actual encode caps
    hwCaps.Baseline         = 1;
    hwCaps.Sequential       = 1;
    hwCaps.Huffman          = 1;
    hwCaps.NonInterleaved   = 1;
    hwCaps.NonDifferential  = 1;
    hwCaps.MaxPicWidth      = 4096;
    hwCaps.MaxPicHeight     = 4096;
    hwCaps.MaxSamplesPerSecond = 32000;

    std::auto_ptr<VideoCORE> pCore(FactoryCORE::CreateCORE(va_type, adapterNum, 1));

    std::auto_ptr<DriverEncoder> ddi;

    ddi.reset( CreatePlatformMJpegEncoder(pCore.get()) );

    mfxStatus sts = ddi->CreateAuxilliaryDevice(pCore.get(), DXVA2_Intel_Encode_JPEG, 640, 480);
    MFX_CHECK_STS(sts);

    sts = ddi->QueryEncodeCaps(hwCaps);
    MFX_CHECK_STS(sts);

    return MFX_ERR_NONE;
}

bool MfxHwMJpegEncode::IsJpegParamExtBufferIdSupported(mfxU32 id)
{
    return
        id == MFX_EXTBUFF_JPEG_QT             ||
        id == MFX_EXTBUFF_JPEG_HUFFMAN    /*    ||
        id == MFX_EXTBUFF_JPEG_PAYLOAD*/;
}

mfxStatus MfxHwMJpegEncode::CheckExtBufferId(mfxVideoParam const & par)
{
    for (mfxU32 i = 0; i < par.NumExtParam; i++)
    {
        if (par.ExtParam[i] == 0)
            return MFX_ERR_INVALID_VIDEO_PARAM;

        if (!IsJpegParamExtBufferIdSupported(par.ExtParam[i]->BufferId))
            return MFX_ERR_INVALID_VIDEO_PARAM;

        // check if buffer presents twice in video param
        if (GetExtBuffer(
            par.ExtParam + i + 1,
            par.NumExtParam - i - 1,
            par.ExtParam[i]->BufferId) != 0)
        {
            return MFX_ERR_INVALID_VIDEO_PARAM;
        }
    }

    return MFX_ERR_NONE;
}

mfxStatus MfxHwMJpegEncode::CheckJpegParam(mfxVideoParam & par,
                                           ENCODE_CAPS_JPEG const & hwCaps,
                                           bool setExtAlloc)
{
    MFX_CHECK((par.mfx.FrameInfo.Width > 0 &&
        par.mfx.FrameInfo.Width <= (mfxU16)hwCaps.MaxPicWidth &&
        par.mfx.FrameInfo.Height > 0 &&
        par.mfx.FrameInfo.Height < (mfxU16)hwCaps.MaxPicHeight),
        MFX_ERR_INVALID_VIDEO_PARAM);

    MFX_CHECK(par.mfx.CodecId == MFX_CODEC_JPEG, MFX_ERR_INVALID_VIDEO_PARAM);
    MFX_CHECK(par.mfx.FrameInfo.ChromaFormat != 0, MFX_ERR_INVALID_VIDEO_PARAM);
    
    if (par.mfx.Interleaved && !hwCaps.Interleaved)
    {
        return MFX_ERR_UNSUPPORTED;
    }

    if (par.IOPattern == MFX_IOPATTERN_IN_VIDEO_MEMORY)
    {
        MFX_CHECK(setExtAlloc, MFX_ERR_INVALID_VIDEO_PARAM);
    }

    MFX_CHECK(
        ((0 == par.IOPattern) || (par.IOPattern == MFX_IOPATTERN_IN_VIDEO_MEMORY) || (par.IOPattern == MFX_IOPATTERN_IN_OPAQUE_MEMORY) ||(par.Protected == 0)),
        MFX_ERR_INVALID_VIDEO_PARAM);

    if (par.mfx.BufferSizeInKB == 0)
    {
        par.mfx.BufferSizeInKB = (par.mfx.FrameInfo.Width * par.mfx.FrameInfo.Height * 4 + 999) / 1000;
    }

    return MFX_ERR_NONE;
}



mfxStatus MfxHwMJpegEncode::CheckEncodeFrameParam(
    mfxVideoParam const & video,
    mfxEncodeCtrl       * ctrl,
    mfxFrameSurface1    * surface,
    mfxBitstream        * bs,
    bool                  isExternalFrameAllocator)
{
    ctrl;
    mfxStatus checkSts = MFX_ERR_NONE;
    MFX_CHECK_NULL_PTR1(bs);

    if (surface != 0)
    {
        MFX_CHECK((surface->Data.Y == 0) == (surface->Data.UV == 0), MFX_ERR_UNDEFINED_BEHAVIOR);
        MFX_CHECK(surface->Data.PitchLow + ((mfxU32)surface->Data.PitchHigh << 16) < 0x8000, MFX_ERR_UNDEFINED_BEHAVIOR);
        MFX_CHECK(surface->Data.Y != 0 || isExternalFrameAllocator, MFX_ERR_UNDEFINED_BEHAVIOR);

        if (surface->Info.Width != video.mfx.FrameInfo.Width || surface->Info.Height != video.mfx.FrameInfo.Height)
            checkSts = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
    }
    else
    {
        checkSts = MFX_ERR_MORE_DATA;
    }

    return checkSts;
}

mfxStatus MfxHwMJpegEncode::FastCopyFrameBufferSys2Vid(
    VideoCORE    * core,
    mfxMemId       vidMemId,
    mfxFrameData & sysSurf,
    mfxFrameInfo & frmInfo
    )
{
    MFX_CHECK_NULL_PTR1(core);
    mfxFrameData d3dSurf = { 0 };
    mfxStatus sts = MFX_ERR_NONE;

    core->LockFrame(vidMemId, &d3dSurf);
    MFX_CHECK(d3dSurf.Y != 0, MFX_ERR_LOCK_MEMORY);
    core->LockExternalFrame(sysSurf.MemId, &sysSurf);
    MFX_CHECK(sysSurf.Y != 0, MFX_ERR_LOCK_MEMORY);

    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "Copy input (sys->d3d)");
        mfxFrameSurface1 surfSrc = { {0,}, frmInfo, sysSurf };
        mfxFrameSurface1 surfDst = { {0,}, frmInfo, d3dSurf };
        sts = core->DoFastCopy(&surfDst, &surfSrc);
        MFX_CHECK_STS(sts);
    }

    sts = core->UnlockExternalFrame(sysSurf.MemId, &sysSurf);
    MFX_CHECK_STS(sts);
    sts = core->UnlockFrame(vidMemId, &d3dSurf);
    MFX_CHECK_STS(sts);

    return sts;
}

mfxStatus ExecuteBuffers::Init(mfxVideoParam const *par)
{
    mfxStatus sts = MFX_ERR_NONE;

    memset(&m_pps, 0, sizeof(m_pps));
    memset(&m_payload, 0, sizeof(m_payload));
    m_payload_data_present = false;

    // prepare DDI execute buffers
    m_pps.Profile        = 0; // 0 -Baseline, 1 - Extended, 2 - Lossless, 3 - Hierarchical
    m_pps.Progressive    = 0;
    m_pps.Huffman        = 1;
    m_pps.Interleaved    = (par->mfx.Interleaved != 0);
    m_pps.Differential   = 0;
    // PATCH: pass actual image size to driver, and driver will query actual surface size to handle surface crop case.
    m_pps.PicWidth       = (UINT)par->mfx.FrameInfo.CropW;
    m_pps.PicHeight      = (UINT)par->mfx.FrameInfo.CropH;
    m_pps.ChromaType     = CHROMA_TYPE_YUV420;
    m_pps.SampleBitDepth = 8;
    m_pps.NumComponent   = 3;
    m_pps.ComponentID[0] = 0;
    m_pps.ComponentID[1] = 1;
    m_pps.ComponentID[2] = 2;
    m_pps.ComponentID[3] = 3;
    m_pps.QuantTableSelector[0] = 0;
    m_pps.QuantTableSelector[1] = 1;
    m_pps.QuantTableSelector[2] = 1;
    m_pps.QuantTableSelector[3] = 1;
    m_pps.Quality = par->mfx.Quality;
    if (m_pps.Quality > 100)
    {
        m_pps.Quality = 100;
    }

    // Scan Header
    {
        m_pps.NumScan = 1;
        m_scan_list.resize(1);
        m_scan_list[0].RestartInterval = 0;
        m_scan_list[0].NumComponent = 3;
        m_scan_list[0].ComponentSelector[0] = 0;
        m_scan_list[0].ComponentSelector[1] = 1;
        m_scan_list[0].ComponentSelector[2] = 2;
        m_scan_list[0].DcCodingTblSelector[0] = 0;
        m_scan_list[0].DcCodingTblSelector[1] = 1;
        m_scan_list[0].DcCodingTblSelector[2] = 1;
        m_scan_list[0].AcCodingTblSelector[0] = 0;
        m_scan_list[0].AcCodingTblSelector[1] = 1;
        m_scan_list[0].AcCodingTblSelector[2] = 1;
        m_scan_list[0].FirstDCTCoeff = 0;
        m_scan_list[0].LastDCTCoeff = 0;
        m_scan_list[0].Ah = 0;
        m_scan_list[0].Al = 0;
    }

    // Quantization Table
    //{
    //    m_pps.NumQuantTable = 2;
    //    m_dqt_list.resize(m_pps.NumQuantTable);

    //    {
    //        m_dqt_list[0].TableID   = 0;
    //        m_dqt_list[0].Precision = 0;
    //        for(mfxU16 i = 0; i < 64; i++)
    //            m_dqt_list[0].Qm[i] = DefaultLuminanceQuant[i];
    //    }

    //    {
    //        m_dqt_list[1].TableID   = 1;
    //        m_dqt_list[1].Precision = 0;
    //        for(mfxU16 i = 0; i < 64; i++)
    //            m_dqt_list[1].Qm[i] = DefaultChrominanceQuant[i];
    //    }
    //}

    // Huffman Table
    {
        m_pps.NumCodingTable = 4;
        m_dht_list.resize(m_pps.NumCodingTable);

        {
            m_dht_list[0].TableClass = 0;
            m_dht_list[0].TableID    = 0;
            for(mfxU16 i = 0; i < 16; i++)
                m_dht_list[0].BITS[i] = DefaultLuminanceDCBits[i];
            for(mfxU16 i = 0; i < 162; i++)
                m_dht_list[0].HUFFVAL[i] = DefaultLuminanceDCValues[i];
        }

        {
            m_dht_list[1].TableClass = 0;
            m_dht_list[1].TableID    = 1;
            for(mfxU16 i = 0; i < 16; i++)
                m_dht_list[1].BITS[i] = DefaultChrominanceDCBits[i];
            for(mfxU16 i = 0; i < 162; i++)
                m_dht_list[1].HUFFVAL[i] = DefaultChrominanceDCValues[i];
        }

        {
            m_dht_list[2].TableClass = 1;
            m_dht_list[2].TableID    = 0;
            for(mfxU16 i = 0; i < 16; i++)
                m_dht_list[2].BITS[i] = DefaultLuminanceACBits[i];
            for(mfxU16 i = 0; i < 162; i++)
                m_dht_list[2].HUFFVAL[i] = DefaultLuminanceACValues[i];
        }

        {
            m_dht_list[3].TableClass = 1;
            m_dht_list[3].TableID    = 1;
            for(mfxU16 i = 0; i < 16; i++)
                m_dht_list[3].BITS[i] = DefaultChrominanceACBits[i];
            for(mfxU16 i = 0; i < 162; i++)
                m_dht_list[3].HUFFVAL[i] = DefaultChrominanceACValues[i];
        }
    }

    // fetch extention buffers if existed
    if (par->ExtParam != 0)
    {
        for (mfxU16 i = 0; i < par->NumExtParam; i++)
        {
            if (par->ExtParam[i] != 0)
            {
                switch (par->ExtParam[i]->BufferId)
                {
                case MFX_EXTBUFF_JPEG_QT:
                    {
                        mfxExtJPEGQuantTables *pExtQuant = (mfxExtJPEGQuantTables*)par->ExtParam[i];
                        m_pps.NumQuantTable = (UINT)pExtQuant->NumTable;
                        if (m_pps.NumQuantTable)
                        {
                            m_dqt_list.resize(m_pps.NumQuantTable);
                            for (mfxU16 j = 0; j < m_pps.NumQuantTable; j++)
                            {
                                m_dqt_list[j].TableID   = j;
                                m_dqt_list[j].Precision = 0; /* (UINT)pExtQuant->Precision[j];*/
                                memcpy(m_dqt_list[j].Qm, &pExtQuant->Qm[j], 64 * sizeof(USHORT));
                            }
                        }
                        else
                        {
                            return MFX_ERR_INVALID_VIDEO_PARAM;
                        }
                    }
                    break;

                case MFX_EXTBUFF_JPEG_HUFFMAN:
                    {
                        mfxExtJPEGHuffmanTables *pExtHuffman = (mfxExtJPEGHuffmanTables*)par->ExtParam[i];
                        m_pps.NumCodingTable = (UINT)(pExtHuffman->NumDCTable + pExtHuffman->NumACTable);
                        if (m_pps.NumCodingTable)
                        {
                            m_dht_list.resize(m_pps.NumCodingTable);
                            for (mfxU16 j = 0; j < pExtHuffman->NumDCTable; j++)
                            {
                                m_dht_list[j].TableClass = 0;
                                m_dht_list[j].TableID    = j;
                                memcpy(m_dht_list[j].BITS, pExtHuffman->DCTables[j].Bits, 16 * sizeof(UCHAR));
                                memcpy(m_dht_list[j].HUFFVAL, pExtHuffman->DCTables[j].Values, 12 * sizeof(UCHAR));
                            }
                            for (mfxU16 j = 0; j < pExtHuffman->NumACTable; j++)
                            {
                                m_dht_list[pExtHuffman->NumDCTable + j].TableClass = 1;
                                m_dht_list[pExtHuffman->NumDCTable + j].TableID    = j;
                                memcpy(m_dht_list[pExtHuffman->NumDCTable + j].BITS, pExtHuffman->ACTables[j].Bits, 16 * sizeof(UCHAR));
                                memcpy(m_dht_list[pExtHuffman->NumDCTable + j].HUFFVAL, pExtHuffman->ACTables[j].Values, 162 * sizeof(UCHAR));
                            }
                        }
                        else
                        {
                            return MFX_ERR_INVALID_VIDEO_PARAM;
                        }
                    }
                    break;

#if 0                    
                case MFX_EXTBUFF_JPEG_PAYLOAD:
                    {
                        mfxExtJPEGPayloadData *pExtPayload = (mfxExtJPEGPayloadData*)par->ExtParam[i];
                        if (pExtPayload->Data && pExtPayload->NumByte > 0)
                        {
                            if (m_payload.data && m_payload.size < pExtPayload->NumByte)
                            {
                                delete [] m_payload.data;
                                m_payload.data = 0;
                            }
                            if (!m_payload.data)
                            {
                                m_payload.data = new UCHAR[pExtPayload->NumByte];
                            }
                            if (m_payload.data)
                            {
                                memcpy(m_payload.data, pExtPayload->Data, pExtPayload->NumByte);
                                m_payload.size = pExtPayload->NumByte;
                                m_payload_data_present = true;
                            }
                        }
                        else
                        {
                            return MFX_ERR_INVALID_VIDEO_PARAM;
                        }
                    }
                    break;
#endif

                default:
                    return MFX_ERR_INVALID_VIDEO_PARAM;
                    break;
                }
            }
        }
    }

    return sts;
}

void ExecuteBuffers::Close()
{
    if (m_payload.data)
    {
        delete [] m_payload.data;
        m_payload.data = 0;
    }
}

TaskManager::TaskManager()
{
    m_pTaskList = 0;
    m_TaskNum   = 0;
}

TaskManager::~TaskManager()
{
    if (m_pTaskList)
    {
        delete [] m_pTaskList;
        m_pTaskList = 0;
    }
}

mfxStatus TaskManager::Init(mfxU32 maxTaskNum)
{
    if (maxTaskNum > 0 &&
        maxTaskNum < JPEG_DDITASK_MAX_NUM)
    {
        m_TaskNum = maxTaskNum;
        m_pTaskList = new DdiTask[m_TaskNum];
        memset(m_pTaskList, 0, m_TaskNum * sizeof(DdiTask));
        for (mfxU32 i = 0; i < m_TaskNum; i++)
        {
            m_pTaskList[i].m_idx = i;
            m_pTaskList[i].m_idxBS = i;
        }
        return MFX_ERR_NONE;
    }
    else
    {
        m_pTaskList = 0;
        m_TaskNum   = 0;
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }
}

mfxStatus TaskManager::AssignTask(DdiTask *& newTask)
{
    if (m_pTaskList)
    {
        mfxU32 i;
        for (i = 0; i < m_TaskNum; i++)
        {
            if (m_pTaskList[i].lInUse == 0)
                break;
        }
        if (i < m_TaskNum)
        {
            newTask = &m_pTaskList[i];
            InterlockedExchange(&newTask->lInUse, 1L);
            return MFX_ERR_NONE;
        }
        else
        {
            return MFX_WRN_DEVICE_BUSY;
        }
    }
    else
    {
        return MFX_ERR_NULL_PTR;
    }
}

mfxStatus TaskManager::RemoveTask(DdiTask & task)
{
    if (m_pTaskList)
    {
        InterlockedExchange(&task.lInUse, 0L);
        task.surface = 0;
        task.bs      = 0;
        return MFX_ERR_NONE;
    }
    else
    {
        return MFX_ERR_NULL_PTR;
    }
}

void CachedFeedback::Reset(mfxU32 cacheSize)
{
    Feedback init;
    memset(&init, 0, sizeof(init));
    init.bStatus = ENCODE_NOTAVAILABLE;

    m_cache.resize(cacheSize, init);
}

void CachedFeedback::Update(const FeedbackStorage& update)
{
    for (size_t i = 0; i < update.size(); i++)
    {
        if (update[i].bStatus != ENCODE_NOTAVAILABLE)
        {
            Feedback* cache = 0;

            for (size_t j = 0; j < m_cache.size(); j++)
            {
                if (m_cache[j].StatusReportFeedbackNumber == update[i].StatusReportFeedbackNumber)
                {
                    cache = &m_cache[j];
                    break;
                }
                else if (cache == 0 && m_cache[j].bStatus == ENCODE_NOTAVAILABLE)
                {
                    cache = &m_cache[j];
                }
            }

            if (cache == 0)
            {
                assert(!"no space in cache");
                throw std::logic_error(__FUNCSIG__": no space in cache");
            }

            *cache = update[i];
        }
    }
}

const CachedFeedback::Feedback* CachedFeedback::Hit(mfxU32 feedbackNumber) const
{
    for (size_t i = 0; i < m_cache.size(); i++)
        if (m_cache[i].StatusReportFeedbackNumber == feedbackNumber)
            return &m_cache[i];

    return 0;
}

void CachedFeedback::Remove(mfxU32 feedbackNumber)
{
    for (size_t i = 0; i < m_cache.size(); i++)
    {
        if (m_cache[i].StatusReportFeedbackNumber == feedbackNumber)
        {
            m_cache[i].bStatus = ENCODE_NOTAVAILABLE;
            return;
        }
    }

    assert(!"wrong feedbackNumber");
}

#endif // MFX_ENABLE_MJPEG_VIDEO_ENCODE
