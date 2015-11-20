#if (defined(LINUX32) || defined(LINUX64))
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <sched.h>
#include <sys/syscall.h>
#endif

#include <vector>
#include <thread>

#include "ts_encoder.h"
#include "ts_parser.h"
#include "ts_struct.h"

namespace threading_mfxinitex
{

class TestSuite : tsSession
{
public:
    TestSuite() {}
    ~TestSuite() {}
#if (defined(LINUX32) || defined(LINUX64))
    enum
    {
        TS_SCHED_FIFO  = SCHED_FIFO,
        TS_SCHED_RR    = SCHED_RR,
        TS_SCHED_OTHER = SCHED_OTHER
    };
#else
    enum
    {
        TS_SCHED_FIFO = 1,
        TS_SCHED_RR,
        TS_SCHED_OTHER
    };
#endif
    enum
    {
        INIT_PAR = 1,
        THREAD_PAR
    };

    enum
    {
        EXT_BUF = 1
    };

    static const unsigned int n_cases;
    int RunTest(unsigned int id);
    struct tc_struct
    {
        mfxStatus sts;
        mfxU32 mode;
        struct f_pair
        {
            mfxU32 ext_type;
            const  tsStruct::Field* f;
            mfxU32 v;
        } set_par[MAX_NPARS];
    };
    static const tc_struct test_case[];
};

const TestSuite::tc_struct TestSuite::test_case[] =
{
    {/*00*/ MFX_ERR_NONE, 0},
    {/*01*/ MFX_ERR_UNSUPPORTED, 0, {INIT_PAR, &tsStruct::mfxInitParam.ExternalThreads, 1}},
    {/*02*/ MFX_ERR_UNSUPPORTED, EXT_BUF},
    {/*03*/ MFX_ERR_NONE, EXT_BUF, {THREAD_PAR, &tsStruct::mfxExtThreadsParam.NumThread, 0}}, // NOTE here we need expect number of spawned threads equal to number of cores!!!
    {/*04*/ MFX_ERR_UNSUPPORTED, EXT_BUF, {THREAD_PAR, &tsStruct::mfxExtThreadsParam.NumThread, 1}},
    {/*05*/ MFX_ERR_NONE, EXT_BUF, {THREAD_PAR, &tsStruct::mfxExtThreadsParam.NumThread, 2}},
    {/*06*/ MFX_ERR_NONE, EXT_BUF, {THREAD_PAR, &tsStruct::mfxExtThreadsParam.NumThread, 4}},

    // THE following scheduling modes are valid for root only!!!
    {/*07*/ MFX_ERR_UNSUPPORTED, EXT_BUF, {
        {THREAD_PAR, &tsStruct::mfxExtThreadsParam.NumThread, 2},
        {THREAD_PAR, &tsStruct::mfxExtThreadsParam.SchedulingType, TS_SCHED_FIFO},
        {THREAD_PAR, &tsStruct::mfxExtThreadsParam.Priority, 0} // unsupported since priority=0
    }},
    {/*08*/ MFX_ERR_NONE, EXT_BUF, {
        {THREAD_PAR, &tsStruct::mfxExtThreadsParam.NumThread, 2},
        {THREAD_PAR, &tsStruct::mfxExtThreadsParam.SchedulingType, TS_SCHED_FIFO},
        {THREAD_PAR, &tsStruct::mfxExtThreadsParam.Priority, 1}
    }},
    {/*09*/ MFX_ERR_NONE, EXT_BUF, {
        {THREAD_PAR, &tsStruct::mfxExtThreadsParam.NumThread, 2},
        {THREAD_PAR, &tsStruct::mfxExtThreadsParam.SchedulingType, TS_SCHED_FIFO},
        {THREAD_PAR, &tsStruct::mfxExtThreadsParam.Priority, 99}
    }},
    {/*10*/ MFX_ERR_NONE, EXT_BUF, {
        {THREAD_PAR, &tsStruct::mfxExtThreadsParam.NumThread, 2},
        {THREAD_PAR, &tsStruct::mfxExtThreadsParam.SchedulingType, TS_SCHED_RR},
        {THREAD_PAR, &tsStruct::mfxExtThreadsParam.Priority, 0}
    }},

    // THE following scheduling modes are valid for any user!!!
    {/*11*/ MFX_ERR_NONE, EXT_BUF, {
        {THREAD_PAR, &tsStruct::mfxExtThreadsParam.NumThread, 2},
        {THREAD_PAR, &tsStruct::mfxExtThreadsParam.SchedulingType, TS_SCHED_OTHER},
        {THREAD_PAR, &tsStruct::mfxExtThreadsParam.Priority, 0}
    }},
    {/*12*/ MFX_ERR_UNSUPPORTED, EXT_BUF, {
        { THREAD_PAR, &tsStruct::mfxExtThreadsParam.NumThread, 2 },
        { THREAD_PAR, &tsStruct::mfxExtThreadsParam.SchedulingType, TS_SCHED_OTHER },
        { THREAD_PAR, &tsStruct::mfxExtThreadsParam.Priority, 1 }
    }},
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

#if (defined(LINUX32) || defined(LINUX64))
static int my_dir_filter(const struct dirent* dir_ent)
{
    if (!dir_ent) return 0;
    if (!strcmp(dir_ent->d_name, ".")) return 0;
    if (!strcmp(dir_ent->d_name, "..")) return 0;
    return 1;
}

struct ThreadInfo {
    pid_t tid;
    int scheduler;
    int priority;
 
    // matches item by tid
    bool operator==(const ThreadInfo& ti) {
      return (tid == ti.tid);
    }
};

// function returns vector of thread IDs for the process identified by PID
std::vector<ThreadInfo> get_threads(pid_t pid)
{
    std::vector<ThreadInfo> threads;
    char proc_pid_tasks[128] = {0};
    struct dirent** dir_entries = NULL;
    ThreadInfo info;

    sprintf(proc_pid_tasks, "/proc/%d/task/", pid);

    int entries_num = scandir(proc_pid_tasks, &dir_entries, my_dir_filter, alphasort);
    if (!entries_num) {
        return threads;
    }

    for (int i = 0; i < entries_num; ++i) {
        struct sched_param param;

        memset(&param, 0, sizeof(param));
        // getting tid
        info.tid = atoi(dir_entries[i]->d_name);
        // getting scheduler
        info.scheduler = sched_getscheduler(info.tid);
        // getting priority
        sched_getparam(info.tid, &param);
        info.priority = param.sched_priority;

        //printf("TID=%d: SC=%d: PR=%d, ", info.tid, info.scheduler, info.priority);

        threads.push_back(info);
        free(dir_entries[i]);
    }

    free(dir_entries);
    return threads;
}
#endif

int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    tc_struct tc = test_case[id];
    mfxStatus expect = tc.sts;

    mfxExtThreadsParam& tp = m_init_par;

    m_init_par.ExternalThreads = 0;
    m_init_par.Implementation = g_tsImpl;
    m_init_par.Version = g_tsVersion;

    SETPARS(&m_init_par, INIT_PAR);

    if (tc.mode == EXT_BUF)
    {
        tp.NumThread = 1;

        SETPARS(&tp, THREAD_PAR);

        if (tp.NumThread == 0)
        {
            tp.NumThread = std::thread::hardware_concurrency();
        }

        if (g_tsOSFamily == MFX_OS_FAMILY_WINDOWS)
            expect = MFX_ERR_UNSUPPORTED;
#if (defined(LINUX32) || defined(LINUX64))
        if (((tp.SchedulingType == TS_SCHED_FIFO) || (tp.SchedulingType == TS_SCHED_RR)) &&
            geteuid()) { // this last condition checks whether test was started under root
            expect = MFX_ERR_UNSUPPORTED;
        }
#endif
    }
    else // case 0 - no extParam
    {
        tp.NumThread = std::thread::hardware_concurrency();
    }

    g_tsStatus.expect(expect);

#if (defined(LINUX32) || defined(LINUX64))
    std::vector<ThreadInfo> threads_a; // this will be the list of TIDs for the current process before call to MFXInitEx()
    std::vector<ThreadInfo> threads_b; // and this will be the list of TIDs after call to MFXInitEx()

    threads_a = get_threads(syscall(SYS_getpid));
#endif

    MFXInitEx();

#if (defined(LINUX32) || defined(LINUX64))
    threads_b = get_threads(syscall(SYS_getpid));

    if (expect != MFX_ERR_UNSUPPORTED) { // NOTE TODO maybe there is no need to run checks below sometimes
        // removing thread(s) existing before the call to MFXInitEx()
        threads_b.erase(
          std::remove_if(
            threads_b.begin(),
            threads_b.end(),
            [&](ThreadInfo ti) {
              return (std::find(threads_a.begin(), threads_a.end(), ti) != threads_a.end())? true: false;
            }),
          threads_b.end());

        // ok, now we have in the 'threads_b' only threads spawned by MFXInitEx() and can check our expectations
        g_tsLog << "Checking number of threads: expected=" << tp.NumThread << ", real=" << threads_b.size() << "\n";
        if (tp.NumThread != threads_b.size())
        {
            g_tsLog << "ERROR: NumThreads real " << threads_b.size() << " != " << tp.NumThread << " (expected)\n";
            g_tsStatus.check(MFX_ERR_ABORTED);
        }
        for (size_t i=0; i < threads_b.size(); ++i)
        {
            g_tsLog << "Checking TID=" << threads_b[i].tid
                    << ": real scheduler=" << threads_b[i].scheduler << ", priority=" << threads_b[i].priority
                    << " VS. expected scheduler=" << tp.SchedulingType << ", priority=" << tp.Priority << "\n";
            if (threads_b[i].scheduler != tp.SchedulingType)
            {
                g_tsLog << "ERROR: TID[" << threads_b[i].tid << "] SchedulingType real " << threads_b[i].scheduler
                        << " != " << tp.SchedulingType << " (expected)\n";
                g_tsStatus.check(MFX_ERR_ABORTED);
            }
            if (threads_b[i].priority != tp.Priority)
            {
                g_tsLog << "ERROR: TID[" << threads_b[i].tid << "] Priority real " << threads_b[i].priority
                        << " != " << tp.Priority << " (expected)\n";
                g_tsStatus.check(MFX_ERR_ABORTED);
            }
        }
    }
#endif

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(threading_mfxinitex);
};
