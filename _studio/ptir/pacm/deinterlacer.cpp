//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2014-2016 Intel Corporation. All Rights Reserved.
//

#include "pacm.h"
#include "cmut/clock.h"
#include "deinterlace_genx_hsw_isa.h"
#include "deinterlace_genx_bdw_isa.h"

//input: pIsaFileNames - file names of CM ISA file list. Now due to a bug in CM runtime, we only support length of 1.
//       size - number of files
//       width/height - size of input/output frames. we only support same resolution for input and output.
DeinterlaceFilter::DeinterlaceFilter(eMFXHWType HWType, UINT width, UINT height, mfxHandleType mfxDeviceType, mfxHDL mfxDeviceHdl)
{
    CMUT_ASSERT_EQUAL(1, width<=3840, "Frame width can not exceed 3840");
    assert(MFX_HW_UNKNOWN != HWType);

    //for (int i = 0; i < size; ++i) {
    //    assert(pIsaFileNames[i] != NULL);
    //    this->isaFileNames.push_back(string(pIsaFileNames[i]));
    //}
    bool jit = false;

    switch (HWType)
    {
    case MFX_HW_HSW_ULT:
    case MFX_HW_HSW:
        jit = false;
        this->device = std::auto_ptr<CmDeviceEx>(new CmDeviceEx(deinterlace_genx_hsw, sizeof(deinterlace_genx_hsw), mfxDeviceType, mfxDeviceHdl, jit));
        break;
    case MFX_HW_BDW:
        jit = false;
        this->device = std::auto_ptr<CmDeviceEx>(new CmDeviceEx(deinterlace_genx_bdw, sizeof(deinterlace_genx_bdw), mfxDeviceType, mfxDeviceHdl, jit));
        break;
    case MFX_HW_VLV:
    case MFX_HW_IVB:
    default:
        jit = true;
        this->device = std::auto_ptr<CmDeviceEx>(new CmDeviceEx(deinterlace_genx_hsw, sizeof(deinterlace_genx_hsw), mfxDeviceType, mfxDeviceHdl, jit));
        break;
    }

    this->height = height;
    this->width = width;

    // Reuse the device and queue since CM runtime only supports one queue now
    //this->device = std::auto_ptr<CmDeviceEx>(new CmDeviceEx(pIsaFileNames, size, mfxDeviceType, mfxDeviceHdl));
    this->queue = std::auto_ptr<CmQueueEx>(new CmQueueEx(this->DeviceEx()));

    this->kernelMedianDeinterlace = std::auto_ptr<CmKernelEx>(new CmKernelEx(this->DeviceEx(), CM_KERNEL_FUNCTION(cmk_media_deinterlace_nv12<64U, 16U>)));
    this->kernelMedianDeinterlaceTop = std::auto_ptr<CmKernelEx>(new CmKernelEx(this->DeviceEx(), CM_KERNEL_FUNCTION(cmk_media_deinterlace_single_field_nv12<1, 64U, 16U>)));
    this->kernelMedianDeinterlaceBot = std::auto_ptr<CmKernelEx>(new CmKernelEx(this->DeviceEx(), CM_KERNEL_FUNCTION(cmk_media_deinterlace_single_field_nv12<0, 64U, 16U>)));
    this->kernelSadRs = std::auto_ptr<CmKernelEx>(new CmKernelEx(this->DeviceEx(), CM_KERNEL_FUNCTION(cmk_sad_rs_nv12<64U, 8U>)));
    this->kernelSad = std::auto_ptr<CmKernelEx>(new CmKernelEx(this->DeviceEx(), CM_KERNEL_FUNCTION(cmk_sad_nv12<64U, 8U>)));
    this->kernelRs = std::auto_ptr<CmKernelEx>(new CmKernelEx(this->DeviceEx(), CM_KERNEL_FUNCTION(cmk_rs_nv12<64U, 8U>)));

    this->kernelFixEdgeTop = std::auto_ptr<CmKernelEx>(new CmKernelEx(this->DeviceEx(), CM_KERNEL_FUNCTION(cmk_FixEdgeDirectionalIYUV_Main_Top_Instance)));
    this->kernelFixEdgeBottom = std::auto_ptr<CmKernelEx>(new CmKernelEx(this->DeviceEx(), CM_KERNEL_FUNCTION(cmk_FixEdgeDirectionalIYUV_Main_Bottom_Instance)));

 //select most fit kernel base on video width. Kernel with _VarWidth surfix can handle smaller width,
 //while kernels without surfix can only handle fixed width.
    if (704 == width) {
        this->kernelLowEdgeMaskTop = std::auto_ptr<CmKernelEx>(new CmKernelEx(this->DeviceEx(), CM_KERNEL_FUNCTION(cmk_FilterMask_Main<704U, 0, 32U>)));
        this->kernelLowEdgeMaskBottom = std::auto_ptr<CmKernelEx>(new CmKernelEx(this->DeviceEx(), CM_KERNEL_FUNCTION(cmk_FilterMask_Main<704U, 1, 32U>)));
        this->kernelLowEdgeMask2Fields = std::auto_ptr<CmKernelEx>(new CmKernelEx(this->DeviceEx(), CM_KERNEL_FUNCTION(cmk_FilterMask_Main_2Fields<704U, 32U>)));
    }
    else if (1920 == width) {
        this->kernelLowEdgeMaskTop = std::auto_ptr<CmKernelEx>(new CmKernelEx(this->DeviceEx(), CM_KERNEL_FUNCTION(cmk_FilterMask_Main<1920U, 0, 32U>)));
        this->kernelLowEdgeMaskBottom = std::auto_ptr<CmKernelEx>(new CmKernelEx(this->DeviceEx(), CM_KERNEL_FUNCTION(cmk_FilterMask_Main<1920U, 1, 32U>)));
        this->kernelLowEdgeMask2Fields = std::auto_ptr<CmKernelEx>(new CmKernelEx(this->DeviceEx(), CM_KERNEL_FUNCTION(cmk_FilterMask_Main_2Fields<1920U, 32U>)));
    }
    else if (3840 == width) {
        this->kernelLowEdgeMaskTop = std::auto_ptr<CmKernelEx>(new CmKernelEx(this->DeviceEx(), CM_KERNEL_FUNCTION(cmk_FilterMask_Main<3840U, 0, 32U>)));
        this->kernelLowEdgeMaskBottom = std::auto_ptr<CmKernelEx>(new CmKernelEx(this->DeviceEx(), CM_KERNEL_FUNCTION(cmk_FilterMask_Main<3840U, 1, 32U>)));
        this->kernelLowEdgeMask2Fields = std::auto_ptr<CmKernelEx>(new CmKernelEx(this->DeviceEx(), CM_KERNEL_FUNCTION(cmk_FilterMask_Main_2Fields<3840U, 32U>)));
    }
    else if (704 > width) {
        this->kernelLowEdgeMaskTop = std::auto_ptr<CmKernelEx>(new CmKernelEx(this->DeviceEx(), CM_KERNEL_FUNCTION(cmk_FilterMask_Main_VarWidth<704U, 0, 32U>)));
        this->kernelLowEdgeMaskBottom = std::auto_ptr<CmKernelEx>(new CmKernelEx(this->DeviceEx(), CM_KERNEL_FUNCTION(cmk_FilterMask_Main_VarWidth<704U, 1, 32U>)));
        this->kernelLowEdgeMask2Fields = std::auto_ptr<CmKernelEx>(new CmKernelEx(this->DeviceEx(), CM_KERNEL_FUNCTION(cmk_FilterMask_Main_2Fields_VarWidth<704U, 32U>)));
    }
    else if (1920 > width) {
        this->kernelLowEdgeMaskTop = std::auto_ptr<CmKernelEx>(new CmKernelEx(this->DeviceEx(), CM_KERNEL_FUNCTION(cmk_FilterMask_Main_VarWidth<1920U, 0, 32U>)));
        this->kernelLowEdgeMaskBottom = std::auto_ptr<CmKernelEx>(new CmKernelEx(this->DeviceEx(), CM_KERNEL_FUNCTION(cmk_FilterMask_Main_VarWidth<1920U, 1, 32U>)));
        this->kernelLowEdgeMask2Fields = std::auto_ptr<CmKernelEx>(new CmKernelEx(this->DeviceEx(), CM_KERNEL_FUNCTION(cmk_FilterMask_Main_2Fields_VarWidth<1920U, 32U>)));
    }
    else if (3840 > width) {
        this->kernelLowEdgeMaskTop = std::auto_ptr<CmKernelEx>(new CmKernelEx(this->DeviceEx(), CM_KERNEL_FUNCTION(cmk_FilterMask_Main_VarWidth<3840U, 0, 32U>)));
        this->kernelLowEdgeMaskBottom = std::auto_ptr<CmKernelEx>(new CmKernelEx(this->DeviceEx(), CM_KERNEL_FUNCTION(cmk_FilterMask_Main_VarWidth<3840U, 1, 32U>)));
        this->kernelLowEdgeMask2Fields = std::auto_ptr<CmKernelEx>(new CmKernelEx(this->DeviceEx(), CM_KERNEL_FUNCTION(cmk_FilterMask_Main_2Fields_VarWidth<3840U, 32U>)));
    }

    this->kernelUndo2FrameTop = std::auto_ptr<CmKernelEx>(new CmKernelEx(this->DeviceEx(), CM_KERNEL_FUNCTION(cmk_undo2frame_nv12<32U, 8U, 1U>)));
    this->kernelUndo2FrameBottom = std::auto_ptr<CmKernelEx>(new CmKernelEx(this->DeviceEx(), CM_KERNEL_FUNCTION(cmk_undo2frame_nv12<32U, 8U, 0U>)));

    this->kernelDeinterlaceBorderTop = std::auto_ptr<CmKernelEx>(new CmKernelEx(this->DeviceEx(), CM_KERNEL_FUNCTION(cmk_DeinterlaceBorder<4U, 4U, 1U>)));
    this->kernelDeinterlaceBorderBottom = std::auto_ptr<CmKernelEx>(new CmKernelEx(this->DeviceEx(), CM_KERNEL_FUNCTION(cmk_DeinterlaceBorder<4U, 4U, 0U>)));

    this->badMCFrame = std::auto_ptr<CmSurface2DEx>(new CmSurface2DEx(this->DeviceEx(), this->width, this->height, CM_SURFACE_FORMAT_NV12));

    UINT threadsWidth = cmut::DivUp(width, RSSAD_PLANE_WIDTH);
    UINT threadsHeight = cmut::DivUp(height, RSSAD_PLANE_HEIGHT);

#if defined(LINUX32) || defined (LINUX64)
    this->sadFrame = std::auto_ptr<CmSurface2DUPEx>(new CmSurface2DUPEx(this->DeviceEx(), threadsWidth * 8, threadsHeight, CM_SURFACE_FORMAT_X8R8G8B8));
    this->rsFrame = std::auto_ptr<CmSurface2DUPEx>(new CmSurface2DUPEx(this->DeviceEx(), threadsWidth * RS_UNIT_SIZE, threadsHeight, CM_SURFACE_FORMAT_X8R8G8B8));
#else
    this->sadFrame = std::auto_ptr<CmSurface2DUPEx>(new CmSurface2DUPEx(this->DeviceEx(), threadsWidth * 8, threadsHeight, CM_SURFACE_FORMAT_A8R8G8B8));
    this->rsFrame = std::auto_ptr<CmSurface2DUPEx>(new CmSurface2DUPEx(this->DeviceEx(), threadsWidth * RS_UNIT_SIZE, threadsHeight, CM_SURFACE_FORMAT_A8R8G8B8));
#endif

#if 1
 this->kernelSad->SetKernelArg(2, *sadFrame);
 this->kernelRs->SetKernelArg(1, *rsFrame);
 this->kernelSadRs->SetKernelArg(2, *sadFrame);
 this->kernelSadRs->SetKernelArg(3, *rsFrame);
 this->kernelRs->SetKernelArg(2, height);
 this->kernelSadRs->SetKernelArg(4, height);
 this->kernelFixEdgeTop->SetKernelArg(1, *this->badMCFrame);
 this->kernelFixEdgeBottom->SetKernelArg(1, *this->badMCFrame);
 this->kernelLowEdgeMask2Fields->SetKernelArg(2, *this->badMCFrame);
 this->kernelLowEdgeMask2Fields->SetKernelArg(3, height);
 this->kernelLowEdgeMask2Fields->SetKernelArg(4, width);
 this->kernelLowEdgeMaskTop->SetKernelArg(1, *this->badMCFrame);
 this->kernelLowEdgeMaskTop->SetKernelArg(2, height);
 this->kernelLowEdgeMaskTop->SetKernelArg(3, width);
 this->kernelLowEdgeMaskBottom->SetKernelArg(1, *this->badMCFrame);
 this->kernelLowEdgeMaskBottom->SetKernelArg(2, height);
 this->kernelLowEdgeMaskBottom->SetKernelArg(3, width);
 this->kernelDeinterlaceBorderTop->SetKernelArg(1, width);
 this->kernelDeinterlaceBorderTop->SetKernelArg(2, height);
 this->kernelDeinterlaceBorderBottom->SetKernelArg(1, width);
 this->kernelDeinterlaceBorderBottom->SetKernelArg(2, height);
#endif
}

