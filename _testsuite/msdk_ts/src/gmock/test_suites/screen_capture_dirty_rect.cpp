/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2015-2017 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#include "ts_decoder.h"
#include "ts_struct.h"
#include "mfxsc.h"

#if defined(_WIN32) || defined(_WIN64)
#include "windows.h"
#include "psapi.h"
#pragma comment(lib, "Gdi32.lib")
#endif

namespace screen_capture_dirty_rect
{
#if !defined(MSDK_ALIGN16)
#define MSDK_ALIGN16(value)                      (((value + 15) >> 4) << 4) // round up to a multiple of 16
#endif

#if defined(_WIN32) || defined(_WIN64)
class MemoryTracker : public tsSurfaceProcessor
{
public:
    MemoryTracker()
    {
        m_process = 0;
        threshold = 400*1024; //allow 400 kb fluctuations
        skip = 10;
        count = 0;
        memset(&m_counters_init,0,sizeof(m_counters_init));
        memset(&m_counters_runtime,0,sizeof(m_counters_runtime));

        m_counters_init.cb = sizeof(m_counters_init);
        m_counters_runtime.cb = sizeof(m_counters_runtime);
    }
    virtual ~MemoryTracker() { }

    mfxStatus Start()
    {
        m_process = GetCurrentProcess();
        if(!m_process)
        {
            g_tsLog << "ERROR: (in test) failed to get current process size\n";
            return MFX_ERR_ABORTED;
        }

        if( !GetProcessMemoryInfo ( m_process, (PROCESS_MEMORY_COUNTERS*) &m_counters_init, sizeof ( m_counters_init ) ) )
            return MFX_ERR_ABORTED;

        return MFX_ERR_NONE;
    }

    mfxStatus Stop()
    {
        if(!m_process)
            return MFX_ERR_NOT_INITIALIZED;

        CoFreeUnusedLibrariesEx(0,0);

        PROCESS_MEMORY_COUNTERS_EX counters;
        memset(&counters,0,sizeof(counters));
        counters.cb = sizeof(counters);
        if( !GetProcessMemoryInfo ( m_process, (PROCESS_MEMORY_COUNTERS*) &counters, sizeof ( counters ) ) )
            return MFX_ERR_ABORTED;

        g_tsLog << "  Process size before MFXInit = " << m_counters_init.PrivateUsage <<" bytes \n";
        g_tsLog << "  Process size after MFXClose = " << counters.PrivateUsage <<" bytes \n";

        //Let's put 33% threshold
        if((m_counters_init.PrivateUsage + m_counters_init.PrivateUsage / 3) < counters.PrivateUsage)
        {
            g_tsLog << "ERROR: Possible memory leaks detected\n";
            return MFX_ERR_ABORTED;
        }

        CloseHandle(m_process);
        m_process = 0;

        return MFX_ERR_NONE;
    }

    mfxStatus ProcessSurface(mfxFrameSurface1& s)
    {
        s;
        ++count;

        PROCESS_MEMORY_COUNTERS_EX counters;
        memset(&counters,0,sizeof(counters));
        counters.cb = sizeof(counters);

        if(skip == count)
        {
            GetProcessMemoryInfo ( m_process, (PROCESS_MEMORY_COUNTERS*) &counters, sizeof ( counters ) );
            m_counters_runtime = counters;

            return MFX_ERR_NONE;
        }
        else if(count > skip)
        {
            if(!m_process || (0 == m_counters_runtime.PrivateUsage))
                return MFX_ERR_NOT_INITIALIZED;

            if( !GetProcessMemoryInfo ( m_process, (PROCESS_MEMORY_COUNTERS*) &counters, sizeof ( counters ) ) )
            {
                g_tsLog << "ERROR: (in test) failed to get current process size\n";
                return MFX_ERR_ABORTED;
            }

            if((m_counters_runtime.PrivateUsage + threshold) < counters.PrivateUsage)
            {
                g_tsLog << "ERROR: Unexpected runtime memory growth detected\n";
                return MFX_ERR_ABORTED;
            }
        }

        return MFX_ERR_NONE;
    }

private:
    HANDLE m_process;
    mfxU32 threshold; //bytes
    mfxU32 skip;
    mfxU32 count;
    PROCESS_MEMORY_COUNTERS_EX m_counters_init;
    PROCESS_MEMORY_COUNTERS_EX m_counters_runtime;
};
#else
class MemoryTracker : public tsSurfaceProcessor
{
public:
    MemoryTracker() { }
    virtual ~MemoryTracker() { }
    mfxStatus Start() {return MFX_ERR_UNSUPPORTED;}
    mfxStatus Stop() {return MFX_ERR_UNSUPPORTED;}
    mfxStatus ProcessSurface(mfxFrameSurface1& s) {s; return MFX_ERR_UNSUPPORTED;}
};
#endif

const mfxExtDirtyRect* GetDirtyRectBuffer(const mfxFrameSurface1& s)
{
    if(!s.Data.NumExtParam || !s.Data.ExtParam)
        return 0;
    for(mfxU32 i = 0; i < s.Data.NumExtParam; ++i)
    {
        if(!s.Data.ExtParam[i])
            continue;
        if(MFX_EXTBUFF_DIRTY_RECTANGLES == s.Data.ExtParam[i]->BufferId && sizeof(mfxExtDirtyRect) == s.Data.ExtParam[i]->BufferSz)
            return (mfxExtDirtyRect*) s.Data.ExtParam[i];
    }
    return 0;
}

class SimpleDirtyRectChecker : public tsSurfaceProcessor
{
private:
    const bool isDrEnabled;
    //forbid constructors
    SimpleDirtyRectChecker(const SimpleDirtyRectChecker& that);
    SimpleDirtyRectChecker();

public:
    SimpleDirtyRectChecker(const mfxExtScreenCaptureParam& ext_par) : isDrEnabled( MFX_CODINGOPTION_ON == ext_par.EnableDirtyRect )
    {
    }
    ~SimpleDirtyRectChecker()
    {
    }

