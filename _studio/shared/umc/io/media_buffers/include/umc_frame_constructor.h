// Copyright (c) 2005-2019 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#ifndef __UMC_FRAME_CONSTRUCTOR_H__
#define __UMC_FRAME_CONSTRUCTOR_H__

#include "umc_defs.h"
#include "umc_media_buffer.h"
#include "umc_linked_list.h"
#include "umc_splitter.h"
#include <algorithm>

namespace UMC
{
    // this macro checks 3 bytes for being '0x000001'
    #define IS_001(PTR) ((PTR)[0] == 0 && (PTR)[1] == 0 && (PTR)[2] == 1)
    // this macro checks 4 bytes for being video start code
    #define IS_CODE(PTR, CODE) (IS_001(PTR) && (PTR)[3] == (CODE))
    // this macro checks 4 bytes for being one of 2 video start codes
    #define IS_CODE_2(PTR, CODE1, CODE2) (IS_001(PTR) && ((PTR)[3] == (CODE1) || (PTR)[3] == (CODE2)))
    // this macro checks 4 bytes for being one of 3 video start codes
    #define IS_CODE_3(PTR, CODE1, CODE2, CODE3) (IS_001(PTR) && ((PTR)[3] == (CODE1) || (PTR)[3] == (CODE2) || (PTR)[3] == (CODE3)))
    // this macro checks 4 bytes for being one of 4 video start codes
    #define IS_CODE_4(PTR, CODE1, CODE2, CODE3, CODE4) (IS_001(PTR) && ((PTR)[3] == (CODE1) || (PTR)[3] == (CODE2) || (PTR)[3] == (CODE3) || (PTR)[3] == (CODE4)))
    // this macro checks 4 bytes for being one of video start codes from interval
    #define IS_CODE_INT(PTR, CODE_MIN, CODE_MAX) ((PTR)[0] == 0 && (PTR)[1] == 0 && (PTR)[2] == 1 && (PTR)[3] >= (CODE_MIN) && (PTR)[3] <= (CODE_MAX))
    // this macro checks 4 bytes for being valid h264 video start code
    #define IS_H264_CODE(PTR, CODE) ((PTR)[0] == 0 && (PTR)[1] == 0 && (PTR)[2] == 1 && ((PTR)[3] & 0x1f) == (CODE))

    // macro for mp3 header parsing
    #define MPEGA_HDR_VERSION(x)       ((x & 0x80000) >> 19)
    #define MPEGA_HDR_LAYER(x)         (4 - ((x & 0x60000) >> 17))
    #define MPEGA_HDR_ERRPROTECTION(x) ((x & 0x10000) >> 16)
    #define MPEGA_HDR_BITRADEINDEX(x)  ((x & 0x0f000) >> 12)
    #define MPEGA_HDR_SAMPLINGFREQ(x)  ((x & 0x00c00) >> 10)
    #define MPEGA_HDR_PADDING(x)       ((x & 0x00200) >> 9)
    #define MPEGA_HDR_EXTENSION(x)     ((x & 0x00100) >> 8)
    #define MPEGA_HDR_MODE(x)          ((x & 0x000c0) >> 6)
    #define MPEGA_HDR_MODEEXT(x)       ((x & 0x00030) >> 4)
    #define MPEGA_HDR_COPYRIGHT(x)     ((x & 0x00008) >> 3)
    #define MPEGA_HDR_ORIGINAL(x)      ((x & 0x00004) >> 2)
    #define MPEGA_HDR_EMPH(x)          ((x & 0x00003))

    inline
    int32_t CalcCurrentLevel(int32_t iStart, int32_t iEnd, int32_t iBufSize)
    {
        int32_t iCurrentLevel = iEnd - iStart;
        return iCurrentLevel + ((iCurrentLevel >= 0) ? 0 : iBufSize);
    }

    inline
    bool ArePTSEqual(double dTime1, double dTime2)
    {
        return 0 == (int32_t)(90000 * (dTime1 - dTime2));
    }