DeinterlaceFilter::~DeinterlaceFilter()
{
    //printf("\n");
    //printf("%40s %9.5lf ms count:%d\n", "SAD CPU Time", clockSAD.Duration() / clockSAD.Count(), clockSAD.Count());
    //printf("%40s %9.5lf ms count:%d\n", "RS CPU Time", clockRs.Duration() / clockRs.Count(), clockRs.Count());

    //printf("%40s %9.5lf ms count:%d\n", "Combined SAD RS GPU Total Time", clockSADRSGPUTotal.Duration() / clockSADRSGPUTotal.Count(), clockSADRSGPUTotal.Count());
    //printf("%40s %17.5lf ms count:%d\n", "Combined SAD RS Copy to GPU Time", clockSADRSCopyToGPU.Duration() / clockSADRSCopyToGPU.Count(), clockSADRSCopyToGPU.Count());
    //printf("%40s %17.5lf ms count:%d\n", "Combined SAD RS Create Surface Time", clockSADRSCreateSurfaces.Duration() / clockSADRSCreateSurfaces.Count(), clockSADRSCreateSurfaces.Count());
    //printf("%40s %17.5lf ms count:%d\n", "Combined SAD RS Enqueue Time", clockSADRSEnqueue.Duration() / clockSADRSEnqueue.Count(), clockSADRSEnqueue.Count());
    //printf("%40s %17.5lf ms count:%d\n", "Combined SAD RS Sync Time", clockSADRSSync.Duration() / clockSADRSSync.Count(), clockSADRSSync.Count());
    //printf("%40s %17.5lf ms count:%d\n", "Combined SAD RS Post Processing Time", clockSADRSPostProcessing.Duration() / clockSADRSPostProcessing.Count(), clockSADRSPostProcessing.Count());
    //printf("%40s %17.5lf ms count:%d\n", "SAD_RS kernel Time", queue->KernelDurationAvg(kernelSadRs->KernelName()), queue->KernelCount(kernelSadRs->KernelName()));

    //printf("\n");
    //printf("%40s %9.5lf ms count:%d\n", "RS GPU Total Time", clockRSGPUTotal.Duration() / clockRSGPUTotal.Count(), clockRSGPUTotal.Count());
    //printf("%40s %17.5lf ms count:%d\n", "RS Copy to GPU Time", clockRSCopyToGPU.Duration() / clockRSCopyToGPU.Count(), clockRSCopyToGPU.Count());
    //printf("%40s %17.5lf ms count:%d\n", "RS kernel Time", queue->KernelDurationAvg(kernelRs->KernelName()), queue->KernelCount(kernelRs->KernelName()));

    //printf("\n");
    //printf("%40s %9.5lf ms count:%d\n", "Total CPU Deinterlace Filter Time", clockDeinterlaceTotalCPU.Duration() / clockDeinterlaceTotalCPU.Count(), clockDeinterlaceTotalCPU.Count());
    //printf("%40s %17.5lf ms count:%d\n", "Deinterlace 2 Frames CPU Time", clockDeinterlace.Duration() / clockDeinterlace.Count(), clockDeinterlace.Count());
    //printf("%40s %17.5lf ms count:%d\n", "Deinterlace Top Field Only CPU Time", clockDeinterlaceOne.Duration() / clockDeinterlaceOne.Count(), clockDeinterlaceOne.Count());
    //printf("%40s %17.5lf ms count:%d\n", "FillBaseLinesIYUV CPU Time", clockFillBaseline.Duration() / clockFillBaseline.Count(), clockFillBaseline.Count());
    //printf("%40s %17.5lf ms count:%d\n", "LowEdgeMask CPU Time", clockLowEdgeMask.Duration() / clockLowEdgeMask.Count(), clockLowEdgeMask.Count());
    //printf("%40s %17.5lf ms count:%d\n", "FixEdge CPU Time", clockFixEdge.Duration() / clockFixEdge.Count(), clockFixEdge.Count());
    //printf("%40s %17.5lf ms count:%d\n", "DeinterBorder CPU Time", clockDeinterBorder.Duration() / clockDeinterBorder.Count(), clockDeinterBorder.Count());

    //printf("%40s %9.5lf ms count:%d\n", "Deintelace Copy to GPU Surface Time", clockCopyToGPU.Duration() / clockCopyToGPU.Count(), clockCopyToGPU.Count());
    //printf("%40s %9.5lf ms count:%d\n", "Deintelace Copy Surface to CPU Time", clockCopyToCPU.Duration() / clockCopyToCPU.Count(), clockCopyToCPU.Count());
    //printf("%40s %9.5lf ms count:%d\n", "Real GPU Deinterlace Filter Time", clockDeinterlaceTotalGPU.Duration() / clockDeinterlaceTotalGPU.Count(), clockDeinterlaceTotalGPU.Count());
    //printf("%40s %17.5lf ms count:%d\n", "Deinterlace 2 fields kernel Time", queue->KernelDurationAvg(kernelMedianDeinterlace->KernelName()), queue->KernelCount(kernelMedianDeinterlace->KernelName()));
    //printf("%40s %17.5lf ms count:%d\n", "Deinterlace Top field kernel Time", queue->KernelDurationAvg(kernelMedianDeinterlaceTop->KernelName()), queue->KernelCount(kernelMedianDeinterlaceTop->KernelName()));
    //printf("%40s %17.5lf ms count:%d\n", "Deinterlace Bot field kernel Time", queue->KernelDurationAvg(kernelMedianDeinterlaceBot->KernelName()), queue->KernelCount(kernelMedianDeinterlaceBot->KernelName()));
    //printf("%40s %17.5lf ms count:%d\n", "LowEdgeMask 2 fields kernel Time", queue->KernelDurationAvg(kernelLowEdgeMask2Fields->KernelName()), queue->KernelCount(kernelLowEdgeMask2Fields->KernelName()));
    //printf("%40s %17.5lf ms count:%d\n", "LowEdgeMask 1 field kernel Time", (queue->KernelDuration(kernelLowEdgeMaskTop->KernelName()) + queue->KernelDuration(kernelLowEdgeMaskBottom->KernelName())) / (queue->KernelCount(kernelLowEdgeMaskTop->KernelName()) + queue->KernelCount(kernelLowEdgeMaskBottom->KernelName())), (queue->KernelCount(kernelLowEdgeMaskTop->KernelName()) + queue->KernelCount(kernelLowEdgeMaskBottom->KernelName())));
    //printf("%40s %17.5lf ms count:%d\n", "FixEdge kernel Time", (queue->KernelDuration(kernelFixEdgeBottom->KernelName()) + queue->KernelDuration(kernelFixEdgeTop->KernelName())) / (queue->KernelCount(kernelFixEdgeBottom->KernelName()) + queue->KernelCount(kernelFixEdgeTop->KernelName())), (queue->KernelCount(kernelFixEdgeBottom->KernelName()) + queue->KernelCount(kernelFixEdgeTop->KernelName())));
    //printf("%40s %17.5lf ms count:%d\n", "DeinterlaceBorder kernel Time", (queue->KernelDuration(kernelDeinterlaceBorderTop->KernelName()) + queue->KernelDuration(kernelDeinterlaceBorderBottom->KernelName())) / (queue->KernelCount(kernelDeinterlaceBorderTop->KernelName()) + queue->KernelCount(kernelDeinterlaceBorderBottom->KernelName())), (queue->KernelCount(kernelDeinterlaceBorderTop->KernelName()) + queue->KernelCount(kernelDeinterlaceBorderBottom->KernelName())));
    //printf("%40s %17.5lf ms count:%d\n", "Deintelace Host Time", clockDeinterlaceHost.Duration() / clockDeinterlaceHost.Count(), clockDeinterlaceHost.Count());
    //printf("%40s %17.5lf ms count:%d\n", "FixEdge Host Time", clockFixEdgeHost.Duration() / clockFixEdgeHost.Count(), clockFixEdgeHost.Count());
    //printf("%40s %17.5lf ms count:%d\n", "LowEdgeMask Host Time", clockLowEdgeMaskHost.Duration() / clockLowEdgeMaskHost.Count(), clockLowEdgeMaskHost.Count());
    //printf("%40s %17.5lf ms count:%d\n", "Deinterlace borders Host Time", clockDeinterlaceBorderHost.Duration() / clockDeinterlaceBorderHost.Count(), clockDeinterlaceBorderHost.Count());
    //printf("%40s %17.5lf ms count:%d\n", "Wait GPU Complete Time", clockSync.Duration() / clockSync.Count(), clockSync.Count());

}

