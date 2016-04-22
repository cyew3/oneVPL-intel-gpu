/* /////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2014-2016 Intel Corporation. All Rights Reserved.
//
*/

#pragma once

#include "gtest/gtest.h"
#include "ts_trace.h"
#include "ts_plugin.h"
#include "ts_streams.h"
#include "test_sample_allocator.h"
#include "ts_platforms.h"
#include "ts_ext_buffers.h"

typedef enum {
      tsOK = 0
    , tsFAIL
} tsRes;

typedef struct {
    mfxU32      lowpower;
    std::string cfg_filename;
} tsConfig;

typedef struct {
    mfxU32 BufferId;
    mfxU32 BufferSz;
    const char *string;
} BufferIdToString;

#define EXTBUF(TYPE, ID) {ID, sizeof(TYPE), #TYPE},
static BufferIdToString g_StringsOfBuffers[] =
{
#include "ts_ext_buffers_decl.h"
};
#undef EXTBUF

class MFXVideoTest : public ::testing::TestWithParam<unsigned int>
{
    void SetUp();
    void TearDown();
};

class tsStatus
{
public:
    mfxStatus m_status;
    mfxStatus m_expected;
    bool      m_failed;
    bool      m_throw_exceptions;
    mfxU16    m_disable;

    tsStatus();
    ~tsStatus();

    bool check(mfxStatus status);
    bool check();
    inline void      expect (mfxStatus expected) { m_expected = expected; }
    inline mfxStatus get    ()                   { return m_status; }
    inline void      disable_next_check()        { m_disable = 1; }
    inline void      disable()                   { m_disable = 2; }
    inline void      enable()                    { m_disable = 0; }
};

extern mfxIMPL      g_tsImpl;
extern mfxVersion   g_tsVersion;
extern HWType       g_tsHWtype;
extern OSFamily     g_tsOSFamily;
extern OSWinVersion g_tsWinVersion;
extern bool         g_tsIsSSW;
extern tsStatus     g_tsStatus;
extern mfxU32       g_tsTrace;
extern tsTrace      g_tsLog;
extern tsPlugin     g_tsPlugin;
extern tsStreamPool g_tsStreamPool;
extern tsConfig     g_tsConfig;


#define _TS_REG_TEST_SUITE(name, routine, n_cases, q_mode) \
    class name : public MFXVideoTest{};\
    TEST_P(name,) { g_tsStreamPool.Init(q_mode); if((routine)(GetParam())) {ADD_FAILURE();} g_tsStreamPool.Close(); }\
    INSTANTIATE_TEST_CASE_P(, name, ::testing::Range<unsigned int>(0, (n_cases)));

