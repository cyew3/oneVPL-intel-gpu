/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011 - 2012 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#pragma once

#include <cmath>
#include "mfxvideo++.h"

//sample bitrate control class
//based on per frame QP settings available via mfxEncodeCtrl structure
//this class also maintains IPPP gop structure using input GopPicSize value
class SampleBRC
    : public IBRC
{
    mfxF64 m_fps;
    mfxU32 m_targetFrameSize;
    int    m_QP;
    mfxU32 m_nFramesAfterReset;
    mfxU32 m_nFrames;
    mfxU64 m_BsSizeAfterReset;
    mfxU16 m_targetKbps;
    mfxU16 m_nGopSize;

public:
    SampleBRC()
    {}

    mfxStatus Init(mfxVideoParam *pvParams)
    {
        m_fps               = (mfxF64)pvParams->mfx.FrameInfo.FrameRateExtN / (mfxF64)pvParams->mfx.FrameInfo.FrameRateExtD;
        m_targetKbps        = pvParams->mfx.TargetKbps;
        m_targetFrameSize   = (mfxU32)(m_targetKbps * 1024.0 / m_fps / 8);
        m_nGopSize          = pvParams->mfx.GopPicSize;
        m_QP                = 20;//just constant
        m_BsSizeAfterReset  = 0;
        m_nFrames           = 0;
        m_nFramesAfterReset = 0;

        return MFX_ERR_NONE;
    }

    void Update(mfxBitstream *pbs)
    {
        mfxU32 encodedFrameSize = pbs->DataLength;

        m_BsSizeAfterReset += encodedFrameSize;
        m_nFrames++;
        m_nFramesAfterReset ++;

        mfxU32 AverageFrameSizeSoFar = (mfxU32)((mfxF64)m_BsSizeAfterReset / m_nFramesAfterReset);

        if (encodedFrameSize > m_targetFrameSize)
            m_QP++;

        if (encodedFrameSize < m_targetFrameSize)
            m_QP--;


        if (encodedFrameSize > 2*m_targetFrameSize)
            m_QP += 2;

        if (encodedFrameSize < m_targetFrameSize/2)
            m_QP -= 2;


        if (AverageFrameSizeSoFar > m_targetFrameSize)
            m_QP++;

        if (AverageFrameSizeSoFar < m_targetFrameSize)
            m_QP--;

        if (AverageFrameSizeSoFar > 1.3 * m_targetFrameSize)
            m_QP++;

        if (AverageFrameSizeSoFar < 0.8 * m_targetFrameSize)
            m_QP--;

        if (m_QP < 1) m_QP = 1;
    }

    mfxStatus Reset(mfxVideoParam *pvParams)
    {
        m_targetKbps      = pvParams->mfx.TargetKbps;
        m_targetFrameSize = (mfxU32)(m_targetKbps * 1024 / m_fps / 8);
        m_QP              = (mfxU16)(m_QP * pow(((mfxF64)m_BsSizeAfterReset / m_nFramesAfterReset / m_targetFrameSize), 0.3));
        if (m_QP < 1)
            m_QP = 1;

        m_BsSizeAfterReset  = 0;
        m_nFramesAfterReset = 0;

        return MFX_ERR_NONE;
    }

    //current target bitrate
    mfxU16 GetCurrentBitrate()
    {
        return m_targetKbps;
    }

    mfxU16 GetFrameQP()
    {
        return (mfxU16)m_QP;
    }

    mfxU16 GetFrameType()
    {
        if (!(m_nFrames % m_nGopSize))
        {
            return MFX_FRAMETYPE_I;
        }
        return MFX_FRAMETYPE_P;
    }
};