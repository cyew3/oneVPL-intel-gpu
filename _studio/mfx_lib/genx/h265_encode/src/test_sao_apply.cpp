//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2015 Intel Corporation. All Rights Reserved.
//

#include <math.h>
#include <stdio.h>
#pragma warning(push)
#pragma warning(disable : 4100)
#pragma warning(disable : 4201)
#include "cm_rt.h"
#pragma warning(pop)
#include "../include/test_common.h"
#include "../include/genx_hevce_sao_apply_hsw_isa.h"
#include "../include/genx_hevce_sao_apply_bdw_isa.h"

#ifdef CMRT_EMU
extern "C"

    void SaoApply(    
    SurfaceIndex SURF_RECON,     
    SurfaceIndex SURF_DST, 
    SurfaceIndex SURF_VIDEO_PARAM,
    SurfaceIndex SURF_FRAME_SAO_OFFSETS);

#endif //CMRT_EMU


//it doesn't matter now. will be configured later
struct VideoParam
{
    Ipp32s Width;
    Ipp32s Height;	
    Ipp32s PicWidthInCtbs;	
    Ipp32s PicHeightInCtbs;		

    Ipp32s  chromaFormatIdc;
    Ipp32s MaxCUSize;		
    Ipp32s bitDepthLuma;
    Ipp32s saoOpt;

    Ipp32f m_rdLambda;
    Ipp32s SAOChromaFlag;
    Ipp32s reserved1;
    Ipp32s reserved2;
}; // // sizeof == 48 bytes


enum SaoComponentIdx
{
    SAO_Y =0,
    SAO_Cb,
    SAO_Cr,
    NUM_SAO_COMPONENTS
};


struct SaoOffsetParam
{
    Ipp8u mode_idx; 
    Ipp8u type_idx; 
    Ipp8u startBand_idx;
    Ipp8u saoMaxOffsetQVal;     
    Ipp16s offset[4];
    Ipp8u reserved[4];
}; // sizeof() == 16bytes

void SimulateRecon(Ipp8u* input, Ipp32s inPitch, Ipp8u* recon, Ipp32s reconPitch, Ipp32s width, Ipp32s height, Ipp32s maxAbsPixDiff);

int RunCpu(    
    Ipp8u* frameRecon, 
    int pitchRecon, 
    Ipp8u* frameDst,   
    int pitchDst, 
    const VideoParam* m_par, 
    SaoOffsetParam* offsets);


