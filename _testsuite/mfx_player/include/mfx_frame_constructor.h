/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2009-2020 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#ifndef __MFX_FRAME_CONSTRUCTOR_H
#define __MFX_FRAME_CONSTRUCTOR_H

#include "mfxvideo++.h"

class MFXFrameConstructor
{
public:
    MFXFrameConstructor();
    virtual ~MFXFrameConstructor();

    virtual mfxStatus Init(mfxVideoParam * /*stream_params*/){return MFX_ERR_NONE;}

    virtual mfxStatus ConstructFrame( mfxBitstream* pBSIn
                                    , mfxBitstream* pBSOut);
    virtual mfxStatus Reset() {return MFX_ERR_NONE;}

};

class MFXVC1FrameConstructor : public MFXFrameConstructor
{
public:    
    MFXVC1FrameConstructor();
    
    virtual mfxStatus Init(mfxVideoParam *stream_params);

    virtual mfxStatus ConstructFrame( mfxBitstream* pBSIn
                                    , mfxBitstream* pBSOut);

    virtual mfxStatus Reset() {m_isSeqHeaderConstructed = false; return MFXFrameConstructor::Reset();}
protected:

    mfxStatus       ConstructSequenceHeaderSM   ( mfxBitstream* pBSIn
                                                , mfxBitstream* pBSOut);
    bool            IsStartCodeExist            ( mfxU8* pStart);

protected:

    mfxVideoParam  m_VideoParam;
    bool           m_isSeqHeaderConstructed;
};

//helper class used by AVC frame constructor
class StartCodeIteratorMP4
{
public:

    StartCodeIteratorMP4();
    
    mfxU32 Init(mfxBitstream *pBS);
    mfxU32 GetCurrentOffset();
    void   SetLenInBytes(mfxU32 nByteLen);
    mfxU32 GetNext();

private:

    mfxU32  m_nFullSize;
    mfxU32  m_nByteLen;

    mfxU8*  m_pSource;
    mfxU32  m_nSourceSize;

    mfxU8*  m_pSourceBase;
    mfxU32  m_nSourceBaseSize;
};



class MFXAVCFrameConstructor : public MFXFrameConstructor
{
public:
    MFXAVCFrameConstructor();
    virtual ~MFXAVCFrameConstructor();
    virtual mfxStatus ConstructFrame( mfxBitstream* pBSIn
                                    , mfxBitstream* pBSOut);
    virtual mfxStatus Reset() { m_bHeaderReaded = false; return MFX_ERR_NONE;};

protected:
    typedef struct AVCRecord
    {
        AVCRecord()
        {
            configurationVersion = 1;
            lengthSizeMinusOne = 1;
            numOfSequenceParameterSets = 0;
            numOfPictureParameterSets = 0;
            AVCProfileIndication = 0;
            profile_compatibility = 0;
            AVCLevelIndication = 0;
            numOfSequenceParameterSets = 0;
            numOfPictureParameterSets = 0;
        }

        mfxU8 configurationVersion;
        mfxU8 AVCProfileIndication;
        mfxU8 profile_compatibility;
        mfxU8 AVCLevelIndication;
        mfxU8 lengthSizeMinusOne;
        mfxU8 numOfSequenceParameterSets;
        mfxU8 numOfPictureParameterSets;

    } AVCRecord;

    enum
    {
        D_START_CODE_LENGHT = 4,
        D_BYTES_FOR_HEADER_LENGHT = 2
    };

    mfxStatus ReadHeader(mfxBitstream* pBSIn, mfxBitstream *pBS);

    bool         m_bHeaderReaded;
    AVCRecord    m_avcRecord;
    mfxBitstream m_StartCodeBS;
    mfxU8        m_startCodesBuf[4];
};

class MFXHEVCFrameConstructor : public MFXFrameConstructor
{
public:
    MFXHEVCFrameConstructor();
    virtual ~MFXHEVCFrameConstructor();
    virtual mfxStatus ConstructFrame( mfxBitstream* pBSIn
                                    , mfxBitstream* pBSOut);
    virtual mfxStatus Reset() { m_bHeaderReaded = false; return MFX_ERR_NONE;};

protected:
    typedef struct HEVCRecord
    {
        HEVCRecord()
        {
            configurationVersion = 1;
            numOfParameterSets = 0;
        }

        mfxU8 configurationVersion;
        mfxU8 numOfParameterSets;
    } HEVCRecord;

    mfxStatus ReadHeader(mfxBitstream* pBSIn, mfxBitstream *pBS);

    bool         m_bHeaderReaded;
    HEVCRecord   m_hevcRecord;
    mfxBitstream m_StartCodeBS;
    mfxU8        m_startCodesBuf[4];
};

#endif//__MFX_FRAME_CONSTRUCTOR_H