//this is used to synchronize GPU tasks.
void DeinterlaceFilter::SyncGPU()
{
    clockSync.Begin();
    //QueueEx().WaitForLastKernel();
    QueueEx().WaitForAllKernels();
    clockSync.End();
}

//Deinterlace interface to process one field only, interface is same as CPU reference code
void DeinterlaceFilter::DeinterlaceMedianFilterSingleFieldCM(Frame **frmBuffer, unsigned int curFrame, int BotBase)
{
    DeinterlaceMedianFilterCM(frmBuffer, !BotBase ? curFrame : SKIP_FIELD, BotBase ? curFrame : SKIP_FIELD);
}

//Deinterlace interface to process one or two fields, interface is same as CPU reference code
//curFrame is TopBased output and curFrame2 is BotBased output
//if output is TopBased, then curFrame should be input and ouptut frame
//if output is BotBased, then curFrame2 should be input and ouptut frame
//set the other frame number to DeinterlaceFilter::SKIP_FIELD to skip that field.
//if there are two fields to output, this function will take input from current surface from curFrame
//and generate output to curFrame and curFrame2. 
//if there is only one field to output, output frame is always same as input frame.
void DeinterlaceFilter::DeinterlaceMedianFilterCM(Frame **frmBuffer, UINT curFrame, UINT curFrame2)
{
    CMUT_ASSERT_MESSAGE((curFrame == SKIP_FIELD || frmBuffer[curFrame]->plaY.uiWidth == this->width), "Frame width doesn't matach filter's processing configuration");
    CMUT_ASSERT_MESSAGE((curFrame == SKIP_FIELD || frmBuffer[curFrame]->plaY.uiHeight == this->height), "Frame height doesn't matach filter's processing configuration");
    CMUT_ASSERT_MESSAGE((curFrame2 == SKIP_FIELD || frmBuffer[curFrame2]->plaY.uiWidth == this->width), "Frame2 width doesn't matach filter's processing configuration");
    CMUT_ASSERT_MESSAGE((curFrame2 == SKIP_FIELD || frmBuffer[curFrame2]->plaY.uiHeight == this->height), "Frame2 height doesn't matach filter's processing configuration");
 if (curFrame == SKIP_FIELD && curFrame2 == SKIP_FIELD)
  return;

#ifdef GPUPATH // 1: turn on GPU path, 0: turn off.
    clockDeinterlaceTotalGPU.Begin();
    Frame *pFrame, *pFrame2;
    pFrame = (SKIP_FIELD == curFrame) ? NULL : frmBuffer[curFrame];
    pFrame2 = (SKIP_FIELD == curFrame2) ? NULL : frmBuffer[curFrame2];
    this->MedianDeinterlaceCM(pFrame, pFrame2);
    this->BuildLowEdgeMask_Main_CM(frmBuffer, curFrame, curFrame2);
    this->FixEdgeDirectionalIYUVCM(frmBuffer, curFrame, curFrame2);
    this->DeinterlaceBordersCM(frmBuffer, curFrame, curFrame2);
    this->SyncGPU();
    clockDeinterlaceTotalGPU.End();
#endif

#ifdef CPUPATH // 1: turn on CPU path, 0: turn off. Can be removed now.
    clockDeinterlaceTotalCPU.Begin();
    if (SKIP_FIELD != curFrame2 && SKIP_FIELD != curFrame) {
        clockFillBaseline.Begin();
        FillBaseLinesIYUV(frmBuffer[curFrame], frmBuffer[curFrame2], false, false);
        clockFillBaseline.End();
        clockDeinterlace.Begin();
        MedianDeinterlace(frmBuffer[curFrame], 0);
        MedianDeinterlace(frmBuffer[curFrame2], 1);
        clockDeinterlace.End();
    }
    else if (SKIP_FIELD != curFrame) {
        clockDeinterlaceOne.Begin();
        MedianDeinterlace(frmBuffer[curFrame], 0);
        clockDeinterlaceOne.End();
    }
    else {
        clockDeinterlaceOne.Begin();
        MedianDeinterlace(frmBuffer[curFrame2], 1);
        clockDeinterlaceOne.End();
    }
    if (SKIP_FIELD != curFrame) {
        clockLowEdgeMask.Begin();
        BuildLowEdgeMask_Main(frmBuffer, curFrame, 0);
        clockLowEdgeMask.End();

        clockFixEdge.Begin();
        CalculateEdgesIYUV(frmBuffer, curFrame, 0);
        EdgeDirectionalIYUV_Main(frmBuffer, curFrame, 0);
        clockFixEdge.End();

        clockDeinterBorder.Begin();
        DeinterlaceBorders(frmBuffer, curFrame, 0);
        clockDeinterBorder.End();
    }

    if (SKIP_FIELD != curFrame2) {
        clockLowEdgeMask.Begin();
        BuildLowEdgeMask_Main(frmBuffer, curFrame2, 1);
        clockLowEdgeMask.End();

        clockFixEdge.Begin();
        CalculateEdgesIYUV(frmBuffer, curFrame2, 1);
        EdgeDirectionalIYUV_Main(frmBuffer, curFrame2, 1);
        clockFixEdge.End();

        clockDeinterBorder.Begin();
        DeinterlaceBorders(frmBuffer, curFrame2, 1);
        clockDeinterBorder.End();
    }
    clockDeinterlaceTotalCPU.End();
#endif

}