#define USE_THREAD_SPACE 
// otherwise  GROUP_SPACE
#if 1
int RunGpu(
    Ipp8u* frameRecon, 
    int pitchRecon, 
    Ipp8u* frameDst,   
    int pitchDst, 
    const VideoParam* m_par, 
    SaoOffsetParam* offsets)
{
    // fake now
    if (0)
    {
        RunCpu(frameRecon, pitchRecon, frameDst, pitchDst, m_par, offsets);
        return 0;
    }

    // DEVICE //

    const Ipp32u paddingLu = 96;
    const Ipp32u paddingCh = 96;

    mfxU32 version = 0;
    CmDevice *device = 0;
    Ipp32s res = ::CreateCmDevice(device, version);
    CHECK_CM_ERR(res);

    CmProgram *program = 0;
    res = device->LoadProgram((void *)genx_hevce_sao_apply_hsw, sizeof(genx_hevce_sao_apply_hsw), program);
    CHECK_CM_ERR(res);

    CmKernel *kernel = 0;
    res = device->CreateKernel(program, CM_KERNEL_FUNCTION(SaoApply), kernel);
    CHECK_CM_ERR(res);

    //-------------------------------------------------------
    // debug printf info
    //-------------------------------------------------------
    res = device->InitPrintBuffer();
    CHECK_CM_ERR(res);

    // RESOURCES // 

    // src = deblocked reconstruct
    CmSurface2D *recon = 0;
    res = device->CreateSurface2D(m_par->Width, m_par->Height, CM_SURFACE_FORMAT_NV12, recon);
    CHECK_CM_ERR(res);
    res = recon->WriteSurfaceStride(frameRecon, NULL, pitchRecon);
    CHECK_CM_ERR(res);

    // output luma
    CmSurface2D *output = 0;
    res = device->CreateSurface2D(m_par->Width, m_par->Height, CM_SURFACE_FORMAT_NV12, output);
    CHECK_CM_ERR(res);

    // output luma UP
    Ipp8u *outputSysLu = 0;
    Ipp32u outputPitchLu = 0;
    CmSurface2DUP *outputLu = 0;
    Ipp32u outputSizeLu = 0;
    res = device->GetSurface2DInfo(m_par->Width+2*paddingLu, m_par->Height, CM_SURFACE_FORMAT_P8, outputPitchLu, outputSizeLu);
    CHECK_CM_ERR(res);
    outputSysLu = (Ipp8u *)CM_ALIGNED_MALLOC(outputSizeLu, 0x1000);
    res = device->CreateSurface2DUP(m_par->Width+2*paddingLu, m_par->Height, CM_SURFACE_FORMAT_P8, outputSysLu, outputLu);
    CHECK_CM_ERR(res);

    // output chroma UP
    Ipp8u *outputSysCh = 0;
    Ipp32u outputPitchCh = 0;
    CmSurface2DUP *outputCh = 0;
    Ipp32u outputSizeCh = 0;
    res = device->GetSurface2DInfo(m_par->Width+2*paddingCh, m_par->Height/2, CM_SURFACE_FORMAT_P8, outputPitchCh, outputSizeCh);
    CHECK_CM_ERR(res);
    outputSysCh = (Ipp8u *)CM_ALIGNED_MALLOC(outputSizeCh, 0x1000);
    res = device->CreateSurface2DUP(m_par->Width+2*paddingCh, m_par->Height/2, CM_SURFACE_FORMAT_P8, outputSysCh, outputCh);
    CHECK_CM_ERR(res);

    // param
    int size = sizeof(VideoParam);
    CmBuffer* video_param = 0;
    res = device->CreateBuffer(size, video_param);
    CHECK_CM_ERR(res);
    res = video_param->WriteSurface((const Ipp8u*)m_par, NULL);
    CHECK_CM_ERR(res);

    int numCtb = m_par->PicHeightInCtbs * m_par->PicWidthInCtbs;       

    // sao modes
    CmBuffer* frame_sao_offset_gfx = 0;
    size = numCtb * sizeof(SaoOffsetParam);
    res = device->CreateBuffer(size, frame_sao_offset_gfx);
    CHECK_CM_ERR(res);
    frame_sao_offset_gfx->WriteSurface((const Ipp8u*)offsets, NULL);

    // IDX //
    SurfaceIndex *idxRecon = 0;    
    res = recon->GetIndex(idxRecon);
    CHECK_CM_ERR(res);

    SurfaceIndex *idxOutputLu = 0;
    res = outputLu->GetIndex(idxOutputLu);
    CHECK_CM_ERR(res);    

    SurfaceIndex *idxOutputCh = 0;
    res = outputCh->GetIndex(idxOutputCh);
    CHECK_CM_ERR(res);    

    SurfaceIndex *idxOutput = 0;
    res = output->GetIndex(idxOutput);
    CHECK_CM_ERR(res);    

    SurfaceIndex *idxParam = 0;    
    res = video_param->GetIndex(idxParam);
    CHECK_CM_ERR(res);    
    
    SurfaceIndex *idx_frame_sao_offset = 0;    
    res = frame_sao_offset_gfx->GetIndex(idx_frame_sao_offset);
    CHECK_CM_ERR(res);

    // SET ARGS // 
    res = kernel->SetKernelArg(0, sizeof(SurfaceIndex), idxRecon);
    CHECK_CM_ERR(res);		
    res = kernel->SetKernelArg(1, sizeof(SurfaceIndex), idxOutput);
    CHECK_CM_ERR(res);		
    res = kernel->SetKernelArg(2, sizeof(SurfaceIndex), idxParam);
    CHECK_CM_ERR(res);		    
    res = kernel->SetKernelArg(3, sizeof(*idx_frame_sao_offset), idx_frame_sao_offset);
    CHECK_CM_ERR(res);
    //res = kernel->SetKernelArg(4, sizeof(Ipp32u), &paddingLu);
    //CHECK_CM_ERR(res);
    //res = kernel->SetKernelArg(5, sizeof(Ipp32u), &paddingCh);
    //CHECK_CM_ERR(res);

    mfxU32 MaxCUSize = m_par->MaxCUSize;

#if defined (USE_THREAD_SPACE)
    // THREAD SPACE //

    // for YUV420 we process (MaxCuSize x MaxCuSize) block        
    mfxU32 tsWidth   = m_par->PicWidthInCtbs;
    mfxU32 tsHeight  = m_par->PicHeightInCtbs;
    res = kernel->SetThreadCount(tsWidth * tsHeight);
    CHECK_CM_ERR(res);

    CmThreadSpace * threadSpace = 0;
    res = device->CreateThreadSpace(tsWidth, tsHeight, threadSpace);
    CHECK_CM_ERR(res);        

    res = threadSpace->SelectThreadDependencyPattern(CM_NONE_DEPENDENCY);	
    CHECK_CM_ERR(res);
#else
    int groupWidth   = m_par->PicWidthInCtbs;
    int groupHeight  = m_par->PicHeightInCtbs;
    int tsWidth = 4;// == 64 / 16
    int tsHeight= 8;// == 64 / 8;
    int threadCount = (groupWidth * groupHeight) * (tsWidth * tsHeight);
    res = kernel->SetThreadCount(threadCount);
    CHECK_CM_ERR(res);

    CmThreadGroupSpace* pTGS = NULL;        
    res = device->CreateThreadGroupSpace(tsWidth, tsHeight, groupWidth, groupHeight, pTGS);
    CHECK_CM_ERR(res);        
#endif		

    CmTask * task = 0;
    res = device->CreateTask(task);
    CHECK_CM_ERR(res);

    res = task->AddKernel(kernel);
    CHECK_CM_ERR(res);

    CmQueue *queue = 0;
    res = device->CreateQueue(queue);
    CHECK_CM_ERR(res);

    CmEvent * e = 0;
#if defined (USE_THREAD_SPACE)
    res = queue->Enqueue(task, e, threadSpace);
#else
    res = queue->EnqueueWithGroup(task, e, pTGS);
#endif
    CHECK_CM_ERR(res);

    res = e->WaitForTaskFinished();
    CHECK_CM_ERR(res);      

    // READ OUT DATA
    output->ReadSurfaceStride(frameDst, NULL, pitchDst);
    //for (Ipp32s y = 0; y < m_par->Height; y++)
    //    memcpy(frameDst + y * pitchDst, outputSysLu + paddingLu + y * outputPitchLu, m_par->Width);
    //for (Ipp32s y = 0; y < m_par->Height/2; y++)
    //    memcpy(frameDst + (y + m_par->Height) * pitchDst, outputSysCh + paddingCh + y * outputPitchCh, m_par->Width);

    //-------------------------------------------------
    // OUTPUT DEBUG
    //-------------------------------------------------
    res = device->FlushPrintBuffer();

    //mfxU64 mintime = mfxU64(-1);       

#ifndef CMRT_EMU
    printf("TIME=%.3f ms ", 

#if defined (USE_THREAD_SPACE)
        GetAccurateGpuTime(queue, task, threadSpace) / 1000000.0);
#else
        GetAccurateGpuTime_ThreadGroup(queue, task, pTGS) / 1000000.0);