    // maps TrackType to AudioStreamType and VideoStreamType
    uint32_t ConvertTrackType(TrackType type);
    // maps AudioStreamType to TrackType
    TrackType ConvertAudioType(uint32_t type);
    // maps VideoStreamType to TrackType
    TrackType ConvertVideoType(uint32_t type);

    struct TeletextStreamInfo : public StreamInfo
    {
        TeletextStreamInfo()
        : uiType(0), uiMagazineNumber(0), uiPageNumber(0)
        {
            memset(szLanguage, 0, sizeof(szLanguage));
        }

        uint8_t szLanguage[4];
        uint8_t uiType;
        uint8_t uiMagazineNumber;
        uint8_t uiPageNumber;
    };

    // extends TrackInfo for needs of mpeg2
    // adds some utilities
    struct Mpeg2TrackInfo : public TrackInfo
    {
        DYNAMIC_CAST_DECL(Mpeg2TrackInfo, TrackInfo)

        // constructor
        Mpeg2TrackInfo();
        // copy track's info structure from another one including all pointers
        Status CopyFrom(Mpeg2TrackInfo *pSrc);
        // releases StreamInfo
        void ReleaseStreamInfo(void);
        // releases decoder specific info
        void ReleaseDecSpecInfo(void);
        // releases all pointers and resets fields
        void ReleaseAll(void);
        // allocates StreamInfo pointer, copies PID and stream type from parent object
        Status Alloc(void);
        // sets duration field (m_Type is requiered)
        void SetDuration(double dDuration);

        // number of program that track belongs to
        // 0 if not applied
        uint32_t m_uiProgNum;
        // number of frames is currently stored in track buffer
        // it decreaments after lock
        uint32_t m_uiFramesReady;
        // number of track in order of its first frame ready
        // -1 means that first frame isn't ready yet
        int32_t m_iFirstFrameOrder;
    };

    class SplMediaData : public MediaData
    {
        DYNAMIC_CAST_DECL(SplMediaData, MediaData)
    public:
        // constructor
        SplMediaData();
        // sets absolute position
        void SetAbsPos(unsigned long long uiAbsPos);
        // returns absolute position
        unsigned long long GetAbsPos(void) const;
        // updates certain flag, returns its previous value
        bool SetFlag(uint32_t mask, bool flag);
        // returns certain flag
        bool GetFlag(uint32_t mask) const;
        // updates all flags, returns previous flagset
        uint32_t SetFlags(uint32_t flags);
        // returns current flagset
        uint32_t GetFlags() const;
    protected:
        // absolute position of sample (specified by data reader)
        unsigned long long m_uiAbsPos;
        uint32_t m_uiFlags;
    };

    struct FCSample
    {
        static const uint32_t STAMPS_APPLIED  = 0x00000010;
        static const uint32_t PES_START       = 0x00000020;
        static const uint32_t ACCESS_POINT    = 0x00000040;
        static const uint32_t ACCESS_UNIT     = 0x00000080;
        static const uint32_t FIRST_SLICE     = 0x00000100;

        // constructor
        FCSample();
        // resets sample
        void Reset(void);
        // copy from MediaData
        void CopyFrom(MediaData &data, int32_t iOffset);
        // copy to MediaData
        void CopyTo(MediaData &data, uint8_t *pBufBase);
        // checks if position is enclosed
        bool IsHit(int32_t iPos);
        // sets frame type, returns previous
        uint32_t SetFrameType(uint32_t uiType);
        // returns frame type
        uint32_t GetFrameType(void);
        // sets certain flag, returns its previous value
        bool SetFlag(uint32_t uiFlagMask, bool bNewFlag);
        // returns certain flag
        bool GetFlag(uint32_t uiFlagMask);
        // updates all flags, returns previous flagset
        uint32_t SetFlags(uint32_t flags);
        // returns current flagset
        uint32_t GetFlags();
        // moves forward data pointer
        void MovePointer(uint32_t off);