void DeinterlaceFilter::DeinterlaceMedianFilterCM_BFF(Frame **frmBuffer, UINT curFrame, UINT curFrame2)
{
    CMUT_ASSERT_MESSAGE((curFrame == SKIP_FIELD || frmBuffer[curFrame]->plaY.uiWidth == this->width), "Frame width doesn't matach filter's processing configuration");
    CMUT_ASSERT_MESSAGE((curFrame == SKIP_FIELD || frmBuffer[curFrame]->plaY.uiHeight == this->height), "Frame height doesn't matach filter's processing configuration");
    CMUT_ASSERT_MESSAGE((curFrame2 == SKIP_FIELD || frmBuffer[curFrame2]->plaY.uiWidth == this->width), "Frame2 width doesn't matach filter's processing configuration");
    CMUT_ASSERT_MESSAGE((curFrame2 == SKIP_FIELD || frmBuffer[curFrame2]->plaY.uiHeight == this->height), "Frame2 height doesn't matach filter's processing configuration");
    if (curFrame == SKIP_FIELD && curFrame2 == SKIP_FIELD)
        return;

    clockDeinterlaceTotalGPU.Begin();
    Frame *pFrame, *pFrame2;
    pFrame = (SKIP_FIELD == curFrame) ? NULL : frmBuffer[curFrame];
    pFrame2 = (SKIP_FIELD == curFrame2) ? NULL : frmBuffer[curFrame2];
    this->MedianDeinterlaceCM(pFrame, pFrame2);
    this->BuildLowEdgeMask_Main_CM(frmBuffer, curFrame, curFrame2);
    this->FixEdgeDirectionalIYUVCM(frmBuffer, curFrame, curFrame2);
    this->DeinterlaceBordersCM(frmBuffer, curFrame, curFrame2);
    this->SyncGPU();
    clockDeinterlaceTotalGPU.End();
}


//post processing for SAD operation
//reducing thru atomic operation on CmSurface2D is very slow.
//each atomic operation (8 DWORD) take about 0.2 us on IVB GT2, to reduce 4050 threads we have on 1920*1080 image it would
//take about 890us, which is much slower than CPU reduction
//We can do SLM reduction, however it's not worth it as CPU reduction is very quick already.. 
void DeinterlaceFilter::SumSAD(CmSurface2DUPEx &sadFrame, Frame *pfrmCur, int threadsWidth, int threadsHeight)
{

    UINT * pSadFrame = new UINT[threadsWidth * 8 * threadsHeight];
    sadFrame.Read(pSadFrame);

    UINT sadSum[5] = { 0 };
    for (UINT index = 0; index < static_cast<UINT>(threadsWidth * threadsHeight); ++index) {
        UINT sadsIndex = index * 8;

        sadSum[InterStraightTop] += pSadFrame[sadsIndex];
        sadSum[InterStraightBottom] += pSadFrame[sadsIndex + 1];
        sadSum[InterCrossTop] += pSadFrame[sadsIndex + 2];
        sadSum[InterCrossBottom] += pSadFrame[sadsIndex + 3];
        sadSum[IntraSAD] += pSadFrame[sadsIndex + 4];
    }

    double * pSADVal = pfrmCur->plaY.ucStats.ucSAD;
    for (int i = InterStraightTop; i < IntraSAD + 1; ++i) {
#if !defined(CPUPATH) || 1// 0 - validation, 1 - no validate
        pSADVal[i] = (double)sadSum[i] / (pfrmCur->plaY.uiSize >> 1);
#else // validate
        if (pSADVal[i] != (double)sadSum[i] / (pfrmCur->plaY.uiSize >> 1))
            printf("SAD Error!\n");
#endif
    }

    delete[] (pSadFrame);
}