#endif
#endif //CMRT_EMU

    device->DestroyTask(task);

    queue->DestroyEvent(e);

    device->DestroySurface(recon);
    device->DestroySurface(output);
    device->DestroySurface2DUP(outputLu);
    device->DestroySurface2DUP(outputCh);
    CM_ALIGNED_FREE(outputSysLu);
    CM_ALIGNED_FREE(outputSysCh);
    device->DestroySurface(video_param);
    device->DestroySurface(frame_sao_offset_gfx);

#if defined (USE_THREAD_SPACE)
    device->DestroyThreadSpace(threadSpace);
#else
    device->DestroyThreadGroupSpace(pTGS);
#endif

    device->DestroyKernel(kernel);
    device->DestroyProgram(program);
    ::DestroyCmDevice(device);

    return PASSED;
}
#endif

//void PadOnePix(Ipp8u *frame, Ipp32s pitch, Ipp32s width, Ipp32s height)
//{
//    Ipp8u *ptr = frame;
//    for (Ipp32s i = 0; i < height; i++, ptr += pitch) {
//        ptr[-1] = ptr[0];
//        ptr[width] = ptr[width - 1];
//    }

//    ptr = frame - 1;
//    memcpy(ptr - pitch, ptr, width + 2);
//    ptr = frame + height * pitch - 1;
//    memcpy(ptr, ptr - pitch, width + 2);
//}

