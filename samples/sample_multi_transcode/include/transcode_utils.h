//* ////////////////////////////////////////////////////////////////////////////// */
//*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2011 - 2014 Intel Corporation. All Rights Reserved.
//
//
//*/

#ifndef __TRANSCODE_UTILS_H__
#define __TRANSCODE_UTILS_H__

#if defined(_WIN32) || defined(_WIN64)
#include <process.h>
#pragma warning(disable : 4201)
#include <d3d9.h>
#include <dxva2api.h>
#endif

#include <vector>
#include <map>
#include "pipeline_transcode.h"

class D3DFrameAllocator;
class D3D11FrameAllocator;
class SysMemFrameAllocator;
struct D3DAllocatorParams;

#pragma warning(disable: 4127) // constant expression


namespace TranscodingSample
{

    struct sInputParams;

    msdk_tick GetTick();
    mfxF64 GetTime(msdk_tick start);

    void PrintHelp(const msdk_char *strAppName, const msdk_char *strErrorMessage, ...);
    void PrintInfo(mfxU32 session_number, sInputParams* pParams, mfxVersion *pVer);

    bool PrintDllInfo(msdk_char *buf, mfxU32 buf_size, sInputParams* pParams);


    template <class T, bool isSingle>
    class s_ptr
    {
    public:
        s_ptr():m_ptr(0)
        {
        };
        ~s_ptr()
        {
            reset(0);
        }
        T* get()
        {
            return m_ptr;
        }
        T* pop()
        {
            T* ptr = m_ptr;
            m_ptr = 0;
            return ptr;
        }
        void reset(T* ptr)
        {
            if (m_ptr)
            {
                if (isSingle)
                    delete m_ptr;
                else
                    delete[] m_ptr;
            }
            m_ptr = ptr;
        }
    protected:
        T* m_ptr;
    };

    class CmdProcessor
    {
    public:
        CmdProcessor();
        virtual ~CmdProcessor();
        mfxStatus ParseCmdLine(int argc, msdk_char *argv[]);
        bool GetNextSessionParams(TranscodingSample::sInputParams &InputParams);
        FILE*     GetPerformanceFile() {return m_PerfFILE;};
        void      PrintParFileName();
    protected:
        mfxStatus ParseParFile(FILE* file);
        mfxStatus TokenizeLine(msdk_char *pLine, mfxU32 length);
        mfxStatus ParseParamsForOneSession(mfxU32 argc, msdk_char *argv[]);
        mfxStatus ParseOption__set(msdk_char* strCodecType, msdk_char* strPluginPath);
        mfxStatus VerifyAndCorrectInputParams(TranscodingSample::sInputParams &InputParams);
        mfxU32                                       m_SessionParamId;
        std::vector<TranscodingSample::sInputParams> m_SessionArray;
        FILE                                         *m_PerfFILE;
        msdk_char                                    *m_parName;
        mfxU32                                        m_nTimeout;
    private:
        DISALLOW_COPY_AND_ASSIGN(CmdProcessor);

    };


    // Wrapper on standard allocator for concurrent allocation of
    // D3D and system surfaces
    class GeneralAllocator : public BaseFrameAllocator
    {
    public:
        GeneralAllocator();
        virtual ~GeneralAllocator();

        virtual mfxStatus Init(mfxAllocatorParams *pParams);
        virtual mfxStatus Close();

    protected:
        virtual mfxStatus LockFrame(mfxMemId mid, mfxFrameData *ptr);
        virtual mfxStatus UnlockFrame(mfxMemId mid, mfxFrameData *ptr);
        virtual mfxStatus GetFrameHDL(mfxMemId mid, mfxHDL *handle);

        virtual mfxStatus ReleaseResponse(mfxFrameAllocResponse *response);
        virtual mfxStatus AllocImpl(mfxFrameAllocRequest *request, mfxFrameAllocResponse *response);

        void    StoreFrameMids(bool isD3DFrames, mfxFrameAllocResponse *response);
        bool    isD3DMid(mfxHDL mid);

        std::map<mfxHDL, bool>                  m_Mids;
        std::auto_ptr<BaseFrameAllocator>       m_D3DAllocator;
        std::auto_ptr<SysMemFrameAllocator>     m_SYSAllocator;
    private:
       DISALLOW_COPY_AND_ASSIGN(GeneralAllocator);

    };
}
#endif