//post processing for RS operation
//reducing thru atomic operation on CmSurface2D is very slow.
//each atomic operation (8 DWORD) take about 0.2 us on IVB GT2, to reduce 4050 threads we have on 1920*1080 image it would
//take about 890us, which is much slower than CPU reduction
//We can do SLM reduction, however it's not worth it as CPU reduction is very quick already.. 
void DeinterlaceFilter::SumRs(CmSurface2DUPEx &rsFrame, Frame *pFrame, UINT size, int threadsWidth, int PLANE_WIDTH, unsigned int uiCorrection)
{
    UINT * pRs = (UINT*)rsFrame.DataPtr();
    UINT pitchInDW = rsFrame.Pitch() / sizeof (UINT);
    double * Rs = new double[sizeof(pFrame->plaY.ucStats.ucRs) / sizeof(pFrame->plaY.ucStats.ucRs[0])];
    for (unsigned int i = 0; i < sizeof(pFrame->plaY.ucStats.ucRs) / sizeof(pFrame->plaY.ucStats.ucRs[0]); ++i) {
        Rs[i] = (double)0;
    }
    double data = 0.0;
    int shift = 0;
    unsigned int uiSize = pFrame->plaY.uiSize;

    if(uiCorrection)
        uiSize = (pFrame->plaY.uiWidth - 8) * pFrame->plaY.uiHeight;

    if (max(pFrame->plaY.ucStats.ucSAD[0], pFrame->plaY.ucStats.ucSAD[1]) < 0.45){
        data = 31.0;
        shift = 0;
    }
    else if (max(pFrame->plaY.ucStats.ucSAD[0], pFrame->plaY.ucStats.ucSAD[1]) < 0.5){
        data = 63.0;
        shift = 2;
    }
    else if (max(pFrame->plaY.ucStats.ucSAD[0], pFrame->plaY.ucStats.ucSAD[1]) < 1){
        data = 127.0;
        shift = 4;
    }
    else if (max(pFrame->plaY.ucStats.ucSAD[0], pFrame->plaY.ucStats.ucSAD[1]) < 1.5){
        data = 255.0;
        shift = 6;
    }
    else if (max(pFrame->plaY.ucStats.ucSAD[0], pFrame->plaY.ucStats.ucSAD[1]) < 2){
        data = 512.0;
        shift = 8;
    }
    else if (max(pFrame->plaY.ucStats.ucSAD[0], pFrame->plaY.ucStats.ucSAD[1]) < 2.5){
        data = 1023.0;
        shift = 10;
    }
    else if (max(pFrame->plaY.ucStats.ucSAD[0], pFrame->plaY.ucStats.ucSAD[1]) < 3){
        data = 2047.0;
        shift = 12;
    }
    else{
        data = 4095.0;
        shift = 14;
    }
    long long Rs2[RS_UNIT_SIZE] = { 0 };
    for (UINT i = 0; i < (this->width + RSSAD_PLANE_WIDTH - 1) / RSSAD_PLANE_WIDTH; ++i) {
        for (UINT j = 0; j < (this->height + RSSAD_PLANE_HEIGHT - 1) / RSSAD_PLANE_HEIGHT - uiCorrection; ++j) {
            UINT rsIndex = (i + j * threadsWidth) * RS_UNIT_SIZE;
#pragma unroll
   for (int k = 0; k < 8; k++)
    Rs2[k] += pRs[rsIndex + k];
   // pRs[16]~pRs[31] contain possible sums of all possible "data" value.
   // in this way we can do most work in GPU kernel.
   Rs2[8] += pRs[rsIndex + 16 + shift];
   Rs2[9] += pRs[rsIndex + 17 + shift];
        }
    }
    // Store Rs results from GPU to Rs array
    {
        Rs[0] = Rs2[0];
        Rs[2] = Rs2[1];
        Rs[1] = Rs2[2];
        Rs[3] = Rs2[3];
        Rs[4] = Rs2[4];
        Rs[5] = Rs2[5];
        Rs[9] = Rs2[6];
        Rs[10] = Rs2[7];
  Rs[6] = Rs2[8];
  Rs[7] = Rs2[9];
    }

 /*   Rs[0] = sqrt(Rs[0] / pFrame->uiSize);
    Rs[1] = sqrt(Rs[1] / (pFrame->plaY.uiSize >> 1));
    Rs[2] = sqrt(Rs[2] / (pFrame->plaY.uiSize >> 1));
    Rs[3] = sqrt(Rs[3] / (pFrame->plaY.uiSize >> 1));
    Rs[5] /= 1000;
    if (Rs[6] != 0)
        Rs[8] = atan((Rs[7] / pFrame->plaY.uiSize) / Rs[6]);
    else
        Rs[8] = 1000;
    Rs[7] /= 1000;
    Rs[9] = (Rs[9] / pFrame->plaY.uiSize) * 1000;
    Rs[10] = (Rs[10] / pFrame->plaY.uiSize) * 1000;*/

       Rs[0] = sqrt(Rs[0] / uiSize);
    Rs[1] = sqrt(Rs[1] / (uiSize >> 1));
    Rs[2] = sqrt(Rs[2] / (uiSize >> 1));
    Rs[3] = sqrt(Rs[3] / (uiSize >> 1));
    Rs[5] /= 1000;
    if (Rs[6] != 0)
        Rs[8] = atan((Rs[7] / uiSize) / Rs[6]);
    else
        Rs[8] = 1000;
    Rs[7] /= 1000;
    Rs[9] = (Rs[9] / uiSize) * 1000;
    Rs[10] = (Rs[10] / uiSize) * 1000;

    for (unsigned int i = 0; i < sizeof(pFrame->plaY.ucStats.ucRs) / sizeof(pFrame->plaY.ucStats.ucRs[0]); ++i) {
#if !defined(CPUPATH) || 0 // 0 - valiation, 1 - no validattion
        pFrame->plaY.ucStats.ucRs[i] = Rs[i];
#else
        if (pFrame->plaY.ucStats.ucRs[i] != Rs[i])
            printf("RS Error!\n");
#endif
    }

    delete[] (Rs);
}

void DeinterlaceFilter::CalculateSAD(Frame *pfrmCur, Frame *pfrmPrv)
{
 CMUT_ASSERT_EQUAL(this->width, pfrmCur->plaY.uiWidth, "Frame width doesn't matach filter's processing configuration");
 CMUT_ASSERT_EQUAL(this->height, pfrmCur->plaY.uiHeight, "Frame height doesn't matach filter's processing configuration");
 //CMUT_ASSERT_EQUAL(0, pfrmCur->plaY.uiHeight % RSSAD_PLANE_HEIGHT, "Frame height doesn't matach filter's processing configuration");
 //CMUT_ASSERT_EQUAL(0, pfrmCur->plaY.uiWidth % RSSAD_PLANE_WIDTH, "Frame height doesn't matach filter's processing configuration");

#ifdef CPUPATH
    clockSAD.Begin();
    sadCalc_I420_frame(pfrmCur, pfrmPrv);
    clockSAD.End();
#endif

#ifdef GPUPATH

    clockSADGPUTotal.Begin();
    UINT threadsWidth = cmut::DivUp(pfrmCur->plaY.uiWidth, RSSAD_PLANE_WIDTH);
    UINT threadsHeight = cmut::DivUp(pfrmCur->plaY.uiHeight, RSSAD_PLANE_HEIGHT);

    CmThreadSpaceEx threadSpace(this->DeviceEx(), threadsWidth, threadsHeight);
    kernelSad->SetThreadCount(threadSpace.ThreadCount());
    kernelSad->SetKernelArg(0, *GetFrameSurfaceCur(pfrmCur));
    kernelSad->SetKernelArg(1, *GetFrameSurfaceCur(pfrmPrv));
#if 0 //this arg is set in constructor
    kernelSad->SetKernelArg(2, *sadFrame);
#endif
    QueueEx().Enqueue(*kernelSad, threadSpace);
    QueueEx().WaitForLastKernel();
    SumSAD(*sadFrame, pfrmCur, threadsWidth, threadsHeight);
    clockSADGPUTotal.End();
#endif
}