union Avail
{
    struct
    {
        Ipp8u m_left         : 1;
        Ipp8u m_top          : 1;
        Ipp8u m_right        : 1;
        Ipp8u m_bottom       : 1;
        Ipp8u m_top_left     : 1;
        Ipp8u m_top_right    : 1;
        Ipp8u m_bottom_left  : 1;
        Ipp8u m_bottom_right : 1;
    };
    Ipp8u data;
};

typedef Ipp8u Pel;
typedef Ipp8s Char;
typedef int Int;

enum SAOType
{
    SAO_TYPE_EO_0 = 0,
    SAO_TYPE_EO_90,
    SAO_TYPE_EO_135,
    SAO_TYPE_EO_45,
    SAO_TYPE_BO,
    MAX_NUM_SAO_TYPE
};

enum SaoModes
{
  SAO_MODE_OFF = 0,
  SAO_MODE_ON,
  SAO_MODE_MERGE,
  NUM_SAO_MODES
};

#define Clip3( m_Min, m_Max, m_Value) ( (m_Value) < (m_Min) ? \
                                                  (m_Min) : \
                                                ( (m_Value) > (m_Max) ? \
                                                              (m_Max) : \
                                                              (m_Value) ) )

#define NUM_SAO_BO_CLASSES_LOG2  5
#define NUM_SAO_BO_CLASSES  (1<<NUM_SAO_BO_CLASSES_LOG2)

inline Ipp16s sgn(Ipp32s x)
{
    return ((x >> 31) | ((Ipp32s)( (((Ipp32u) -x)) >> 31)));
}