    mfxStatus ProcessSurface(mfxFrameSurface1& s)
    {
        const mfxExtDirtyRect* dr = GetDirtyRectBuffer(s);
        EXPECT_NE(nullptr, dr);

        size_t i = 0;
        for(i = 0; i < dr->NumRect; ++i)
        {
            if(isDrEnabled)
            {
                //mfxRect rect;
                EXPECT_NE(0u, (dr->Rect[i].Left +   dr->Rect[i].Top + dr->Rect[i].Right + dr->Rect[i].Bottom) ) << "ERROR: dirty rect has zero coordinates!\n";
                EXPECT_NE(0u, (dr->Rect[i].Right -  dr->Rect[i].Left) )   << "ERROR: dirty rect has zero width!\n";
                EXPECT_NE(0u, (dr->Rect[i].Bottom - dr->Rect[i].Top) )    << "ERROR: dirty rect has zero height!\n";
                EXPECT_LE(dr->Rect[i].Right,       s.Info.Width)         << "ERROR: right coordinate of dirty rect > frame width!\n";
                EXPECT_LE(dr->Rect[i].Bottom,      s.Info.Height)        << "ERROR: bottom coordinate of dirty rect > frame height!\n";
                EXPECT_LE(dr->Rect[i].Left,        dr->Rect[i].Right)    << "ERROR: left coordinate of dirty rect > right coordinate!\n";
                EXPECT_LE(dr->Rect[i].Top,         dr->Rect[i].Bottom)   << "ERROR: top coordinate of dirty rect > bootom coordinate!\n";

                for(size_t j = 0; j < dr->NumRect; ++j)
                {
                    if(i != j)
                    {
                        EXPECT_NE(0, memcmp( &(dr->Rect[i]), &(dr->Rect[j]), sizeof(dr->Rect[0]))) << "ERROR: there are two equal dirty rects: " << i << " and " << j << "!\n;";
                    }
                }
            }
            else
            {
                EXPECT_EQ(0u, dr->NumRect) << "ERROR: Although Dirty Rect detection is turned off, some dirty rects reported!\n";
            }
        }

        const size_t maxRects = sizeof(dr->Rect) / sizeof(dr->Rect[0]);
        for(i; i < maxRects; ++i)
        {
            if(isDrEnabled)
                EXPECT_EQ(0u, (dr->Rect[i].Left + dr->Rect[i].Top + dr->Rect[i].Right + dr->Rect[i].Bottom) ) << "ERROR: Although only "<< dr->NumRect <<"rects reported, rect #"<< i <<"is dirty!\n";
            else
                EXPECT_EQ(0u, (dr->Rect[i].Left + dr->Rect[i].Top + dr->Rect[i].Right + dr->Rect[i].Bottom) ) << "ERROR: Dirty rects must not be reported (EnableDirtyRect = OFF)!\n";
        }

        return MFX_ERR_NONE;
    }
};

#if 0 //reserved for "render -> capture -> check detected dirty rects" test
class Renderer
{
protected:
    bool classRegistered;
    std::vector<HWND>  windows;

public:
    Renderer() {};
    ~Renderer()
    {
        if(classRegistered)
        {
            UnregisterClass(TEXT("WindowClass"), GetModuleHandle(NULL));
        }
    };

    static LRESULT CALLBACK WindowProc(HWND hWnd,
                            UINT message,
                            WPARAM wParam,
                            LPARAM lParam)
    {
        switch(message)
        {
            case WM_PAINT:
                {
                    PAINTSTRUCT ps;
                    HDC hdc = BeginPaint(hWnd, &ps);

                    RECT rec;
                    //SetRect(rect, x ,y ,width, height)
                    SetRect(&rec,10,10,280,280);
                    TCHAR text[ ] = TEXT("MSDK Screen capture\nbehavioral test\nin process");
                    //DrawText(HDC, text, text length, drawing area, parameters "DT_XXX")
                    DrawText(hdc, text, ARRAYSIZE(text), &rec, DT_TOP|DT_LEFT);

                    EndPaint(hWnd, &ps);
                    ReleaseDC(hWnd, hdc);
                } break;
            case WM_DESTROY:
                {
                    return 0;
                } break;

            default:
                break;
        }
        return DefWindowProc (hWnd, message, wParam, lParam);
    }

    mfxStatus DrawWindow(HWND& wind, mfxU32 x = 0, mfxU32 y = 0, mfxU32 w = 128, mfxU32 h = 128)
    {
        HWND window = 0;
        if(!classRegistered)
        {
            WNDCLASSEX wc = {0};
            wc.cbSize = sizeof(WNDCLASSEX);
            wc.style = CS_HREDRAW | CS_VREDRAW;
            wc.lpfnWndProc   = WindowProc;
            wc.hInstance     = GetModuleHandle(NULL);
            wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
            wc.lpszClassName = TEXT("WindowClass");
            wc.hbrBackground = (HBRUSH)COLOR_WINDOW;

            RegisterClassEx(&wc);
            HRESULT hr = GetLastError();
            EXPECT_EQ(S_OK, hr) << "ERROR: (in test) Failed to register window class!\n";
        }

        window = CreateWindowEx(NULL,
                              TEXT("WindowClass"),
                              TEXT("MSDK Screen Capture GMock test"),
                              WS_OVERLAPPEDWINDOW,
                              x, y,
                              w, h,
                              NULL,
                              NULL,
                              GetModuleHandle(NULL),
                              NULL);

        ShowWindow(window, SW_SHOW);
        UpdateWindow(window);
        HRESULT hr = GetLastError();
        EXPECT_EQ(S_OK, hr) << "ERROR: (in test) Failed to create window!\n";

        windows.push_back(window);
        wind = window;

        return MFX_ERR_NONE;
    }

    mfxStatus DestroyWindow(HWND& wind)
    {
        std::vector<HWND>::iterator it = std::find(windows.begin(), windows.end(), wind);
        if(it != windows.end())
        {
            windows.erase(it);

            ShowWindow(wind, SW_HIDE);
            DestroyWindow(wind);
            wind = 0;
        }
        else
        {
            return MFX_ERR_NOT_FOUND;
        }

        return MFX_ERR_NONE;
    }
};
#endif //#if 0

class TestSuite : tsVideoDecoder, tsSurfaceProcessor
{
public:
    TestSuite() : tsVideoDecoder(MFX_CODEC_CAPTURE, true, MFX_MAKEFOURCC('C','A','P','T'))
    {
        m_surf_processor = this;
    }
    ~TestSuite() {}
    int RunTest(unsigned int id);
    static const unsigned int n_cases;

    struct f_pair
    {
        mfxU32 ext_type;
        const  tsStruct::Field* f;
        mfxU32 v;
    };

private:
    enum
    {
        MFXPAR = 1,
        MFXSC,
        MFXDR,
    };

