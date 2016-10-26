//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2013 Intel Corporation. All Rights Reserved.
//

#ifndef __OUTLINE_H__
#define __OUTLINE_H__

#include <vector>
#include <list>

#include "entry.h"
#include "umc_video_data.h"
#include "umc_base_codec.h"
#include "umc_video_decoder.h"

static const Ipp32u TIME_STAMP_FREQUENCY = 90000;
static const Ipp64u TIME_STAMP_INVALID = (Ipp64u)-1;

inline Ipp64u CalculateTimestampAlignValue(Ipp64f frameRate)
{
    return (Ipp64u)((1.0 / frameRate) * TIME_STAMP_FREQUENCY);
}

inline Ipp64u AlignTimestamp(Ipp64u ts, Ipp64u align_value)
{
    return ((ts + align_value - 1) / align_value) * align_value;
}

inline Ipp64f GetFloatTimeStamp(Ipp64u ts)
{
    Ipp64f tsFloat = ts == TIME_STAMP_INVALID ? -1.0 : ts / (Ipp64f)TIME_STAMP_FREQUENCY;
    return tsFloat;
}

inline Ipp64u GetIntTimeStamp(Ipp64f ts)
{
    Ipp64u tsInt = ts < 0.0 ? TIME_STAMP_INVALID : (Ipp64u)(ts * TIME_STAMP_FREQUENCY);
    tsInt = (tsInt + 9) / 10;
    tsInt *= 10;
    return tsInt;
}

enum FindModes
{
    FIND_MODE_ID = 0,
    FIND_MODE_TIMESTAMP = 1,
    FIND_MODE_CRC = 2,
};

enum Ignore
{
    IGNORE_TIMESTAMP = 1,
};

class OutlineParams
{
public:
    OutlineParams()
        : m_isPrettyPrint(false)
        , m_searchMethod(FIND_MODE_ID)
    {
    }

    bool        m_isPrettyPrint;
    FindModes   m_searchMethod;
    Ipp32u      m_ignoreList; // see Ignore enum
};

class TestVideoData : public UMC::VideoData
{
    DYNAMIC_CAST_DECL(TestVideoData, UMC::VideoData)

public:
    TestVideoData()
        : m_mfx_picStruct(0)
        , m_mfx_dataFlag(0)
        , m_mfx_viewId(0)
        , m_mfx_temporalId(0)
        , m_mfx_priorityId(0)
        , m_is_mfx(false)

        , m_frameNum(-1)
        , m_seqNum(-1)
        , m_native_crc32(0)
        , m_time(0)

    {
    };

    TestVideoData(const TestVideoData & data)
        : m_mfx_picStruct(data.m_mfx_picStruct)
        , m_mfx_dataFlag(data.m_mfx_dataFlag)
        , m_mfx_viewId(data.m_mfx_viewId)
        , m_mfx_temporalId(data.m_mfx_temporalId)
        , m_mfx_priorityId(data.m_mfx_priorityId)
        , m_is_mfx(data.m_is_mfx)

        , m_frameNum(data.m_frameNum)
        , m_seqNum(data.m_seqNum)
        , m_native_crc32(data.m_native_crc32)
        , m_time(data.m_time)
    {
        VideoData::operator = (data);
    };

    UMC::Status Init(UMC::MediaData *in, Ipp32s seqNum, Ipp32s frameNum)
    {
        VideoData *info = DynamicCast<VideoData> (in);
        if (!info)
            return UMC::UMC_ERR_INVALID_PARAMS;

        VideoData::operator = (*(info));

        m_native_crc32 = 0;
        m_frameNum = frameNum;
        m_seqNum = seqNum;

        Ipp64f start, end;
        in->GetTime(start, end);

        m_is_mfx = false;
        m_mfx_picStruct = 0;
        m_mfx_dataFlag = 0;
        m_mfx_viewId = 0;
        m_mfx_temporalId = 0;
        m_mfx_priorityId = 0;

        m_time = GetIntTimeStamp(start);
        return UMC::UMC_OK;
    }

    void SetSequenceNumber(Ipp32s num)
    {
        m_seqNum = num;
    }

    Ipp32s GetSequenceNumber() const
    {
        return m_seqNum;
    }

    void SetFrameNumber(Ipp32s num)
    {
        m_frameNum = num;
    }

    Ipp32s GetFrameNumber() const
    {
        return m_frameNum;
    }

    void SetCRC32(Ipp32u crc32)
    {
        m_native_crc32 = crc32;
    }

    Ipp32u GetCRC32() const
    {
        return m_native_crc32;
    }

    void SetIntTime(Ipp64u time)
    {
        m_time = time;
    }

    Ipp64u GetIntTime() const
    {
        return m_time;
    }