void DeinterlaceFilter::MeasureRs(Frame * pfrmCur)
{
 CMUT_ASSERT_EQUAL(this->width, pfrmCur->plaY.uiWidth, "Frame width doesn't matach filter's processing configuration");
 CMUT_ASSERT_EQUAL(this->height, pfrmCur->plaY.uiHeight, "Frame height doesn't matach filter's processing configuration");
 //CMUT_ASSERT_EQUAL(0, pfrmCur->plaY.uiHeight % RSSAD_PLANE_HEIGHT, "Frame height doesn't matach filter's processing configuration");
 //CMUT_ASSERT_EQUAL(0, pfrmCur->plaY.uiWidth % RSSAD_PLANE_WIDTH, "Frame width doesn't matach filter's processing configuration");

#ifdef CPUPATH
    clockRs.Begin();
 Rs_measurement(pfrmCur);
    clockRs.End();
#endif


#ifdef GPUPATH
    clockRSGPUTotal.Begin();
 UINT threadsWidth = cmut::DivUp(this->width, RSSAD_PLANE_WIDTH);
    UINT threadsHeight = cmut::DivUp(this->height, RSSAD_PLANE_HEIGHT);

    CmThreadSpaceEx threadSpace(this->DeviceEx(), threadsWidth, threadsHeight);
    kernelRs->SetThreadCount(threadSpace.ThreadCount());
 kernelRs->SetKernelArg(0, *GetFrameSurfaceCur(pfrmCur));
#if 0 //this arg is set in constructor
 kernelRs->SetKernelArg(1, *rsFrame);
 kernelRs->SetKernelArg(2, height);
#endif

    QueueEx().Enqueue(*kernelRs, threadSpace);
    QueueEx().WaitForLastKernel();
    UINT size = threadsWidth * RS_UNIT_SIZE * threadsHeight;
 SumRs(*rsFrame, pfrmCur, size, threadsWidth, RSSAD_PLANE_WIDTH, 0);
    clockRSGPUTotal.End();

#endif
}

void DeinterlaceFilter::CalculateSADRs(Frame *pfrmCur, Frame *pfrmPrv)
{
    CMUT_ASSERT_EQUAL(this->width, pfrmCur->plaY.uiWidth, "Frame width doesn't matach filter's processing configuration");
    CMUT_ASSERT_EQUAL(this->height, pfrmCur->plaY.uiHeight, "Frame height doesn't matach filter's processing configuration");
    CMUT_ASSERT_EQUAL(this->width, pfrmPrv->plaY.uiWidth, "Frame width doesn't matach filter's processing configuration");
    CMUT_ASSERT_EQUAL(this->height, pfrmPrv->plaY.uiHeight, "Frame height doesn't matach filter's processing configuration");
    //CMUT_ASSERT_EQUAL(0, pfrmCur->plaY.uiHeight % RSSAD_PLANE_HEIGHT, "Frame height doesn't matach filter's processing configuration");
    //CMUT_ASSERT_EQUAL(0, pfrmCur->plaY.uiWidth % RSSAD_PLANE_WIDTH, "Frame width doesn't matach filter's processing configuration");


#ifdef  CPUPATH
    clockSAD.Begin();
    sadCalc_I420_frame(pfrmCur, pfrmPrv);
    clockSAD.End();
    clockRs.Begin();
    Rs_measurement(pfrmCur);
    clockRs.End();
#endif

#ifdef GPUPATH
    clockSADRSGPUTotal.Begin();
    clockSADRSCreateSurfaces.Begin();
    UINT threadsWidth = cmut::DivUp(this->width, RSSAD_PLANE_WIDTH);
    UINT threadsHeight = cmut::DivUp(this->height, RSSAD_PLANE_HEIGHT);

    clockSADRSCreateSurfaces.End();

    clockSADRSEnqueue.Begin();
    CmThreadSpaceEx threadSpace(this->DeviceEx(), threadsWidth, threadsHeight);
    kernelSadRs->SetThreadCount(threadSpace.ThreadCount());
    kernelSadRs->SetKernelArg(0, *GetFrameSurfaceCur(pfrmCur));
    kernelSadRs->SetKernelArg(1, *GetFrameSurfaceCur(pfrmPrv));
#if 0 //this arg is set in constructor
    kernelSadRs->SetKernelArg(2, *sadFrame);
    kernelSadRs->SetKernelArg(3, *rsFrame);
    kernelSadRs->SetKernelArg(4, height);
#endif
    QueueEx().Enqueue(*kernelSadRs, threadSpace);
    clockSADRSEnqueue.End();
    clockSADRSSync.Begin();
    QueueEx().WaitForLastKernel();
    clockSADRSSync.End();

    clockSADRSPostProcessing.Begin();
    UINT size = threadsWidth * RS_UNIT_SIZE * threadsHeight;
    SumSAD(*sadFrame, pfrmCur, threadsWidth, threadsHeight);
    SumRs(*rsFrame, pfrmCur, size, threadsWidth, RSSAD_PLANE_WIDTH, 0);
    clockSADRSPostProcessing.End();
    clockSADRSGPUTotal.End();
#endif
}

//thread block size 64*16, data out of border will be thrown so no border risk.
//This is async call, need to use GPU Sync before fetch result.
void DeinterlaceFilter::MedianDeinterlaceCM(Frame * pFrame, Frame * pFrame2)
{
    clockDeinterlaceHost.Begin();
    enum
    {
        PLANE_WIDTH = 64, PLANE_HEIGHT = 16
    };

    if(pFrame)
    {
        CMUT_ASSERT_EQUAL(0, pFrame->plaY.uiBorder, "Border is not 0");
    }
    if(pFrame2)
    {
        CMUT_ASSERT_EQUAL(0, pFrame2->plaY.uiBorder, "Border is not 0");
    }

    CmThreadSpaceEx threadSpace(this->DeviceEx(), cmut::DivUp(width, PLANE_WIDTH), cmut::DivUp(height, PLANE_HEIGHT));

    if (NULL != pFrame && NULL != pFrame2) {
        kernelMedianDeinterlace->SetThreadCount(threadSpace.ThreadCount());
        kernelMedianDeinterlace->SetKernelArg(0, *GetFrameSurfaceCur(pFrame));
        kernelMedianDeinterlace->SetKernelArg(1, *GetFrameSurfaceOut(pFrame));
        kernelMedianDeinterlace->SetKernelArg(2, *GetFrameSurfaceOut(pFrame2));

        QueueEx().Enqueue(*kernelMedianDeinterlace, threadSpace);
    }
    else if (NULL != pFrame) {
        kernelMedianDeinterlaceTop->SetThreadCount(threadSpace.ThreadCount());
        kernelMedianDeinterlaceTop->SetKernelArg(0, *GetFrameSurfaceCur(pFrame));
        kernelMedianDeinterlaceTop->SetKernelArg(1, *GetFrameSurfaceOut(pFrame));

        QueueEx().Enqueue(*kernelMedianDeinterlaceTop, threadSpace);
    }
    else if (NULL != pFrame2) {
        kernelMedianDeinterlaceBot->SetThreadCount(threadSpace.ThreadCount());
        kernelMedianDeinterlaceBot->SetKernelArg(0, *GetFrameSurfaceCur(pFrame2));
        kernelMedianDeinterlaceBot->SetKernelArg(1, *GetFrameSurfaceOut(pFrame2));

        QueueEx().Enqueue(*kernelMedianDeinterlaceBot, threadSpace);
    }


    if (NULL != pFrame) {
        pFrame->frmProperties.processed = true;
        pFrame->frmProperties.drop = 0;
        pFrame->outState = Frame::OUT_PROCESSED;
    }
    if (NULL != pFrame2) {
        pFrame2->frmProperties.processed = true;
        pFrame2->frmProperties.drop = 0;
        pFrame2->outState = Frame::OUT_PROCESSED;
    }
    clockDeinterlaceHost.End();
}

