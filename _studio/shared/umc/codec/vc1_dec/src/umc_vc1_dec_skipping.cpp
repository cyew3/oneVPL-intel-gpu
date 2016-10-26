//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2004-2012 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"
#if defined (UMC_ENABLE_VC1_VIDEO_DECODER)

#include "umc_vc1_dec_skipping.h"
#include "umc_vc1_common_defs.h"

using namespace UMC::VC1Skipping;
void VC1SkipMaster::SetSkipPeriod(bool ForRefFrames, Ipp32u period)
{
    if (ForRefFrames)
    {
        m_iRefSkipPeriod = period;
        m_iRefSkipCounter = 0;
    }
    else
    {
        m_iBSkipPeriod = period;
        m_iBSkipCounter = 0;
    }
}
void VC1SkipMaster::SetPerformMode(bool ForRefFrames, Ipp32u perfomMode)
{
    if (perfomMode > VC1MaxSpeed)
        return;
    if (ForRefFrames)
        m_RefPerfMode = perfomMode;
    else
        m_BPerfMode = perfomMode;
}
void VC1SkipMaster::MoveToNextState()
{
    switch(m_SpeedMode)
    {
    case B1:
        m_BPerfMode = VC1PostProcDisable;
        break;
    case B2:
        m_BPerfMode = VC1PostProcDisable;
        break;
    case B3:
        m_BPerfMode = VC1PostProcDisable;
        SetSkipPeriod(false,3);
        break;
    case B4:
        m_BPerfMode = VC1PostProcDisable;
        SetSkipPeriod(false,2);
        break;
    case B5:
        m_BPerfMode = VC1PostProcDisable;
        SetSkipPeriod(false,1);
        break;
    case Ref1:
        m_RefPerfMode = VC1PostProcDisable;
        m_BPerfMode = VC1PostProcDisable;
        SetSkipPeriod(false,1);
        break;
    case Ref2:
        m_RefPerfMode = VC1PostProcDisable;
        m_BPerfMode = VC1PostProcDisable;
        SetSkipPeriod(false,1);
        break;
    case Ref3:
        m_RefPerfMode = VC1PostProcDisable;
        m_BPerfMode = VC1PostProcDisable;
        SetSkipPeriod(false,1);
        SetSkipPeriod(true,4);
        break;
    default:
        break;
}



}
void VC1SkipMaster::Reset()
{
    m_iRefSkipPeriod = 0;
    m_iRefSkipCounter = 0;
    m_iBSkipPeriod = 0;
    m_iBSkipCounter = 0;
    m_RefPerfMode = VC1Routine;
    m_BPerfMode = VC1Routine;
    //m_SpeedMode = Normal;
}
void VC1SkipMaster::SetVideoDecodingSpeed(SkippingMode skip_mode)
{
    Reset();
    m_SpeedMode = skip_mode;
    MoveToNextState();
}

bool VC1SkipMaster::IsDeblockingOn()
{
    return m_bOnDbl;
}

bool VC1SkipMaster::ChangeVideoDecodingSpeed(Ipp32s& speed_shift)
{
    if (speed_shift)
    {
        // special dbl disable mode
        if (0x22 == speed_shift)
        {
            m_bOnDbl = false;
        }
        else
        {
            if (((Ref3 == m_SpeedMode) && (speed_shift > 0))||
                ((Normal == m_SpeedMode) && (speed_shift < 0)))
                return false;
            Reset();
            m_SpeedMode += speed_shift;
            if (Ref3 < m_SpeedMode)
                m_SpeedMode = Ref3;
            else if (Normal > m_SpeedMode)
                m_SpeedMode = Normal;
            MoveToNextState();
            speed_shift = m_SpeedMode;
        }
    }
    return true;
}

bool VC1SkipMaster::IsNeedSkipFrame(Ipp32u picType)
{
    if (VC1_IS_REFERENCE(picType))
    {
        if ((m_SpeedMode < Ref3)||(VC1_I_FRAME == picType))
            return false;
        return true;
    }
    else
    {
        if (m_SpeedMode < B3)
            return false;

        ++m_iBSkipCounter;
        if (m_iBSkipCounter == m_iBSkipPeriod)
        {
            m_iBSkipCounter = 0;
            return true;
        }
    }
    return false;
}
bool VC1SkipMaster::IsNeedPostProcFrame(Ipp32u picType)
{
    if (!m_bOnDbl)
        return false;

    if (VC1_IS_REFERENCE(picType))
    {
        return (m_RefPerfMode == VC1Routine);
    }
    else
    {
        return (m_BPerfMode == VC1Routine);
    }
}
bool VC1SkipMaster::IsNeedSimlifyReconstruct(Ipp32u picType)
{
    if (VC1_IS_REFERENCE(picType))
    {
        return (m_RefPerfMode == VC1MaxSpeed);
    }
    else
    {
        return (m_BPerfMode == VC1MaxSpeed);
    }

}


#endif