void ApplyBlockSao(
    const int channelBitDepth,
    int typeIdx, 
    int startBand_idx, 
    Ipp16s* offset4,// [4] 
    Pel* srcBlk, 
    Int srcStride, 
    Pel* resBlk,     
    Int resStride,  
    Int width, 
    Int height,
    Avail & avail,
    int MaxCuSize
    )
{
    int buf_offset[32];
    for (int idx = 0; idx < 32; idx++) {
        buf_offset[idx] = 0;
    }

    if (typeIdx == SAO_TYPE_BO) {
        buf_offset[(startBand_idx + 0) & 31] = offset4[0];
        buf_offset[(startBand_idx + 1) & 31] = offset4[1];
        buf_offset[(startBand_idx + 2) & 31] = offset4[2];
        buf_offset[(startBand_idx + 3) & 31] = offset4[3];
    } else { // any EO
        buf_offset[0] = offset4[0];
        buf_offset[1] = offset4[1];
        buf_offset[2] = 0;
        buf_offset[3] = offset4[2];
        buf_offset[4] = offset4[3];
    }
    int* offset = buf_offset;

    bool isLeftAvail = avail.m_left;  
    bool isRightAvail = avail.m_right; 
    bool isAboveAvail = avail.m_top; 
    bool isBelowAvail = avail.m_bottom; 
    bool isAboveLeftAvail = avail.m_top_left; 
    bool isAboveRightAvail = avail.m_top_right;  
    bool isBelowLeftAvail = avail.m_bottom_left;
    bool isBelowRightAvail = avail.m_bottom_right;

    Char m_signLineBuf1[64 + 1];
    Char m_signLineBuf2[64 + 1];
    int m_lineBufWidth = MaxCuSize;

    const Int maxSampleValueIncl = (1<< channelBitDepth )-1;

    Int x,y, startX, startY, endX, endY, edgeType;
    Int firstLineStartX, firstLineEndX, lastLineStartX, lastLineEndX;
    Char signLeft, signRight, signDown;

    Pel* srcLine = srcBlk;
    Pel* resLine = resBlk;

    switch(typeIdx)
    {
    case SAO_TYPE_EO_0:
        {
            offset += 2;
            startX = isLeftAvail ? 0 : 1;
            endX   = isRightAvail ? width : (width -1);
            for (y=0; y< height; y++)
            {
                signLeft = (Char)sgn(srcLine[startX] - srcLine[startX-1]);
                for (x=startX; x< endX; x++)
                {
                    signRight = (Char)sgn(srcLine[x] - srcLine[x+1]); 
                    edgeType =  signRight + signLeft;
                    signLeft  = -signRight;

                    resLine[x] = Clip3(0, maxSampleValueIncl, srcLine[x] + offset[edgeType]);
                }
                srcLine  += srcStride;
                resLine += resStride;
            }

        }
        break;
    case SAO_TYPE_EO_90:
        {
            offset += 2;
            Char *signUpLine = m_signLineBuf1;

            startY = isAboveAvail ? 0 : 1;
            endY   = isBelowAvail ? height : height-1;
            if (!isAboveAvail)
            {
                srcLine += srcStride;
                resLine += resStride;
            }

            Pel* srcLineAbove= srcLine- srcStride;
            for (x=0; x< width; x++)
            {
                signUpLine[x] = (Char)sgn(srcLine[x] - srcLineAbove[x]);
            }

            Pel* srcLineBelow;
            for (y=startY; y<endY; y++)
            {
                srcLineBelow= srcLine+ srcStride;

                for (x=0; x< width; x++)
                {
                    signDown  = (Char)sgn(srcLine[x] - srcLineBelow[x]);
                    edgeType = signDown + signUpLine[x];
                    signUpLine[x]= -signDown;

                    resLine[x] = Clip3(0, maxSampleValueIncl, srcLine[x] + offset[edgeType]);
                }
                srcLine += srcStride;
                resLine += resStride;
            }

        }
        break;
    case SAO_TYPE_EO_135:
        {
            offset += 2;
            Char *signUpLine, *signDownLine, *signTmpLine;

            signUpLine  = m_signLineBuf1;
            signDownLine= m_signLineBuf2;

            startX = isLeftAvail ? 0 : 1 ;
            endX   = isRightAvail ? width : (width-1);

            //prepare 2nd line's upper sign
            Pel* srcLineBelow= srcLine+ srcStride;
            for (x=startX; x< endX+1; x++)
            {
                signUpLine[x] = (Char)sgn(srcLineBelow[x] - srcLine[x- 1]);
            }

            //1st line
            Pel* srcLineAbove= srcLine- srcStride;
            firstLineStartX = isAboveLeftAvail ? 0 : 1;
            firstLineEndX   = isAboveAvail? endX: 1;
            for(x= firstLineStartX; x< firstLineEndX; x++)
            {
                edgeType  =  sgn(srcLine[x] - srcLineAbove[x- 1]) - signUpLine[x+1];

                resLine[x] = Clip3(0, maxSampleValueIncl, srcLine[x] + offset[edgeType]);
            }
            srcLine  += srcStride;
            resLine  += resStride;


            //middle lines
            for (y= 1; y< height-1; y++)
            {
                srcLineBelow= srcLine+ srcStride;

                for (x=startX; x<endX; x++)
                {
                    signDown =  (Char)sgn(srcLine[x] - srcLineBelow[x+ 1]);
                    edgeType =  signDown + signUpLine[x];
                    resLine[x] = Clip3(0, maxSampleValueIncl, srcLine[x] + offset[edgeType]);

                    signDownLine[x+1] = -signDown;
                }
                signDownLine[startX] = (Char)sgn(srcLineBelow[startX] - srcLine[startX-1]);

                signTmpLine  = signUpLine;
                signUpLine   = signDownLine;
                signDownLine = signTmpLine;

                srcLine += srcStride;
                resLine += resStride;
            }

            //last line
            srcLineBelow= srcLine+ srcStride;
            lastLineStartX = isBelowAvail ? startX : (width -1);
            lastLineEndX   = isBelowRightAvail ? width : (width -1);
            for(x= lastLineStartX; x< lastLineEndX; x++)
            {
                edgeType =  sgn(srcLine[x] - srcLineBelow[x+ 1]) + signUpLine[x];
                resLine[x] = Clip3(0, maxSampleValueIncl, srcLine[x] + offset[edgeType]);

            }
        }
        break;
    case SAO_TYPE_EO_45:
        {
            offset += 2;
            Char *signUpLine = m_signLineBuf1+1;

            startX = isLeftAvail ? 0 : 1;
            endX   = isRightAvail ? width : (width -1);

            //prepare 2nd line upper sign
            Pel* srcLineBelow= srcLine+ srcStride;
            for (x=startX-1; x< endX; x++)
            {
                signUpLine[x] = (Char)sgn(srcLineBelow[x] - srcLine[x+1]);
            }


            //first line
            Pel* srcLineAbove= srcLine- srcStride;
            firstLineStartX = isAboveAvail ? startX : (width -1 );
            firstLineEndX   = isAboveRightAvail ? width : (width-1);
            for(x= firstLineStartX; x< firstLineEndX; x++)
            {
                edgeType = sgn(srcLine[x] - srcLineAbove[x+1]) -signUpLine[x-1];
                resLine[x] = Clip3(0, maxSampleValueIncl, srcLine[x] + offset[edgeType]);
            }
            srcLine += srcStride;
            resLine += resStride;

            //middle lines
            for (y= 1; y< height-1; y++)
            {
                srcLineBelow= srcLine+ srcStride;

                for(x= startX; x< endX; x++)
                {
                    signDown =  (Char)sgn(srcLine[x] - srcLineBelow[x-1]);
                    edgeType =  signDown + signUpLine[x];
                    resLine[x] = Clip3(0, maxSampleValueIncl, srcLine[x] + offset[edgeType]);
                    signUpLine[x-1] = -signDown;
                }
                signUpLine[endX-1] = (Char)sgn(srcLineBelow[endX-1] - srcLine[endX]);
                srcLine  += srcStride;
                resLine += resStride;
            }

            //last line
            srcLineBelow= srcLine+ srcStride;
            lastLineStartX = isBelowLeftAvail ? 0 : 1;
            lastLineEndX   = isBelowAvail ? endX : 1;
            for(x= lastLineStartX; x< lastLineEndX; x++)
            {
                edgeType = sgn(srcLine[x] - srcLineBelow[x-1]) + signUpLine[x];
                resLine[x] = Clip3(0, maxSampleValueIncl, srcLine[x] + offset[edgeType]);

            }
        }
        break;
    case SAO_TYPE_BO:
        {
            const Int shiftBits = channelBitDepth - NUM_SAO_BO_CLASSES_LOG2;
            for (y=0; y< height; y++)
            {
                for (x=0; x< width; x++)
                {
                    resLine[x] = Clip3(0, maxSampleValueIncl, srcLine[x] + offset[srcLine[x] >> shiftBits] );
                }
                srcLine += srcStride;
                resLine += resStride;
            }
        }
        break;
    default:
        {
            printf("Not a supported SAO types\n");
            //assert(0);
            //exit(-1);
        }
    }
}


