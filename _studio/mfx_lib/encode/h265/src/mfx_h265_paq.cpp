/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//     Copyright (c) 2013 - 2014 Intel Corporation. All Rights Reserved.
//
*/

#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE) && defined(MFX_ENABLE_H265_PAQ)

//#include <time.h>
#include <math.h>
#include <memory>
#include <vector>

#include "mfx_h265_paq.h"
#include "vm_debug.h"

#pragma warning(disable : 4018)
#pragma warning(disable : 4389)

namespace H265Enc {

//*********************************************************
//             LookAhead Interface
//*********************************************************
template<class T> inline void Zero(T & obj)                { memset(&obj, 0, sizeof(obj)); }
template<class T> inline void Zero(std::vector<T> & vec)   { if (vec.size() > 0) memset(&vec[0], 0, sizeof(T) * vec.size()); }
template<class T> inline void Zero(T * first, size_t cnt)  { memset(first, 0, sizeof(T) * cnt); }


template<class T> inline T & RemoveConst(T const & t) { return const_cast<T &>(t); }

template<class T> inline T * RemoveConst(T const * t) { return const_cast<T *>(t); }

template<class T, size_t N> inline T * Begin(T(& t)[N]) { return t; }

template<class T, size_t N> inline T * End(T(& t)[N]) { return t + N; }

template<class T, size_t N> inline size_t SizeOf(T(&)[N]) { return N; }

template<class T> inline T const * Begin(std::vector<T> const & t) { return &*t.begin(); }

template<class T> inline T const * End(std::vector<T> const & t) { return &*t.begin() + t.size(); }

template<class T> inline T * Begin(std::vector<T> & t) { return &*t.begin(); }

template<class T> inline T * End(std::vector<T> & t) { return &*t.begin() + t.size(); }

//}
//---------------------------------------------------------

H265Enc::AsyncRoutineEmulator::AsyncRoutineEmulator()
{
    std::fill(Begin(m_stageGreediness), End(m_stageGreediness), 1);
    Zero(m_queueFullness);
    Zero(m_queueFlush);
}

H265Enc::AsyncRoutineEmulator::AsyncRoutineEmulator(mfxVideoParam const & video)
{
    Init(video);
}

void H265Enc::AsyncRoutineEmulator::Init(mfxVideoParam const & video)
{
    //mfxExtCodingOption2 const * extOpt2 = NULL;//GetExtBuffer(video);

    const int LookAheadLength = 2*video.mfx.GopRefDist + 1;// 9 in case of 3 B frames

    m_stageGreediness[STG_ACCEPT_FRAME] = 1;

    m_stageGreediness[STG_START_LA    ] = 1;//video.mfx.EncodedOrder ? 1 : video.mfx.GopRefDist;

    m_stageGreediness[STG_WAIT_LA     ] = 1 + !!(video.AsyncDepth > 1);
    m_stageGreediness[STG_START_ENCODE] = IPP_MAX(LookAheadLength, video.mfx.GopRefDist); //!!!!!extOpt2->LookAheadDepth;
    m_stageGreediness[STG_WAIT_ENCODE ] = 1 + !!(video.AsyncDepth > 1);

    Zero(m_queueFullness);
    Zero(m_queueFlush);
}


mfxU32 H265Enc::AsyncRoutineEmulator::GetTotalGreediness() const
{
    mfxU32 greediness = 0;
    for (mfxU32 i = 0; i < STG_COUNT; i++)
        greediness += m_stageGreediness[i] - 1;
    return greediness + 1;
}

mfxU32 H265Enc::AsyncRoutineEmulator::GetStageGreediness(mfxU32 stage) const
{
    return (stage < STG_COUNT) ? m_stageGreediness[stage] : 0;
}

mfxU32 H265Enc::AsyncRoutineEmulator::CheckStageOutput(mfxU32 stage)
{
    mfxU32 in  = stage;
    mfxU32 out = stage + 1;
    mfxU32 hasOutput = 0;
    if (m_queueFullness[in] >= m_stageGreediness[stage] ||
        m_queueFullness[in] > 0 && m_queueFlush[in])
    {
        --m_queueFullness[in];
        ++m_queueFullness[out];
        hasOutput = 1;
    }

    m_queueFlush[out] = (m_queueFlush[in] && m_queueFullness[in] == 0);
    return hasOutput;
}


mfxU32 H265Enc::AsyncRoutineEmulator::Go(bool hasInput)
{
    if (hasInput)
        ++m_queueFullness[STG_ACCEPT_FRAME];
    else
        m_queueFlush[STG_ACCEPT_FRAME] = 1;

    mfxU32 stages = 0;
    for (mfxU32 i = 0; i < STG_COUNT; i++)
        stages += CheckStageOutput(i) << i;

    /*if (!hasInput && stages != 0 && (stages & STG_BIT_WAIT_ENCODE) == 0)
        stages += STG_BIT_RESTART;*/

    return stages;
}

//*********************************************************
//                      QPMap structure
//*********************************************************
TQPMap::TQPMap()
: m_uiPartWidth(0)
, m_uiPartHeight(0)
, m_uiNumPartInWidth(0)
, m_uiNumPartInHeight(0)
, m_iOffsetLow(0)
, m_iOffsetHigh(0)
, m_aiQPLevel(NULL)
, m_dAvgGopSADpp(0)
, m_dAvgGopSCpp(0)
, m_dAvgQP(0)
{
}

/** Destructor
 */
TQPMap::~TQPMap()
{
  destroy();
}

/** Initialize member variables
 * \param iWidth Picture width
 * \param iHeight Picture height
 * \param uiPartWidth Width of unit block for QP Map
 * \param uiPartHeight Height of unit block for QP Map
 * \return void
 */
void TQPMap::create( int iWidth, int iHeight, Ipp32u uiPartWidth, Ipp32u uiPartHeight )
{
  // Only 64x64 Supported for now in Analysis
  m_uiPartWidth = uiPartWidth;
  m_uiPartHeight = uiPartHeight;
  m_uiNumPartInWidth = (iWidth + m_uiPartWidth-1) / m_uiPartWidth;
  m_uiNumPartInHeight = (iHeight + m_uiPartHeight-1) / m_uiPartHeight;
  m_aiQPLevel = new int[ m_uiNumPartInWidth * m_uiNumPartInHeight ];
  ::memset( m_aiQPLevel, 0, sizeof(int)*( m_uiNumPartInWidth * m_uiNumPartInHeight ) );
}

/** Clean up
 * \return void
 */
void TQPMap::destroy()
{
  if (m_aiQPLevel)
  {
    delete[] m_aiQPLevel;
    m_aiQPLevel = NULL;
  }
}
//#endif


//#define MAXPDIST            (5)
#define INPUT_IMAGE_QUEUE_SIZE MAXPDIST*(REFMOD_MAX+1)+1


void WriteQpMap(RFrame *inFrame, TQPMap *pcQPMap) {
    int heightInTiles = inFrame->heightInTiles;
    int widthInTiles = inFrame->widthInTiles;
    int row, col;
    Ipp64f dAvgQP = 0.0;
    // Only 64x64 Supported for now in Analysis
    int nParts = MAX_TILE_SIZE/pcQPMap->getPartWidth();
    int mParts = MAX_TILE_SIZE/pcQPMap->getPartHeight();
    pcQPMap->setOffsetLow(inFrame->deltaQpLow);
    pcQPMap->setOffsetHigh(inFrame->deltaQpHigh);
    for(row = 0; row<heightInTiles; row++) {
        for(col=0; col<widthInTiles; col++) {
            int i,j;
            for(i=0;i<mParts;i++) {
                for(j=0;j<nParts;j++) {
                    pcQPMap->setLevel(row*mParts+i, col*nParts+j, inFrame->qp_mask[row*widthInTiles+col]);
                }
            }
            dAvgQP += inFrame->qp_mask[row*widthInTiles+col];
        }
    }
    dAvgQP/=(double)(heightInTiles*widthInTiles);
    pcQPMap->setAvgQP(dAvgQP);
}


//*********************************************************
//      NGV R Frame structures
//*********************************************************

void EnQueue(RFrameQueue *pInputFrameQueue, RFrame *pInputFrame)
{
    RFrameQueue_Append_Tail(pInputFrameQueue, pInputFrame);
}

NGV_Bool DeQueue(RFrameQueue *pInputFrameQueue, RFrame *pCodeThisFrame)
{        
    if(pCodeThisFrame)
    {
        if(pCodeThisFrame->ePicType == RV_IPIC) pCodeThisFrame->reference = TRUE;
        else if(pCodeThisFrame->ePicType == RV_PPIC) pCodeThisFrame->reference = TRUE;
        else pCodeThisFrame->reference = FALSE;
        RFrameQueue_Remove(pInputFrameQueue, pCodeThisFrame);
    }
    return FALSE;
}

RFrame* past_reference_frame(RFrame *frame)
{
    RFrame *frm = frame->past_frame;
    while(frm)
    {
        if(frm->coded && frm->reference)
        {
            return frm;
        }
        frm = frm->past_frame;
    }
    return NULL;
}

NGV_Bool past_reference_available(RFrame *frame)
{
    RFrame *frm = frame->past_frame;
    while(frm)
    {
        if(frm->coded && frm->reference)
        {
            return TRUE;
        }
        frm = frm->past_frame;
    }
    return FALSE;
}

NGV_Bool future_reference_available(RFrame *frame)
{
    RFrame *frm = frame->futr_frame;
    while(frm)
    {
        if(frm->coded && frm->reference)
        {
            return TRUE;
        }
        frm = frm->futr_frame;
    }
    return FALSE;
}

RFrame* NextReadyInQueue(RFrameQueue *pInputFrameQueue, NGV_Bool /*last_frame*/)
{
    RFrame *ihead=pInputFrameQueue->head;    
    NGV_Bool ready=FALSE;
    RFrame *inFrame=NULL;

    do
    {
        switch(ihead->ePicType)
        {            
        case RV_IPIC:
        case RV_PPIC:
        case RV_FPIC:
            {
                ready = TRUE;
            }
            break;
        }

        if(ready)
        {
            inFrame = ihead;
        }
        ihead = ihead->next_frame;
    } while(ihead && !ready);

    return inFrame;
}

void destroy_frames_in_list(RFrameVector *list)
{
    Ipp32u i;
    RFrame *pFrame;
    for (i = list->length; i >0 ; i--)
    {
        pFrame = RFrameVector_Detach_Head(list);
        RFrame_Destroy(pFrame);
    }
}


//*********************************************************
//                      TVideoPreanalyzer
//*********************************************************
bool TVideoPreanalyzer::preAnalyzeOne(TQPMap *acQPMap)
{

    //-----------------------------------------------------
    NGV_Bool        failed                = FALSE;
    //NGV_Bool        firstFrame            = TRUE;
    NGV_Bool        keyFrame            = FALSE;
    NGV_Bool        lastFrame            = FALSE;

    FILE* dqpFile = m_dqpFile;
    YuvImage            inputImage;
    RFrame *pInputFrame, *pCodeThisFrame;

    int width, height;//,  corrected_height;
    //int corrected_width;
    int& i= m_frameNumber;
    int& uLatency= m_uLatency;
    // starting time
//    double dResult;
//    long lBefore;

    width = m_iWidth;
    height = m_iHeight;
    
    NGV_PDist& np = *m_np;
//    int tmp_flag = 0;
    
    if(m_inputSurface)
    {
        // convert m_inputSurface => inputImage
        {
            inputImage.width = m_inputSurface->Info.Width;
            inputImage.height= m_inputSurface->Info.Height;
            inputImage.y_pitch = m_inputSurface->Data.Pitch;
            inputImage.y_plane = m_inputSurface->Data.Y;
        }

        // Ring Buffer
        if (m_pInputFrameList->length < INPUT_IMAGE_QUEUE_SIZE)
        {
            RFrame *pFrame;
            pFrame = RFrame_Construct();
            pInputFrame = pFrame;
            RFrame_Allocate(pFrame, width, height, MAX_TILE_SIZE, RFrame_Input);
            // Add to list
            RFrameVector_Append_Tail(m_pInputFrameList, pFrame);
        }
        else
        {
            pInputFrame = RFrameVector_Detach_Head(m_pInputFrameList);
            RFrameVector_Append_Tail(m_pInputFrameList, pInputFrame);
            pInputFrame->coded=FALSE;
            pInputFrame->reference=FALSE;
        }

        // convert image
        RFrame_Convert_Image(pInputFrame, &inputImage);
        pInputFrame->TR = i;

        if(i%m_uiIntraPeriod == 0)    
        {
            keyFrame    =    TRUE;
        } 
        else 
        {
            keyFrame = FALSE;
        }

        // Determine Frame Type        
        failed = NGV_PDist_Determine(m_np, pInputFrame, m_firstFrame, keyFrame, lastFrame);
        {
            np.avgSC += pInputFrame->SC; 
            np.avgTSC += pInputFrame->TSC; 
            np.avgSCid += pInputFrame->SCindex; 
            np.avgTSCid += pInputFrame->TSCindex; 
            np.totalFrames++; 
        }
        m_firstFrame = FALSE;
        // EnQueue
        EnQueue(m_pInputFrameQueue, pInputFrame);
        // NextReadyInQueue
        pCodeThisFrame = NextReadyInQueue(m_pInputFrameQueue, FALSE);
        if(pCodeThisFrame)
        {
            if(pCodeThisFrame->ePicType==0) 
            {
                // FDME
                NGV_PDME_Frame(&np, pCodeThisFrame, pInputFrame, pCodeThisFrame->FDSAD, pCodeThisFrame->FMV);
                // IPic QP Map
                IPicDetermineQpMap(&np, pCodeThisFrame);
                WriteQpMap(pCodeThisFrame, &acQPMap[pCodeThisFrame->TR]);

                double dqp_orig = acQPMap[pCodeThisFrame->TR].getAvgQP();
                int dqp = (int)floor (dqp_orig + 0.5);
                if(dqpFile)
                {
                    fprintf(dqpFile, "TR = %u, %15.3f, %i\n", pCodeThisFrame->TR, dqp_orig, dqp);
                }
            } 
            else if(pCodeThisFrame->ePicType==1) 
            {
                RFrame* past_frame = past_reference_frame(pCodeThisFrame);
                // PDME
                NGV_PDME_Frame(&np, pCodeThisFrame, past_frame, pCodeThisFrame->PDSAD, pCodeThisFrame->PMV);
                // FDME
                NGV_PDME_Frame(&np, pCodeThisFrame, pInputFrame, pCodeThisFrame->FDSAD, pCodeThisFrame->FMV);
                // PPic QP Map
                PPicDetermineQpMap(&np, pCodeThisFrame, past_frame);
                WriteQpMap(pCodeThisFrame, &acQPMap[pCodeThisFrame->TR]);

                double dqp_orig = acQPMap[pCodeThisFrame->TR].getAvgQP();
                int dqp = (int)floor (dqp_orig + 0.5);
                if(dqpFile)
                {
                    fprintf(dqpFile, "TR = %u, %15.3f, %i\n", pCodeThisFrame->TR, dqp_orig, dqp);
                }                
                // Compute GOP SADpp, sqrSCpp
                {
                    int gop_size = 1;
                    double avgTSC=pCodeThisFrame->TSC;
                    double avgsqrSCpp =  pCodeThisFrame->SC;
                    RFrame *pFrame=pCodeThisFrame->past_frame;
                    while(past_frame!=pFrame) {
                        avgTSC += pFrame->TSC;
                        avgsqrSCpp += pFrame->SC;
                        gop_size++;
                        pFrame = pFrame->past_frame;
                    }
                    avgTSC/=gop_size;
                    avgTSC/=(pCodeThisFrame->image.width*pCodeThisFrame->image.height);
                    avgsqrSCpp/=gop_size;
                    avgsqrSCpp/=sqrt((double)(pCodeThisFrame->image.width*pCodeThisFrame->image.height));
                    avgsqrSCpp = sqrt(avgsqrSCpp);
                    //LogMessage(generic_debug, LOG_LVL_ALWAYS, "Generating P Qp Map TR %d SC %f TC %f", pCodeThisFrame->TR, avgsqrSCpp, avgTSC);
                    pFrame=pCodeThisFrame;
                    gop_size = 0;
                    while(past_frame!=pFrame) {
                        acQPMap[pFrame->TR].setAvgGopSADpp(avgTSC);
                        acQPMap[pFrame->TR].setAvgGopSCpp(avgsqrSCpp);
                        pFrame = pFrame->past_frame;
                        gop_size++;
                    }
                }
            } 
            else 
            {
                //printf("\n fake processing??? \n");fflush(stderr);
                memset(pCodeThisFrame->qp_mask, 0, sizeof(int)*pCodeThisFrame->widthInTiles*pCodeThisFrame->heightInTiles);
                memset(pCodeThisFrame->sc_mask, 0, sizeof(int)*pCodeThisFrame->widthInTiles*pCodeThisFrame->heightInTiles);
            }

            //if(global_log_level>0 && pCodeThisFrame->ePicType!=RV_FPIC) VisualizeAllBlks(&viz, pCodeThisFrame);
            pCodeThisFrame->coded = TRUE;
            DeQueue(m_pInputFrameQueue, pCodeThisFrame);
        }
        else
        {
            uLatency++;
        }
        i++;
    }
    else if(uLatency)
    // FLUSH    
    //while(uLatency)
    {
        // Ring Buffer
        if (m_pInputFrameList->length < INPUT_IMAGE_QUEUE_SIZE)
        {
            RFrame *pFrame;
            pFrame = RFrame_Construct();        
            pInputFrame = pFrame;
            RFrame_Allocate(pFrame, width, height, 16, RFrame_Input);
            // Add to list
            RFrameVector_Append_Tail(m_pInputFrameList, pFrame);
        }
        else
        {
            pInputFrame = RFrameVector_Detach_Head(m_pInputFrameList);
            RFrameVector_Append_Tail(m_pInputFrameList, pInputFrame);
            pInputFrame->coded=FALSE;
            pInputFrame->reference=FALSE;
        }
        // NULL inputImage
        pInputFrame->TR = i;
        // Determine Frame Type
        failed = NGV_PDist_Determine(m_np, pInputFrame, FALSE, FALSE, TRUE);
        // EnQueue
        EnQueue(m_pInputFrameQueue, pInputFrame);
        // NextReadyInQueue
        pCodeThisFrame = NextReadyInQueue(m_pInputFrameQueue, FALSE);
        if(pCodeThisFrame)
        {
            //LogMessage(generic_debug, LOG_LVL_VIDEO, "Encoding %s TR %d", (pCodeThisFrame->ePicType==0)?("IPIC"):((pCodeThisFrame->ePicType==1)?("PPIC"):("FPIC")), pCodeThisFrame->TR);
            if(pCodeThisFrame->ePicType==0) 
            {
                // FDME
                {
                    Ipp32u fPos;
                    for(fPos=0;fPos<((VidData*)(m_np->inData))->R1.fBlocks;fPos++)
                    {
                        pCodeThisFrame->FDSAD[fPos]                =    INT_MAX;
                    }
                }
                // IPic QP Map (Not for end Key Frames)
                if(uLatency>=m_np->maxpdist) 
                {
                    //LogMessage(generic_debug, LOG_LVL_ALWAYS, "Generating I Qp Map TR %d", pCodeThisFrame->TR);
                    IPicDetermineQpMap(m_np, pCodeThisFrame);
                    WriteQpMap(pCodeThisFrame, &acQPMap[pCodeThisFrame->TR]);

                    // aya=====================================================
                    double dqp_orig = acQPMap[pCodeThisFrame->TR].getAvgQP();
                    int dqp = (int)floor (dqp_orig + 0.5);
                    if(dqpFile)
                    {
                        fprintf(dqpFile, "I0 TR = %u, %15.3f, %i\n", pCodeThisFrame->TR, dqp_orig, dqp);
                    }
                    //fwrite(&dqp, sizeof(int), 1, dqpFile_bin);
                    // ========================================================
                }
            } 
            else if(pCodeThisFrame->ePicType==1) 
            {
                RFrame* past_frame = past_reference_frame(pCodeThisFrame);
                // PDME
                NGV_PDME_Frame(m_np, pCodeThisFrame, past_frame, pCodeThisFrame->PDSAD, pCodeThisFrame->PMV);
                // FDME
                {
                    Ipp32u fPos;
                    for(fPos=0;fPos<((VidData*)(m_np->inData))->R1.fBlocks;fPos++)
                    {
                        pCodeThisFrame->FDSAD[fPos]                =    INT_MAX;
                    }
                }
                // PPic QP Map
                // Last PPic QP Map's (for complete gop only)
                if( (pCodeThisFrame->TR-past_frame->TR) >= m_np->maxpdist) 
                {
                    //LogMessage(generic_debug, LOG_LVL_ALWAYS, "Generating P Qp Map TR %d", pCodeThisFrame->TR);
                    PPicDetermineQpMap(m_np, pCodeThisFrame, past_frame);
                    WriteQpMap(pCodeThisFrame, &acQPMap[pCodeThisFrame->TR]);

                    // aya=====================================================
                    double dqp_orig = acQPMap[pCodeThisFrame->TR].getAvgQP();
                    int dqp = (int)floor (dqp_orig + 0.5);
                    if(dqpFile)
                    {
                        fprintf(dqpFile, "P0 TR = %u, %15.3f, %i\n", pCodeThisFrame->TR, dqp_orig, dqp);
                    }
                    //fwrite(&dqp, sizeof(int), 1, dqpFile_bin);
                    // ========================================================
                }
                // Compute GOP SADpp, sqrSCpp
                {
                    int gop_size = 1;
                    double avgTSC=pCodeThisFrame->TSC;
                    double avgsqrSCpp =  pCodeThisFrame->SC;
                    RFrame *pFrame=pCodeThisFrame->past_frame;
                    while(past_frame!=pFrame) {
                        avgTSC += pFrame->TSC;
                        avgsqrSCpp += pFrame->SC;
                        gop_size++;
                        pFrame = pFrame->past_frame;
                    }
                    avgTSC/=gop_size;
                    avgTSC/=(pCodeThisFrame->image.width*pCodeThisFrame->image.height);
                    avgsqrSCpp/=gop_size;
                    avgsqrSCpp/=sqrt((double)(pCodeThisFrame->image.width*pCodeThisFrame->image.height));
                    avgsqrSCpp = sqrt(avgsqrSCpp);
                    if( (pCodeThisFrame->TR-past_frame->TR) >= np.maxpdist) {
                        //LogMessage(generic_debug, LOG_LVL_ALWAYS, "Generating P Qp Map TR %d SC %f TC %f", pCodeThisFrame->TR, avgsqrSCpp, avgTSC);
                    } else {
                        //LogMessage(generic_debug, LOG_LVL_ALWAYS, "                    TR %d SC %f TC %f", pCodeThisFrame->TR, avgsqrSCpp, avgTSC);
                    }
                    pFrame=pCodeThisFrame;
                    gop_size = 0;
                    while(past_frame!=pFrame) {
                        acQPMap[pFrame->TR].setAvgGopSADpp(avgTSC);
                        acQPMap[pFrame->TR].setAvgGopSCpp(avgsqrSCpp);
                        pFrame = pFrame->past_frame;
                        gop_size++;
                    }
                }
            } else {
                memset(pCodeThisFrame->qp_mask, 0, sizeof(int)*pCodeThisFrame->widthInTiles*pCodeThisFrame->heightInTiles);
            }
           // if(global_log_level>0 && pCodeThisFrame->ePicType!=RV_FPIC) VisualizeAllBlks(&viz, pCodeThisFrame);
            pCodeThisFrame->coded = TRUE;
            DeQueue(m_pInputFrameQueue, pCodeThisFrame);
        }
        i++;
        uLatency--;
    }

    // ending time
    //dResult = (double)(clock()-lBefore) / CLOCKS_PER_SEC;

    //NGV_PDist np = *m_np;
    //np.avgSC /= np.totalFrames; np.avgSCid /= np.totalFrames; np.avgTSC /=np.totalFrames; np.avgTSCid /= np.totalFrames; 
    
    return failed;
}

/**
 * Open file for reading Y'CbCr frames and Preanalyze withgive GOP size, framerate and key frame interval.
 *
 * Frames read have bitdepth fileBitDepth, and are automatically reformatted to 8 bit depth
 *
 * \param pchFile          file name string
 * \param fileBitDepthY     bit-depth of input/output file data (luma component).
 * \param fileBitDepthC     bit-depth of input/output file data (chroma components).
 * \param internalBitDepthY bit-depth to scale image data to/from when reading/writing (luma component).
 * \param internalBitDepthC bit-depth to scale image data to/from when reading/writing (chroma components).
 */
void TVideoPreanalyzer::open( Ipp8u* pchFile, Ipp32u width, Ipp32u height, int fileBitDepthY, int fileBitDepthC, int iFrameRate, int iGOPSize, Ipp32u uiIntraPeriod, Ipp32u framesToBeEncoded)
{
    m_pchFile       = pchFile;
    m_iWidth        = width;
    m_iHeight       = height;
    m_fileBitDepthY = fileBitDepthY;
    m_fileBitDepthC = fileBitDepthC;
    m_iFrameRate    = iFrameRate;
    m_iGOPSize      = iGOPSize;
    m_uiIntraPeriod  = uiIntraPeriod;
    m_uiSkipFrames  = 0;
    m_uiFramesToBeEncoded = framesToBeEncoded;

    m_np = NULL;
    m_np = new NGV_PDist;
    NGV_PDist_Init(m_np);

    m_pInputFrameList    = &m_InputFrameList;
    m_pInputFrameQueue    = &m_InputFrameQueue;

    RFrameVector_Init(&m_InputFrameList);
    RFrameQueue_Init(&m_InputFrameQueue);
    //Viz_Init(&m_viz);

    //int width = m_iWidth;
    //int height = m_iHeight;
    m_corrected_height = (height + REGION_GRID_SIZE-1)/REGION_GRID_SIZE;
    m_corrected_height *= REGION_GRID_SIZE;
    m_corrected_width  = (width + REGION_GRID_SIZE-1)/REGION_GRID_SIZE;
    m_corrected_width  *= REGION_GRID_SIZE;

    //NGV_PDist np = *m_np;
    //global_log_level    = 0; 
    m_np->uForcePDistVal   = 2;
    m_np->bForcePDist      = TRUE;
    m_np->uRefMod          = m_iGOPSize/2 -1;
    m_np->maxpdist         = m_iGOPSize;
    m_np->fps              = m_iFrameRate;
    ((VidData*)m_np->inData)->Key = m_uiIntraPeriod;

    if(m_uiIntraPeriod < (Ipp32u)(2*m_iFrameRate)) m_np->bClosedGOP = FALSE;    // Make it automatic

    NGV_PDist_Alloc(m_np, m_corrected_width, m_corrected_height);

    m_dqpFile = NULL;
    //m_dqpFile = fopen("dqp_file.txt", "w+t");

    m_uLatency = 0;
    
    m_acQPMap = NULL;
    m_acQPMap = new TQPMap[framesToBeEncoded];

    for(Ipp32u iPoc=0; iPoc<framesToBeEncoded; iPoc++) 
    {
        m_acQPMap[iPoc].create(m_iWidth, m_iHeight, 64, 64);   // Only 64x64 supported for now
    }

    m_frameNumber = 0;

    m_firstFrame = TRUE;
    // Open Files
    // Alloc Pdist
}

void TVideoPreanalyzer::skipFrames(Ipp32u numFrames)
{
  m_uiSkipFrames = numFrames;
}


void TVideoPreanalyzer::close()
{
    if(m_np)
    {
        NGV_PDist_Free(m_np, m_pInputFrameList->head, m_pInputFrameList->length);
        delete m_np;
        m_np = NULL;
    }

    if(m_dqpFile)
    {
        fclose( m_dqpFile );
    }

    if ( m_acQPMap )
    {
        delete[] m_acQPMap;
    }
    
    //destroy_frames_in_list(pInputFrameList);
}

//*********************************************************
//  NGV PDIST
//*********************************************************
#define DUMP_PDIST 0
                                           //7,6,5,4, 3,2,1,0
const Ipp32s   fir7[16]                    =   {0,0,0,0,-1,0,9,16,9,0,-1,0,0,0,0,32};
                                           //6,5,4,3, 2,1,0
const Ipp32s   fir6[14]                    =   {0,0,0,0, 0,1,2,1,0,0,0,0,0,4};
#define NEW_TABLE

#ifndef NEW_TABLE
const Ipp32s   PDISTTbl2[NumTSC*NumSC]    =
{
    0x05,0x05,0x05,0x05,0x05,0x05,0x05,0x05,0x05,0x05,
    0x04,0x04,0x04,0x04,0x04,0x04,0x04,0x04,0x04,0x04,
    0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,
    0x02,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,
    0x01,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,
    0x01,0x02,0x02,0x02,0x03,0x03,0x03,0x03,0x03,0x03,
    0x01,0x02,0x02,0x02,0x02,0x03,0x03,0x02,0x02,0x02,
    0x01,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x01,0x01,
    0x01,0x01,0x02,0x02,0x02,0x02,0x02,0x02,0x01,0x01,
    0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01
};
#else
const Ipp32s   PDISTTbl2[NumTSC*NumSC]    =
{
//  0   1   2   3   4   5   6   7   8   9   SCVal  TSCVal
    2,  3,  4,  4,  4,  5,  5,  5,  5,  5,    // 0
    2,  2,  3,  3,  4,  4,  5,  5,  5,  5,    // 1
    1,  2,  2,  3,  3,  3,  4,  4,  5,  5,  // 2
    1,  1,  2,  2,  3,  3,  3,  4,  4,  5,  // 3
    1,  1,  2,  2,  3,  3,  3,  3,  3,  4,  // 4
    1,  1,  1,  2,  2,  3,  3,  3,  3,  3,  // 5
    1,  1,  1,  1,  2,  2,  3,  3,  3,  3,  // 6
    1,  1,  1,  1,  2,  2,  2,  3,  3,  3,  // 7
    1,  1,  1,  1,  1,  2,  2,  2,  2,  2,  // 8
    1,  1,  1,  1,  1,  1,  1,  1,  1,  1   // 9
};

const Ipp32s   PDISTTbl3[NumTSC*NumSC]    =
{
//  0   1   2   3   4   5   6   7   8   9   SCVal  TSCVal
    5,    5,    5,    5,    4,    5,    5,    5,    5,    5,            // 0
    5,    5,    5,    5,    5,    4,    5,    5,    5,    5,            // 1
    1,    5,    5,    5,    5,    3,    4,    4,    5,    5,            // 2
    1,    5,    5,    5,    5,    5,    3,    4,    4,    5,            // 3
    1,    1,    4,    4,    5,    5,    3,    3,    3,    4,            // 4
    1,    1,    1,    4,    4,    5,    3,    3,    3,    3,            // 5
    1,    1,    1,    1,    2,    4,    3,    3,    3,    3,            // 6
    1,    1,    1,    1,    2,    2,    2,    3,    3,    3,            // 7
    1,    1,    1,    1,    1,    2,    2,    2,    2,    2,            // 8
    1,    1,    1,    1,    1,    1,    1,    1,    1,    1            // 9
};

//const Ipp32s   PDISTTbl3[NumTSC*NumSC]    =
//{
//    5,    5,    5,    5,    5,    5,    5,    5,    5,    5,
//    5,    5,    5,    5,    5,    5,    5,    5,    5,    5,
//    4,    5,    5,    5,    5,    5,    5,    5,    5,    5,
//    4,    4,    5,    5,    5,    5,    5,    5,    5,    5,
//    4,    4,    4,    4,    5,    5,    5,    5,    5,    5,
//    3,    3,    4,    4,    4,    5,    5,    5,    5,    5,
//    3,    3,    3,    4,    4,    4,    4,    5,    5,    5,
//    2,    2,    2,    2,    3,    4,    4,    4,    5,    5,
//    1,    2,    2,    2,    2,    3,    4,    4,    4,    5,
//    1,    1,    2,    2,    2,    2,    3,    3,    4,    4
//
//};

#endif

static Ipp64f lmt_sc2[NumSC]            =    {4.0, 9.0, 15.0, 23.0, 32.0, 42.0, 53.0, 65.0, 78, INT_MAX}; // lower limit of SFM(Rs,Cs) range for spatial classification
// 9 ranges of SC are: 0 0-4, 1 4-9, 2 9-15, 3 15-23, 4 23-32, 5 32-44, 6 42-53, 7 53-65, 8 65-78, 9 78->??

static Ipp64f lmt_tsc2[NumTSC]            =    {0.75, 1.5, 2.25, 3.0, 4.0, 5.0, 6.0, 7.5, 9.25, INT_MAX};               // lower limit of AFD
// 8 ranges of TSC (based on FD) are:0 0-0.75 1 0.75-1.5, 2 1.5-2.25. 3 2.25-3, 4 3-4, 5 4-5, 6 5-6, 7 6-7.5, 8 7.5-9.25, 9 9.25->??

static Ipp64f TH2[4]                    =    {-12,-4,4,12};

NGV_Bool    Params_Init(VidData *inData)
{
    inData->accuracy                    =    4;
    inData->R1.block_size_h                =    8;
    inData->R1.block_size_w                =    8;
    inData->frameRange                    =    500;
#ifndef EXTERNAL_RELEASE
    inData->mvFile                        =    "motest.txt";
    inData->SADfile                        =    "SAD.txt";
    inData->outputFile                    =    "out.yuv";
#endif
    inData->shotFile                    =    NULL;
    inData->NumOfFrames                    =    1;
    inData->jumpToFrame                    =    0;
    inData->minpdist                    =    1;
    inData->maxpdist                    =    1;
    inData->pdistCorrect                =   0;
    inData->bForcePDist                 =   0;
    inData->uForcePDistVal              =   2;
    inData->bNoSceneDetect              =   0;
    inData->filter                        =    2;
    inData->stages                        =    3;
    inData->R1.height                    =    288;
    inData->R1.width                    =    352;
    inData->R1.Yframe                    =    101376;
    inData->R1.UVframe                    =    50688;
    inData->R1.YUVframe                    =    152064;
    inData->varpdist                    =    0;
    inData->fps                            =    30;
    inData->Key                            =    INT_MAX;
    inData->logLevel                    = 0;
    inData->uRefMod                     = 3;

    return 0;
}

void NGV_PDist_Init(NGV_PDist *t)
{
    t->inData=NULL;
    t->SADOut = NULL;
    t->video = NULL;
    t->inData = (VidData*)NgvMalloc(sizeof(VidData));
    // Error checking
    if (t->inData == NULL)
    {
        VM_ASSERT(0);
        NgvPrintf("Error NGV_PDist_Init: at line number %d in file %s\n", __LINE__, __FILE__);
        return;
    }
    Params_Init((VidData*)t->inData);    
    t->SADOut = NgvMalloc(sizeof(DataWriter));
    // Error checking
    if (t->SADOut == NULL)
    {
        VM_ASSERT(0);
        NgvPrintf("Error NGV_PDist_Init: at line number %d in file %s\n", __LINE__, __FILE__);
        return;
    }

    writerInit((DataWriter*)t->SADOut);
    t->video = NgvMalloc(sizeof(VidRead));
    // Error checking
    if (t->video == NULL)
    {
        VM_ASSERT(0);
        NgvPrintf("Error NGV_PDist_Init: at line number %d in file %s\n", __LINE__, __FILE__);
        return;
    }

    readerInit((VidRead*)t->video);
#ifndef NEW_TABLE
    t->minpdist = 2;
#else
    t->minpdist = 1;
#endif
    t->maxpdist = MAXPDIST;
#ifndef NEW_TABLE
    t->pdistCorrect = -1;
#else
    t->pdistCorrect = -1;   // v326 Default
#endif
    t->bClosedGOP   = TRUE;
    t->bClosedGOPSet = FALSE;
    t->bForcePDist  = FALSE;
    t->bRankOrder   = TRUE;     // v326 Default
    t->bModToF      = TRUE;     // v326 Default
    t->bAutoModToF  = FALSE;    // New Default
    t->bPartialInOrder = FALSE;
    t->uRankToF     = 1;
    
    t->uRefMod      = REFMOD_LONG_GOP;
    t->uRefModStart = 0;
    t->bRefModSet   = FALSE;
    t->uRefRankMax   = 3;
    t->uForcePDistVal = 3;
    t->bNoSceneDetect = FALSE;
    t->fps = 30;
    t->TR = 0;
    t->unmodTR = 0;
    t->IPframeFound = FALSE;
    t->PdIndex = -1;
#ifdef PD_LOCAL_POOL 
    t->samplePoolIndex = t->samplePoolSize = 0;
#endif
    t->alloc_pdist = 1;
    t->dealloc_pdist = 1;
    t->lookahead = 0; 
    t->remaining = 0;
    t->PdIndexL = 0; 
}

static NGV_Bool    SetParams(NGV_PDist *t, int width, int height)
{
    NGV_Bool    failed                        =    FALSE;
    VidData* inData = (VidData*)t->inData;

    t->totalFrames = 0;
    t->avgSC = 0.0; 
    t->avgTSC = 0.0; 
    t->avgSCid = 0.0; 
    t->avgTSCid = 0.0; 

    inData->R1.width = width;
    inData->R1.height = height;    
    inData->minpdist = t->minpdist;
    inData->fps = t->fps;
    inData->pdistCorrect = t->pdistCorrect;
    inData->bForcePDist = t->bForcePDist;
    inData->uForcePDistVal = t->uForcePDistVal;
    inData->bNoSceneDetect  = t->bNoSceneDetect;


    /* logic */        
    if(inData->R1.block_size_w > 4)
        inData->R1.block_size_w            =    8;
    else if(inData->R1.block_size_w <= 4)
        inData->R1.block_size_w            =    4;

    inData->R1.block_size_h                =    inData->R1.block_size_w;
    inData->R16th.block_size_h            =    inData->R1.block_size_w;
    inData->R16th.block_size_w            =    inData->R1.block_size_w;
    inData->R4er.block_size_h            =    inData->R1.block_size_w;
    inData->R4er.block_size_w            =    inData->R1.block_size_w;

    if(inData->accuracy < 2)
        inData->accuracy                =    1;
    else if(inData->accuracy < 3)
        inData->accuracy                =    2;
    else
        inData->accuracy                =    4;
    if(inData->stages < 2)
        inData->stages                    =    2;
    else if(inData->stages > 3)
        inData->stages                    =    3;

    if(inData->minpdist < 1)
        inData->minpdist                =    1;
    if(inData->minpdist > 8)
        inData->minpdist                =    8;
    if(inData->maxpdist < inData->minpdist)
        inData->maxpdist                =    inData->minpdist;
    if(inData->maxpdist > 8)
        inData->maxpdist                =    8;

    /*Full size image*/
    inData->R1.Yframe                    =    inData->R1.width * inData->R1.height;
    inData->R1.YUVframe                    =    (inData->R1.Yframe * 3) >> 1;
    inData->R1.UVframe                    =    inData->R1.Yframe >> 2;
    inData->R1.exwidth                    =    inData->R1.width + (2 * (inData->R1.block_size_w + 4));
    inData->R1.exheight                    =    inData->R1.height + (2 * (inData->R1.block_size_h + 4));
    inData->R1.exYframe                    =    inData->R1.exwidth * inData->R1.exheight;
    inData->R1.hBlocks                    =    inData->R1.height / inData->R1.block_size_h;
    inData->R1.wBlocks                    =    inData->R1.width / inData->R1.block_size_w;
    inData->R1.fBlocks                    =    inData->R1.hBlocks * inData->R1.wBlocks;
    inData->R1.initPoint                =    (inData->R1.exwidth * (inData->R1.block_size_h + 4)) + inData->R1.block_size_w + 4;
    inData->R1.endPoint                    =    inData->R1.initPoint + (inData->R1.exwidth * inData->R1.height);
    inData->R1.sidesize                    =    inData->R1.height + inData->R1.block_size_h + 4;
    inData->R1.MVspaceSize                =    inData->R1.hBlocks * inData->R1.wBlocks;

    /*Quarter sized image*/
    inData->R4er.width                    =   inData->R1.width >> 1;
    inData->R4er.height                    =    inData->R1.height >> 1;
    inData->R4er.Yframe                    =    inData->R4er.width * inData->R4er.height;
    inData->R4er.YUVframe                =    (inData->R4er.Yframe * 3) >> 1;
    inData->R4er.UVframe                =    inData->R4er.Yframe >> 2;
    inData->R4er.exwidth                =    inData->R4er.width + (2 * (inData->R4er.block_size_w + 4));
    inData->R4er.exheight                =    inData->R4er.height + (2 * (inData->R4er.block_size_h + 4));
    inData->R4er.exYframe                =    inData->R4er.exwidth * inData->R4er.exheight;
    inData->R4er.hBlocks                =    inData->R4er.height / inData->R4er.block_size_h;
    inData->R4er.wBlocks                =    inData->R4er.width / inData->R4er.block_size_w;
    inData->R4er.fBlocks                =    inData->R4er.hBlocks * inData->R4er.wBlocks;
    inData->R4er.initPoint                =    (inData->R4er.exwidth * (inData->R4er.block_size_h + 4)) + inData->R4er.block_size_w + 4;
    inData->R4er.endPoint                =    inData->R4er.initPoint + (inData->R4er.exwidth * inData->R4er.height);
    inData->R4er.sidesize                =    inData->R4er.height + inData->R4er.block_size_h + 4;
    inData->R4er.MVspaceSize            =    inData->R4er.hBlocks * inData->R4er.wBlocks;

    /*1/16 sized image*/
    inData->R16th.width                    =    inData->R1.width >> 2;
    inData->R16th.height                =    inData->R1.height >> 2;
    inData->R16th.Yframe                =    inData->R16th.width * inData->R16th.height;
    inData->R16th.YUVframe                =    (inData->R16th.Yframe * 3) >> 1;
    inData->R16th.UVframe                =    inData->R16th.Yframe >> 2;
    inData->R16th.exwidth                =    inData->R16th.width + (2 * (inData->R16th.block_size_w + 4));
    inData->R16th.exheight                =    inData->R16th.height + (2 * (inData->R16th.block_size_h + 4));
    inData->R16th.exYframe                =    inData->R16th.exwidth * inData->R16th.exheight;
    inData->R16th.hBlocks                =    inData->R16th.height / inData->R16th.block_size_h;
    inData->R16th.wBlocks                =    inData->R16th.width / inData->R16th.block_size_w;
    inData->R16th.fBlocks                =    inData->R16th.hBlocks * inData->R16th.wBlocks;
    inData->R16th.initPoint                =    (inData->R16th.exwidth * (inData->R16th.block_size_h + 4)) + inData->R16th.block_size_w + 4;
    inData->R16th.endPoint                =    inData->R16th.initPoint + (inData->R16th.exwidth * inData->R16th.height);
    inData->R16th.sidesize                =    inData->R16th.height + inData->R16th.block_size_h + 4;
    inData->R16th.MVspaceSize            =    inData->R16th.hBlocks * inData->R16th.wBlocks;

    inData->SADfile                     =   NULL;

    return failed;
}

NGV_Bool NGV_PDist_Alloc(NGV_PDist *t, int width, int height)
{    
    VidData* inData = (VidData*)t->inData;
    //DataWriter* SADOut = (DataWriter*)t->SADOut;
    VidRead* video = (VidRead *) t->video;
    NGV_Bool failed = FALSE;
    int d= 0;
    int RefMod0 = 0;
    int RefMod1 = t->uRefMod+1;
    do {
        d++;
        if(RefMod0+(RefMod1-RefMod0)/2==1) {
            break;
        } else if(1<RefMod0+(RefMod1-RefMod0)/2) {
            RefMod1 = RefMod0 + (RefMod1-RefMod0)/2;
        } else {
            RefMod0 = RefMod0 + (RefMod1-RefMod0)/2;
        }
    } while((RefMod1-RefMod0)>1);
    if(t->bForcePDist && t->uForcePDistVal==1)  t->uRefRankMax = d; //  No F Frames
    else                                        t->uRefRankMax = d+1;
    SetParams(t, width, height);

    ////PDist table selection with respect to FPS
    //if(t->fps > 35)
    //    video->PDistanceTable    =    (Ipp32s*)PDISTTbl3; //This has to be redesign
    //else
        video->PDistanceTable    =    (Ipp32s*)PDISTTbl2;


    //failed = MvFileWriter_Open(SADOut, inData->SADfile);
    //if(failed) {printf("Could not open file %s\n",inData->SADfile); }
    failed = PdYuvFileReader_Open(video, inData);

    t->videoIn = NgvMalloc(sizeof(VidSample));
    return failed;
}

void NGV_PDist_SetMinPDist(NGV_PDist *t, int uLatency)
{    
    if(uLatency) t->minpdist = 2;
    else t->minpdist = 1;
}

void NGV_PDist_Free(NGV_PDist *t, RFrame *ptr, int length)
{
    int i;

    if(t->inData) free(t->inData);
    t->inData = NULL;

    if(t->SADOut) {
        //MvFileWriter_Close((DataWriter*)t->SADOut);
        free(t->SADOut);
        t->SADOut = NULL;
    }

    if(t->video) {
        PdYuvFileReader_Free((VidRead*)t->video);
        free(t->video);
        t->video = NULL;
    }
    
    if(t->videoIn) {
        free(t->videoIn);
        t->videoIn = NULL;
    }

#ifdef PD_LOCAL_POOL 
    for(i=0;i<t->samplePoolSize;i++)
    {       
        if(t->samplePool[i]) {
            VidSampleFree(t, t->samplePool[i]);
            free(t->samplePool[i]);
            t->samplePool[i] = NULL;
        }
    }
#else
    for(i=0;i<length;i++)
    {
        if(ptr) {
            if(ptr->sample) {
                VidSampleFree(t, (VidSample*)ptr->sample);
                free(ptr->sample);
                ptr->sample = NULL;
            }
            ptr=ptr->futr_frame;
        }
    }
#endif
}

void ExtendBorders(Ipp8u* ImageToExtend, ImDetails imageInfo)    
{
    Ipp8u*  bRow;
    Ipp8u*  tRow;
    Ipp32u        row, col;
    Ipp8u        uLeftFillVal;
    Ipp8u        uRightFillVal;
    Ipp8u        *pByteSrc, *pSrcLeft, *pSrcRight;

    // section 1 at bottom
    // obtain pointer to start of bottom row of original frame
    //if((Ipp32u)StartPtr %4) printf("Not 4 Byte aligned");
    pByteSrc                        =    ImageToExtend + imageInfo.endPoint;
    bRow                            =    pByteSrc - imageInfo.exwidth;
    for(row=0; row<(imageInfo.block_size_h + 4); row++)    {
        for(col=0;col<imageInfo.width;col++)
            *(pByteSrc + col)        =    *(bRow + col);
        pByteSrc                    =    pByteSrc + imageInfo.exwidth;
    }

    // section 2 on left and right
    // obtain pointer to start of first row of original frame
    pByteSrc                        =    ImageToExtend + imageInfo.initPoint;
    for(row=0; row<imageInfo.sidesize; row++, pByteSrc += imageInfo.exwidth)    {
        // get fill values from left and right columns of original frame
        uLeftFillVal                =    *pByteSrc;
        pSrcLeft                    =    pByteSrc - (imageInfo.block_size_w + 4);
        pSrcRight                    =    pByteSrc + imageInfo.width;
        uRightFillVal                =    *(pSrcRight - 1);

        // fill all bytes on both edges
        for(col=0; col<imageInfo.block_size_w + 4; col++)    {
            *(pSrcLeft + col)        =    uLeftFillVal;
            *(pSrcRight + col)        =    uRightFillVal;
        }
    }

    // section 3 at top
    // obtain pointer to top row of original frame, less expand pels
    pByteSrc                        =    ImageToExtend + imageInfo.initPoint - (imageInfo.block_size_w + 4);
    tRow                            =    ImageToExtend;

    for(row=0; row<(imageInfo.block_size_h+4); row++, tRow += imageInfo.exwidth)
        for(col=0; col<imageInfo.exwidth; col++)
            *(tRow + col)            =    *(pByteSrc + col);
}

NGV_Bool    PdReduceImage(VidRead *video, VidData *inData, YuvImage *t, VidSample *videoIn)    {
    NGV_Bool    failed                    =    FALSE;
    Ipp32u        i;
    Ipp32u        j,k;
    Ipp32u        im7, im6, im5, im4, im3, im2, im1, i0, ip1, ip2, ip3, ip4, ip5, ip6, ip7;
    Ipp32u        jm6, jm5, jm4, jm3, jm2, jm1, j0, jp1, jp2, jp3,jp4, jp5, jp6;
    Ipp32u        width, height;
    Ipp8u        *qdd, *hdd, *fdd;
    Ipp8u        *tt,*bH,*bQ;

    tt                                =    t->y_plane;
    fdd                                =    videoIn->R1Buffer.exImage.Y;
    fdd                                +=    inData->R1.initPoint;
    hdd                                =    videoIn->R4erBuffer.exImage.Y;
    hdd                                +=    inData->R4er.initPoint;
    qdd                                =    videoIn->R16thBuffer.exImage.Y;
    qdd                                +=    inData->R16th.initPoint;
    bH                                =    video->bufferH;
    bQ                                =    video->bufferQ;

    for(i=0;i<inData->R1.height;i++)    {
        memcpy_s(fdd, t->width, tt, t->width);
        fdd                        +=    inData->R1.exwidth;
        tt                        +=    t->y_pitch;
    }

    /*Horizontal filter - Half / Half*/
    tt                                =    t->y_plane;
    width                            =    inData->R1.width;
    for(i=0;i<inData->R1.height;i++)        {
        for(j=0;j<inData->R4er.width;j++)    {
#ifdef ODD_FILTER
            k                        =    2*j + 1;
#else
            k                        =    2*j;
#endif
            im7                    =    (k<7) ? 0 : k-7;
            im6                    =    (k<6) ? 0 : k-6;
            im5                    =    (k<5) ? 0 : k-5;
            im4                    =    (k<4) ? 0 : k-4;
            im3                    =    (k<3) ? 0 : k-3;
            im2                    =    (k<2) ? 0 : k-2;
            im1                    =    (k<1) ? 0 : k-1;
            i0                    =    (k<width) ? k : width-1;
            ip1                    =    (k<width-1) ? k+1 : width-1;
            ip2                    =    (k<width-2) ? k+2 : width-1;
            ip3                    =    (k<width-3) ? k+3 : width-1;
            ip4                    =    (k<width-4) ? k+4 : width-1;
            ip5                    =    (k<width-5) ? k+5 : width-1;
            ip6                    =    (k<width-6) ? k+6 : width-1;
            ip7                    =    (k<width-7) ? k+7 : width-1;

            bH[j]                =    ClampVal((Ipp32s)((
                fir7[0]*tt[im7]
            +    fir7[1]*tt[im6]
            +    fir7[2]*tt[im5]
            +    fir7[3]*tt[im4]
            +    fir7[4]*tt[im3]
            +    fir7[5]*tt[im2]
            +    fir7[6]*tt[im1]
            +    fir7[7]*tt[i0]
            +    fir7[8]*tt[ip1]
            +    fir7[9]*tt[ip2]
            +    fir7[10]*tt[ip3]
            +    fir7[11]*tt[ip4]
            +    fir7[12]*tt[ip5]
            +    fir7[13]*tt[ip6]
            +    fir7[14]*tt[ip7]
            +    fir7[15]/2) / fir7[15]));
        }
        tt                            +=    t->y_pitch;
        bH                            +=    inData->R4er.width;
    }

    /*Vertical filter - Half / Half*/
    width                            =    inData->R4er.width;
    height                            =    inData->R1.height;
    bH                                =    video->bufferH;
    for (i=0; i<width; i++)            {
        for (j=0; j<inData->R4er.height; j++)    {
            k                    =    j*2;

            jm6                    =    (k<6) ? 0 : k-6;
            jm5                    =    (k<5) ? 0 : k-5;
            jm4                    =    (k<4) ? 0 : k-4;
            jm3                    =    (k<3) ? 0 : k-3;
            jm2                    =    (k<2) ? 0 : k-2;
            jm1                    =    (k<1) ? 0 : k-1;
            j0                    =    (k<height) ? k : height-1;
            jp1                    =    (k<height-1) ? k+1 : height-1;
            jp2                    =    (k<height-2) ? k+2 : height-1;
            jp3                    =    (k<height-3) ? k+3 : height-1;
            jp4                    =    (k<height-4) ? k+4 : height-1;
            jp5                    =    (k<height-5) ? k+5 : height-1;
            jp6                    =    (k<height-6) ? k+6 : height-1;

            hdd[inData->R4er.exwidth*j]    =    ClampVal((Ipp32s)((
                fir6[0]*bH[width*jm6]
            +    fir6[1]*bH[width*jm5]
            +    fir6[2]*bH[width*jm4]
            +    fir6[3]*bH[width*jm3]
            +    fir6[4]*bH[width*jm2]
            +    fir6[5]*bH[width*jm1]
            +    fir6[6]*bH[width*j0]           
            +    fir6[7]*bH[width*jp1]
            +    fir6[8]*bH[width*jp2]
            +    fir6[9]*bH[width*jp3]
            +    fir6[10]*bH[width*jp4]
            +    fir6[11]*bH[width*jp5]
            +    fir6[12]*bH[width*jp6]             
            +    fir6[13]/2) / fir6[13]));
        }
        bH++;
        hdd++;
    }

    // This code just sub samples the half half result */
    /*Horizontal filter - Quarter / Quarter*/
    tt                                =    t->y_plane;
    width                            =    inData->R1.width;
    for(i=0;i<inData->R1.height;i++)        {
        for(j=0;j<inData->R16th.width;j++)    {
#ifdef ODD_FILTER
            k                    =    4*j + 2;
#else
            k                    =    4*j;
#endif
            im7                    =    (k<7) ? 0 : k-7;
            im6                    =    (k<6) ? 0 : k-6;
            im5                    =    (k<5) ? 0 : k-5;
            im4                    =    (k<4) ? 0 : k-4;
            im3                    =    (k<3) ? 0 : k-3;
            im2                    =    (k<2) ? 0 : k-2;
            im1                    =    (k<1) ? 0 : k-1;
            i0                    =    (k<width) ? k : width-1;
            ip1                    =    (k<width-1) ? k+1 : width-1;
            ip2                    =    (k<width-2) ? k+2 : width-1;
            ip3                    =    (k<width-3) ? k+3 : width-1;
            ip4                    =    (k<width-4) ? k+4 : width-1;
            ip5                    =    (k<width-5) ? k+5 : width-1;
            ip6                    =    (k<width-6) ? k+6 : width-1;
            ip7                    =    (k<width-7) ? k+7 : width-1;

            bQ[j]                =    ClampVal((Ipp32s)((
                fir7[0]*tt[im7]
            +    fir7[1]*tt[im6]
            +    fir7[2]*tt[im5]
            +    fir7[3]*tt[im4]
            +    fir7[4]*tt[im3]
            +    fir7[5]*tt[im2]
            +    fir7[6]*tt[im1]
            +    fir7[7]*tt[i0]
            +    fir7[8]*tt[ip1]
            +    fir7[9]*tt[ip2]
            +    fir7[10]*tt[ip3]
            +    fir7[11]*tt[ip4]
            +    fir7[12]*tt[ip5]
            +    fir7[13]*tt[ip6]                
            +    fir7[14]*tt[ip7]                
            +    fir7[15]/2) / fir7[15]));
        }
        tt                            +=    t->y_pitch;
        bQ                            +=    inData->R16th.width;
    }

    /*Vertical filter - Quarter / Quarter*/
    width                            =    inData->R16th.width;
    height                            =    inData->R1.height;
    bQ                                =    video->bufferQ;
    for (i=0; i<width; i++)            {
        for (j=0; j<inData->R16th.height; j++)    {
            k                    =    j*4;
            jm6                    =    (k<6) ? 0 : k-6;
            jm5                    =    (k<5) ? 0 : k-5;
            jm4                    =    (k<4) ? 0 : k-4;
            jm3                    =    (k<3) ? 0 : k-3;
            jm2                    =    (k<2) ? 0 : k-2;
            jm1                    =    (k<1) ? 0 : k-1;
            j0                    =    (k<height) ? k : height-1;
            jp1                    =    (k<height-1) ? k+1 : height-1;
            jp2                    =    (k<height-2) ? k+2 : height-1;
            jp3                    =    (k<height-3) ? k+3 : height-1;
            jp4                    =    (k<height-4) ? k+4 : height-1;
            jp5                    =    (k<height-5) ? k+5 : height-1;
            jp6                    =    (k<height-6) ? k+6 : height-1;

            qdd[inData->R16th.exwidth*j]    =    ClampVal((Ipp32s)((
                fir6[0]*bQ[width*jm6]
            +    fir6[1]*bQ[width*jm5]
            +    fir6[2]*bQ[width*jm4]
            +    fir6[3]*bQ[width*jm3]
            +    fir6[4]*bQ[width*jm2]
            +    fir6[5]*bQ[width*jm1]
            +    fir6[6]*bQ[width*j0]           
            +    fir6[7]*bQ[width*jp1]
            +    fir6[8]*bQ[width*jp2]
            +    fir6[9]*bQ[width*jp3]
            +    fir6[10]*bQ[width*jp4]
            +    fir6[11]*bQ[width*jp5]
            +    fir6[12]*bQ[width*jp6]             
            +    fir6[13]/2) / fir6[13]));
        }
        bQ++;
        qdd++;
    }

    ExtendBorders(videoIn->R1Buffer.exImage.Y, inData->R1);
    ExtendBorders(videoIn->R4erBuffer.exImage.Y, inData->R4er);
    ExtendBorders(videoIn->R16thBuffer.exImage.Y, inData->R16th);

    return failed;
}

void processMEFrame(VidRead *video, VidData *inData, VidSample *videoIn, VidSample *videoRef, Ipp32s PdIndex, NGV_Bool firstFrame, NGV_Bool keyFrame, RFrame *ptr)
{
    Ipp32s        tscVal, scVal, pdVal;
    Ipp32u        valNoM, valb, fPos, tbPos;
    Ipp64f        TSC, SC, RsGlobal, CsGlobal, AFD;
    NGV_Bool    Schange                        =    FALSE;
    NGV_Bool    Gchange                        =    FALSE;

    if(firstFrame)
    {
        RsCsCalc(&(videoIn->R1Buffer), inData->R1);
        RsGlobal                        =    videoIn->R1Buffer.RsVal;
        CsGlobal                        =    videoIn->R1Buffer.CsVal;
        SC                              =    sqrt((RsGlobal*RsGlobal) + (CsGlobal*CsGlobal));
        video->logic[PdIndex].frameNum    =    ptr->TR;
        video->logic[PdIndex].Rs        =    RsGlobal;
        video->logic[PdIndex].Cs        =    CsGlobal;
        video->logic[PdIndex].SC        =    SC;
        video->logic[PdIndex].TSC        =    0;
        video->logic[PdIndex].AFD        =    256.0;
        video->logic[PdIndex].TSCindex    =    0;
        video->logic[PdIndex].SCindex    =    0;
        video->logic[PdIndex].Schg        =    0;
        video->logic[PdIndex].Gchg        =    0;
        video->logic[PdIndex].picType    =    0;
        video->logic[PdIndex].pdist        =    0;
        video->logic[PdIndex].ptr        =    ptr;
        ptr->SCindex = video->logic[PdIndex].SCindex;
        ptr->TSCindex = video->logic[PdIndex].TSCindex;
        ptr->SC = sqrt(((RsGlobal*RsGlobal) + (CsGlobal*CsGlobal))*inData->R1.Yframe); //video->logic[PdIndex].SC; 
        ptr->TSC = video->logic[PdIndex].TSC;
        ptr->pdist = video->logic[PdIndex].pdist;
        ptr->Schg = video->logic[PdIndex].Schg; 
        for(fPos=0;fPos<inData->R1.fBlocks;fPos++)
        {
            ptr->PSAD[fPos]                =    0;
        }
    }
    else
    {
        if (videoRef == NULL) { VM_ASSERT(0); return; }

        /*---First stage----*/
        deltaIm(videoIn->R16thBuffer.deltaF.Y,
            videoRef->R16thBuffer.exImage.Y + inData->R16th.initPoint,
            videoIn->R16thBuffer.exImage.Y + inData->R16th.initPoint,
            inData->R16th.width,inData->R16th.exwidth,
            inData->R16th);
        RsCsCalc2(&(videoIn->R16thBuffer), inData->R16th);

        /*---Second stage---*/
        deltaIm(videoIn->R4erBuffer.deltaF.Y,
            videoRef->R4erBuffer.exImage.Y + inData->R4er.initPoint,
            videoIn->R4erBuffer.exImage.Y + inData->R4er.initPoint,
            inData->R4er.width,inData->R4er.exwidth,
            inData->R4er);
        RsCsCalc2(&(videoIn->R4erBuffer), inData->R4er);

        /*---Third stage----*/
        deltaIm(videoIn->R1Buffer.deltaF.Y,
            videoRef->R1Buffer.exImage.Y + inData->R1.initPoint,
            videoIn->R1Buffer.exImage.Y + inData->R1.initPoint,
            inData->R1.width,inData->R1.exwidth,
            inData->R1);
        RsCsCalc(&(videoIn->R1Buffer), inData->R1);
        RsCsCalc2(&(videoIn->R1Buffer), inData->R1);
        RsGlobal                        =   videoIn->R1Buffer.RsVal;
        CsGlobal                        =   videoIn->R1Buffer.CsVal;
        SC                                =   sqrt((RsGlobal*RsGlobal) + (CsGlobal*CsGlobal));

        /*--Motion Estimation--*/
        valNoM                            =   0;
        valb                            =   0;
        /*---First stage----*/
        video->average                    =    0;
        for(fPos=0;fPos<inData->R16th.fBlocks;fPos++)
        {
            ME_One(    video, fPos,
                inData->R16th,
                inData->R4er,
                &(videoIn->R16thBuffer),
                &(videoRef->R16thBuffer),
                &(videoIn->R4erBuffer),
                TRUE, backward,
                inData->accuracy, 0, 1);
            ptr->PDMV[fPos].iMVx = (Ipp16s)((videoIn->R16thBuffer.pBackward[fPos].x * inData->accuracy) + (videoIn->R16thBuffer.pBhalf[fPos].x * (inData->accuracy >> 1)) + (videoIn->R16thBuffer.pBquarter[fPos].x * (inData->accuracy >> 2)));
            ptr->PDMV[fPos].iMVy = (Ipp16s)((videoIn->R16thBuffer.pBackward[fPos].y * inData->accuracy) + (videoIn->R16thBuffer.pBhalf[fPos].y * (inData->accuracy >> 1)) + (videoIn->R16thBuffer.pBquarter[fPos].y * (inData->accuracy >> 2)));
        }
        //write_mv_array(ptr->PDMV, (ptr->heightInRegionGrid*2)/4, (ptr->widthInRegionGrid*2)/4, 8, ptr->TR, "psmv_me1");
        
        /*---Second stage---*/
        video->average                    =    0;
        for(fPos=0;fPos<inData->R4er.fBlocks;fPos++)
        {
            ME_One(    video, fPos,
                inData->R4er,
                inData->R1,
                &(videoIn->R4erBuffer),
                &(videoRef->R4erBuffer),
                &(videoIn->R1Buffer),
                FALSE, backward,
                inData->accuracy, 1, 1);
            /// NGVv326 Fix Thresh for extended range -N
            ptr->PDMV[fPos].iMVx = (Ipp16s)((videoIn->R4erBuffer.pBackward[fPos].x * inData->accuracy) + (videoIn->R4erBuffer.pBhalf[fPos].x * (inData->accuracy >> 1)) + (videoIn->R4erBuffer.pBquarter[fPos].x * (inData->accuracy >> 2)));
            ptr->PDMV[fPos].iMVy = (Ipp16s)((videoIn->R4erBuffer.pBackward[fPos].y * inData->accuracy) + (videoIn->R4erBuffer.pBhalf[fPos].y * (inData->accuracy >> 1)) + (videoIn->R4erBuffer.pBquarter[fPos].y * (inData->accuracy >> 2)));
        }
        //write_mv_array(ptr->PDMV, (ptr->heightInRegionGrid*2)/2, (ptr->widthInRegionGrid*2)/2, 8, ptr->TR, "psmv_me2");
        

        /*---Third stage----*/
        video->average                    =    0;
        for(fPos=0;fPos<inData->R1.fBlocks;fPos++)
        {
            valNoM                        +=    ME_One3(video, fPos,
                inData->R1,
                inData->R1,
                &(videoIn->R1Buffer),
                &(videoRef->R1Buffer),
                &(videoIn->R1Buffer),
                FALSE, backward,
                inData->accuracy);

            ptr->PSAD[fPos]                =    videoIn->R1Buffer.BSAD[fPos];
            valb                        +=    videoIn->R1Buffer.BSAD[fPos];
            ptr->PDMV[fPos].iMVx = (Ipp16s)((videoIn->R1Buffer.pBackward[fPos].x * inData->accuracy) + (videoIn->R1Buffer.pBhalf[fPos].x * (inData->accuracy >> 1)) + (videoIn->R1Buffer.pBquarter[fPos].x * (inData->accuracy >> 2)));
            ptr->PDMV[fPos].iMVy = (Ipp16s)((videoIn->R1Buffer.pBackward[fPos].y * inData->accuracy) + (videoIn->R1Buffer.pBhalf[fPos].y * (inData->accuracy >> 1)) + (videoIn->R1Buffer.pBquarter[fPos].y * (inData->accuracy >> 2)));
        }
        //write_mv_array(ptr->PDMV, (ptr->heightInRegionGrid*2), (ptr->widthInRegionGrid*2), 8, ptr->TR, "psmv_me3");

        TSC                                =    (Ipp64f)valb/(Ipp64f)inData->R1.Yframe;
        AFD                                =    (Ipp64f)valNoM/(Ipp64f)inData->R1.Yframe;
        video->logic[PdIndex].frameNum    =    ptr->TR;
        video->logic[PdIndex].Rs        =    RsGlobal;
        video->logic[PdIndex].Cs        =    CsGlobal;
        video->logic[PdIndex].SC        =    SC;
        video->logic[PdIndex].TSC        =    TSC;
        video->logic[PdIndex].AFD        =    AFD;
        video->logic[PdIndex].ptr        =    ptr;

        tscVal = 0;
        for(tbPos = 0;tbPos<NumTSC;tbPos++)
        {
            if(TSC < lmt_tsc2[tbPos])
            {
                tscVal                    =    tbPos;
                break;
            }
        }
        video->logic[PdIndex].TSCindex    =    tscVal;

        scVal = 0;
        for(tbPos = 0;tbPos<NumSC;tbPos++)
        {
            if(SC < lmt_sc2[tbPos])
            {
                scVal                    =    tbPos;
                break;
            }
        }
        video->logic[PdIndex].SCindex    =    scVal;

        ptr->SCindex = video->logic[PdIndex].SCindex;
        ptr->TSCindex = video->logic[PdIndex].TSCindex;
        ptr->SC = sqrt(((RsGlobal*RsGlobal) + (CsGlobal*CsGlobal))*inData->R1.Yframe); //video->logic[PdIndex].SC; 
        ptr->TSC = (Ipp64f)valb; //video->logic[PdIndex].TSC;

        /*------Shot Detection------*/
        SceneDetect(videoIn->R1Buffer, videoRef->R1Buffer, inData->R1, scVal, TSC, AFD, &Schange, &Gchange);

        //Schange                    =    ShotDetect(video->logic, PdIndex);

        video->logic[PdIndex].Schg        =    Schange;
        video->logic[PdIndex].Gchg        =    Gchange;

        pdVal                            =    video->PDistanceTable[(tscVal*NumSC) + scVal];
        if(inData->pdistCorrect)
            pdVal                        +=    inData->pdistCorrect;
        if(pdVal < inData->minpdist)
            pdVal                        =    inData->minpdist;
        if(inData->bForcePDist)
            pdVal                        =    inData->uForcePDistVal;
        if(inData->bNoSceneDetect) {
            Schange                     = 0;
            Gchange                     = 0;
        }

        //LogMessage(generic_debug, LOG_LVL_VIDEO, "ME PDist Logic:\t%i,%f,%f,%f,%i,%f,%f,%i,%i", video->logic[PdIndex].frameNum, video->logic[PdIndex].Rs, video->logic[PdIndex].Cs, video->logic[PdIndex].SC, video->logic[PdIndex].SCindex, video->logic[PdIndex].AFD, video->logic[PdIndex].TSC, video->logic[PdIndex].TSCindex, pdVal);

        if(keyFrame)
        {
            video->logic[PdIndex].pdist    =    0;
            video->logic[PdIndex].Schg    =    0;
            video->logic[PdIndex].Gchg    =    0;
        }
        else if(valb == 0)
        {
            video->logic[PdIndex].repeatedFrame    =    TRUE;
            video->logic[PdIndex].pdist            =    2;
        }
        else
        {
            if(Schange)
            {
                // Seperate Setting to disable scene cuts
                video->logic[PdIndex].pdist    =    0;
                video->logic[PdIndex].Gchg    =    FALSE;
            }
            else if(Gchange)
            {
                if(!inData->bForcePDist)
                    video->logic[PdIndex].pdist =   2;
                else 
                    video->logic[PdIndex].pdist =   pdVal;
            }
            else                    {
                video->logic[PdIndex].pdist     =   pdVal;
            }
        }
        ptr->Schg = video->logic[PdIndex].Schg; 
        ptr->pdist = video->logic[PdIndex].pdist;
        //LogMessage(generic_debug, LOG_LVL_VIDEO, "Post ME PDist %d", video->logic[PdIndex].pdist);
    }
}

void processPDMEFrame(VidRead *video, VidData *inData, VidSample *videoIn, VidSample *videoRef, RFrame * /*ptr*/, Ipp32u *PDSAD, T_ECORE_MV *PDMV)
{
    Ipp32u        valNoM, valb, fPos;

    {
        /*--Motion Estimation--*/
        valNoM                      =   0;
        valb                        =   0;
        /*---First stage----*/
        for(fPos=0;fPos<inData->R16th.fBlocks;fPos++)   {
            ME_One(    video, fPos,
                inData->R16th,
                inData->R4er,
                &(videoIn->R16thBuffer),
                &(videoRef->R16thBuffer),
                &(videoIn->R4erBuffer),
                TRUE, backward,
                inData->accuracy, 0, 1);
            PDMV[fPos].iMVx = (Ipp16s)((videoIn->R16thBuffer.pBackward[fPos].x * inData->accuracy) + (videoIn->R16thBuffer.pBhalf[fPos].x * (inData->accuracy >> 1)) + (videoIn->R16thBuffer.pBquarter[fPos].x * (inData->accuracy >> 2)));
            PDMV[fPos].iMVy = (Ipp16s)((videoIn->R16thBuffer.pBackward[fPos].y * inData->accuracy) + (videoIn->R16thBuffer.pBhalf[fPos].y * (inData->accuracy >> 1)) + (videoIn->R16thBuffer.pBquarter[fPos].y * (inData->accuracy >> 2)));
        }

        /*---Second stage---*/
        for(fPos=0;fPos<inData->R4er.fBlocks;fPos++)    {
            ME_One(    video, fPos,
                inData->R4er,
                inData->R1,
                &(videoIn->R4erBuffer),
                &(videoRef->R4erBuffer),
                &(videoIn->R1Buffer),
                FALSE, backward,
                inData->accuracy, 1, 1);
            /// NGVv326 Fix Thresh for extended range -N
            PDMV[fPos].iMVx = (Ipp16s)((videoIn->R4erBuffer.pBackward[fPos].x * inData->accuracy) + (videoIn->R4erBuffer.pBhalf[fPos].x * (inData->accuracy >> 1)) + (videoIn->R4erBuffer.pBquarter[fPos].x * (inData->accuracy >> 2)));
            PDMV[fPos].iMVy = (Ipp16s)((videoIn->R4erBuffer.pBackward[fPos].y * inData->accuracy) + (videoIn->R4erBuffer.pBhalf[fPos].y * (inData->accuracy >> 1)) + (videoIn->R4erBuffer.pBquarter[fPos].y * (inData->accuracy >> 2)));
        }
        /*---Third stage----*/
        for(fPos=0;fPos<inData->R1.fBlocks;fPos++)      {
            valNoM    +=    ME_One3(video, fPos,
                inData->R1,
                inData->R1,
                &(videoIn->R1Buffer),
                &(videoRef->R1Buffer),
                &(videoIn->R1Buffer),
                FALSE, backward,
                inData->accuracy);

            PDSAD[fPos] = videoIn->R1Buffer.BSAD[fPos];
            valb            += videoIn->R1Buffer.BSAD[fPos];
            PDMV[fPos].iMVx = (Ipp16s)((videoIn->R1Buffer.pBackward[fPos].x * inData->accuracy) + (videoIn->R1Buffer.pBhalf[fPos].x * (inData->accuracy >> 1)) + (videoIn->R1Buffer.pBquarter[fPos].x * (inData->accuracy >> 2)));
            PDMV[fPos].iMVy = (Ipp16s)((videoIn->R1Buffer.pBackward[fPos].y * inData->accuracy) + (videoIn->R1Buffer.pBhalf[fPos].y * (inData->accuracy >> 1)) + (videoIn->R1Buffer.pBquarter[fPos].y * (inData->accuracy >> 2)));
        }
        /*
        TSC =   (Ipp64f)valb/(Ipp64f)inData->R1.Yframe;
        tscVal = 0;
        for(tbPos = 0;tbPos<NumTSC;tbPos++)
        {
            if(TSC < lmt_tsc2[tbPos])
            {
                tscVal                    =    tbPos;
                break;
            }
        }
        */
    }
}

#if DUMP_PDIST
void printStats2(TSCstat *data, Ipp32s fps, DataWriter    *SADOut)    {
    if (!data->frameNum)
    {
        printf("FrameNum ");
        printf("      Rs ");
        printf("      Cs ");
        printf("      SC ");
        printf("     AFD ");
        printf("     TSC ");
        printf("TSCid ");
        printf("SCid ");

        printf("Schg ");
        printf("Gchg ");
        printf("pdist ");
        printf("picType ");
        printf("picRank\n"); 
        if(SADOut->pDataOut)
        {
            fprintf(SADOut->pDataOut,"FrameNum ");
            fprintf(SADOut->pDataOut,"      Rs ");
            fprintf(SADOut->pDataOut,"      Cs ");
            fprintf(SADOut->pDataOut,"      SC ");
            fprintf(SADOut->pDataOut,"     AFD ");
            fprintf(SADOut->pDataOut,"     TSC ");
            fprintf(SADOut->pDataOut,"TSCid ");
            fprintf(SADOut->pDataOut,"SCid ");


            fprintf(SADOut->pDataOut,"Schg ");
            fprintf(SADOut->pDataOut,"Gchg ");
            fprintf(SADOut->pDataOut,"pdist ");
            fprintf(SADOut->pDataOut,"picType ");
            fprintf(SADOut->pDataOut,"picRank\n");
        }
    }
    printf("%8d ",data->frameNum);
    printf("%8.3f ",data->Rs);
    printf("%8.3f ",data->Cs);
    printf("%8.3f ",data->SC);
    printf("%8.3f ",data->AFD);
    printf("%8.3f ",data->TSC);
    printf("%5d ",data->TSCindex);
    printf("%4d ",data->SCindex);

    printf("%4d ",data->Schg);
    printf("%4d ",data->Gchg);
    printf("%5d ",data->ptr->pdist);
    if(data->repeatedFrame)    {
        printf("*",data->ptr->pdist);
        data->repeatedFrame = FALSE;
    }
    if(fps > 30 && (data->pdist <= 3) && (data->pdist > 0))
        printf("+",data->ptr->pdist);
    printf("%7d ",data->ptr->ePicType);
    printf("%7d\n",data->ptr->uPicRank);

    if(SADOut->pDataOut)
    {
        fprintf(SADOut->pDataOut,"%8d ",data->frameNum);
        fprintf(SADOut->pDataOut,"%8.3f ",data->Rs);
        fprintf(SADOut->pDataOut,"%8.3f ",data->Cs);
        fprintf(SADOut->pDataOut,"%8.3f ",data->SC);
        fprintf(SADOut->pDataOut,"%8.3f ",data->AFD);
        fprintf(SADOut->pDataOut,"%8.3f ",data->TSC);
        fprintf(SADOut->pDataOut,"%5d ",data->TSCindex);
        fprintf(SADOut->pDataOut,"%4d ",data->SCindex);

        fprintf(SADOut->pDataOut,"%4d ",data->Schg);
        fprintf(SADOut->pDataOut,"%4d ",data->Gchg);

        fprintf(SADOut->pDataOut,"%5d ",data->ptr->pdist);
        if(data->repeatedFrame)    {
            fprintf(SADOut->pDataOut,"*",data->ptr->pdist);
            data->repeatedFrame = FALSE;
        }
        if(fps > 30 && (data->ptr->pdist <= 3) && (data->pdist > 0))
            fprintf(SADOut->pDataOut,"+",data->ptr->pdist);

        fprintf(SADOut->pDataOut,"%7d ",data->ptr->ePicType);
        fprintf(SADOut->pDataOut,"%7d\n",data->ptr->uPicRank);
    }
}
#endif

NGV_Bool DetermineFrameTypes(NGV_PDist *t, VidRead *videoIn, VidData * /*inData*/, NGV_Bool lastFrame, RFrame * /*ptr*/)
{
    int iPdIndex=0, imaxPdIndex=t->maxpdist+1,dcandidate;
    Ipp32s        pointer;
    Ipp32s        i, fgroup;

    NGV_Bool PdistanceDetermined = FALSE; 

    imaxPdIndex = t->PdIndex-t->PdIndexL;
    dcandidate = videoIn->logic[t->PdIndexL].pdist;

    for(iPdIndex=0; iPdIndex<imaxPdIndex && !PdistanceDetermined; iPdIndex++)
    {
        pointer        =    videoIn->logic[t->PdIndexL+iPdIndex].pdist;
        if(pointer == 0)    {            /*I frame found*/
            if(iPdIndex)
            {
                fgroup =    iPdIndex - 1;
                if(videoIn->logic[t->PdIndexL+iPdIndex].Schg || t->bClosedGOP)    {    /*I frame because of shot change*/ 
                    for(i=0;i<=fgroup-1;i++){
                        videoIn->logic[t->PdIndexL+i].picType    =    RV_FPIC;
                    }
                    videoIn->logic[t->PdIndexL+fgroup].picType    =    RV_PPIC;
                }
                else                        {
                    for(i=0;i<=fgroup;i++)    {
                        videoIn->logic[t->PdIndexL+i].picType    =    RV_FPIC;
                    }
                }
            }
            videoIn->logic[t->PdIndexL+iPdIndex].picType = RV_IPIC;
            PdistanceDetermined            =    TRUE;
            t->IPframeFound                =    TRUE;
            t->IPFrame                    =    iPdIndex;
        }            
        else if(iPdIndex == (dcandidate-1))        {
            if(pointer >= dcandidate)        {
                fgroup                    =    iPdIndex;
                for(i=0;i<=fgroup-1;i++){
                    videoIn->logic[t->PdIndexL+i].picType    =    RV_FPIC;
                }
                videoIn->logic[t->PdIndexL+fgroup].picType    =    RV_PPIC;
                t->IPframeFound            =    TRUE;
                t->IPFrame                =    fgroup;
                PdistanceDetermined        =    TRUE;
            }
            else                            {
                fgroup                    =    pointer-1;
                for(i=0;i<=fgroup-1;i++){
                    videoIn->logic[t->PdIndexL+i].picType    =    RV_FPIC;
                }
                videoIn->logic[t->PdIndexL+fgroup].picType    =    RV_PPIC;
                PdistanceDetermined        =    TRUE;
                t->IPframeFound            =    TRUE;
                t->IPFrame                =    fgroup;
            }
        }    
        else if(iPdIndex > (dcandidate-1))    {
            fgroup                    =    dcandidate-1;
            for(i=0;i<=fgroup-1;i++){
                videoIn->logic[t->PdIndexL+i].picType        =    RV_FPIC;
            }
            videoIn->logic[t->PdIndexL+fgroup].picType        =    RV_PPIC;
            PdistanceDetermined        =    TRUE;
            t->IPframeFound            =    TRUE;
            t->IPFrame                =    fgroup;
        }
    }
    if(dcandidate>=imaxPdIndex && !PdistanceDetermined) 
    {
        VM_ASSERT(lastFrame);
        fgroup                    =    MAX(0,imaxPdIndex);
        for(i=0;i<=fgroup-1;i++){
            videoIn->logic[t->PdIndexL+i].picType        =    RV_FPIC;
        }
        videoIn->logic[t->PdIndexL+fgroup].picType        =    RV_PPIC;
        PdistanceDetermined        =    TRUE;
        t->IPframeFound            =    TRUE;
        t->IPFrame                =    fgroup;
    }

    return PdistanceDetermined;
}

static RFrame* past_reference_frame_epic(RFrame *frame)
{
    RFrame *frm = frame->past_frame;
    while(frm)
    {
        if(frm->ePicType == RV_IPIC || frm->ePicType == RV_PPIC || (frm->ePicType == RV_FPIC && frm->uPicRank<MAX_RANK)) break;
        frm = frm->past_frame;
    }
    return frm;
}



void NGV_PDME_Frame(NGV_PDist *t, RFrame *in, RFrame *ref, Ipp32u *PDSAD, T_ECORE_MV *PDMV)
{
    VidRead *video  = (VidRead *)t->video;
    VidData *inData     = (VidData *)t->inData;
    //FUNCTION_TIMER_START
    processPDMEFrame(video, inData, (VidSample *) in->sample, (VidSample *)  ref->sample, in, PDSAD, PDMV);
    //FUNCTION_TIMER_END
    //write_mv_array(PDMV, in->heightInRegionGrid*2, in->widthInRegionGrid*2, 8, in->TR, "pdmv");
}

NGV_Bool NGV_PDist_Determine(NGV_PDist *t,
                            RFrame *in,
                            NGV_Bool firstFrame,
                            NGV_Bool keyFrame,
                            NGV_Bool lastFrame
                            )
{
    NGV_Bool failed  =   FALSE;
    VidRead *video  = (VidRead *)t->video;
    VidSample *sampleRef = NULL, *sampleIn=NULL;
    VidData *inData     = (VidData *)t->inData;
    Ipp32s prevNotSet      =   1;
    RFrame *ptr, *ref;
    int i;

    if(!lastFrame) 
    {
        t->PdIndex++;
        if(in) {
#ifdef PD_LOCAL_POOL
#if 0//MEM_OPT//USE_PDMV
            if(t->samplePoolSize<=2/*(t->maxpdist*8+1+t->lookahead)*/)    // Next ref av lookahead for (projection)
#else
            if(t->samplePoolSize<=(t->maxpdist*(REFMOD_MAX+1)+1+t->lookahead))    // Next ref av lookahead for (projection)
#endif
            {
                void *sample;
                sample = t->samplePool[t->samplePoolSize] = NgvMalloc(sizeof(VidSample));
                if (!sample)
                    return TRUE;
                VidSampleInit(sample);
                VidSampleAlloc(t, sample, inData);
                sampleIn = in->sample = sample;
                t->samplePoolSize++;
                LogMessage(generic_debug, LOG_LVL_VIDEO, "samplePool Increased to = %d\n", t->samplePoolSize);
            }
            if(t->samplePoolIndex >= t->samplePoolSize)
                t->samplePoolIndex = 0;
            sampleIn = in->sample = t->samplePool[t->samplePoolIndex];
            t->samplePoolIndex++;
#else
            if(in->sample) {
                sampleIn = (VidSample*)in->sample;
            } else {
                in->sample = NgvMalloc(sizeof(VidSample));
                if (!in->sample)
                    return TRUE;
                VidSampleInit((VidSample*)in->sample);
                VidSampleAlloc(t, (VidSample*)in->sample, inData);
                sampleIn = (VidSample*)in->sample;
            }
#endif
            if (t->lookahead) in->latency = 0; 
            if(in->past_frame) {
                if(in->past_frame->sample) {
                    sampleRef = (VidSample*)in->past_frame->sample;
                } else if(!firstFrame) {
#ifdef PD_LOCAL_POOL
                    VM_ASSERT(0);  // Memory Leak
#endif
                    in->past_frame->sample = NgvMalloc(sizeof(VidSample));
                    // Error checking
                    if (in->past_frame->sample == NULL)
                    {
                        VM_ASSERT(0);
                        NgvPrintf("Error NGV_PDist_Determine: at line number %d in file %s\n", __LINE__, __FILE__);
                        return TRUE;  // Error detected need to exit
                    }
                    VidSampleInit((VidSample*)in->past_frame->sample);
                    VidSampleAlloc(t, (VidSample*)in->past_frame->sample, inData);
                    sampleRef = (VidSample*)in->past_frame->sample;
                }
            }
        }

        if (in == NULL)
        {
            VM_ASSERT(0);
            NgvPrintf("Error NGV_PDist_Determine: at line number %d in file %s\n", __LINE__, __FILE__);
            return TRUE;  // Error detected need to exit
        }
        PdReduceImage(video, inData, &in->image, sampleIn);
        processMEFrame(video, inData, sampleIn, sampleRef, t->PdIndex, firstFrame, keyFrame, in);

        t->TR= in->TR;
        t->unmodTR++;
        if(!firstFrame)
        {
            //LogToFile(generic_debug, LOG_LVL_FRAME, "psad_map.txt", "a", "PSAD for Frame %d", in->TR);
            //LogToFile(generic_debug_int_array_2d, LOG_LVL_FRAME, "psad_map.txt", "a", in->heightInRegionGrid*2, in->widthInRegionGrid*2, in->widthInRegionGrid*2, in->PSAD);
        }
    }
    if(lastFrame) {
        Ipp32u fPos;
        in->ePicType = RV_NPIC;
        in->uPicRank = 0;
        in->uPicMod  = 0;
        for(fPos=0;fPos<inData->R1.fBlocks;fPos++)
        {
            in->PSAD[fPos]                =    INT_MAX;
        }
    } else {
        in->ePicType = RV_DUMY;
        in->uPicRank = 0;
        in->uPicMod  = 0;
    }
    t->IPframeFound                 =   FALSE;
    if((t->PdIndex - t->PdIndexL)>=t->maxpdist || lastFrame)
    {
        t->IPframeFound = DetermineFrameTypes(t, video, inData, lastFrame, in);

        if(t->IPframeFound)
        {
            if(lastFrame)
            {
                ptr = in;
                prevNotSet=0;
                while(ptr->past_frame && (ptr->past_frame->ePicType == RV_DUMY || ptr->past_frame->ePicType == RV_NPIC))
                {
                    if(ptr->past_frame->ePicType == RV_DUMY) prevNotSet++;
                    ptr = ptr->past_frame;
                }
                ref = (ptr)?ptr->past_frame:NULL;
                // Error checking. If ptr is null do not want to dereference in for loop below
                if (ptr == NULL)
                {
                    VM_ASSERT(0);
                    NgvPrintf("Error NGV_PDist_Determine: at line number %d in file %s\n", __LINE__, __FILE__);
                    return TRUE; // failed
                }

                for(i=0;i<=t->IPFrame;i++)    {
                    ptr->ePicType = video->logic[t->PdIndexL+i].picType;
                    ptr->npTR = t->TR;
                    if(ptr->ePicType == RV_PPIC && ref)
                    {
                        int refmod = t->uRefModStart;            // Use RefMod
                        if(t->uRefMod) {
                            int RefMod1 = t->uRefMod+1;
                            int RefMod0 = 0;
                            int d = 0;
                            {
                                RFrame *past_ref = ptr;
                                do {
                                    refmod++;
                                    past_ref = past_reference_frame_epic(past_ref);
                                } while(past_ref && past_ref->uPicMod!=0);
                                if(refmod<=(int)t->uRefMod)                              ptr->uPicMod = 1;
                                else                                                ptr->uPicMod = 0;
                            }
                            if(ptr->uPicMod) {
                                do {
                                    d++;
                                    if(refmod==RefMod0+(RefMod1-RefMod0)/2) {
                                        break;
                                    } else if(refmod<RefMod0+(RefMod1-RefMod0)/2) {
                                        RefMod1 = RefMod0 + (RefMod1-RefMod0)/2;
                                    } else {
                                        RefMod0 = RefMod0 + (RefMod1-RefMod0)/2;
                                    }
                                } while((RefMod1-RefMod0)>1);
                                if(((t->PdIndex-t->PdIndexL) - t->IPFrame -1)>=0)   ptr->uPicMod = d;
                                else                                                ptr->uPicMod = 0;
                                if(ptr->uPicMod>=1) {
                                    if(ptr->uPicMod>=t->uRefRankMax)   ptr->uPicMod=MAX_RANK;
                                    else if(ptr->uPicMod==t->uRefRankMax-1) ptr->uPicMod=MAX_RANK-1;
                                    else                               ptr->uPicMod=MAX_RANK-2;
                                }
                            } else {
                                if(!t->bRankOrder && ((t->PdIndex-t->PdIndexL) - t->IPFrame -1)<0)   ptr->uPicMod = 1;  // In low delay we can have last frame bpictype 1
                            }
                        } else                                                      ptr->uPicMod = 0;
                        if(t->bRankOrder && ((t->PdIndex-t->PdIndexL) - t->IPFrame -1)>=0) {
                            ptr->uPicRank = ptr->uPicMod;
                            if(t->bModToF && ptr->uPicRank>=t->uRankToF)  ptr->ePicType = RV_FPIC;
                            // Adaptive Mod To F 
                            if(t->bAutoModToF) {
                                if(t->bPartialInOrder) {
                                    if(t->bModToF && ptr->uPicRank==1 && ptr->ePicType == RV_PPIC &&  video->PDistanceTable[(video->logic[t->PdIndexL+i].TSCindex*NumSC) + video->logic[t->PdIndexL+i].SCindex]>=3) ptr->ePicType = RV_FPIC;
                                    if(t->bModToF && ptr->uPicRank==1 && ptr->ePicType == RV_PPIC && (video->PDistanceTable[(video->logic[t->PdIndexL+i].TSCindex*NumSC) + video->logic[t->PdIndexL+i].SCindex]<=1)) ptr->uPicRank = 0;
                                    if(t->bModToF && ptr->uPicRank==2 && ptr->ePicType == RV_PPIC &&  video->PDistanceTable[(video->logic[t->PdIndexL+i].TSCindex*NumSC) + video->logic[t->PdIndexL+i].SCindex]>=2) ptr->ePicType = RV_FPIC;
                                    if(t->bModToF && ptr->uPicRank==2 && ptr->ePicType == RV_PPIC && (video->PDistanceTable[(video->logic[t->PdIndexL+i].TSCindex*NumSC) + video->logic[t->PdIndexL+i].SCindex]<=1)) ptr->uPicRank = 0;
                                } else {
                                    if(t->bModToF && ptr->uPicRank==1 && ptr->ePicType == RV_PPIC &&  video->PDistanceTable[(video->logic[t->PdIndexL+i].TSCindex*NumSC) + video->logic[t->PdIndexL+i].SCindex]>=3) ptr->ePicType = RV_FPIC;
                                    if(t->bModToF && ptr->uPicRank==1 && ptr->ePicType == RV_PPIC && (video->PDistanceTable[(video->logic[t->PdIndexL+i].TSCindex*NumSC) + video->logic[t->PdIndexL+i].SCindex]<=2)) ptr->uPicRank = 0;
                                    if(t->bModToF && ptr->uPicRank==2 && ptr->ePicType == RV_PPIC &&  video->PDistanceTable[(video->logic[t->PdIndexL+i].TSCindex*NumSC) + video->logic[t->PdIndexL+i].SCindex]>=2) ptr->ePicType = RV_FPIC;
                                    if(t->bModToF && ptr->uPicRank==2 && ptr->ePicType == RV_PPIC && (video->PDistanceTable[(video->logic[t->PdIndexL+i].TSCindex*NumSC) + video->logic[t->PdIndexL+i].SCindex]<=1)) ptr->uPicRank = 0;
                                }
                            }
                        }
                    } else if(ptr->ePicType == RV_FPIC) {
                        ptr->uPicMod = ptr->uPicRank = MAX_RANK;
                    } else if(ptr->ePicType == RV_IPIC) {
                        if(ptr->past_frame) {
                            if(ptr->past_frame->ePicType == RV_PPIC) {
                                ptr->past_frame->uPicRank = 0;
                            } else if(ptr->past_frame->ePicType == RV_FPIC && ptr->past_frame->uPicRank<MAX_RANK) {
                                ptr->past_frame->ePicType = RV_PPIC;
                                ptr->past_frame->uPicRank = 0;
                            } else {
                                if(t->bClosedGOP) VM_ASSERT(ptr->past_frame->ePicType!= RV_FPIC);
                            }
                        }
                    }
                    ptr = ptr->futr_frame;
                }
            } else {
                ptr = in;
                prevNotSet=1;
                while(ptr->past_frame && ptr->past_frame->ePicType == RV_DUMY)
                {
                    prevNotSet++;
                    ptr = ptr->past_frame;
                }
                ref = (ptr)?ptr->past_frame:NULL;
                for(i=0;i<=t->IPFrame;i++)    {
                    ptr->ePicType = video->logic[t->PdIndexL+i].picType;
                    ptr->npTR = t->TR;
                    if(ptr->ePicType == RV_PPIC && ref)
                    {
                        int refmod = t->uRefModStart;            // Use RefMod
                        if(t->uRefMod) {
                            int RefMod1 = t->uRefMod+1;
                            int RefMod0 = 0;
                            int d = 0;
                            {
                                RFrame *past_ref = ptr;
                                do {
                                    past_ref = past_reference_frame_epic(past_ref);
                                    refmod++;
                                } while(past_ref && past_ref->uPicMod!=0);
                                if(refmod<=(int)t->uRefMod)                              ptr->uPicMod = 1;
                                else                                                ptr->uPicMod = 0;
                            }
                            do {
                                d++;
                                if(refmod==RefMod0+(RefMod1-RefMod0)/2) {
                                    break;
                                } else if(refmod<RefMod0+(RefMod1-RefMod0)/2) {
                                    RefMod1 = RefMod0 + (RefMod1-RefMod0)/2;
                                } else {
                                    RefMod0 = RefMod0 + (RefMod1-RefMod0)/2;
                                }
                            } while((RefMod1-RefMod0)>1);
                            if(ptr->uPicMod)                                       ptr->uPicMod = d;
                            if(ptr->uPicMod>=1) {
                                if(ptr->uPicMod>=t->uRefRankMax)   ptr->uPicMod=MAX_RANK;
                                else if(ptr->uPicMod==t->uRefRankMax-1) ptr->uPicMod=MAX_RANK-1;
                                else                               ptr->uPicMod=MAX_RANK-2;
                            }
                        } else                                                     ptr->uPicMod = 0;
                        if(t->bRankOrder) {
                            ptr->uPicRank = ptr->uPicMod;
                            if(t->bModToF && ptr->uPicRank>=t->uRankToF)  ptr->ePicType = RV_FPIC;
                            // Adaptive Mod To F 
                            if(t->bAutoModToF) {
                                if(t->bPartialInOrder) {
                                    if(t->bModToF && ptr->uPicRank==1 && ptr->ePicType == RV_PPIC &&  video->PDistanceTable[(video->logic[t->PdIndexL+i].TSCindex*NumSC) + video->logic[t->PdIndexL+i].SCindex]>=3) ptr->ePicType = RV_FPIC;
                                    if(t->bModToF && ptr->uPicRank==1 && ptr->ePicType == RV_PPIC && (video->PDistanceTable[(video->logic[t->PdIndexL+i].TSCindex*NumSC) + video->logic[t->PdIndexL+i].SCindex]<=1)) ptr->uPicRank = 0;
                                    if(t->bModToF && ptr->uPicRank==2 && ptr->ePicType == RV_PPIC &&  video->PDistanceTable[(video->logic[t->PdIndexL+i].TSCindex*NumSC) + video->logic[t->PdIndexL+i].SCindex]>=2) ptr->ePicType = RV_FPIC;
                                    if(t->bModToF && ptr->uPicRank==2 && ptr->ePicType == RV_PPIC && (video->PDistanceTable[(video->logic[t->PdIndexL+i].TSCindex*NumSC) + video->logic[t->PdIndexL+i].SCindex]<=1)) ptr->uPicRank = 0;
                                } else {
                                    if(t->bModToF && ptr->uPicRank==1 && ptr->ePicType == RV_PPIC &&  video->PDistanceTable[(video->logic[t->PdIndexL+i].TSCindex*NumSC) + video->logic[t->PdIndexL+i].SCindex]>=3) ptr->ePicType = RV_FPIC;
                                    if(t->bModToF && ptr->uPicRank==1 && ptr->ePicType == RV_PPIC && (video->PDistanceTable[(video->logic[t->PdIndexL+i].TSCindex*NumSC) + video->logic[t->PdIndexL+i].SCindex]<=2)) ptr->uPicRank = 0;
                                    if(t->bModToF && ptr->uPicRank==2 && ptr->ePicType == RV_PPIC &&  video->PDistanceTable[(video->logic[t->PdIndexL+i].TSCindex*NumSC) + video->logic[t->PdIndexL+i].SCindex]>=2) ptr->ePicType = RV_FPIC;
                                    if(t->bModToF && ptr->uPicRank==2 && ptr->ePicType == RV_PPIC && (video->PDistanceTable[(video->logic[t->PdIndexL+i].TSCindex*NumSC) + video->logic[t->PdIndexL+i].SCindex]<=1)) ptr->uPicRank = 0;
                                }
                            }
                        }
                    } else if(ptr->ePicType == RV_FPIC) {
                        ptr->uPicMod = ptr->uPicRank = MAX_RANK;
                    } else if(ptr->ePicType == RV_IPIC) {
                        if(ptr->past_frame) {
                            if(ptr->past_frame->ePicType == RV_PPIC) {
                                ptr->past_frame->uPicRank = 0;
                            } else if(ptr->past_frame->ePicType == RV_FPIC && ptr->past_frame->uPicRank<MAX_RANK) {
                                ptr->past_frame->ePicType = RV_PPIC;
                                ptr->past_frame->uPicRank = 0;
                            } else {
                                if(t->bClosedGOP) VM_ASSERT(ptr->past_frame->ePicType!= RV_FPIC);
                            }
                        }
                    }
                    ptr = ptr->futr_frame;
                }
            }
#if DUMP_PDIST
            for(i=0;i<=t->IPFrame;i++)    {
                printStats2(&(video->logic[i]),inData->fps, t->SADOut);
            }
#endif
            if (t->lookahead)
            {
                t->PdIndexL += (t->IPFrame+1);
                if ((t->PdIndex > t->lookahead) || lastFrame)
                {
                    t->IPFrame = 0; 
                    while( (video->logic[t->IPFrame].ptr->ePicType<=RV_FPIC) && (video->logic[t->IPFrame].ptr->uPicRank==MAX_RANK) ) t->IPFrame++; 
                    for(i=0;i<=t->IPFrame;i++) 
                        video->logic[i].ptr->latency = 1; 
                    t->PdIndexL -= (t->IPFrame+1);
                }
                else
                    return failed;
            }

            t->remaining = t->PdIndex - t->IPFrame -1;
            for(i=0;i<=t->remaining;i++){
                video->logic[i]  =   video->logic[t->IPFrame + i + 1];
            }
            video->logic[i].pdist = -INT_MAX; 
            t->PdIndex = t->remaining;
        } 
        else
        {
            if (t->remaining>=0)
            {
                t->IPFrame = 0; 
                while( (video->logic[t->IPFrame].ptr->ePicType<=RV_FPIC) && (video->logic[t->IPFrame].ptr->uPicRank==MAX_RANK) ) t->IPFrame++; 
                for(i=0;i<=t->IPFrame;i++)
                    video->logic[i].ptr->latency = 1;
                t->PdIndexL -= (t->IPFrame+1);
                t->remaining = t->PdIndex - t->IPFrame -1;
                for(i=0;i<=t->remaining;i++)
                    video->logic[i]  =   video->logic[t->IPFrame + i + 1];
                video->logic[i].pdist = -INT_MAX; 
                t->PdIndex = t->remaining;
            }
        }
    }
    return failed;
}


typedef enum
{
    RFlat,     // Flat
    RMixd,     // Mixed
    RStrt,     // Strong Texture
    RNUM
} RClass;

static void AddCandidate(T_ECORE_MV *pMV, int *uMVnum, T_ECORE_MV *MVCandidate) 
{
    int i;
    MVCandidate[*uMVnum] = *pMV;
    for (i = 0; i < *uMVnum; i ++)
    {        
        if (MVCandidate[*uMVnum].iMVx == MVCandidate[i].iMVx &&
            MVCandidate[*uMVnum].iMVy == MVCandidate[i].iMVy)
            break;
    }
    /* Test if Last Predictor vector is valid */
    if (i == *uMVnum)
    {
        (*uMVnum)++;
    }
}

void IPicDetermineQpMap(NGV_PDist *np, RFrame *inFrame)
{
    int row, col;
    int c8_width, c8_height;
    int wBlock = inFrame->widthInRegionGrid*2;
    int w4 = wBlock*2;
    int heightInTiles = inFrame->heightInTiles;
    int widthInTiles = inFrame->widthInTiles;
    int rbw = MAX_TILE_SIZE/8;
    int rbh = MAX_TILE_SIZE/8;
    int r4w = MAX_TILE_SIZE/4;
    int r4h = MAX_TILE_SIZE/4;

    double tdiv;
    int M,N, mbT,nbT, m4T, n4T;
    RFrame *ptr;
    VidSample *sample = (VidSample *) (inFrame->sample);
    int count[MAXPDIST*(REFMOD_MAX+1)]={0};
    //int max_count=-1, best_qp=0;//, all_count=0;
    static double lmt_sc2[10]   =   {4.0, 9.0, 15.0, 23.0, 32.0, 42.0, 53.0, 65.0, 78, INT_MAX}; // lower limit of SFM(Rs,Cs) range for spatial classification
    T_ECORE_MV candMVs[(MAX_TILE_SIZE/8)*(MAX_TILE_SIZE/8)];
    
    double tsc_RTF, tsc_RTML, tsc_RTMG, tsc_RTS;
    double futr_qp = 6.0;

    if(((VidData*)(np->inData))->Key<2*((VidData*)(np->inData))->fps) futr_qp = 4.0;
    c8_height = (inFrame->oheight + 8-1)/8;
    c8_height *= 8;
    c8_width  = (inFrame->owidth + 8-1)/8;
    c8_width  *= 8;

    for(row=0;row<heightInTiles;row++) {
        for(col=0;col<widthInTiles;col++) {
            int i,j;
            double SC, Rs=0.0, Cs=0.0;
            int scVal = 0;//, tcVal=0;
            {
                N = (col==widthInTiles-1)?(c8_width-(widthInTiles-1)*MAX_TILE_SIZE):MAX_TILE_SIZE;
                M = (row==heightInTiles-1)?(c8_height-(heightInTiles-1)*MAX_TILE_SIZE):MAX_TILE_SIZE;
            }
            mbT = M/8;
            nbT = N/8;
            m4T = M/4;
            n4T = N/4;
            tdiv = M*N;

            for(i=0;i<m4T;i++) {
                for(j=0;j<n4T;j++) {
                    Rs += (sample->R1Buffer.Rs[(row*r4h+i)*w4+(col*r4w+j)]*sample->R1Buffer.Rs[(row*r4h+i)*w4+(col*r4w+j)]);
                    Cs += (sample->R1Buffer.Cs[(row*r4h+i)*w4+(col*r4w+j)]*sample->R1Buffer.Cs[(row*r4h+i)*w4+(col*r4w+j)]);
                }
            }
            Rs/=(m4T*n4T);
            Cs/=(m4T*n4T);
            SC = sqrt(Rs + Cs);
            for(i = 0;i<10;i++) {
                if(SC < lmt_sc2[i]) {
                    scVal   =   i;
                    break;
                }
            }
            inFrame->sc_mask[row*widthInTiles+col] = scVal;
            tsc_RTML = 0.6*sqrt(SC);
            
            tsc_RTF = tsc_RTML/2;
            tsc_RTMG= IPP_MIN(2*tsc_RTML, SC/1.414);
            tsc_RTS = IPP_MIN(3*tsc_RTML, SC/1.414);
            {
                int coloc=0;
                double pdsad_futr=0.0;
                ptr=inFrame;

                if(np->TR > (int)ptr->TR) 
                {
                    double psad_futr=0.0;
                    do {
                        int uMVnum = 0;
                        ptr=ptr->futr_frame;
                        psad_futr=0;
                        
                        for(i=0;i<mbT;i++) {
                            for(j=0;j<nbT;j++) {
                                psad_futr += ptr->PSAD[(row*rbh+i)*(wBlock)+(col*rbw+j)];
                                AddCandidate(&ptr->PDMV[(row*rbh+i)*(wBlock)+(col*rbw+j)], &uMVnum, candMVs);
                            }
                        }
                        psad_futr/=tdiv;

                        if(psad_futr<tsc_RTML && uMVnum<(mbT*nbT)/2) 
                        {
                            coloc++;
                        }
                        else 
                        {
                            break;
                        }
                    } while(np->TR > (int)ptr->TR);

                    for(i=0;i<mbT;i++)
                        for(j=0;j<nbT;j++)
                            pdsad_futr += inFrame->FDSAD[(row*rbh+i)*(wBlock)+(col*rbw+j)];

                    pdsad_futr/=tdiv;
                } else {
                    pdsad_futr = tsc_RTS;
                    coloc = 0;
                }

                inFrame->coloc_futr[row*widthInTiles+col] = coloc;

                if(scVal<1 && pdsad_futr<tsc_RTML) {
                    // Visual Quality (Flat area first, because coloc count doesn't work in flat areas)
                    coloc = 1;
                    inFrame->qp_mask[row*widthInTiles+col] = -1*coloc;
                } else if(coloc>=8 && pdsad_futr<tsc_RTML) {
                    // Stable Motion, Propagation & Motion reuse (long term stable hypothesis, 8+=>inf)
                    inFrame->qp_mask[row*widthInTiles+col] = -1*IPP_MIN((int)futr_qp, (int)(((float)coloc/8.0)*futr_qp));
                } else if(coloc>=8 && pdsad_futr<tsc_RTMG) {
                    // Stable Motion, Propagation possible & Motion reuse 
                    inFrame->qp_mask[row*widthInTiles+col] = -1*IPP_MIN((int)futr_qp, (int)(((float)coloc/8.0)*4.0));
                } else if(coloc>1 && pdsad_futr<tsc_RTMG) {
                    // Stable Motion & Motion reuse 
                    inFrame->qp_mask[row*widthInTiles+col] = -1*IPP_MIN(4, (int)(((float)coloc/8.0)*4.0));
                } else if(scVal>=6 && pdsad_futr>tsc_RTS) {
                    // Reduce disproportional cost on high texture and bad motion
                    inFrame->qp_mask[row*widthInTiles+col] = 1;
                    coloc = 0;
                } else {
                    // Default
                    inFrame->qp_mask[row*widthInTiles+col] = 0;
                    coloc = 0;
                }
                count[coloc]++;
            }
        } // col
    }   // row

    inFrame->deltaQpLow  = 1;
    inFrame->deltaQpHigh = 1;
}

void PPicDetermineQpMap(NGV_PDist *np, RFrame *inFrame, RFrame *past_frame)
{
    int row,col;
    int c8_height, c8_width;
    int wBlock = inFrame->widthInRegionGrid*2;
    int w4 = wBlock*2;
    int heightInTiles = inFrame->heightInTiles;
    int widthInTiles = inFrame->widthInTiles;
    int rbw = MAX_TILE_SIZE/8;
    int rbh = MAX_TILE_SIZE/8;
    int r4w = MAX_TILE_SIZE/4;
    int r4h = MAX_TILE_SIZE/4;
    VidSample *sample = (VidSample *) (inFrame->sample);
    int count[MAXPDIST*(REFMOD_MAX+1)]={0};
    //int max_count=-1, best_qp=0;//, all_count=0;
    double tdiv;
    int M,N, mbT,nbT, m4T, n4T;
    RFrame *ptr;
    static double lmt_sc2[10]   =   {4.0, 9.0, 15.0, 23.0, 32.0, 42.0, 53.0, 65.0, 78, INT_MAX};                // lower limit of SFM(Rs,Cs) range for spatial classification
    T_ECORE_MV candMVs[(MAX_TILE_SIZE/8)*(MAX_TILE_SIZE/8)];
    double tsc_RTF, tsc_RTML, tsc_RTMG, tsc_RTS;

    //Ipp32u uPicRank = inFrame->uPicRank;
    double futr_qp = 6.0;
    NGV_Bool futr_key = FALSE;
    if(((VidData*)(np->inData))->Key<2*((VidData*)(np->inData))->fps) {
        futr_qp = 4.0;
        if((np->TR%((VidData*)(np->inData))->Key)==0) futr_key=TRUE;
    }
    c8_height = (inFrame->oheight + 8-1)/8;
    c8_height *= 8;
    c8_width  = (inFrame->owidth + 8-1)/8;
    c8_width  *= 8;

    for(row=0;row<heightInTiles;row++) {
        for(col=0;col<widthInTiles;col++) {
            int i, j;//, type_count[3]={0,0,0};
            double SC, Rs=0.0, Cs=0.0;
            int scVal = 0;//,  tcVal =0;
            {
                N = (col==widthInTiles-1)?(c8_width-(widthInTiles-1)*MAX_TILE_SIZE):MAX_TILE_SIZE;
                M = (row==heightInTiles-1)?(c8_height-(heightInTiles-1)*MAX_TILE_SIZE):MAX_TILE_SIZE;
            }
            mbT = M/8;
            nbT = N/8;
            m4T = M/4;
            n4T = N/4;
            tdiv = M*N;
            for(i=0;i<m4T;i++) {
                for(j=0;j<n4T;j++) {
                    Rs += (sample->R1Buffer.Rs[(row*r4h+i)*w4+(col*r4w+j)]*sample->R1Buffer.Rs[(row*r4h+i)*w4+(col*r4w+j)]);
                    Cs += (sample->R1Buffer.Cs[(row*r4h+i)*w4+(col*r4w+j)]*sample->R1Buffer.Cs[(row*r4h+i)*w4+(col*r4w+j)]);
                }
            }
            Rs/=(m4T*n4T);
            Cs/=(m4T*n4T);
            SC = sqrt(Rs + Cs);
            for(i = 0;i<10;i++) {
                if(SC < lmt_sc2[i]) {
                    scVal   =   i;
                    break;
                }
            }
            inFrame->sc_mask[row*widthInTiles+col] = scVal;
            tsc_RTML = 0.6*sqrt(SC);

            tsc_RTF = tsc_RTML/2;
            tsc_RTMG= IPP_MIN(2*tsc_RTML, SC/1.414);
            tsc_RTS = IPP_MIN(3*tsc_RTML, SC/1.414);

            {
                int coloc=0;
                int coloc_futr=0;
                int coloc_past=0;
                double pdsad_past=0.0;
                double pdsad_futr=0.0;
                
                // RTF RTS logic for P Frame is based on MC Error
                for(i=0;i<mbT;i++)
                    for(j=0;j<nbT;j++)
                        pdsad_past += inFrame->PDSAD[(row*rbh+i)*(wBlock)+(col*rbw+j)];
                pdsad_past/=tdiv;

                for(i=0;i<mbT;i++)
                    for(j=0;j<nbT;j++)
                        pdsad_futr += inFrame->FDSAD[(row*rbh+i)*(wBlock)+(col*rbw+j)];
                pdsad_futr/=tdiv;

                // future
                ptr=inFrame;
                if(np->TR > (int)inFrame->TR) 
                {
                    double psad_futr=0.0;
                    // Test Futr
                    do {
                        int uMVnum = 0;
                        ptr=ptr->futr_frame;
                        psad_futr=0;
                        for(i=0;i<mbT;i++) 
                        {
                            for(j=0;j<nbT;j++) 
                            {
                                psad_futr += ptr->PSAD[(row*rbh+i)*(wBlock)+(col*rbw+j)];
                                AddCandidate(&ptr->PDMV[(row*rbh+i)*(wBlock)+(col*rbw+j)], &uMVnum, candMVs);
                            }
                        }
                        psad_futr/=tdiv;

                        if(psad_futr<tsc_RTML && uMVnum < (mbT*nbT)/2) 
                        {
                            coloc_futr++;
                        }
                        else
                        {
                            break;
                        }
                    } while(ptr->TR!= np->TR);//t->np.TR);  /* future note: unsigned/signed mismatch */
                }// future

                // past
                ptr=inFrame;
                if(inFrame->TR > past_frame->TR) 
                {
                    double psad_past=0.0;
                    // Test Past
                    while(ptr->TR!=past_frame->TR) {
                        int uMVnum = 0; 
                        psad_past=0;
                        for(i=0;i<mbT;i++) 
                        {
                            for(j=0;j<nbT;j++) 
                            {
                                psad_past += ptr->PSAD[(row*rbh+i)*(wBlock)+(col*rbw+j)];
                                AddCandidate(&ptr->PDMV[(row*rbh+i)*(wBlock)+(col*rbw+j)], &uMVnum, candMVs);
                            }
                        }
                        psad_past/=tdiv;

                        if(psad_past < tsc_RTML && uMVnum<(mbT*nbT)/2) 
                        {
                            coloc_past++;
                        }
                        else
                        {
                            break;
                        }
                        ptr=ptr->past_frame;
                    };
                } // past

                inFrame->coloc_past[row*widthInTiles+col] = coloc_past;
                inFrame->coloc_futr[row*widthInTiles+col] = coloc_futr;
                coloc = IPP_MAX(coloc_past, coloc_futr);
                if(futr_key) coloc = coloc_past;

                if(coloc_past>=IPP_MIN(8, ((int)inFrame->TR-(int)past_frame->TR)) && past_frame->coloc_futr[row*widthInTiles+col]>=IPP_MIN(8, ((int)inFrame->TR-(int)past_frame->TR))) {
                    // Stable Motion & P Skip (when GOP is small repeated P QP lowering can change RD Point)
                    // Avoid quantization noise recoding
                    //inFrame->qp_mask[row*widthInTiles+col] = IPP_MIN(0, past_frame->qp_mask[row*widthInTiles+col]+1);
                    inFrame->qp_mask[row*widthInTiles+col] = 0; // Propagate
                } else if(coloc>=8 && coloc==coloc_futr && pdsad_futr<tsc_RTML && !futr_key) {
                    // Stable Motion & Motion reuse 
                    inFrame->qp_mask[row*widthInTiles+col] = -1*IPP_MIN((int)futr_qp, (int)(((float)coloc/8.0)*futr_qp));
                } else if(coloc>=8 && coloc==coloc_futr && pdsad_futr<tsc_RTMG && !futr_key) {
                    // Stable Motion & Motion reuse 
                    inFrame->qp_mask[row*widthInTiles+col] = -1*IPP_MIN((int)futr_qp, (int)(((float)coloc/8.0)*4.0));
                } else if(coloc>1 && coloc==coloc_futr && pdsad_futr<tsc_RTMG && !futr_key) {
                    // Stable Motion & Motion Reuse
                    inFrame->qp_mask[row*widthInTiles+col] = -1*IPP_MIN(4, (int)(((float)coloc/8.0)*4.0));
                    // Possibly propagation
                } else if(coloc>1 && coloc==coloc_past && pdsad_past<tsc_RTMG) {
                    // Stable Motion & Motion Reuse
                    inFrame->qp_mask[row*widthInTiles+col] = -1*IPP_MIN(4, (int)(((float)coloc/8.0)*4.0));
                    // Past Boost probably no propagation since coloc_futr is less than coloc_past
                }  else if(scVal>=6 && pdsad_past>tsc_RTS && coloc==0) {
                    // reduce disproportional cost on high texture and bad motion
                    // use pdsad_past since its coded a in order p frame
                    inFrame->qp_mask[row*widthInTiles+col] = 1;
                    coloc = 0;
                } else {
                   // Default
                    inFrame->qp_mask[row*widthInTiles+col] = 0;
                    coloc = 0;
                }
                count[coloc]++;
            }
        }
    }
    inFrame->deltaQpLow  = 1;
    inFrame->deltaQpHigh = 1;
}

//*********************************************************
//  PAQ UTILS
//*********************************************************

void RFrame_Init(RFrame *t)
{
    YuvImage_Init(&t->padded_image);
    YuvImage_Init(&t->image);
    t->allocated_buffer = NULL;
    t->allocated_buffer_size = 0;
    t->heightInTiles =0;
    t->widthInTiles = 0;
    t->heightInRegionGrid =0;
    t->widthInRegionGrid = 0;
    t->padding=MAX_TILE_SIZE;
    t->prev_frame = NULL;
    t->next_frame= NULL;
    t->past_frame = NULL;
    t->futr_frame= NULL;
    t->TR = (Ipp32u)-1;
    t->coded=0;
    t->ePicType=0;
    t->reference=0;
    t->displayed=0;
    t->latency = 1; 
    t->type = RFrame_Image;
    t->BSAD = NULL;
    t->PSAD = NULL;
    t->PDMV  = NULL;
    t->PDSAD = NULL;
    t->PMV  = NULL;
    t->FDSAD = NULL;
    t->FMV  = NULL;
    t->sample = NULL;
    t->qp_mask = NULL;
    t->sc_mask = NULL;
    t->coloc_past = NULL;
    t->coloc_futr = NULL;
    t->deltaQpLow = 0;
    t->deltaQpHigh = 0;
}

/* Copy image to Enc frame */
NGV_Bool RFrame_Convert_Image(RFrame *t, YuvImage *image)
{
    //Ipp32u i;
    //Ipp8u *ss, *dd;

    //// Y
    //ss = image->y_plane;
    //dd = t->image.y_plane;
    //for (i = 0; i < image->height; i++) {
    //    memcpy(dd,ss, image->width);
    //    dd += t->padded_image.y_pitch;
    //    ss += image->y_pitch;
    //}

    t->image.y_plane = image->y_plane;
    t->image.y_pitch = t->padded_image.y_pitch = image->y_pitch;

    return TRUE;
}

NGV_Bool RFrame_Allocate(RFrame *t, Ipp32u width, Ipp32u height, Ipp32u padding, RFrameType type)
{
    NGV_Bool failed = FALSE;
    Ipp32u corrected_height = width;
    Ipp32u corrected_width = height;    
    Ipp32u padded_height = corrected_height + padding*2;
    Ipp32u padded_width = corrected_width + padding*2;
    Ipp32u pitch = width;
    Ipp32u shift_offset_y =0;
    Ipp32u shift_offset_u =0;
    Ipp32u shift_offset_v =0;

    // Correct it to next tile size
    corrected_height = (height + REGION_GRID_SIZE-1)/REGION_GRID_SIZE;
    corrected_height *= REGION_GRID_SIZE;
    corrected_width  = (width + REGION_GRID_SIZE-1)/REGION_GRID_SIZE;
    corrected_width  *= REGION_GRID_SIZE;

    padded_height = corrected_height + padding*2;
    padded_width = corrected_width + padding*2;

    t->heightInTiles = (corrected_height+MAX_TILE_SIZE-1) / MAX_TILE_SIZE;
    t->widthInTiles  = (corrected_width+MAX_TILE_SIZE-1) / MAX_TILE_SIZE;
    t->heightInRegionGrid = (corrected_height) / REGION_GRID_SIZE;
    t->widthInRegionGrid  = (corrected_width) / REGION_GRID_SIZE;
    t->owidth = width;
    t->oheight = height;
    t->padding = padding;

    // 128 bytes Aligned Pitch
    // is an optimization to allow 128 byte reads in a vector processor
    pitch = ((corrected_width + t->padding*2 + 127) & ~127) | 128;    

    /* allocate only if needed */
    if (t->allocated_buffer && (t->allocated_buffer_size == padded_height * pitch * 3 / 2))
        goto done;  

    t->allocated_buffer_size = padded_height * pitch * 3 / 2;
    /* free if allocated (may be because file dimensions changed?) */
    if(t->allocated_buffer)
        free(t->allocated_buffer);
    t->allocated_buffer = (unsigned char *) malloc(sizeof(unsigned char)*t->allocated_buffer_size);

    if(t->allocated_buffer)
    {                
        shift_offset_y = pitch * t->padding + t->padding;
        shift_offset_u = (pitch * padded_height) + ((pitch/2) * (t->padding/2) + t->padding/2);
        shift_offset_v = (pitch * padded_height) + ((pitch/2) * (padded_height/2)) + ((pitch/2 * t->padding/2) + t->padding/2);        

        YuvImage_SetAllPointers(&t->image, 
            (Ipp8u*)t->allocated_buffer + shift_offset_y,
            (Ipp8u*)t->allocated_buffer + shift_offset_u,
            (Ipp8u*)t->allocated_buffer + shift_offset_v,
            corrected_width,
            corrected_height,
            pitch,
            pitch/2);
        t->image.size = t->allocated_buffer_size;

        YuvImage_SetAllPointers(&t->padded_image, 
            (Ipp8u*)t->allocated_buffer,
            (Ipp8u*)t->allocated_buffer + (pitch * padded_height),
            (Ipp8u*)t->allocated_buffer + (pitch * padded_height) + ((pitch/2) * (padded_height/2)),
            padded_width,
            padded_height,
            pitch,
            pitch/2);       
        t->padded_image.size = t->allocated_buffer_size;                          

        RFrame_Clear(t);

    } else {
        failed = TRUE;
    }
    t->type = type;
    if(t->type==RFrame_Input) 
    {
        // HARD CODED to 8x8
        t->BSAD = (Ipp32u*)malloc(t->heightInRegionGrid*t->widthInRegionGrid*4*sizeof(Ipp32u));
        t->PSAD = (Ipp32u*)malloc(t->heightInRegionGrid*t->widthInRegionGrid*4*sizeof(Ipp32u));
        t->PDMV  = (T_ECORE_MV*)malloc(t->heightInRegionGrid*t->widthInRegionGrid*4*sizeof(T_ECORE_MV));
        t->PDSAD = (Ipp32u*)malloc(t->heightInRegionGrid*t->widthInRegionGrid*4*sizeof(Ipp32u));
        t->PMV  = (T_ECORE_MV*)malloc(t->heightInRegionGrid*t->widthInRegionGrid*4*sizeof(T_ECORE_MV));
        t->FDSAD = (Ipp32u*)malloc(t->heightInRegionGrid*t->widthInRegionGrid*4*sizeof(Ipp32u));
        t->FMV  = (T_ECORE_MV*)malloc(t->heightInRegionGrid*t->widthInRegionGrid*4*sizeof(T_ECORE_MV));
        t->qp_mask = (Ipp32s*)malloc(t->heightInTiles*t->widthInTiles*sizeof(Ipp32s));
        t->coloc_past = (Ipp32s*)malloc(t->heightInTiles*t->widthInTiles*sizeof(Ipp32s));
        t->coloc_futr = (Ipp32s*)malloc(t->heightInTiles*t->widthInTiles*sizeof(Ipp32s));
        t->sc_mask = (Ipp32s*)malloc(t->heightInTiles*t->widthInTiles*sizeof(Ipp32s));
    }

done:
    return failed;
}

void RFrame_Clear(RFrame* t)
{
    // fill bufer with 128?
    memset(t->allocated_buffer, 128, t->allocated_buffer_size);
    // convert image should do the padding and copy?
}

void RFrame_Free(RFrame *t)
{
    if (t->allocated_buffer)
    {
        free(t->allocated_buffer);
        t->allocated_buffer = NULL;
        t->allocated_buffer_size = 0;
        YuvImage_SetPointers(&t->image, NULL, 0,0);
        YuvImage_SetPointers(&t->padded_image, NULL, 0,0);
    }    
    if(t->type==RFrame_Input) 
    {
        if(t->BSAD)
        {
            free(t->BSAD);
            t->BSAD = NULL;
        }
        if(t->PSAD)
        {
            free(t->PSAD);
            t->PSAD = NULL;
        }
        if(t->PDMV) {
            free(t->PDMV);
            t->PDMV = NULL;
        }
        if(t->PDSAD) {
            free(t->PDSAD);
            t->PDSAD = NULL;
        }
        if(t->PMV) {
            free(t->PMV);
            t->PMV = NULL;
        }
        if(t->FDSAD) {
            free(t->FDSAD);
            t->FDSAD = NULL;
        }
        if(t->FMV) {
            free(t->FMV);
            t->FMV = NULL;
        }
        if(t->qp_mask) {
            free(t->qp_mask);
            t->qp_mask = NULL;
        }
        if(t->coloc_past) {
            free(t->coloc_past);
            t->coloc_past = NULL;
        }
        if(t->coloc_futr) {
            free(t->coloc_futr);
            t->coloc_futr = NULL;
        }
        if(t->sc_mask) {
            free(t->sc_mask);
            t->sc_mask = NULL;
        }
    }
}

RFrame * RFrame_Construct()
{
    RFrame * t;
    t = (RFrame*) malloc(sizeof(RFrame));
    if(t)
    {
        RFrame_Init(t);
    }
    return t;
}

RFrame* RFrame_Destroy(RFrame *t)
{
    if(t)
    {
        RFrame_Free(t);
        free(t);
        t= NULL;
    }
    return t;
}

void RFrame_Copy(RFrame *t, RFrame *src)
{
    int j;
    // both frames should be of same dimensions
    /*
    int H = t->image.height;
    int W = t->image.width;
    Ipp8u* dd = t->image.y_plane;
    Ipp8u* ss = src->image.y_plane;
    int yP = t->image.y_pitch;
    int uP = t->image.u_pitch;
    int vP = t->image.v_pitch;
    */

    int H = t->padded_image.height;
    int W = t->padded_image.width;
    Ipp8u* dd = t->padded_image.y_plane;
    Ipp8u* ss = src->padded_image.y_plane;
    int yP = t->padded_image.y_pitch;
    int uP = t->padded_image.u_pitch;
    int vP = t->padded_image.v_pitch;

    for (j = 0; j < H; j++)
    {
        memcpy(dd, ss, W);
        dd += yP;
        ss += yP;
    }
    /*
    dd = t->image.u_plane;
    ss = src->image.u_plane;    
    */
    dd = t->padded_image.u_plane;
    ss = src->padded_image.u_plane;    

    for (j = 0; j < H/2; j++)
    {
        memcpy(dd, ss, W/2);
        dd += uP;
        ss += uP;
    }
    /*
    dd = t->image.v_plane;
    ss = src->image.v_plane;    
    */
    dd = t->padded_image.v_plane;
    ss = src->padded_image.v_plane;    

    for (j = 0; j < H/2; j++)
    {
        memcpy(dd, ss, W/2);
        dd += vP;
        ss += vP;
    }
}

/*
* Bands
*/
NGV_Bool RSubband_Allocate(RSubband *t, Ipp32u width, Ipp32u height, Ipp32u padding)
{   
    NGV_Bool failed=FALSE;
    int band_width;
    int band_height;    
    int corrected_width;
    int corrected_height;
    int pitch;
    int buf_pitch, buf_height;    
    Ipp32u size;

    band_width= width/2;
    band_height= height/2;
    // Correct it to next tile size
    corrected_height = (band_height + MAX_TILE_SIZE-1)/MAX_TILE_SIZE;
    corrected_height *= MAX_TILE_SIZE;
    corrected_width  = (band_width + MAX_TILE_SIZE-1)/MAX_TILE_SIZE;
    corrected_width  *= MAX_TILE_SIZE;

    t->heightInTiles = (corrected_height) / MAX_TILE_SIZE;
    t->widthInTiles  = (corrected_width) / MAX_TILE_SIZE;
    t->heightInRegionGrid = (corrected_height) / REGION_GRID_SIZE;
    t->widthInRegionGrid  = (corrected_width) / REGION_GRID_SIZE;

    t->padding = padding;

    // Is this overkill?
    pitch = ((corrected_width + t->padding*2 + 127) & ~127) | 128;  
    // each band is allocated in the same buffers space 
    // This allows for in place wavelet analysis and synthesis
    buf_pitch =pitch*2;    

    t->bandLL.width = t->bandHL.width = t->bandLH.width = t->bandHH.width = corrected_width;
    t->bandLL.height = t->bandHL.height = t->bandLH.height = t->bandHH.height = corrected_height;
    t->bandLL.y_pitch = t->bandHL.y_pitch = t->bandLH.y_pitch = t->bandHH.y_pitch = buf_pitch;
    t->bandLL.u_pitch = t->bandHL.u_pitch = t->bandLH.u_pitch = t->bandHH.u_pitch = buf_pitch/2;
    t->bandLL.v_pitch = t->bandHL.v_pitch = t->bandLH.v_pitch = t->bandHH.v_pitch = buf_pitch/2;

    t->padded_band_ll.width = t->padded_band_hl.width = t->padded_band_lh.width = t->padded_band_hh.width = corrected_width + t->padding*2;
    t->padded_band_ll.height = t->padded_band_hl.height = t->padded_band_lh.height = t->padded_band_hh.height = corrected_height + t->padding*2;
    t->padded_band_ll.y_pitch = t->padded_band_hl.y_pitch = t->padded_band_lh.y_pitch = t->padded_band_hh.y_pitch = buf_pitch;
    t->padded_band_ll.u_pitch = t->padded_band_hl.u_pitch = t->padded_band_lh.u_pitch = t->padded_band_hh.u_pitch = buf_pitch/2;
    t->padded_band_ll.v_pitch = t->padded_band_hl.v_pitch = t->padded_band_lh.v_pitch = t->padded_band_hh.v_pitch = buf_pitch/2;

    // total height per plane    
    buf_height = t->padded_band_ll.height*2;
    // YUV
    size = buf_height*buf_pitch*3/2;
    /* allocate only if needed */
    if (t->allocated_buffer && (t->allocated_buffer_size == size))
        goto done;

    t->allocated_buffer_size = size;
    /* free if allocated (may be because file dimensions changed?) */
    if(t->allocated_buffer)
        free(t->allocated_buffer);

    t->allocated_buffer = (Ipp16s*) malloc(sizeof(Ipp16s)*t->allocated_buffer_size);        
    // To return a pointer to a type other than void, use a type cast on the return value. 
    // The storage space pointed to by the return value is guaranteed to be suitably aligned for storage of any type of object

    if(t->allocated_buffer)
    {
        int shift_offset_y, shift_offset_cr; 
        int base_u, base_v;

        base_u = t->allocated_buffer_size*4/6;
        base_v = t->allocated_buffer_size*5/6;            

        shift_offset_y = buf_pitch * t->padding + t->padding;
        shift_offset_cr = ((buf_pitch/2) * (t->padding/2) + t->padding/2); 

        t->padded_band_ll.y_plane = (Ipp16s*)t->allocated_buffer;
        t->padded_band_ll.u_plane = (Ipp16s*)t->allocated_buffer+base_u;
        t->padded_band_ll.v_plane = (Ipp16s*)t->allocated_buffer+base_v;
        t->padded_band_ll.size = t->allocated_buffer_size;                

        t->bandLL.y_plane = t->padded_band_ll.y_plane + shift_offset_y;
        t->bandLL.u_plane = t->padded_band_ll.u_plane + shift_offset_cr;
        t->bandLL.v_plane = t->padded_band_ll.v_plane + shift_offset_cr;
        t->bandLL.size = t->allocated_buffer_size;

        t->padded_band_hl.y_plane = (Ipp16s*)t->allocated_buffer + buf_pitch/2;
        t->padded_band_hl.u_plane = (Ipp16s*)t->allocated_buffer + base_u;
        t->padded_band_hl.u_plane += buf_pitch/4;
        t->padded_band_hl.v_plane = (Ipp16s*)t->allocated_buffer + base_v;
        t->padded_band_hl.v_plane += buf_pitch/4;
        t->padded_band_hl.size = t->allocated_buffer_size;        

        t->bandHL.y_plane = t->padded_band_hl.y_plane + shift_offset_y;
        t->bandHL.u_plane = t->padded_band_hl.u_plane + shift_offset_cr;
        t->bandHL.v_plane = t->padded_band_hl.v_plane + shift_offset_cr;
        t->bandHL.size = t->allocated_buffer_size;

        t->padded_band_lh.y_plane = (Ipp16s*)t->allocated_buffer + buf_pitch*buf_height/2;
        t->padded_band_lh.u_plane = (Ipp16s*)t->allocated_buffer + base_u;
        t->padded_band_lh.u_plane +=  (buf_pitch/2)*(buf_height/4);
        t->padded_band_lh.v_plane = (Ipp16s*)t->allocated_buffer + base_v;
        t->padded_band_lh.v_plane += (buf_pitch/2)*(buf_height/4);
        t->padded_band_lh.size = t->allocated_buffer_size;        

        t->bandLH.y_plane = t->padded_band_lh.y_plane + shift_offset_y;
        t->bandLH.u_plane = t->padded_band_lh.u_plane + shift_offset_cr;
        t->bandLH.v_plane = t->padded_band_lh.v_plane + shift_offset_cr;
        t->bandLH.size = t->allocated_buffer_size;

        t->padded_band_hh.y_plane = (Ipp16s*)t->allocated_buffer + buf_pitch*buf_height/2;
        t->padded_band_hh.y_plane += buf_pitch/2;
        t->padded_band_hh.u_plane = (Ipp16s*)t->allocated_buffer + base_u;
        t->padded_band_hh.u_plane += buf_pitch*buf_height/8;
        t->padded_band_hh.u_plane += buf_pitch/4;
        t->padded_band_hh.v_plane = (Ipp16s*)t->allocated_buffer + base_v;
        t->padded_band_hh.v_plane += buf_pitch*buf_height/8;
        t->padded_band_hh.v_plane += buf_pitch/4;
        t->padded_band_hh.size = t->allocated_buffer_size;        

        t->bandHH.y_plane = t->padded_band_hh.y_plane + shift_offset_y;
        t->bandHH.u_plane = t->padded_band_hh.u_plane + shift_offset_cr;
        t->bandHH.v_plane = t->padded_band_hh.v_plane + shift_offset_cr;
        t->bandHH.size = t->allocated_buffer_size;

        YuvSubband_SetPlanePointers(&t->padded_band_ll);
        YuvSubband_SetPlanePointers(&t->padded_band_hl);
        YuvSubband_SetPlanePointers(&t->padded_band_lh);
        YuvSubband_SetPlanePointers(&t->padded_band_hh);

        YuvSubband_SetPlanePointers(&t->bandLL);
        YuvSubband_SetPlanePointers(&t->bandHL);
        YuvSubband_SetPlanePointers(&t->bandLH);
        YuvSubband_SetPlanePointers(&t->bandHH);
        RSubband_Clear(t);

    } else {
        failed = TRUE;
    }            
done:
    return FALSE;
}

void RSubband_Clear(RSubband *t)
{
    // simple scheme for now,.         
    memset(t->allocated_buffer, 0, sizeof(Ipp16s)*t->allocated_buffer_size);
    // convert image should do the padding and copy?
}

RSubband * RSubband_Construct()
{
    RSubband * t;
    t = (RSubband*) malloc(sizeof(RSubband));
    if(t)
    {
        RSubband_Init(t);
    }
    return t;
}

RSubband * RSubband_Destroy(RSubband *t)
{
    if(t)
    {
        RSubband_Free(t);
        free(t);
        t= NULL;
    }
    return t;
}

/**
* An encapsulation / abstraction for interger source and destinations 
* when 1 level decomposition is used.
*/

void RSubband_Init(RSubband *t)
{
    YuvSubband_Init(&t->padded_band_ll);
    YuvSubband_Init(&t->bandLL);
    YuvSubband_Init(&t->padded_band_hl);
    YuvSubband_Init(&t->bandHL);
    YuvSubband_Init(&t->padded_band_lh);
    YuvSubband_Init(&t->bandLH);
    YuvSubband_Init(&t->padded_band_hh);
    YuvSubband_Init(&t->bandHH);

    t->bands[0]=&t->bandLL;
    t->bands[1]=&t->bandHL;
    t->bands[2]=&t->bandLH;
    t->bands[3]=&t->bandHH;

    t->allocated_buffer = NULL;
    t->allocated_buffer_size = 0;
    t->heightInTiles =0;
    t->widthInTiles = 0;
    t->padding=MAX_TILE_SIZE;
    t->prev_frame = NULL;
    t->next_frame = NULL;
    t->past_frame = NULL;
    t->futr_frame = NULL;

    t->TR = (Ipp32u)-1;
    t->coded=0;
    t->ePicType=0;
    t->reference=0;
    t->displayed=0;
}    


void RSubband_Free(RSubband *t)
{
    if (t->allocated_buffer)
    {
        free(t->allocated_buffer);
        t->allocated_buffer = NULL;
        t->allocated_buffer_size = 0;
        YuvSubband_SetPointers(&t->bandLL, NULL, 0,0);
        YuvSubband_SetPointers(&t->padded_band_ll, NULL, 0,0);
        YuvSubband_SetPointers(&t->bandHL, NULL, 0,0);
        YuvSubband_SetPointers(&t->padded_band_hl, NULL, 0,0);
        YuvSubband_SetPointers(&t->bandLH, NULL, 0,0);
        YuvSubband_SetPointers(&t->padded_band_lh, NULL, 0,0);
        YuvSubband_SetPointers(&t->bandHH, NULL, 0,0);
        YuvSubband_SetPointers(&t->padded_band_hh, NULL, 0,0);
    }    
}

//---------------------------------------------------------

RFrameVector * RFrameVector_Construct()
{
    RFrameVector * t;
    t = (RFrameVector *)malloc(sizeof(RFrameVector));
    if (t)
        RFrameVector_Init(t);
    return t;
}

void RFrameVector_Destroy(RFrameVector *t)
{
    if (t)
    {
        RFrameVector_Delete(t);
        free(t);
        t = NULL;
    }
    return;
}

void RFrameVector_Init(RFrameVector *t) 
{ 
    t->head = t->tail = 0; 
    t->length = 0;
}

/**
* Should rename it to close
* Reset init vars
*/
void RFrameVector_Delete(RFrameVector *t)
{
    t->head = t->tail = 0;
    t->length = 0;
}

RFrame*
    RFrameVector_Detach_Head(RFrameVector *t)
{
    RFrame *head;
    head = t->head;
    if (head)
    {
        t->length--;
        t->head = t->head->futr_frame;
        if (t->head)
        {
            t->head->past_frame = 0;
        }
        else
        {
            t->tail = 0;
        }
    }
    //DebugMessage(generic_debug, LOG_LVL_FRAME, "RFrameVector 0x%x len %d", t, t->length);
    return head;
}

void
    RFrameVector_Remove(RFrameVector *t, 
    RFrame *frame)
{    
    RFrame *past;
    RFrame *futr;
    RFrame *head;

    if(frame && t->length)
    {
        past = frame->past_frame;
        futr = frame->futr_frame;
        if(past)
        {
            past->futr_frame = futr;
            if(!futr) t->tail = past;
            else futr->past_frame = past;
            t->length--;
            //DebugMessage(generic_debug, LOG_LVL_FRAME, "RFrameVector 0x%x len %d", t, t->length);
        }
        else
        {
            // this frame is head
            // assert(head == frame)
            head = RFrameVector_Detach_Head(t);
        }        
    }    
    return ;
}

void
    RFrameVector_Append_Before_Tail(RFrameVector *t,
    RFrame *frame)
{    
    RFrame *past;
    RFrame *tail;

    if(frame && t->length)
    {        
        tail = t->tail;
        past = tail->past_frame;
        if(past)
        {
            past->futr_frame = frame;
            frame->past_frame = past;
            frame->futr_frame = tail;
            tail->past_frame = frame;
            t->length++;
        }
        else
        {
            t->head = frame;
            frame->past_frame = NULL;
            frame->futr_frame = tail;
            tail->past_frame = frame;
            t->length++;            
        }
        //DebugMessage(generic_debug, LOG_LVL_FRAME, "RFrameVector 0x%x len %d", t, t->length);     
    }    
    return ;
}

void
    RFrameVector_Append_Tail(RFrameVector *t, 
    RFrame *frame)
{
    frame->past_frame=t->tail;
    frame->futr_frame=0;
    if (t->tail)
        t->tail->futr_frame=frame;
    else
        t->head = frame;

    t->tail = frame;
    t->length++;
    //DebugMessage(generic_debug, LOG_LVL_FRAME, "RFrameVector 0x%x len %d", t, t->length);
}


/* 
* incomplete
*/
void
    RFrameVector_Insert_List(RFrameVector *t, 
    RFrameVector *src)
{
    if (src->head)
    {
        src->tail->futr_frame = t->head;
        if (t->head)
            t->head->past_frame = src->tail;
        t->head = src->head;

        if (!t->tail)
            t->tail = src->tail;

        src->head = src->tail = 0;
    }
}


RSubbandVector * RSubbandVector_Construct()
{
    RSubbandVector * t;
    t = (RSubbandVector *)malloc(sizeof(RSubbandVector));
    if (t)
        RSubbandVector_Init(t);
    return t;
}

void RSubbandVector_Destroy(RSubbandVector *t)
{
    if (t)
    {
        RSubbandVector_Delete(t);
        free(t);
        t = NULL;
    }
    return;
}

void RSubbandVector_Init(RSubbandVector *t) 
{ 
    t->head = t->tail = 0; 
    t->length = 0;
}

/**
* Should rename it to close
* Reset init vars
*/
void RSubbandVector_Delete(RSubbandVector *t)
{
    t->head = t->tail = 0;
    t->length = 0;
}

RSubband*
    RSubbandVector_Detach_Head(RSubbandVector *t)
{
    RSubband *head;
    head = t->head;
    if (head)
    {
        t->length--;
        t->head = t->head->futr_frame;
        if (t->head)
            t->head->past_frame = 0;
        else
            t->tail = 0;
    }
    //DebugMessage(generic_debug, LOG_LVL_FRAME, "RSubbandVector 0x%x len %d", t, t->length);
    return head;
}

void
    RSubbandVector_Remove(RSubbandVector *t, 
    RSubband *frame)
{    
    RSubband *past;
    RSubband *futr;
    RSubband *head;

    if(frame && t->length)
    {
        past = frame->past_frame;
        futr = frame->futr_frame;
        if(past)
        {
            past->futr_frame = futr;
            if(!futr) t->tail = past;
            else futr->past_frame = past;
            t->length--;
            //DebugMessage(generic_debug, LOG_LVL_FRAME, "RSubbandVector 0x%x len %d", t, t->length);
        }
        else
        {
            // this frame is head
            // assert(head == frame)
            head = RSubbandVector_Detach_Head(t);
        }        
    }    
    return ;
}
void
    RSubbandVector_Append_Tail(RSubbandVector *t, 
    RSubband *frame)
{
    frame->past_frame=t->tail;
    frame->futr_frame=0;
    if (t->tail)
        t->tail->futr_frame=frame;
    else
        t->head = frame;

    t->tail = frame;
    t->length++;
    //DebugMessage(generic_debug, LOG_LVL_FRAME, "RSubbandVector 0x%x len %d", t, t->length);    
}

void
    RSubbandVector_Append_Before_Tail(RSubbandVector *t,
    RSubband *frame)
{    
    RSubband *past;
    RSubband *tail;

    if(frame && t->length)
    {        
        tail = t->tail;
        past = tail->past_frame;
        if(past)
        {
            past->futr_frame = frame;
            frame->past_frame = past;
            frame->futr_frame = tail;
            tail->past_frame = frame;
            t->length++;
        }
        else
        {
            t->head = frame;
            frame->past_frame = NULL;
            frame->futr_frame = tail;
            tail->past_frame = frame;
            t->length++;            
        }
        //DebugMessage(generic_debug, LOG_LVL_FRAME, "RSubbandVector 0x%x len %d", t, t->length);     
    }    
    return ;
}

/*
* incomplete
*/
void
    RSubbandVector_Insert_List(RSubbandVector *t, 
    RSubbandVector *src)
{
    if (src->head)
    {
        src->tail->futr_frame = t->head;
        if (t->head)
            t->head->past_frame = src->tail;
        t->head = src->head;

        if (!t->tail)
            t->tail = src->tail;

        src->head = src->tail = 0;
    }
}

/**
* RFrameQueue
*/


RFrameQueue * RFrameQueue_Construct()
{
    RFrameQueue * t;
    t = (RFrameQueue *)malloc(sizeof(RFrameQueue));
    if (t)
        RFrameQueue_Init(t);
    return t;
}

void RFrameQueue_Destroy(RFrameQueue *t)
{
    if (t)
    {
        RFrameQueue_Delete(t);
        free(t);
        t = NULL;
    }
    return;
}

void RFrameQueue_Init(RFrameQueue *t) 
{ 
    t->head = t->tail = 0; 
    t->length = 0;
}

/**
* Should rename it to close
* Reset init vars
*/
void RFrameQueue_Delete(RFrameQueue *t)
{
    t->head = t->tail = 0;
    t->length = 0;
}

RFrame*
    RFrameQueue_Detach_Head(RFrameQueue *t)
{
    RFrame *head;
    head = t->head;
    if (head)
    {
        t->length--;
        t->head = t->head->next_frame;
        if (t->head)
            t->head->prev_frame = 0;
        else
            t->tail = 0;
    }
    //DebugMessage(generic_debug, LOG_LVL_FRAME, "RFrameQueue 0x%x len %d", t, t->length);
    return head;
}

void
    RFrameQueue_Remove(RFrameQueue *t, 
    RFrame *frame)
{    
    RFrame *prev;
    RFrame *next;
    RFrame *head;

    if(frame && t->length)
    {
        prev = frame->prev_frame;
        next = frame->next_frame;
        if(prev)
        {
            prev->next_frame = next;
            if(!next) t->tail = prev;
            else next->prev_frame = prev;
            t->length--;
            //DebugMessage(generic_debug, LOG_LVL_FRAME, "RFrameQueue 0x%x len %d", t, t->length);
        }
        else
        {
            // this frame is head
            // assert(head == frame)
            head = RFrameQueue_Detach_Head(t);
        }        
    }    
    return ;
}

void
    RFrameQueue_Append_Tail(RFrameQueue *t, 
    RFrame *frame)
{
    frame->prev_frame=t->tail;
    frame->next_frame=0;
    if (t->tail)
        t->tail->next_frame=frame;
    else
        t->head = frame;

    t->tail = frame;
    t->length++;
    //DebugMessage(generic_debug, LOG_LVL_FRAME, "RFrameQueue 0x%x len %d", t, t->length);
}


/*
* Incomplete
*/
void
    RFrameQueue_Insert_List(RFrameQueue *t, 
    RFrameQueue *src)
{
    if (src->head)
    {
        src->tail->next_frame = t->head;
        if (t->head)
            t->head->prev_frame = src->tail;
        t->head = src->head;

        if (!t->tail)
            t->tail = src->tail;

        src->head = src->tail = 0;
    }
}


RSubbandQueue * RSubbandQueue_Construct()
{
    RSubbandQueue * t;
    t = (RSubbandQueue *)malloc(sizeof(RSubbandQueue));
    if (t)
        RSubbandQueue_Init(t);
    return t;
}

void RSubbandQueue_Destroy(RSubbandQueue *t)
{
    if (t)
    {
        RSubbandQueue_Delete(t);
        free(t);
        t = NULL;
    }
    return;
}

void RSubbandQueue_Init(RSubbandQueue *t) 
{ 
    t->head = t->tail = 0; 
    t->length = 0;
}

/**
* Should rename it to close
* Reset init vars
*/
void RSubbandQueue_Delete(RSubbandQueue *t)
{
    t->head = t->tail = 0;
    t->length = 0;
}

RSubband*
    RSubbandQueue_Detach_Head(RSubbandQueue *t)
{
    RSubband *head;
    head = t->head;
    if (head)
    {
        t->length--;
        t->head = t->head->next_frame;
        if (t->head)
            t->head->prev_frame = 0;
        else
            t->tail = 0;
    }

    //DebugMessage(generic_debug, LOG_LVL_FRAME, "RSubbandQueue 0x%x len %d", t, t->length);
    return head;
}

void
    RSubbandQueue_Remove(RSubbandQueue *t, 
    RSubband *frame)
{    
    RSubband *prev;
    RSubband *next;
    RSubband *head;

    if(frame && t->length)
    {
        prev = frame->prev_frame;
        next = frame->next_frame;
        if(prev)
        {
            prev->next_frame = next;
            if(!next) t->tail = prev;
            else next->prev_frame = prev;
            t->length--;
            //DebugMessage(generic_debug, LOG_LVL_FRAME, "RSubbandQueue 0x%x len %d", t, t->length);
        }
        else
        {
            // this frame is head
            // assert(head == frame)
            head = RSubbandQueue_Detach_Head(t);
        }        
    }        
}


void
    RSubbandQueue_Append_Tail(RSubbandQueue *t, 
    RSubband *frame)
{
    frame->prev_frame=t->tail;
    frame->next_frame=0;
    if (t->tail)
        t->tail->next_frame=frame;
    else
        t->head = frame;

    t->tail = frame;
    t->length++;
    //DebugMessage(generic_debug, LOG_LVL_FRAME, "RSubbandQueue 0x%x len %d", t, t->length);
}

/*
* incomplete
*/
void
    RSubbandQueue_Insert_List(RSubbandQueue *t, 
    RSubbandQueue *src)
{
    if (src->head)
    {
        src->tail->next_frame = t->head;
        if (t->head)
            t->head->prev_frame = src->tail;
        t->head = src->head;

        if (!t->tail)
            t->tail = src->tail;

        src->head = src->tail = 0;
    }
}


//---------------------------------------------------------
//      image.cpp
//---------------------------------------------------------
/**
* YuvImage
*/
void YuvImage_Init(YuvImage *t)
{
    t->width=0;
    t->height=0;
    t->sequence_number=0;
    t->y_plane=NULL;
    t->u_plane=NULL;
    t->v_plane=NULL;
    t->plane[PLANE_LUMA] = t->plane[PLANE_CR_U] = t->plane[PLANE_CR_V] = NULL;
}
/**
* Simple Construct YuvImage
* check returned pointer for failure 
*/
YuvImage* YuvImage_Construct(YuvImage* t)
{  
    if(t == NULL) t = (YuvImage*) malloc(sizeof(YuvImage));
    if(t) YuvImage_Init(t);  
    return t;
}

YuvImage* YuvImage_Destroy(YuvImage* t)
{
    if(t) free(t);
    t = NULL;  
    return t;
}

void YuvImage_SetPlanePointers(YuvImage* t)
{
    t->plane[PLANE_LUMA] = t->y_plane;
    t->plane[PLANE_CR_U] = t->u_plane;
    t->plane[PLANE_CR_V] = t->v_plane;
    t->pitch[PLANE_LUMA] = t->y_pitch;
    t->pitch[PLANE_CR_U] = t->u_pitch;
    t->pitch[PLANE_CR_V] = t->v_pitch;
}

void YuvImage_SetPointers(YuvImage* t, Ipp8u* y_plane, Ipp32u width, Ipp32u height)
{
    t->width = width;
    t->height = height;
    t->y_plane = y_plane;
    t->u_plane = y_plane+width*height;
    t->v_plane = t->u_plane+width*height/4;
    t->y_pitch = width;
    t->u_pitch = width/2;
    t->v_pitch = width/2;
    YuvImage_SetPlanePointers(t);
}

/*
* For more complicated images 
*/
void YuvImage_SetAllPointers(YuvImage* t, Ipp8u* y_plane, Ipp8u* u_plane, Ipp8u* v_plane, Ipp32u width, Ipp32u height, Ipp32u y_pitch, Ipp32u cr_pitch)
{
    t->width = width;
    t->height = height;
    t->y_plane = y_plane;    
    t->u_plane = u_plane;
    t->v_plane = v_plane;
    t->y_pitch = y_pitch;
    t->u_pitch = cr_pitch;
    t->v_pitch = cr_pitch;
    YuvImage_SetPlanePointers(t);
}


void YuvImage_Copy(YuvImage *t, YuvImage *src)
{
    int j;
    // both frames should be of same dimensions
    int H = t->height;
    int W = t->width;
    Ipp8u* dd = t->y_plane;
    Ipp8u* ss = src->y_plane;
    int yDP = t->y_pitch;
    int uDP = t->u_pitch;
    int vDP = t->v_pitch;
    int ySP = src->y_pitch;
    int uSP = src->u_pitch;
    int vSP = src->v_pitch;

    for (j = 0; j < H; j++)
    {
        memcpy_s(dd, t->width, ss, W);
        dd += yDP;
        ss += ySP;
    }

    dd = t->u_plane;
    ss = src->u_plane;    

    for (j = 0; j < H/2; j++)
    {
        memcpy_s(dd, t->width/2, ss, W/2);
        dd += uDP;
        ss += uSP;
    }

    dd = t->v_plane;
    ss = src->v_plane;    

    for (j = 0; j < H/2; j++)
    {
        memcpy_s(dd, t->width/2, ss, W/2);
        dd += vDP;
        ss += vSP;
    }
}

/**
* YuvImageBuffer
*/
void YuvImageBuffer_Init(YuvImageBuffer *t)
{
    t->m_uBufferSize =0;
    t->m_pBuffer = NULL;
    YuvImage_Init(&(t->image));
}
/**
* Simple Construct YuvImageBuffer
* can test pointer for failure
*/
YuvImageBuffer* YuvImageBuffer_Construct(YuvImageBuffer* t)
{  
    if(t == NULL) t = (YuvImageBuffer*) malloc(sizeof(YuvImageBuffer));
    if(t) YuvImageBuffer_Init(t);  
    return t;
}

YuvImageBuffer* YuvImageBuffer_Destroy(YuvImageBuffer* t)
{
    YuvImageBuffer_Free(t);
    if(t) free(t);
    t = NULL;  
    return t;
}

NGV_Bool YuvImageBuffer_Alloc(YuvImageBuffer *t, Ipp32u width, Ipp32u height)
{
    NGV_Bool failed = FALSE;
    // logic    
    if(t->m_pBuffer && t->m_uBufferSize!=width*height*3/2) { free(t->m_pBuffer); t->m_pBuffer=NULL; };
    t->m_uBufferSize = width*height*3/2;
    if(!t->m_pBuffer) t->m_pBuffer = (Ipp8u*) malloc(t->m_uBufferSize);    
    if(!t->m_pBuffer) failed = TRUE;
    if(!failed) YuvImage_SetPointers(&(t->image), t->m_pBuffer, width, height);
    // memset -> for debbuging purposes only
    //memset(t->image.y_plane, 0, width*height);
    //memset(t->image.u_plane, 128, width*height/4);
    //memset(t->image.v_plane, 128, width*height/4);
    return failed;
}

void YuvImageBuffer_Free(YuvImageBuffer *t)
{
    if(t->m_pBuffer) free(t->m_pBuffer);
    t->m_pBuffer = NULL;
    t->m_uBufferSize = 0;
}

/**
* BinImage
*/
void BinImage_Init(BinImage *t)
{
    t->width=0;
    t->height=0;
    t->size=0;
    t->sequence_number=0;
    t->flags=0;
    t->data=0;
    // Always add 1 to num_packets
    t->num_packets = 0;
    t->Packets[0].bIsValid=1;
    t->Packets[0].ulPacketOffset=0;
}

BinImage* BinImage_Construct(BinImage* t)
{  
    if(t == NULL) t = (BinImage*) malloc(sizeof(BinImage));
    if(t) BinImage_Init(t);
    return t;
}


BinImage* BinImage_Destroy(BinImage* t)
{
    if(t) free(t);
    t = NULL;  
    return t;
}

void BinImage_SetDim(BinImage *t, Ipp32u width, Ipp32u height)
{
    t->width = width;
    t->height = height;
}

void BinImage_SetPointers(BinImage* t, Ipp8u* base, Ipp32u size)
{
    t->data = base;
    t->size = size;
}

NGV_Bool BinImage_isBFrame(BinImage* t)
{
    // Implement
    return (t->frameType==2 || t->frameType==6)?TRUE:FALSE;
}

NGV_Bool BinImage_isPFrame(BinImage* t)
{
    // Implement
    return (t->frameType==1 || t->frameType==5)?TRUE:FALSE;
}

NGV_Bool BinImage_isKeyFrame(BinImage* t)
{
    // Implement
    return (t->frameType==0 || t->frameType==4)?TRUE:FALSE;
}

/**
* BinImageBuffer
*/

BinImageBuffer* BinImageBuffer_Construct(BinImageBuffer* t)
{
    if(t == NULL) t = (BinImageBuffer*) malloc(sizeof(BinImageBuffer));
    if(t) BinImageBuffer_Init(t);  
    return t;
}

BinImageBuffer* BinImageBuffer_Destroy(BinImageBuffer* t)
{
    BinImageBuffer_Free(t);
    if(t) free(t);
    t = NULL;  
    return t;
}

void BinImageBuffer_Init(BinImageBuffer *t)
{           
    t->m_uBufferSize=0;
    t->m_pBuffer=0;
    BinImage_Init(&(t->image));
}

NGV_Bool BinImageBuffer_Alloc(BinImageBuffer *t, Ipp32u size)
{
    NGV_Bool failed = FALSE;
    // logic    
    if(t->m_pBuffer && t->m_uBufferSize!=size) { free(t->m_pBuffer); t->m_pBuffer=NULL; };
    t->m_uBufferSize = size;
    if(!t->m_pBuffer) t->m_pBuffer = (Ipp8u*) malloc(t->m_uBufferSize);
    if(!t->m_pBuffer) failed = TRUE;
    if(!failed) BinImage_SetPointers(&(t->image), t->m_pBuffer, size);
    return failed;
}

void BinImageBuffer_Free(BinImageBuffer *t)
{
    if(t->m_pBuffer) free(t->m_pBuffer);
    t->m_pBuffer = NULL;
    t->m_uBufferSize = 0;
}


/**
* YuvSubband
*/
void YuvSubband_Init(YuvSubband *t)
{
    t->width=0;
    t->height=0;
    t->sequence_number=0;
    t->y_plane=NULL;
    t->u_plane=NULL;
    t->v_plane=NULL;
    t->plane[PLANE_LUMA] = t->plane[PLANE_CR_U] = t->plane[PLANE_CR_V] = NULL;
}
/**
* Simple Construct YuvImage
* check returned pointer for failure 
*/
YuvSubband* YuvSubband_Construct(YuvSubband* t)
{  
    if(t == NULL) t = (YuvSubband*) malloc(sizeof(YuvSubband));
    if(t) YuvSubband_Init(t);  
    return t;
}

YuvSubband* YuvSubband_Destroy(YuvSubband* t)
{
    if(t) free(t);
    t = NULL;  
    return t;
}

void YuvSubband_SetPlanePointers(YuvSubband* t)
{
    t->plane[PLANE_LUMA] = t->y_plane;
    t->plane[PLANE_CR_U] = t->u_plane;
    t->plane[PLANE_CR_V] = t->v_plane;
    t->pitch[PLANE_LUMA] = t->y_pitch;
    t->pitch[PLANE_CR_U] = t->u_pitch;
    t->pitch[PLANE_CR_V] = t->v_pitch;
}

void YuvSubband_SetPointers(YuvSubband* t, Ipp16s* y_plane, Ipp32u width, Ipp32u height)
{
    t->width = width;
    t->height = height;
    t->y_plane = y_plane;
    t->u_plane = y_plane+width*height;
    t->v_plane = t->u_plane+width*height/4;
    t->y_pitch = width;
    t->u_pitch = width/2;
    t->v_pitch = width/2;
    YuvSubband_SetPlanePointers(t);
}
/*
* For more complicated images 
*/
void YuvSubband_SetAllPointers(YuvSubband* t, Ipp16s* y_plane, Ipp16s* u_plane, Ipp16s* v_plane, Ipp32u width, Ipp32u height, Ipp32u y_pitch, Ipp32u cr_pitch)
{
    t->width = width;
    t->height = height;
    t->y_plane = y_plane;    
    t->u_plane = u_plane;
    t->v_plane = v_plane;
    t->y_pitch = y_pitch;
    t->u_pitch = cr_pitch;
    t->v_pitch = cr_pitch;
    YuvSubband_SetPlanePointers(t);
}

//---------------------------------------------------------
//   init_iop
//---------------------------------------------------------
NGV_Bool    PdYuvImage_Alloc(YUV *pImage, Ipp32u dimVideoExtended);
NGV_Bool    PdYuvImage_Alloc2(YUV *pImage, Ipp32u dimVideoExtended, Ipp32u dimColorExtended);
void    PdYuvImage_NULL(YUV *pImage);
void    imageInit(YUV *buffer);

const __int64                x6        =    0x0008000800080008;
const __int64                x7        =    0x0002000200020002;

Ipp32s            filter1[14]                =    {0,0,0,0,0,5,11,11,5,0,0,0,0,32};     
Ipp32s            fir2[14]                =    {2,0,-4,-3,5,19,26,19,5,-3,-4,0,2,64};
Ipp32s            fir3[16]                =    {0,2,0,-4,-3,5,19,26,19,5,-3,-4,0,2,0,64};
Ipp32s            fir4[16]                =    {0,0,0,0,-1,0,9,16,9,0,-1,0,0,0,0,32};
Ipp32s            fir5[14]                =    {0,0,0,0,0,1,2,1,0,0,0,0,0,4};
Ipp32s            filter4[16]                =    {-5,-4,0,5,12,19,24,26,24,19,12,5,0,-4,-5,128};

void    VidSampleInit(VidSample *videoIn)
{
    /*Full Size image*/
    imageInit(&(videoIn->R1Buffer.exImage));
    videoIn->R1Buffer.pBackward        =    NULL;
    videoIn->R1Buffer.deltaF.Y        =    NULL;
    videoIn->R1Buffer.deltaF.U        =    NULL;
    videoIn->R1Buffer.deltaF.V        =    NULL;
#ifndef LESS_MEM_PDIST
    videoIn->R1Buffer.pForward        =    NULL;
    videoIn->R1Buffer.FSAD            =    NULL;
    videoIn->R1Buffer.BSAD            =    NULL;
    videoIn->R1Buffer.ASAD            =    NULL;
    videoIn->R1Buffer.Forward.Y        =    NULL;
    videoIn->R1Buffer.Average.Y        =    NULL;
    videoIn->R1Buffer.pFhalf        =    NULL;
    videoIn->R1Buffer.pFquarter        =    NULL;
#endif

    videoIn->R1Buffer.Backward.Y    =    NULL;
    videoIn->R1Buffer.pBhalf        =    NULL;
    videoIn->R1Buffer.pBquarter        =    NULL;
    videoIn->R1Buffer.Pframe.Y        =    NULL;
    videoIn->R1Buffer.Pframe.U        =    NULL;
    videoIn->R1Buffer.Pframe.V        =    NULL;
    videoIn->R1Buffer.deltaB.Y        =    NULL;
    videoIn->R1Buffer.deltaB.U        =    NULL;
    videoIn->R1Buffer.deltaB.V        =    NULL;
    videoIn->R1Buffer.Cs            =    NULL;
    videoIn->R1Buffer.Rs            =    NULL;
    videoIn->R1Buffer.DiffCs        =    NULL;
    videoIn->R1Buffer.DiffRs        =    NULL;

    /*Quarter sized image*/
    imageInit(&(videoIn->R4erBuffer.exImage));
    videoIn->R4erBuffer.pBackward    =    NULL;
    videoIn->R4erBuffer.pBhalf        =    NULL;
    videoIn->R4erBuffer.pBquarter    =    NULL;
    videoIn->R4erBuffer.deltaF.Y    =    NULL;
    videoIn->R4erBuffer.deltaF.U    =    NULL;
    videoIn->R4erBuffer.deltaF.V    =    NULL;
#ifndef LESS_MEM_PDIST
    videoIn->R4erBuffer.pForward    =    NULL;
    videoIn->R4erBuffer.pFhalf        =    NULL;
    videoIn->R4erBuffer.pFquarter    =    NULL;
    videoIn->R4erBuffer.FSAD        =    NULL;
    videoIn->R4erBuffer.ASAD        =    NULL;
    videoIn->R4erBuffer.Forward.Y    =    NULL;
    videoIn->R4erBuffer.Average.Y    =    NULL;
#endif
    videoIn->R4erBuffer.BSAD        =    NULL;
    videoIn->R4erBuffer.Backward.Y    =    NULL;
    videoIn->R4erBuffer.deltaB.Y    =    NULL;
    videoIn->R4erBuffer.deltaB.U    =    NULL;
    videoIn->R4erBuffer.deltaB.V    =    NULL;
    videoIn->R4erBuffer.Pframe.Y    =    NULL;
    videoIn->R4erBuffer.Pframe.U    =    NULL;
    videoIn->R4erBuffer.Pframe.V    =    NULL;
    videoIn->R4erBuffer.Cs            =    NULL;
    videoIn->R4erBuffer.Rs            =    NULL;
    videoIn->R4erBuffer.DiffCs        =    NULL;
    videoIn->R4erBuffer.DiffRs        =    NULL;

    /*1/16 sized image*/
    imageInit(&(videoIn->R16thBuffer.exImage));
    videoIn->R16thBuffer.pBackward    =    NULL;
    videoIn->R16thBuffer.pBhalf        =    NULL;
    videoIn->R16thBuffer.pBquarter    =    NULL;
    videoIn->R16thBuffer.deltaF.Y    =    NULL;
    videoIn->R16thBuffer.deltaF.U    =    NULL;
    videoIn->R16thBuffer.deltaF.V    =    NULL;
#ifndef LESS_MEM_PDIST
    videoIn->R16thBuffer.pForward    =    NULL;
    videoIn->R16thBuffer.pFhalf        =    NULL;
    videoIn->R16thBuffer.pFquarter    =    NULL;
    videoIn->R16thBuffer.FSAD        =    NULL;
    videoIn->R16thBuffer.ASAD        =    NULL;
    videoIn->R16thBuffer.Forward.Y    =    NULL;
    videoIn->R16thBuffer.Average.Y    =    NULL;
#endif
    videoIn->R16thBuffer.BSAD        =    NULL;
    videoIn->R16thBuffer.Backward.Y    =    NULL;
    videoIn->R16thBuffer.deltaB.Y    =    NULL;
    videoIn->R16thBuffer.deltaB.U    =    NULL;
    videoIn->R16thBuffer.deltaB.V    =    NULL;
    videoIn->R16thBuffer.Pframe.Y    =    NULL;
    videoIn->R16thBuffer.Pframe.U    =    NULL;
    videoIn->R16thBuffer.Pframe.V    =    NULL;
    videoIn->R16thBuffer.Cs            =    NULL;
    videoIn->R16thBuffer.Rs            =    NULL;
    videoIn->R16thBuffer.DiffCs        =    NULL;
    videoIn->R16thBuffer.DiffRs        =    NULL;
}

void    readerInit(VidRead *videoIn)    {
    int        i;


    for(i=0;i<MAXPDIST+1+MAX_LOOKAHEAD;i++)
    {
        videoIn->logic[i].AFD        =    -1.0;
        videoIn->logic[i].Cs        =    -1.0;
        videoIn->logic[i].frameNum    =    (Ipp32u)-1;  // Future Note: Conversion from int to Ipp32u. signed/unsigned mismatch
        videoIn->logic[i].Gchg        =    FALSE;
        videoIn->logic[i].pdist        =    -1;
        videoIn->logic[i].picType    =    0;
        videoIn->logic[i].repeatedFrame    =    0;
        videoIn->logic[i].Rs        =    -1.0;
        videoIn->logic[i].SC        =    -1.0;
        videoIn->logic[i].Schg        =    FALSE;
        videoIn->logic[i].SCindex    =    -1;
        videoIn->logic[i].TSC        =    -1.0;
        videoIn->logic[i].TSCindex    =    -1;
    }

    videoIn->bufferH                =    NULL;
    videoIn->bufferQ                =    NULL;

    /*Half Pixel Accuracy variables*/
    videoIn->cornerBox.Y            =    NULL;
    videoIn->topBox.Y                =    NULL;
    videoIn->leftBox.Y                =    NULL;

    /*Quarter Pixel Accuracy variables*/
    for(i=0;i<8;i++)                {
        videoIn->spBuffer[i].Y        =    NULL;
    }
    videoIn->average                =    0;
    videoIn->avgSAD                    =    0;
    videoIn->longerMV                =    0;
}

void    writerInit(DataWriter *dataOut)        {
    dataOut->filename                =    NULL;
    dataOut->pDataOut                =    NULL;
}

void    imageInit(YUV *buffer)    {
    buffer->Y                    =    NULL;
    buffer->U                    =    NULL;
    buffer->V                    =    NULL;
}


NGV_Bool    VidSampleAlloc(void *vt, VidSample *videoIn, VidData *inData)
{
    NGV_Bool failed;
    NGV_PDist *t = (NGV_PDist *)vt;
    VidSample *g_videoIn = (VidSample *)(t->videoIn);
    if(t->alloc_pdist)                  VidSampleInit(g_videoIn);
    failed                            =    PdYuvImage_Alloc(&(g_videoIn->R1Buffer.exImage), inData->R1.exYframe);
    failed                            =    PdYuvImage_Alloc(&(g_videoIn->R4erBuffer.exImage), inData->R4er.exYframe);
    failed                            =    PdYuvImage_Alloc(&(g_videoIn->R16thBuffer.exImage), inData->R16th.exYframe);

    /*Full Size image*/

    if(t->alloc_pdist){failed                            =    PdYuvImage_Alloc(&(g_videoIn->R1Buffer.Backward), inData->R1.exYframe);}
    if(t->alloc_pdist){failed                            =    PdYuvImage_Alloc2(&(g_videoIn->R1Buffer.deltaF), inData->R1.Yframe, inData->R1.UVframe);}
    if(t->alloc_pdist){g_videoIn->R1Buffer.BSAD            =    (Ipp32u*) NgvMalloc(sizeof(Ipp32u) * inData->R1.MVspaceSize);}
    if(t->alloc_pdist){g_videoIn->R1Buffer.pBackward    =    (MVector*) NgvMalloc(sizeof(MVector) * inData->R1.MVspaceSize);}
    if(t->alloc_pdist){g_videoIn->R1Buffer.pBhalf        =    (MVector*) NgvMalloc(sizeof(MVector) * inData->R1.MVspaceSize);}
    if(t->alloc_pdist){g_videoIn->R1Buffer.pBquarter    =    (MVector*) NgvMalloc(sizeof(MVector) * inData->R1.MVspaceSize);}

    PdYuvImage_NULL(&(g_videoIn->R1Buffer.Pframe));
    PdYuvImage_NULL(&(g_videoIn->R1Buffer.deltaB));

    {
        g_videoIn->R1Buffer.Cs            =    (Ipp64f*) NgvMalloc(sizeof(Ipp64f) * inData->R1.Yframe / RsCsSIZE);
    }
    {
        g_videoIn->R1Buffer.Rs            =    (Ipp64f*) NgvMalloc(sizeof(Ipp64f) * inData->R1.Yframe / RsCsSIZE);
    }

    if(t->alloc_pdist)
    {
        g_videoIn->R1Buffer.DiffCs        =    (Ipp64f*) NgvMalloc(sizeof(Ipp64f) * inData->R1.Yframe / RsCsSIZE);
    }

    if(t->alloc_pdist)
    {
        g_videoIn->R1Buffer.DiffRs        =    (Ipp64f*) NgvMalloc(sizeof(Ipp64f) * inData->R1.Yframe / RsCsSIZE);
    }

    g_videoIn->R1Buffer.bSval        =    0;
    g_videoIn->R1Buffer.CsVal        =    -1.0;
    g_videoIn->R1Buffer.RsVal        =    -1.0;
    g_videoIn->R1Buffer.DiffCsVal    =    -1.0;
    g_videoIn->R1Buffer.DiffRsVal    =    -1.0;

    /*Quarter sized image*/

    if(t->alloc_pdist){failed        =    PdYuvImage_Alloc(&(g_videoIn->R4erBuffer.Backward), inData->R4er.exYframe);}
    if(t->alloc_pdist){failed        =    PdYuvImage_Alloc2(&(g_videoIn->R4erBuffer.deltaF), inData->R4er.Yframe, inData->R4er.UVframe);}
    if(t->alloc_pdist){g_videoIn->R4erBuffer.BSAD        =    (Ipp32u*) NgvMalloc(sizeof(Ipp32u) * inData->R4er.MVspaceSize);}
    if(t->alloc_pdist){g_videoIn->R4erBuffer.pBackward    =    (MVector*) NgvMalloc(sizeof(MVector) * inData->R4er.MVspaceSize);}
    if(t->alloc_pdist){g_videoIn->R4erBuffer.pBhalf        =    (MVector*) NgvMalloc(sizeof(MVector) * inData->R4er.MVspaceSize);}
    if(t->alloc_pdist){g_videoIn->R4erBuffer.pBquarter    =    (MVector*) NgvMalloc(sizeof(MVector) * inData->R4er.MVspaceSize);}

    PdYuvImage_NULL(&(g_videoIn->R4erBuffer.Pframe));
    PdYuvImage_NULL(&(g_videoIn->R4erBuffer.deltaB));

    if(t->alloc_pdist){g_videoIn->R4erBuffer.Cs            =    (Ipp64f*) NgvMalloc(sizeof(Ipp64f) * inData->R4er.Yframe / RsCsSIZE);}
    if(t->alloc_pdist){g_videoIn->R4erBuffer.Rs            =    (Ipp64f*) NgvMalloc(sizeof(Ipp64f) * inData->R4er.Yframe / RsCsSIZE);}
    if(t->alloc_pdist)    {g_videoIn->R4erBuffer.DiffCs        =    (Ipp64f*) NgvMalloc(sizeof(Ipp64f) * inData->R4er.Yframe / RsCsSIZE);}
    if(t->alloc_pdist)    {g_videoIn->R4erBuffer.DiffRs        =    (Ipp64f*) NgvMalloc(sizeof(Ipp64f) * inData->R4er.Yframe / RsCsSIZE);}

    g_videoIn->R4erBuffer.bSval        =    0;
    g_videoIn->R4erBuffer.CsVal        =    -1.0;
    g_videoIn->R4erBuffer.RsVal        =    -1.0;
    g_videoIn->R4erBuffer.DiffCsVal    =    -1.0;
    g_videoIn->R4erBuffer.DiffRsVal    =    -1.0;

    /*1/16 sized image*/
    if(t->alloc_pdist){failed        =    PdYuvImage_Alloc(&(g_videoIn->R16thBuffer.Backward), inData->R16th.exYframe);}
    if(t->alloc_pdist){failed        =    PdYuvImage_Alloc2(&(g_videoIn->R16thBuffer.deltaF), inData->R16th.Yframe, inData->R16th.UVframe);}

    if(t->alloc_pdist){g_videoIn->R16thBuffer.BSAD        =    (Ipp32u*) NgvMalloc(sizeof(Ipp32u) * inData->R16th.MVspaceSize);}
    if(t->alloc_pdist){g_videoIn->R16thBuffer.pBackward    =    (MVector*) NgvMalloc(sizeof(MVector) * inData->R16th.MVspaceSize);}
    if(t->alloc_pdist){g_videoIn->R16thBuffer.pBhalf    =    (MVector*) NgvMalloc(sizeof(MVector) * inData->R16th.MVspaceSize);}
    if(t->alloc_pdist){g_videoIn->R16thBuffer.pBquarter    =    (MVector*) NgvMalloc(sizeof(MVector) * inData->R16th.MVspaceSize);}

    PdYuvImage_NULL(&(g_videoIn->R4erBuffer.Pframe));
    PdYuvImage_NULL(&(g_videoIn->R4erBuffer.deltaB));

    if(t->alloc_pdist){g_videoIn->R16thBuffer.Cs        =    (Ipp64f*) NgvMalloc(sizeof(Ipp64f) * inData->R16th.Yframe / RsCsSIZE);}
    if(t->alloc_pdist){g_videoIn->R16thBuffer.Rs        =    (Ipp64f*) NgvMalloc(sizeof(Ipp64f) * inData->R16th.Yframe / RsCsSIZE);}

    g_videoIn->R16thBuffer.bSval    =    0;
    g_videoIn->R16thBuffer.CsVal    =    -1.0;
    g_videoIn->R16thBuffer.RsVal    =    -1.0;
    g_videoIn->R16thBuffer.DiffCsVal    =    -1.0;
    g_videoIn->R16thBuffer.DiffRsVal    =    -1.0;
    if(t->alloc_pdist)    {g_videoIn->R16thBuffer.DiffCs        =    (Ipp64f*) NgvMalloc(sizeof(Ipp64f) * inData->R16th.Yframe / RsCsSIZE);}
    if(t->alloc_pdist)    {g_videoIn->R16thBuffer.DiffRs        =    (Ipp64f*) NgvMalloc(sizeof(Ipp64f) * inData->R16th.Yframe / RsCsSIZE);}

    t->alloc_pdist = 0;

    *videoIn = *g_videoIn;
    memset(videoIn->R1Buffer.BSAD,0,sizeof(Ipp32u) * inData->R1.MVspaceSize);
    memset(videoIn->R4erBuffer.BSAD,0,sizeof(Ipp32u) * inData->R4er.MVspaceSize);
    memset(videoIn->R16thBuffer.BSAD,0,sizeof(Ipp32u) * inData->R16th.MVspaceSize);
    return failed;
}

NGV_Bool    PdYuvFileReader_Open(VidRead *videoIn, VidData *inData)        {
    NGV_Bool    failed                    =    FALSE;    

    /*Half Pixel Accuracy variables*/
    videoIn->cornerBox.Y            =    (Ipp8u*) NgvMalloc_aligned(16 * inData->R1.block_size_w,16);
    videoIn->topBox.Y                =    (Ipp8u*) NgvMalloc_aligned(16 * inData->R1.block_size_w,16);
    videoIn->leftBox.Y                =    (Ipp8u*) NgvMalloc_aligned(16 * inData->R1.block_size_w,16);


    /*Quarter Pixel Accuracy variables*/
    {
        int i;
        for(i=0;i<8;i++)    {
            videoIn->spBuffer[i].Y  =   (Ipp8u*) NgvMalloc_aligned(8 * inData->R1.block_size_w,16);
            memset(videoIn->spBuffer[i].Y,0,8 * inData->R1.block_size_w);
        }
        for(i=0;i<(MAXPDIST+1+MAX_LOOKAHEAD);i++)
            videoIn->logic[i].repeatedFrame    =    FALSE;
    }
    videoIn->bufferH                =   (Ipp8u*) NgvMalloc(sizeof(Ipp8u) * (inData->R1.width >> 1) * inData->R1.height);
    videoIn->bufferQ                =   (Ipp8u*) NgvMalloc(sizeof(Ipp8u) * (inData->R1.width >> 2) * inData->R1.height);

    return failed;
}

NGV_Bool VidSampleFree(void *vt, VidSample *videoIn)
{
    NGV_PDist *t = (NGV_PDist *)vt;
    _aligned_free(videoIn->R1Buffer.exImage.Y);
    _aligned_free(videoIn->R4erBuffer.exImage.Y);
    _aligned_free(videoIn->R16thBuffer.exImage.Y);

    free(videoIn->R1Buffer.Cs);
    free(videoIn->R1Buffer.Rs);

    /*Full Size image*/
    if(t->dealloc_pdist){
        _aligned_free(videoIn->R1Buffer.Backward.Y);
        //_aligned_free(videoIn->R1Buffer.Pframe.Y);
        // _aligned_free(videoIn->R1Buffer.deltaB.Y);
        //_aligned_free(videoIn->R1Buffer.Pframe.U);
        // _aligned_free(videoIn->R1Buffer.deltaB.U);
        //_aligned_free(videoIn->R1Buffer.Pframe.V);
        //_aligned_free(videoIn->R1Buffer.deltaB.V);
        _aligned_free(videoIn->R1Buffer.deltaF.Y);
        _aligned_free(videoIn->R1Buffer.deltaF.U);
        _aligned_free(videoIn->R1Buffer.deltaF.V);
        free(videoIn->R1Buffer.BSAD);
        free(videoIn->R1Buffer.pBackward);
        free(videoIn->R1Buffer.pBhalf);
        free(videoIn->R1Buffer.pBquarter);
    }
    if(t->dealloc_pdist)
    {
        free(videoIn->R1Buffer.DiffCs);
        free(videoIn->R1Buffer.DiffRs);
    }
    /*Quarter sized image*/

    if(t->dealloc_pdist){
        _aligned_free(videoIn->R4erBuffer.Backward.Y);
        // _aligned_free(videoIn->R4erBuffer.Pframe.Y);
        // _aligned_free(videoIn->R4erBuffer.deltaB.Y);
        //_aligned_free(videoIn->R4erBuffer.Pframe.U);
        // _aligned_free(videoIn->R4erBuffer.deltaB.U);
        //_aligned_free(videoIn->R4erBuffer.Pframe.V);
        // _aligned_free(videoIn->R4erBuffer.deltaB.V);
        _aligned_free(videoIn->R4erBuffer.deltaF.Y);
        _aligned_free(videoIn->R4erBuffer.deltaF.U);
        _aligned_free(videoIn->R4erBuffer.deltaF.V);

        free(videoIn->R4erBuffer.BSAD);
        free(videoIn->R4erBuffer.pBackward);
        free(videoIn->R4erBuffer.pBhalf);
        free(videoIn->R4erBuffer.pBquarter);
        free(videoIn->R4erBuffer.Cs);
        free(videoIn->R4erBuffer.Rs);
    }
    if(t->dealloc_pdist)
    {
        free(videoIn->R4erBuffer.DiffCs);
        free(videoIn->R4erBuffer.DiffRs);
    }
    /*1/16 sized image*/

    if(t->dealloc_pdist){
        _aligned_free(videoIn->R16thBuffer.Backward.Y);
        //_aligned_free(videoIn->R16thBuffer.Pframe.Y);
        //_aligned_free(videoIn->R16thBuffer.deltaB.Y);
        // _aligned_free(videoIn->R16thBuffer.Pframe.U);
        //_aligned_free(videoIn->R16thBuffer.deltaB.U);
        // _aligned_free(videoIn->R16thBuffer.Pframe.V);
        // _aligned_free(videoIn->R16thBuffer.deltaB.V);
        _aligned_free(videoIn->R16thBuffer.deltaF.Y);
        _aligned_free(videoIn->R16thBuffer.deltaF.U);
        _aligned_free(videoIn->R16thBuffer.deltaF.V);

        free(videoIn->R16thBuffer.BSAD);
        free(videoIn->R16thBuffer.pBackward);
        free(videoIn->R16thBuffer.pBhalf);
        free(videoIn->R16thBuffer.pBquarter);
        free(videoIn->R16thBuffer.Cs);
        free(videoIn->R16thBuffer.Rs);
    }
    if(t->dealloc_pdist)
    {    
        if(videoIn->R16thBuffer.DiffCs) free(videoIn->R16thBuffer.DiffCs);
        videoIn->R16thBuffer.DiffCs = NULL;
        if(videoIn->R16thBuffer.DiffRs) free(videoIn->R16thBuffer.DiffRs);
        videoIn->R16thBuffer.DiffRs = NULL;
    }
    t->dealloc_pdist = 0;

    return FALSE;
}


NGV_Bool PdYuvFileReader_Free(VidRead *videoIn)    {
    NGV_Bool    failed                    =    FALSE;
    Ipp32u        i;

    /*Half Pixel Accuracy variables*/
    if(videoIn->cornerBox.Y)
        _aligned_free(videoIn->cornerBox.Y);
    videoIn->cornerBox.Y = NULL;
    if(videoIn->topBox.Y) 
        _aligned_free(videoIn->topBox.Y);
    videoIn->topBox.Y = NULL;
    if(videoIn->leftBox.Y)
        _aligned_free(videoIn->leftBox.Y);
    videoIn->leftBox.Y = NULL;

    /*Quarter Pixel Accuracy variables*/
    for(i=0;i<8;i++)                {
        if(videoIn->spBuffer[i].Y)
            _aligned_free(videoIn->spBuffer[i].Y);
        videoIn->spBuffer[i].Y = NULL;
    }

    if(videoIn->bufferH) free(videoIn->bufferH);
    videoIn->bufferH = NULL;
    if(videoIn->bufferQ) free(videoIn->bufferQ);
    videoIn->bufferQ = NULL;

    return failed;
}


Ipp32u        logBase2aligned(Ipp32u number)
{
    Ipp32u        data;
    data                            =    (int)(ceil((log((float)number) / LN2)));
    data                            =    (int)pow((float)2,(float)data);
    return data;
}

NGV_Bool    PdYuvImage_Alloc(YUV *pImage, Ipp32u dimVideoExtended)
{
    NGV_Bool    failed                    =    FALSE;
    Ipp32u        dimAligned;

    pImage->U                        =    NULL;
    pImage->V                        =    NULL;
    //dimAligned                        =    logBase2aligned(dimVideoExtended);
    dimAligned                        =    dimVideoExtended;
    //if(!pImage->Y)
    pImage->Y                    =    (Ipp8u*) NgvMalloc_aligned(dimAligned,16);    
    if(!pImage->Y)
        failed                        =    TRUE;
    if(!failed)
        memset(pImage->Y, 0, dimVideoExtended);
    return failed;
}

NGV_Bool    PdYuvImage_Alloc2(YUV *pImage, Ipp32u dimVideoExtended, Ipp32u dimColorExtended)
{
    NGV_Bool    failed                    =    FALSE;
    Ipp32u        dimAligned;

    //dimAligned                        =    logBase2aligned(dimVideoExtended);
    dimAligned                        =    dimVideoExtended;
    if(!pImage->Y)
        pImage->Y                    =    (Ipp8u*) NgvMalloc_aligned(dimAligned,16);    
    if(!pImage->Y)
        failed                        =    TRUE;
    if(!failed)
        memset(pImage->Y, 0, dimVideoExtended);

    //dimAligned                        =    logBase2aligned(dimColorExtended);
    dimAligned                        =    dimColorExtended;
    if(!pImage->U && !pImage->V)    {
        pImage->U                    =    (Ipp8u*) NgvMalloc_aligned(dimAligned,16);
        pImage->V                    =    (Ipp8u*) NgvMalloc_aligned(dimAligned,16);
    }
    if(!pImage->U || !pImage->V)
        failed                        =    TRUE;
    if(!failed)                        {
        memset(pImage->U, 128, dimColorExtended);
        memset(pImage->V, 128, dimColorExtended);
    }
    return failed;
}

void    PdYuvImage_NULL(YUV *pImage)
{
    pImage->Y                    =    NULL;
    pImage->U                    =    NULL;
    pImage->V                    =    NULL;
}

Ipp32u        fileSize(FILE *fin)
{
    Ipp32u EndPos;

    fseek(fin,0,SEEK_END);
    EndPos                            =    ftell(fin);
    fseek(fin,0,SEEK_SET);
    return(EndPos);
}

Ipp32u        numFrames(Ipp32u EndPos, Ipp32u YUVframeSize)        {
    Ipp32u            totalNumFrames;

    totalNumFrames                    =    EndPos / YUVframeSize;
    return totalNumFrames;
}

NGV_Bool    PdYuvFileWriter_Open(DataWriter *videoOut, const char *fileName)    {
    NGV_Bool    failed                    =    FALSE;
    int        newSize; 

    if(!fileName) {
        failed                        =    TRUE;
        return failed;
    }

    newSize                            =    (int)strlen(fileName)+1;
    //AYA - disabled
    //videoOut->filename                =    NgvMalloc(newSize);
    if(!videoOut->filename) {
        failed                        =    TRUE;
        return failed;
    }
    strcpy_s(videoOut->filename, newSize, fileName); /* Flawfinder: ignore */

    fopen_s(&videoOut->pDataOut, videoOut->filename, "wb+");
    if(!videoOut->pDataOut) {
        failed                        =    TRUE;
        return failed;
    }

    return failed;
}

NGV_Bool    MvFileWriter_Open(DataWriter *mvOut, const char *fileName)    {
    NGV_Bool    failed                    =    FALSE;
    int        newSize;

    if(!fileName) {
        failed                        =    TRUE;
        return failed;
    }

    newSize                            =    (int)strlen(fileName)+1;
    // aya - disabled
    //mvOut->filename                    =    NgvMalloc(newSize);
    if(!mvOut->filename) {
        failed                        =    TRUE;
        return failed;
    }
    strcpy_s(mvOut->filename, newSize, fileName); /* Flawfinder: ignore */

    fopen_s(&mvOut->pDataOut, mvOut->filename, "w+");
    if(!mvOut->pDataOut) {
        failed = TRUE;
        return failed;
    }

    return failed;
}

void    MvFileWriter_Close(DataWriter *mvOut)    {

    if(mvOut->pDataOut) fclose(mvOut->pDataOut);
    if(mvOut->filename) free(mvOut->filename);
    mvOut->filename = 0;
}

/**
*******************************************************************************
* \brief Write Image routine
*
*
* \param videoOut   [IN] -  Input data
* \param initPoint  [IN] -  Initial offset
* \param width      [IN] -  Width
* \param height     [IN] -  Height
* \param height     [IN] -  stride
* \param height     [OUT] - YImage
* 
* \return TRUE or FALSE
*/
NGV_Bool    WriteImageYOnly(DataWriter *videoOut, Ipp32u initPoint, Ipp32u width, Ipp32u height, Ipp32u stride, Ipp8u *YImage)        {
    NGV_Bool    failed                    =    FALSE;    
    Ipp32u        i;
    Ipp8u        *ss;
    Ipp8u        *color                    =    NULL;

    color                            =    (Ipp8u*)NgvMalloc(width/2);

    // Error checking
    if (color == NULL)
    {
        VM_ASSERT(0);
        NgvPrintf("Error WriteImageYOnly: at line number %d in file %s\n", __LINE__, __FILE__);
        return TRUE;
    }
    memset(color,128,width/2);

    ss                                =    YImage;
    ss                                +=    initPoint;
    for(i=0;i<height;i++)    {
        if(fwrite(ss, width, 1, videoOut->pDataOut) != 1)
        {
            failed = TRUE;
        }
        ss                            +=    stride;
    }
    for(i=0;i<height;i++)    {
        if(fwrite(color, width/2, 1, videoOut->pDataOut) != 1)
        {
            failed = TRUE;
        }
    }

    free(color);
    return failed;
}

void    refCurRotation(Ipp8u* tImageLoc, Ipp8u* *YCur, Ipp8u* *YRef)        {
    tImageLoc                        =    *YRef;
    *YRef                            =    *YCur;
    *YCur                            =    tImageLoc;
}

void    extendBorders(VidRead * /*videoIn*/, VidData *inData, Ipp8u *frameToextend)
{
    Ipp8u* bRow; 
    Ipp8u* tRow;
    Ipp32u        row, col;
    Ipp8u        uLeftFillVal;
    Ipp8u        uRightFillVal;
    Ipp8u        *pByteSrc, *pSrcLeft, *pSrcRight;

    // section 1 at bottom
    // obtain pointer to start of bottom row of original frame
    //if((Ipp32u)StartPtr %4) NgvPrintf("Not 4 Byte aligned");
    pByteSrc                        =    frameToextend + inData->R16th.endPoint;
    bRow                            =    pByteSrc - inData->R16th.exwidth;
    for(row=0; row<(inData->R16th.block_size_h + 3); row++)    {
        for(col=0;col<inData->R16th.width;col++)
            *(pByteSrc + col)        =    *(bRow + col);
        pByteSrc                    =    pByteSrc + inData->R16th.exwidth;
    }

    // section 2 on left and right
    // obtain pointer to start of first row of original frame
    pByteSrc                        =    frameToextend + inData->R16th.initPoint;
    for(row=0; row<inData->R16th.sidesize; row++, pByteSrc += inData->R16th.exwidth)    {
        // get fill values from left and right columns of original frame
        uLeftFillVal                =    *pByteSrc;
        pSrcLeft                    =    pByteSrc - (inData->R16th.block_size_w + 3);
        pSrcRight                    =    pByteSrc + inData->R16th.width;
        uRightFillVal                =    *(pSrcRight - 1);

        // fill all bytes on both edges
        for(col=0; col<inData->R16th.block_size_w + 3; col++)    {
            *(pSrcLeft + col)        =    uLeftFillVal;
            *(pSrcRight + col)        =    uRightFillVal;
        }
    }

    // section 3 at top
    // obtain pointer to top row of original frame, less expand pels
    pByteSrc                        =    frameToextend + inData->R16th.initPoint - (inData->R16th.block_size_w + 3);
    tRow                            =    frameToextend;

    for(row=0; row<(inData->R16th.block_size_h+3); row++, tRow += inData->R16th.exwidth)
        for(col=0; col<inData->R16th.exwidth; col++)
            *(tRow + col)            =    *(pByteSrc + col);
}

void    extendBorders2(VidRead * /*videoIn*/, VidData *inData, Ipp8u *frameToextend)
{
    Ipp8u*        bRow;
    Ipp8u*        tRow;
    Ipp32u        row, col;
    Ipp8u        uLeftFillVal;
    Ipp8u        uRightFillVal;
    Ipp8u        *pByteSrc, *pSrcLeft, *pSrcRight;

    // section 1 at bottom
    // obtain pointer to start of bottom row of original frame
    //if((Ipp32u)StartPtr %4) NgvPrintf("Not 4 Byte aligned");
    pByteSrc                        =    frameToextend + inData->R1.endPoint;
    bRow                            =    pByteSrc - inData->R1.exwidth;
    for(row=0; row<(inData->R1.block_size_h + 3); row++)    {
        for(col=0;col<inData->R1.width;col++)
            *(pByteSrc + col)        =    *(bRow + col);
        pByteSrc                    =    pByteSrc + inData->R1.exwidth;
    }

    // section 2 on left and right
    // obtain pointer to start of first row of original frame
    pByteSrc                        =    frameToextend + inData->R1.initPoint;
    for(row=0; row<inData->R1.sidesize; row++, pByteSrc += inData->R1.exwidth)    {
        // get fill values from left and right columns of original frame
        uLeftFillVal                =    *pByteSrc;
        pSrcLeft                    =    pByteSrc - (inData->R1.block_size_w + 3);
        pSrcRight                    =    pByteSrc + inData->R1.width;
        uRightFillVal                =    *(pSrcRight - 1);

        // fill all bytes on both edges
        for(col=0; col<(inData->R1.block_size_w + 3); col++)    {
            *(pSrcLeft + col)        =    uLeftFillVal;
            *(pSrcRight + col)        =    uRightFillVal;
        }
    }

    // section 3 at top
    // obtain pointer to top row of original frame, less expand pels
    pByteSrc                        =    frameToextend + inData->R1.initPoint - (inData->R1.block_size_w + 3);
    tRow                            =    frameToextend;

    for(row=0; row<(inData->R1.block_size_h + 3); row++, tRow += inData->R1.exwidth)
        for(col=0; col<inData->R1.exwidth; col++)
            *(tRow + col)            =    *(pByteSrc + col);
}

void    extendBorders3(VidRead * /*videoIn*/, VidData *inData, Ipp8u *frameToextend)
{
    Ipp8u* bRow;
    Ipp8u* tRow;
    Ipp32u        row, col;
    Ipp8u        uLeftFillVal;
    Ipp8u        uRightFillVal;
    Ipp8u        *pByteSrc, *pSrcLeft, *pSrcRight;

    // section 1 at bottom
    // obtain pointer to start of bottom row of original frame
    //if((Ipp32u)StartPtr %4) NgvPrintf("Not 4 Byte aligned");
    pByteSrc                        =    frameToextend + inData->R4er.endPoint;
    bRow                            =    pByteSrc - inData->R4er.exwidth;
    for(row=0; row<(inData->R4er.block_size_h + 3); row++)    {
        for(col=0;col<inData->R4er.width;col++)
            *(pByteSrc + col)        =    *(bRow + col);
        pByteSrc                    =    pByteSrc + inData->R4er.exwidth;
    }

    // section 2 on left and right
    // obtain pointer to start of first row of original frame
    pByteSrc                        =    frameToextend + inData->R4er.initPoint;
    for(row=0; row<inData->R4er.sidesize; row++, pByteSrc += inData->R4er.exwidth)    {
        // get fill values from left and right columns of original frame
        uLeftFillVal                =    *pByteSrc;
        pSrcLeft                    =    pByteSrc - (inData->R4er.block_size_w + 3);
        pSrcRight                    =    pByteSrc + inData->R4er.width;
        uRightFillVal                =    *(pSrcRight - 1);

        // fill all bytes on both edges
        for(col=0; col<inData->R4er.block_size_w  + 3; col++)    {
            *(pSrcLeft + col)        =    uLeftFillVal;
            *(pSrcRight + col)        =    uRightFillVal;
        }
    }

    // section 3 at top
    // obtain pointer to top row of original frame, less expand pels
    pByteSrc                        =    frameToextend + inData->R4er.initPoint - (inData->R4er.block_size_w + 3);
    tRow                            =    frameToextend;

    for(row=0; row<(inData->R4er.block_size_h+3); row++, tRow += inData->R4er.exwidth)
        for(col=0; col<inData->R4er.exwidth; col++)
            *(tRow + col)            =    *(pByteSrc + col);
}

void    initHistogram(VidRead *videoIn)    {
    videoIn->average                =    0;
    videoIn->avgSAD                    =    0;
    videoIn->longerMV                =    0;
}


void    RsCsCalc(imageData *exBuffer, ImDetails vidCar)            {
    Ipp8u        *ss, *init;
    Ipp32s        i,j,k,l;
    Ipp32s        locx, locy, loc, loc2;
    Ipp32s        hblocks, wblocks, bjump;
    Ipp64f        temp, temp2;

    ss                            =    exBuffer->exImage.Y + vidCar.initPoint;
    hblocks                        =    (Ipp32s)vidCar.height / BLOCK_SIZE;
    wblocks                        =    (Ipp32s)vidCar.width / BLOCK_SIZE;
    bjump                        =    (Ipp32s)vidCar.exwidth * BLOCK_SIZE;
    exBuffer->CsVal                =    0.0;
    exBuffer->RsVal                =    0.0;

    for(i=0;i<hblocks;i++)        {
        locy                    =    i * wblocks;
        for(j=0;j<wblocks;j++)    {
            locx                =    locy + j;
            exBuffer->Cs[locx]    =    0.0;
            exBuffer->Rs[locx]    =    0.0;
            for(k=0;k<BLOCK_SIZE;k++)                        {
                for(l=0;l<BLOCK_SIZE;l++)                    {
                    loc2        =    (i * bjump) + (j * BLOCK_SIZE) + l;
                    loc            =    loc2 + (k * vidCar.exwidth);
                    init        =    ss + loc;
                    if((l + (j * BLOCK_SIZE)) == 0)
                        temp    =    0;
                    else
                        temp    =    (Ipp64f)*(init) - (Ipp64f)*(init - 1);
                    temp        *=    temp;
                    exBuffer->CsVal        +=    temp;

                    if((k + (i *BLOCK_SIZE)) == 0)
                        temp2    =    0;
                    else
                        temp2    =    (Ipp64f)*(init) - (Ipp64f)*(ss + loc2 + ((k - 1) * (signed int)vidCar.exwidth));  /* signed int cast for 64-bit */
                    temp2        *=    temp2;
                    exBuffer->RsVal        +=    temp2;

                    exBuffer->Cs[locx]    +=    temp;
                    exBuffer->Rs[locx]    +=    temp2;
                }
            }
            exBuffer->Cs[locx]    /=    RsCsSIZE;
            exBuffer->Cs[locx]    =    sqrt(exBuffer->Cs[locx]);
            exBuffer->Rs[locx]    /=    RsCsSIZE;
            exBuffer->Rs[locx]    =    sqrt(exBuffer->Rs[locx]);
        }
    }
    exBuffer->CsVal                /=    hblocks * wblocks * RsCsSIZE;
    exBuffer->RsVal                /=    hblocks * wblocks * RsCsSIZE;
    exBuffer->CsVal                =    sqrt(exBuffer->CsVal);
    exBuffer->RsVal                =    sqrt(exBuffer->RsVal);
}

void    RsCsCalc2(imageData *exBuffer, ImDetails vidCar)            {
    Ipp8u        *ss, *init;
    Ipp32s        i,j,k,l;
    Ipp32s        locx, locy, loc, loc2;
    Ipp32s        hblocks, wblocks, bjump;
    Ipp64f        temp, temp2;

    ss                            =    exBuffer->deltaF.Y;
    hblocks                        =    (Ipp32s)vidCar.height / BLOCK_SIZE;
    wblocks                        =    (Ipp32s)vidCar.width / BLOCK_SIZE;
    bjump                        =    (Ipp32s)vidCar.width * BLOCK_SIZE;
    exBuffer->DiffCsVal            =    0.0;
    exBuffer->DiffRsVal            =    0.0;

    for(i=0;i<hblocks;i++)        {
        locy                    =    i * wblocks;
        for(j=0;j<wblocks;j++)    {
            locx                =    locy + j;
            exBuffer->DiffCs[locx]    =    0.0;
            exBuffer->DiffRs[locx]    =    0.0;
            for(k=0;k<BLOCK_SIZE;k++)                        {
                for(l=0;l<BLOCK_SIZE;l++)                    {
                    loc2        =    (i * bjump) + (j * BLOCK_SIZE) + l;
                    loc            =    loc2 + (k * vidCar.width);
                    init        =    ss + loc;
                    if((l + (j * BLOCK_SIZE)) == 0 || (l + (j * BLOCK_SIZE)) == vidCar.width - 1)  /* Note: == signed/unsigned mismatch */
                        temp    =    0;
                    else
                        temp    =    (Ipp64f)*(init) - (Ipp64f)*(init - 1);
                    exBuffer->DiffCsVal        +=    abs(temp);
                    temp        *=    temp;
                    if((k + (i * BLOCK_SIZE)) == 0 || (k + (i * BLOCK_SIZE)) == vidCar.height - 1) /* Note: == signed/unsigned mismatch */
                        temp2    =    0;
                    else
                        temp2    =    (Ipp64f)*(init) - (Ipp64f)*(ss + loc2 + ((k - 1) * (signed int)vidCar.exwidth));  /* signed int cast for 64-bit */
                    exBuffer->DiffRsVal        +=    abs(temp2);
                    temp2        *=    temp2;
                    exBuffer->DiffCs[locx]    +=    temp;
                    exBuffer->DiffRs[locx]    +=    temp2;
                }
            }
            exBuffer->DiffCs[locx]    /=    RsCsSIZE;
            exBuffer->DiffCs[locx]    =    sqrt(exBuffer->DiffCs[locx]);
            exBuffer->DiffRs[locx]    /=    RsCsSIZE;
            exBuffer->DiffRs[locx]    =    sqrt(exBuffer->DiffRs[locx]);
        }
    }
    exBuffer->DiffCsVal            /=    hblocks * wblocks * RsCsSIZE;
    exBuffer->DiffRsVal            /=    hblocks * wblocks * RsCsSIZE;
    exBuffer->DiffCsVal            =    sqrt(exBuffer->DiffCsVal);
    exBuffer->DiffRsVal            =    sqrt(exBuffer->DiffRsVal);
}

void    deltaIm(Ipp8u* dd, Ipp8u* ss1, Ipp8u* ss2, Ipp32u widthd, Ipp32u widths, ImDetails ImRes)        {
    Ipp32s        i,j,a,result;
    Ipp32s        ppos1,ppos2,pos2;

    for(i=0;i<(Ipp32s)ImRes.height;i++)    {
        ppos1                        =    i * widthd;
        ppos2                        =    i * widths;
        for(j=0;j<(Ipp32s)ImRes.width;j++){
            pos2                    =    ppos2 + j;
            a                        =    ((j<1)?0:1);
            result                    =    (((Ipp32s)ss1[pos2] - (Ipp32s)ss1[pos2-1]) - ((Ipp32s)ss2[pos2] - (Ipp32s)ss2[pos2-a])) + 128;
            if(result>255)
                result                =    255;
            else if(result<0)
                result                =    0;
            dd[ppos1 + j]            =    (Ipp8u)result;
        }
    }
}

void    deltaIm2(Ipp8u* dd, Ipp8u* ss1, Ipp8u* ss2, Ipp32u widthd, Ipp32u widths, ImDetails ImRes)        {
    Ipp32s        i,j,a,result;
    Ipp32s        ppos1,ppos2,pos2;
    Ipp32s        height, width;
    height                            =    (Ipp32s)ImRes.height/2;
    width                            =    (Ipp32s)ImRes.width/2;

    for(i=0;i<height;i++)    {
        ppos1                        =    i * widthd;
        ppos2                        =    i * widths;
        for(j=0;j<width;j++){
            pos2                    =    ppos2 + j;
            a                        =    ((j<1)?0:1);
            if(j == 0)
                result                =    0;
            else
                result                =    (((Ipp32s)ss1[pos2] - (Ipp32s)ss1[pos2-1]) - ((Ipp32s)ss2[pos2] - (Ipp32s)ss2[pos2-a]));// + 128;
            //result                    =    ((Ipp32s)ss1[pos2] - (Ipp32s)ss2[pos2]);
            //result                    =    (((Ipp32s)ss1[pos2] - (Ipp32s)ss2[pos2])) + 128;
            //result                    =    (((Ipp32s)ss1[pos2] - (Ipp32s)ss2[pos2])>>3) + 128;
            result                    =    abs(result);
            if(result>255)
                result                =    255;
            else if(result<0)
                result                =    0;

            //dd[ppos1 + j]            =    (Ipp8u)NABS(result);
            dd[ppos1 + j]            =    (Ipp8u)result;
        }
    }
}

//*********************************************************
//   clip table
//********************************************************

const Ipp8u ClampTbl[CLIP_RANGE] =
{
    0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00  // 0
    ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 // 1
    ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 // 2
    ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 // 3
    ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 // 4
    ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 // 5
    ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 // 6
    ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 // 7
    ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 // 8
    ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 // 9
    ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 // 10
    ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 // 11
    ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 // 12
    ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 // 13
    ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 // 14
    ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 // 15
    ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 // 16
    ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 // 17
    ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00, //18
     0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 // 19
    ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 // 20
    ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 // 21
    ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 // 22
    ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 // 23
    ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 // 24
    ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 // 25
    ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 // 26
    ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 // 27
    ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 // 28
    ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 // 29
    ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 // 30
    ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 // 31
    ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 // 32
    ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 // 33
    ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 // 34
    ,0x00 ,0x01 ,0x02 ,0x03 ,0x04 ,0x05 ,0x06 ,0x07
    ,0x08 ,0x09 ,0x0a ,0x0b ,0x0c ,0x0d ,0x0e ,0x0f
    ,0x10 ,0x11 ,0x12 ,0x13 ,0x14 ,0x15 ,0x16 ,0x17
    ,0x18 ,0x19 ,0x1a ,0x1b ,0x1c ,0x1d ,0x1e ,0x1f
    ,0x20 ,0x21 ,0x22 ,0x23 ,0x24 ,0x25 ,0x26 ,0x27
    ,0x28 ,0x29 ,0x2a ,0x2b ,0x2c ,0x2d ,0x2e ,0x2f
    ,0x30 ,0x31 ,0x32 ,0x33 ,0x34 ,0x35 ,0x36 ,0x37
    ,0x38 ,0x39 ,0x3a ,0x3b ,0x3c ,0x3d ,0x3e ,0x3f
    ,0x40 ,0x41 ,0x42 ,0x43 ,0x44 ,0x45 ,0x46 ,0x47
    ,0x48 ,0x49 ,0x4a ,0x4b ,0x4c ,0x4d ,0x4e ,0x4f
    ,0x50 ,0x51 ,0x52 ,0x53 ,0x54 ,0x55 ,0x56 ,0x57
    ,0x58 ,0x59 ,0x5a ,0x5b ,0x5c ,0x5d ,0x5e ,0x5f
    ,0x60 ,0x61 ,0x62 ,0x63 ,0x64 ,0x65 ,0x66 ,0x67
    ,0x68 ,0x69 ,0x6a ,0x6b ,0x6c ,0x6d ,0x6e ,0x6f
    ,0x70 ,0x71 ,0x72 ,0x73 ,0x74 ,0x75 ,0x76 ,0x77
    ,0x78 ,0x79 ,0x7a ,0x7b ,0x7c ,0x7d ,0x7e ,0x7f
    ,0x80 ,0x81 ,0x82 ,0x83 ,0x84 ,0x85 ,0x86 ,0x87
    ,0x88 ,0x89 ,0x8a ,0x8b ,0x8c ,0x8d ,0x8e ,0x8f
    ,0x90 ,0x91 ,0x92 ,0x93 ,0x94 ,0x95 ,0x96 ,0x97
    ,0x98 ,0x99 ,0x9a ,0x9b ,0x9c ,0x9d ,0x9e ,0x9f
    ,0xa0 ,0xa1 ,0xa2 ,0xa3 ,0xa4 ,0xa5 ,0xa6 ,0xa7
    ,0xa8 ,0xa9 ,0xaa ,0xab ,0xac ,0xad ,0xae ,0xaf
    ,0xb0 ,0xb1 ,0xb2 ,0xb3 ,0xb4 ,0xb5 ,0xb6 ,0xb7
    ,0xb8 ,0xb9 ,0xba ,0xbb ,0xbc ,0xbd ,0xbe ,0xbf
    ,0xc0 ,0xc1 ,0xc2 ,0xc3 ,0xc4 ,0xc5 ,0xc6 ,0xc7
    ,0xc8 ,0xc9 ,0xca ,0xcb ,0xcc ,0xcd ,0xce ,0xcf
    ,0xd0 ,0xd1 ,0xd2 ,0xd3 ,0xd4 ,0xd5 ,0xd6 ,0xd7
    ,0xd8 ,0xd9 ,0xda ,0xdb ,0xdc ,0xdd ,0xde ,0xdf
    ,0xe0 ,0xe1 ,0xe2 ,0xe3 ,0xe4 ,0xe5 ,0xe6 ,0xe7
    ,0xe8 ,0xe9 ,0xea ,0xeb ,0xec ,0xed ,0xee ,0xef
    ,0xf0 ,0xf1 ,0xf2 ,0xf3 ,0xf4 ,0xf5 ,0xf6 ,0xf7
    ,0xf8 ,0xf9 ,0xfa ,0xfb ,0xfc ,0xfd ,0xfe ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
    ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff ,0xff
};

// -----------------AdaptQp--------------------------------
#define NEW_CONSTS
#ifdef NEW_CONSTS
const double LQ_M[5][8]   = {
    {4.2415, 3.9818, 3.9818, 3.9818, 4.0684, 4.0684, 4.0684, 4.0684},   // I
    {4.5878, 4.5878, 4.5878, 4.5878, 4.5878, 4.2005, 4.2005, 4.2005},   // P
    {4.3255, 4.3255, 4.3255, 4.3255, 4.3255, 4.3255, 4.3255, 4.3255},   // B1
    {4.4052, 4.4052, 4.4052, 4.4052, 4.4052, 4.4052, 4.4052, 4.4052},   // B2
    {4.2005, 4.2005, 4.2005, 4.2005, 4.2005, 4.2005, 4.2005, 4.2005}    // B3
};
const double LQ_K[5][8] = {
    {12.8114, 13.8536, 13.8536, 13.8536, 13.8395, 13.8395, 13.8395, 13.8395},   // I
    {12.3857, 12.3857, 12.3857, 12.3857, 12.3857, 13.7122, 13.7122, 13.7122},   // P
    {13.7286, 13.7286, 13.7286, 13.7286, 13.7286, 13.7286, 13.7286, 13.7286},   // B1
    {13.1463, 13.1463, 13.1463, 13.1463, 13.1463, 13.1463, 13.1463, 13.1463},   // B2 
    {13.7122, 13.7122, 13.7122, 13.7122, 13.7122, 13.7122, 13.7122, 13.7122}    // B3
};
const double LQ_M16[5][8]   = {
    {4.3281, 3.9818, 3.9818, 3.9818, 4.0684, 4.0684, 4.0684, 4.3281},   // I
    {4.5878, 4.5878, 4.5878, 4.5878, 4.5878, 4.3281, 4.3281, 4.3281},   // P
    {4.3255, 4.3255, 4.3255, 4.3255, 4.3255, 4.3255, 4.3255, 4.3255},   // B1
    {4.4052, 4.4052, 4.4052, 4.4052, 4.4052, 4.4052, 4.4052, 4.4052},   // B2
    {4.2005, 4.2005, 4.2005, 4.2005, 4.2005, 4.2005, 4.2005, 4.2005}    // B3
};
const double LQ_K16[5][8] = {
    {14.4329, 14.8983, 14.8983, 14.8983, 14.9069, 14.9069, 14.9069, 14.4329},   // I
    {12.4456, 12.4456, 12.4456, 12.4456, 12.4456, 13.5336, 13.5336, 13.5336},   // P
    {13.7286, 13.7286, 13.7286, 13.7286, 13.7286, 13.7286, 13.7286, 13.7286},   // B1
    {13.1463, 13.1463, 13.1463, 13.1463, 13.1463, 13.1463, 13.1463, 13.1463},   // B2 
    {13.7122, 13.7122, 13.7122, 13.7122, 13.7122, 13.7122, 13.7122, 13.7122}    // B3
};
#else
const double LQ_M[5][8]   = {
    {4.2005, 4.2005, 4.2005, 4.2005, 4.2005, 4.2005, 4.2005, 4.2005},   // I
    {4.2005, 4.2005, 4.2005, 4.2005, 4.2005, 4.2005, 4.2005, 4.2005},   // P
    {4.2005, 4.2005, 4.2005, 4.2005, 4.2005, 4.2005, 4.2005, 4.2005},   // B1
    {4.2005, 4.2005, 4.2005, 4.2005, 4.2005, 4.2005, 4.2005, 4.2005},   // B2
    {4.2005, 4.2005, 4.2005, 4.2005, 4.2005, 4.2005, 4.2005, 4.2005}    // B3
};
const double LQ_K[5][8] = {
    {13.7122, 13.7122, 13.7122, 13.7122, 13.7122, 13.7122, 13.7122, 13.7122},   // I
    {13.7122, 13.7122, 13.7122, 13.7122, 13.7122, 13.7122, 13.7122, 13.7122},   // P
    {13.7122, 13.7122, 13.7122, 13.7122, 13.7122, 13.7122, 13.7122, 13.7122},   // B1
    {13.7122, 13.7122, 13.7122, 13.7122, 13.7122, 13.7122, 13.7122, 13.7122},   // B2 
    {13.7122, 13.7122, 13.7122, 13.7122, 13.7122, 13.7122, 13.7122, 13.7122}    // B3
};
#endif

const double CU_RSCS_TH[5][4][8] = {
{{  4.0,  6.0,  8.0, 11.0, 14.0, 18.0, 26.0,65025.0},{  4.0,  6.0,  8.0, 11.0, 14.0, 18.0, 26.0,65025.0},{  4.0,  6.0,  9.0, 11.0, 14.0, 18.0, 26.0,65025.0},{  4.0,  6.0,  9.0, 11.0, 14.0, 18.0, 26.0,65025.0}},
{{  5.0,  6.0,  8.0, 11.0, 14.0, 18.0, 24.0,65025.0},{  5.0,  7.0,  9.0, 11.0, 14.0, 18.0, 25.0,65025.0},{  5.0,  7.0,  9.0, 12.0, 15.0, 19.0, 25.0,65025.0},{  5.0,  7.0,  9.0, 12.0, 14.0, 18.0, 25.0,65025.0}},
{{  5.0,  7.0, 10.0, 12.0, 15.0, 19.0, 25.0,65025.0},{  5.0,  8.0, 10.0, 13.0, 15.0, 20.0, 26.0,65025.0},{  5.0,  8.0, 10.0, 12.0, 14.0, 18.0, 24.0,65025.0},{  5.0,  8.0, 10.0, 12.0, 15.0, 18.0, 24.0,65025.0}},
{{  6.0,  8.0, 10.0, 13.0, 16.0, 19.0, 25.0,65025.0},{  5.0,  8.0, 10.0, 13.0, 15.0, 19.0, 26.0,65025.0},{  5.0,  8.0, 10.0, 12.0, 15.0, 18.0, 24.0,65025.0},{  6.0,  9.0, 11.0, 13.0, 15.0, 19.0, 24.0,65025.0}},
{{  6.0,  9.0, 11.0, 14.0, 17.0, 21.0, 27.0,65025.0},{  6.0,  9.0, 11.0, 13.0, 16.0, 19.0, 25.0,65025.0},{  7.0,  9.0, 12.0, 14.0, 16.0, 19.0, 25.0,65025.0},{  7.0,  9.0, 11.0, 13.0, 16.0, 19.0, 24.0,65025.0}}
};


const int DQP_SEQ_RSCS[5][4][8][5] = {        // [PC][QC][RC][DQ]
{
{{0,1,2,3,4},{1,0,2,3,4},{1,0,2,3,4},{1,0,2,3,4},{1,0,2,3,4},{1,2,0,3,4},{1,2,0,3,4},{1,2,0,3,4}},
{{1,0,2,3,4},{1,0,2,3,4},{1,0,2,3,4},{1,0,2,3,4},{1,0,2,3,4},{1,0,2,3,4},{1,0,2,3,4},{1,2,0,3,4}},
{{1,0,2,3,4},{0,1,2,3,4},{0,1,2,3,4},{0,1,2,3,4},{1,0,2,3,4},{1,0,2,3,4},{1,0,2,3,4},{1,0,2,3,4}},
{{1,0,2,3,4},{0,1,2,3,4},{0,1,2,3,4},{0,1,2,3,4},{0,1,2,3,4},{0,1,2,3,4},{0,1,2,3,4},{1,0,2,3,4}}
},
{
{{1,2,0,3,4},{1,2,0,3,4},{1,2,0,3,4},{1,2,3,0,4},{1,2,3,0,4},{2,1,3,0,4},{2,1,3,0,4},{2,1,3,0,4}},
{{2,1,3,0,4},{2,1,3,0,4},{2,1,3,0,4},{2,1,3,0,4},{2,1,3,0,4},{2,1,3,0,4},{1,2,3,0,4},{2,1,3,0,4}},
{{2,1,3,0,4},{2,1,3,0,4},{2,1,3,0,4},{2,1,3,0,4},{2,1,3,0,4},{2,1,3,0,4},{2,1,3,0,4},{2,1,3,0,4}},
{{2,1,3,4,0},{2,1,3,0,4},{2,1,3,0,4},{2,1,3,0,4},{2,1,3,0,4},{2,1,3,0,4},{2,1,3,0,4},{2,1,3,0,4}}
},
{
{{4,3,5,2,6},{3,4,5,2,6},{3,4,5,2,6},{3,4,5,2,6},{3,4,5,2,6},{3,4,5,2,6},{3,4,5,2,6},{4,3,5,2,6}},
{{6,5,7,4,8},{6,5,7,4,8},{6,5,4,7,8},{6,5,4,7,8},{6,5,4,7,8},{6,5,4,7,8},{5,6,4,7,8},{5,6,4,7,8}},
{{7,6,8,5,4},{7,6,8,5,4},{7,6,8,5,4},{7,6,8,5,4},{7,6,8,5,4},{7,6,8,5,4},{7,6,8,5,4},{7,6,8,5,4}},
{{7,8,6,5,4},{7,8,6,5,4},{7,8,6,5,4},{7,8,6,5,4},{7,8,6,5,4},{7,8,6,5,4},{7,8,6,5,4},{7,8,6,5,4}}
},
{
{{4,5,3,6,2},{4,5,3,6,2},{4,5,3,6,2},{4,3,5,6,2},{4,3,5,6,2},{4,3,5,6,2},{4,3,5,6,2},{4,3,5,6,2}},
{{6,5,7,4,8},{6,5,7,4,8},{6,5,7,4,8},{6,5,7,4,8},{6,5,7,4,8},{6,5,7,4,8},{6,5,7,4,8},{6,5,7,4,8}},
{{7,6,8,5,4},{7,6,8,5,4},{7,6,8,5,4},{7,6,8,5,4},{7,6,8,5,4},{7,6,8,5,4},{7,6,8,5,4},{7,6,8,5,4}},
{{7,8,6,5,4},{7,8,6,5,4},{7,8,6,5,4},{7,8,6,5,4},{7,8,6,5,4},{7,8,6,5,4},{7,8,6,5,4},{7,8,6,5,4}}
},
{
{{7,8,6,5,4},{7,8,6,5,4},{7,8,6,5,4},{7,8,6,5,4},{7,8,6,5,4},{7,8,6,5,4},{7,8,6,5,4},{7,8,6,5,4}},
{{8,7,6,5,4},{8,7,6,5,4},{8,7,6,5,4},{8,7,6,5,4},{8,7,6,5,4},{8,7,6,5,4},{8,7,6,5,4},{8,7,6,5,4}},
{{8,7,6,5,4},{8,7,6,5,4},{8,7,6,5,4},{8,7,6,5,4},{8,7,6,5,4},{8,7,6,5,4},{8,7,6,5,4},{8,7,6,5,4}},
{{8,7,6,5,4},{8,7,6,5,4},{8,7,6,5,4},{8,7,6,5,4},{8,7,6,5,4},{8,7,6,5,4},{8,7,6,5,4},{8,7,4,6,5}}
}};


static int clip3(int minVal, int maxVal, int val)
{
    if(val < minVal)    return minVal;
    if(val > maxVal)    return maxVal;

    return val;
}


////////////////////////////////////////////////////////////////////////////////////////////////



void TAdapQP::setClass_RSCS(double dVal)
{
    int i, k;

    k = 7;
    for(i = 0; i < 8; i++)
    {
        if(dVal < CU_RSCS_TH[m_picClass][m_qpClass][i])
        {
            k = i;
            break;
        }
    }

    m_rscsClass = k;
}

void TAdapQP::setSliceQP(int iQP)            
{
    int pQPi[5][4] = {{22,27,32,37}, {23,28,33,38}, {24,29,34,39}, {25,30,35,40}, {26,31,36,41}};
//    int pThQP[5][4] = {{25,30,35,37}, {23,28,33,38}, {24,29,34,39}, {25,30,35,40}, {26,31,36,41}};

    m_sliceQP = iQP;
    if(iQP < pQPi[m_picClass][0])
        m_qpClass = 0;
    else if(iQP > pQPi[m_picClass][3])
        m_qpClass = 3;
    else
    {
        m_qpClass = (iQP - 22 - m_picClass) / 5;
    }
}

void TAdapQP::setQpBuffer_RSCS()
{
    for(int dqpIdx = 0; dqpIdx < 5; dqpIdx++)
    {
        m_qpBuffer[dqpIdx] = m_sliceQP + DQP_SEQ_RSCS[m_picClass][m_qpClass][m_rscsClass][dqpIdx] - 4;
    }
}

int TAdapQP::getDeltaQPFromBase(int dQPIdx)
{
    return DQP_SEQ_RSCS[m_picClass][m_qpClass][m_rscsClass][dQPIdx] - 4;
}

int TAdapQP::getQPFromLambda(double dLambda)
{
    double QP = m_sliceQP;
    if(m_GOPSize>8) {
      QP = LQ_M16[m_picClass][m_rscsClass]*log( dLambda ) + LQ_K16[m_picClass][m_rscsClass];
    } else {
      QP = LQ_M[m_picClass][m_rscsClass]*log( dLambda ) + LQ_K[m_picClass][m_rscsClass];
    }
    return int(QP + 0.5);
}

void TAdapQP::set_pic_coding_class(int iGOPId)
{
    if(m_SliceType == I_SLICE)
    {
        m_picClass = 0;   // I
    }
    else 
    {
        m_picClass = m_GOPID2Class[iGOPId];
    }
}

TAdapQP::TAdapQP()
{
    
}

TAdapQP::~TAdapQP()
{
    
}


/**
 \param    uiMaxWidth    largest CU width
 \param    uiMaxHeight   largest CU height
 */
void TAdapQP::create(Ipp32u uiMaxWidth, Ipp32u uiMaxHeight, Ipp32u picWidth, Ipp32u picHeight, int iGOPSize)
{    
    m_picWidth = picWidth;
    m_picHeight = picHeight;

    m_maxCUWidth = uiMaxWidth;
    m_maxCUHeight = uiMaxHeight;
    m_maxCUSize = m_maxCUWidth * m_maxCUHeight;
    m_numPartition = m_maxCUSize >> 4;

    m_picWidthInCU  = (m_picWidth + m_maxCUWidth - 1) / m_maxCUWidth;
    m_picHeightInCU = (m_picHeight + m_maxCUHeight - 1) / m_maxCUHeight;

    if( (m_picWidthInCU*m_maxCUWidth) > m_picWidth)        m_edgeCUWidth = m_maxCUWidth;
    else    m_edgeCUWidth = m_picWidth - (m_picWidthInCU - 1)*m_maxCUWidth;

    if( (m_picHeightInCU*m_maxCUHeight) > m_picHeight)        m_edgeCUHeight = m_maxCUHeight;
    else    m_edgeCUHeight = m_picHeight - (m_picHeightInCU - 1)*m_maxCUHeight;

    m_GOPSize = iGOPSize;
    m_GOPID2Class = new int[m_GOPSize];
    if ( iGOPSize == 4 )
    {
      // Setup for LowDelay
      m_GOPID2Class[0] = 3;
      m_GOPID2Class[1] = 2;
      m_GOPID2Class[2] = 3;
      m_GOPID2Class[3] = 1;
    }
    else if ( iGOPSize == 8 )
    {
      m_GOPID2Class[0] = 1;
      m_GOPID2Class[1] = 2;
      m_GOPID2Class[2] = 3;
      m_GOPID2Class[3] = 4;
      m_GOPID2Class[4] = 4;
      m_GOPID2Class[5] = 3;
      m_GOPID2Class[6] = 4;
      m_GOPID2Class[7] = 4;
    } 
    else if ( iGOPSize == 16 )
    {
        m_GOPID2Class[0] = 1;
        m_GOPID2Class[1] = 2;
        m_GOPID2Class[2] = 3;
        m_GOPID2Class[3] = 4;
        m_GOPID2Class[4] = 4;
        m_GOPID2Class[5] = 2;
        m_GOPID2Class[6] = 3;
        m_GOPID2Class[7] = 4;
        m_GOPID2Class[8] = 4;
        m_GOPID2Class[9] = 2;
        m_GOPID2Class[10] = 3;
        m_GOPID2Class[11] = 4;
        m_GOPID2Class[12] = 4;
        m_GOPID2Class[13] = 3;
        m_GOPID2Class[14] = 4;
        m_GOPID2Class[15] = 4;
    } else {
        int i;
        for(i=0;i<iGOPSize;i++)
            m_GOPID2Class[i] = 1;
    }
}

void TAdapQP::destroy()
{
    if ( m_GOPID2Class != NULL )
    {
        delete[] m_GOPID2Class;
        m_GOPID2Class = NULL;
    }
}
} // namespace

#endif
/* EOF */