        // PTS of media sample
        double dPTS;
        // DTS or media sample
        double dDTS;
        // media sample size
        uint32_t uiSize;
        // bits 0...2 - picture type (only for video)
        // bit      3 - reserved
        // bit      4 - is set if sample's timestamps are used (for input samples)
        // bit      5 - indicates if access unit commence here
        // bit      6 - indicates if access point commence here
        uint32_t uiFlags;
        // absolute position of sample in stream
        unsigned long long uiAbsPos;
        // offset in buffer of first byte of sample
        int32_t iBufOffset;
    };

    inline
    void UpdateInputSample(FCSample &sample, int32_t iFrom, int32_t iTo, bool isPure)
    {
        if (isPure && (iTo > sample.iBufOffset))
            sample.uiAbsPos += std::min(iTo - std::max(iFrom, sample.iBufOffset), (int32_t)sample.uiSize);

        int32_t iEndOffset = sample.iBufOffset + sample.uiSize;
        if (sample.iBufOffset > iFrom)
            sample.iBufOffset -= std::min(sample.iBufOffset, iTo) - iFrom;
        if (iEndOffset > iFrom)
            iEndOffset -= std::min(iEndOffset, iTo) - iFrom;
        sample.uiSize = iEndOffset - sample.iBufOffset;
    }

    inline
    void CutInterval(FCSample &sample1, FCSample &sample2, uint8_t *pBuf, int32_t iFrom, int32_t iTo, int32_t iLastByte, bool isPure = false)
    {
        uint8_t *pFrom = pBuf + iTo;
        uint8_t *pTo = pBuf + iFrom;
        MFX_INTERNAL_CPY(pTo, pFrom, iLastByte - iTo);
        UpdateInputSample(sample1, iFrom, iTo, isPure);
        UpdateInputSample(sample2, iFrom, iTo, isPure);
    }

    struct InnerListElement
    {
        InnerListElement(void);
        InnerListElement(FCSample &rData);
        FCSample m_data;
        InnerListElement *pNext;
        InnerListElement *pPrev;
    };

#if defined (__ICL)
    //function "UMC::MediaData::Alloc(size_t={unsigned int})" is hidden by "UMC::VideoData::Alloc" -- virtual function override intended?
#pragma warning(disable:1125)
#endif
    // used for reordering at backward
    class ReorderQueue : public LinkedList<InnerListElement>
    {
    public:
        ReorderQueue(void);

        using LinkedList<InnerListElement>::Add;
        using LinkedList<InnerListElement>::First;
        using LinkedList<InnerListElement>::Last;
        using LinkedList<InnerListElement>::Next;
        using LinkedList<InnerListElement>::Prev;
        using LinkedList<InnerListElement>::Get;

        Status Add(FCSample &rSample);
        Status Add(FCSample &rSample, int32_t idx);
        Status Remove(void);
        Status Remove(int32_t idx);
        Status First(FCSample &rSample);
        Status Last(FCSample &rSample);
        Status Next(FCSample &rSample);
        Status Prev(FCSample &rSample);
        Status Get(FCSample &rSample, int32_t idx);

        const FCSample *FirstBO(void) const;
        const FCSample *LastBO(void) const;

    protected:
        void AddToSuperList(InnerListElement *pAddedElem);
        void RemoveFromSuperList(void);
        InnerListElement *m_pSuperFirst;
        InnerListElement *m_pSuperLast;
    };

    // object performs elementary bitstream operations
    class BitstreamReader
    {
    public:
        // Default constructor
        BitstreamReader(void);
        BitstreamReader(uint8_t *pStream, uint32_t len);
        // Destructor
        virtual ~BitstreamReader(void) {}
        // Initialize bit stream reader
        void Init(uint8_t *pStream, uint32_t len = 0xffffffff);
        // Copy next bit
        uint32_t CopyBit(void);
        // Get bit (move pointer)
        uint32_t GetBit(void);
        //Pick bits (doesn't move pointer)
        uint32_t PeekBits(int32_t iNum);
        // Get bits (move pointer)
        uint32_t GetBits(int32_t iNum);
        // Skip bits (move pointer)
        void SkipBits(int32_t iNum);
        // Get current pointer
        uint8_t *Stream(void);
        // Get unsigned integer Exp-Golomb-coded element (move pointer)
        uint32_t GetUE(void);
        // Get signed integer Exp-Golomb-coded element (move pointer)
        int32_t GetSE(void);

