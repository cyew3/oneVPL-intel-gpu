/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2003-2011 Intel Corporation. All Rights Reserved.
//
//
*/

#pragma once 

#include <vector>
#include <list>
#pragma warning(disable : 4201)
#include <memory>
#pragma warning(default : 4201)

#include "mfxdefs.h"
#include "mfxstructures.h"
#include "avc_structures.h"
#include "avc_bitstream.h"

#include "avc_headers.h"
#include "avc_nal.h"

struct SliceSplitterInfo
{
    mfxU32   DataOffset;
    mfxU32   DataLength;
    mfxU32   HeaderLength;
};

struct AVCFrameSplitterInfo
{
    SliceSplitterInfo * Slice;   // array
    mfxU32     SliceNum;
    mfxU32     FirstFieldSliceNum;

    mfxU8  *   Data;    // including data of slices 
    mfxU32     DataLength;
    mfxU64     TimeStamp;
};

class AVCSlice : public SliceSplitterInfo
{
public:

    AVCSlice();

    AVCSliceHeader* GetSliceHeader();

    bool IsField() const {return m_sliceHeader.field_pic_flag != 0;}

    mfxI32 RetrievePicParamSetNumber(mfxU8 *pSource, mfxU32 nSourceSize);

    bool DecodeHeader(mfxU8 *pSource, mfxU32 nSourceSize);

    AVCHeadersBitstream *GetBitStream(void){return &m_BitStream;}

    AVCPicParamSet* m_picParamSet;
    AVCSeqParamSet* m_seqParamSet;
    AVCSeqParamSetExtension* m_seqParamSetEx;

    mfxU64 m_dTime;

protected:
    AVCSliceHeader m_sliceHeader;
    AVCHeadersBitstream m_BitStream;
};

class AVCFrameInfo
{
public:

    AVCFrameInfo();

    void Reset();

    AVCSlice  * m_slice;
    mfxU32      m_index;
};

class AVCFrameSplitter
{
public:

    AVCFrameSplitter();
    virtual ~AVCFrameSplitter();

    virtual mfxStatus GetFrame(mfxBitstream * bs_in, AVCFrameSplitterInfo ** frame);
    void ResetCurrentState();

protected:
    std::auto_ptr<NALUnitSplitter> m_pNALSplitter;

    mfxStatus Init();

    void Close();

    mfxStatus ProcessNalUnit(mfxI32 nalType, mfxBitstream * destination);

    mfxStatus DecodeHeader(mfxBitstream * nalUnit);
    mfxStatus DecodeSEI(mfxBitstream * nalUnit);
    AVCSlice * DecodeSliceHeader(mfxBitstream * nalUnit);
    mfxStatus AddSlice(AVCSlice * pSlice);

    AVCFrameInfo * GetFreeFrame();

    mfxU8 * GetMemoryForSwapping(mfxU32 size);

    mfxStatus AddNalUnit(mfxBitstream * nalUnit);
    mfxStatus AddSliceNalUnit(mfxBitstream * nalUnit, AVCSlice * pSlice);
    bool IsFieldOfOneFrame(AVCFrameInfo * frame, const AVCSliceHeader * slice1, const AVCSliceHeader *slice2);

    bool                m_WaitForIDR;

    AVCHeaders     m_headers;
    std::auto_ptr<AVCFrameInfo> m_AUInfo;
    AVCFrameInfo * m_currentInfo;
    AVCSlice * m_pLastSlice;

    mfxBitstream * m_lastNalUnit;

    enum
    {
        BUFFER_SIZE = 1024 * 1024
    };

    std::vector<mfxU8>  m_currentFrame;
    std::vector<mfxU8>  m_swappingMemory;
    std::list<AVCSlice> m_slicesStorage;

    std::vector<SliceSplitterInfo>  m_slices;
    AVCFrameSplitterInfo m_frame;
};