//Thread input 32*7, output 32*6 (Y plane), data in even and odd lines for bottom/top field
//data out of border will be thrown, left and right border pixels will be replaced later, so no border risk
//input uses GetFrameSurfaceOut because incoming data is from previous filters.
//This is async call, need to use GPU Sync before fetch result.
void DeinterlaceFilter::FixEdgeDirectionalIYUVCM(Frame **frmBuffer, unsigned int curFrame, unsigned int curFrame2)
{
 CMUT_ASSERT_MESSAGE((curFrame == SKIP_FIELD || frmBuffer[curFrame]->outState == Frame::OUT_PROCESSED), "Unprocessed frame is served as input.");
 CMUT_ASSERT_MESSAGE((curFrame2 == SKIP_FIELD || frmBuffer[curFrame2]->outState == Frame::OUT_PROCESSED), "Unprocessed frame is served as input.");
 clockFixEdgeHost.Begin();

 enum {
  WIDTHPERTHREAD = 16,
  HEIGHTPERTHREAD = 12,
 };

    //CmKernelEx* kernelBot, *kernelTop;
    //kernelTop = this->kernelFixEdgeTop.get();
    //kernelBot = this->kernelFixEdgeBottom.get();

 CmThreadSpaceEx threadSpace(DeviceEx(), cmut::DivUp(width, WIDTHPERTHREAD), cmut::DivUp(height, HEIGHTPERTHREAD));

    if (SKIP_FIELD != curFrame) {
  this->kernelFixEdgeTop->SetThreadCount(cmut::DivUp(width, WIDTHPERTHREAD) * cmut::DivUp(height, HEIGHTPERTHREAD));
  this->kernelFixEdgeTop->SetKernelArg(0, *GetFrameSurfaceOut(frmBuffer[curFrame]));
#if 0 //this arg is set in constructor
  this->kernelFixEdgeTop->SetKernelArg(1, *this->badMCFrame);
#endif
  QueueEx().Enqueue(*this->kernelFixEdgeTop.get(), threadSpace);
    }
    if (SKIP_FIELD != curFrame2) {
  this->kernelFixEdgeBottom->SetThreadCount(cmut::DivUp(width, WIDTHPERTHREAD) * cmut::DivUp(height, HEIGHTPERTHREAD));
  this->kernelFixEdgeBottom->SetKernelArg(0, *GetFrameSurfaceOut(frmBuffer[curFrame2]));
#if 0 //this arg is set in constructor
  this->kernelFixEdgeBottom->SetKernelArg(1, *this->badMCFrame);
#endif
  QueueEx().Enqueue(*this->kernelFixEdgeBottom.get(), threadSpace);
    }
    clockFixEdgeHost.End();
}

//Thread input 3*full width, output 1 full width line
//border control is in GEN code
//input uses GetFrameSurfaceOut because incoming data is from previous filters.
//This is async call, need to use GPU Sync before fetch result.
void DeinterlaceFilter::BuildLowEdgeMask_Main_CM(Frame **frmBuffer, unsigned int curFrame, unsigned int curFrame2)
{
 CMUT_ASSERT_MESSAGE((curFrame == SKIP_FIELD || frmBuffer[curFrame]->outState == Frame::OUT_PROCESSED), "Unprocessed frame is served as input.");
 CMUT_ASSERT_MESSAGE((curFrame2 == SKIP_FIELD || frmBuffer[curFrame2]->outState == Frame::OUT_PROCESSED), "Unprocessed frame is served as input.");
 
 clockLowEdgeMaskHost.Begin();

    if (SKIP_FIELD != curFrame && SKIP_FIELD != curFrame2) {
        CmThreadSpaceEx threadSpace(DeviceEx(), 32, cmut::DivUp(height * 3 / 2, 32));
        this->kernelLowEdgeMask2Fields->SetThreadCount(32 * cmut::DivUp(height * 3 / 2, 32));
        this->kernelLowEdgeMask2Fields->SetKernelArg(0, *GetFrameSurfaceOut(frmBuffer[curFrame]));
        this->kernelLowEdgeMask2Fields->SetKernelArg(1, *GetFrameSurfaceOut(frmBuffer[curFrame2]));
#if 0 //this arg is set in constructor
  this->kernelLowEdgeMask2Fields->SetKernelArg(2, *this->badMCFrame);
        this->kernelLowEdgeMask2Fields->SetKernelArg(3, height);
        this->kernelLowEdgeMask2Fields->SetKernelArg(4, width);
#endif
  QueueEx().Enqueue(*(this->kernelLowEdgeMask2Fields), threadSpace);
    }
    else if (SKIP_FIELD != curFrame) {
        CmThreadSpaceEx threadSpace(DeviceEx(), 32, cmut::DivUp(height * 3 / 4, 32));
        this->kernelLowEdgeMaskTop->SetThreadCount(32 * cmut::DivUp(height * 3 / 4, 32));
        this->kernelLowEdgeMaskTop->SetKernelArg(0, *GetFrameSurfaceOut(frmBuffer[curFrame]));
#if 0 //this arg is set in constructor
  this->kernelLowEdgeMaskTop->SetKernelArg(1, *this->badMCFrame);
        this->kernelLowEdgeMaskTop->SetKernelArg(2, height);
        this->kernelLowEdgeMaskTop->SetKernelArg(3, width);
#endif
  QueueEx().Enqueue(*(this->kernelLowEdgeMaskTop), threadSpace);
    }
    else if (SKIP_FIELD != curFrame2) {
        CmThreadSpaceEx threadSpace(DeviceEx(), 32, cmut::DivUp(height * 3 / 4, 32));
        this->kernelLowEdgeMaskBottom->SetThreadCount(32 * cmut::DivUp(height * 3 / 4, 32));
        this->kernelLowEdgeMaskBottom->SetKernelArg(0, *GetFrameSurfaceOut(frmBuffer[curFrame2]));
#if 0 //this arg is set in constructor
  this->kernelLowEdgeMaskBottom->SetKernelArg(1, *this->badMCFrame);
        this->kernelLowEdgeMaskBottom->SetKernelArg(2, height);
        this->kernelLowEdgeMaskBottom->SetKernelArg(3, width);
#endif
  QueueEx().Enqueue(*this->kernelLowEdgeMaskBottom, threadSpace);
    }
    clockLowEdgeMaskHost.End();
}

//Thread block 32*8
//Data out of border will be thrown, so no border risk
void DeinterlaceFilter::Undo2FramesYUVCM(Frame *frmBuffer1, Frame *frmBuffer2, bool BFF)
{
    enum
    {
        PLANE_WIDTH = 32, PLANE_HEIGHT = 8
    };

    unsigned int uiWidth = frmBuffer2->plaY.uiWidth;
    unsigned int uiHeight = frmBuffer2->plaY.uiHeight;

    CmKernelEx* kernel;
    if (BFF) {
        kernel = this->kernelUndo2FrameBottom.get();
    }
    else {
        kernel = this->kernelUndo2FrameTop.get();
    }

    CmThreadSpaceEx threadSpace(DeviceEx(), cmut::DivUp(uiWidth, PLANE_WIDTH), cmut::DivUp(uiHeight, PLANE_HEIGHT));

    kernel->SetThreadCount(cmut::DivUp(uiWidth, PLANE_WIDTH) * cmut::DivUp(uiHeight, PLANE_HEIGHT));
    kernel->SetKernelArg(0, *GetFrameSurfaceCur(frmBuffer2));
    kernel->SetKernelArg(1, *GetFrameSurfaceCur(frmBuffer1));
    kernel->SetKernelArg(2, *GetFrameSurfaceOut(frmBuffer1));

    QueueEx().Enqueue(*kernel, threadSpace);
    frmBuffer1->frmProperties.candidate = true;
 frmBuffer1->outState = Frame::OUT_PROCESSED;
 
 QueueEx().WaitForLastKernel();
}