    protected:
        // Refresh pre-read bits
        virtual void Refresh(void);

    protected:
        // pointer to source stream
        uint8_t *m_pSource;
        uint8_t *m_pEnd;
        // pre-read stream bits
        uint32_t m_nBits;
        // amount of pre-read bits
        int32_t m_iReadyBits;
    };

    // object extends base bitstream reader for h264 video
    // it removes start code emulation prevention bytes
    class H264BitstreamReader : public BitstreamReader
    {
    public:
        // Constructor
        H264BitstreamReader(void);
        H264BitstreamReader(uint8_t *pStream, uint32_t len);
        void Init(uint8_t *pStream, uint32_t len);

    protected:
        // Function is overloaded for removing prevention bytes
        virtual void Refresh(void);

    protected:
        // amount of sequential zeroes
        int32_t m_iZeroes;
    };

    class FrameConstructorParams : public MediaReceiverParams
    {
        DYNAMIC_CAST_DECL(FrameConstructorParams, MediaReceiverParams)

    public:
        // constructor-initializer
        FrameConstructorParams();

        // pointer to heritable ESInfo (may be NULL if inheritance isn't needed)
        Mpeg2TrackInfo *m_pInfo;
        // size of buffer to allocate
        size_t m_lBufferSize;
        // initial number of samples in buffer (might be dynamically allocated if needed)
        uint32_t m_nOfFrames;
        // indicates if stop when frame is constructed or parses whole input chunk
        bool m_bStopAtFrame;
        // this option affect absPos value assignment
        // for pure stream it is byte-accurate
        // otherwise it is packet-accurate
        bool m_bPureStream;
        // pointer to memory allocator object (if NULL it will be created internally)
        MemoryAllocator *m_pMemoryAllocator;
    };

    // base class for all frame constructors
    // it implements most of functionality
    // that can be extended by by derived classes or used directly
    // the most important fuction to implement is GetFrame()
    // that actually performs frame constructing
    class FrameConstructor : public MediaBuffer
    {
        DYNAMIC_CAST_DECL(FrameConstructor, MediaBuffer)

    public:
        // constructor
        FrameConstructor();
        // destructor
        ~FrameConstructor();

        // inherited from base MediaBuffer
        virtual Status Init(MediaReceiverParams *pInit);
        virtual Status Close();
        virtual Status Reset();
        virtual Status Stop();
        virtual Status LockInputBuffer(MediaData *in);
        virtual Status UnLockInputBuffer(MediaData *in, Status streamStatus = UMC_OK);
        virtual Status LockOutputBuffer(MediaData *out);
        virtual Status UnLockOutputBuffer(MediaData *out);

        // resets tailing bytes after last constracted sample
        virtual Status SoftReset();
        // sets playback rate, affects only video frame constructors
        virtual void SetRate(double dRate);
        // unlocks input buffer and tries to construct frame
        // but constructed frame is uncommited (can be rolled back)
        virtual Status PreUnLockInputBuffer(MediaData *in, Status streamStatus = UMC_OK);
        // returns last constructed frame
        virtual Status GetLastFrame(MediaData *data);
        // returns pointer to ESInfo
        virtual Mpeg2TrackInfo *GetInfo(void);

    protected:
        virtual Status GetFrame(SplMediaData *frame);
        virtual Status GetSampleFromQueue(FCSample *pSample);
        void AssignAbsPos(int32_t iPos);
        void AssignTimeStamps(int32_t iPos);

        // synchro object
        std::mutex m_synchro;

