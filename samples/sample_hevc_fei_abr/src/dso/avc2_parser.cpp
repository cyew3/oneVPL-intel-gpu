#include "avc2_parser.h"

using namespace BS_AVC2;
using namespace BsReader2;

void ThreadProc(ThreadCtx& self, SyncPoint& sp, Bs8u id);

Parser::Parser(Bs32u mode)
    : m_dpb(0)
    , m_free_dpb(16)
    , m_au(0)
    , m_mode(mode)
{
    std::fill_n(m_sps, 32, (SPS*)0);
    std::fill_n(m_pps, 256, (PPS*)0);
    SetTraceLevel(TRACE_DEFAULT);
    SetEmulation(true);
    SetZero(true);

    if ((m_mode & INIT_MODE_MT_SD) == INIT_MODE_MT_SD)
    {
        Bs8u id = 0;

        m_thread.resize(std::thread::hardware_concurrency());

        for (auto& t : m_thread)
        {
            t.state = WAIT;
            t.allocator = this;
            t.thread = std::thread(ThreadProc, std::ref(t), std::ref(m_sp), id++);
        }
    }
}

Parser::~Parser()
{
    for (auto& t : m_thread)
    {
        std::unique_lock<std::mutex> lock(t.mtx);

        while (t.state == BUSY)
            t.cv.wait(lock);

        t.state = EXIT;
        t.cv.notify_one();
    }

    for (auto& t : m_thread)
        t.thread.join();
}

BSErr Parser::open(const char* file_name)
{
    if (!Open(file_name))
        return BS_ERR_INVALID_PARAMS;

    Reset(this);

    return BS_ERR_NONE;
}

BSErr Parser::Lock(void* p)
{
    try
    {
        lock(p);
    }
    catch (std::bad_alloc())
    {
        return BS_ERR_MEM_ALLOC;
    }
    return BS_ERR_NONE;
}

BSErr Parser::Unlock(void* p)
{
    try
    {
        unlock(p);
    }
    catch (std::bad_alloc())
    {
        return BS_ERR_MEM_ALLOC;
    }
    return BS_ERR_NONE;
}

void Parser::set_trace_level(Bs32u level)
{
    SetTraceLevel(level);

    for (auto& t : m_thread)
        t.parser.SetTraceLevel(level);
}


void Parser::dpbStore(DPBFrame const & f)
{
    if (m_free_dpb.empty())
        throw BsReader2::InvalidSyntax();
    m_dpb.splice(m_dpb.end(), m_free_dpb, m_free_dpb.begin());
    m_dpb.back() = f;
}

bool Parser::dpbRemove(DPB::iterator it)
{
    if (it == m_dpb.end())
        return false;
    m_free_dpb.splice(m_free_dpb.end(), m_dpb, it);
    return true;
}

void Parser::dpbClear()
{
    m_free_dpb.splice(m_free_dpb.end(), m_dpb);
}

void CopyMBs(BS_MEM::Allocator& allocator, Slice& s, MB* src)
{
    if (!s.NumMB)
        return;

    s.mb = allocator.alloc<MB>(&s, s.NumMB);
    memmove(s.mb, src, sizeof(MB) * s.NumMB);

    for (Bs32u i = 1; i < s.NumMB; i++)
        s.mb[i - 1].next = &s.mb[i];
}

void Parser::CopyMBs(Slice& s, MB* src)
{
    ::CopyMBs(*this, s, src);
}

bool isSlice(NALU const & nalu)
{
    switch (nalu.nal_unit_type)
    {
    case SLICE_NONIDR:
    case SLICE_IDR:
    case SLICE_AUX:
    case SLICE_EXT:
        return true;
    default: break;
    }
    return false;
}

Bs16u GetNaluOrder(NALU const & nalu)
{
    switch (nalu.nal_unit_type)
    {
    case AUD_NUT:
        return 0;
    case SPS_NUT:
    case SPS_EXT_NUT:
    case SUBSPS_NUT:
        return 1;
    case PPS_NUT:
        return 2;
    case SEI_NUT:
        return 3;
    case PREFIX_NUT:
        return 4;
    case SLICE_NONIDR:
    case SLICE_IDR:
    case SD_PART_A:
    case SLICE_AUX:
    case SD_PART_B:
    case SD_PART_C:
    case SLICE_EXT:
        return 5;
    case EOSEQ_NUT:
    case EOSTR_NUT:
        return 6;
    case FD_NUT:
    default:
        break;
    }
    return 0xFFFF;
}

