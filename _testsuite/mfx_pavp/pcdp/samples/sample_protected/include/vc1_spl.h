//* ////////////////////////////////////////////////////////////////////////////// */
//*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2010-2011 Intel Corporation. All Rights Reserved.
//
//
//*/

#ifndef _VC1_SPL_H__
#define _VC1_SPL_H__

#include "vc1_spl_defs.h"
#include "vc1_spl_header_parser.h"

namespace ProtectedLibrary
{

class VC1_Spl : public AbstractSplitter
{
public:
    VC1_Spl();
    virtual ~VC1_Spl();

    virtual mfxStatus Reset();

    virtual mfxStatus GetFrame(mfxBitstream * bs_in, FrameSplitterInfo ** frame);

    virtual mfxStatus PostProcessing(FrameSplitterInfo *frame, mfxU32 sliceNum);

    virtual void ResetCurrentState();

protected:
    mfxU8 m_iFrameReady;
    mfxU32 m_iFrameSize;
    mfxU32 m_iFrameStartCode;
    mfxU32 m_iLastFrame;
    FrameSplitterInfo m_frame;
    std::vector<mfxU8>  m_currentFrame;
    std::vector<SliceSplitterInfo>  m_slices;

    VC1_SPL_Headers* m_pHeaders;

    mfxU32 m_iSliceNum;
    VC1_StartCode m_StartCodeData[MAX_START_CODE_NUM];

    mfxU64   m_pts;

    mfxStatus Init();
    mfxStatus Close();

    //start codes search
    VC1_Sts_Type StartCodeParsing(mfxBitstream * bs_in);

    //frame construction function
    mfxStatus GetNextData(mfxBitstream * bs_in);

    mfxStatus FillOutData();
    mfxStatus ParsePictureHeader();
private:
    VC1_Spl(const VC1_Spl &spl);
    VC1_Spl& operator =(const VC1_Spl &spl);
};

}//namespace
#endif // _VC1_SPL_H__