        // pointer to allocated buffer
        uint8_t *m_pBuf;
        // memory ID of allocated buffer
        MemID m_midAllocatedBuffer;

        // frame being constructed
        FCSample m_CurFrame;
        // queue of output samples
        ReorderQueue m_OutputQueue;
        // index of first-to-decode sample
        int32_t m_iFirstInDecOrderIdx;
        // last constructed frame
        SplMediaData m_LastFrame;
        // previous input sample
        FCSample m_PrevSample;
        // last input sample
        FCSample m_LastSample;

        // indicates end of stream
        bool m_bEndOfStream;
        // is output buffer locked
        bool m_bIsOutputBufferLocked;
        // is info filled, this is needed to prevent info changing
        bool m_bInfoFilled;
        // indicates if stop when frame is constructed or parses whole input chunk
        bool m_bStopAtFrame;
        bool m_bPureStream;

        // size of allocated buffer
        int32_t m_lBufferSize;
        // position in buffer of byte after last data byte
        int32_t m_lLastBytePos;
        // position in buffer of first data byte
        int32_t m_lFirstBytePos;
        // current position in buffer
        int32_t m_lCurPos;
        // position in buffer of picture start code of frame being constuncted
        int32_t m_lCurPicStart;

        // playback rate
        double m_dRate;
        // number of commited frames
        uint32_t m_uiCommitedFrames;
        // total number of frames in buffer
        uint32_t m_uiTotalFrames;
        // actual size of user data
        int32_t m_iCurrentLevel;
        // pointer to info structure
        Mpeg2TrackInfo *m_pInfo;
    };

    class AudioFrameConstructor : public FrameConstructor
    {
    public:
        static const AudioStreamType MpegAStreamType[2][3];
        static const int32_t MpegAFrequency[3][4];
        static const int32_t MpegABitrate[2][3][15];
        static const int32_t MpegAChannels[4];
        static const int32_t AC3Frequency[3];
        static const int32_t AC3FrequencyExt[8];
        static const int32_t AC3BitRate[19];
        static const int32_t AC3NumChannels[];
        static const float MpegAFramesize[2][4];
        static const int32_t AACFrequency[16];
        static const int32_t AACChannels[16];
        static const int32_t DTSChannels[16];
        static const int32_t DTSFrequency[16];
        static const int32_t DTSBitrate[32];
        static const int32_t LPCMChannels[16];
        Status Init(MediaReceiverParams *pInit);
    protected:
        // parses headers of all supported audio
        Status ParseHeader();
        // indicate if header parsed
        bool m_bHeaderParsed;
    };

    class TimeStampedAudioFrameConstructor : public AudioFrameConstructor
    {
    public:
        Status GetFrame(SplMediaData *frame);
    };

    class BufferedAudioFrameConstructor : public AudioFrameConstructor
    {
    public:
        explicit BufferedAudioFrameConstructor(double dToBuf = 0.05);
        Status GetFrame(SplMediaData *frame);
    protected:
        double m_dToBuf;
    };

    class PureAudioFrameConstructor : public AudioFrameConstructor
    {
    public:
        Status GetFrame(SplMediaData *frame);
    };

    class VideoFrameConstructor : public FrameConstructor
    {
    public:
        VideoFrameConstructor(void);
        virtual Status Init(MediaReceiverParams *pInit);
        virtual Status Reset(void);
        virtual Status SoftReset(void);
        virtual void SetRate(double dRate);
        virtual bool IsFrameStartFound(void);

    protected:
        // find next sample to output, performs reordering at backward
        virtual Status GetSampleFromQueue(FCSample *pSample);
        // returns true when sample such sample is allowed at playback rate
        bool IsSampleComplyWithTmPolicy(FCSample &sample, double dRate);
        // is sequence header found and parsed
        bool m_bSeqSCFound;
        // is picture header found and parsed
        bool m_bPicSCFound;
        // is start of frame found
        bool m_bFrameBegFound;
        // FrameConstructor generate 4-zero sequence at the end of last video frame
        bool m_bIsFinalizeSequenceSent;
    };