int RunCpu(
    Ipp8u* frameRecon,
    int pitchRecon,
    Ipp8u* frameDst,
    int pitchDst,
    const VideoParam* m_par,
    SaoOffsetParam* offsets)
{     
    int numCtb = m_par->PicWidthInCtbs * m_par->PicHeightInCtbs;

    for (int y = 0; y < 3*m_par->Height/2; y++)
        memcpy(frameDst + y * pitchDst, frameRecon + y * pitchRecon, m_par->Width);

    for (int ctbAddr = 0; ctbAddr < numCtb; ctbAddr++) {

        Ipp32s ctbPelX = (ctbAddr % m_par->PicWidthInCtbs) * m_par->MaxCUSize;
        Ipp32s ctbPelY = (ctbAddr / m_par->PicWidthInCtbs) * m_par->MaxCUSize;
        // apply for pixels.
        Ipp32s height  = (ctbPelY + m_par->MaxCUSize > m_par->Height) ? (m_par->Height- ctbPelY) : m_par->MaxCUSize;
        Ipp32s width   = (ctbPelX + m_par->MaxCUSize > m_par->Width)  ? (m_par->Width - ctbPelX) : m_par->MaxCUSize;
        
        Avail avail;
        if(1)
        {
            Ipp32s ctbAddrEnd = m_par->PicWidthInCtbs * m_par->PicHeightInCtbs;
            avail.m_left = (ctbAddr % m_par->PicWidthInCtbs) > 0 ? 1 : 0;
            avail.m_top  = (ctbAddr >= m_par->PicWidthInCtbs) ? 1 : 0;
            avail.m_top_left = avail.m_top & avail.m_left;
            //avail.m_right = ((ctbAddr % m_par->PicWidthInCtbs) < (m_par->PicWidthInCtbs - 1)) ? 1 : 0;
            avail.m_right = ((ctbPelX + width) < m_par->Width) ? 1 : 0;

            avail.m_top_right = avail.m_top & avail.m_right;
            //avail.m_bottom = (ctbAddr < (ctbAddrEnd - m_par->PicWidthInCtbs)) ? 1 : 0;
            avail.m_bottom = ((ctbPelY + height) < m_par->Height) ? 1 : 0;

            avail.m_bottom_left = avail.m_bottom & avail.m_left;
            avail.m_bottom_right = avail.m_bottom & avail.m_right;
        }

        // luma
        Ipp8u* reconY  = frameRecon  + ctbPelX + ctbPelY * pitchRecon;	
        Ipp8u* dstY    = frameDst    + ctbPelX + ctbPelY * pitchDst;	

        SaoOffsetParam & offset = offsets[ctbAddr];
        if (offset.mode_idx != SAO_MODE_OFF) {
            ApplyBlockSao(8, offset.type_idx, offset.startBand_idx, offset.offset, reconY, pitchRecon, dstY, pitchDst, width, height, avail, m_par->MaxCUSize);  
        }

        // chroma
        // TODO
    }

    return PASSED;
}

