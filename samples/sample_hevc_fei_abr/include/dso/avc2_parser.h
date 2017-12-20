#pragma once

#include "bs_reader2.h"
#include "bs_mem+.h"
#include "avc2_struct.h"
#include "avc2_info.h"
#include "avc2_entropy.h"
#include <list>
#include <vector>
#include <algorithm>
#include <thread>
#include <mutex>
#include <condition_variable>

namespace BS_AVC2
{

struct NALUDesc
{
    NALU* ptr;
    Bs32u NuhBytes;
    bool  complete;
    bool  shParsed;
};

struct PocInfo
{
    Bs32s Poc;
    Bs32s TopPoc;
    Bs32s BotPoc;
    bool  isBot;
    bool  mmco5;
    Bs32s PocMsb;
    Bs32s PocLsb;
    Bs32s FrameNum;
    Bs32s FrameNumOffset;
};

struct DPBFrame
{
    Bs16u FrameNum;
    Bs8u  long_term : 1;
    Bs8u  non_existing : 1;

    union
    {
        Bs32s FrameNumWrap;
        Bs32s LongTermFrameIdx;
    };

    struct Field
    {
        Bs32s POC;
        union
        {
            Bs32s PicNum;
            Bs32s LongTermPicNum;
        };
        Bs8u  bottom_field_flag : 1;
        Bs8u  available : 1;
    } field[2];
};

typedef std::list<DPBFrame> DPB;

struct DPBPic
{
    DPB::iterator frame;
    bool          field;
};

class SDParser //Slice data parser
    : public BsReader2::Reader
    , public Info
    , private CABAC
    , private CAVLC
{
public:
    std::vector<MB> m_mb; // temporal storage for parsed MBs

    SDParser()
        : CABAC(*this, this)
        , CAVLC(*this, this)
    {
        SetTraceLevel(TRACE_DEFAULT);
        SetEmulation(false);
    }

    virtual ~SDParser() {}

    inline Bs32u u(Bs32u n)  { return GetBits(n); };
    inline Bs32u u1()        { return GetBit(); };
    inline Bs32s si(Bs32u n) { return u1() ? -(Bs32s)u(n - 1) : (Bs32s)u(n - 1); };
    inline Bs32u u8()        { return GetBits(8); };
    inline Bs32u ue()        { return GetUE(); };
    inline Bs32s se()        { return GetSE(); };
    inline Bs32s te(Bs32u x) { return (x > 1) ? ue() : !u1(); }

    bool more_rbsp_data();

    void parseSD_CABAC(Slice& s);
    void parseMB_CABAC(MB& mb);
    void parseSubMbPred_CABAC(MB& mb);
    void parseMbPred_CABAC(MB& mb);
    void parseResidual_CABAC(
        MB&    mb,
        Bs8u   startIdx,
        Bs8u   endIdx);
    void parseResidualLuma_CABAC(
        MB&    mb,
        Bs16s  i16x16DClevel[16 * 16],
        Bs16s  i16x16AClevel[16][16],
        Bs16s  level4x4[16][16],
        Bs16s  level8x8[4][64],
        Bs8u   startIdx,
        Bs8u   endIdx,
        Bs8u   blkType);
    void parseResidualBlock_CABAC(
        MB&     mb,
        Bs16s* coeffLevel,
        Bs8u   startIdx,
        Bs8u   endIdx,
        Bs8u   maxNumCoeff,
        Bs8u   blkIdx,
        Bs8u   blkType,
        bool   isCb);


    void parseSD_CAVLC(Slice& s);
    void parseMB_CAVLC(MB& mb);
    void parseSubMbPred_CAVLC(MB& mb);
    void parseMbPred_CAVLC(MB& mb);
    void parseResidual_CAVLC(
        MB&    mb,
        Bs8u   startIdx,
        Bs8u   endIdx);
    void parseResidualLuma_CAVLC(
        MB&    mb,
        Bs16s  i16x16DClevel[16 * 16],
        Bs16s  i16x16AClevel[16][16],
        Bs16s  level4x4[16][16],
        Bs8u   startIdx,
        Bs8u   endIdx,
        Bs8u   blkType);
    void parseResidualBlock_CAVLC(
        MB&     mb,
        Bs16s* coeffLevel,
        Bs8u   startIdx,
        Bs8u   endIdx,
        Bs8u   maxNumCoeff,
        Bs8u   blkIdx,
        Bs8u   blkType,
        bool   isCb);
};

typedef struct
{
    std::mutex mtx;
    std::condition_variable cv;
} SyncPoint;

typedef enum
{
      WAIT = 0
    , SUBMITTED
    , BUSY
    , DONE
    , EXIT
} ThreadState;

struct ThreadCtx
{
    SDParser parser;
    BS_MEM::Allocator* allocator;
    std::vector<Bs8u> RBSP;
    std::mutex mtx;
    std::condition_variable cv;
    std::thread thread;
    ThreadState state;
    BSErr sts;
};

class Parser
    : private BS_MEM::Allocator //thread-safe allocator
    , private BsReader2::File
    , private SDParser /* This instance used as slice data parser in single-thread mode, in MT mode used as reader only.*/    
{
    friend class Sync; //helper class to get free(waiting) thread task or wait for first ready
public:

    Parser(Bs32u mode = 0);

    virtual ~Parser();

    BSErr open(const char* file_name);
    void close() { Close(); }

    void set_buffer(Bs8u* buf, Bs32u buf_size) { Reset(buf, buf_size, 0); }

    BSErr parse_next_au(AU*& pAU);

    BSErr Lock(void* p);
    BSErr Unlock(void* p);

    inline Bs16u get_async_depth() { return (Bs16u)m_thread.size(); }

    BSErr sync_slice(Slice* s); //usefull in ASYNC_MT mode only, other modes have forced synchronization for all slices on parse_next_au() exit
    void sync_task(ThreadCtx& t);

    void set_trace_level(Bs32u level);
    inline Bs64u get_cur_pos() { return GetByteOffset(); }

private:
    SyncPoint           m_sp;
    std::mutex          m_ssync_mtx;
    std::list<ThreadCtx> m_thread;
    std::list<NALUDesc> m_nalu;
    std::list<SEI*>     m_postponed_sei;
    DPB                 m_dpb;
    DPB                 m_free_dpb;
    AU*                 m_au;
    SPS*                m_sps[32];
    PPS*                m_pps[256];
    Bs32u               m_mode;
    PocInfo             m_prev;
    PocInfo             m_prev_ref;

    bool singleSPS(SPS*& activeSPS);

    void parseNUH(NALU& nalu);
    void parseRBSP(NALU& nalu);
    void parseSPS(SPS& sps);
    void parseScalingMatrix(ScalingMatrix& sm, Bs32u nFlags);
    bool parseScalingList(Bs8u* sl, Bs32u sz);
    void parseVUI(VUI& vui);
    void parseHRD(HRD& hrd);
    void parsePPS(PPS& pps);
    void parseSEI(SEI& sei);
    void parseSEIPayload(BufferingPeriod& bp);
    void parseSEIPayload(PicTiming& pt);
    void parseSEIPayload(DRPMRep& rep);
    void parseSliceHeader(NALU& nalu);
    void parseRPLM(RefPicListMod& rplm);
    void parseDRPM(DRPM& drpm);
    void parsePostponedSEI(SEI& sei, Slice& s);

    void parseSD(Slice& s);

    void  decodeSlice(NALU& nalu, Slice* first);
    Bs32s decodePOC(NALU& nalu, PocInfo& prev, PocInfo& prevRef);
    void  decodePicNum(Slice& s);
    void  initRefLists(Slice& s, std::list<DPBPic>& List0, std::list<DPBPic>& List1);
    void  modifyRefList(Slice& s, std::list<DPBPic>& ListX, Bs8u X);
    void  updateDPB(NALU& slice);

    void traceRefLists(Slice& s);

    void dpbStore(DPBFrame const & f);
    bool dpbRemove(DPB::iterator it);
    void dpbClear();

    void CopyMBs(Slice& s, MB* src);
};

}

typedef BS_AVC2::Parser AVC2_BitStream;