    Ipp32u          m_mfx_picStruct;
    Ipp32u          m_mfx_dataFlag;
    Ipp32u          m_mfx_viewId;
    Ipp32u          m_mfx_temporalId;
    Ipp32u          m_mfx_priorityId;
    bool            m_is_mfx;

protected:
    Ipp32s          m_frameNum;
    Ipp32s          m_seqNum;
    Ipp32u          m_native_crc32;
    Ipp64u          m_time;
};

typedef std::list<Entry_Read *> ListOfEntries;

// OutlineReader base class
class OutlineReader
{
public:
    OutlineReader()
    {}

    virtual ~OutlineReader()
    {}

    virtual void Init(const vm_char *) = 0;

    virtual void Close() = 0;

    virtual Entry_Read * GetEntry(Ipp32s seq_index, Ipp32s index) = 0;

    virtual Entry_Read * GetSequenceEntry(Ipp32s index) = 0;

    virtual ListOfEntries FindEntry(const TestVideoData & tstData, Ipp32u mode) = 0;

    virtual void ReadEntryStruct(Entry_Read * entry, TestVideoData * data);

    virtual void ReadSequenceStruct(Entry_Read * entry, UMC::VideoDecoderParams *info);
};

class CheckerBase
{
public:
    virtual ~CheckerBase() {}

    virtual void CheckSequenceInfo(const UMC::BaseCodecParams *info1, const UMC::BaseCodecParams *info2) = 0;
    virtual void CheckFrameInfo(const UMC::MediaData *in1, const UMC::MediaData *in2) = 0;
};

// OutlineWriter base class
class OutlineWriter
{
public:
    OutlineWriter(){};
    virtual ~OutlineWriter(){};
    virtual void Init(const vm_char *fileName) = 0;
    virtual void Close() = 0;
};

// VideoOutlineWriter class
class VideoOutlineWriter : public OutlineWriter
{
public:
    VideoOutlineWriter(){};
    virtual ~VideoOutlineWriter(){};
    virtual void WriteFrame(TestVideoData *pData) = 0;
    virtual void WriteSequence(Ipp32s seq_id, UMC::BaseCodecParams *info) = 0;
};

class OutlineError
{
public:

    enum ErrorType
    {
        OUTLINE_ERROR_TYPE_UNKNOWN,
        OUTLINE_ERROR_TYPE_SEQUENCE,
        OUTLINE_ERROR_TYPE_FRAME
    };

    OutlineError()
        : m_type(OUTLINE_ERROR_TYPE_UNKNOWN)
    {
    }

    OutlineError(ErrorType type, const TestVideoData &refData, const TestVideoData &testData)
        : m_type(type)
        , m_data1(refData)
        , m_data2(testData)
    {
    }

    OutlineError(ErrorType type, const UMC::VideoDecoderParams &refSeq, const UMC::VideoDecoderParams &testSeq)
        : m_type(type)
        , m_seq1(refSeq)
        , m_seq2(testSeq)
    {
    }

    virtual ~OutlineError()
    {}

    ErrorType GetErrorType() const
    {
        return m_type;
    }

    const TestVideoData * GetReferenceTestVideoData() const
    {
        if (m_type != OUTLINE_ERROR_TYPE_FRAME)
            return 0;
        return &m_data1;
    }

    const TestVideoData * GetTestVideoData() const
    {
        if (m_type != OUTLINE_ERROR_TYPE_FRAME)
            return 0;

        return &m_data2;
    }

    const UMC::VideoDecoderParams * GetReferenceSequence() const
    {
        if (m_type != OUTLINE_ERROR_TYPE_SEQUENCE)
            return 0;
        return &m_seq1;
    }

    const UMC::VideoDecoderParams * GetSequence() const
    {
        if (m_type != OUTLINE_ERROR_TYPE_SEQUENCE)
            return 0;

        return &m_seq2;
    }

private:

    ErrorType m_type;

    TestVideoData m_data1;
    TestVideoData m_data2;

    UMC::VideoDecoderParams  m_seq1;
    UMC::VideoDecoderParams  m_seq2;
};

class OutlineException
{
public:

    OutlineException (UMC::Status err, const vm_char * str = 0)
        : m_err(err)
        , m_str(str)
    {
        if (!m_str)
            m_str = VM_STRING("Uknown outline error");
    }

    UMC::Status GetStatus () const { return m_err; }

    const vm_char * GetDescription () const { return m_str; }

    void SetError(OutlineError & err)
    {
        m_error = err;
    }

    const OutlineError & GetError() const
    {
        return m_error;
    }

private:
    UMC::Status m_err;
    const vm_char * m_str;

    OutlineError m_error;
};

class OutlineErrors
{
public:
    OutlineErrors();
    virtual ~OutlineErrors() {};

    virtual const OutlineException * GetLastError() const;

    void SetLastError(OutlineException * );

    virtual void PrintError();

private:

    OutlineException m_exception;
};

#endif // __OUTLINE_H__