bool isNextAU(NALU* currSlice, NALU* prevSlice)
{
    if (!currSlice || !prevSlice) return false;

    Slice const & curr = *currSlice->slice;
    Slice const & prev = *prevSlice->slice;

    if (prev.redundant_pic_cnt < curr.redundant_pic_cnt) return false;
    if (prev.redundant_pic_cnt > curr.redundant_pic_cnt) return true;

    if (prev.frame_num != curr.frame_num) return true;
    if (prev.pic_parameter_set_id != curr.pic_parameter_set_id) return true;
    if (prev.field_pic_flag != curr.field_pic_flag) return true;
    if (prev.bottom_field_flag != curr.bottom_field_flag) return true;
    if (!prevSlice->nal_ref_idc != !currSlice->nal_ref_idc) return true;
    if ((prevSlice->nal_unit_type == SLICE_IDR)
        != (currSlice->nal_unit_type == SLICE_IDR))
        return true;

    if (currSlice->nal_unit_type == SLICE_IDR
        && prev.idr_pic_id != curr.idr_pic_id)
        return true;

    if (curr.pps->sps->pic_order_cnt_type == 0)
    {
        if (prev.pic_order_cnt_lsb != curr.pic_order_cnt_lsb) return true;
        if (prev.delta_pic_order_cnt_bottom != curr.delta_pic_order_cnt_bottom) return true;
    }

    if (curr.pps->sps->pic_order_cnt_type == 1)
    {
        if (prev.delta_pic_order_cnt[0] != curr.delta_pic_order_cnt[0]) return true;
        if (prev.delta_pic_order_cnt[1] != curr.delta_pic_order_cnt[1]) return true;
    }

    return false;
}

namespace BS_AVC2
{
    //helper class to get free(waiting) thread task or wait for first ready
    class Sync
    {
    public:
        Sync(Parser& p)
            : m_p(p)
            , m_task(0)
        {
        }

        inline bool operator() () //used as predicate in wait(), see GetTask()
        {
            size_t wait = 0;
            m_task = 0;

            for (auto& t : m_p.m_thread)
            {
                wait += t.state == WAIT;

                if (t.state == m_state)
                {
                    m_task = &t;
                    return true;
                }
            }

            return m_state == WAIT || wait == m_p.m_thread.size();
        }

        /*(state = DONE) - wait until any task is DONE, return this task or 0 if all tasks are free
        (state = WAIT) - get any free task or 0 immediately*/
        inline ThreadCtx* GetTask(ThreadState state)
        {
            m_state = state;
            std::unique_lock<std::mutex> lock(m_p.m_sp.mtx);
            m_p.m_sp.cv.wait<Sync&>(lock, *this);
            return m_task;
        }

    private:
        Parser& m_p;
        ThreadCtx* m_task;
        ThreadState m_state;
    };
}