    class Mpeg2FrameConstructor : public VideoFrameConstructor
    {
    public:
        // parses sequence header and fills info if passed
        static Status ParseSequenceHeader(uint8_t *buf, int32_t iLen, Mpeg2TrackInfo *pInfo);
        // parses picture header and sets interlace_type field if info struct passed
        static Status ParsePictureHeader(uint8_t *buf, int32_t iLen, Mpeg2TrackInfo *pInfo);
        // mpeg1/2 frame rate values
        static const double FrameRate[9];
        // mpeg1 aspect ratio valus
        static const double AspectRatioMp1[15];
        // mpeg2 aspect ratio valus
        static const int32_t AspectRatioMp2[5][2];
        // mpeg2 color format values
        static const ColorFormat ColorFormats[4];
    protected:
        virtual Status GetFrame(SplMediaData *frame);
    };

    class Mpeg4FrameConstructor : public VideoFrameConstructor
    {
    public:
        // parses video object layer header and fills info if passed
        static Status ParseVideoObjectLayer(uint8_t *buf, int32_t iLen, Mpeg2TrackInfo *pInfo);
        // mpeg4 aspect ratio values
        static const int32_t AspectRatio[6][2];
    protected:
        virtual Status GetFrame(SplMediaData *frame);
    };

    class H261FrameConstructor : public VideoFrameConstructor
    {
    public:
        // parses h261 header and fills info if passed
        static Status ParseHeader(uint8_t *buf, int32_t iLen, Mpeg2TrackInfo *pInfo);
    protected:
        virtual Status GetFrame(SplMediaData *frame);
    };

    class H263FrameConstructor : public VideoFrameConstructor
    {
    public:
        // parses h263 header and fills info if passed
        static Status ParseHeader(uint8_t *buf, int32_t iLen, Mpeg2TrackInfo *pInfo);
        // valid frame sizes for h263 video
        static const int16_t PictureSize[6][2];
    protected:
        virtual Status GetFrame(SplMediaData *frame);
    };

    enum
    {
        NALU_SLICE      = 1,
        NALU_IDR_SLICE  = 5,
        NALU_SEI        = 6,
        NALU_SPS        = 7,
        NALU_PPS        = 8,
        NALU_DELIMITER  = 9,
        NALU_SPS_SUBSET = 15,
        NALU_SLICE_EXTENSION = 20
    };

    class PesFrameConstructor : public FrameConstructor
    {
    public:
        virtual Status Init(MediaReceiverParams *pInit);
        virtual Status Reset(void);
        virtual Status SoftReset(void);

    protected:
        virtual Status GetFrame(SplMediaData *frame);

    protected:
        bool m_bIsFirst;
    };

    class H264ParsingCore
    {
    public:
        enum Result { Ok, OkPic, ErrNeedSync, ErrNeedData, ErrInvalid };

        H264ParsingCore();
        Result Sync(MediaData& data, bool eos = false);
        Result Construct(MediaData& data, bool eos = false);
        void GetInfo(VideoStreamInfo& info);
        void Reset();

    protected:
        static const uint32_t BytesForSliceReq = 30;
        static const uint32_t MaxNumSps = 32;
        static const uint32_t MaxNumPps = 256;
        static const FrameType SliceType[10];
        static const uint16_t AspectRatio[17][2];
        static const ColorFormat ColorFormats[4];
        struct Slice;

        Status ParseSps(uint8_t* buf, int32_t len);
        Status ParsePps(uint8_t* buf, int32_t len);
        Status ParseSh(uint8_t* buf, int32_t len);
        bool IsNewPicture(const Slice& prev, const Slice& last);
        bool IsFieldOfOneFrame(const Slice& prev, const Slice& last);

        struct BaseNaluHeader
        {
            BaseNaluHeader() : m_ready(false) {}
            void SetReady(bool ready) { m_ready = ready; }
            bool IsReady() const { return m_ready; }
        private:
            bool m_ready;
        };