int Compare(const Ipp8u *data1, Ipp32s pitch1, const Ipp8u *data2, Ipp32s pitch2, Ipp32s width, Ipp32s height)
{
    // Luma
    for (Ipp32s y = 0; y < height; y++) {
        for (Ipp32s x = 0; x < width; x++) {            
            Ipp32s diff = abs(data1[y * pitch1 + x] - data2[y * pitch2 + x]);
            if (diff) {
                printf("Luma bad pixels (x,y)=(%i,%i)\n", x, y);
                return FAILED;
            }
        }
    }

#if 0

    // Chroma
    const Ipp8u* refChroma =  data1 + height*pitch1;
    const Ipp8u* tstChroma =  data2 + height*pitch2;

    for (Ipp32s y = 0; y < height/2; y++) {
        for (Ipp32s x = 0; x < width; x++) {            
            Ipp32s diff = abs(refChroma[y * pitch1 + x] - tstChroma[y * pitch2 + x]);
            if (diff) {
                printf("Chroma %s bad pixels (x,y)=(%i,%i)\n", x >> 1, y, (x & 1) ? "U" : "V");
                return FAILED;
            }
        }
    }
#endif

    return PASSED;
}

int Dump(Ipp8u* data, size_t frameSize, const char* fileName)
{
    FILE *fout = fopen(fileName, "wb");
    if (!fout)
        return FAILED;

    fwrite(data, 1, frameSize, fout);	
    fclose(fout);

    return PASSED;
}


static
    int random(int a,int b)
{    
    if (a > 0) return a + rand() % (b - a);
    else return a + rand() % (abs(a) + b);
}

#define Saturate(min_val, max_val, val) MAX((min_val), MIN((max_val), (val)))

// 8b / 420 only!!!
void SimulateRecon(Ipp8u* input, Ipp32s inPitch, Ipp8u* recon, Ipp32s reconPitch, Ipp32s width, Ipp32s height, Ipp32s maxAbsPixDiff)
{	
    int pix;
    for (int row = 0; row < height; row++) {		
        for (int x = 0; x < width; x++) {
            pix = input[row * inPitch + x] + random(-maxAbsPixDiff, maxAbsPixDiff);
            recon[row * reconPitch + x] = Saturate(0, 255, pix);
        }
    }


    Ipp8u* inputUV = input + height * inPitch;
    Ipp8u* reconUV = recon + height * reconPitch;
    for (int row = 0; row < height/2; row++) {
        for (int x = 0; x < width; x++) {
            pix = inputUV[row * inPitch + x] + random(-maxAbsPixDiff, maxAbsPixDiff);
            reconUV[row * reconPitch + x] = Saturate(0, 255, pix);
        }
    }
}

void SetVideoParam(VideoParam & par, Ipp32s width, Ipp32s height)
{
    par.Width = width;
    par.Height= height;		

    par.MaxCUSize = 64;// !!!		

    par.PicWidthInCtbs  = (par.Width + par.MaxCUSize - 1) / par.MaxCUSize;	
    par.PicHeightInCtbs = (par.Height + par.MaxCUSize - 1) / par.MaxCUSize;		

    par.chromaFormatIdc = 1;// !!!	

    par.bitDepthLuma = 8;// !!!

    par.saoOpt = 1;
    par.SAOChromaFlag = 0;	

    Ipp32s qp = 27; // !!!
    par.m_rdLambda = pow(2.0, (qp - 12) * (1.0 / 3.0)) * (1.0 / 256.0);	
}