BSErr Parser::parse_next_au(AU*& pAU)
{
    Bs32u nextAUCnt = 0;
    NALU* prevSlice = 0;
    Slice* firstSlice = 0;
    pAU = 0;

    // parse NALUs
    try
    {
        for (;;)
        {
            NALUDesc newnalu = {};

            if (m_nalu.empty() || m_nalu.back().complete)
            {
                bool LongStartCode = NextStartCode(false);

                SetEmuBytes(0);

                newnalu.ptr = alloc<NALU>();

                newnalu.ptr->StartOffset = GetByteOffset() - (3 + LongStartCode);

                parseNUH(*newnalu.ptr);

                newnalu.NuhBytes = Bs32u(GetByteOffset() - newnalu.ptr->StartOffset);

                if (!m_nalu.empty() && GetNaluOrder(*newnalu.ptr) < GetNaluOrder(*m_nalu.back().ptr) && prevSlice)
                {
                    m_nalu.push_back(newnalu);
                    nextAUCnt++;
                    break; // new AU
                }

                m_nalu.push_back(newnalu);
            }
            else if (m_nalu.back().shParsed)
            {
                prevSlice = m_nalu.back().ptr;
            }

            NALUDesc& curnalu = m_nalu.back();

            if (!curnalu.shParsed)
            {
                parseRBSP(*curnalu.ptr);

                if (isSlice(*curnalu.ptr))
                {
                    curnalu.shParsed = true;

                    while (!m_postponed_sei.empty())
                    {
                        // for some SEI payloads parsing may be postponed 
                        // due to unresolved dependencies 
                        // (like undefined active SPS for current AU)
                        parsePostponedSEI(*m_postponed_sei.front(), *curnalu.ptr->slice);
                        m_postponed_sei.pop_front();
                    }

                    if (isNextAU(curnalu.ptr, prevSlice))
                    {
                        nextAUCnt++;
                        break; // new AU
                    }

                    prevSlice = curnalu.ptr;

                    decodeSlice(*curnalu.ptr, firstSlice);
                    parseSD(*curnalu.ptr->slice);
                    if (!firstSlice)
                        firstSlice = curnalu.ptr->slice;
                }
            }
            else
            {
                decodeSlice(*curnalu.ptr, firstSlice);
                parseSD(*curnalu.ptr->slice);
                if (!firstSlice)
                    firstSlice = curnalu.ptr->slice;
            }

            try
            {
                NextStartCode(true);
            }
            catch (EndOfBuffer)
            {
                //do nothing
            }

            curnalu.ptr->NumBytesInNalUnit = Bs32u(GetByteOffset() - curnalu.ptr->StartOffset - curnalu.NuhBytes);
            curnalu.ptr->NumBytesInRbsp = curnalu.ptr->NumBytesInNalUnit - GetEmuBytes();

            TLStart(TRACE_SIZE);
            BS2_TRACE(curnalu.ptr->NumBytesInNalUnit, nalu.NumBytesInNalUnit);
            BS2_TRACE(curnalu.ptr->NumBytesInRbsp, nalu.NumBytesInRbsp);
            TLEnd();

            curnalu.complete = true;
        }
    }
    catch (EndOfBuffer)
    {
        if (m_nalu.empty() || !m_nalu.front().complete)
            return BS_ERR_MORE_DATA;
    }
    catch (std::bad_alloc)
    {
        return BS_ERR_MEM_ALLOC;
    }
    catch (Exception ex)
    {
        return ex.Err();
    }
    catch (...)
    {
        return BS_ERR_UNKNOWN;
    }

    //construct AU
    try
    {
        if (TLTest(TRACE_MARKERS))
            BS2_TRACE_STR(" <<<< NEW AU >>>> ");

        if (m_au)
            free(m_au);

        m_au = alloc<AU>();

        m_au->NumUnits = (Bs32u)m_nalu.size() - nextAUCnt;
        m_au->nalu = alloc<NALU*>(m_au, m_au->NumUnits);

        for (Bs32u i = 0; i < m_au->NumUnits; i++)
        {
            m_au->nalu[i] = m_nalu.front().ptr;
            m_nalu.pop_front();
            bound(m_au->nalu[i], m_au);
            free(m_au->nalu[i]);
        }

        if (prevSlice && prevSlice->nal_ref_idc)
            updateDPB(*prevSlice);

        if ((m_mode & INIT_MODE_ASYNC_SD) != INIT_MODE_ASYNC_SD)
        {
            Sync sync(*this);

            //Sync all tasks
            for (auto t = sync.GetTask(DONE); !!t; t = sync.GetTask(DONE))
            {
                t->state = WAIT;

                if (t->sts)
                    return t->sts;
            }
        }
    }
    catch (std::bad_alloc)
    {
        return BS_ERR_MEM_ALLOC;
    }

    pAU = m_au;

    return BS_ERR_NONE;
}