//This is async call, need to use GPU Sync before fetch result.
//No border risk
void DeinterlaceFilter::DeinterlaceBordersCM(Frame **frmBuffer, unsigned int curFrame, unsigned int curFrame2)
{
 CMUT_ASSERT_MESSAGE((curFrame == SKIP_FIELD || frmBuffer[curFrame]->outState == Frame::OUT_PROCESSED), "Unprocessed frame is served as input.");
 CMUT_ASSERT_MESSAGE((curFrame2 == SKIP_FIELD || frmBuffer[curFrame2]->outState == Frame::OUT_PROCESSED), "Unprocessed frame is served as input.");
 
 enum
    {
        PLANE_WIDTH = 4, PLANE_HEIGHT = 4
    };

    clockDeinterlaceBorderHost.Begin();

    unsigned int spaceHeightSeg1 = cmut::DivUp(width, PLANE_WIDTH);        // Top/Bottom border of PlaneY
    unsigned int spaceHeightSeg2 = cmut::DivUp(height, PLANE_HEIGHT) - 2;   // Left/Right border of PlaneY
    unsigned int spaceHeightSeg3 = cmut::DivUp(width, PLANE_WIDTH);        // Top/Bottom border of PlaneUV
    unsigned int spaceHeightSeg4 = cmut::DivUp(height / 2, PLANE_HEIGHT / 2) - 1; // Left/Right border of PlaneUV
    unsigned int spaceHeightTotoal = spaceHeightSeg1 + spaceHeightSeg2 + spaceHeightSeg3 + spaceHeightSeg4;
    unsigned int spaceHeight = cmut::DivUp(spaceHeightTotoal * 2, 32);

    CmThreadSpaceEx threadSpace(DeviceEx(), 32, spaceHeight);

    if (SKIP_FIELD != curFrame) {
        this->kernelDeinterlaceBorderTop->SetThreadCount(32 * spaceHeight);
        this->kernelDeinterlaceBorderTop->SetKernelArg(0, *GetFrameSurfaceOut(frmBuffer[curFrame]));
#if 0 //this arg is set in constructor
  this->kernelDeinterlaceBorderTop->SetKernelArg(1, width);
        this->kernelDeinterlaceBorderTop->SetKernelArg(2, height);
#endif
  QueueEx().Enqueue(*this->kernelDeinterlaceBorderTop.get(), threadSpace);
    }

    if (SKIP_FIELD != curFrame2) {
        this->kernelDeinterlaceBorderBottom->SetThreadCount(32 * spaceHeight);
        this->kernelDeinterlaceBorderBottom->SetKernelArg(0, *GetFrameSurfaceOut(frmBuffer[curFrame2]));
#if 0 //this arg is set in constructor
  this->kernelDeinterlaceBorderBottom->SetKernelArg(1, width);
        this->kernelDeinterlaceBorderBottom->SetKernelArg(2, height);
#endif
  QueueEx().Enqueue(*this->kernelDeinterlaceBorderBottom.get(), threadSpace);
    }

    clockDeinterlaceBorderHost.End();
}

void DeinterlaceFilter::FrameCreateSurface(Frame * pfrmIn,  bool bcreate)
{
 CMUT_ASSERT_MESSAGE(pfrmIn != NULL, "NULL Frame is served as input.");
 
    assert(pfrmIn->inSurf == 0);
    assert(pfrmIn->outSurf == 0);

    if(bcreate)
    {
 pfrmIn->inSurf = new CmSurface2DEx(DeviceEx(), pfrmIn->plaY.uiWidth, pfrmIn->plaY.uiHeight, CM_SURFACE_FORMAT_NV12);
    pfrmIn->outSurf = new CmSurface2DEx(DeviceEx(), pfrmIn->plaY.uiWidth, pfrmIn->plaY.uiHeight, CM_SURFACE_FORMAT_NV12);
    }
    else
    {
        pfrmIn->inSurf = new CmSurface2DEx(DeviceEx(), pfrmIn->plaY.uiWidth, pfrmIn->plaY.uiHeight, CM_SURFACE_FORMAT_NV12, 0);
        pfrmIn->outSurf = new CmSurface2DEx(DeviceEx(), pfrmIn->plaY.uiWidth, pfrmIn->plaY.uiHeight, CM_SURFACE_FORMAT_NV12, 0);
    }
    pfrmIn->outState = Frame::OUT_UNCHANGED;
}

void DeinterlaceFilter::FrameReleaseSurface(Frame * pfrmIn)
{
 CMUT_ASSERT_MESSAGE(pfrmIn != NULL, "NULL Frame is served as input.");
 CMUT_ASSERT_MESSAGE((GetFrameSurfaceIn(pfrmIn) != NULL && GetFrameSurfaceOut(pfrmIn) != NULL), "Surface Leaked.");
    if (NULL != GetFrameSurfaceIn(pfrmIn)) {
        delete GetFrameSurfaceIn(pfrmIn);
        pfrmIn->inSurf = NULL;
    }
    if (NULL != GetFrameSurfaceOut(pfrmIn)) {
        delete GetFrameSurfaceOut(pfrmIn);
        pfrmIn->outSurf = NULL;
    }
    pfrmIn->outState = Frame::OUT_UNCHANGED;
}

/* Not used by plugin
//convert Raw I420 data to NV12 surface
//Not optimized for speed, should not be used in plugin
void DeinterlaceFilter::WriteRAWI420ToGPUNV12(Frame * pFrame, void* ucMem)
{

 CMUT_ASSERT_MESSAGE((pFrame->plaY.uiHeight & 1) == 0 && (pFrame->plaY.uiWidth & 1) == 0, "Frame Height or Width is odd. Can't process.");
 
 int height = pFrame->plaY.uiHeight;
    int width = pFrame->plaY.uiWidth;
    int heightby2 = height / 2;
    int widthby2 = width / 2;

    unsigned char *inplaYucCorner, *inplaUucCorner, *inplaVucCorner;
    unsigned int uiOffset = width * height;
    inplaYucCorner = (unsigned char*)ucMem;
    inplaUucCorner = (unsigned char*)ucMem + uiOffset;
    uiOffset += widthby2 * heightby2;
    inplaVucCorner = (unsigned char*)ucMem + uiOffset;

    unsigned char *nv12Plane, *yPlane, *uvPlane;
    nv12Plane = (unsigned char*)malloc(height*width * 3 >> 1);
    assert(nv12Plane != NULL);
    yPlane = nv12Plane;
    uvPlane = nv12Plane + height*width;

    for (int i = 0; i < height; i++)
        ptir_memcpy(yPlane + i*width, inplaYucCorner + pFrame->plaY.uiWidth*i, width);

    for (int i = 0; i < heightby2; i++) {
        for (int j = 0; j < widthby2; j++) {
            uvPlane[i*width + j * 2] = inplaUucCorner[pFrame->plaU.uiWidth*i + j];
            uvPlane[i*width + j * 2 + 1] = inplaVucCorner[pFrame->plaV.uiWidth*i + j];
        }
    }
    GetFrameSurfaceIn(pFrame)->Write(nv12Plane);
    pFrame->outState = Frame::OUT_UNCHANGED;

    free(nv12Plane);
}

//convert NV12 surface to Raw I420 data
//Not optimized for speed, should not be used in plugin
void DeinterlaceFilter::ReadRAWI420FromGPUNV12(Frame * pFrame, void* ucMem)
{
 CMUT_ASSERT_MESSAGE((pFrame->plaY.uiHeight & 1) == 0 && (pFrame->plaY.uiWidth & 1) == 0, "Frame Height or Width is odd. Can't process.");
 int height = pFrame->plaY.uiHeight;
    int width = pFrame->plaY.uiWidth;
    int heightby2 = height / 2;
    int widthby2 = width / 2;
    
    unsigned char *inplaYucCorner, *inplaUucCorner, *inplaVucCorner;
    unsigned int uiOffset = width * height;
    inplaYucCorner = (unsigned char*)ucMem;
    inplaUucCorner = (unsigned char*)ucMem + uiOffset;
    uiOffset += widthby2 * heightby2;
    inplaVucCorner = (unsigned char*)ucMem + uiOffset;

    unsigned char *nv12Plane, *yPlane, *uvPlane;
    nv12Plane = (unsigned char*)malloc(height*width * 3 >> 1);
    assert(nv12Plane != NULL);
    yPlane = nv12Plane;
    uvPlane = nv12Plane + height*width;

    GetFrameSurfaceCur(pFrame)->Read(nv12Plane);

    for (int i = 0; i < height; i++)
        ptir_memcpy(inplaYucCorner + pFrame->plaY.uiWidth*i, yPlane + i*width, width);

    for (int i = 0; i < heightby2; i++) {
        for (int j = 0; j < widthby2; j++) {
            inplaUucCorner[pFrame->plaU.uiWidth*i + j] = uvPlane[i*width + j * 2];
            inplaVucCorner[pFrame->plaV.uiWidth*i + j] = uvPlane[i*width + j * 2 + 1];
        }
    }

    free(nv12Plane);
}
*/