//enum SAOType
//{
//    SAO_TYPE_EO_0 = 0,
//    SAO_TYPE_EO_90,
//    SAO_TYPE_EO_135,
//    SAO_TYPE_EO_45,
//    SAO_TYPE_BO,
//    MAX_NUM_SAO_TYPE
//};
//
//enum SaoModes
//{
//  SAO_MODE_OFF = 0,
//  SAO_MODE_ON,
//  SAO_MODE_MERGE,
//  NUM_SAO_MODES
//};

//static
//int random(int a,int b)
//{
//    int result;
//    if (a >= 0)
//        result = a + (rand() % (b - a));
//    else
//        result = a + (rand() % (abs(a) + b));
//
//    return result;
//}

void SimulateSaoOffsets(int seed, SaoOffsetParam* params, int numCtb)
{
    srand(seed);

    for (int ctbAddr = 0; ctbAddr < numCtb; ctbAddr++) {
        SaoOffsetParam & offset = params[ctbAddr];

        offset.mode_idx = random(0, NUM_SAO_MODES);
        offset.type_idx = random(0, MAX_NUM_SAO_TYPE);
        offset.startBand_idx = random(0, 32);
        offset.saoMaxOffsetQVal = 7;// 8bit

        offset.offset[0] = random(-7, 8);
        offset.offset[1] = random(-7, 8);
        offset.offset[2] = random(-7, 8);
        offset.offset[3] = random(-7, 8);

#if 0
        printf("\nctb %i mode %i, type %i, startBand %i, offsets: %i, %i, %i, %i\n", 
            ctbAddr, offset.mode_idx, offset.type_idx, offset.startBand_idx,
            offset.offset[0], offset.offset[1], offset.offset[2], offset.offset[3]);
#endif
    }
}

int main()
{
    FILE *f = fopen(YUV_NAME, "rb");
    if (!f)
        return FAILED;

    // const per frame params
    Ipp32s width = WIDTH;
    Ipp32s height = HEIGHT;

    VideoParam videoParam;
    SetVideoParam(videoParam, width, height);


    Ipp32s frameSize = 3 * (width * height) >> 1;
    Ipp8u *input = new Ipp8u[frameSize];	
    Ipp32s inPitch = width;
    size_t bytesRead = fread(input, 1, frameSize, f);
    if (bytesRead != frameSize)
        return FAILED;
    fclose(f);

    Ipp8u *recon = new Ipp8u[frameSize];
    Ipp32s reconPitch = width;

    const int maxAbsPixDiff = 10;
    SimulateRecon(input, inPitch, recon, reconPitch, width, height, maxAbsPixDiff);
    // recon = input + random[-diff, +diff]

    Ipp8u *outputGpu = new Ipp8u[frameSize];
    Ipp32s outPitchGpu = width;
    Ipp8u *outputCpu = new Ipp8u[frameSize];
    memset(outputCpu, 0, frameSize);
    Ipp32s outPitchCpu = width;	


    Ipp32s numCtbs = videoParam.PicHeightInCtbs * videoParam.PicWidthInCtbs;
    SaoOffsetParam* frame_sao_param = new SaoOffsetParam [numCtbs];

    int seed = 1234567;
    SimulateSaoOffsets(seed, frame_sao_param, numCtbs);


    // DUMP
    //Dump(input, frameSize, "input.yuv");
    //Dump(recon, frameSize, "recon.yuv");


    // CORE TEST
    Ipp32s res;	
    res = RunCpu(        
        recon, 
        reconPitch,         
        outputCpu,
        outPitchCpu,
        &videoParam,
        frame_sao_param); 
    CHECK_ERR(res);


#if 1
    res = RunGpu(     
        recon, 
        reconPitch,         
        outputGpu,
        outPitchGpu,
        &videoParam,
        frame_sao_param); 
    CHECK_ERR(res);
#endif
    
    // VALIDATION
    res = Compare(outputGpu, outPitchGpu, outputCpu, outPitchCpu, width, height);
    CHECK_ERR(res);
    res = Compare(outputGpu+width*height, outPitchGpu, outputCpu+width*height, outPitchCpu, width, height/2);
    CHECK_ERR(res);


    // RESOURCE FREE 
    delete [] input;
    delete [] recon;
    delete [] outputGpu;
    delete [] outputCpu;
    delete [] frame_sao_param;	   

    return printf("passed\n"), 0;
}

/* EOF */