    enum
    {
        CHECK_MEMORY = 1,
        ADD_WRONG_BUFFER,
        ADD_WRONG_BUFFER_RT,
        WRONGLY_ATTACHED,
    };

    struct tc_struct;

    typedef mfxStatus (*callback)(TestSuite* pthis, const tc_struct& tc);
    struct tc_struct
    {
        callback  testFnc;
        mfxStatus sts;
        mfxU32    mode;
        f_pair    set_par[MAX_NPARS];
    };

    static const tc_struct test_case[];

    static mfxStatus t_query        (TestSuite* pthis, const tc_struct& tc);
    static mfxStatus t_query_io_surf(TestSuite* pthis, const tc_struct& tc);
    static mfxStatus t_init         (TestSuite* pthis, const tc_struct& tc);
    static mfxStatus t_reset        (TestSuite* pthis, const tc_struct& tc);
    static mfxStatus t_getvideoparam(TestSuite* pthis, const tc_struct& tc);
    static mfxStatus t_decodeframeasync_simple(TestSuite* pthis, const tc_struct& tc);
    static const TestSuite::f_pair  sys_mem;
    static const TestSuite::f_pair  vid_mem;
    static const TestSuite::f_pair  rgb4_fourcc;
    static const TestSuite::f_pair  chroma_444;
};

static const tsStruct::Field* Async (&tsStruct::mfxVideoParam.AsyncDepth);
static const tsStruct::Field* Width (&tsStruct::mfxVideoParam.mfx.FrameInfo.Width);
static const tsStruct::Field* Height(&tsStruct::mfxVideoParam.mfx.FrameInfo.Height);
static const tsStruct::Field* CropW (&tsStruct::mfxVideoParam.mfx.FrameInfo.CropW);
static const tsStruct::Field* CropH (&tsStruct::mfxVideoParam.mfx.FrameInfo.CropH);
static const tsStruct::Field* EnableDirtyRect(&tsStruct::mfxExtScreenCaptureParam.EnableDirtyRect);

const TestSuite::f_pair TestSuite::sys_mem     = {MFXPAR, &tsStruct::mfxVideoParam.IOPattern,                  MFX_IOPATTERN_OUT_SYSTEM_MEMORY };
const TestSuite::f_pair TestSuite::vid_mem     = {MFXPAR, &tsStruct::mfxVideoParam.IOPattern,                  MFX_IOPATTERN_OUT_VIDEO_MEMORY  };
const TestSuite::f_pair TestSuite::rgb4_fourcc = {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC,       MFX_FOURCC_RGB4                 };
const TestSuite::f_pair TestSuite::chroma_444  = {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.ChromaFormat, MFX_CHROMAFORMAT_YUV444         };

const TestSuite::f_pair v(const mfxU32 ext_type, const tsStruct::Field* field, const mfxU32 value)
{
    TestSuite::f_pair pair = {ext_type, field, value};
    return pair;
}
const TestSuite::f_pair v(const tsStruct::Field* field, const mfxU32 value)
{
    if(std::string::npos != field->name.find("mfxVideoParam"))
        return v(1, field, value);
    else if(std::string::npos != field->name.find("mfxExtScreenCaptureParam"))
        return v(2, field, value);
    else if(std::string::npos != field->name.find("mfxExtDirtyRect"))
        return v(3, field, value);
    else
        return v(0,0,0);
}

const TestSuite::tc_struct TestSuite::test_case[] =
{
    {/*00*/ t_query,  MFX_ERR_NONE, 0, { sys_mem}                          },
    {/*01*/ t_query,  MFX_ERR_NONE, 0, { vid_mem }                         },
    {/*02*/ t_query,  MFX_ERR_NONE, 0, { sys_mem, rgb4_fourcc, chroma_444} },
    {/*03*/ t_query,  MFX_ERR_NONE, 0, { vid_mem, rgb4_fourcc, chroma_444} },
    {/*04*/ t_query,  MFX_ERR_NONE, 0, { v(EnableDirtyRect, 1                         ), sys_mem}},
    {/*05*/ t_query,  MFX_ERR_NONE, 0, { v(EnableDirtyRect, 0 | 0x10 | 0x20 | 0x30    ), sys_mem}},
    {/*06*/ t_query,  MFX_ERR_NONE,                0, { v(EnableDirtyRect, MFX_CODINGOPTION_UNKNOWN  ), sys_mem}},
    {/*07*/ t_query,  MFX_ERR_NONE,                0, { v(EnableDirtyRect, MFX_CODINGOPTION_OFF      ), vid_mem }},
    {/*08*/ t_query,  MFX_ERR_NONE, 0, { v(EnableDirtyRect, MFX_CODINGOPTION_ADAPTIVE ), vid_mem }},
    {/*09*/ t_query,  MFX_ERR_UNSUPPORTED, ADD_WRONG_BUFFER, { vid_mem }},
    {/*10*/ t_query,  MFX_ERR_NONE, 0, v(&tsStruct::mfxExtScreenCaptureParam.Header.BufferSz, (sizeof(mfxExtScreenCaptureParam)+1))},
    {/*11*/ t_query,  MFX_ERR_UNKNOWN,     WRONGLY_ATTACHED, { vid_mem }},
    {/*12*/ t_query,  MFX_ERR_NONE, 0, { v(Width,1920),v(CropW,1920),v(Height,1080),v(CropH,1080), v(Async,1), sys_mem }},
    {/*13*/ t_query,  MFX_ERR_NONE, 0, { v(Width,2160),v(CropW,2160),v(Height,1200),v(CropH,1200), v(Async,5), vid_mem }},
    {/*14*/ t_query,  MFX_ERR_NONE, 0, { v(Width,3840),v(CropW,3840),v(Height,2160),v(CropH,2160), v(Async,2), sys_mem, rgb4_fourcc, chroma_444 }},
    {/*15*/ t_query,  MFX_ERR_NONE, 0, { v(Width,4096),v(CropW,4096),v(Height,3840),v(CropH,3840), v(Async,3), vid_mem, rgb4_fourcc, chroma_444 }},
    {/*16*/ t_query,  MFX_ERR_NONE, 0, { v(Width,4096+16),v(CropW,4096+16),v(Height,4096+16),v(CropH,4096+16), v(Async,1), sys_mem }},
    {/*17*/ t_query,  MFX_ERR_NONE, 0, { v(Width,4096+16),v(CropW,4096+16),v(Height,4096+16),v(CropH,4096+16), v(Async,5), vid_mem }},
    {/*18*/ t_query,  MFX_ERR_NONE, 0, { v(Width,4096+16),v(CropW,4096+16),v(Height,4096+16),v(CropH,4096+16), v(Async,2), sys_mem, rgb4_fourcc, chroma_444 }},
    {/*19*/ t_query,  MFX_ERR_NONE, 0, { v(Width,4096+16),v(CropW,4096+16),v(Height,4096+16),v(CropH,4096+16), v(Async,3), vid_mem, rgb4_fourcc, chroma_444 }},

    {/*20*/ t_init,  MFX_ERR_NONE, 0, { sys_mem}                          },
    {/*21*/ t_init,  MFX_ERR_NONE, 0, { vid_mem }                         },
    {/*22*/ t_init,  MFX_ERR_NONE, 0, { sys_mem, rgb4_fourcc, chroma_444} },
    {/*23*/ t_init,  MFX_ERR_NONE, 0, { vid_mem, rgb4_fourcc, chroma_444} },
    {/*24*/ t_init,  MFX_ERR_NONE, 0, { v(EnableDirtyRect, 1                         ), sys_mem}},
    {/*25*/ t_init,  MFX_ERR_NONE, 0, { v(EnableDirtyRect, 0 | 0x10 | 0x20 | 0x30    ), sys_mem}},
    {/*26*/ t_init,  MFX_ERR_NONE,                0, { v(EnableDirtyRect, MFX_CODINGOPTION_UNKNOWN  ), sys_mem}},
    {/*27*/ t_init,  MFX_ERR_NONE,                0, { v(EnableDirtyRect, MFX_CODINGOPTION_OFF      ), vid_mem }},
    {/*28*/ t_init,  MFX_ERR_NONE, 0, { v(EnableDirtyRect, MFX_CODINGOPTION_ADAPTIVE ), vid_mem }},
    {/*29*/ t_init,  MFX_ERR_INVALID_VIDEO_PARAM, ADD_WRONG_BUFFER, { vid_mem }},
    {/*30*/ t_init,  MFX_ERR_NONE, 0, v(&tsStruct::mfxExtScreenCaptureParam.Header.BufferSz, (sizeof(mfxExtScreenCaptureParam)+1))},
    {/*31*/ t_init,  MFX_ERR_UNKNOWN,     WRONGLY_ATTACHED, { vid_mem }},
    {/*32*/ t_init,  MFX_ERR_NONE, 0, { v(Width,1920),v(CropW,1920),v(Height,1080),v(CropH,1080), sys_mem }},
    {/*33*/ t_init,  MFX_ERR_NONE, 0, { v(Width,1920),v(CropW,1920),v(Height,1080),v(CropH,1080), vid_mem }},
    {/*34*/ t_init,  MFX_ERR_NONE, 0, { v(Width,1920),v(CropW,1920),v(Height,1080),v(CropH,1080), sys_mem, rgb4_fourcc, chroma_444 }},
    {/*35*/ t_init,  MFX_ERR_NONE, 0, { v(Width,1920),v(CropW,1920),v(Height,1080),v(CropH,1080), vid_mem, rgb4_fourcc, chroma_444 }},
    {/*36*/ t_init,  MFX_ERR_NONE, 0, { v(Width,1920),v(CropW,1920),v(Height,1080),v(CropH,1080), v(Async,5), sys_mem }},
    {/*37*/ t_init,  MFX_ERR_NONE, 0, { v(Width,1920),v(CropW,1920),v(Height,1080),v(CropH,1080), v(Async,5), vid_mem }},
    {/*38*/ t_init,  MFX_ERR_NONE, 0, { v(Width,1920),v(CropW,1920),v(Height,1080),v(CropH,1080), v(Async,5), sys_mem, rgb4_fourcc, chroma_444 }},
    {/*39*/ t_init,  MFX_ERR_NONE, 0, { v(Width,1920),v(CropW,1920),v(Height,1080),v(CropH,1080), v(Async,5), vid_mem, rgb4_fourcc, chroma_444 }},
    {/*40*/ t_init,  MFX_ERR_NONE, 0, { v(Width,2160),v(CropW,2160),v(Height,1440),v(CropH,1440), sys_mem }},
    {/*41*/ t_init,  MFX_ERR_NONE, 0, { v(Width,2160),v(CropW,2160),v(Height,1440),v(CropH,1440), vid_mem }},
    {/*42*/ t_init,  MFX_ERR_NONE, 0, { v(Width,2160),v(CropW,2160),v(Height,1440),v(CropH,1440), sys_mem, rgb4_fourcc, chroma_444 }},
    {/*43*/ t_init,  MFX_ERR_NONE, 0, { v(Width,2160),v(CropW,2160),v(Height,1440),v(CropH,1440), vid_mem, rgb4_fourcc, chroma_444 }},
    {/*44*/ t_init,  MFX_ERR_NONE, 0, { v(Width,2160),v(CropW,2160),v(Height,1440),v(CropH,1440), v(Async,5), sys_mem }},
    {/*45*/ t_init,  MFX_ERR_NONE, 0, { v(Width,2160),v(CropW,2160),v(Height,1440),v(CropH,1440), v(Async,5), vid_mem }},
    {/*46*/ t_init,  MFX_ERR_NONE, 0, { v(Width,2160),v(CropW,2160),v(Height,1440),v(CropH,1440), v(Async,5), sys_mem, rgb4_fourcc, chroma_444 }},
    {/*47*/ t_init,  MFX_ERR_NONE, 0, { v(Width,2160),v(CropW,2160),v(Height,1440),v(CropH,1440), v(Async,5), vid_mem, rgb4_fourcc, chroma_444 }},
    {/*48*/ t_init,  MFX_ERR_NONE, 0, { v(Width,3840),v(CropW,3840),v(Height,2160),v(CropH,2160), sys_mem }},
    {/*49*/ t_init,  MFX_ERR_NONE, 0, { v(Width,3840),v(CropW,3840),v(Height,2160),v(CropH,2160), vid_mem }},
    {/*50*/ t_init,  MFX_ERR_NONE, 0, { v(Width,3840),v(CropW,3840),v(Height,2160),v(CropH,2160), sys_mem, rgb4_fourcc, chroma_444 }},
    {/*51*/ t_init,  MFX_ERR_NONE, 0, { v(Width,3840),v(CropW,3840),v(Height,2160),v(CropH,2160), vid_mem, rgb4_fourcc, chroma_444 }},
    {/*52*/ t_init,  MFX_ERR_NONE, 0, { v(Width,3840),v(CropW,3840),v(Height,2160),v(CropH,2160), v(Async,5), sys_mem }},
    {/*53*/ t_init,  MFX_ERR_NONE, 0, { v(Width,3840),v(CropW,3840),v(Height,2160),v(CropH,2160), v(Async,5), vid_mem }},
    {/*54*/ t_init,  MFX_ERR_NONE, 0, { v(Width,3840),v(CropW,3840),v(Height,2160),v(CropH,2160), v(Async,5), sys_mem, rgb4_fourcc, chroma_444 }},
    {/*55*/ t_init,  MFX_ERR_NONE, 0, { v(Width,3840),v(CropW,3840),v(Height,2160),v(CropH,2160), v(Async,5), vid_mem, rgb4_fourcc, chroma_444 }},
    {/*56*/ t_init,  MFX_ERR_NONE, 0, { v(Width,4096),v(CropW,4096),v(Height,4096),v(CropH,4096), v(Async,5), sys_mem }},
    {/*57*/ t_init,  MFX_ERR_NONE, 0, { v(Width,4096),v(CropW,4096),v(Height,4096),v(CropH,4096), v(Async,5), vid_mem }},
    {/*58*/ t_init,  MFX_ERR_NONE, 0, { v(Width,4096),v(CropW,4096),v(Height,4096),v(CropH,4096), v(Async,5), sys_mem, rgb4_fourcc, chroma_444 }},
    {/*59*/ t_init,  MFX_ERR_NONE, 0, { v(Width,4096),v(CropW,4096),v(Height,4096),v(CropH,4096), v(Async,5), vid_mem, rgb4_fourcc, chroma_444 }},
    {/*60*/ t_init,  MFX_ERR_NONE, 0, { v(Width,4096+16),v(CropW,4096+16),v(Height,4096),v(CropH,4096), v(Async,5), sys_mem }},
    {/*61*/ t_init,  MFX_ERR_NONE, 0, { v(Width,4096),v(CropW,4096),v(Height,4096+16),v(CropH,4096+16), v(Async,1), vid_mem }},
    {/*62*/ t_init,  MFX_ERR_NONE, 0, { v(Width,4096+16),v(CropW,4096+16),v(Height,4096),v(CropH,4096), v(Async,5), sys_mem, rgb4_fourcc, chroma_444 }},
    {/*63*/ t_init,  MFX_ERR_NONE, 0, { v(Width,4096),v(CropW,4096),v(Height,4096+16),v(CropH,4096+16), v(Async,1), vid_mem, rgb4_fourcc, chroma_444 }},

    {/*64*/ t_query_io_surf,  MFX_ERR_NONE, 0, { sys_mem}                          },
    {/*65*/ t_query_io_surf,  MFX_ERR_NONE, 0, { vid_mem }                         },
    {/*66*/ t_query_io_surf,  MFX_ERR_NONE, 0, { sys_mem, rgb4_fourcc, chroma_444} },
    {/*67*/ t_query_io_surf,  MFX_ERR_NONE, 0, { vid_mem, rgb4_fourcc, chroma_444} },
    {/*68*/ t_query_io_surf,  MFX_ERR_NONE, 0, { v(EnableDirtyRect, 1                         ), sys_mem}},
    {/*69*/ t_query_io_surf,  MFX_ERR_NONE, 0, { v(EnableDirtyRect, 0 | 0x10 | 0x20 | 0x30    ), sys_mem}},
    {/*70*/ t_query_io_surf,  MFX_ERR_NONE,                0, { v(EnableDirtyRect, MFX_CODINGOPTION_UNKNOWN  ), sys_mem}},
    {/*71*/ t_query_io_surf,  MFX_ERR_NONE,                0, { v(EnableDirtyRect, MFX_CODINGOPTION_OFF      ), vid_mem }},
    {/*72*/ t_query_io_surf,  MFX_ERR_NONE, 0, { v(EnableDirtyRect, MFX_CODINGOPTION_ADAPTIVE ), vid_mem }},
    {/*73*/ t_query_io_surf,  MFX_ERR_INVALID_VIDEO_PARAM, ADD_WRONG_BUFFER, { vid_mem }},
    {/*74*/ t_query_io_surf,  MFX_ERR_NONE, 0, v(&tsStruct::mfxExtScreenCaptureParam.Header.BufferSz, (sizeof(mfxExtScreenCaptureParam)+1))},
    {/*75*/ t_query_io_surf,  MFX_ERR_UNKNOWN,     WRONGLY_ATTACHED, { vid_mem }},

    {/*76*/ t_getvideoparam,  MFX_ERR_NONE, 0, { sys_mem}                          },
    {/*77*/ t_getvideoparam,  MFX_ERR_NONE, 0, { vid_mem }                         },
    {/*78*/ t_getvideoparam,  MFX_ERR_NONE, 0, { sys_mem, rgb4_fourcc, chroma_444} },
    {/*79*/ t_getvideoparam,  MFX_ERR_NONE, 0, { vid_mem, rgb4_fourcc, chroma_444} },
    {/*80*/ t_getvideoparam,  MFX_ERR_NONE,                0, { v(EnableDirtyRect, MFX_CODINGOPTION_UNKNOWN  ), sys_mem}},
    {/*81*/ t_getvideoparam,  MFX_ERR_NONE,                0, { v(EnableDirtyRect, MFX_CODINGOPTION_OFF      ), vid_mem }},
    {/*82*/ t_getvideoparam,  MFX_ERR_INVALID_VIDEO_PARAM, ADD_WRONG_BUFFER, { vid_mem }},
    {/*83*/ t_getvideoparam,  MFX_ERR_NULL_PTR,     WRONGLY_ATTACHED, { vid_mem }},

    {/*84*/ t_decodeframeasync_simple,  MFX_ERR_NONE, 0, { sys_mem}                          },
    {/*85*/ t_decodeframeasync_simple,  MFX_ERR_NONE, 0, { vid_mem }                         },
    {/*86*/ t_decodeframeasync_simple,  MFX_ERR_NONE, 0, { sys_mem, rgb4_fourcc, chroma_444} },
    {/*87*/ t_decodeframeasync_simple,  MFX_ERR_NONE, 0, { vid_mem, rgb4_fourcc, chroma_444} },
    {/*88*/ t_decodeframeasync_simple,  MFX_ERR_NONE, 0, { v(Async,5), sys_mem}                          },
    {/*89*/ t_decodeframeasync_simple,  MFX_ERR_NONE, 0, { v(Async,5), vid_mem }                         },
    {/*90*/ t_decodeframeasync_simple,  MFX_ERR_NONE, 0, { v(Async,5), sys_mem, rgb4_fourcc, chroma_444} },
    {/*91*/ t_decodeframeasync_simple,  MFX_ERR_NONE, 0, { v(Async,5), vid_mem, rgb4_fourcc, chroma_444} },
    {/*92*/ t_decodeframeasync_simple,  MFX_ERR_NONE, CHECK_MEMORY, { sys_mem}                          },
    {/*93*/ t_decodeframeasync_simple,  MFX_ERR_NONE, CHECK_MEMORY, { vid_mem }                         },
    {/*94*/ t_decodeframeasync_simple,  MFX_ERR_NONE, CHECK_MEMORY, { sys_mem, rgb4_fourcc, chroma_444} },
    {/*95*/ t_decodeframeasync_simple,  MFX_ERR_NONE, CHECK_MEMORY, { vid_mem, rgb4_fourcc, chroma_444} },
    {/*96*/ t_decodeframeasync_simple,  MFX_ERR_NONE,                0, { v(EnableDirtyRect, MFX_CODINGOPTION_UNKNOWN  ), sys_mem}},  //disable dirty rect
    {/*97*/ t_decodeframeasync_simple,  MFX_ERR_NONE,                0, { v(EnableDirtyRect, MFX_CODINGOPTION_OFF      ), vid_mem }}, //disable dirty rect
    {/*98*/ t_decodeframeasync_simple,  MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, ADD_WRONG_BUFFER_RT, { vid_mem }},
    {/*99*/ t_decodeframeasync_simple,  MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, 0, v(&tsStruct::mfxExtDirtyRect.Header.BufferSz, (sizeof(mfxExtDirtyRect)+1))},
    {/*100*/t_decodeframeasync_simple,  MFX_ERR_NONE, 0, { v(Width,1920),v(CropW,1920),v(Height,1080),v(CropH,1080), v(Async,1), sys_mem }},
    {/*101*/t_decodeframeasync_simple,  MFX_ERR_NONE, 0, { v(Width,2160),v(CropW,2160),v(Height,1200),v(CropH,1200), v(Async,5), vid_mem }},
    {/*102*/t_decodeframeasync_simple,  MFX_ERR_NONE, 0, { v(Width,3840),v(CropW,3840),v(Height,2160),v(CropH,2160), v(Async,2), sys_mem, rgb4_fourcc, chroma_444 }},
    {/*103*/t_decodeframeasync_simple,  MFX_ERR_NONE, 0, { v(Width,4096),v(CropW,4096),v(Height,3840),v(CropH,3840), v(Async,3), vid_mem, rgb4_fourcc, chroma_444 }},
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

mfxStatus TestSuite::t_query(TestSuite* pthis, const tc_struct& tc)
{
    EXPECT_NE(nullptr, (void*) pthis) << "ERROR (in test)!\n";

    mfxExtScreenCaptureParam& ext_sc_in = pthis->m_par;
    const mfxExtScreenCaptureParam copyInExtPar = ext_sc_in;

    tsExtBufType<mfxVideoParam> par_out;
    par_out.mfx.CodecId = MFX_CODEC_CAPTURE;
    mfxExtScreenCaptureParam&   ext_sc_out = par_out;
    pthis->m_pParOut = &par_out;

    mfxExtScreenCaptureParam tempSC = { {MFX_EXTBUFF_SCREEN_CAPTURE_PARAM, sizeof(mfxExtScreenCaptureParam)} };
    mfxU32 lTsTrace = g_tsTrace;
    if(WRONGLY_ATTACHED == tc.mode)
    {
        pthis->m_par.RemoveExtBuffer(MFX_EXTBUFF_SCREEN_CAPTURE_PARAM);
        pthis->m_pPar->NumExtParam = 1;
        pthis->m_pPar->ExtParam = (mfxExtBuffer**) (&tempSC);
        g_tsTrace = 0;
    }
    if(ADD_WRONG_BUFFER == tc.mode)
    {
        mfxExtDirtyRect& dirty_rect = pthis->m_par;
        mfxExtDirtyRect& dirty_rect_out = par_out;
    }

    g_tsStatus.expect(tc.sts);
    pthis->Query();

    if(MFX_ERR_NONE == tc.sts && 0 == tc.mode)
    {
        const mfxExtScreenCaptureParam& ext_sc = pthis->m_par;

        if(MFX_CODINGOPTION_ON == ext_sc.EnableDirtyRect || MFX_CODINGOPTION_OFF == ext_sc.EnableDirtyRect)
            EXPECT_EQ(ext_sc.EnableDirtyRect, ext_sc_out.EnableDirtyRect) << "ERROR: Query function did not copy the value from in structure to out!\n";
        else
            EXPECT_EQ(MFX_CODINGOPTION_UNKNOWN, ext_sc_out.EnableDirtyRect) << "ERROR: Query function did not fill the \"EnableDirtyRect\" field!\n";
    }
    if(WRONGLY_ATTACHED != tc.mode)
    {
        EXPECT_EQ(copyInExtPar.EnableDirtyRect, ext_sc_in.EnableDirtyRect) << "ERROR: Component must not change input structure in Query function call\n";
    }

    if(WRONGLY_ATTACHED == tc.mode)
    {
        pthis->m_pPar->NumExtParam = 0;
        pthis->m_pPar->ExtParam    = 0;
        g_tsTrace = lTsTrace;
    }
    return MFX_ERR_NONE;
}

mfxStatus TestSuite::t_init(TestSuite* pthis, const tc_struct& tc)
{
    EXPECT_NE(nullptr, (void*) pthis) << "ERROR (in test)!\n";

    mfxExtScreenCaptureParam& ext_sc_in = pthis->m_par;
    const mfxExtScreenCaptureParam copyInExtPar = ext_sc_in;

    mfxExtScreenCaptureParam tempSC = { {MFX_EXTBUFF_SCREEN_CAPTURE_PARAM, sizeof(mfxExtScreenCaptureParam)} };
    mfxU32 lTsTrace = g_tsTrace;
    if(WRONGLY_ATTACHED == tc.mode)
    {
        pthis->m_par.RemoveExtBuffer(MFX_EXTBUFF_SCREEN_CAPTURE_PARAM);
        pthis->m_pPar->NumExtParam = 1;
        pthis->m_pPar->ExtParam = (mfxExtBuffer**) (&tempSC);
        g_tsTrace = 0;
    }
    if(ADD_WRONG_BUFFER == tc.mode)
    {
        mfxExtDirtyRect& dirty_rect = pthis->m_par;
    }

    if(     !pthis->m_pFrameAllocator 
        && (   (pthis->m_request.Type & (MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET|MFX_MEMTYPE_VIDEO_MEMORY_PROCESSOR_TARGET))
            || (pthis->m_par.IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY)
            || pthis->m_use_memid))
    {
        if(!pthis->GetAllocator())
        {
            pthis->UseDefaultAllocator(
                    (pthis->m_par.IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY) 
                || (pthis->m_request.Type & MFX_MEMTYPE_SYSTEM_MEMORY)
            );
        }
        pthis->m_pFrameAllocator = pthis->GetAllocator();
        pthis->SetFrameAllocator();
    }

    g_tsStatus.expect(tc.sts);
    pthis->Init();

    if(WRONGLY_ATTACHED != tc.mode)
    {
        EXPECT_EQ(copyInExtPar.EnableDirtyRect, ext_sc_in.EnableDirtyRect) << "ERROR: Component must not change input structure in Init function call\n";
    }
    if(WRONGLY_ATTACHED == tc.mode)
    {
        pthis->m_pPar->NumExtParam = 0;
        pthis->m_pPar->ExtParam    = 0;
        g_tsTrace = lTsTrace;
    }

    if(MFX_ERR_NONE <= tc.sts)
    {
        g_tsStatus.expect(MFX_ERR_UNDEFINED_BEHAVIOR);
        pthis->Init();
    }

    if(MFX_ERR_NONE <= tc.sts)
        g_tsStatus.expect(MFX_ERR_NONE);
    else
        g_tsStatus.expect(MFX_ERR_NOT_INITIALIZED);

    pthis->GetVideoParam(pthis->m_session, pthis->m_pPar);

    if(MFX_ERR_NONE <= tc.sts)
        g_tsStatus.expect(MFX_ERR_NONE);
    else
        g_tsStatus.expect(MFX_ERR_NOT_INITIALIZED);
    pthis->Close();

    return MFX_ERR_NONE;
}

mfxStatus TestSuite::t_getvideoparam(TestSuite* pthis, const tc_struct& tc)
{
    EXPECT_NE(nullptr, (void*) pthis) << "ERROR (in test)!\n";

    mfxExtScreenCaptureParam& ext_sc_in = pthis->m_par;
    const mfxExtScreenCaptureParam copyInExtPar = ext_sc_in;

    if(     !pthis->m_pFrameAllocator 
        && (   (pthis->m_request.Type & (MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET|MFX_MEMTYPE_VIDEO_MEMORY_PROCESSOR_TARGET))
            || (pthis->m_par.IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY)
            || pthis->m_use_memid))
    {
        if(!pthis->GetAllocator())
        {
            pthis->UseDefaultAllocator(
                    (pthis->m_par.IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY) 
                || (pthis->m_request.Type & MFX_MEMTYPE_SYSTEM_MEMORY)
            );
        }
        pthis->m_pFrameAllocator = pthis->GetAllocator();
        pthis->SetFrameAllocator();
    }

    pthis->Init();

    mfxExtScreenCaptureParam tempSC = { {MFX_EXTBUFF_SCREEN_CAPTURE_PARAM, sizeof(mfxExtScreenCaptureParam)} };
    mfxU32 lTsTrace = g_tsTrace;
    if(WRONGLY_ATTACHED == tc.mode)
    {
        pthis->m_par.RemoveExtBuffer(MFX_EXTBUFF_SCREEN_CAPTURE_PARAM);
        pthis->m_pPar->NumExtParam = 1;
        pthis->m_pPar->ExtParam = (mfxExtBuffer**) (&tempSC);
        g_tsTrace = 0;
    }
    if(ADD_WRONG_BUFFER == tc.mode)
    {
        mfxExtDirtyRect& dirty_rect = pthis->m_par;
    }

    ext_sc_in.EnableDirtyRect = 0;
    g_tsStatus.expect(tc.sts);
    pthis->GetVideoParam();

    if(MFX_ERR_NONE == tc.sts)
    {
        if(MFX_CODINGOPTION_ON == copyInExtPar.EnableDirtyRect)
        {
            EXPECT_EQ(copyInExtPar.EnableDirtyRect, ext_sc_in.EnableDirtyRect) << "GetVideoParam didn't fill EnableDirtyRect value!\n";
        }
        else
        {
            EXPECT_EQ(MFX_CODINGOPTION_OFF, ext_sc_in.EnableDirtyRect) << "GetVideoParam didn't fill EnableDirtyRect value!\n";
        }
    }

    if(WRONGLY_ATTACHED == tc.mode)
    {
        pthis->m_pPar->NumExtParam = 0;
        pthis->m_pPar->ExtParam    = 0;
        g_tsTrace = lTsTrace;
    }

    pthis->Close();

    return MFX_ERR_NONE;
}

mfxStatus TestSuite::t_query_io_surf(TestSuite* pthis, const tc_struct& tc)
{
    EXPECT_NE(nullptr, (void*) pthis) << "ERROR (in test)!\n";

    mfxExtScreenCaptureParam& ext_sc_in = pthis->m_par;
    const mfxExtScreenCaptureParam copyInExtPar = ext_sc_in;

    mfxExtScreenCaptureParam tempSC = { {MFX_EXTBUFF_SCREEN_CAPTURE_PARAM, sizeof(mfxExtScreenCaptureParam)} };
    mfxU32 lTsTrace = g_tsTrace;
    if(WRONGLY_ATTACHED == tc.mode)
    {
        pthis->m_par.RemoveExtBuffer(MFX_EXTBUFF_SCREEN_CAPTURE_PARAM);
        pthis->m_pPar->NumExtParam = 1;
        pthis->m_pPar->ExtParam = (mfxExtBuffer**) (&tempSC);
        g_tsTrace = 0;
    }
    if(ADD_WRONG_BUFFER == tc.mode)
    {
        mfxExtDirtyRect& dirty_rect = pthis->m_par;
    }

    g_tsStatus.expect(tc.sts);
    pthis->QueryIOSurf();

    if(WRONGLY_ATTACHED != tc.mode)
    {
        EXPECT_EQ(copyInExtPar.EnableDirtyRect, ext_sc_in.EnableDirtyRect) << "ERROR: Component must not change input structure in QueryIOSurf function call\n";
    }
    if(WRONGLY_ATTACHED == tc.mode)
    {
        pthis->m_pPar->NumExtParam = 0;
        pthis->m_pPar->ExtParam    = 0;
        g_tsTrace = lTsTrace;
    }

    if(MFX_ERR_NONE <= tc.sts && MFX_CODINGOPTION_ON == copyInExtPar.EnableDirtyRect)
    {
        EXPECT_GT(pthis->m_request.NumFrameMin, 0) << "ERROR: QueryIOSurf requested 0 frames!\n";
        EXPECT_GT(pthis->m_request.NumFrameSuggested, 0) << "ERROR: QueryIOSurf requested 0 frames!\n";

        mfxFrameAllocRequest lRequest = {};

        pthis->m_par.RemoveExtBuffer(MFX_EXTBUFF_SCREEN_CAPTURE_PARAM);

        g_tsStatus.expect(MFX_ERR_NONE);
        pthis->QueryIOSurf(pthis->m_session, pthis->m_pPar, &lRequest);

        EXPECT_GT(lRequest.NumFrameMin, 0) << "ERROR: QueryIOSurf requested 0 frames!\n";
        EXPECT_GT(lRequest.NumFrameSuggested, 0) << "ERROR: QueryIOSurf requested 0 frames!\n";
        EXPECT_GT(pthis->m_request.NumFrameMin, lRequest.NumFrameMin) << "ERROR: Screen capture with DirtyRect enabled must request more frames than wihtout DirtyRect\n";
        EXPECT_GT(pthis->m_request.NumFrameSuggested, lRequest.NumFrameSuggested) << "ERROR: Screen capture with DirtyRect enabled must request more frames than wihtout DirtyRect\n";
    }

    return MFX_ERR_NONE;
}

mfxStatus TestSuite::t_decodeframeasync_simple(TestSuite* pthis, const tc_struct& tc)
{
    pthis->Init();
    std::vector<mfxExtDirtyRect> dirty_rects;
    std::vector<mfxExtScreenCaptureParam> ext_sc_pars;
    std::vector<mfxExtBuffer*>  ext_buf;

    mfxU32 n_frames = 100;
    MemoryTracker memTracker;

    mfxExtScreenCaptureParam& ext_sc_in = pthis->m_par;
    SimpleDirtyRectChecker drChecker(ext_sc_in);
    pthis->m_surf_processor = &drChecker;

    if(CHECK_MEMORY & tc.mode)
    {
        g_tsStatus.check( memTracker.Start() );
        pthis->m_surf_processor = &memTracker;
        n_frames = 500;
        g_tsTrace = 0;
    }

    pthis->SetPar4_DecodeFrameAsync();
    for(mfxU32 i = 0; i < pthis->PoolSize(); ++i)
    {
        mfxExtDirtyRect dirty_rect = {MFX_EXTBUFF_DIRTY_RECTANGLES, sizeof(dirty_rect)};
        SETPARS(&dirty_rect, MFXDR);
        dirty_rects.push_back(dirty_rect);

        if(ADD_WRONG_BUFFER_RT == tc.mode)
        {
            mfxExtScreenCaptureParam ext_sc = {MFX_EXTBUFF_SCREEN_CAPTURE_PARAM, sizeof(ext_sc)};
            ext_sc_pars.push_back(ext_sc);
        }
        mfxExtBuffer* extb = 0;
        ext_buf.push_back(extb);
    }
    for(mfxU32 i = 0; i < pthis->PoolSize(); ++i)
    {
        mfxFrameSurface1* surf = pthis->GetSurface(i);
        EXPECT_NE(nullptr, (void*) surf);
        
        mfxExtBuffer*& extb = ext_buf[i];
        extb = (mfxExtBuffer*) &(dirty_rects[i]);
        if(ADD_WRONG_BUFFER_RT == tc.mode)
        {
            extb = (mfxExtBuffer*) &(ext_sc_pars[i]);
            surf->Data.NumExtParam = 1;
        }
        surf->Data.NumExtParam = 1;
        surf->Data.ExtParam = &extb;
    }

    if(MFX_ERR_NONE != tc.sts)
    {
        pthis->SetPar4_DecodeFrameAsync();

        g_tsStatus.expect(tc.sts);
        g_tsStatus.check( pthis->DecodeFrameAsync(pthis->m_session, 0, pthis->m_pSurf, &pthis->m_pSurfOut, pthis->m_pSyncPoint) );
    }
    else
    {
        pthis->DecodeFrames(n_frames);
    }

    if(CHECK_MEMORY & tc.mode)
    {
        //Free test-controlled resources as much as possible
        pthis->Close();
        pthis->UnLoad();
        pthis->MFXClose();
        pthis->SetAllocator(0,false);

        g_tsStatus.check( memTracker.Stop() );
    }


    return MFX_ERR_UNKNOWN;
}

int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    const tc_struct& tc = test_case[id];

    EXPECT_NE(nullptr, (void*) tc.testFnc) << "ERROR (in test)!\n";
    if(tc.testFnc)
    {
        MFXInit();
        Load();

#if defined(_WIN32) || defined(_WIN64)
        SetProcessDPIAware();
        mfxU32 w = GetSystemMetrics(SM_CXSCREEN);
        mfxU32 h = GetSystemMetrics(SM_CYSCREEN);
        m_par.mfx.FrameInfo.Width  = MSDK_ALIGN16(w);
        m_par.mfx.FrameInfo.Height = MSDK_ALIGN16(h);
        m_par.mfx.FrameInfo.CropW = w;
        m_par.mfx.FrameInfo.CropH = h;
#endif

        mfxExtScreenCaptureParam& ext_sc = m_par; 
        ext_sc.EnableDirtyRect = MFX_CODINGOPTION_ON;

        SETPARS(m_pPar,  MFXPAR);
        SETPARS(&ext_sc, MFXSC);
        m_par_set = true;

        if(MFX_IMPL_HARDWARE == MFX_IMPL_BASETYPE(g_tsImpl))
        {
            mfxHandleType type;
            mfxHDL hdl;
            UseDefaultAllocator( !!(m_pPar->IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY) );
            if(GetAllocator() && GetAllocator()->get_hdl(type, hdl))
                SetHandle(m_session, type, hdl);
        }

        tc.testFnc(this, tc);
    }

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(screen_capture_dirty_rect);

}