//* ////////////////////////////////////////////////////////////////////////////// */
//*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2013 Intel Corporation. All Rights Reserved.
//
//
//*/


#pragma once

#include "sample_defs.h"
#include "ffmpeg_port.h"
#include "surface_auto_lock.h"

//#define USE_FF_PARSER

//data structure submitted to async thread routine
class FFDecWorkload
{
public:
    FFDecWorkload()
        : m_ffCodec(NULL)
        , m_bsIn()
        , m_pOut(NULL)
        , m_pAlloc (NULL)
        , m_coreAndData(NULL, NULL)
        , m_bDrain()
        , m_ffPortable()
    {
    }
    FFDecWorkload( FFPortable &ffPortable
                 , FFCodec *pCodec
                 , FFParser *pParser
                 , FFScaler *scl
                 , MFXCoreInterface &pCore
                 , mfxBitstream *bs_in
                 , mfxFrameSurface1 *frame_out)
        : m_ffPortable(&ffPortable)
        , m_scl(scl)
        , m_ffCodec(pCodec)
        , m_ffParser(pParser)
        , m_bsIn(bs_in? *bs_in : mfxBitstream())
        , m_bDrain(!bs_in)
        , m_pOut(frame_out)
        , m_coreAndData(&pCore, &frame_out->Data)
    {
        if (!m_bDrain) {
            m_bsData.reserve(bs_in->DataLength + FF_INPUT_BUFFER_PADDING_SIZE);
            m_bsData.insert(m_bsData.end(), bs_in->Data + bs_in->DataOffset, bs_in->Data + bs_in->DataOffset + bs_in->DataLength);
            m_bsData.resize(bs_in->DataLength + FF_INPUT_BUFFER_PADDING_SIZE);


            m_bsIn.MaxLength  = (mfxU32)m_bsData.size();
            m_bsIn.DataOffset = 0;

            //all data copied
            bs_in->DataOffset += bs_in->DataLength;
            bs_in->DataLength  = 0;
        }
    }

    mfxStatus operator () () {
        // Task is run once and saved status
        if (m_executeSts)
            return m_executeSts;

        //copy ctor implementation is absent, and this buffer will point to wrong location, if initialized in curent ctor
        if (!m_bsData.empty()) {
            m_bsIn.Data       = &m_bsData.front();
        }

        mfxBitstream bsOut =  {};

#ifdef USE_FF_PARSER
        m_executeSts = m_ffPortable->ConstructFrame(*m_ffParser, m_bsIn, bsOut);
        if (m_executeSts != MFX_ERR_NONE) {
            return m_executeSts;
        }

        //FFMPEG: design note
        /** @warning The input buffer must be FF_INPUT_BUFFER_PADDING_SIZE larger than
        * the actual read bytes because some optimized bitstream readers read 32 or 64
        * bits at once and could read over the end.
        *
        * @warning The end of the input buffer buf should be set to 0 to ensure that
        * no overreading happens for damaged MPEG streams.*/
        std::vector <mfxU8> bufAlign(bsOut.DataLength + FF_INPUT_BUFFER_PADDING_SIZE);
        std::copy(bsOut.Data + bsOut.DataOffset, bsOut.Data + bsOut.DataOffset + bsOut.DataLength, bufAlign.begin());
        bsOut.Data = &bufAlign.front();
        bsOut.DataOffset = 0;
        bsOut.MaxLength = bufAlign.size();
        bsOut.FrameType = m_bsIn.FrameType;
#else
        bsOut = m_bsIn;
#endif

        mfxFrameSurface1 srf;
        //do the decoding
        m_executeSts = m_ffPortable->DecodeFrame(*m_ffCodec, bsOut, srf);
        if (m_executeSts != MFX_ERR_NONE) {
            return m_executeSts;
        }

        SurfaceAutoLock lock(m_coreAndData.m_pCore->FrameAllocator(), *m_pOut);
        if (MFX_ERR_NONE != lock) {
            return lock;
        }
        //need to transfer data from ff internal buffers to mediasdk surface
        m_executeSts = m_ffPortable->ConvertSurface(*m_scl, srf, *m_pOut);

        return m_executeSts;
    }

protected:
    FFPortable        *m_ffPortable;
    FFScaler          *m_scl;
    FFCodec           *m_ffCodec;
    FFParser          *m_ffParser;
    mfxBitstream       m_bsIn;
    bool               m_bDrain;
    mfxFrameSurface1  *m_pOut;
    mfxFrameAllocator *m_pAlloc;
    std::vector<mfxU8> m_bsData;
    struct ExecuteStatus {
        std::pair<bool, mfxStatus> m_CompletedSts;
        ExecuteStatus & operator = (mfxStatus sts) {
            m_CompletedSts.first = true;
            m_CompletedSts.second = sts;
            return *this;
        }
        operator mfxStatus & () {
            m_CompletedSts.first = true;
            return m_CompletedSts.second;
        }
        operator const mfxStatus & () const {
            return m_CompletedSts.second;
        }
        operator bool() {
            return m_CompletedSts.first;
        }
    } m_executeSts;
    struct CoreAndData
    {
        MFXCoreInterface *m_pCore;
        mfxFrameData *m_pData;
        CoreAndData(MFXCoreInterface *pCore, mfxFrameData *pData)
            : m_pCore(pCore)
            , m_pData(pData) {
                if (m_pCore) {
                    m_pCore->IncreaseReference(m_pData);
                }
        }
        CoreAndData(const CoreAndData & that)
            : m_pCore(that.m_pCore)
            , m_pData(that.m_pData) {
                if (m_pCore) {
                    m_pCore->IncreaseReference(m_pData);
                }
        }
        ~CoreAndData()
        {
            if (m_pCore) {
                m_pCore->DecreaseReference(m_pData);
            }
        }
    }m_coreAndData;
};