        struct Sps : public BaseNaluHeader
        {
            Sps() { Reset(); }
            void Reset();
            uint32_t time_scale; // 30
            uint32_t num_units_in_tick; // 1
            uint16_t sar_width; // 1
            uint16_t sar_height; // 1
            uint8_t profile_idc;
            uint8_t level_idc;
            uint8_t chroma_format_idc; // 1
            uint8_t log2_max_pic_order_cnt_lsb;
            uint8_t log2_max_frame_num;
            uint8_t frame_mbs_only_flag;
            uint8_t pic_order_cnt_type;
            uint8_t delta_pic_order_always_zero_flag;
            uint8_t frame_width_in_mbs;
            uint8_t frame_height_in_mbs;
            uint8_t frame_cropping_rect_left_offset;
            uint8_t frame_cropping_rect_right_offset;
            uint8_t frame_cropping_rect_top_offset;
            uint8_t frame_cropping_rect_bottom_offset;
        };

        struct Pps : public BaseNaluHeader
        {
            Pps() { Reset(); }
            void Reset();
            void SetSps(const Sps& sps) { m_sps = &sps; }
            const Sps& GetSps() const { return *m_sps; }
            uint8_t pic_order_present_flag;
        private:
            const Sps* m_sps;
        };

        struct Slice : public BaseNaluHeader
        {
            Slice() { Reset(); }
            void Reset();
            void SetPps(const Pps& pps) { m_pps = &pps; }
            const Pps& GetPps() const { return *m_pps; }

            uint16_t pic_parameter_set_id;
            uint8_t field_pic_flag;
            uint8_t bottom_field_flag;
            uint32_t frame_num;
            int32_t slice_type;
            uint32_t pic_order_cnt_lsb;
            int32_t delta_pic_order_cnt_bottom;
            int32_t delta_pic_order_cnt[2];
            uint8_t nal_ref_idc;
            uint8_t idr_flag;
            uint32_t idr_pic_id;
        private:
            const Pps* m_pps;
        };

        Sps m_sps[MaxNumSps];
        Pps m_pps[MaxNumPps];
        Slice m_prev;
        Slice m_last;
        bool m_synced;
        uint32_t m_skip;
        FrameType m_type;
        bool m_spsParsed;
    };

    class H264FrameConstructor : public VideoFrameConstructor
    {
    public:
        H264FrameConstructor();
        virtual Status Init(MediaReceiverParams *pInit);
        virtual Status Reset();
        virtual Status SoftReset();

    protected:
        virtual Status GetFrame(SplMediaData *frame);
        void InternBuf2MediaData(MediaData& data);
        void MediaData2InternBuf(MediaData& data);
        H264ParsingCore m_core;
    };

    class MJPEGFrameConstructor : public VideoFrameConstructor
    {
#define SWAP_SHORT(a) ((a >> 8) + ((a & 0xFF) << 8))

        enum {
            JPEG_SOI = SWAP_SHORT(0xFFD8),
            JPEG_EOI = SWAP_SHORT(0xFFD9),
            JPEG_APP1 = SWAP_SHORT(0xFFE1),
            JPEG_APP13 = SWAP_SHORT(0xFFED)
        };
        enum {
            StateWaitSoi,
            StateWaitEoi,
            StateWaitMarkerLength,
        } m_state;
        int32_t m_lParserStartPos;
        int32_t m_lNumBytesToSkip;
    public:
        MJPEGFrameConstructor();
        virtual Status Init(MediaReceiverParams *pInit);
        virtual Status Reset() ;
        virtual Status SoftReset() ;

    protected:
        virtual Status GetFrame(SplMediaData *frame);
        //void InternBuf2MediaData(MediaData& data);
        //void MediaData2InternBuf(MediaData& data);
    };
}

#endif /* __UMC_FRAME_CONSTRUCTOR_H__ */