#define TS_REG_TEST_SUITE(name, routine, n_cases) \
    _TS_REG_TEST_SUITE(name, routine, n_cases, false)\
    _TS_REG_TEST_SUITE(query_streams_##name, routine, n_cases, true)

#define _TS_REG_TEST_SUITE_CLASS(name, q_mode)\
    class name : public MFXVideoTest{};\
    TEST_P(name,) { g_tsStreamPool.Init(q_mode); TestSuite ts; if(ts.RunTest(GetParam())) {ADD_FAILURE();} g_tsStreamPool.Close(); }\
    INSTANTIATE_TEST_CASE_P(, name, ::testing::Range<unsigned int>(0, TestSuite::n_cases));

#define QUERY_STREAMS(name)  query_streams_##name
#define TS_REG_TEST_SUITE_CLASS(name)\
    _TS_REG_TEST_SUITE_CLASS(name, false)\
    _TS_REG_TEST_SUITE_CLASS(QUERY_STREAMS(name), true)

#define INC_PADDING() g_tsLog.inc_offset();
#define DEC_PADDING() g_tsLog.dec_offset();
#define TRACE_FUNCN(n, name, p1, p2, p3, p4, p5, p6, p7)\
    g_tsLog << "---------------------------------START-----------------------------------------\n";\
    g_tsLog << "CALL: " << #name << "\n";\
    INC_PADDING();\
    if(g_tsTrace){\
        if((n) > 0) g_tsLog << g_tsLog.m_off << #p1 " = " << p1 << "\n";\
        if((n) > 1) g_tsLog << g_tsLog.m_off << #p2 " = " << p2 << "\n";\
        if((n) > 2) g_tsLog << g_tsLog.m_off << #p3 " = " << p3 << "\n";\
        if((n) > 3) g_tsLog << g_tsLog.m_off << #p4 " = " << p4 << "\n";\
        if((n) > 4) g_tsLog << g_tsLog.m_off << #p5 " = " << p5 << "\n";\
        if((n) > 5) g_tsLog << g_tsLog.m_off << #p6 " = " << p6 << "\n";\
        if((n) > 8) g_tsLog << g_tsLog.m_off << #p7 " = " << p6 << "\n";\
    }\
    g_tsLog << "CALL: " << #name << "(";\
    if((n) > 0) g_tsLog << #p1;\
    if((n) > 1) g_tsLog << ", " << #p2;\
    if((n) > 2) g_tsLog << ", " << #p3;\
    if((n) > 3) g_tsLog << ", " << #p4;\
    if((n) > 4) g_tsLog << ", " << #p5;\
    if((n) > 5) g_tsLog << ", " << #p6;\
    if((n) > 6) g_tsLog << ", " << #p7;\
    g_tsLog << ");" << "\n";\
    DEC_PADDING();
#define TRACE_FUNC6(name, p1, p2, p3, p4, p5, p6) TRACE_FUNCN(6, name, p1, p2, p3, p4, p5, p6, 0)
#define TRACE_FUNC5(name, p1, p2, p3, p4, p5) TRACE_FUNCN(5, name, p1, p2, p3, p4, p5, 0, 0)
#define TRACE_FUNC4(name, p1, p2, p3, p4) TRACE_FUNCN(4, name, p1, p2, p3, p4, 0, 0, 0)
#define TRACE_FUNC3(name, p1, p2, p3) TRACE_FUNCN(3, name, p1, p2, p3, 0, 0, 0, 0)
#define TRACE_FUNC2(name, p1, p2) TRACE_FUNCN(2, name, p1, p2, 0, 0, 0, 0, 0)
#define TRACE_FUNC1(name, p1) TRACE_FUNCN(1, name, p1, 0, 0, 0, 0, 0, 0)
#define TRACE_FUNC0(name) TRACE_FUNCN(0, name, 0, 0, 0, 0, 0, 0, 0)
#define TS_TRACE(par)   if(g_tsTrace) {INC_PADDING(); g_tsLog << g_tsLog.m_off << #par " = " << par << "\n"; DEC_PADDING();}
#define TS_CHECK        { if(g_tsStatus.m_failed) return 1; }
#define TS_CHECK_MFX    { if(g_tsStatus.m_failed) return g_tsStatus.get(); }
#define TS_FAIL_TEST(err, sts) { g_tsLog << "ERROR: " << err << "\n"; ADD_FAILURE(); return sts; }
#define TS_MAX(x,y) ((x)>(y) ? (x) : (y))
#define TS_MIN(x,y) ((x)<(y) ? (x) : (y))
#define TS_START try {
#define TS_END   } catch(tsRes r) { return r; }

#define TS_HW_ALLOCATOR_TYPE (!!((g_tsImpl) & 0xF00) ? frame_allocator::HARDWARE_DX11 : frame_allocator::HARDWARE)

#define MAX_NPARS 15

#define SETPARS(p, type)                                                                                        \
for(mfxU32 i = 0; i < MAX_NPARS; i++)                                                                           \
{                                                                                                               \
    if(tc.set_par[i].f && tc.set_par[i].ext_type == type)                                                       \
    {                                                                                                           \
        SetParam(p, tc.set_par[i].f->name, tc.set_par[i].f->offset, tc.set_par[i].f->size, tc.set_par[i].v);    \
    }                                                                                                           \
}

std::string ENV(const char* name, const char* def);
void set_brc_params(tsExtBufType<mfxVideoParam>* p);

bool operator == (const mfxFrameInfo&, const mfxFrameInfo&);
bool operator == (const mfxFrameData&, const mfxFrameData&);

void GetBufferIdSz(const std::string& name, mfxU32& bufId, mfxU32& bufSz);

void SetParam(void* base, const std::string name, const mfxU32 offset, const mfxU32 size, mfxU64 value);
void SetParam(tsExtBufType<mfxVideoParam>* base,  const std::string name, const mfxU32 offset, const mfxU32 size, mfxU64 value);

