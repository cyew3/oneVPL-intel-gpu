// Copyright (c) 2014-2018 Intel Corporation
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

#pragma once

#if defined(__GNUC__)
#if defined(__INTEL_COMPILER)
    #pragma warning (disable:1478)
#else
    #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
#endif

#include "cmut/cmrtex.h"
extern "C" {
#include "../pa/api.h"
#if defined(LINUX32) || defined (LINUX64)
//Undefine since cm linux include files are defining it itself which cause compilation errors
//  (for reference, CPU version is in C-code and do not use CM headers)
    #undef BOOL
    #undef TRUE
    #undef FALSE
    #undef BYTE
#endif
}
#include "pacm_genx.h"
#include "cmut/clock.h"
#include "mfxvideo++int.h"

//#define CPUPATH
#define GPUPATH

class DeinterlaceFilter
{
public:
    DeinterlaceFilter(eMFXHWType HWtype, unsigned int width, unsigned int height, mfxHandleType mfxDeviceType, mfxHDL mfxDeviceHdl);
    ~DeinterlaceFilter();

    enum {
        SKIP_FIELD = 0xffffffff,
    };

    void CalculateSAD(Frame *pfrmCur, Frame *pfrmPrv);
    void MeasureRs(Frame * pFrame);
    void CalculateSADRs(Frame *pfrmCur, Frame *pfrmPrv);
    void DeinterlaceMedianFilterCM(Frame **frmBuffer, unsigned int curFrame, unsigned int curFrame2 = SKIP_FIELD); // when curFrame2 is SKIP_FIELD, only Top Field is deinterlaced.
    void DeinterlaceMedianFilterCM_BFF(Frame **frmBuffer, unsigned int curFrame, unsigned int curFrame2 = SKIP_FIELD); // when curFrame2 is SKIP_FIELD, only Top Field is deinterlaced.
    void DeinterlaceMedianFilterSingleFieldCM(Frame **frmBuffer, unsigned int curFrame, int BotBase);
    void MedianDeinterlaceCM(Frame * pFrame, Frame * pFrame2);
    void FixEdgeDirectionalIYUVCM(Frame **frmBuffer, unsigned int curFrame, unsigned int curFrame2);
    void BuildLowEdgeMask_Main_CM(Frame **frmBuffer, unsigned int curFrame, unsigned int curFrame2);
    void Undo2FramesYUVCM(Frame *frmBuffer1, Frame *frmBuffer2, bool BFF);
    void DeinterlaceBordersCM(Frame **frmBuffer, unsigned int curFrame, unsigned int curFrame2);
    void SyncGPU();
    void FrameCreateSurface(Frame *pfrmIn, bool bcreate = true);
    void FrameReleaseSurface(Frame *pfrmIn);
    CmDeviceEx & DeviceEx() { return *device; }

protected:

    CmQueueEx & QueueEx() { return *queue; }

    const CmDeviceEx & DeviceEx() const { return *device; }
    const CmQueueEx & QueueEx() const { return *queue; }

    void SumSAD (CmSurface2DUPEx &sadFrame, Frame *pfrmCur, int threadsWidth, int threadsHeight);
    void SumRs (CmSurface2DUPEx &rsFrame, Frame *pfrmCur, UINT size, int threadsWidth, int PLANE_WIDTH, unsigned int uiCorrection);

private:
    std::auto_ptr<CmDeviceEx> device;
    std::auto_ptr<CmQueueEx> queue;
    std::auto_ptr<CmKernelEx> kernelMedianDeinterlace;
    std::auto_ptr<CmKernelEx> kernelMedianDeinterlaceTop;
    std::auto_ptr<CmKernelEx> kernelMedianDeinterlaceBot;
    std::auto_ptr<CmKernelEx> kernelSadRs;
    std::auto_ptr<CmKernelEx> kernelSad;
    std::auto_ptr<CmKernelEx> kernelRs;
    std::auto_ptr<CmKernelEx> kernelFixEdgeBottom;
    std::auto_ptr<CmKernelEx> kernelFixEdgeTop;
    std::auto_ptr<CmKernelEx> kernelLowEdgeMaskBottom;
    std::auto_ptr<CmKernelEx> kernelLowEdgeMaskTop;
    std::auto_ptr<CmKernelEx> kernelLowEdgeMask2Fields;
    std::auto_ptr<CmKernelEx> kernelUndo2FrameTop;
    std::auto_ptr<CmKernelEx> kernelUndo2FrameBottom;
    std::auto_ptr<CmKernelEx> kernelDeinterlaceBorderTop;
    std::auto_ptr<CmKernelEx> kernelDeinterlaceBorderBottom;
    std::vector<std::string> isaFileNames;

    void CopyBadMCFrameToCPU(Frame *pFrameDst1, Frame *pFrameDst2);
    static inline CmSurface2DEx* GetFrameSurfaceIn(Frame * pfrmIn) { return (CmSurface2DEx*)(pfrmIn->inSurf); }
    static inline CmSurface2DEx* GetFrameSurfaceCur(Frame * pfrmIn) { return pfrmIn->outState == Frame::OUT_UNCHANGED ? (CmSurface2DEx*)(pfrmIn->inSurf) : (CmSurface2DEx*)(pfrmIn->outSurf); }
    static inline CmSurface2DEx* GetFrameSurfaceOut(Frame * pfrmIn) { return (CmSurface2DEx*)(pfrmIn->outSurf); }

    unsigned int width;
    unsigned int height;

    std::auto_ptr<CmSurface2DEx> badMCFrame;
    std::auto_ptr<CmSurface2DUPEx> sadFrame;
    std::auto_ptr<CmSurface2DUPEx> rsFrame;

    Clock clockDeinterlace;
    Clock clockDeinterlaceOne;
    Clock clockSAD;
    Clock clockRs;
    Clock clockFixEdge;
    Clock clockLowEdgeMask;
    Clock clockFillBaseline;
    Clock clockDeinterBorder;

    Clock clockDeinterlaceTotalGPU;
    Clock clockDeinterlaceTotalCPU;
    Clock clockCopyToGPU;
    Clock clockSADCopyToGPU;
    Clock clockSADGPUTotal;
    Clock clockRSCopyToGPU;
    Clock clockRSGPUTotal;
    Clock clockSADRSCopyToGPU;
    Clock clockSADRSGPUTotal;
    Clock clockSADRSCreateSurfaces;
    Clock clockSADRSEnqueue;
    Clock clockSADRSSync;
    Clock clockSADRSPostProcessing;
    Clock clockCopyToCPU;
    Clock clockSync;
    Clock clockDeinterlaceHost;
    Clock clockFixEdgeHost;
    Clock clockLowEdgeMaskHost;
    Clock clockDeinterlaceBorderHost;

    enum
    {
        RSSAD_PLANE_WIDTH = 64, RSSAD_PLANE_HEIGHT = 8,
    };


    enum
    {
        RS_UNIT_SIZE = 32
    };

    //prohobit copy constructor
    DeinterlaceFilter(const DeinterlaceFilter& that);
    //prohibit assignment operator
    DeinterlaceFilter& operator=(const DeinterlaceFilter&);

};