BSErr Parser::sync_slice(Slice* s)
{
    std::unique_lock<std::mutex> lock(m_ssync_mtx);

    for (auto& t : m_thread)
    {
        if (&t.parser.cSlice() == s && t.state != WAIT)
        {
            sync_task(t);
            return touch(s) ? t.sts : BS_ERR_UNKNOWN;
        }
    }

    return touch(s) ? BS_ERR_NONE : BS_ERR_UNKNOWN;
}

void Parser::sync_task(ThreadCtx& t)
{
    std::unique_lock<std::mutex> lock(t.mtx);

    while (t.state != DONE)
        t.cv.wait_for(lock, std::chrono::milliseconds(20));

    t.state = WAIT;
}

void Parser::parseSD(Slice& s)
{
    if (!(m_mode & INIT_MODE_PARSE_SD))
        return;

    if ((m_mode & INIT_MODE_MT_SD) == INIT_MODE_MT_SD)
    {
        std::unique_lock<std::mutex> lock0(m_ssync_mtx);
        Sync sync(*this);
        ThreadCtx* pt = sync.GetTask(WAIT);

        if (!pt)
        {
            //no free threads, wait until any task done
            pt = sync.GetTask(DONE);

            if (!pt)
                throw Exception(BS_ERR_UNKNOWN);
            if (pt->sts)
                throw Exception(pt->sts);
        }

        //submit new task
        {
            auto& t = *pt;
            std::unique_lock<std::mutex> lock(t.mtx);

            t.parser.newSlice(s);

            Bs32u maxRBSP = (Bs32u)t.parser.PicSizeInMbs * ((t.parser.RawMbBits + 128) / 8);
            Bs8u bit = (Bs8u)GetBitOffset();

            if (t.RBSP.size() < maxRBSP)
                t.RBSP.resize(maxRBSP);

            t.parser.Reset(&t.RBSP[0], ExtractRBSP(&t.RBSP[0], maxRBSP) + 2, bit);
            t.state = SUBMITTED;
            this->lock(&s);
            t.cv.notify_one();
        }
    }
    else
    {
        newSlice(s);

        try
        {
            if (s.pps->entropy_coding_mode_flag)
                parseSD_CABAC(s);
            else
                parseSD_CAVLC(s);
        }
        catch (EndOfBuffer)
        {
            CopyMBs(s, &m_mb[0]);
            throw EndOfBuffer();
        }

        CopyMBs(s, &m_mb[0]);
    }
}

void ThreadProc(ThreadCtx& self, SyncPoint& sp, Bs8u id)
{
#ifdef __BS_TRACE__
    char fname[] = "X_bs_thread.log";
    fname[0] = '0' + (id % 10);
#pragma warning(disable:4996)
    FILE* log = fopen(fname, "w");
    self.parser.SetLog(log);
#endif

    for (;;)
    {
        std::unique_lock<std::mutex> lock(self.mtx);

        switch (self.state)
        {
        case SUBMITTED:
            self.state = BUSY;
            self.sts = BS_ERR_NONE;

            try
            {
#ifdef __BS_TRACE__
                fprintf(log, "POC = %d\n", self.parser.cSlice().POC); fflush(log);
#endif
                if (self.parser.cSlice().pps->entropy_coding_mode_flag)
                    self.parser.parseSD_CABAC(self.parser.cSlice());
                else
                    self.parser.parseSD_CAVLC(self.parser.cSlice());
            }
            catch (std::bad_alloc)
            {
                self.sts = BS_ERR_MEM_ALLOC;
            }
            catch (Exception ex)
            {
                self.sts = ex.Err();
            }
            catch (...)
            {
                self.sts = BS_ERR_UNKNOWN;
            }

            CopyMBs(*self.allocator, self.parser.cSlice(), &self.parser.m_mb[0]);
            self.allocator->unlock(&self.parser.cSlice());

            self.state = DONE;

            {
                std::unique_lock<std::mutex> lock1(sp.mtx);
                sp.cv.notify_one();
            }

        case DONE:
        case WAIT:
            self.cv.wait(lock);
            break;
        case EXIT:
        default:
#ifdef __BS_TRACE__
            fclose(log);
#endif
            return;
        }

    }
}
