//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2007-2013 Intel Corporation. All Rights Reserved.
//

//#define DEBUG_ONLY_AVS_ENCODER
#include <umc_defs.h>
#ifdef UMC_ENABLE_ME

#include <math.h>
#include "umc_me.h"
#include "umc_me_cost_func.h"
#include "umc_vec_prediction.h"

//int frame_count = 0;
#ifndef DEBUG_ONLY_AVS_ENCODER
namespace UMC_VC1_ENCODER{
    extern const Ipp16u* VLCTableCBPCY_PB[4];
    extern const Ipp32u* CBPCYFieldTable_VLC[8];
    extern const  Ipp16s TTMBVLC_HighRate[2][4][3*2];
    extern const  Ipp16s TTMBVLC_MediumRate[2][4][3*2];
    extern const  Ipp16s TTMBVLC_LowRate[2][4][3*2];

    extern const Ipp8u  TTBLKVLC_HighRate[4][3*2];
    extern const Ipp8u  TTBLKVLC_MediumRate[4][3*2];
    extern const Ipp8u  TTBLKVLC_LowRate[4][3*2];

    extern const Ipp8u  SubPattern4x4VLC_HighRate[16*2];
    extern const Ipp8u  SubPattern4x4VLC_MediumRate[16*2] ;
    extern const Ipp8u  SubPattern4x4VLC_LowRate[16*2] ;
    extern const Ipp8u SubPattern8x4_4x8VLC[3*2];

    //VLC table selection
    extern const Ipp16u*  MVDiffTablesVLC[4];

    typedef struct
    {
       const Ipp8u * pTableDL;
       const Ipp8u * pTableDR;
       const Ipp8u * pTableInd;
       const Ipp8u * pTableDLLast;
       const Ipp8u * pTableDRLast;
       const Ipp8u * pTableIndLast;
       const Ipp32u* pEncTable;
    } sACTablesSet;
    extern const sACTablesSet ACTablesSet[8];    

    typedef enum
    {
        VC1_ENC_HIGH_MOTION_INTRA   = 0,
        VC1_ENC_LOW_MOTION_INTRA    = 1,
        VC1_ENC_MID_RATE_INTRA      = 2,
        VC1_ENC_HIGH_RATE_INTRA     = 3,
        VC1_ENC_HIGH_MOTION_INTER   = 4,
        VC1_ENC_LOW_MOTION_INTER    = 5,
        VC1_ENC_MID_RATE_INTER      = 6,
        VC1_ENC_HIGH_RATE_INTER     = 7,
    }eCodingSet;
    extern const    eCodingSet  LumaCodingSetsIntra[2][3];
    extern const    eCodingSet  ChromaCodingSetsIntra[2][3];
    extern const    eCodingSet  CodingSetsInter[2][3];
}

#else // #ifdef DEBUG_ONLY_AVS_ENCODER
namespace UMC_VC1_ENCODER{
    extern const Ipp16u* VLCTableCBPCY_PB[4]={0};
    extern const  Ipp16s TTMBVLC_HighRate[2][4][3*2]={0};
    extern const  Ipp16s TTMBVLC_MediumRate[2][4][3*2]={0};
    extern const  Ipp16s TTMBVLC_LowRate[2][4][3*2]={0};

    extern const Ipp8u  TTBLKVLC_HighRate[4][3*2]={0};
    extern const Ipp8u  TTBLKVLC_MediumRate[4][3*2]={0};
    extern const Ipp8u  TTBLKVLC_LowRate[4][3*2]={0};

    extern const Ipp8u  SubPattern4x4VLC_HighRate[16*2]={0};
    extern const Ipp8u  SubPattern4x4VLC_MediumRate[16*2]={0} ;
    extern const Ipp8u  SubPattern4x4VLC_LowRate[16*2]={0} ;
    extern const Ipp8u SubPattern8x4_4x8VLC[3*2]={0};

    //VLC table selection
    extern const Ipp16u*  MVDiffTablesVLC[4]={0};

    typedef struct
    {
       const Ipp8u * pTableDL;
       const Ipp8u * pTableDR;
       const Ipp8u * pTableInd;
       const Ipp8u * pTableDLLast;
       const Ipp8u * pTableDRLast;
       const Ipp8u * pTableIndLast;
       const Ipp32u* pEncTable;
    } sACTablesSet;
    const sACTablesSet ACTablesSet[8];

    typedef enum
    {
        VC1_ENC_HIGH_MOTION_INTRA   = 0,
        VC1_ENC_LOW_MOTION_INTRA    = 1,
        VC1_ENC_MID_RATE_INTRA      = 2,
        VC1_ENC_HIGH_RATE_INTRA     = 3,
        VC1_ENC_HIGH_MOTION_INTER   = 4,
        VC1_ENC_LOW_MOTION_INTER    = 5,
        VC1_ENC_MID_RATE_INTER      = 6,
        VC1_ENC_HIGH_RATE_INTER     = 7,
    }eCodingSet;
    const    eCodingSet  LumaCodingSetsIntra[2][3]={{VC1_ENC_HIGH_MOTION_INTRA,
                                                     VC1_ENC_HIGH_MOTION_INTRA,
                                                     VC1_ENC_HIGH_MOTION_INTRA},
                                                    {VC1_ENC_HIGH_MOTION_INTRA,
                                                     VC1_ENC_HIGH_MOTION_INTRA,
                                                     VC1_ENC_HIGH_MOTION_INTRA}};
    const    eCodingSet  ChromaCodingSetsIntra[2][3]={{VC1_ENC_HIGH_MOTION_INTRA,
                                                     VC1_ENC_HIGH_MOTION_INTRA,
                                                     VC1_ENC_HIGH_MOTION_INTRA},
                                                    {VC1_ENC_HIGH_MOTION_INTRA,
                                                     VC1_ENC_HIGH_MOTION_INTRA,
                                                     VC1_ENC_HIGH_MOTION_INTRA}};
    const    eCodingSet  CodingSetsInter[2][3]={{VC1_ENC_HIGH_MOTION_INTRA,
                                                     VC1_ENC_HIGH_MOTION_INTRA,
                                                     VC1_ENC_HIGH_MOTION_INTRA},
                                                    {VC1_ENC_HIGH_MOTION_INTRA,
                                                     VC1_ENC_HIGH_MOTION_INTRA,
                                                     VC1_ENC_HIGH_MOTION_INTRA}};
}
#endif // #ifdef DEBUG_ONLY_AVS_ENCODER

#ifdef ME_DEBUG
 int CbpcyTableIndex;
 int MvTableIndex;
 int AcTableIndex;
 bool UseTrellisQuant;
 int EncMbAdr;
 int NumOfHibrids;           
#endif
 
namespace UMC
{

using namespace UMC_VC1_ENCODER;

//$$$$$$$$$$$$$$
static IppStatus _own_ippiInterpolate8x8QPBilinear_VC1_8u_C1R_NV12   (const Ipp8u* pSrc, Ipp32s srcStep,
                                                                      Ipp8u *pDst,       Ipp32s dstStep,
                                                                      Ipp32s dx,         Ipp32s dy,
                                                                      Ipp32s roundControl)
{
    IppStatus ret = ippStsNoErr;
    Ipp32s i, j;
    const Ipp32s F[4] = {4, 3, 2, 1};
    const Ipp32s G[4] = {0, 1, 2, 3};

    Ipp32s Mult1 = F[dx]*F[dy];
    Ipp32s Mult2 = F[dx]*G[dy];
    Ipp32s Mult3 = F[dy]*G[dx];
    Ipp32s Mult4 = G[dx]*G[dy];

    for(j = 0; j < 8; j++)
    {
        for(i = 0; i < 16; i += 2)
        {
            pDst[i] = (Ipp8u)((pSrc[i]               * Mult1 +
                               pSrc[i + srcStep]     * Mult2 +
                               pSrc[i + 2]           * Mult3 +
                               pSrc[i + srcStep + 2] * Mult4 +
                               8 - roundControl) >> 4);

            pDst[i + 1] = (Ipp8u)((pSrc[i + 1]               * Mult1 +
                                   pSrc[i + 1 + srcStep]     * Mult2 +
                                   pSrc[i + 1 + 2]           * Mult3 +
                                   pSrc[i + 1 +srcStep + 2] * Mult4 +
                                   8 - roundControl) >> 4);
        }

        pSrc += srcStep;
        pDst += dstStep;
    }
    return ret;
}

IppStatus _own_ippiInterpolateQPBilinear_VC1_8u_C1R_NV12  (const IppVCInterpolate_8u* inter_struct)
{
    IppStatus ret = ippStsNoErr;

    ret= _own_ippiInterpolate8x8QPBilinear_VC1_8u_C1R_NV12(inter_struct->pSrc,
                                               inter_struct->srcStep,
                                               inter_struct->pDst,
                                               inter_struct->dstStep,
                                               inter_struct->dx,
                                               inter_struct->dy,
                                               inter_struct->roundControl);
    return ret;
}
inline UMC::Status  copyChromaBlockNV12 (Ipp8u*  pUVRow,  Ipp8u* /*pVRow*/,  Ipp32u UVRowStep, 
                                        Ipp16s* pUBlock, Ipp16s* pVBlock,    Ipp32u UVBlockStep,
                                        Ipp32u  nPos)
{
    pUVRow += (nPos << 4);
    for (int i=0; i<8; i++)
    {
        Ipp16s* pU = pUBlock;
        Ipp16s* pV = pVBlock;
        Ipp8u* pPlane = pUVRow;

        for (int j = 0; j <8; j++)
        {
            *(pU ++) = *(pPlane ++);
            *(pV ++) = *(pPlane ++);
        }
        pUBlock = (Ipp16s*)((Ipp8u*)pUBlock + UVBlockStep);
        pVBlock = (Ipp16s*)((Ipp8u*)pVBlock + UVBlockStep);
        pUVRow +=  UVRowStep;

    }
    return UMC::UMC_OK;
}

inline UMC::Status  copyDiffChromaBlockNV12 (Ipp8u*   pUVRowS,   Ipp8u* /*pVRowS*/,    Ipp32u UVRowStepS, 
                                             Ipp8u*   pUVRowR,   Ipp8u* /*pVRowR*/,    Ipp32u UVRowStepR, 
                                             Ipp16s*  pUBlock,   Ipp16s* pVBlock,      Ipp32u UVBlockStep,
                                             Ipp32u  nPosS, Ipp32u nPosR)
{
    pUVRowS += (nPosS << 4);
    pUVRowR += (nPosR << 4);

    for (int i=0; i<8; i++)
    {
        Ipp16s* pU     = pUBlock;
        Ipp16s* pV     = pVBlock;
        Ipp8u* pPlaneS = pUVRowS;
        Ipp8u* pPlaneR = pUVRowR;

        for (int j = 0; j <8; j++)
        {
            *(pU ++) = *(pPlaneS ++) - *(pPlaneR ++);
            *(pV ++) = *(pPlaneS ++) - *(pPlaneR ++);
        }
        pUBlock = (Ipp16s*)((Ipp8u*)pUBlock + UVBlockStep);
        pVBlock = (Ipp16s*)((Ipp8u*)pVBlock + UVBlockStep);
        pUVRowS +=  UVRowStepS;
        pUVRowR +=  UVRowStepR;

    }
    return UMC::UMC_OK;
}

inline UMC::Status  pasteSumChromaBlockNV12 (Ipp8u*  pUVRowRef,  Ipp8u* /*pVRowRef*/,   Ipp32u UVRowStepRef, 
                                             Ipp8u*  pUVRowDst,  Ipp8u* /*pVRowDst*/,   Ipp32u UVRowStepDst,
                                             Ipp16s* pUBlock,   Ipp16s* pVBlock,   Ipp32u UVBlockStep,
                                             Ipp32u  nPosRef,   Ipp32u  nPosDst)
{
    pUVRowDst += (nPosDst << 4);
    pUVRowRef += (nPosRef << 4);

    for (int i=0; i<8; i++)
    {
        Ipp16s* pU = pUBlock;
        Ipp16s* pV = pVBlock;
        Ipp8u* pPlane    = pUVRowDst;
        Ipp8u* pPlaneRef = pUVRowRef;


        for (int j = 0; j <8; j++)
        {
            Ipp16s U = *(pU ++) + *(pPlaneRef++);
            Ipp16s V = *(pV ++) + *(pPlaneRef++);

            U = (U < 0)  ? 0   : U;
            U = (U > 255)? 255 : U;
            V = (V < 0)  ? 0   : V;
            V = (V >255) ? 255 : V;

            *(pPlane ++) = (Ipp8u)U;
            *(pPlane ++) = (Ipp8u)V;
        }
        pUBlock = (Ipp16s*)((Ipp8u*)pUBlock + UVBlockStep);
        pVBlock = (Ipp16s*)((Ipp8u*)pVBlock + UVBlockStep);
        pUVRowDst +=  UVRowStepDst;
        pUVRowRef +=  UVRowStepRef;
    }
    return UMC::UMC_OK;
}

inline UMC::Status  pasteChromaBlockNV12 (Ipp8u*  pUVRow,  Ipp8u* /*pVRow*/,   Ipp32u UVRowStep, 
                                          Ipp16s* pUBlock, Ipp16s* pVBlock,    Ipp32u UVBlockStep,
                                          Ipp32u  nPos)
{
    pUVRow += (nPos << 4);
    for (int i=0; i<8; i++)
    {
        Ipp16s* pU = pUBlock;
        Ipp16s* pV = pVBlock;
        Ipp8u* pPlane = pUVRow;

        for (int j = 0; j <8; j++)
        {
            Ipp16s U = *(pU ++);
            Ipp16s V = *(pV ++);

            U = (U < 0)  ? 0   : U;
            U = (U > 255)? 255 : U;
            V = (V < 0)  ? 0   : V;
            V = (V >255) ? 255 : V;

            *(pPlane ++) = (Ipp8u)U;
            *(pPlane ++) = (Ipp8u)V;
        }
        pUBlock = (Ipp16s*)((Ipp8u*)pUBlock + UVBlockStep);
        pVBlock = (Ipp16s*)((Ipp8u*)pVBlock + UVBlockStep);
        pUVRow +=  UVRowStep;

    }
    return UMC::UMC_OK;
}
//$$$$$$$$$$$$$$



//*******************************************************************************************************
MeInitParams::MeInitParams()
{
    refPadding      = 0;
    SearchDirection = ME_BidirSearch;
    MbPart          = ME_Mb16x16;
    UseStatistics = true;
    UseDownSampling = true;
    bNV12 = false;
}


//*******************************************************************************************************
///Function sets default value to MeParams structure.
///See also MeParams::SetSearchSpeed
MeParams::MeParams()
{
    pSrc    = NULL;
    FirstMB = -1;  //for multy threading slice processing
    LastMB  = -1;

    for(Ipp32u i = 0; i < MAX_REF; i++)
    {
        pRefF[i] = NULL;
        pRefB[i] = NULL;
    }

    SearchRange.x           = 0;
    SearchRange.y           = 0;
    SearchDirection         = ME_ForwardSearch;
    Interpolation           = ME_VC1_Bilinear;
    MbPart                  = 0;
    PixelType               = ME_IntegerPixel;
    PicRange.top_left.x     = 0;
    PicRange.top_left.y     = 0;
    PicRange.bottom_right.x = 0;
    PicRange.bottom_right.y = 0;
    SkippedMetrics          = ME_Sad;
    Quant                   = 0;
    FRefFramesNum           = 1;
    BRefFramesNum           = 0;
    ProcessSkipped          = false;
    ProcessDirect           = false;
    FieldMCEnable           = false;
    UseVarSizeTransform = false;
    memset(&ScaleInfo,0,sizeof(MeVC1fieldScaleInfo));
    bSecondField            = false;
    Bfraction               = 0.5;

    UseFastInterSearch=false;
    UseDownSampledImageForSearch=false;
    UseFeedback=false;
    UpdateFeedback=false;
    UseFastFeedback=false;
   // UseChromaForME = false;
    UseChromaForMD = true;
    FastChroma = false;
    ChangeInterpPixelType = false;
    SelectVlcTables=false;
    UseTrellisQuantization=false;
    UseTrellisQuantizationChroma=false;
    CbpcyTableIndex=0;
    MvTableIndex=0;
    IsSMProfile = false;
    AcTableIndex=0;
    
    OutCbpcyTableIndex=0;
    OutMvTableIndex=0;
    OutAcTableIndex=0;
}


///Function sets MeParams value to default presets. Next speed defined right now:
///   - -1 change nothing, this is some kind of expert mode. 
///   - 75 and more. Fast speed search. Only fastest features are used. Really bad quality.
///   - 25-74 Quality search. Best possible quality without full search.
///   - 0-24  Full search. Really slow.
///
///\param Speed search speed
void MeParams::SetSearchSpeed(Ipp32s Speed)
{
    SearchSpeed=Speed;
        //parse Speed parameter
    if(SearchSpeed<0)
        return;

    if(SearchSpeed >= 75)
    {// paranoid speed
        UseFastInterSearch = true;
        //UseDownSampledImageForSearch = true;
    }
    else if(SearchSpeed >= 25)
    { // quality
    
#ifdef ME_DEBUG
//        if(SearchSpeed == 25) SelectVlcTables=true;
//        else SelectVlcTables=false;
        if(SearchSpeed == 29){
            SelectVlcTables=true;            UseTrellisQuantization =true;
        }else if(SearchSpeed == 28){
            SelectVlcTables=false;            UseTrellisQuantization =false;
        }else if(SearchSpeed == 27){
            SelectVlcTables=false;            UseTrellisQuantization =true;
        }else if(SearchSpeed == 26){
            SelectVlcTables=true;            UseTrellisQuantization =false;
        }else if(SearchSpeed == 25){
            SelectVlcTables=true;            UseTrellisQuantization =true;
        }else{
            UseTrellisQuantization=false;   UseTrellisQuantization =false;
        }
        UseTrellisQuant = UseTrellisQuantization;
#endif
        //SelectVlcTables=false;
        UseFastInterSearch = false;
        UseDownSampledImageForSearch = true;
    }
    else 
    { // paranoid quality
        UseFastInterSearch = false;
        UseDownSampledImageForSearch = false;
    }
}


//*******************************************************************************************************
void MeCurrentMB::Reset()
{
    //set costs
    for(Ipp32s i=0; i<ME_NUM_OF_BLOCKS; i++){
        for(Ipp32s j=0; j<ME_NUM_OF_REF_FRAMES; j++){
        }
        InterCost[i]=ME_BIG_COST;
        IntraCost[i]=ME_BIG_COST;
        SkipCost[i]=ME_BIG_COST;
    }
    DirectCost=ME_BIG_COST;
    VlcTableSearchInProgress=false;
    for(Ipp32s i=0; i<10; i++)
        for(Ipp32s j=0; j<6; j++)
            InterTransf[i][j] = ME_Tranform8x8;
    for(Ipp32s i = 0; i < 5; i++)
        for(Ipp32s j = 0; j < 6; j++)
        {
            memset(RoundControl[i][j],0,64*sizeof(RoundControl[0][0][0]));
        }
}

void MeCurrentMB::ClearMvScoreBoardArr(){
        //ippsZero_8u(&mvEstimateDoneArr[0][0],128*64);
        memset(mvScoreBoardArr,0,128*64);
}
Ipp32s MeCurrentMB::CheckScoreBoardMv(MePixelType pix,MeMV mv) {
    if(mvScoreBoardFlag) {
        if(pix==ME_IntegerPixel) {
            if(mvScoreBoardArr[(mv.x>>2)+64][(mv.y>>2)+32] == 0){
                mvScoreBoardArr[(mv.x>>2)+64][(mv.y>>2)+32] = 1;
            } else {
                return 1;
            }
        }
    }
    return 0;
}
void MeCurrentMB::SetScoreBoardFlag(){
    mvScoreBoardFlag = 1;
}
void MeCurrentMB::ClearScoreBoardFlag(){
    mvScoreBoardFlag = 0;
}

void MeMB::Reset()
{
    NumOfNZ=0;
    for(Ipp32s j = 0; j <= 5; j++)
    {
        memset(RoundControl[j],0,64*sizeof(RoundControl[0][0]));
    }
}

void MeMB::Reset(Ipp32s numOfMVs)
{
    NumOfNZ=0;

    for (Ipp32s i = 0; i < 2; i++)
        for (Ipp32s j = 0; j < numOfMVs; j++)
        {
            MV[i][j].SetInvalid();
            MVPred[i][j].SetInvalid();
            Refindex[i][j] = 0;
        }
    for (Ipp32s j = 0; j < numOfMVs + 1; j++)
        MbCosts[j] = ME_BIG_COST;
    
    MbPart = 0;
    BlockPart = 0;
    MbType = 0;
    McType = 0;
    BlockTrans = 0;
    memset(BlockType, 0, sizeof(BlockType));
    memset(predType, 0, sizeof(predType));
    for(Ipp32s j = 0; j <= 5; j++)
    {
        memset(RoundControl[j],0,64*sizeof(RoundControl[0][0]));
    }
}

//function converts 64 coefficients at once
//transf - block of transformed coefficients
//trellis - block of trellis quantized coefficients
//rc - block of round control values
//to quantize with round control value use (transf[i] + rc[i])/QP
void MeMB::CnvertCoeffToRoundControl(Ipp16s *transf, Ipp16s *trellis, Ipp16s *rc, Ipp32s QP)
{
    for(Ipp32s i=0; i<64; i++){
        rc[i]=(Ipp16s)(QP*trellis[i]-transf[i]);    
    }
}


//*******************************************************************************************************
MeBase::MeBase()
{
    /*
#ifdef ME_DEBUG
    CbpcyTableIndex=0;
    MvTableIndex=0;
    AcTableIndex=0;
#endif
    */
    InitVlcTableIndexes();

    m_AllocatedSize = 0;
    m_OwnMem = NULL;
    m_CurRegSet = ME_RG_EMPTY_SET;
    ChangeInterpPixelType = false;
    m_CRFormat = ME_NV12;//ME_YUV420;

    #ifdef ME_USE_BACK_DOOR_LAMBDA
        Ipp32s array[8];
        FILE * InPar = fopen("MeParams.bin","rb");
        if(InPar == NULL){
            array[0] = 1;
            array[1] = 1;
        } else{
            fread(&array[0], sizeof(array), 1, InPar);
            fclose(InPar);
        }        
        m_lambda = array[0];
        //IntraCoeff=m_lambda;
        m_lambda /= 100.;
        printf("lambda=%f\n", m_lambda);
    #endif

}

MeBase::~MeBase()
{
    Close();
#ifdef ME_DEBUG
    delete pStat;
#endif    

};

///First version of Init. It allocates memory by malloc. To free memory call Close or destroy object.
///Some basic checks for passed parameters are performed. If some of requested modes are not
///supported function returns false. Initialization parameters are saved inside MeBase and used later in 
///EstimateFrame to check MeParams. They should not violate initialization agreement. If, for example,
///memory was not allocated for downsampled images in Init, any attempt to perform search on downsampled
///image will lead to immediate return from EstimateFrame with false.
///\param par initialization parameters
bool MeBase::Init(MeInitParams *par)
{
    m_OwnMem=NULL;
    Ipp32s size=0;
    if(par->bNV12)
        m_CRFormat = ME_NV12;
    else
        m_CRFormat = ME_YUV420;

    if(!Init(par, (Ipp8u*)m_OwnMem, size))return false;
    m_OwnMem = malloc(size);
    if(m_OwnMem==NULL)return false;
    return Init(par, (Ipp8u*)m_OwnMem, size);
}

///Second version of init. Intended for memory allocation from external buffer. Two subsequent calls are needed
///for proper allocation. First one with NULL pointer to calculate necessary memory size, second one to memory allocation.
///See also first Init.
///\param par initialization parameter
///\param ptr pointer to external buffer or NULL to calculate memory necessary  size
///\param size size of external buffer or necessary amount of memory
bool MeBase::Init(MeInitParams *par, Ipp8u* ptr, Ipp32s &size)
{

    //1 workaround
    par->UseStatistics=true;
    par->UseDownSampling=true;
    //1 end of workaround

    if(par->bNV12)
        m_CRFormat = ME_NV12;
    else
        m_CRFormat = ME_YUV420;


    bool aloc=(ptr != NULL);
    if(aloc && (m_AllocatedSize==0 || m_AllocatedSize<size)){
        SetError((vm_char*)"Wrong buffer size in MeBase::Init");
        return false;
    }
    //Ipp32s NumOfMVs = par->MbPart == ME_Mb16x16?1:4;
    Ipp32s NumOfMVs = 4;
    Ipp32s NumOfMBs = par->HeightMB*par->WidthMB;

    m_InitPar = *par;

    par->pFrames = (MeFrame*)ptr;
    ptr+=par->MaxNumOfFrame*sizeof(MeFrame);

    for(Ipp32s fr = 0; fr < par->MaxNumOfFrame; fr++){
        if(aloc)par->pFrames[fr].NumOfMVs=NumOfMVs;
        if(par->SearchDirection == ME_BidirSearch){
            if(aloc) par->pFrames[fr].MVDirect = (MeMV*)ptr; ptr+=NumOfMBs*sizeof(MeMV);
            if(aloc) par->pFrames[fr].RefIndx = (Ipp32s*)ptr; ptr+=NumOfMBs*sizeof(Ipp32s);
        }else{
            if(aloc) par->pFrames[fr].MVDirect = (MeMV*)(par->pFrames[fr].RefIndx = (Ipp32s*)NULL);
        }

        //MB
        if(aloc) par->pFrames[fr].MBs = (MeMB*)ptr; ptr += NumOfMBs*sizeof(MeMB);
        for(Ipp32s m=0; m<NumOfMBs; m++)
        {
            if(aloc) par->pFrames[fr].MBs[m].MV[frw] = (MeMV*)ptr; ptr+=NumOfMVs*sizeof(MeMV);
            if(aloc) par->pFrames[fr].MBs[m].MV[bkw] = (MeMV*)ptr; ptr+=NumOfMVs*sizeof(MeMV);
            if(aloc) par->pFrames[fr].MBs[m].Refindex[frw] = (Ipp32s*)ptr; ptr+=NumOfMVs*sizeof(Ipp32s);
            if(aloc) par->pFrames[fr].MBs[m].Refindex[bkw] = (Ipp32s*)ptr; ptr+=NumOfMVs*sizeof(Ipp32s);
            if(aloc) par->pFrames[fr].MBs[m].MbCosts = (Ipp32s*)ptr; ptr+=(NumOfMVs+1)*sizeof(Ipp32s);
            if(aloc) par->pFrames[fr].MBs[m].MVPred[frw] = (MeMV*)ptr; ptr+=NumOfMVs*sizeof(MeMV);
            if(aloc) par->pFrames[fr].MBs[m].MVPred[bkw] = (MeMV*)ptr; ptr+=NumOfMVs*sizeof(MeMV);
        }

        //stat
        if(par->UseStatistics){
            if(aloc) par->pFrames[fr].stat = (MeMbStat*)ptr; ptr += NumOfMBs*sizeof(MeMbStat);
            for(Ipp32s m=0; m<NumOfMBs; m++){
                if(aloc) par->pFrames[fr].stat[m].MVF = (Ipp16u*)ptr; ptr+=NumOfMVs*sizeof(Ipp16u);
                if(aloc) par->pFrames[fr].stat[m].MVB = (Ipp16u*)ptr; ptr+=NumOfMVs*sizeof(Ipp16u);
                if(aloc) par->pFrames[fr].stat[m].coeff = (Ipp16u*)ptr; ptr+=6*sizeof(Ipp16u);
            }
        }else{
            if(aloc) par->pFrames[fr].stat = NULL;
        }

        //plane allocation including downsampling
        if(aloc) par->pFrames[fr].plane[0].clear();

        for(Ipp32s lev=1; lev<4; lev++){
            if(aloc) par->pFrames[fr].plane[lev].clear();
            if(par->UseDownSampling){
                Ipp32s scale=1<<lev;
                Ipp32s height = (16*par->HeightMB+2*par->refPadding+scale-1)/scale;
                Ipp32s width = (16*par->WidthMB+2*par->refPadding+scale-1)/scale;
                if(aloc) par->pFrames[fr].plane[lev].set(Y, ptr+(width+1)*(par->refPadding/scale), width);
                ptr += width*height;
            }
        }
    }
    //allocate different intermediate buffers
    //interpolation
    Ipp32s dim;
    for(Ipp32s dir=0; dir<3; dir++){
        dim=16;
        for(Ipp32s ch=0; ch<3; ch++){
            if(aloc) m_cur.buf[dir].set(ch,(Ipp8u*)ptr,dim); ptr+=dim*dim;
            //if(ch==0)dim/=2; !!!!! for m_CRFormat = ME_NV12 
        }
    }
    

    //get size
    if(!aloc)m_AllocatedSize=size=(Ipp32s)(ptr-(Ipp8u*)(NULL));        


#ifdef ME_DEBUG
    pStat = new MeStat(par->WidthMB, par->HeightMB);
#endif    

#ifdef OTL
    otl = fopen("otlad.txt","wt");
    assert(otl);
#endif

    return true;
}

int ParFileQpToMeQP(int x, bool &uniform)
{
/*
Uniform
x1
2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,  //par file
2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,  //ME

NonUniform
x2
18,20,22,24,26,28,30,32,34,36,38,40,42,44,46,48,50,52,54,56   //par file
12,14,16,18,20,22,24,26,28,30,32,34,36,38,40,42,44,46,48,50   //ME
x4
58,60,62  //par file 
54,58,62  //ME

*/
    if(x<=17){uniform=true; return x;}
    if(x<=56){uniform=false; return x-6;}
    uniform=false;
    return 2*(x-58) + 54;
}

#ifdef VC1_ENC_DEBUG_ON
static void PrintField(Ipp8u* pY, Ipp32s Ystep, Ipp8u* pU, Ipp32s Ustep, Ipp8u* pV, Ipp32s Vstep, FILE* out, Ipp32u Width, Ipp32u Height)
{
    static Ipp32u framenum = 0;

   Ipp32s i = 0;
   Ipp8u* ptr = NULL;

   for (i=0; i < Height; i++)
    {
        fwrite(pY+i*Ystep, 1, Width, out);

    }
    for (i=0; i < Height/2; i++)
    {
        fwrite(pU+i*Ustep, 1, Width/2, out);

    }
    for (i=0; i < Height/2; i++)
    {
        fwrite(pV+i*Vstep, 1, Width/2, out);

    }

    fflush(out);
    framenum++;
}
#endif

///Perform motion estimation for one frame or slice.
///See also MeBase::Init and MeParams descriptions.
bool MeBase::EstimateFrame(MeParams *par)
{

#ifdef VC1_ENC_DEBUG_ON
static FILE* f1 = NULL;
static FILE* f2 = NULL;
static FILE* f3 = NULL;
static FILE* f4 = NULL;
static FILE* f5 = NULL;

if(!f1)
    f1 = fopen("curr.yuv", "wb");
if(!f2)
    f2 = fopen("ref_f1.yuv", "wb");

if(!f3)
    f3 = fopen("ref_f2.yuv", "wb");

if(!f4)
    f4 = fopen("ref_b1.yuv", "wb");

if(!f5)
    f5 = fopen("ref b2.yuv", "wb");

static Ipp32u h = 480;
static Ipp32u w = 720;
if(par->pSrc)
    PrintField(par->pSrc[0].plane[0].ptr[0], par->pSrc[0].plane[0].step[0],
    par->pSrc[0].plane[0].ptr[1], par->pSrc[0].plane[0].step[1],
    par->pSrc[0].plane[0].ptr[2], par->pSrc[0].plane[0].step[2],
    f1, w, h);

if(par->pRefF[0])
    PrintField(par->pRefF[0]->plane[0].ptr[0],
    par->pRefF[0]->plane[0].step[0],
    par->pRefF[0]->plane[0].ptr[1], par->pRefF[0]->plane[0].step[1],
    par->pRefF[0]->plane[0].ptr[2], par->pRefF[0]->plane[0].step[2],
    f2, w, h);

if(par->pRefF[1])
    PrintField(par->pRefF[1]->plane[0].ptr[0],
    par->pRefF[1]->plane[0].step[0],
    par->pRefF[1]->plane[0].ptr[1], par->pRefF[0]->plane[0].step[1],
    par->pRefF[1]->plane[0].ptr[2], par->pRefF[0]->plane[0].step[2],
    f3, w, h);

if(par->pRefB[0])
    PrintField(par->pRefB[0]->plane[0].ptr[0],
    par->pRefB[0]->plane[0].step[0],
    par->pRefB[0]->plane[0].ptr[1], par->pRefB[0]->plane[0].step[1],
    par->pRefB[0]->plane[0].ptr[2], par->pRefB[0]->plane[0].step[2],
    f4, w, h);

if(par->pRefB[1])
    PrintField(par->pRefB[1]->plane[0].ptr[0],
    par->pRefB[1]->plane[0].step[0],
    par->pRefB[1]->plane[0].ptr[1], par->pRefB[0]->plane[0].step[1],
    par->pRefB[1]->plane[0].ptr[2], par->pRefB[0]->plane[0].step[2],
    f5, w, h);

#endif

    ChangeInterpPixelType = par->ChangeInterpPixelType 
        && par->SearchDirection == ME_ForwardSearch
        && par->MbPart != ME_Mb8x8;

    ReInitVlcTableIndexes(par);

    if(ChangeInterpPixelType){
        //interpolation selection
        MeParams CurPar=*par;
        CurPar.UseFastInterSearch = true;
        CurPar.UseFeedback = false;
        memset(CostOnInterpolation,0,sizeof(CostOnInterpolation));
        Ipp32s max_j = 2;
        for(int j = 0; j < max_j; j++)
        {
            switch(j)
            {
            case 0:
                CurPar.Interpolation = ME_VC1_Bilinear;
                CurPar.PixelType = ME_HalfPixel;
                break;
            case 1:
                CurPar.Interpolation = ME_VC1_Bicubic;
                CurPar.PixelType = ME_QuarterPixel;
                break;
            default:
                VM_ASSERT(0);
            }
            EstimateFrameInner(&CurPar);
        }
    }
    SetInterpPixelType(par);

    //return EstimateFrameInner(par);
    bool rc = EstimateFrameInner(par);

    /*
    if(frame_count == 1)
    {
        FILE *otl = NULL;
        if(m_CRFormat == ME_YUV420)
            otl = fopen("otlad_YUV420.txt","wt");
        else
            otl = fopen("otlad_NV12.txt","wt");
        assert(otl);
        for(Ipp32s MbAdr = m_par->FirstMB; MbAdr <= m_par->LastMB; MbAdr++)
        {
            MeMB *mb=&m_par->pSrc->MBs[MbAdr];
            fprintf(otl,"mb_num = %d, mb_type = %d, MVF.x = %d, MVF.y = %d,MVB.x = %d, MVB.y = %d\n",
                MbAdr,mb->MbType, mb->MV[0][0].x, mb->MV[0][0].y,mb->MV[1][0].x, mb->MV[1][0].y);
        }

        fclose(otl);
    }
    
    frame_count++; 
    */
    return rc;
}

bool MeBase::EstimateFrameInner(MeParams *par)
{
#ifdef ME_DEBUG
    NumOfHibrids=0;
    MeQP = par->Quant;
    MeQPUNF=par->UniformQuant;
    MeLambda=100*m_lambda;

    //1 workaround
    par->ChromaInterpolation=ME_VC1_Bilinear;
    //1 end of workaround
#endif

    //check input params, init some variables
    m_par = par;
//$$$$$$$$$$$$$
    //temporary!!!
    if(m_par->SearchDirection == ME_IntraSearch)
    {
        for(Ipp32s MbAdr = m_par->FirstMB; MbAdr <= m_par->LastMB; MbAdr++)
        {
            //reset costs
            m_cur.Reset();
            m_cur.y = MbAdr/par->pSrc->WidthMB;
            m_cur.x = MbAdr%par->pSrc->WidthMB;
            m_cur.adr = MbAdr;

            m_par->pSrc->MBs[m_cur.adr].Reset();
        }
        return true;
    }
//$$$$$$$$$$$$

    if(!CheckParams())
        return false;

    DownsampleFrames();

    //init feedback
    LoadRegressionPreset();

    //init auxiliary classes
    m_PredCalc->Init(par, &m_cur);

    //VLC selection
    for(Ipp32s i=0; i<4; i++){
        m_cur.CbpcyRate[i]=0;
        m_cur.MvRate[i]=0;
        m_cur.AcRate[i]=0;
    }

        //estimate all MB
    for(Ipp32s MbAdr = m_par->FirstMB; MbAdr <= m_par->LastMB; MbAdr++)
    {
        //reset costs
        m_cur.Reset();
        m_cur.y = MbAdr/par->pSrc->WidthMB;
        m_cur.x = MbAdr%par->pSrc->WidthMB;
        m_cur.adr = MbAdr;

        m_par->pSrc->MBs[m_cur.adr].Reset();
        // TODO: rewrite this
        if(m_cur.adr!=0)m_par->pSrc->MBs[m_cur.adr].NumOfNZ=m_par->pSrc->MBs[m_cur.adr-1].NumOfNZ;
        

        //set ptr to source MB
//        m_cur.SrcStep[Y] = m_par->pSrc->step[Y];
//        m_cur.pSrc[Y] = m_par->pSrc->ptr[Y]+16*m_cur.y*m_cur.SrcStep[Y]+16*m_cur.x;

        //choose estimation function
        if(m_par->MbPart == ME_Mb16x16)
        {
            EstimateMB16x16();
        }
        else if(m_par->MbPart == (ME_Mb8x8 | ME_Mb16x16))
        {
            EstimateMB8x8();
        }
        else
        {
            SetError((vm_char*)"Unsupported partitioning");
        }
        UpdateVlcTableStatistic();
    }
    MakeVlcTableDecision();
    SetVlcTableIndexes(m_par);

    return true;
}

void MeBase::EstimateMB16x16()
{
    //set block index and type, for 16x16 it is always 0 & ME_Mb16x16
    m_cur.BlkIdx=0;
    m_cur.MbPart = ME_Mb16x16;

    m_PredCalc->ResetPredictors();

    for(;;){
        if(EstimateSkip()) break;
        if(Estimate16x16Direct()) break;

        Ipp32s j = 0;

        for(Ipp32s i=0; i<m_par->FRefFramesNum+m_par->BRefFramesNum; i++){
            if(i<m_par->FRefFramesNum){
                j = i;
                SetReferenceFrame(frw,i);
            }else{
                j = i-m_par->FRefFramesNum;
                SetReferenceFrame(bkw,j);
            }

            m_cur.PredMV[m_cur.RefDir][j][m_cur.BlkIdx]=m_PredCalc->GetPrediction();
            m_cur.BestMV[m_cur.RefDir][j][m_cur.BlkIdx] = MeMV(0);
            EstimateInter(); //this is MV search, and inter cost calculation
        }
        Estimate16x16Bidir();//here bidir costs is calculated
        EstimateIntra();
        break;
    }

    MakeMBModeDecision16x16();
}

void MeBase::EstimateMB8x8()
{
    //estimate 16x16, exit search if result is good enough
    EstimateMB16x16();
    //if(EarlyExitAfter16x16()) return;

    m_PredCalc->ResetPredictors();
    //reset costs
    m_cur.Reset();

    //for all block in MB
    m_cur.MbPart = ME_Mb8x8;
    for(m_cur.BlkIdx=0; m_cur.BlkIdx<4; m_cur.BlkIdx++){
        for(;;){
            if(EstimateSkip()) break;
            Ipp32s j = 0;

            for(Ipp32s i=0; i<m_par->FRefFramesNum+m_par->BRefFramesNum; i++){
                if(i<m_par->FRefFramesNum){
                    j = i;
                    SetReferenceFrame(frw,i);
                }else{
                    j = i-m_par->FRefFramesNum;
                    SetReferenceFrame(bkw,j);
                }

                m_cur.PredMV[m_cur.RefDir][j][m_cur.BlkIdx]=m_PredCalc->GetPrediction();
                m_cur.BestMV[m_cur.RefDir][j][m_cur.BlkIdx] = MeMV(0);
                EstimateInter(); //this is MV search, and inter cost calculation
            }
            EstimateIntra();
            break;
        }
        MakeBlockModeDecision();
    }

    MakeMBModeDecision8x8();
}

bool MeBase::EarlyExitForSkip()
{
    Ipp32s thr=m_SkippedThreshold;
    if(m_cur.MbPart == ME_Mb8x8)
        thr/=4;

    if(m_cur.SkipCost[m_cur.BlkIdx]<thr)
        return true;

    return false;
}


void MeBase::EstimateInter()
{
    //reset intermediate cost
    m_cur.BestCost =ME_BIG_COST;

    //search
     if(m_par->UseFastInterSearch){
        EstimateMbInterFast();
     }else if(m_par->UseDownSampledImageForSearch){
         EstimateMbInterFullSearchDwn();
    }else{
         EstimateMbInterFullSearch();
    }

    //check if found MV is better
//    if(!m_par->UseFeedback)
//    {
    if(m_cur.BestCost<m_cur.InterCost[m_cur.BlkIdx]){
        m_cur.InterCost[m_cur.BlkIdx]=m_cur.BestCost;
        if(m_cur.RefDir == frw){
            m_cur.InterType[m_cur.BlkIdx] = ME_MbFrw;
            m_cur.BestIdx[frw][m_cur.BlkIdx] = m_cur.RefIdx;
           // m_cur.BestIdx[bkw][m_cur.BlkIdx] = -1;
        }else{
            m_cur.InterType[m_cur.BlkIdx] = ME_MbBkw;
          //  m_cur.BestIdx[frw][m_cur.BlkIdx] = -1;
            m_cur.BestIdx[bkw][m_cur.BlkIdx] = m_cur.RefIdx;
        }
    }
//}
}


void MeBase::Estimate16x16Bidir()
{
    if(m_par->SearchDirection != ME_BidirSearch)
        return;

    if(m_par->UseFeedback)
        return;

    //find best bidir
    for(Ipp32s i=0; i< m_par->FRefFramesNum; i++){
        for(Ipp32s j=0; j<m_par->BRefFramesNum; j++){
            //check bidir
            Ipp32s tmpCost=EstimatePointAverage(ME_Mb16x16, m_par->PixelType, m_par->CostMetric, frw, i, m_cur.BestMV[frw][i][0], bkw, j, m_cur.BestMV[bkw][j][0]);
            tmpCost +=WeightMV(m_cur.BestMV[frw][i][0], m_cur.PredMV[frw][i][0]);
            tmpCost +=WeightMV(m_cur.BestMV[bkw][j][0], m_cur.PredMV[bkw][j][0]);
            tmpCost +=32; //some additional penalty for bidir type of MB
            if(tmpCost<m_cur.InterCost[0]){
                m_cur.InterCost[0] = tmpCost;
                m_cur.InterType[0] = ME_MbBidir;
                m_cur.BestIdx[frw][0] = i;
                m_cur.BestIdx[bkw][0] = j;
            }
        }
    }
}


bool MeBase::Estimate16x16Direct()
{
    if(!m_par->ProcessDirect)
        return false;
    if(m_par->SearchDirection != ME_BidirSearch)
        return false;

    MeMV MVDirectFW, MVDirectBW;
    Ipp32s IdxF, IdxB;

    m_DirectOutOfBound = false;
    for(IdxF = 0; IdxF < m_par->FRefFramesNum; IdxF++)
        if(!m_par->pRefF[IdxF]->MVDirect[m_cur.adr].IsInvalid())
        {
            break;
        }

    for(IdxB = 0; IdxB < m_par->BRefFramesNum; IdxB++)
        if(!m_par->pRefB[IdxB]->MVDirect[m_cur.adr].IsInvalid())
        {
            break;
        }

    if(IdxF == m_par->FRefFramesNum || IdxB == m_par->BRefFramesNum)
        SetError((vm_char*)"Wrong MVDirect in MeBase::Estimate16x16Direct");

    m_cur.DirectIdx[frw] = IdxF;
    m_cur.DirectIdx[bkw] = IdxB;

    MVDirectFW = m_par->pRefF[IdxF]->MVDirect[m_cur.adr];
    MVDirectBW = m_par->pRefB[IdxB]->MVDirect[m_cur.adr];

    SetReferenceFrame(frw,IdxF);

    if(m_PredCalc->IsOutOfBound(ME_Mb16x16, m_par->PixelType, MVDirectFW))
    {
        m_DirectOutOfBound = true;
        return false;
    }

    SetReferenceFrame(bkw,IdxB);
    if(m_PredCalc->IsOutOfBound(ME_Mb16x16, m_par->PixelType, MVDirectBW))
    {
        m_DirectOutOfBound = true;
        return false;
    }

    if(m_par->UseFeedback)
        return false;

    //check direct
    Ipp32s tmpCost=EstimatePointAverage(ME_Mb16x16, m_par->PixelType, m_par->CostMetric,
                                        frw, IdxF, MVDirectFW,
                                        bkw, IdxB, MVDirectBW);
    m_cur.DirectCost     = tmpCost;
    m_cur.DirectType     = ME_MbDirect;
    m_cur.DirectIdx[frw] = IdxF;
    m_cur.DirectIdx[bkw] = IdxB;
    

    if(m_cur.DirectCost<m_SkippedThreshold){
        m_cur.DirectType= ME_MbDirectSkipped;
        return true;
    }

    return false;
}


//should be called after EstimateInter.
void MeBase::EstimateIntra()
{
    // TODO: move intra cost calculation from MakeMBModeDecision16x16ByFB here
    if(m_par->UseFeedback) return;

    Ipp32s i = m_cur.BlkIdx;
    if(m_cur.InterCost[i]>=ME_BIG_COST)
        return; //inter estimation was skiped, skip intra as well
    
    //usually Inter and Intra costs calculated by incompatible metrics, so recalculate inter cost
    Ipp32s InterCost;
    if(m_cur.InterType[i] != ME_MbBidir){
        //frw or bkw
        Ipp32s dir = (m_cur.InterType[i] == ME_MbFrw)?frw:bkw;
        SetReferenceFrame(dir, m_cur.BestIdx[dir][i]);
        InterCost=EstimatePoint(m_cur.MbPart, m_par->PixelType, ME_Sadt, m_cur.BestMV[dir][m_cur.BestIdx[dir][i]][i]);
    }else{
        //bidir
        InterCost=EstimatePointAverage(m_cur.MbPart, m_par->PixelType, ME_Sadt,
                    frw, m_cur.BestIdx[frw][i], m_cur.BestMV[frw][  m_cur.BestIdx[frw][i]  ][i],
                    bkw, m_cur.BestIdx[bkw][i], m_cur.BestMV[bkw][  m_cur.BestIdx[bkw][i]  ][i]);
    }

    // TODO: check if there is adjusted intra block, for both 16x16 and 8x8
    MeDecisionMetrics met=ME_SadtSrc;
    Ipp32s IntraCost = EstimatePoint(m_cur.MbPart, m_par->PixelType, met, 0);

    //calculate fake Intra cost
    if(IntraCost<InterCost)
        m_cur.IntraCost[i] = m_cur.InterCost[i]-1;
    else
        m_cur.IntraCost[i] = m_cur.InterCost[i]+1;
}

void MeBase::MakeVlcTableDecision()
{
    if(!m_par->UseFeedback || !m_par->SelectVlcTables)
        return;

    //choose tables
    Ipp32s BestPat= IPP_MIN(IPP_MIN(m_cur.CbpcyRate[0],m_cur.CbpcyRate[1]),IPP_MIN(m_cur.CbpcyRate[2],m_cur.CbpcyRate[3]));
    Ipp32s BestMV= IPP_MIN(IPP_MIN(m_cur.MvRate[0],m_cur.MvRate[1]),IPP_MIN(m_cur.MvRate[2],m_cur.MvRate[3]));
    Ipp32s BestAC= IPP_MIN(IPP_MIN(m_cur.AcRate[0],m_cur.AcRate[1]),IPP_MIN(m_cur.AcRate[2],m_cur.AcRate[2]));
    for(Ipp32s idx=0; idx<4; idx++){
        if(m_cur.CbpcyRate[idx]==BestPat)m_par->OutCbpcyTableIndex=idx;
        if(m_cur.MvRate[idx]==BestMV)m_par->OutMvTableIndex=idx;
        if(idx!=3&&m_cur.AcRate[idx]==BestAC)m_par->OutAcTableIndex=idx;
    }

    //CbpcyTableIndex=3&rand();
    //MvTableIndex=3&rand();
    //AcTableIndex=rand()%3;
}

void MeBase::UpdateVlcTableStatistic()
{
    if(!m_par->UseFeedback || !m_par->SelectVlcTables)
        return;
    MeMB *mb=&m_par->pSrc->MBs[m_cur.adr];
    bool intra = ((MeMbType)(mb->MbType) == ME_MbIntra);

    if(mb->MbType != (Ipp8u)ME_MbIntra && mb->MbType != (Ipp8u)ME_MbFrw)
        return;

    if(m_par->UseTrellisQuantization)m_cur.VlcTableSearchInProgress = true;
    //for all table set
    for(Ipp32s idx=0; idx<4; idx++){
        //select tables
        m_cur.CbpcyTableIndex=m_cur.MvTableIndex=m_cur.AcTableIndex=idx;
        if(idx==3)m_cur.AcTableIndex=2;

        //3 AC tables
        MeMV mv;
        mv = m_cur.BestMV[m_cur.RefDir][m_cur.RefIdx][0];
        MeMV pred = m_PredCalc->GetPrediction(mv);

        MeAdr src, ref; src.chroma=ref.chroma=true; //true means load also chroma plane addresses
        MakeRefAdrFromMV(m_cur.MbPart, m_par->PixelType, mv, &ref);
        MakeSrcAdr(m_cur.MbPart, m_par->PixelType, &src);
        //MeCostRD tmp= GetCostRD(ME_InterRD,ME_Mb16x16,ME_Tranform8x8,&src,&ref);


        MeCostRD tmp=0;
        for(Ipp32s blk=0; blk<6;blk++){
            src.channel = (blk<4?Y:(blk==4?U:V));
            m_cur.BlkIdx = blk; //it is for DC prediction calculation
            if(intra)
                tmp+=GetCostRD(ME_IntraRD, ME_Mb8x8, ME_Tranform8x8, &src, NULL);  //can be called as 16x16
            else
                tmp+=GetCostRD(ME_InterRD, ME_Mb8x8, m_cur.InterTransf[mb->MbType][blk], &src, &ref);
            Ipp32s dx=8, dy=0;
            if(blk==1){dx=-dx; dy=8;}
            src.move(Y,dx,dy);
            if(!intra)ref.move(Y,dx,dy);
        }
        m_cur.BlkIdx =0;

        m_cur.AcRate[idx] += tmp.R;
        
        //3 CBPCY tables
        const Ipp16u*  pCBPCYTable=VLCTableCBPCY_PB[m_cur.CbpcyTableIndex];
        if(tmp.BlkPat!=0){
            m_cur.CbpcyRate[idx]+=pCBPCYTable[2*tmp.BlkPat+1];
        }
        //if(!intra&&m_cur.HybridPredictor)m_cur.CbpcyRate[idx]++;  //1 just for testing!
#ifdef ME_DEBUG
        if(!intra&&m_cur.HybridPredictor)NumOfHibrids++;
#endif
        //3 MV tables
        if(!intra){
            m_par->pSrc->MBs[m_cur.adr].NumOfNZ=tmp.NumOfCoeff;
            bool bNonDominant = m_PredCalc->GetNonDominant(m_cur.RefDir,m_cur.RefIdx,idx);
            bool hybrid = GetHybrid(m_cur.RefIdx);
            m_cur.MvRate[idx]+=GetMvSize(mv.x-pred.x,mv.y-pred.y,bNonDominant,hybrid);
        }else{
            Ipp32s         NotSkip =   (tmp.BlkPat != 0)?1:0;
            const Ipp16u*   MVDiffTables=MVDiffTablesVLC[m_cur.MvTableIndex];
            m_cur.MvRate[idx]+=MVDiffTables[2*(37*NotSkip + 36 - 1)+1];
        }
    }       

    //restore original tables
    m_cur.CbpcyTableIndex = m_par->CbpcyTableIndex;
    m_cur.MvTableIndex = m_par->MvTableIndex;
    m_cur.AcTableIndex = m_par->AcTableIndex;
    m_cur.VlcTableSearchInProgress = false;
}

///Function select best transform mode for Inter MB and return its cost.
///It is standard dependend functionaliry, so see derived class for implementation.
Ipp32s MeBase::MakeTransformDecisionInterFast(MeMbPart mt, MePixelType pix,
                                           MeMbType type,
                                           MeMV mvF, MeMV mvB,
                                           Ipp32s IdxF, Ipp32s IdxB)
{
    return 0;
}

MeCostRD MeBase::MakeTransformDecisionInter(MeMbPart mt, MePixelType pix,
                                           MeMbType type,
                                           MeMV mvF, MeMV mvB,
                                           Ipp32s IdxF, Ipp32s IdxB)
{
    MeCostRD cost;
    cost.R=ME_BIG_COST;
    cost.D=ME_BIG_COST;
    return cost;
}

void MeBase::MakeMBModeDecision16x16()
{
    if(!m_par->UseFeedback)
        MakeMBModeDecision16x16Org();
    else
        MakeMBModeDecision16x16ByFB();
}


void MeBase::MakeMBModeDecision16x16Org()
{
    if(m_par->SearchDirection == ME_ForwardSearch){
        Ipp32s bestCost = ME_BIG_COST;
        if(m_cur.SkipCost[0] < m_SkippedThreshold){
            //skip
            m_cur.BestIdx[frw][0] = m_cur.SkipIdx[frw][0];
            m_cur.BestIdx[bkw][0] = -1;
            SetMB16x16B(m_cur.SkipType[0], m_cur.PredMV[frw][m_cur.SkipIdx[frw][0]][0], 0, m_cur.SkipCost[0]);
            bestCost = m_cur.SkipCost[0];
        }else if(m_cur.IntraCost[0]<m_cur.InterCost[0]){
            //intra
            SetMB16x16B(ME_MbIntra, 0, 0, ME_BIG_COST);
            bestCost = m_cur.InterCost[0]+1;
        }else{
            //inter
            if(m_par->UseVarSizeTransform)
            {
                MakeTransformDecisionInterFast(ME_Mb16x16, m_par->PixelType,
                                               ME_MbFrw,
                                               m_cur.BestMV[m_cur.RefDir][m_cur.BestIdx[frw][0]][0], MeMV(0),
                                               m_cur.BestIdx[frw][0], 0);
            }
            SetMB16x16B(ME_MbFrw, m_cur.BestMV[m_cur.RefDir][m_cur.RefIdx][0], 0, m_cur.InterCost[0]);
            SetError((vm_char*)"Wrong decision in MakeMBModeDecision16x16", m_cur.InterCost[0]>=ME_BIG_COST);
            bestCost = m_cur.InterCost[0];
        }
        if(ChangeInterpPixelType)
        {
            Ipp32s index = m_par->Interpolation + (m_par->PixelType >> 1);
            CostOnInterpolation[index] += bestCost;
        }
    }else{
        //Bidir
        MeMV mvF, mvB;

        m_PredCalc->SetDefFrwBkw(mvF, mvB);

        if(m_cur.SkipCost[0] < m_SkippedThreshold){
            //skip
            switch(m_cur.SkipType[0]){
                case ME_MbFrwSkipped:
                    m_cur.BestIdx[frw][0] = m_cur.SkipIdx[frw][0];
                    mvF = m_cur.PredMV[frw][m_cur.SkipIdx[frw][0]][0];
                    break;
                case ME_MbBkwSkipped:
                    m_cur.BestIdx[bkw][0] = m_cur.SkipIdx[bkw][0];
                    mvB = m_cur.PredMV[bkw][m_cur.SkipIdx[bkw][0]][0];
                    break;
                case ME_MbBidirSkipped:
                    m_cur.BestIdx[frw][0] = m_cur.SkipIdx[frw][0];
                    m_cur.BestIdx[bkw][0] = m_cur.SkipIdx[bkw][0];;
                    mvF = m_cur.PredMV[frw][m_cur.SkipIdx[frw][0]][0];
                    mvB = m_cur.PredMV[bkw][m_cur.SkipIdx[bkw][0]][0];
                    break;
            }
            SetMB16x16B(m_cur.SkipType[0], mvF, mvB, m_cur.SkipCost[0]);
        }else if(m_cur.DirectCost<m_SkippedThreshold ||m_cur.DirectCost<m_cur.InterCost[0]){
            //direct or direct skip
            m_cur.BestIdx[frw][0] = m_cur.DirectIdx[frw];
            m_cur.BestIdx[bkw][0] = m_cur.DirectIdx[bkw];;
            mvF = m_par->pRefF[m_cur.DirectIdx[frw]]->MVDirect[m_cur.adr];
            mvB = m_par->pRefB[m_cur.DirectIdx[bkw]]->MVDirect[m_cur.adr];
            if(m_cur.DirectType == ME_MbDirect && m_par->UseVarSizeTransform)
            {
                MakeTransformDecisionInterFast(ME_Mb16x16, m_par->PixelType,
                                               ME_MbDirect,
                                               mvF, mvB,
                                               m_cur.BestIdx[frw][0], m_cur.BestIdx[bkw][0]);
            }
            SetMB16x16B(m_cur.DirectType, mvF, mvB, m_cur.DirectCost);
        }else if(m_cur.IntraCost[0]<m_cur.InterCost[0]){
            //intra
            SetMB16x16B(ME_MbIntra, 0, 0, ME_BIG_COST);
        }else{
            //Inter
            switch(m_cur.InterType[0]){
                case ME_MbFrw:
                    mvF = m_cur.BestMV[frw][  m_cur.BestIdx[frw][0]  ][0];
                    break;
                case ME_MbBkw:
                    mvB = m_cur.BestMV[bkw][  m_cur.BestIdx[bkw][0]  ][0];
                    break;
                case ME_MbBidir:
                    mvF = m_cur.BestMV[frw][  m_cur.BestIdx[frw][0]  ][0];
                    mvB = m_cur.BestMV[bkw][  m_cur.BestIdx[bkw][0]  ][0];
                    break;
            }
            if(m_par->UseVarSizeTransform)
            {
                MakeTransformDecisionInterFast(ME_Mb16x16, m_par->PixelType,
                                               m_cur.InterType[0],
                                               mvF, mvB,
                                               m_cur.BestIdx[frw][0], m_cur.BestIdx[bkw][0]);
            }
            SetMB16x16B(m_cur.InterType[0], mvF, mvB, m_cur.InterCost[0]);
            SetError((vm_char*)"Wrong decision in MakeMBModeDecision16x16", m_cur.InterCost[0]>=ME_BIG_COST);
        }
    }

}

void MeBase::MakeMBModeDecision16x16ByFB()
{
    m_cur.InterCost[0] = ME_BIG_COST; //it was reset in EstimateInter
    if(m_par->UseFastFeedback)
    {
        if(m_par->SearchDirection == ME_ForwardSearch)
            ModeDecision16x16ByFBFastFrw();
        else
            ModeDecision16x16ByFBFastBidir();
    }
    else
    {
        if(m_par->SearchDirection == ME_ForwardSearch)
            ModeDecision16x16ByFBFullFrw();
        else
            ModeDecision16x16ByFBFullBidir();
    }
}
void  MeBase::MakeSkipModeDecision16x16BidirByFB()
{
    // SKIP
    if(!m_par->ProcessSkipped)
        return;

    MeCostRD InterCostRD = 0;
    Ipp32s SkipCost,IdxF,IdxB;
    bool FOutOfBound = false, BOutOfBound = false;
    //m_cur.SkipCost[0] = ME_BIG_COST;

    MeMB *mb=&m_par->pSrc->MBs[m_cur.adr];
    for(IdxF = 0; IdxF < m_par->FRefFramesNum; IdxF++)
    {
        m_cur.RefDir = frw;
        m_cur.RefIdx = IdxF;

        InterCostRD.D=EstimatePoint(m_cur.MbPart, m_par->PixelType, ME_SSD, m_cur.PredMV[frw][IdxF][m_cur.BlkIdx]);
        FOutOfBound = (InterCostRD.D == ME_BIG_COST)? true: false;
        if(!FOutOfBound)
        {
            AddHeaderCost(InterCostRD, NULL, ME_MbFrwSkipped,IdxF, 0);
            SkipCost = (Ipp32s)( m_InterRegFunD.Weight(InterCostRD.D)+m_lambda*m_InterRegFunR.Weight(InterCostRD.R));
        }
        else
        {
            SkipCost = ME_BIG_COST;
        }

        if(m_cur.SkipCost[0] > SkipCost)
        {
            m_cur.SkipCost[0] = SkipCost;
            m_cur.SkipIdx[frw][0] = IdxF;
            m_cur.SkipType[0] = ME_MbFrwSkipped;
            mb->SkippedCostRD = InterCostRD;
        }
        for(IdxB = 0; IdxB < m_par->BRefFramesNum; IdxB++)
        {
            m_cur.RefDir = bkw;
            m_cur.RefIdx = IdxB;

            InterCostRD.D=EstimatePoint(m_cur.MbPart, m_par->PixelType, ME_SSD, m_cur.PredMV[bkw][IdxB][m_cur.BlkIdx]);
            BOutOfBound = (InterCostRD.D == ME_BIG_COST)? true: false;
            if(!BOutOfBound)
            {
                AddHeaderCost(InterCostRD, NULL, ME_MbBkwSkipped,IdxF,IdxB);
                SkipCost = (Ipp32s)( m_InterRegFunD.Weight(InterCostRD.D)+m_lambda*m_InterRegFunR.Weight(InterCostRD.R));
            }
            else
            {
                SkipCost = ME_BIG_COST;
            }

            if(m_cur.SkipCost[0] > SkipCost)
            {
                m_cur.SkipCost[0] = SkipCost;
                m_cur.SkipIdx[bkw][0] = IdxB;
                m_cur.SkipType[0] = ME_MbBkwSkipped;
                mb->SkippedCostRD = InterCostRD;
            }
            //bidir
            if(!FOutOfBound && !BOutOfBound)
            {
                InterCostRD.D=EstimatePointAverage(m_cur.MbPart, m_par->PixelType, ME_SSD,
                                        frw, IdxF, m_cur.PredMV[frw][IdxF][m_cur.BlkIdx],
                                        bkw, IdxB, m_cur.PredMV[bkw][IdxB][m_cur.BlkIdx]);
                AddHeaderCost(InterCostRD, NULL, ME_MbBidirSkipped,IdxF,IdxB);
                SkipCost = (Ipp32s)( m_InterRegFunD.Weight(InterCostRD.D)+m_lambda*m_InterRegFunR.Weight(InterCostRD.R));
                if(m_cur.SkipCost[0] > SkipCost)
                {
                    m_cur.SkipCost[0] = SkipCost;
                    m_cur.SkipIdx[frw][0] = IdxF;
                    m_cur.SkipIdx[bkw][0] = IdxB;
                    m_cur.SkipType[0] = ME_MbBidirSkipped;
                    mb->SkippedCostRD = InterCostRD;
                }
            }
        }
    }

    if(m_DirectOutOfBound) return;

    //Direct skipped
    MeMV MVDirectFW, MVDirectBW;
    InterCostRD = 0;

    IdxF = m_cur.DirectIdx[frw];
    IdxB = m_cur.DirectIdx[bkw];

    MVDirectFW = m_par->pRefF[IdxF]->MVDirect[m_cur.adr];
    MVDirectBW = m_par->pRefB[IdxB]->MVDirect[m_cur.adr];

    InterCostRD.D=EstimatePointAverage(m_cur.MbPart, m_par->PixelType, ME_SSD,
                            frw, IdxF, MVDirectFW,
                            bkw, IdxB, MVDirectBW);
    AddHeaderCost(InterCostRD, NULL, ME_MbDirectSkipped,IdxF,IdxB);
    m_cur.DirectCost = (Ipp32s)( m_InterRegFunD.Weight(InterCostRD.D)+m_lambda*m_InterRegFunR.Weight(InterCostRD.R));
    m_cur.DirectType = ME_MbDirectSkipped;
    mb->DirectSkippedCostRD = InterCostRD;
}

void MeBase::SetModeDecision16x16BidirByFB(MeMV &mvF, MeMV &mvB)
{
    Ipp32s bestCost = IPP_MIN(IPP_MIN(m_cur.IntraCost[0],m_cur.InterCost[0]),IPP_MIN(m_cur.DirectCost,m_cur.SkipCost[0]));

    if(bestCost == m_cur.SkipCost[0]){
        //skip
        switch(m_cur.SkipType[0]){
            case ME_MbFrwSkipped:
                m_cur.BestIdx[frw][0] = m_cur.SkipIdx[frw][0];
                mvF = m_cur.PredMV[frw][m_cur.SkipIdx[frw][0]][0];
                m_PredCalc->SetDefMV(mvB, bkw);
                break;
            case ME_MbBkwSkipped:
                m_cur.BestIdx[bkw][0] = m_cur.SkipIdx[bkw][0];
                mvB = m_cur.PredMV[bkw][m_cur.SkipIdx[bkw][0]][0];
                m_PredCalc->SetDefMV(mvF, frw);
                break;
            case ME_MbBidirSkipped:
                m_cur.BestIdx[frw][0] = m_cur.SkipIdx[frw][0];
                m_cur.BestIdx[bkw][0] = m_cur.SkipIdx[bkw][0];;
                mvF = m_cur.PredMV[frw][m_cur.SkipIdx[frw][0]][0];
                mvB = m_cur.PredMV[bkw][m_cur.SkipIdx[bkw][0]][0];
                break;
        }
        SetMB16x16B(m_cur.SkipType[0], mvF, mvB, m_cur.SkipCost[0]);
    }else if(bestCost == m_cur.DirectCost){
        //direct or direct skip
        m_cur.BestIdx[frw][0] = m_cur.DirectIdx[frw];
        m_cur.BestIdx[bkw][0] = m_cur.DirectIdx[bkw];;
        mvF = m_par->pRefF[m_cur.DirectIdx[frw]]->MVDirect[m_cur.adr];
        mvB = m_par->pRefB[m_cur.DirectIdx[bkw]]->MVDirect[m_cur.adr];
        if(m_cur.DirectType == ME_MbDirect)
        {
            MeMB *mb=&m_par->pSrc->MBs[m_cur.adr];
            mb->InterCostRD = mb->DirectCostRD;
        }
        SetMB16x16B(m_cur.DirectType, mvF, mvB, m_cur.DirectCost);
    }else if(bestCost == m_cur.IntraCost[0]){
        //intra
        SetMB16x16B(ME_MbIntra, 0, 0, ME_BIG_COST);
    }else{
        //Inter
        switch(m_cur.InterType[0]){
            case ME_MbFrw:
                mvF = m_cur.BestMV[frw][  m_cur.BestIdx[frw][0]  ][0];
                m_PredCalc->SetDefMV(mvB, bkw);
                break;
            case ME_MbBkw:
                mvB = m_cur.BestMV[bkw][  m_cur.BestIdx[bkw][0]  ][0];
                m_PredCalc->SetDefMV(mvF, frw);
                break;
            case ME_MbBidir:
                mvF = m_cur.BestMV[frw][  m_cur.BestIdx[frw][0]  ][0];
                mvB = m_cur.BestMV[bkw][  m_cur.BestIdx[bkw][0]  ][0];
                break;
        }
        SetMB16x16B(m_cur.InterType[0], mvF, mvB, m_cur.InterCost[0]);
        SetError((vm_char*)"Wrong decision in MakeMBModeDecision16x16", m_cur.InterCost[0]>=ME_BIG_COST);
    }
}

void MeBase::ModeDecision16x16ByFBFastFrw()
{
    SetError((vm_char*)"Wrong MbPart in MakeMBModeDecision16x16ByFB", m_par->MbPart != ME_Mb16x16);
    MeMB *mb=&m_par->pSrc->MBs[m_cur.adr];

    // SKIP
    Ipp32s SkipCost = ME_BIG_COST;
    if(m_par->ProcessSkipped)
        SkipCost=EstimatePoint(m_cur.MbPart, m_par->PixelType, ME_SSD, m_cur.PredMV[frw][0][m_cur.BlkIdx]);

    MeMV mv;
    Ipp32s InterCost, IntraCost;

    // INTER
    mv = m_cur.BestMV[m_cur.RefDir][m_cur.RefIdx][0];
    MeMV pred = m_cur.PredMV[frw][m_cur.BestIdx[frw][0]][0];
    mb->PureSAD = EstimatePoint(m_cur.MbPart, m_par->PixelType, ME_Sad, mv);
    mb->InterCostRD.D = EstimatePoint(m_cur.MbPart, m_par->PixelType, ME_Sadt, mv);
    InterCost = (Ipp32s)( m_InterRegFunD.Weight(mb->InterCostRD.D)+m_lambda*m_MvRegrFunR.Weight(mv,pred));
    // INTRA
    mb->IntraCostRD.D=EstimatePoint(m_cur.MbPart, m_par->PixelType, ME_SadtSrcNoDC, 0);
    IntraCost = (Ipp32s)(m_IntraRegFunD.Weight(mb->IntraCostRD.D));

    //for preset calculation
    #ifdef ME_GENERATE_PRESETS
        SkipCost=IPP_MAX_32S;
        InterCost  = rand()/2;
        IntraCost  = rand()/2;
    #endif

    //make decision
    Ipp32s bestCost=IPP_MIN(IPP_MIN(SkipCost,InterCost),IntraCost);
    if(bestCost==SkipCost){
        //skip
        SetMB16x16B(ME_MbFrwSkipped, m_cur.PredMV[frw][0][0], 0, m_cur.SkipCost[0]);
    }else if(bestCost == IntraCost){
        //intra
        SetMB16x16B(ME_MbIntra, 0, 0, 0xBAD);
    }else{
            //inter
        SetMB16x16B(ME_MbFrw, m_cur.BestMV[m_cur.RefDir][m_cur.RefIdx][0], 0, m_cur.InterCost[0]);
        SetError((vm_char*)"Wrong decision in MakeMBModeDecision16x16ByFB", m_cur.InterCost[0]>=ME_BIG_COST);
    }
}

void MeBase::ModeDecision16x16ByFBFastBidir()
{
    MeMV mvF, mvB;
    Ipp32s IdxF, IdxB;

    SetError((vm_char*)"Wrong MbPart in MakeMBModeDecision16x16ByFB", m_par->MbPart != ME_Mb16x16);

    MeMB *mb=&m_par->pSrc->MBs[m_cur.adr];

    m_PredCalc->SetDefFrwBkw(mvF, mvB);

    MakeSkipModeDecision16x16BidirByFB();

    Ipp32s InterCost, PureSAD, InterCostD;

    //Direct
    MeMV MVDirectFW, MVDirectBW;
    Ipp32s DirectCost;

    IdxF = m_cur.DirectIdx[frw];
    IdxB = m_cur.DirectIdx[bkw];

    MVDirectFW = m_par->pRefF[IdxF]->MVDirect[m_cur.adr];
    MVDirectBW = m_par->pRefB[IdxB]->MVDirect[m_cur.adr];

    //Direct:
    if(m_par->ProcessDirect && !m_DirectOutOfBound)
    {
        PureSAD = EstimatePointAverage(m_cur.MbPart, m_par->PixelType,
                        ME_Sad, frw, IdxF, MVDirectFW, bkw, IdxB, MVDirectBW);
        DirectCost = EstimatePointAverage(m_cur.MbPart, m_par->PixelType,
                        ME_Sadt, frw, IdxF, MVDirectFW, bkw, IdxB, MVDirectBW);
        DirectCost = (Ipp32s)m_InterRegFunD.Weight(DirectCost);

        if(DirectCost < m_cur.DirectCost)
        {
            m_cur.DirectType = ME_MbDirect;
            m_cur.DirectCost = DirectCost;
        }
    }
    //Inter
    //m_cur.InterCost[0] = ME_BIG_COST;
    for(Ipp32s i=0; i< m_par->FRefFramesNum; i++)
    {
        MeMV mv = m_cur.BestMV[frw][i][0];
        MeMV pred = m_cur.PredMV[frw][i][0];
        PureSAD = EstimatePoint(m_cur.MbPart, m_par->PixelType, ME_Sad, mv);
        InterCostD = EstimatePoint(m_cur.MbPart, m_par->PixelType, ME_Sadt, mv);
        InterCost = (Ipp32s)( m_InterRegFunD.Weight(InterCostD)+m_lambda*m_MvRegrFunR.Weight(mv,pred));

        if(InterCost<m_cur.InterCost[0]){
            m_cur.InterCost[0] = InterCost;
            m_cur.InterType[0] = ME_MbFrw;
            m_cur.BestIdx[frw][0] = i;
           /// m_cur.BestIdx[bkw][0] = -1;//?
            mb->PureSAD = PureSAD;
            mb->InterCostRD.D = InterCostD;
        }
        for(Ipp32s j=0; j< m_par->BRefFramesNum; j++)
        {
            MeMV mv = m_cur.BestMV[bkw][j][0];
            MeMV pred = m_cur.PredMV[bkw][j][0];
            PureSAD = EstimatePoint(m_cur.MbPart, m_par->PixelType, ME_Sad, mv);
            InterCostD = EstimatePoint(m_cur.MbPart, m_par->PixelType, ME_Sadt, mv);
            InterCost = (Ipp32s)( m_InterRegFunD.Weight(InterCostD)+m_lambda*m_MvRegrFunR.Weight(mv,pred));

            if(InterCost<m_cur.InterCost[0]){
                m_cur.InterCost[0] = InterCost;
                m_cur.InterType[0] = ME_MbBkw;
                m_cur.BestIdx[bkw][0] = j;
               // m_cur.BestIdx[frw][0] = -1;
                mb->PureSAD = PureSAD;
                mb->InterCostRD.D = InterCostD;
            }

            MeMV mv0 = m_cur.BestMV[frw][i][0];
            MeMV pred0 = m_cur.PredMV[frw][i][0];
            MeMV mv1 = m_cur.BestMV[bkw][j][0];
            MeMV pred1 = m_cur.PredMV[bkw][j][0];
            PureSAD = EstimatePointAverage(m_cur.MbPart, m_par->PixelType,
                        ME_Sad, frw, i, mv0, bkw, j, mv1);
            InterCostD = EstimatePointAverage(m_cur.MbPart, m_par->PixelType,
                        ME_Sadt, frw, i, mv0, bkw, j, mv1);
            InterCost = (Ipp32s)( m_InterRegFunD.Weight(InterCostD)
                                    +m_lambda*(m_MvRegrFunR.Weight(mv0,pred0)+m_MvRegrFunR.Weight(mv1,pred1)));

            if(InterCost<m_cur.InterCost[0]){
                m_cur.InterCost[0] = InterCost;
                m_cur.InterType[0] = ME_MbBidir;
                m_cur.BestIdx[frw][0] = i;
                m_cur.BestIdx[bkw][0] = j;
                mb->PureSAD = PureSAD;
                mb->InterCostRD.D = InterCostD;
            }
        }//for(j)
    }//for(i)

    //INTRA
    mb->IntraCostRD.D=EstimatePoint(m_cur.MbPart, m_par->PixelType, ME_SadtSrcNoDC, 0);
    m_cur.IntraCost[0] = (Ipp32s)(m_IntraRegFunD.Weight(mb->IntraCostRD.D));

    SetModeDecision16x16BidirByFB(mvF, mvB);
}

void MeBase::ModeDecision16x16ByFBFullFrw()
{
    MeMB *mb=&m_par->pSrc->MBs[m_cur.adr];

    Ipp32s FbestCost = ME_BIG_COST;
    Ipp32s bestIdx = 0;
    Ipp32s bestType = ME_MbFrw;
    MeCostRD InterCostRD;
    Ipp32s PureSAD;

    MeMV mv;
    Ipp32s InterCost, IntraCost, SkipCost;


    // INTRA
    MeAdr src; src.chroma=true;
    MakeSrcAdr(m_cur.MbPart, ME_IntegerPixel, &src);
    mb->IntraCostRD = GetCostRD(ME_IntraRD,ME_Mb16x16,ME_Tranform8x8,&src,NULL);
    AddHeaderCost(mb->IntraCostRD, NULL, ME_MbIntra,0,0);
    IntraCost = (Ipp32s)(m_IntraRegFunD.Weight(mb->IntraCostRD.D) + m_lambda*m_IntraRegFunR.Weight(mb->IntraCostRD.R));
    if(IntraCost<0) IntraCost= ME_BIG_COST;

    for(Ipp32s i=0; i< m_par->FRefFramesNum; i++)
    {
        m_cur.RefDir = frw;
        m_cur.RefIdx = i;
        // SKIP
        SkipCost = ME_BIG_COST;
        if(m_par->ProcessSkipped)
        {
            SkipCost=EstimatePoint(m_cur.MbPart, m_par->PixelType, ME_SSD, m_cur.PredMV[frw][i][m_cur.BlkIdx]);
            mb->SkippedCostRD.D = SkipCost;
            mb->SkippedCostRD.R = 1;
        }

        // INTER
        // TODO: rewrite this, currently we use previous MB value for MV weighting, should we use another?
        mv = m_cur.BestMV[m_cur.RefDir][m_cur.RefIdx][0];
        MeMV pred = m_PredCalc->GetPrediction(mv);
        PureSAD = EstimatePoint(m_cur.MbPart, m_par->PixelType, ME_Sad, mv);
        if(m_par->UseVarSizeTransform){
            InterCostRD = MakeTransformDecisionInter(m_cur.MbPart, m_par->PixelType, 
                        ME_MbFrw, mv, MeMV(0),m_cur.RefIdx,0);
        }else{
            // TODO: replace this by EstimatePoint or MakeTransformDecisionInter
            MeAdr src, ref; src.chroma=ref.chroma=true; //true means load also chroma plane addresses
            MakeRefAdrFromMV(m_cur.MbPart, m_par->PixelType, mv, &ref);
            MakeSrcAdr(m_cur.MbPart, m_par->PixelType, &src);
            InterCostRD = GetCostRD(ME_InterRD,ME_Mb16x16,ME_Tranform8x8,&src,&ref);
            AddHeaderCost(InterCostRD, NULL, ME_MbFrw,i,0);
        }
        m_par->pSrc->MBs[m_cur.adr].NumOfNZ=InterCostRD.NumOfCoeff;
        bool bNonDominant = m_PredCalc->GetNonDominant(frw,i,0);
        bool hybrid = GetHybrid(i);
        InterCost = (Ipp32s)( m_InterRegFunD.Weight(InterCostRD.D)+m_lambda*(m_InterRegFunR.Weight(InterCostRD.R)+GetMvSize(mv.x-pred.x,mv.y-pred.y,bNonDominant,hybrid)));
        if(InterCost<0) InterCost= ME_BIG_COST;

//for preset calculation
#ifdef ME_GENERATE_PRESETS
    SkipCost=IPP_MAX_32S;
    InterCost  = rand()/2;
    IntraCost  = rand()/2;
#endif

    //make decision
    Ipp32s bestCost=IPP_MIN(IPP_MIN(SkipCost,InterCost),IntraCost);
    /*
        if(FbestCost > bestCost)
        {
            FbestCost = bestCost;
            bestIdx = i;
            if(bestCost==SkipCost){
                //skip              
                bestType = ME_MbFrwSkipped;
            }else if(bestCost == InterCost){
                //inter
                mb->InterCostRD = InterCostRD;
                mb->PureSAD = PureSAD;
               // m_par->pSrc->MBs[m_cur.adr].NumOfNZ=InterCostRD.NumOfCoeff;
                bestType = ME_MbFrw;
            }else{
                //intra 
                bestType = ME_MbIntra;
            }
        }
        */
        if(FbestCost > bestCost)
        {
            FbestCost = bestCost;
            bestIdx = i;
            if(bestCost==SkipCost){
                //skip              
                bestType = ME_MbFrwSkipped;
            }else if(bestCost == IntraCost){
                //intra
                bestType = ME_MbIntra;
            }else{
                //inter
                mb->InterCostRD = InterCostRD;
                mb->PureSAD = PureSAD;
               // m_par->pSrc->MBs[m_cur.adr].NumOfNZ=InterCostRD.NumOfCoeff;
                bestType = ME_MbFrw;
            }
        }
    }//for(i)
    m_cur.BestIdx[frw][0] = bestIdx;
    if(bestType == ME_MbFrwSkipped){
        //skip
        SetMB16x16B(ME_MbFrwSkipped, m_cur.PredMV[frw][bestIdx][0], 0, FbestCost);
    }else if(bestType == ME_MbIntra){
        //intra
        SetMB16x16B(ME_MbIntra, 0, 0, FbestCost);
    }else{
        //inter
        SetMB16x16B(ME_MbFrw, m_cur.BestMV[frw][bestIdx][0], 0, FbestCost);
        SetError((vm_char*)"Wrong decision in MakeMBModeDecision16x16ByFB", m_cur.InterCost[0]>=ME_BIG_COST);
    }
}

void MeBase::ModeDecision16x16ByFBFullBidir()
{
    MeMV mvF, mvB;
    Ipp32s IdxF, IdxB;

    SetError((vm_char*)"Wrong MbPart in MakeMBModeDecision16x16ByFB", m_par->MbPart != ME_Mb16x16);

    MeMB *mb=&m_par->pSrc->MBs[m_cur.adr];
    m_PredCalc->SetDefFrwBkw(mvF, mvB);

    MakeSkipModeDecision16x16BidirByFB();

    Ipp32s InterCost, PureSAD;
    MeCostRD InterCostRD;

    //Direct
    MeMV MVDirectFW, MVDirectBW;
    Ipp32s DirectCost;

    IdxF = m_cur.DirectIdx[frw];
    IdxB = m_cur.DirectIdx[bkw];

    MVDirectFW = m_par->pRefF[IdxF]->MVDirect[m_cur.adr];
    MVDirectBW = m_par->pRefB[IdxB]->MVDirect[m_cur.adr];

    MeAdr src, ref; src.chroma=ref.chroma=true; //true means load also chroma plane addresses
    MeAdr ref0, ref1; ref0.chroma=ref1.chroma=true; //true means load also chroma plane addresses

    if(m_par->ProcessDirect && !m_DirectOutOfBound)
    {
        PureSAD = EstimatePointAverage(m_cur.MbPart, m_par->PixelType,
                    ME_Sad, frw, IdxF, MVDirectFW, bkw, IdxB, MVDirectBW);
        if(m_par->UseVarSizeTransform)
        {
            InterCostRD = MakeTransformDecisionInter(m_cur.MbPart, m_par->PixelType, 
                        ME_MbDirect, MVDirectFW, MVDirectBW,IdxF,IdxB);
            DirectCost = (Ipp32s)( m_InterRegFunD.Weight(InterCostRD.D)+m_lambda*m_InterRegFunR.Weight(InterCostRD.R));
            if(DirectCost<0) DirectCost= ME_BIG_COST;
            mb->DirectCostRD = InterCostRD;
        }
        else
        {
            // TODO: replace this by EstimatePoint or MakeTransformDecisionInter
            m_cur.RefDir = frw;
            m_cur.RefIdx = IdxF;
            MakeRefAdrFromMV(m_cur.MbPart, m_par->PixelType, MVDirectFW, &ref0);
            m_cur.RefDir = bkw;
            m_cur.RefIdx = IdxB;
            MakeRefAdrFromMV(m_cur.MbPart, m_par->PixelType, MVDirectBW, &ref1);
            MakeSrcAdr(m_cur.MbPart, m_par->PixelType, &src);

            ippiAverage16x16_8u_C1R(ref0.ptr[Y], ref0.step[Y], ref1.ptr[Y], ref1.step[Y], m_cur.buf[bidir].ptr[Y], 16);
            ref.set(Y, m_cur.buf[bidir].ptr[Y], 16);

            if(m_CRFormat == ME_YUV420)
            {
                ippiAverage8x8_8u_C1R(ref0.ptr[U], ref0.step[U], ref1.ptr[U], ref1.step[U], m_cur.buf[bidir].ptr[U], 8);
                ref.set(U, m_cur.buf[bidir].ptr[U], 8);
                ippiAverage8x8_8u_C1R(ref0.ptr[V], ref0.step[V], ref1.ptr[V], ref1.step[V], m_cur.buf[bidir].ptr[V], 8);
                ref.set(V, m_cur.buf[bidir].ptr[V], 8);
            }
            else
            {
                ippiAverage8x8_8u_C1R(ref0.ptr[U], ref0.step[U], ref1.ptr[U], ref1.step[U], m_cur.buf[bidir].ptr[U], 16);
                ippiAverage8x8_8u_C1R(ref0.ptr[U]+8, ref0.step[U], ref1.ptr[U]+8, ref1.step[U], m_cur.buf[bidir].ptr[U]+8, 16);
                ref.set(U, m_cur.buf[bidir].ptr[U], 16);
            }

            InterCostRD = GetCostRD(ME_InterRD,ME_Mb16x16,ME_Tranform8x8,&src,&ref);
            AddHeaderCost(InterCostRD, NULL, ME_MbDirect,IdxF,IdxB);

            DirectCost = (Ipp32s)( m_InterRegFunD.Weight(InterCostRD.D)+m_lambda*m_InterRegFunR.Weight(InterCostRD.R));
            if(DirectCost<0) DirectCost= ME_BIG_COST;
            mb->DirectCostRD = InterCostRD;
        }
        if(DirectCost < m_cur.DirectCost)
        {
            m_cur.DirectType = ME_MbDirect;
            m_cur.DirectCost = DirectCost;
        }
    }
    //Inter
    for(Ipp32s i=0; i< m_par->FRefFramesNum; i++)
    {
        m_cur.RefDir = frw;
        m_cur.RefIdx = i;
        MeMV mv = m_cur.BestMV[frw][i][0];
        MeMV pred = m_cur.PredMV[frw][i][0];
        //MeMV pred = m_PredCalc->GetPrediction(mv);

        PureSAD = EstimatePoint(m_cur.MbPart, m_par->PixelType, ME_Sad, mv);
        if(m_par->UseVarSizeTransform)
        {
            InterCostRD = MakeTransformDecisionInter(m_cur.MbPart, m_par->PixelType, 
                        ME_MbFrw, mv, MeMV(0), i,0);

            bool bNonDominant = m_PredCalc->GetNonDominant(frw,i,0);
            bool hybrid = GetHybrid(i);
            InterCost = (Ipp32s)( m_InterRegFunD.Weight(InterCostRD.D)+m_lambda*(m_InterRegFunR.Weight(InterCostRD.R)+GetMvSize(mv.x-pred.x,mv.y-pred.y,bNonDominant,hybrid)));
            if(InterCost<0) InterCost= ME_BIG_COST;
        }
        else
        {
            // TODO: replace this by EstimatePoint or MakeTransformDecisionInter
            src.chroma=ref.chroma=true; //true means load also chroma plane addresses
            MakeRefAdrFromMV(m_cur.MbPart, m_par->PixelType, mv, &ref);
            MakeSrcAdr(m_cur.MbPart, m_par->PixelType, &src);
            InterCostRD = GetCostRD(ME_InterRD,ME_Mb16x16,ME_Tranform8x8,&src,&ref);
            AddHeaderCost(InterCostRD, NULL, ME_MbFrw,i,0);
            bool bNonDominant = m_PredCalc->GetNonDominant(frw,i,0);
            bool hybrid = GetHybrid(i);
            InterCost = (Ipp32s)( m_InterRegFunD.Weight(InterCostRD.D)+m_lambda*(m_InterRegFunR.Weight(InterCostRD.R)+GetMvSize(mv.x-pred.x,mv.y-pred.y,bNonDominant,hybrid)));
            if(InterCost<0) InterCost= ME_BIG_COST;
        }

        if(InterCost<m_cur.InterCost[0]){
            m_cur.InterCost[0] = InterCost;
            m_cur.InterType[0] = ME_MbFrw;
            m_cur.BestIdx[frw][0] = i;
            //m_cur.BestIdx[bkw][0] = -1;//?
            mb->PureSAD = PureSAD;
            m_par->pSrc->MBs[m_cur.adr].NumOfNZ=InterCostRD.NumOfCoeff;
            mb->InterCostRD = InterCostRD;
        }
        for(Ipp32s j=0; j< m_par->BRefFramesNum; j++)
        {
            m_cur.RefDir = bkw;
            m_cur.RefIdx = j;
            MeMV mv = m_cur.BestMV[bkw][j][0];
            MeMV pred = m_cur.PredMV[bkw][j][0];
            //MeMV pred = m_PredCalc->GetPrediction(mv);

            PureSAD = EstimatePoint(m_cur.MbPart, m_par->PixelType, ME_Sad, mv);
            if(m_par->UseVarSizeTransform)
            {
                InterCostRD = MakeTransformDecisionInter(m_cur.MbPart, m_par->PixelType, 
                            ME_MbBkw, MeMV(0),mv,0,j);
                bool bNonDominant = m_PredCalc->GetNonDominant(bkw,j,0);
                bool hybrid = GetHybrid(j);
                InterCost = (Ipp32s)( m_InterRegFunD.Weight(InterCostRD.D)+m_lambda*(m_InterRegFunR.Weight(InterCostRD.R)+GetMvSize(mv.x-pred.x,mv.y-pred.y,bNonDominant,hybrid)));                
                if(InterCost<0) InterCost= ME_BIG_COST;
            }
            else
            {
                // TODO: replace this by EstimatePoint or MakeTransformDecisionInter
                src.chroma=ref.chroma=true; //true means load also chroma plane addresses
                MakeRefAdrFromMV(m_cur.MbPart, m_par->PixelType, mv, &ref);
                MakeSrcAdr(m_cur.MbPart, m_par->PixelType, &src);
                InterCostRD = GetCostRD(ME_InterRD,ME_Mb16x16,ME_Tranform8x8,&src,&ref);
                AddHeaderCost(InterCostRD, NULL, ME_MbBkw,i,j);
                bool bNonDominant = m_PredCalc->GetNonDominant(bkw,j,0);
                bool hybrid = GetHybrid(j);
                InterCost = (Ipp32s)( m_InterRegFunD.Weight(InterCostRD.D)+m_lambda*(m_InterRegFunR.Weight(InterCostRD.R)+GetMvSize(mv.x-pred.x,mv.y-pred.y,bNonDominant,hybrid)));
                if(InterCost<0) InterCost= ME_BIG_COST;
            }

            if(InterCost<m_cur.InterCost[0]){
                m_cur.InterCost[0] = InterCost;
                m_cur.InterType[0] = ME_MbBkw;
                m_cur.BestIdx[bkw][0] = j;
               // m_cur.BestIdx[frw][0] = -1;//?
                mb->PureSAD = PureSAD;
                m_par->pSrc->MBs[m_cur.adr].NumOfNZ=InterCostRD.NumOfCoeff;
                mb->InterCostRD = InterCostRD;
            }

            MeMV mv0 = m_cur.BestMV[frw][i][0];
            MeMV pred0 = m_cur.PredMV[frw][i][0];
            //MeMV pred0 = m_PredCalc->GetPrediction(mv0);

            MeMV mv1 = m_cur.BestMV[bkw][j][0];
            MeMV pred1 = m_cur.PredMV[bkw][j][0];
            //MeMV pred1 = m_PredCalc->GetPrediction(mv1);

            PureSAD = EstimatePointAverage(m_cur.MbPart, m_par->PixelType,
                        ME_Sad, frw, i, mv0, bkw, j, mv1);

            if(m_par->UseVarSizeTransform)
            {
                InterCostRD = MakeTransformDecisionInter(m_cur.MbPart, m_par->PixelType, 
                            ME_MbBidir, mv0,mv1,i,j);
                bool bNonDominant0 = m_PredCalc->GetNonDominant(frw,i,0);
                bool bNonDominant1 = m_PredCalc->GetNonDominant(bkw,j,0);
                bool hybrid0 = GetHybrid(i);
                bool hybrid1 = GetHybrid(j);

                InterCost = (Ipp32s)( m_InterRegFunD.Weight(InterCostRD.D)+m_lambda*(m_InterRegFunR.Weight(InterCostRD.R)+GetMvSize(mv0.x-pred0.x,mv0.y-pred0.y,bNonDominant0,hybrid0)+GetMvSize(mv1.x-pred1.x,mv1.y-pred1.y,bNonDominant1,hybrid1)));                
                if(InterCost<0) InterCost= ME_BIG_COST;
            }
            else
            {
                // TODO: replace this by EstimatePoint or MakeTransformDecisionInter
                src.chroma=ref.chroma=true; //true means load also chroma plane addresses
                ref0.chroma=ref1.chroma=true; //true means load also chroma plane addresses
                m_cur.RefDir = frw;
                m_cur.RefIdx = i;
                MakeRefAdrFromMV(m_cur.MbPart, m_par->PixelType, mv0, &ref0);
                m_cur.RefDir = bkw;
                m_cur.RefIdx = j;
                MakeRefAdrFromMV(m_cur.MbPart, m_par->PixelType, mv1, &ref1);
                MakeSrcAdr(m_cur.MbPart, m_par->PixelType, &src);

                ippiAverage16x16_8u_C1R(ref0.ptr[Y], ref0.step[Y], ref1.ptr[Y], ref1.step[Y], m_cur.buf[bidir].ptr[Y], 16);
                ref.set(Y, m_cur.buf[bidir].ptr[Y], 16);
                if(m_CRFormat == ME_YUV420)
                {
                    ippiAverage8x8_8u_C1R(ref0.ptr[U], ref0.step[U], ref1.ptr[U], ref1.step[U], m_cur.buf[bidir].ptr[U], 8);
                    ref.set(U, m_cur.buf[bidir].ptr[U], 8);
                    ippiAverage8x8_8u_C1R(ref0.ptr[V], ref0.step[V], ref1.ptr[V], ref1.step[V], m_cur.buf[bidir].ptr[V], 8);
                    ref.set(V, m_cur.buf[bidir].ptr[V], 8);
                }
                else
                {
                    ippiAverage8x8_8u_C1R(ref0.ptr[U], ref0.step[U], ref1.ptr[U], ref1.step[U], m_cur.buf[bidir].ptr[U], 16);
                    ippiAverage8x8_8u_C1R(ref0.ptr[U]+8, ref0.step[U], ref1.ptr[U]+8, ref1.step[U], m_cur.buf[bidir].ptr[U]+8, 16);
                    ref.set(U, m_cur.buf[bidir].ptr[U], 16);
                }
                InterCostRD = GetCostRD(ME_InterRD,ME_Mb16x16,ME_Tranform8x8,&src,&ref);
                AddHeaderCost(InterCostRD, NULL, ME_MbBidir,i,j);

                bool bNonDominant0 = m_PredCalc->GetNonDominant(frw,i,0);
                bool bNonDominant1 = m_PredCalc->GetNonDominant(bkw,j,0);
                bool hybrid0 = GetHybrid(i);
                bool hybrid1 = GetHybrid(j);

                InterCost = (Ipp32s)( m_InterRegFunD.Weight(InterCostRD.D)+m_lambda*(m_InterRegFunR.Weight(InterCostRD.R)+GetMvSize(mv0.x-pred0.x,mv0.y-pred0.y,bNonDominant0,hybrid0)+GetMvSize(mv1.x-pred1.x,mv1.y-pred1.y,bNonDominant1,hybrid1)));
                if(InterCost<0) InterCost= ME_BIG_COST;
            }

            if(InterCost<m_cur.InterCost[0]){
                m_cur.InterCost[0] = InterCost;
                m_cur.InterType[0] = ME_MbBidir;
                m_cur.BestIdx[frw][0] = i;
                m_cur.BestIdx[bkw][0] = j;
                mb->PureSAD = PureSAD;
                m_par->pSrc->MBs[m_cur.adr].NumOfNZ=InterCostRD.NumOfCoeff;
                mb->InterCostRD = InterCostRD;
            }
        }
    }

    //INTRA
    src.chroma=true;
    MakeSrcAdr(m_cur.MbPart, ME_IntegerPixel, &src);
    mb->IntraCostRD = GetCostRD(ME_IntraRD,ME_Mb16x16,ME_Tranform8x8,&src,NULL);
    AddHeaderCost(mb->IntraCostRD, NULL, ME_MbIntra,0,0);
    m_cur.IntraCost[0] = (Ipp32s)(m_IntraRegFunD.Weight(mb->IntraCostRD.D) + m_lambda*m_IntraRegFunR.Weight(mb->IntraCostRD.R));
    if(m_cur.IntraCost[0]<0) m_cur.IntraCost[0]= ME_BIG_COST;

    SetModeDecision16x16BidirByFB(mvF, mvB);
}

//function performs mode decision for one block and saves results to m_cur.InterType.
void MeBase::MakeBlockModeDecision()
{
    //currently two blocks type are possible skip and inter
    // TODO: move this somewhere
    if(m_par->UseFeedback) return;

    if(m_cur.SkipCost[m_cur.BlkIdx]<m_SkippedThreshold/4){
        //skip
        Ipp32s dir = (m_cur.SkipType[m_cur.BlkIdx] == ME_MbFrwSkipped) ? frw : bkw;
        Ipp32s opp_dir = (dir == frw)? bkw : frw;
        Ipp32s idx = m_cur.SkipIdx[dir][m_cur.BlkIdx];
        m_cur.InterCost[m_cur.BlkIdx] = m_cur.SkipCost[m_cur.BlkIdx];
        m_cur.InterType[m_cur.BlkIdx] = m_cur.SkipType[m_cur.BlkIdx]; //MB type is used here!
        m_cur.BestIdx[dir][m_cur.BlkIdx] = idx;
        m_cur.BestIdx[opp_dir][m_cur.BlkIdx] = -1;
        m_cur.BestMV[dir][idx][m_cur.BlkIdx] = m_cur.PredMV[dir][idx][m_cur.BlkIdx];
        m_cur.SkipCost[m_cur.BlkIdx] = ME_BIG_COST;
    }else if(m_cur.IntraCost[m_cur.BlkIdx] < m_cur.InterCost[m_cur.BlkIdx]){
        //intra
        m_cur.InterType[m_cur.BlkIdx] = ME_MbIntra; //MB type is used here!
        // TODO: some meaningful value should be assigned to cost here for proper 16x16 8x8 mode decision
//        m_cur.InterCost[m_cur.BlkIdx] = ME_BIG_COST; //for T2 calculation
    }else{
        //inter
        //leave current type
        SetError((vm_char*)"Wrong decision in MakeBlockModeDecision", m_cur.InterCost[m_cur.BlkIdx]>=ME_BIG_COST);
    }
}


void MeBase::MakeMBModeDecision8x8()
{
    if(!m_par->UseFeedback)
        MakeMBModeDecision8x8Org();
    else
        MakeMBModeDecision8x8ByFB();
}

void MeBase::MakeMBModeDecision8x8Org()
{
    MeMB *mb=&m_par->pSrc->MBs[m_cur.adr];
    MeMbType Type;

    if(m_par->SearchDirection == ME_ForwardSearch)
    {
        if(m_cur.InterType[0] == ME_MbIntra && m_cur.InterType[1] == ME_MbIntra &&
            m_cur.InterType[2] == ME_MbIntra && m_cur.InterType[3] == ME_MbIntra)
        {
            //not set 8x8 - intra MB
            return;
        }
        if(m_cur.InterType[0] == ME_MbIntra || m_cur.InterType[1] == ME_MbIntra ||
            m_cur.InterType[2] == ME_MbIntra || m_cur.InterType[3] == ME_MbIntra)
        {
            Type = ME_MbMixed;
        }
        else if(m_cur.SkipCost[0] != ME_BIG_COST)
        {
            Type = ME_MbFrwSkipped;
        }
        else
        {
            Type = ME_MbFrw;
        }
    }
    else
    {
        if(m_cur.SkipCost[0] != ME_BIG_COST)
        {
            if(m_cur.InterType[0] == ME_MbFrwSkipped)
            {
                Type = ME_MbFrwSkipped;
            }
            else
            {
                Type = ME_MbBkwSkipped;
            }
        }
        else if(m_cur.InterType[0] == ME_MbFrw)
        {
            Type = ME_MbFrw;
        }
        else
        {
            Type = ME_MbBkw;
        }
    }
    //choose 16x16 or 8x8
    Ipp32s InterCost=0;
    if(m_cur.SkipCost[0] != ME_BIG_COST)
    {
        for(Ipp32s i=0; i<4; i++){
            InterCost+=m_cur.SkipCost[i];
            Ipp32s dir = (m_cur.SkipType[i] == ME_MbFrwSkipped) ? frw : bkw;
            Ipp32s opp_dir = (dir == frw)? bkw : frw;
            Ipp32s idx = m_cur.SkipIdx[dir][i];
            m_cur.InterCost[i] = m_cur.SkipCost[i];
            m_cur.InterType[i] = m_cur.SkipType[i]; //MB type is used here!
            m_cur.BestIdx[dir][i] = idx;
            m_cur.BestIdx[opp_dir][i] = -1;
            m_cur.BestMV[dir][idx][i] = m_cur.PredMV[dir][idx][i];
        }
    }
    for(Ipp32s i=0; i<4; i++){
        InterCost+=m_cur.InterCost[i];
    }
    InterCost = (Ipp32s)1.2*InterCost;

    if(mb->MbCosts[0] > InterCost){
        //8x8 is better, save results
        Ipp32s dir = (Type == ME_MbFrw ||
                        Type == ME_MbFrwSkipped || Type == ME_MbMixed) ? frw : bkw;
        if(m_par->UseVarSizeTransform)
        {
            for(Ipp32s i=0; i<4; i++)
            {
                m_cur.BlkIdx = i;
                if(m_cur.InterType[i] == ME_MbFrw)
                    MakeTransformDecisionInterFast(ME_Mb8x8, m_par->PixelType,
                                           m_cur.InterType[i],
                                           m_cur.BestMV[0][m_cur.BestIdx[0][i]][i], 
                                           0,
                                           m_cur.BestIdx[0][i], 0);
                                                              
                else if(m_cur.InterType[i] == ME_MbBkw)
                    MakeTransformDecisionInterFast(ME_Mb8x8, m_par->PixelType,
                                           m_cur.InterType[i],
                                           0, 
                                           m_cur.BestMV[1][m_cur.BestIdx[1][i]][i],
                                           0, m_cur.BestIdx[1][i]);
            }
              
        }
        SetMB8x8(Type, m_cur.BestMV[dir], m_cur.InterCost);
    }
}

Ipp16s median4(Ipp16s a, Ipp16s b, Ipp16s c, Ipp16s d)
{
    Ipp16s amin=IPP_MIN(IPP_MIN(a,b),IPP_MIN(c,d));
    Ipp16s amax=IPP_MAX(IPP_MAX(a,b),IPP_MAX(c,d));
    return (a+b+c+d-amin-amax)/2;
}

void MeBase::MakeMBModeDecision8x8ByFB()
{
    SetError((vm_char*)"Fast feedback is not supported in MeBase::MakeMBModeDecision8x8ByFB", m_par->UseFastFeedback);

    MeMV mv, pred;
    MeCostRD res=0, tmp;
    Ipp32s pattern=0;
    MeMB *mb=&m_par->pSrc->MBs[m_cur.adr];

    m_PredCalc->ResetPredictors(); // TODO: change pred calculator to avoid this
    for(Ipp32s blk=0; blk<4;blk++){
        m_cur.BlkIdx = blk;
        mv = m_cur.BestMV[m_cur.RefDir][m_cur.RefIdx][m_cur.BlkIdx];
        m_PredCalc->GetPrediction(); //get predictor, this call initializes some inner variables
        pred = m_PredCalc->GetPrediction(mv); //this is real calculation

        MeAdr src, ref;
        MakeRefAdrFromMV(m_cur.MbPart, m_par->PixelType, mv, &ref);
        MakeSrcAdr(m_cur.MbPart, m_par->PixelType, &src);

        if(m_par->UseVarSizeTransform)
        {
            MeTransformType tr[4]={ME_Tranform8x8, ME_Tranform8x4, ME_Tranform4x8,ME_Tranform4x4};
            Ipp32s BestCost = ME_BIG_COST;
            MeCostRD CurCost;
            for(Ipp32s t = 0; t < 4; t++)
            {
                CurCost = GetCostRD(ME_InterRD,ME_Mb8x8,tr[t],&src,&ref);
                if(CurCost.J < BestCost)
                {
                    BestCost = CurCost.J;
                    m_cur.InterTransf[ME_MbFrw][blk] = tr[t];
                }
            }
            tmp = BestCost;
        }
        else
        {
            tmp=GetCostRD(ME_InterRD,ME_Mb8x8,ME_Tranform8x8,&src,&ref);
        }
        res += tmp;

        m_par->pSrc->MBs[m_cur.adr].NumOfNZ=tmp.NumOfCoeff;
        bool hybrid = GetHybrid(0);
        int mvlen = GetMvSize(mv.x-pred.x,mv.y-pred.y,false,hybrid);
        //if(m_cur.HybridPredictor[0])mvlen++;  // TODO:  move to GetMvSize

        pattern = pattern<<1;
        if((mv.x-pred.x) != 0 || (mv.y-pred.y)!=0 || tmp.NumOfCoeff!=0) pattern |=1;
        res.R += mvlen;
    }

    //add chroma
    m_cur.BlkIdx=0;
    MeMV* mvx = m_cur.BestMV[m_cur.RefDir][m_cur.RefIdx];
    mv.x=median4(mvx[0].x, mvx[1].x,mvx[2].x,mvx[3].x);
    mv.y=median4(mvx[0].y, mvx[1].y,mvx[2].y,mvx[3].y);
    MeMV cmv = GetChromaMV(mv);
    
    // ME_Mb16x16 is used here to process chroma as 8x8 block
    MeAdr src, ref; src.chroma=ref.chroma=true; //true means load also chroma plane addresses
    MakeRefAdrFromMV(ME_Mb16x16, m_par->PixelType, mv, &ref);
    MakeSrcAdr(ME_Mb16x16, m_par->PixelType, &src);

    m_cur.BlkIdx = 4;
    src.channel = U;
    res+=GetCostRD(ME_InterRD,ME_Mb8x8,ME_Tranform8x8,&src,&ref);
    if(m_CRFormat == ME_YUV420)
    {
        m_cur.BlkIdx = 5;
        src.channel = V;
        res+=GetCostRD(ME_InterRD,ME_Mb8x8,ME_Tranform8x8,&src,&ref);
    }

     pattern = pattern<<2;
     res.BlkPat = 3&res.BlkPat;
     res.BlkPat = res.BlkPat |pattern;

    AddHeaderCost(res, NULL, ME_MbFrw,0,0);

    // TODO: move this to AddHeaderCost
    res.R+=2; //this is to account for skip and 1/4 MV

    Ipp32s InterCost8x8 = (Ipp32s)( res.D+m_lambda*res.R);
    if(InterCost8x8<mb->MbCosts[0]){ //this is comparision to best 16x16 RD cost
        //printf("[%d], %d,%d,\n", m_cur.adr, res.D, res.R);
        for(Ipp32s i=0; i<4; i++) m_cur.InterType[i] = ME_MbFrw; //only inter type is estimated right now
        SetMB8x8(ME_MbFrw, m_cur.BestMV[m_cur.RefDir], m_cur.InterCost); // it is not RD costs here!
    }

    m_cur.BlkIdx=0;
}

bool MeBase::IsSkipEnabled()
{
    // TODO: add fast skip estimation for FB
    if(!m_par->ProcessSkipped || m_par->UseFeedback)
        return false;
    if((m_par->PredictionType == ME_MPEG2) && 
        (m_cur.x == 0 || m_cur.x == m_par->pSrc->WidthMB - 1 || m_par->pSrc->MBs[m_cur.adr-1].MbType == ME_MbIntra))
        return false;

    return true;
}

//Interpolate data from src to dst, only two lowest bits of MV is used.
//Src and dst are left top pixel adresses of MB, block or subblock.
//Src address wil be replaced at the end of the function by destination one.
void MeBase::Interpolate(MeMbPart mt, MeInterpolation interp, MeMV mv, MeAdr* src, MeAdr *dst)
{
    Ipp32s ch=src->channel;
    
    IppVCInterpolate_8u par;
    IppVCInterpolateBlock_8u m_lumaMCParams;
    IppVCInterpolateBlock_8u m_chromaMCParams;

    par.roiSize.height = 16;
    par.roiSize.width = 16;
    par.pSrc = src->ptr[ch];
    par.srcStep = src->step[ch];
    par.pDst = dst->ptr[ch];
    par.dstStep = dst->step[ch];
    par.dx = 3&mv.x;
    par.dy = 3&mv.y;
    par.roundControl = 0;
    
    if (ME_AVS_Luma == interp)
    {
        m_lumaMCParams.pSrc[0] = src->ptr[ch];
        m_lumaMCParams.srcStep = src->step[ch];
        m_lumaMCParams.pDst[0] = dst->ptr[ch];
        m_lumaMCParams.dstStep = dst->step[ch];
        m_lumaMCParams.pointBlockPos.x = m_cur.x *16;
        m_lumaMCParams.pointBlockPos.y = m_cur.y *16;
        m_lumaMCParams.sizeBlock.height = 16;
        m_lumaMCParams.sizeBlock.width = 16;
        m_lumaMCParams.pointVector.x = mv.x;
        m_lumaMCParams.pointVector.y = mv.y;
        m_lumaMCParams.sizeFrame.height = m_par->pSrc->HeightMB*16;
        m_lumaMCParams.sizeFrame.width = m_par->pSrc->WidthMB*16;
        // set pSrc into begin of picture
        m_lumaMCParams.pSrc[0] = m_lumaMCParams.pSrc[0]
                                 - m_lumaMCParams.pointBlockPos.y * m_lumaMCParams.srcStep
                                 - m_lumaMCParams.pointBlockPos.x
                                 - ((m_lumaMCParams.pointVector.y)>>2) * m_lumaMCParams.srcStep
                                 - ((m_lumaMCParams.pointVector.x)>>2);
    }

    switch(mt) {
        case ME_Mb16x16:
        break;
        case ME_Mb8x8:
            par.roiSize.height = 8;
            par.roiSize.width = 8;
            m_lumaMCParams.sizeBlock.height = 8;
            m_lumaMCParams.sizeBlock.width = 8;
        break;
        default:
            SetError((vm_char*)"Wrong mt in MeBase::Interpolate");
    }
    IppStatus st;
    switch(interp){
        case ME_VC1_Bicubic:
        ippiInterpolateQPBicubic_VC1_8u_C1R(&par);
        break;

        case ME_VC1_Bilinear:
        if(m_CRFormat == ME_YUV420 || ch == 0)
            ippiInterpolateQPBilinear_VC1_8u_C1R(&par);
        else //if(m_CRFormat == ME_NV12 && ch > 0)//chroma
            _own_ippiInterpolateQPBilinear_VC1_8u_C1R_NV12(&par);
        break;

        case ME_H264_Luma:{
            IppiSize roiSize;
            roiSize.height = 16;
            roiSize.width = 16;
            //ippiInterpolateLuma_H264_8u_C1R(src,srcStep,dst,dstStep,(3&mv.x),(3&mv.y),roiSize);
        }
        break;

        case ME_AVS_Luma:
        st = ippiInterpolateLumaBlock_AVS_8u_P1R(&m_lumaMCParams);
        break;
/*
        case ME_H264_Chroma:
            roiSize.height = 8;
            roiSize.width = 8;
            ippiInterpolateChroma_H264_8u_C1R(src,srcStep,dst,dstStep,(3&mv.x),(3&mv.y),roiSize);
        break;
*/
        default:
            SetError((vm_char*)"Wrong interpolation type in MeBase::Interpolate");
    }

    src->set(ch,dst->ptr[ch],dst->step[ch]);
}


void MeBase::EstimatePointInter(MeMbPart mt, MePixelType pix, MeMV mv)
{
    Ipp32s cost = EstimatePoint(mt, pix, m_par->CostMetric, mv);
    if(cost == ME_BIG_COST)
        return;

    bool UseFB = m_par->UseFeedback;
    #ifdef ME_GENERATE_PRESETS
        UseFB = false;
    #endif

    if(!UseFB || m_cur.MbPart == ME_Mb8x8){ // TODO: add 8x8 J regression 
        cost+=WeightMV(mv, m_cur.PredMV[m_cur.RefDir][m_cur.RefIdx][m_cur.BlkIdx]);
    }else{
        if(m_par->UseFastFeedback){
            MeMV pred = m_cur.PredMV[m_cur.RefDir][m_cur.RefIdx][0];
            cost = (Ipp32s)(m_InterRegFunJ.Weight(cost) + m_lambda*m_MvRegrFunR.Weight(mv,pred));;
        }else{
            MeMV pred = m_PredCalc->GetPrediction(mv);
            bool bNonDominant = m_PredCalc->GetNonDominant(m_cur.RefDir,m_cur.RefIdx,0);
            bool hybrid = GetHybrid(m_cur.RefIdx);
            cost = (Ipp32s)(m_InterRegFunJ.Weight(cost) + m_lambda * GetMvSize(mv.x-pred.x,mv.y-pred.y,bNonDominant,hybrid));
        }
        SetError((vm_char*)"overflow was detected in EstimatePointInter", cost<0);
    }

    if(cost < m_cur.BestCost){
        m_cur.BestCost = cost;
        m_cur.BestMV[m_cur.RefDir][m_cur.RefIdx][m_cur.BlkIdx] = mv;
    }
}


//MV should be valid, there is no check inside!
Ipp32s MeBase::EstimatePointAverage(MeMbPart mt, MePixelType pix, MeDecisionMetrics CostMetric, Ipp32s RefDirection0, Ipp32s RefIndex0, MeMV mv0, Ipp32s RefDirection1, Ipp32s RefIndex1, MeMV mv1)
{
    SetError((vm_char*)"Wrong partitioning in MeBase::EstimatePointAverage",mt!=ME_Mb16x16);

    MeAdr src, ref, ref0, ref1;

    if(CostMetric==ME_SSD && m_par->UseChromaForMD)
    {
        src.chroma = ref.chroma = ref0.chroma = ref1.chroma = true;
    }
    
    //average
    SetReferenceFrame(RefDirection0,RefIndex0);
    MakeRefAdrFromMV(mt, pix, mv0, &ref0);
    SetReferenceFrame(RefDirection1,RefIndex1);
    MakeRefAdrFromMV(mt, pix, mv1, &ref1);

    // TODO: choose proper ROI here
    ippiAverage16x16_8u_C1R(ref0.ptr[Y], ref0.step[Y], ref1.ptr[Y], ref1.step[Y], m_cur.buf[bidir].ptr[Y], 16);
    ref.set(Y, m_cur.buf[bidir].ptr[Y], 16);
    if(CostMetric==ME_SSD && m_par->UseChromaForMD)
    {
        if(m_CRFormat == ME_YUV420)
        {
            ippiAverage8x8_8u_C1R(ref0.ptr[U], ref0.step[U], ref1.ptr[U], ref1.step[U], m_cur.buf[bidir].ptr[U], 8);
            ref.set(U, m_cur.buf[bidir].ptr[U], 8);
            ippiAverage8x8_8u_C1R(ref0.ptr[V], ref0.step[V], ref1.ptr[V], ref1.step[V], m_cur.buf[bidir].ptr[V], 8);
            ref.set(V, m_cur.buf[bidir].ptr[V], 8);
        }
        else
        {
            ippiAverage8x8_8u_C1R(ref0.ptr[U], ref0.step[U], ref1.ptr[U], ref1.step[U], m_cur.buf[bidir].ptr[U], 16);
            ippiAverage8x8_8u_C1R(ref0.ptr[U]+8, ref0.step[U], ref1.ptr[U]+8, ref1.step[U], m_cur.buf[bidir].ptr[U]+8, 16);
            ref.set(U, m_cur.buf[bidir].ptr[U], 16);
        }
    }

    
    MakeSrcAdr(mt,pix,&src);

    //get chroma addresses
/*    
    if(m_par->UseChromaForME){
        ippiAverage8x8_8u_C1R(adr0.ptr[U], adr0.step[U], adr1.ptr[U], adr1.step[U], m_cur.buf[bidir], 8);
        ippiAverage8x8_8u_C1R(adr0.ptr[V], adr0.step[V], adr1.ptr[V], adr1.step[V], m_cur.buf[bidir], 8);

        src.ptr[U] = m_cur.pSrc[U];
        src.step[U] = m_cur.SrcStep[U];
        ref.ptr[U] = m_cur.buf[bidir];
        ref.step[U] = 8;

        src.ptr[V] = m_cur.pSrc[V];
        src.step[V] = m_cur.SrcStep[V];
        ref.ptr[V] = m_cur.buf[bidir];
        ref.step[V] = 8;
    }
*/
    return GetCost(CostMetric, (MeMbPart)(mt>>GetPlaneLevel(pix)), &src, &ref);
}


//Estimate point and return its cost.
//MV should be valid, there is no check inside!
Ipp32s MeBase::EstimatePoint(MeMbPart mt, MePixelType pix, MeDecisionMetrics CostMetric, MeMV mv)
{
    //check range
    // TODO: temporal solution, move check one or two levles up. To EstimateSkip, FullSearch and so on.
    if(m_PredCalc->IsOutOfBound(mt, pix, mv))
        return ME_BIG_COST;

    MeAdr src, ref;
    if(CostMetric==ME_SSD && m_par->UseChromaForMD)
        src.chroma=ref.chroma=true;
    MakeRefAdrFromMV(mt, pix, mv, &ref);
    MakeSrcAdr(mt,pix,&src);

    //get cost
    return GetCost(CostMetric, (MeMbPart)(mt>>GetPlaneLevel(pix)), &src, &ref);
}


Ipp32s MeBase::GetCost(MeDecisionMetrics CostMetric, MeMbPart mt, MeAdr* src, MeAdr* ref)
{
    //this is shortcut due to performace reason
    Ipp32s cost=ME_BIG_COST;
    Ipp32s channel = src->channel;
    if(CostMetric == ME_Sad){
        if(mt==ME_Mb4x4)ippiSAD4x4_8u32s(src->ptr[channel], src->step[channel], ref->ptr[channel], ref->step[channel], &cost, 0);
        else if(mt==ME_Mb8x8)ippiSAD8x8_8u32s_C1R(src->ptr[channel], src->step[channel], ref->ptr[channel], ref->step[channel], &cost, 0);
        else if(mt==ME_Mb16x16) ippiSAD16x16_8u32s(src->ptr[channel], src->step[channel], ref->ptr[Y], ref->step[channel], &cost, 0);
        return cost;
    }

    switch(CostMetric)
    {
        case ME_Sad: // TODO: it looks like a bug
            switch(mt){
                case ME_Mb2x2:{
                    Ipp8u *s = src->ptr[channel];
                    Ipp32s ss = src->step[channel];
                    Ipp8u *r = ref->ptr[channel];
                    Ipp32s rs = ref->step[channel];
                    cost= abs((Ipp32s)*(s+0*ss +0)-(Ipp32s)*(r+0*rs +0)) +  abs((Ipp32s)*(s+0*ss +1)-(Ipp32s)*(r+0*rs +1))+
                                         abs((Ipp32s)*(s+1*ss +0)-(Ipp32s)*(r+1*rs +0)) +  abs((Ipp32s)*(s+1*ss +1)-(Ipp32s)*(r+1*rs +1));
                    }
                    break;
                default:
                    SetError((vm_char*)"wrong partitioning in GetCost ME_Sad");
            }
            break;

        case ME_SSD://it is not used for skipped MB without RD-decision!!!
            switch(mt){
                case ME_Mb16x16:
                        ippiSqrDiff16x16_8u32s(src->ptr[Y], src->step[Y], ref->ptr[Y], ref->step[Y], IPPVC_MC_APX_FF, &cost);
                        if(src->chroma){
                            Ipp32s CostU,CostV;
                            if(m_CRFormat == ME_YUV420)
                            {
                                ippiSSD8x8_8u32s_C1R(src->ptr[U], src->step[U], ref->ptr[U], ref->step[U],&CostU,0);
                                ippiSSD8x8_8u32s_C1R(src->ptr[V], src->step[V], ref->ptr[V], ref->step[V],&CostV,0);
                            }
                            else//NV12
                            {
                                ippiSSD8x8_8u32s_C1R(src->ptr[U], src->step[U], ref->ptr[U], ref->step[U],&CostU,0);
                                ippiSSD8x8_8u32s_C1R(src->ptr[U]+8, src->step[U], ref->ptr[U]+8, ref->step[U],&CostV,0);
                            }
                            cost+=CostU+CostV;
                        }
                break;

                default:
                    SetError((vm_char*)"wrong partitioning in GetCost SSD");
            }
            break;

        case ME_InterSSD:
        case ME_InterRate:
        case ME_IntraSSD:
        case ME_IntraRate:
            //cost=GetTransformBasedCost(CostMetric,  mt,  src->ptr[Y], src->step[Y], ref->ptr[Y], ref->step[Y]);
            SetError((vm_char*)"GetTransformBasedCost was called");
            break;

        case ME_Sadt:
        case ME_SadtSrc:
        case ME_SadtSrcNoDC:
            cost=GetCostHDMR(CostMetric, mt, src, ref);
            break;

        default:
            SetError((vm_char*)"wrong cost metric in GetCost");
    }

    return cost;
}

void MeBase::GetCost(MeMbPart mt, MeAdr* src, MeAdr* ref, Ipp32s numX, Ipp16u *sadArr)
{
    if(mt==ME_Mb4x4) ippiSAD4x4xN_8u16u_C1R( src->ptr[Y], src->step[Y], ref->ptr[Y], ref->step[Y],sadArr,numX);
    else if (mt==ME_Mb2x2) {
        for(Ipp32s i=0;i<numX;i++) {
            Ipp8u *s = src->ptr[Y];
            Ipp32s ss = src->step[Y];
            Ipp8u *r = ref->ptr[Y];
            Ipp32s rs = ref->step[Y];
            Ipp32s cost = abs((Ipp32s)*(s+0*ss +0)-(Ipp32s)*(r+0*rs +0)) +  abs((Ipp32s)*(s+0*ss +1)-(Ipp32s)*(r+0*rs +1))+
                          abs((Ipp32s)*(s+1*ss +0)-(Ipp32s)*(r+1*rs +0)) +  abs((Ipp32s)*(s+1*ss +1)-(Ipp32s)*(r+1*rs +1));
            sadArr[i] = (Ipp16u)cost;
        }
    }
}

//separated from GetCost due to performace reason
Ipp32s MeBase::GetCostHDMR(MeDecisionMetrics CostMetric, MeMbPart mt, MeAdr* src, MeAdr* ref)
{
    Ipp16s bufDiff[64];
    Ipp16s bufHDMR[64];

    Ipp32s cost=ME_BIG_COST;
    Ipp32s channel = src->channel;

    switch(CostMetric){

        case ME_Sadt:
            switch(mt){
                case ME_Mb16x16:
                    {
                        cost=0;
                        for(Ipp32u i=0; i<4; i++) {
                            MeAdr s=*src;
                            MeAdr r=*ref;
                            MakeBlockAdr(0,ME_Mb8x8, i, &s);
                            MakeBlockAdr(0,ME_Mb8x8, i, &r);
                            
#ifdef USE_OPTIMIZED_SATD
                             cost+=SAT8x8D(s.ptr[Y],s.step[Y], r.ptr[Y], r.step[Y]);
#else
                             ippiSub8x8_8u16s_C1R(s.ptr[channel],s.step[channel], r.ptr[channel], r.step[channel],bufDiff,16);
                             HadamardFwdFast<Ipp16s,8>(bufDiff, 16, bufHDMR);
                             for(Ipp32u j=0; j<64; j++)
                                cost+=abs((Ipp32s)bufHDMR[j]);
#endif
                        }
                }
                break;

                case ME_Mb8x8:
#ifdef USE_OPTIMIZED_SATD
                    cost=SAT8x8D(src->ptr[Y], src->step[Y], ref->ptr[Y], ref->step[Y]);
#else
                    {
                        cost=0;
                        ippiSub8x8_8u16s_C1R(src->ptr[channel], src->step[channel], ref->ptr[channel], ref->step[channel], bufDiff, 16);
                        HadamardFwdFast<Ipp16s,8>(bufDiff, 16, bufHDMR);
                        for(Ipp32u j=0; j<64; j++)
                                cost+=abs((Ipp32s)bufHDMR[j]);
                    }
#endif
                break;

                default:
                    SetError((vm_char*)"wrong partitioning in GetCost ME_Sadt");
            }
            break;

        case ME_SadtSrc:
        case ME_SadtSrcNoDC:
            {
                cost = 0;
                switch(mt){
                    case ME_Mb16x16:
                        for(Ipp32s i=0; i<4; i++){
                            MeAdr s=*src;
                            MakeBlockAdr(0,ME_Mb8x8, i, &s);
                            HadamardFwdFast<Ipp8u,8>(s.ptr[channel],s.step[channel],bufHDMR);

                            for(Ipp32s j=(i==0 && CostMetric!=ME_SadtSrcNoDC?0:1); j<64; j++)
                                cost+=abs((Ipp32s)bufHDMR[j]);
                        }
                    break;

                    case ME_Mb8x8:
                            HadamardFwdFast<Ipp8u,8>(src->ptr[channel], src->step[channel], bufHDMR);
                        for(Ipp32s j=0; j<64; j++)
                            cost+=abs((Ipp32s)bufHDMR[j]);
                    break;

                    default:
                        SetError((vm_char*)(vm_char*)"wrong partitioning in GetCost ME_SadtSrc");
                }
            }
            break;

        }

    return cost;

}

MeCostRD MeBase::GetCostRD(MeDecisionMetrics /*CostMetric*/, MeMbPart /*mt*/, MeTransformType /*transf*/, MeAdr* /*src*/, MeAdr* /*ref*/)
{
    MeCostRD cost;
    cost.R=ME_BIG_COST;
    cost.D=ME_BIG_COST;
    return cost;
}

void MeBase::DownsampleOne(Ipp32s level, Ipp32s x0, Ipp32s y0, Ipp32s w, Ipp32s h, MeAdr *adr)
{
    Ipp32s scale = 1<<level;
    IppiSize size = {w,h};
    IppiRect roi = {0,0,size.width,size.height};
    IppiSize DwnSize = {(size.width+scale-1)>>level,(size.height+scale-1)>>level};

    Ipp8u* pSrc=adr[0].ptr[Y];
    Ipp32s SrcStep = adr[0].step[Y];
    Ipp8u* pDst=adr[level].ptr[Y];
    Ipp32s DstStep = adr[level].step[Y];

    pSrc += y0*SrcStep + x0;
    pDst += (y0>>level)*DstStep + (x0>>level);
    ippiResize_8u_C1R(pSrc, size, SrcStep, roi, pDst, DstStep, DwnSize, (double)(1./scale),  (double)(1./scale), IPPI_INTER_SUPER);
}

void MeBase::DownsampleFrames()
{
    if(!m_InitPar.UseDownSampling)
        return;

    // TODO: remove after allocation in init
    SetError((vm_char*) "Wrong size of m_cur.DwnPoints", sizeof(m_cur.DwnPoints)/sizeof(MeSearchPoint)/5<m_par->pSrc->WidthMB*m_par->pSrc->HeightMB);

    for(Ipp32s lev=1; lev<4; lev++){
        Ipp32s x0, y0, w, h;

        //src
        x0=y0=0;

        w=16*m_par->pSrc->WidthMB;
        h=16*m_par->pSrc->HeightMB;
        DownsampleOne(lev, x0, y0, w, h, m_par->pSrc->plane);

        //ref
        for(Ipp32s i=0; i<m_par->FRefFramesNum+m_par->BRefFramesNum; i++){
            MeFrame *ref;
            if(i<m_par->FRefFramesNum)
                ref=m_par->pRefF[i];
            else
                ref=m_par->pRefB[i-m_par->FRefFramesNum];

            Ipp32s PadX,PadY;
            PadX=PadY=ref->padding;
            SetError((vm_char*)"Wrong padding in MeBase::DownsampleFrames", PadX>m_InitPar.refPadding || PadY>m_InitPar.refPadding || (PadX&0xf)!=0 || (PadY&0xf)!=0);
            
            x0=-PadX;
            y0=-PadY;

            w=16*ref->WidthMB+2*PadX;
            h=16*ref->HeightMB+2*PadY;
            DownsampleOne(lev, x0, y0, w, h, ref->plane);
        }
    }
}


void MeBase::SetReferenceFrame(Ipp32s RefDirection, Ipp32s RefIndex)
{
    m_cur.RefIdx=RefIndex;
    m_cur.RefDir=RefDirection;
}

Ipp32s MeBase::GetPlaneLevel(MePixelType pix)
{
    if(pix == ME_DoublePixel) return 1;
    if(pix == ME_QuadPixel) return 2;
    if(pix == ME_OctalPixel) return 3;
    return 0;
}

void MeBase::MakeSrcAdr(MeMbPart mt, MePixelType pix, MeAdr* adr)
{
    Ipp32s level=GetPlaneLevel(pix);
    *adr = m_par->pSrc->plane[level];
    adr->move(m_CRFormat,(16*m_cur.x)>>level,(16*m_cur.y)>>level);
    MakeBlockAdr(level, mt, m_cur.BlkIdx, adr);
}

//Function returns "adr", this is set of pointers to the left top pixel of the MB, current block or subblock in current reference frame.
//If interpolation is needed function performs interpolation and returns pointer to the buffer with interpolated data.
void MeBase::MakeRefAdrFromMV(MeMbPart mt, MePixelType pix, MeMV mv, MeAdr* adr)
{
    MeMV cmv;
    Ipp32s level=GetPlaneLevel(pix);
    MeFrame **fr;
    if(m_cur.RefDir==frw)fr=m_par->pRefF;
    else fr=m_par->pRefB;
    *adr =fr[m_cur.RefIdx] ->plane[level];
    bool isField = (m_par->PredictionType == ME_VC1Field1 || m_par->PredictionType == ME_VC1Field2
        || m_par->PredictionType == ME_VC1Field2Hybrid);

    Ipp32s y_shift = 0;

    if(isField)
    {
        if(m_par->pSrc->bBottom != fr[m_cur.RefIdx]->bBottom)
        {
            if(fr[m_cur.RefIdx]->bBottom == false) y_shift = 2;
            else y_shift = -2;
        }
        else
        {
            y_shift = 0;
        }
    }
    adr->move(Y,(16*m_cur.x+(mv.x>>2))>>level,(16*m_cur.y+((mv.y+y_shift)>>2))>>level);

    if(adr->chroma){
        cmv = GetChromaMV(mv);
        Ipp32s x=(8*m_cur.x+(cmv.x>>2))>>level;
        Ipp32s y=(8*m_cur.y+((cmv.y+y_shift)>>2))>>level;
        if(m_CRFormat == ME_YUV420)
        {
            adr->move(U,x,y);
            adr->move(V,x,y);
        }
        else
        {
            adr->move(U,2*x,y);
        }
    }

    MakeBlockAdr(level, mt, m_cur.BlkIdx, adr);

    if(pix==ME_HalfPixel || pix==ME_QuarterPixel){
        //all blocks are put at the beginig of the buffer, so MakeBlockAdr is not called
        adr->channel=Y;
        MeMV mv1 = mv;
        mv1.y += y_shift;
        Interpolate(mt, m_par->Interpolation, mv1, adr, &m_cur.buf[m_cur.RefDir]);
    }

    if(adr->chroma){ // TODO: remove for double and higher pixel types
        for(int ch=U; ch<=V; ch++){
            adr->channel=ch;
            MeMV mv1 = cmv;
            mv1.y += y_shift;
            Interpolate(ME_Mb8x8/*((MeMbPart)mt>>1)*/, m_par->ChromaInterpolation, mv1, adr, &m_cur.buf[m_cur.RefDir]);
            if(m_CRFormat == ME_NV12) break;
        }
    }
}

void MeBase::MakeBlockAdr(Ipp32s level, MeMbPart mt, Ipp32s BlkIdx, MeAdr* adr)
{
    if(BlkIdx==0)return; //speed up a bit

    static const int offset=4;
    Ipp32s shiftX=0, maskX=0, shiftY=0, maskY=0;
    BlkIdx<<=offset;
    switch(mt){
//        case ME_Mb16x16:
//            return;
        case ME_Mb8x8:
            maskX=1<<offset;
            shiftX=offset-3;
            shiftY=offset-2;
            break;
        case ME_Mb4x4:
            maskX=3<<offset;
            shiftX=offset-2;
            shiftY=offset-0;
            break;
       default:
            SetError((vm_char*)"Wrong mt in MakeBlockAdr\n");
    }
    shiftX+=level;
    shiftY+=level;
    maskY=~maskX;
    
    adr->move(m_CRFormat,(BlkIdx&maskX)>>shiftX,(BlkIdx&maskY)>>shiftY);
}


void MeBase::SetMB16x16B(MeMbType mbt, MeMV mvF, MeMV mvB, Ipp32s cost)
{
    MeMB *mb=&m_par->pSrc->MBs[m_cur.adr];
    int i = 0;
    mb->MbPart = ME_Mb16x16;
    mb->McType = ME_FrameMc;
    mb->MbType = (Ipp8u)mbt;
    mb->MbCosts[0] = cost;

    if(m_cur.BestIdx[frw][0] >= 0)
        mb->MVPred[frw][0] = m_cur.PredMV[frw][m_cur.BestIdx[frw][0]][0];
    if(m_cur.BestIdx[bkw][0] >= 0)
        mb->MVPred[bkw][0] = m_cur.PredMV[bkw][m_cur.BestIdx[bkw][0]][0];

    for(i=0; i<4; i++){
        mb->MV[frw][i] = mvF;
        mb->MV[bkw][i] = mvB;
        mb->Refindex[frw][i]= m_cur.BestIdx[frw][0];
        mb->Refindex[bkw][i]= m_cur.BestIdx[bkw][0];
        mb->BlockType[i] = mb->MbType;
    }

    Ipp32s cost_4 = cost/4+1;
    for(i=1; i<5; i++)
        mb->MbCosts[i] = cost_4;

    //transform size
    for(i=5; i>=0; i--){
        mb->BlockTrans<<=2;
        mb->BlockTrans|=3&m_cur.InterTransf[mb->MbType][i];
    }

    //quantized coefficients, rearrange during copy
    if(m_par->UseTrellisQuantization){
        if(mbt == ME_MbIntra){
                MFX_INTERNAL_CPY(mb->Coeff,m_cur.TrellisCoefficients[4],(6*64)*sizeof(mb->Coeff[0][0]));
                MFX_INTERNAL_CPY(mb->RoundControl,m_cur.RoundControl[4],(6*64)*sizeof(mb->RoundControl[0][0]));
        }else{
            for(Ipp32s blk=0; blk<6; blk++){
                MeTransformType tr=m_cur.InterTransf[mb->MbType][blk];
                switch(tr){
                    case ME_Tranform4x4:
                        for(Ipp32s subblk=0; subblk<4; subblk++)
                            for(Ipp32s y=0; y<4; y++)
                                for(Ipp32s x=0; x<4; x++)
                                {
                                    //MeQuantTable[m_cur.adr][intra?0:1][m_cur.BlkIdx][(32*(subblk/2))+(4*(subblk&1))+8*y+x]=buf2[16*subblk+4*y+x]; 
                                    mb->Coeff[blk][(32*(subblk/2))+(4*(subblk&1))+8*y+x]=m_cur.TrellisCoefficients[tr][blk][16*subblk+4*y+x];
                                    mb->RoundControl[blk][(32*(subblk/2))+(4*(subblk&1))+8*y+x]=m_cur.RoundControl[tr][blk][16*subblk+4*y+x];
                                }
                        break;
                    case ME_Tranform4x8:
                        for(Ipp32s subblk=0; subblk<2; subblk++)
                            for(Ipp32s y=0; y<8; y++)
                                for(Ipp32s x=0; x<4; x++)
                                {
                                    mb->Coeff[blk][4*subblk+8*y+x]=m_cur.TrellisCoefficients[tr][blk][32*subblk+4*y+x];
                                    mb->RoundControl[blk][4*subblk+8*y+x]=m_cur.RoundControl[tr][blk][32*subblk+4*y+x];
                                    //MeQuantTable[m_cur.adr][intra?0:1][m_cur.BlkIdx][4*subblk+8*y+x]=buf2[32*subblk+4*y+x]; 
                                }
                        break;
                    case ME_Tranform8x4:
                    case ME_Tranform8x8:
                        MFX_INTERNAL_CPY(mb->Coeff[blk],m_cur.TrellisCoefficients[tr][blk],64*sizeof(mb->Coeff[0][0]));
                        MFX_INTERNAL_CPY(mb->RoundControl[blk],m_cur.RoundControl[tr][blk],64*sizeof(mb->RoundControl[0][0]));
                        break;
                }
            }
        }
    }
}

void MeBase::SetMB8x8(MeMbType mbt, MeMV mv[2][4], Ipp32s cost[4])
{
    MeMB *mb=&m_par->pSrc->MBs[m_cur.adr];
    mb->MbPart = ME_Mb8x8;
    mb->McType = ME_FrameMc;
    mb->MbType = (Ipp8u)mbt;
    Ipp32s dir = (mb->MbType == ME_MbFrw || mb->MbType == ME_MbFrwSkipped
        || mb->MbType == ME_MbMixed) ? frw : bkw;

    for(int i=0; i<4; i++){
        mb->Refindex[dir][i]= m_cur.BestIdx[dir][i];
        mb->MV[dir][i] = mv[m_cur.BestIdx[dir][i]][i];
        mb->BlockType[i] = (Ipp8u)m_cur.InterType[i];  // TODO: rename this
        mb->MbCosts[i+1] = cost[i]; //0 is for 16x16
        mb->MVPred[dir][i] = m_cur.PredMV[dir][mb->Refindex[dir][i]][i];
    }

    if(m_par->SearchDirection == ME_BidirSearch)
    {
        MeMV mv;
        m_PredCalc->SetDefMV(mv,!dir);
        for(int i=0; i<4; i++){
            mb->MV[!dir][i] = mv;
            mb->Refindex[!dir][i]= m_cur.BestIdx[!dir][0];
        }
    }

    //transform size
    for(int i=5; i>=0; i--){
        mb->BlockTrans<<=2;
        mb->BlockTrans|=3&m_cur.InterTransf[mb->MbType][i];
    }
 }

void MeBase::SetError(vm_char *msg, bool condition)
{
    msg, condition; //warning suppression
#ifdef ME_DEBUG
    if(!condition)
        return;
    printf("\n\n%s\n\n", msg);
    abort();
#endif
}

void MeBase::EstimateMbInterFullSearch()
{
    FullSearch(m_cur.MbPart, ME_IntegerPixel,0,4*m_par->SearchRange.x,4*m_par->SearchRange.y);
    if(m_par->PixelType != ME_IntegerPixel){
        FullSearch(m_cur.MbPart, m_par->PixelType,m_cur.BestMV[m_cur.RefDir][m_cur.RefIdx][m_cur.BlkIdx],4,4);
    }
}

void MeBase::EstimateMbInterFullSearchDwn()
{
    SetError((vm_char*)"Downsampling is disabled in MeBase::EstimateMbInterFullSearchDwn", !m_InitPar.UseDownSampling);

    //first check predictors
    EstimatePointInter(m_cur.MbPart, m_par->PixelType, 0);
    MeMV mv=m_PredCalc->GetMvA();
    mv.x=(mv.x>>2)<<2;
    mv.y=(mv.y>>2)<<2;
    EstimatePointInter(m_cur.MbPart, m_par->PixelType, mv);
    mv=m_PredCalc->GetMvB();
    mv.x=(mv.x>>2)<<2;
    mv.y=(mv.y>>2)<<2;
    EstimatePointInter(m_cur.MbPart, m_par->PixelType, mv);
    mv=m_PredCalc->GetMvC();
    mv.x=(mv.x>>2)<<2;
    mv.y=(mv.y>>2)<<2;
    EstimatePointInter(m_cur.MbPart, m_par->PixelType, mv);

    DiamondSearch(m_cur.MbPart, ME_IntegerPixel, ME_BigDiamond);
    DiamondSearch(m_cur.MbPart, ME_IntegerPixel, ME_MediumDiamond);
    DiamondSearch(m_cur.MbPart, ME_IntegerPixel, ME_SmallDiamond);

 
    // TODO: choose right upper level!
    Ipp32s UpLevel=2; //for 16x16

    for(Ipp32s l=0; l<=UpLevel; l++){
        m_cur.DwnCost[l] = ME_BIG_COST;
        m_cur.DwnNumOfPoints[l] = 0;
    }

    Ipp32s x0, x1, y0, y1=ME_BIG_COST; //any big number will sute
    x0=y0=-y1; 
    x1=y1; 
    m_PredCalc->TrimSearchRange(m_cur.MbPart,(MePixelType)0, x0, x1, y0, y1);

    //allign to 16 for 16x16 upper level search
    // TODO: add padding check here
    Ipp32s xu0=-((-x0)&(~0xf));
    Ipp32s xu1=x1&(~0xf);
    Ipp32s yu0=-((-y0)&(~0xf));
    Ipp32s yu1=y1&(~0xf);
//    if(x0!=xu0 || y0!=yu0 || x1!=xu1 || y1!=yu1) printf("x vs xu (%d:%d) (%d:%d) (%d:%d) (%d:%d) \n",x0, xu0, y0, yu0, x1, xu1, y1, yu1);

    for(Ipp32s l=UpLevel; l>=0; l--){
        EstimateMbInterOneLevel(l==UpLevel,l, m_cur.MbPart, xu0, xu1, yu0, yu1);
    }

    if(m_par->PixelType == ME_HalfPixel){
        FullSearch(m_cur.MbPart, m_par->PixelType,m_cur.BestMV[m_cur.RefDir][m_cur.RefIdx][m_cur.BlkIdx],4,4);
    }else if(m_par->PixelType == ME_QuarterPixel){
        RefineSearch(m_cur.MbPart, m_par->PixelType);
    }
}


void MeBase::EstimateMbInterOneLevel(bool UpperLevel, Ipp32s level, MeMbPart mt, Ipp32s x0, Ipp32s x1, Ipp32s y0, Ipp32s y1)
{
    Ipp16u sadArr[8192];
    //x0, x1, y0, y1 here are limits for MV, not the pixel coordinates
    MePixelType pix=ME_IntegerPixel;
    switch(level){
        //case 0: pix = ME_IntegerPixel; break;
        case 1: pix = ME_DoublePixel; break;
        case 2: pix = ME_QuadPixel; break;
        case 3: pix = ME_OctalPixel; break;
    }


    MeAdr src, ref;
    MakeRefAdrFromMV(mt, pix, MeMV(x0,y0), &ref);
    MakeSrcAdr(mt,pix,&src);

    Ipp32s step = 4<<level;
    SetError((vm_char*)"Wrong range in EstimateMbInterOneLevel", x0>=x1 || y0>=y1 || (x0&(step-1))!=0 || (x1&(step-1))!=0 || (y0&(step-1))!=0 || (y1&(step-1))!=0);

    Ipp32s NumOfPoints=0;

    //upper level
    if(UpperLevel){

        for(Ipp32s y=y0; y<y1; y+=step,ref.move(m_CRFormat,(x0-x1)>>(level+2),1)){

            Ipp32s numX = (((abs(x1-x0))/step)>>3)<<3;
            GetCost((MeMbPart)(mt>>level), &src, &ref, numX, sadArr);
            Ipp32s x2 = x0;

            for(Ipp32s i=0; i<numX; i++,x2+=step,ref.ptr[Y]+=1){
                Ipp32s cost=(Ipp32s)sadArr[i];
                if(cost<m_cur.DwnCost[level])m_cur.DwnCost[level] = cost;
                m_cur.DwnPoints[level][NumOfPoints].cost = cost;
                m_cur.DwnPoints[level][NumOfPoints].x = (Ipp16s)x2;
                m_cur.DwnPoints[level][NumOfPoints].y = (Ipp16s)y;
                NumOfPoints++;
            }

            for(Ipp32s x=x2; x<x1; x+=step,ref.ptr[Y]+=1){
                Ipp32s cost = GetCost(ME_Sad, (MeMbPart)(mt>>level), &src, &ref);
                if(cost<m_cur.DwnCost[level])m_cur.DwnCost[level] = cost;
                m_cur.DwnPoints[level][NumOfPoints].cost = cost;
                m_cur.DwnPoints[level][NumOfPoints].x = (Ipp16s)x;
                m_cur.DwnPoints[level][NumOfPoints].y = (Ipp16s)y;
                NumOfPoints++;
            }
        }
        m_cur.DwnNumOfPoints[level] =NumOfPoints;
    }else{
        //not upper level
        Ipp32s UpPoints=m_cur.DwnNumOfPoints[level+1];
        SetError((vm_char*)"NumOfPoints==0 in EstimateMbInterOneLevel1", UpPoints ==0);

        //calculate threshold
        Ipp32s MinCost = m_cur.DwnCost[level+1];
        Ipp32s thr = 4*MinCost;
        Ipp32s ActivePoints=0;
        Ipp32s MaxActivePoints=UpPoints/32+16;  //keep at least 16 points
        // TODO: speed up is needed here
        for(;;){
            ActivePoints=0;
            for(Ipp32s p=0; p<UpPoints; p++)
                if(m_cur.DwnPoints[level+1][p].cost<=thr)ActivePoints++;

            if(ActivePoints<=MaxActivePoints)break;
            thr = (thr+MinCost)/2;
            if(thr == MinCost)break;
        }

        //check number of points
        // TODO: improved algorithm is needed
        if(ActivePoints>MaxActivePoints){
            //leave only one point, closest to zero MV
            Ipp32s MinMV=ME_BIG_COST;
            Ipp32s num=0;
            for(Ipp32s p=0; p<UpPoints; p++){
                if(m_cur.DwnPoints[level+1][p].cost==thr){
                    Ipp32s a=abs(m_cur.DwnPoints[level+1][p].x)+abs(m_cur.DwnPoints[level+1][p].y);
                    if(a<MinMV){
                        num=p;
                        MinMV=a;
                    }
                }
            }
            m_cur.DwnPoints[level+1][0]=m_cur.DwnPoints[level+1][num];
            m_cur.DwnNumOfPoints[level+1]=UpPoints=1;
        }

        //estimate points on current level        
        m_cur.ClearMvScoreBoardArr();
        for(Ipp32s p=0; p<UpPoints; p++){
            if(m_cur.DwnPoints[level+1][p].cost>thr)continue;
            Ipp32s xt0, xt1, yt0, yt1;
            xt0=IPP_MAX(m_cur.DwnPoints[level+1][p].x-step,x0);
            xt1=IPP_MIN(xt0+3*step,x1);
            yt0=IPP_MAX(m_cur.DwnPoints[level+1][p].y-step,y0);
            yt1=IPP_MIN(yt0+3*step,y1);
//            xt0=m_cur.DwnPoints[level+1][p].x;
//            xt1=xt0+2*step;
//            yt0=m_cur.DwnPoints[level+1][p].y;
//            yt1=yt0+2*step;

            // TODO: move this up to avoid recalculation
            MakeRefAdrFromMV(mt, pix, MeMV(xt0,yt0), &ref);
            MakeSrcAdr(mt,pix,&src);
            
            m_cur.SetScoreBoardFlag();
            for(Ipp32s y=yt0; y<yt1; y+=step,ref.move(m_CRFormat,(xt0-xt1)>>(level+2),1)){
                for(Ipp32s x=xt0; x<xt1; x+=step,ref.ptr[Y]+=1){
                    if(level!=0){
                        Ipp32s cost = GetCost(ME_Sad, (MeMbPart)(mt>>level), &src, &ref);
                        if(cost<m_cur.DwnCost[level])m_cur.DwnCost[level] = cost;
                        m_cur.DwnPoints[level][NumOfPoints].cost = cost;
                        m_cur.DwnPoints[level][NumOfPoints].x = (Ipp16s)x;
                        m_cur.DwnPoints[level][NumOfPoints].y = (Ipp16s)y;
                        NumOfPoints++;
                    }else{
                        if(!m_cur.CheckScoreBoardMv(pix,MeMV(x,y)))
                        EstimatePointInter(mt, pix, MeMV(x,y));
                    }
                }
            }
            m_cur.ClearScoreBoardFlag();
            m_cur.DwnNumOfPoints[level] =NumOfPoints;
        }
    }
}

void MeBase::EstimateMbInterFast()
{
    /*
    //check predictors
    EstimatePointInter(m_cur.MbPart, ME_IntegerPixel, 0);
    EstimatePointInter(m_cur.MbPart, ME_IntegerPixel, m_cur.PredMV[m_cur.RefDir][m_cur.RefIdx][m_cur.BlkIdx]);
    EstimatePointInter(m_cur.MbPart, ME_IntegerPixel, m_PredCalc->GetMvA());
    EstimatePointInter(m_cur.MbPart, ME_IntegerPixel, m_PredCalc->GetMvB());
    EstimatePointInter(m_cur.MbPart, ME_IntegerPixel, m_PredCalc->GetMvC());

    if(m_cur.BestCost<m_PredCalc->GetT2()){
        //prediction is good enough, refine
        if(RefineSearch(m_cur.MbPart, ME_IntegerPixel)){
            //search converged, refine to subpixel
            RefineSearch(m_cur.MbPart, m_par->PixelType);
            return;
        }
    }

    //prediction was poor, search bigger area
//    m_cur.BestCost = ME_BIG_COST;
//    m_cur.BestMV[m_cur.RefDir][m_cur.RefIdx][m_cur.BlkIdx] = 0;
    DiamondSearch(m_cur.MbPart,ME_IntegerPixel, ME_MediumHexagon);
//    DiamondSearch(m_cur.MbPart,ME_IntegerPixel, ME_SmallHexagon);
    DiamondSearch(m_cur.MbPart,ME_IntegerPixel, ME_BigDiamond);
    DiamondSearch(m_cur.MbPart,ME_IntegerPixel, ME_MediumDiamond);
    DiamondSearch(m_cur.MbPart,ME_IntegerPixel, ME_SmallDiamond);
    RefineSearch(m_cur.MbPart, m_par->PixelType);
    */

    //check predictors
    EstimatePointInter(m_cur.MbPart, m_par->PixelType, 0);
    //EstimatePointInter(m_cur.MbPart, m_par->PixelType, m_cur.PredMV[m_cur.RefDir][m_cur.RefIdx][m_cur.BlkIdx]);
    EstimatePointInter(m_cur.MbPart, m_par->PixelType, m_PredCalc->GetMvA());
    EstimatePointInter(m_cur.MbPart, m_par->PixelType, m_PredCalc->GetMvB());
    EstimatePointInter(m_cur.MbPart, m_par->PixelType, m_PredCalc->GetMvC());

    MeMV BestMV = m_cur.BestMV[m_cur.RefDir][m_cur.RefIdx][m_cur.BlkIdx];
    Ipp32s BestCost = m_cur.BestCost;

    m_cur.BestMV[m_cur.RefDir][m_cur.RefIdx][m_cur.BlkIdx].x =
        (m_cur.BestMV[m_cur.RefDir][m_cur.RefIdx][m_cur.BlkIdx].x >> 2) << 2;
    m_cur.BestMV[m_cur.RefDir][m_cur.RefIdx][m_cur.BlkIdx].y =
        (m_cur.BestMV[m_cur.RefDir][m_cur.RefIdx][m_cur.BlkIdx].y >> 2) << 2;
    m_cur.BestCost = ME_BIG_COST;

    if(m_cur.BestCost<m_PredCalc->GetT2()){
        //prediction is good enough, refine
        if(RefineSearch(m_cur.MbPart, ME_IntegerPixel)){
            //search converged, refine to subpixel
            RefineSearch(m_cur.MbPart, m_par->PixelType);
            if(BestCost > m_cur.BestCost)
            {
                return;
            }
            else
            {
                m_cur.BestMV[m_cur.RefDir][m_cur.RefIdx][m_cur.BlkIdx] = BestMV;
                m_cur.BestCost = BestCost;
            }
        }
    }

    //prediction was poor, search bigger area
//    m_cur.BestCost = ME_BIG_COST;
//    m_cur.BestMV[m_cur.RefDir][m_cur.RefIdx][m_cur.BlkIdx] = 0;
    DiamondSearch(m_cur.MbPart,ME_IntegerPixel, ME_MediumHexagon);
//    DiamondSearch(m_cur.MbPart,ME_IntegerPixel, ME_SmallHexagon);
    DiamondSearch(m_cur.MbPart,ME_IntegerPixel, ME_BigDiamond);
    DiamondSearch(m_cur.MbPart,ME_IntegerPixel, ME_MediumDiamond);
    DiamondSearch(m_cur.MbPart,ME_IntegerPixel, ME_SmallDiamond);
    RefineSearch(m_cur.MbPart, m_par->PixelType);
    if(BestCost > m_cur.BestCost)
    {
        return;
    }
    else
    {
        m_cur.BestMV[m_cur.RefDir][m_cur.RefIdx][m_cur.BlkIdx] = BestMV;
        m_cur.BestCost = BestCost;
    }
}



void MeBase::FullSearch(MeMbPart mt, MePixelType pix, MeMV org, Ipp32s RangeX, Ipp32s RangeY)
{
    Ipp32s Step=1, rangeX, rangeY;
    rangeX = RangeX;
    rangeY = RangeY;
    if(pix == ME_QuadPixel) {Step = 16; rangeX = (RangeX/16)*16; rangeY = (RangeY/16)*16;}
    if(pix == ME_IntegerPixel) Step = 4;
    if(pix == ME_HalfPixel) Step = 2;
    if(pix == ME_QuarterPixel) Step = 1;

    for(int y = -rangeY; y<=rangeY; y+=Step )
    {
        for(int x = -rangeX; x<=rangeX; x+=Step )
        {
            EstimatePointInter(mt, pix, org+MeMV(x,y));
        }
    }
}

void MeBase::DiamondSearch(MeMbPart mt, MePixelType pix, MeDiamondType dm)
{
    static const Ipp32s BigDiamondTable[][2] = { {-3,0}, {-2,2}, {0,3}, {2,2}, {3,0}, {2,-2}, {0,-3}, {-2,-2} };
    static const Ipp32s MediumDiamondTable[][2] = { {-2,0}, {-1,1}, {0,2}, {1,1}, {2,0}, {1,-1}, {0,-2}, {-1,-1} };
    static const Ipp32s SmallDiamondTable[][2] = {{-1,0}, {0,1}, {1,0}, {0,-1}};
    static const Ipp32s MediumHexagonTable[][2] = {
                                                {0,4}, {2,3}, {4,2}, {4,1}, {4,0}, {4,-1}, {4,-2}, {2,-3}, {0,-4}, {-2,-3}, {-4,-2}, {-4,-1}, {-4,0}, {-4,1}, {-4,2}, {-2,3},
                                                {0,8}, {4,6}, {8,4}, {8,2}, {8,0}, {8,-2}, {8,-4}, {4,-6}, {0,-8}, {-4,-6}, {-8,-4}, {-8,-2}, {-8,0}, {-8,2}, {-8,4}, {-4,6},
                                                {0,12}, {6,9}, {12,6}, {12,3}, {12,0}, {12,-3}, {12,-6}, {6,-9}, {0,-12}, {-6,-9}, {-12,-6}, {-12,-3}, {-12,0}, {-12,3}, {-12,6}, {-6,9},
                                                {0,16}, {8,12}, {16,8}, {16,4}, {16,0}, {16,-4}, {16,-8}, {8,-12}, {0,-16}, {-8,-12}, {-16,-8}, {-16,-4}, {-16,0}, {-16,4}, {-16,8}, {-8,12}};
    static const Ipp32s SmallHexagonTable[][2] = {
                                                {0,4}, {2,3}, {4,2}, {4,1}, {4,0}, {4,-1}, {4,-2}, {2,-3}, {0,-4}, {-2,-3}, {-4,-2}, {-4,-1}, {-4,0}, {-4,1}, {-4,2}, {-2,3},
                                                {0,8}, {4,6}, {8,4}, {8,2}, {8,0}, {8,-2}, {8,-4}, {4,-6}, {0,-8}, {-4,-6}, {-8,-4}, {-8,-2}, {-8,0}, {-8,2}, {-8,4}, {-4,6}};

    const Ipp32s* Table=NULL;
    Ipp32s TableSize = 0;
    Ipp32s Step = 0;
    if(dm == ME_SmallDiamond)
    {
        Table = (const Ipp32s*)SmallDiamondTable;
        TableSize = sizeof(SmallDiamondTable)/(2*sizeof(Ipp32s));
    }
    if(dm == ME_MediumDiamond)
    {
        Table = (const Ipp32s*)MediumDiamondTable;
        TableSize = sizeof(MediumDiamondTable)/(2*sizeof(Ipp32s));
    }
    if(dm == ME_BigDiamond)
    {
        Table = (const Ipp32s*)BigDiamondTable;
        TableSize = sizeof(BigDiamondTable)/(2*sizeof(Ipp32s));
    }
    if(dm == ME_SmallHexagon)
    {
        Table = (const Ipp32s*)SmallHexagonTable;
        TableSize = sizeof(SmallHexagonTable)/(2*sizeof(Ipp32s));
    }
    if(dm == ME_MediumHexagon)
    {
        Table = (const Ipp32s*)MediumHexagonTable;
        TableSize = sizeof(MediumHexagonTable)/(2*sizeof(Ipp32s));
    }


    if(pix == ME_IntegerPixel)
        Step = 4;
    if(pix == ME_HalfPixel)
        Step = 2;
    if(pix == ME_QuarterPixel)
        Step = 1;

    MeMV   CenterMV = m_cur.BestMV[m_cur.RefDir][m_cur.RefIdx][m_cur.BlkIdx];
    for(Ipp32s i=0; i<TableSize; i++)
        EstimatePointInter(mt, pix, CenterMV + MeMV(Step*Table[2*i+0], Step*Table[2*i+1]));
}


bool MeBase::RefineSearch(MeMbPart mt, MePixelType pix)
{
    SetError((vm_char*)"Wrong mt in RefineSearch", mt != ME_Mb8x8 && mt != ME_Mb16x16);

    switch(pix){
        case ME_IntegerPixel:  //this means refinement after prediction.
            for(int j=0; j<2; j++)
            { //maximum number of big steps that are allowed
                MeMV CenterMV = m_cur.BestMV[m_cur.RefDir][m_cur.RefIdx][m_cur.BlkIdx];
                DiamondSearch(mt,pix, ME_MediumDiamond);

                //check if best MV in center
                if(CenterMV == m_cur.BestMV[m_cur.RefDir][m_cur.RefIdx][m_cur.BlkIdx])
                {
                    //refine by small diamond
                    DiamondSearch(mt,pix, ME_SmallDiamond);
                    return true;
                }
            }
            return false;

        case ME_HalfPixel:
            DiamondSearch(mt,pix, ME_MediumDiamond);
            DiamondSearch(mt,pix, ME_SmallDiamond);
            return true;

        case ME_QuarterPixel:
            DiamondSearch(mt,pix, ME_BigDiamond);
            DiamondSearch(mt,pix, ME_MediumDiamond);
            DiamondSearch(mt,pix, ME_SmallDiamond);
            return true;
        default:
            SetError((vm_char*)"Wrong pix in RefineSearch");
    }
    return false;
}

///Close motion estimator.
///Free all allocated memory.
void MeBase::Close()
{
    //save presets
    #ifdef ME_GENERATE_PRESETS    
    Ipp32s idx = QpToIndex(MeQP, MeQPUNF);
    pStat->SaveRegression((Ipp8u*)"ME_RG_DInter", 0, NULL, NULL, 0, idx, &m_InterRegFunD);
    pStat->SaveRegression((Ipp8u*)"ME_RG_RInter", 0, NULL, NULL, 0, idx, &m_InterRegFunR);
    pStat->SaveRegression((Ipp8u*)"ME_RG_JInter", 0, NULL, NULL, 0, idx, &m_InterRegFunJ);
    pStat->SaveRegression((Ipp8u*)"ME_RG_RMV",    0, NULL, NULL, 0, idx, &m_MvRegrFunR);
    pStat->SaveRegression((Ipp8u*)"ME_RG_DIntra", 0, NULL, NULL, 0, idx, &m_IntraRegFunD);
    pStat->SaveRegression((Ipp8u*)"ME_RG_RIntra", 0, NULL, NULL, 0, idx, &m_IntraRegFunR);
    #endif

    free(m_OwnMem);
    m_AllocatedSize = 0;
    m_OwnMem = NULL;
}

bool MeBase::CheckParams()
{
    m_par->MbPart |= ME_Mb16x16;


    //feedback and downsampling check
    if(m_par->UseFeedback && (m_par->PredictionType == ME_VC1Field1 || m_par->PredictionType == ME_VC1Field2 ||
        m_par->PredictionType == ME_VC1Field2Hybrid) && (m_par->MbPart & ME_Mb8x8))
        m_par->UseFeedback = false;
//        return false;

    if(!m_par->UseFeedback && (m_par->UseTrellisQuantization ||
        m_par->UseTrellisQuantizationChroma))
    {
        m_par->UseTrellisQuantization = false;
        m_par->UseTrellisQuantizationChroma = false;
    }

    if(m_par->UseFastFeedback && (m_par->MbPart & ME_Mb8x8))
    {
        m_par->UseFastFeedback = false;
        m_par->UseFeedback = true;
    }
    if(m_par->UseFeedback && m_par->UpdateFeedback && !m_InitPar.UseStatistics)
        return false;

    if(m_par->UseDownSampledImageForSearch && !m_InitPar.UseDownSampling)
        return false;
    //no  DownSampledImageForSearch for SSD:   
    if(m_par->UseDownSampledImageForSearch && m_par->CostMetric == ME_SSD)
        return false;
   
    

    // TODO: add check for CostMetric
    // TODO: add check for enabled statistic in MEStat
    if(m_par->SkippedMetrics != ME_Maxad && m_par->SkippedMetrics != ME_Sad
       && m_par->SkippedMetrics != ME_Sadt && m_par->SkippedMetrics != ME_VarMean)
       return false;
    if(m_par->SkippedMetrics == ME_Maxad)
        m_SkippedThreshold = m_par->Quant/4;
    else if(m_par->SkippedMetrics == ME_Sad)
        m_SkippedThreshold = 150*m_par->Quant/4;
    else if(m_par->SkippedMetrics == ME_Sadt)
        m_SkippedThreshold = m_par->Quant/2;
    else if(m_par->SkippedMetrics == ME_VarMean)
        m_SkippedThreshold = m_par->Quant;

    //check actual request
    //check if current input parameters violate Init agreement
    if( m_par->SearchRange.x < 8 || m_par->SearchRange.x > 256 ||
        m_par->SearchRange.y < 8 || m_par->SearchRange.y > 256)
        return false;
    if( m_par->Interpolation != ME_VC1_Bicubic && m_par->Interpolation != ME_VC1_Bilinear
        && m_par->Interpolation != ME_H264_Luma && m_par->Interpolation != ME_H264_Chroma
        && m_par->Interpolation != ME_AVS_Luma)
        return false;
    if( m_par->MbPart & ME_Mb16x8 || m_par->MbPart & ME_Mb8x16 ||
        m_par->MbPart & ME_Mb4x4)
        return false;

    if(m_par->PredictionType != ME_VC1 && m_par->PredictionType != ME_VC1Hybrid && m_par->PredictionType != ME_MPEG2
        && m_par->PredictionType != ME_VC1Field1 && m_par->PredictionType != ME_VC1Field2 &&
        m_par->PredictionType != ME_VC1Field2Hybrid && m_par->PredictionType != ME_AVS)
        return false;

    //check input ptrs
//    if( m_par->pSrc->plane[0].ptr[Y] == NULL)
//        return false;
//    if( (m_par->SearchDirection == ME_ForwardSearch || m_par->SearchDirection == ME_BidirSearch)&& (m_par->pRefF==NULL || m_par->pRefF[0]->ptr[0]==NULL))
//        return false;
//    if( m_par->SearchDirection == ME_BidirSearch && (m_par->pRefB==NULL || m_par->pRefB[0]->ptr[0]==NULL))
//        return false;

    Ipp32s NMB =  m_par->pSrc->WidthMB*m_par->pSrc->HeightMB;
    if(m_par->FirstMB ==-1 && m_par->LastMB==-1){
        m_par->FirstMB =0;
        m_par->LastMB = NMB-1;
    }else if( m_par->FirstMB < 0 || m_par->LastMB >= NMB ||  m_par->FirstMB>m_par->LastMB)
        return false;

    //check stupid errors
    if(!(m_par->MbPart & ME_Mb16x16) && !(m_par->MbPart & ME_Mb8x8)) return false;
    //if(m_par->SearchDirection == ME_BidirSearch && (m_par->MbPart & ME_Mb8x8)) return false;
    if(m_par->SearchDirection != ME_ForwardSearch && m_par->SearchDirection != ME_BidirSearch) return false;
    if(m_par->PixelType != ME_IntegerPixel && m_par->PixelType != ME_HalfPixel && m_par->PixelType != ME_QuarterPixel) return false;
    if(m_par->PixelType == ME_HalfPixel && m_par->Interpolation != ME_VC1_Bilinear
       && m_par->Interpolation != ME_H264_Luma && m_par->Interpolation != ME_H264_Chroma
       && m_par->Interpolation != ME_AVS_Luma)return false;

    //VC1 specific
    m_cur.qp8=(m_par->Quant/2)>8?1:0;
    m_cur.CbpcyTableIndex = m_par->CbpcyTableIndex;
    m_cur.MvTableIndex = m_par->MvTableIndex;
    m_cur.AcTableIndex = m_par->AcTableIndex;

    return true;
 }

inline Ipp32s MeBase::WeightMV(MeMV mv, MeMV predictor)
{
    static const Ipp32s cost_table[128] =
                    { 2, 4, 8, 8,12,12,12,12,
                     16,16,16,16,16,16,16,16,
                     20,20,20,20,20,20,20,20,
                     20,20,20,20,20,20,20,20,
                     24,24,24,24,24,24,24,24,
                     24,24,24,24,24,24,24,24,
                     24,24,24,24,24,24,24,24,
                     24,24,24,24,24,24,24,24,
                     28,28,28,28,28,28,28,28,
                     28,28,28,28,28,28,28,28,
                     28,28,28,28,28,28,28,28,
                     28,28,28,28,28,28,28,28,
                     28,28,28,28,28,28,28,28,
                     28,28,28,28,28,28,28,28,
                     28,28,28,28,28,28,28,28,
                     28,28,28,28,28,28,28,28
                     };

    Ipp32s d = abs(mv.x-predictor.x) + abs(mv.y-predictor.y);
    if(d > 127) return 32;
    return cost_table[d & 0x0000007f];
}

Ipp32s MeBase::QpToIndex(Ipp32s QP, bool UniformQuant)
{
    // 2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,20,22,24,26,28,30,32,34,36,38,40,42,44,46,48,50,54,58,62
    // 1,2,...,36

    //convert all QP to 1-72 range, 0 means unsupported combination
    //from 1 to 36 are uniform quants, from 37 to 72 non uniform
    Ipp32s idx=0;
    if(QP>=2 && QP <=17)idx=QP-1; //1-16
    else if(QP>=18 && QP <=50 && (QP&1)==0) idx=(QP-18)/2+17; //17-33
    else if(QP==54 ||  QP==58 || QP==62) idx=(QP-54)/4+34; //34-36
   // if(idx==0){
    //    printf("Wrong QP=%d Unif=%d\n", QP, UniformQuant);
    //    SetError((vm_char*)"Wrong QP");
   // }

    if(!UniformQuant)
        idx+=36;

//    printf("QpToIndex %d %d > %d\n", QP, UniformQuant, idx);
    return idx;
}

void MeBase::LoadRegressionPreset()
{
    MeRegrFunSet  RegSet=ME_RG_EMPTY_SET;
    if(!m_par->UseFeedback) return;
    if(m_par->UseFastFeedback)RegSet=ME_RG_VC1_FAST_SET;
    else RegSet=ME_RG_VC1_PRECISE_SET;

    if (m_par->PredictionType == ME_AVS)
        RegSet=ME_RG_AVS_FAST_SET;

    if(RegSet!=m_CurRegSet){
        m_CurRegSet = RegSet;
        #ifndef ME_GENERATE_PRESETS
            m_InterRegFunD.LoadPresets(m_CurRegSet,ME_RG_DInter);
            m_InterRegFunR.LoadPresets(m_CurRegSet,ME_RG_RInter);
            m_InterRegFunJ.LoadPresets(m_CurRegSet,ME_RG_JInter);
            m_IntraRegFunD.LoadPresets(m_CurRegSet,ME_RG_DIntra);
            m_IntraRegFunR.LoadPresets(m_CurRegSet,ME_RG_RIntra);
            m_MvRegrFunR.LoadPresets(m_CurRegSet,ME_RG_RMV);
        #endif    
    }

    RegrFun::SetQP(QpToIndex(m_par->Quant, m_par->UniformQuant));
#ifndef ME_USE_BACK_DOOR_LAMBDA
        if(m_par->UniformQuant){
            m_lambda= 10*m_par->Quant*m_par->Quant;
        }else{
            m_lambda= 35*m_par->Quant*m_par->Quant;
        }
        if (ME_AVS == m_par->PredictionType)
            m_lambda= m_par->Quant*m_par->Quant;

        if(m_lambda<0) m_lambda=0;
        m_lambda /= 100.;
    #ifdef ME_DEBUG
            MeLambda=100*m_lambda;
    #endif
#endif
}


#pragma warning( disable: 4189 )
void MeBase::ProcessFeedback(MeParams *pPar)
{
    // TODO: add check that statistic is valid
    //skip I&B frames for now
    MeFrame *pFrame = pPar->pSrc;

//    printf("ProcessFeedback %d\n", pFrame->type);
    if(pFrame->type == ME_FrmIntra) return;
//    if(pFrame->type != ME_FrmFrw) return;
    
    //calculate distortion
    // TODO: add check here that pFrame->SrcPlane != pFrame->RecPlane
    for(Ipp32s y=0; y<pFrame->HeightMB; y++){
        for(Ipp32s x=0; x<pFrame->WidthMB; x++){
            Ipp32s adr = pFrame->WidthMB*y + x;
            Ipp8u* pS=pFrame->SrcPlane.at(Y,16*x,16*y);//source MB
            Ipp8u* pR=pFrame->RecPlane.at(Y,16*x,16*y);; //reconstructed MB

            Ipp32s sum=0;
            ippiSqrDiff16x16_8u32s(pS, pFrame->SrcPlane.step[Y], pR, pFrame->RecPlane.step[Y], IPPVC_MC_APX_FF, &sum);
            pFrame->stat[adr].dist = sum;
        }
    }

#ifdef ME_DEBUG
    pStat->AddFrame(pPar);
#endif

    if(!pPar->UpdateFeedback)
        return;

    Ipp32s x[16384];
    Ipp32s y[16384];
    Ipp32s NumOfMB=pFrame->WidthMB*pFrame->HeightMB, N;
    Ipp32s QpIdx=QpToIndex(pPar->Quant, pPar->UniformQuant);


    static int counter=0;   
    counter++;


    bool SaveRegr = false;
    #ifdef ME_GENERATE_PRESETS    
    SaveRegr = false;
    #endif

    //1 INTER
    int MvSize=0;
    int AcSize=0;
    int WholeSize=0;
    int CbpSizeInter=0;
    int MvCbpSizeIntra=0;
    int nInter=0,nIntra=0;

    N=0; 
    for(Ipp32s i=0; i<NumOfMB; i++){
        //whole frame statistics
        Ipp32s coeffS=pFrame->stat[i].coeff[0]+pFrame->stat[i].coeff[1]+pFrame->stat[i].coeff[2]+pFrame->stat[i].coeff[3]+pFrame->stat[i].coeff[4]+pFrame->stat[i].coeff[5];
        if(pFrame->MBs[i].MbType == ME_MbIntra){
            nIntra++;
            AcSize+=coeffS;
            MvCbpSizeIntra+=pFrame->stat[i].whole-coeffS-2; // 2=1(for skip) + 1 for AC pred
            WholeSize+=pFrame->stat[i].whole;
        }

        if(pFrame->MBs[i].MbType == ME_MbFrw){
            nInter++;
            AcSize+=coeffS;
            MvSize+=pFrame->stat[i].MVF[0];
            CbpSizeInter += pFrame->stat[i].whole - coeffS - pFrame->stat[i].MVF[0] - 1; //for skip, also hibrid should be taken into account
            WholeSize+=pFrame->stat[i].whole;
        }
        
        if(pFrame->MBs[i].MbType == ME_MbFrw){
//        if(pFrame->MBs[i].MbType == ME_MbFrw && pFrame->MBs[i].MbCosts[0]!=0){ //zero cost lead to terrible regression, avoid it
            x[N] = pFrame->MBs[i].InterCostRD.D;
            //x[N] = pFrame->MBs[i].MbCosts[0]; //SAD
            if(m_par->UseFastFeedback)
                y[N] = (Ipp32s)(pFrame->stat[i].dist+m_lambda*(pFrame->stat[i].whole-pFrame->stat[i].MVF[0]));
            else
                y[N] = pFrame->stat[i].dist;
                
            //printf("[%d] D Inter  %5d, %5d dis=%d rate=%d mv=%d L=%f\n",i, x[N], y[N],pFrame->stat[i].dist, pFrame->stat[i].whole,pFrame->stat[i].MVF[0],m_lambda );
            N++;
        }
    }
    m_InterRegFunD.ProcessFeedback(x, y, N);
#ifdef ME_DEBUG
    if(SaveRegr)pStat->SaveRegression((Ipp8u*)"ME_RG_DInter", counter, x, y, N, QpIdx, &m_InterRegFunD);
    //printf("Inter Mv=%5d Ac=%5d Whole=%5d CbpSizeInter=%5d MvCbpSizeIntra=%5d  Mv+Cbp=%5d\n",  MvSize, AcSize, WholeSize, CbpSizeInter, MvCbpSizeIntra, MvSize+CbpSizeInter+MvCbpSizeIntra);
#endif

    N=0;
    for(Ipp32s i=0; i<NumOfMB && !m_par->UseFastFeedback; i++){
        if(pFrame->MBs[i].MbType == ME_MbFrw){
            //x[N] = pFrame->MBs[i].MbCosts[0];
            x[N] = pFrame->MBs[i].InterCostRD.R;
            //x[N] = pFrame->MBs[i].HdmrdCost;
//            y[N] = pFrame->stat[i].coeff[0] + pFrame->stat[i].coeff[1]+pFrame->stat[i].coeff[2]+pFrame->stat[i].coeff[3]+pFrame->stat[i].coeff[4]+pFrame->stat[i].coeff[5];
            y[N] = pFrame->stat[i].whole-pFrame->stat[i].MVF[0];
            //y[N] = pFrame->stat[i].whole;
            //printf("R inter %5d, %5d\n",x[N], y[N] );
            N++;
        }
    }
    m_InterRegFunR.ProcessFeedback(x, y, N);
#ifdef ME_DEBUG
    if(SaveRegr)pStat->SaveRegression((Ipp8u*)"ME_RG_RInter", counter, x, y, N, QpIdx, &m_InterRegFunR);
#endif


    N=0;
    for(Ipp32s i=0; i<NumOfMB; i++){
        if(pFrame->MBs[i].MbType == ME_MbFrw){
            x[N] = pFrame->MBs[i].PureSAD;
            //y[N] = (Ipp32s)( m_InterRegFunD.Weight(pFrame->MBs[i].InterCostD)+m_lambda*(m_InterRegFunR.Weight(pFrame->MBs[i].InterCostR)));
            y[N] = (Ipp32s)(pFrame->stat[i].dist+m_lambda*(pFrame->stat[i].whole-pFrame->stat[i].MVF[0]));
            N++;
        }
    }
    m_InterRegFunJ.ProcessFeedback(x, y, N);
#ifdef ME_DEBUG
    if(SaveRegr)pStat->SaveRegression((Ipp8u*)"ME_RG_JInter", counter, x, y, N, QpIdx, &m_InterRegFunJ);
#endif

    N=0;
    for(Ipp32s i=0; i<NumOfMB && m_par->UseFastFeedback; i++){
        if(pFrame->MBs[i].MbType == ME_MbFrw){
           x[N] = abs(pFrame->MBs[i].MV[frw][0].x - pFrame->MBs[i].MVPred[frw][0].x)+abs(pFrame->MBs[i].MV[frw][0].y - pFrame->MBs[i].MVPred[frw][0].y);
//            x[N] = GetMvSize(pFrame->MBs[i].MV[frw][0].x - pFrame->MBs[i].MVPred[frw][0].x,pFrame->MBs[i].MV[frw][0].y - pFrame->MBs[i].MVPred[frw][0].y,i);
//            x[N] =  MVSizeForAllMB[i];
 
            y[N] = pFrame->stat[i].MVF[0];
            //printf("%5d, %5d\n",x[N], y[N] );
            N++;
        }
    }
    m_MvRegrFunR.ProcessFeedback(x, y, N);
#ifdef ME_DEBUG
    if(SaveRegr)pStat->SaveRegression((Ipp8u*)"ME_RG_RMV", counter, x, y, N, QpIdx, &m_MvRegrFunR);
#endif

    //1 INTRA
    N=0;
    for(Ipp32s i=0; i<NumOfMB; i++){
        if(pFrame->MBs[i].MbType == ME_MbIntra){
            x[N] = pFrame->MBs[i].IntraCostRD.D;
            //x[N] = pFrame->MBs[i].MbCosts[0];
            if(m_par->UseFastFeedback)
                y[N] = (Ipp32s)(pFrame->stat[i].dist+m_lambda*pFrame->stat[i].whole);
            else
                y[N] = pFrame->stat[i].dist;
            //printf("%5d, %5d\n",x[N], y[N] );
            N++;
        }
    }
    m_IntraRegFunD.ProcessFeedback(x, y, N);
#ifdef ME_DEBUG
    if(SaveRegr)pStat->SaveRegression((Ipp8u*)"ME_RG_DIntra", counter, x, y, N, QpIdx, &m_IntraRegFunD);
#endif

    N=0;
    for(Ipp32s i=0; i<NumOfMB && !m_par->UseFastFeedback; i++){
        if(pFrame->MBs[i].MbType == ME_MbIntra){
            //x[N] = pFrame->MBs[i].MbCosts[0];
            x[N] = pFrame->MBs[i].IntraCostRD.R;
            //y[N] = pFrame->stat[i].coeff[0] + pFrame->stat[i].coeff[1]+pFrame->stat[i].coeff[2]+pFrame->stat[i].coeff[3]+pFrame->stat[i].coeff[4]+pFrame->stat[i].coeff[5];;
            y[N] = pFrame->stat[i].whole;
            //printf("%5d, %5d\n",x[N], y[N] );
            N++;
        }
    }
    m_IntraRegFunR.ProcessFeedback(x, y, N);
#ifdef ME_DEBUG
    if(SaveRegr)pStat->SaveRegression((Ipp8u*)"ME_RG_RIntra", counter, x, y, N, QpIdx, &m_IntraRegFunR);
#endif

}


//*******************************************************************************************************
void MeVC1::InitVlcTableIndexes()
{
    CbpcyTableIndex = 0;
    MvTableIndex    = 0;
    AcTableIndex    = 0;
}
void MeVC1::ReInitVlcTableIndexes(MeParams *par)
{
    if(par->UseFeedback && par->SelectVlcTables)
    {
        par->CbpcyTableIndex = CbpcyTableIndex;
        par->MvTableIndex    = MvTableIndex;
        par->AcTableIndex    = AcTableIndex;
    }
}
void MeVC1::SetVlcTableIndexes(MeParams *par)
{
    if(par->UseFeedback && par->SelectVlcTables)
    {
        CbpcyTableIndex = par->OutCbpcyTableIndex;
        MvTableIndex    = par->OutMvTableIndex;
        AcTableIndex    = par->OutAcTableIndex;
    }
}

void MeVC1::SetInterpPixelType(MeParams *par)
{
    if(!ChangeInterpPixelType)
    {
        par->OutInterpolation = par->Interpolation;
        par->OutPixelType     = par->PixelType;
        return;
    }

    Ipp32s cost = IPP_MIN(CostOnInterpolation[1], CostOnInterpolation[3]);
    if(cost == CostOnInterpolation[1])
    {
        par->OutInterpolation = ME_VC1_Bilinear;
        par->OutPixelType     = ME_HalfPixel;
    }
    else
    {
        par->OutInterpolation = ME_VC1_Bicubic;
        par->OutPixelType     = ME_QuarterPixel;
    }
    par->Interpolation = par->OutInterpolation;
    par->PixelType     = par->OutPixelType;
}

bool MeVC1::EstimateSkip()
{
    if(!IsSkipEnabled())
        return false;

    if(m_cur.MbPart == ME_Mb16x16)
    {
        return EstimateSkip16x16();
    }

    if(m_cur.MbPart == ME_Mb8x8)
    {
        if(m_cur.BlkIdx > 0)
            return false;
        return EstimateSkip8x8();
    }
    return false;
}

bool MeVC1::EstimateSkip16x16()
{
    Ipp32s i, j;
    Ipp32s tmpCost, tmpCostU, tmpCostV;
    bool OutBoundF = false;
    bool OutBoundB = false;
        //P & B
    for(i=0; i< m_par->FRefFramesNum; i++){
        SetReferenceFrame(frw,i);
        m_cur.PredMV[frw][i][m_cur.BlkIdx]=m_PredCalc->GetPrediction();
        tmpCost = EstimatePoint(m_cur.MbPart, m_par->PixelType, m_par->SkippedMetrics, m_cur.PredMV[frw][i][m_cur.BlkIdx]);
        if(m_cur.SkipCost[0] > tmpCost)
        {
            m_cur.SkipCost[0]     = tmpCost;
            m_cur.SkipType[0]     = ME_MbFrwSkipped;
            m_cur.SkipIdx[frw][0] = i;
        }
        OutBoundF = (tmpCost == ME_BIG_COST ? true: false);

        //B
        if(m_par->SearchDirection ==ME_BidirSearch){
            for(j=0; j<m_par->BRefFramesNum; j++){

                SetReferenceFrame(bkw,j);
                m_cur.PredMV[bkw][j][m_cur.BlkIdx] = m_PredCalc->GetPrediction();
                tmpCost = EstimatePoint(m_cur.MbPart, m_par->PixelType, m_par->SkippedMetrics, m_cur.PredMV[bkw][j][0]);
                if(tmpCost<m_cur.SkipCost[m_cur.BlkIdx]){
                    m_cur.SkipCost[0]     = tmpCost;
                    m_cur.SkipType[0]     = ME_MbBkwSkipped;
                    m_cur.SkipIdx[bkw][0] = j;
                }
                OutBoundB = (tmpCost == ME_BIG_COST ? true: false);

                if(!OutBoundF && !OutBoundB)
                {
                    tmpCost = EstimatePointAverage(m_cur.MbPart, m_par->PixelType, m_par->SkippedMetrics, frw, i, m_cur.PredMV[frw][i][0], bkw, j, m_cur.PredMV[bkw][j][0]);
                    tmpCost +=32;
                    if(tmpCost<m_cur.SkipCost[0]){
                        m_cur.SkipCost[0]     = tmpCost;
                        m_cur.SkipType[0]     = ME_MbBidirSkipped;
                        m_cur.SkipIdx[frw][0] = i;
                        m_cur.SkipIdx[bkw][0] = j;
                    }
                }
            }//for(j)
        }//B
    }//for(i)
    //return EarlyExitForSkip();
    if(EarlyExitForSkip() == true)
    {
        //return true;
       
        MeAdr src, ref; src.chroma=ref.chroma=true; //true means load also chroma plane addresses
        MeAdr ref0, ref1; ref0.chroma=ref1.chroma=true; //true means load also chroma plane addresses
        Ipp32s mvSize = 0;

        //for NV12:
        Ipp8u buf1[64];
        Ipp8u buf2[64];

        if(m_cur.SkipType[0] == ME_MbFrwSkipped)
        {
            m_cur.RefDir = frw;
            m_cur.RefIdx = m_cur.SkipIdx[frw][0];
            MeMV mvl = m_cur.PredMV[frw][m_cur.RefIdx][0];
            MakeRefAdrFromMV(m_cur.MbPart, m_par->PixelType, mvl, &ref); 
        }
        else if(m_cur.SkipType[0] == ME_MbBkwSkipped)
        {
            m_cur.RefDir = bkw;
            m_cur.RefIdx = m_cur.SkipIdx[bkw][0];
            MeMV mvl = m_cur.PredMV[bkw][m_cur.RefIdx][0];
            MakeRefAdrFromMV(m_cur.MbPart, m_par->PixelType, mvl, &ref);
        }
        else if(m_cur.SkipType[0] == ME_MbBidirSkipped)
        {
            m_cur.RefDir = frw;
            m_cur.RefIdx = m_cur.SkipIdx[frw][0];
            MeMV mvF = m_cur.PredMV[frw][m_cur.RefIdx][0];
            MakeRefAdrFromMV(m_cur.MbPart, m_par->PixelType, mvF, &ref0);
            m_cur.RefDir = bkw;
            m_cur.RefIdx = m_cur.SkipIdx[bkw][0];
            MeMV mvB = m_cur.PredMV[bkw][m_cur.RefIdx][0];
            MakeRefAdrFromMV(m_cur.MbPart, m_par->PixelType, mvB, &ref1);

          //  ippiAverage16x16_8u_C1R(ref0.ptr[Y], ref0.step[Y], ref1.ptr[Y], ref1.step[Y], m_cur.buf[bidir].ptr[Y], 16);

            if(m_CRFormat == ME_YUV420)
            {
                ippiAverage8x8_8u_C1R(ref0.ptr[U], ref0.step[U], ref1.ptr[U], ref1.step[U], m_cur.buf[bidir].ptr[U], 8);
                ref.set(U, m_cur.buf[bidir].ptr[U], 8);
                ippiAverage8x8_8u_C1R(ref0.ptr[V], ref0.step[V], ref1.ptr[V], ref1.step[V], m_cur.buf[bidir].ptr[V], 8);
                ref.set(V, m_cur.buf[bidir].ptr[V], 8);
            }
            else//NV12
            {
                ippiAverage8x8_8u_C1R(ref0.ptr[U], ref0.step[U], ref1.ptr[U], ref1.step[U], m_cur.buf[bidir].ptr[U], 16);
                ippiAverage8x8_8u_C1R(ref0.ptr[U]+8, ref0.step[U], ref1.ptr[U]+8, ref1.step[U], m_cur.buf[bidir].ptr[U]+8, 16);
                ref.set(U, m_cur.buf[bidir].ptr[U], 16);
            }
        }
        else
        {
            VM_ASSERT(0);
        }

        MakeSrcAdr(m_cur.MbPart, m_par->PixelType, &src);
        if(m_CRFormat == ME_YUV420)
        {
            src.channel = ref.channel = U;
            tmpCostU = GetCost(m_par->CostMetric, ME_Mb8x8, &src, &ref);
            src.channel = ref.channel = V;
            tmpCostV = GetCost(m_par->CostMetric, ME_Mb8x8, &src, &ref);
        }
        else//NV12
        {
            src.channel = ref.channel = U;
            tmpCostU = GetCost(m_par->CostMetric, ME_Mb8x8, &src, &ref);
            ref.move(U,8,0);
            src.move(U,8,0);
            tmpCostV = GetCost(m_par->CostMetric, ME_Mb8x8, &src, &ref);
        }
       // Ipp32s th = m_SkippedThreshold/4;
       //  if(tmpCostU < th && tmpCostV < th)
       // {
       //     return true;
       // }
        Ipp32s th = m_SkippedThreshold/2;
        if(tmpCostU + tmpCostV < th)
        {
            return true;
        }
        m_cur.SkipCost[0] = ME_BIG_COST;
        return false;
        
    }
    return false;
}

bool MeVC1::EstimateSkip8x8()
{
    Ipp32s i;
    Ipp32s tmpCostF[ME_NUM_OF_BLOCKS];
    Ipp32s tmpCostB[ME_NUM_OF_BLOCKS];
    Ipp32s tmpIdxF[ME_NUM_OF_BLOCKS];
    Ipp32s tmpIdxB[ME_NUM_OF_BLOCKS];
    Ipp32s tmpCost;
    Ipp32s OutOfBoundCount = 0;

    for(i = 0; i < ME_NUM_OF_BLOCKS; i++)
    {
        tmpCostF[i] = tmpCostB[i] = ME_BIG_COST;
    }
    //for 2 ref. we need to estimate 16 variants (2^4- one block two indexes)!!! 
    //P & B
    for(m_cur.BlkIdx=0; m_cur.BlkIdx<4; m_cur.BlkIdx++)
    {
        OutOfBoundCount = 0;
        for(i=0; i< m_par->FRefFramesNum; i++)
        {
            SetReferenceFrame(frw,i);
            m_cur.PredMV[frw][i][m_cur.BlkIdx] = m_PredCalc->GetPrediction();
            tmpCost = EstimatePoint(m_cur.MbPart, m_par->PixelType, m_par->SkippedMetrics, m_cur.PredMV[frw][i][m_cur.BlkIdx]);
            if(tmpCost < tmpCostF[m_cur.BlkIdx])
            {
                tmpCostF[m_cur.BlkIdx] = tmpCost;
                tmpIdxF[m_cur.BlkIdx]  = i;
                m_cur.BestMV[frw][i][m_cur.BlkIdx] = m_cur.PredMV[frw][i][m_cur.BlkIdx];
                m_cur.BestIdx[frw][m_cur.BlkIdx] = i;
                m_cur.InterType[m_cur.BlkIdx] = ME_MbFrwSkipped;
                m_cur.SkipCost[m_cur.BlkIdx] = tmpCost;
            }
            else if(tmpCost == ME_BIG_COST)
            {
                OutOfBoundCount++;
            }
        }//for(i)
        if((OutOfBoundCount == m_par->FRefFramesNum) || !EarlyExitForSkip())
        {
            for(i=0; i<ME_NUM_OF_BLOCKS; i++)
            {
                m_cur.SkipCost[i] = tmpCostF[i]=ME_BIG_COST;
            }
            break;
        }
    }//for(m_cur.BlkIdx)

    if(m_par->SearchDirection ==ME_BidirSearch)
    {
        for(m_cur.BlkIdx=0; m_cur.BlkIdx<4; m_cur.BlkIdx++)
        {
            OutOfBoundCount = 0;
            for(i=0; i< m_par->BRefFramesNum; i++)
            {
                SetReferenceFrame(bkw,i);
                m_cur.PredMV[bkw][i][m_cur.BlkIdx] = m_PredCalc->GetPrediction();
                tmpCost = EstimatePoint(m_cur.MbPart, m_par->PixelType, m_par->SkippedMetrics, m_cur.PredMV[bkw][i][m_cur.BlkIdx]);
                if(tmpCost < tmpCostB[m_cur.BlkIdx])
                {
                    tmpCostB[m_cur.BlkIdx] = tmpCost;
                    tmpIdxB[m_cur.BlkIdx]  = i;
                    m_cur.BestMV[bkw][i][m_cur.BlkIdx] = m_cur.PredMV[bkw][i][m_cur.BlkIdx];
                    m_cur.BestIdx[bkw][m_cur.BlkIdx] = i;
                    m_cur.InterType[m_cur.BlkIdx] = ME_MbBkwSkipped;
                    m_cur.SkipCost[m_cur.BlkIdx] = tmpCost;
                }
                else if(tmpCost == ME_BIG_COST)
                {
                    OutOfBoundCount++;
                }
            }//for(i)
            if((OutOfBoundCount == m_par->FRefFramesNum) || !EarlyExitForSkip())
            {
                for(i=0; i<ME_NUM_OF_BLOCKS; i++)
                {
                    m_cur.SkipCost[i] = tmpCostB[i]=ME_BIG_COST;
                }
                break;
            }
        }//for(m_cur.BlkIdx)
    }//if(m_par->SearchDirection == ME_BidirSearch)

    m_cur.BlkIdx = 0;

    if(tmpCostF[0] == ME_BIG_COST && tmpCostB[0] == ME_BIG_COST)
        return false;

    if(tmpCostF[0] != ME_BIG_COST && tmpCostB[0] != ME_BIG_COST)
    {
        Ipp32s sumF = tmpCostF[0] + tmpCostF[1] + tmpCostF[2] + tmpCostF[3];
        Ipp32s sumB = tmpCostB[0] + tmpCostB[1] + tmpCostB[2] + tmpCostB[3];

        if(sumF < sumB)
        {
            for(i=0; i<ME_NUM_OF_BLOCKS; i++)
            {
                m_cur.SkipCost[i]     = tmpCostF[i];
                m_cur.SkipType[i]     = ME_MbFrwSkipped;
                m_cur.InterType[i]    = ME_MbFrwSkipped;
                m_cur.SkipIdx[frw][i] = tmpIdxF[i];
            }
        }
        else
        {
            for(i=0; i<ME_NUM_OF_BLOCKS; i++)
            {
                m_cur.SkipCost[i]     = tmpCostB[i];
                m_cur.SkipType[i]     = ME_MbBkwSkipped;
                m_cur.InterType[i]    = ME_MbBkwSkipped;
                m_cur.SkipIdx[bkw][i] = tmpIdxB[i];
            }
        }
    }
    else
    {
        if(tmpCostF[0] != ME_BIG_COST)
        {
            for(i=0; i<ME_NUM_OF_BLOCKS; i++)
            {
                m_cur.SkipCost[i]     = tmpCostF[i];
                m_cur.SkipType[i]     = ME_MbFrwSkipped;
                m_cur.InterType[i]    = ME_MbFrwSkipped;
                m_cur.SkipIdx[frw][i] = tmpIdxF[i];
            }

        }

        else//if(tmpCostB[0] != ME_BIG_COST)
        {
            for(i=0; i<ME_NUM_OF_BLOCKS; i++)
            {
                m_cur.SkipCost[i]     = tmpCostB[i];
                m_cur.SkipType[i]     = ME_MbBkwSkipped;
                m_cur.InterType[i]    = ME_MbBkwSkipped;
                m_cur.SkipIdx[bkw][i] = tmpIdxB[i];
            }
        }
    }

    MeMV mv;
    Ipp32s dir = m_cur.SkipType[0] == ME_MbFrwSkipped? frw: bkw;
    MeMV mvx[4];
    mvx[0]= m_cur.PredMV[dir][m_cur.SkipIdx[dir][0]][0];
    mvx[1]= m_cur.PredMV[dir][m_cur.SkipIdx[dir][1]][1];
    mvx[2]= m_cur.PredMV[dir][m_cur.SkipIdx[dir][2]][2];
    mvx[3]= m_cur.PredMV[dir][m_cur.SkipIdx[dir][3]][3];
    mv.x=median4(mvx[0].x, mvx[1].x,mvx[2].x,mvx[3].x);
    mv.y=median4(mvx[0].y, mvx[1].y,mvx[2].y,mvx[3].y);

    m_cur.RefDir = dir;
    Ipp32s c_idx = m_cur.SkipIdx[dir][0]+m_cur.SkipIdx[dir][1]+m_cur.SkipIdx[dir][2]+m_cur.SkipIdx[dir][3]; 
    m_cur.RefIdx = c_idx < 3? 0 : 1;

    if(m_PredCalc->IsOutOfBound(ME_Mb16x16, m_par->PixelType, mv))
    {
        for(i=0; i<ME_NUM_OF_BLOCKS; i++)
        {
            m_cur.SkipCost[i] = ME_BIG_COST;
        }
        return false;
    }
    //MeMV cmv = GetChromaMV(mv);
    // ME_Mb16x16 is used here to process chroma as 8x8 block
    MeAdr src, ref; src.chroma=ref.chroma=true; //true means load also chroma plane addresses
    MakeRefAdrFromMV(ME_Mb16x16, m_par->PixelType, mv, &ref);
    MakeSrcAdr(ME_Mb16x16, m_par->PixelType, &src);

    Ipp32s tmpCostU,tmpCostV;

    if(m_CRFormat == ME_YUV420)
    {
        src.channel = ref.channel = U;
        tmpCostU = GetCost(m_par->CostMetric, ME_Mb8x8, &src, &ref);
        src.channel = ref.channel = V;
        tmpCostV = GetCost(m_par->CostMetric, ME_Mb8x8, &src, &ref);
    }
    else//NV12
    {
        src.channel = ref.channel = U;
        tmpCostU = GetCost(m_par->CostMetric, ME_Mb8x8, &src, &ref);
        ref.move(U,8,0);
        src.move(U,8,0);
        tmpCostV = GetCost(m_par->CostMetric, ME_Mb8x8, &src, &ref);
    }
   // Ipp32s th = m_SkippedThreshold/4;
   //  if(tmpCostU < th && tmpCostV < th)
   // {
   //     return true;
   // }
    Ipp32s th = m_SkippedThreshold/2;
    if(tmpCostU + tmpCostV < th)
    {
        return true;
    }
    for(i=0; i<ME_NUM_OF_BLOCKS; i++)
    {
        m_cur.SkipCost[i] = ME_BIG_COST;
    }
    return false;
}//EstimateSkip8x8()

void MeVC1::EstimateMB8x8()
{
    //estimate 16x16, exit search if result is good enough
    EstimateMB16x16();
    //if(EarlyExitAfter16x16()) return;

    bool IsFieldPic = (m_par->PredictionType == ME_VC1Field1 ||
        m_par->PredictionType == ME_VC1Field2 || m_par->PredictionType == ME_VC1Field2Hybrid);
    m_PredCalc->ResetPredictors();
    //reset costs
    m_cur.Reset();

    //for all block in MB
    m_cur.MbPart = ME_Mb8x8;

    if(EstimateSkip())
    {
        MakeMBModeDecision8x8();
        return;
    }

    Ipp32s i;
    for(m_cur.BlkIdx=0; m_cur.BlkIdx<4; m_cur.BlkIdx++)
    {
        for(i=0; i<m_par->FRefFramesNum; i++)
        {
            SetReferenceFrame(frw,i);
            m_cur.PredMV[m_cur.RefDir][i][m_cur.BlkIdx]=m_PredCalc->GetPrediction();
            if(m_cur.BlkIdx > 0)
                m_cur.BestMV[m_cur.RefDir][i][m_cur.BlkIdx] = m_cur.BestMV[m_cur.RefDir][i][0] ;
            EstimateInter();
        }
        if(m_par->SearchDirection == ME_ForwardSearch && !IsFieldPic)
            EstimateIntra();
        MakeBlockModeDecision();
    }
    if(m_par->SearchDirection == ME_BidirSearch)
    {
        Ipp32s InterCost[4];
        for(i=0; i<4; i++)
        {
            InterCost[i] = m_cur.InterCost[i];
            m_cur.InterCost[i] = ME_BIG_COST;
        }

        for(m_cur.BlkIdx=0; m_cur.BlkIdx<4; m_cur.BlkIdx++)
        {
            for(i=0; i<m_par->BRefFramesNum; i++)
            {
                SetReferenceFrame(bkw,i);
                m_cur.PredMV[m_cur.RefDir][i][m_cur.BlkIdx]=m_PredCalc->GetPrediction();
            if(m_cur.BlkIdx > 0)
                m_cur.BestMV[m_cur.RefDir][i][m_cur.BlkIdx] = m_cur.BestMV[m_cur.RefDir][i][0] ;
                EstimateInter(); 
                MakeBlockModeDecision();
            }
        }
        Ipp32s SumF = InterCost[0]+ InterCost[1]+InterCost[2]+InterCost[3];
        Ipp32s SumB = m_cur.InterCost[0]+ m_cur.InterCost[1]+m_cur.InterCost[2]+m_cur.InterCost[3];
        if(SumF < SumB)
        {
            for(i=0; i<4; i++)
            {
                m_cur.InterType[i] = ME_MbFrw;
                m_cur.InterCost[i] = InterCost[i];
            }
        }
    }
    MakeMBModeDecision8x8();
}
///Function select best transforming type for inter block.

Ipp32s MeVC1::MakeTransformDecisionInterFast(MeMbPart mt, MePixelType pix,
                                           MeMbType type,
                                           MeMV mvF, MeMV mvB,
                                           Ipp32s IdxF, Ipp32s IdxB)
{
    MeAdr src, ref; src.chroma=ref.chroma=false; //true means load also chroma plane addresses
    MeAdr ref0, ref1; ref0.chroma=ref1.chroma=false; //true means load also chroma plane addresses
    Ipp32s mvSize = 0;

    if(type == ME_MbFrw)
    {
        m_cur.RefDir = frw;
        m_cur.RefIdx = IdxF;
        MakeRefAdrFromMV(mt, pix, mvF, &ref); 
    }
    else if(type == ME_MbBkw)
    {
        m_cur.RefDir = bkw;
        m_cur.RefIdx = IdxB;
        MakeRefAdrFromMV(mt, pix, mvB, &ref);
    }
    else if(type == ME_MbBidir || type == ME_MbDirect)
    {
        m_cur.RefDir = frw;
        m_cur.RefIdx = IdxF;
        MakeRefAdrFromMV(m_cur.MbPart, m_par->PixelType, mvF, &ref0);
        m_cur.RefDir = bkw;
        m_cur.RefIdx = IdxB;
        MakeRefAdrFromMV(m_cur.MbPart, m_par->PixelType, mvB, &ref1);

        ippiAverage16x16_8u_C1R(ref0.ptr[Y], ref0.step[Y], ref1.ptr[Y], ref1.step[Y], m_cur.buf[bidir].ptr[Y], 16);
        ref.set(Y, m_cur.buf[bidir].ptr[Y], 16);
        /*
        ippiAverage8x8_8u_C1R(ref0.ptr[U], ref0.step[U], ref1.ptr[U], ref1.step[U], m_cur.buf[bidir].ptr[U], 8);
        ref.set(U, m_cur.buf[bidir].ptr[U], 8);
        ippiAverage8x8_8u_C1R(ref0.ptr[V], ref0.step[V], ref1.ptr[V], ref1.step[V], m_cur.buf[bidir].ptr[V], 8);
        ref.set(V, m_cur.buf[bidir].ptr[V], 8);
        */
    }
    else
    {
        VM_ASSERT(0);
    }
    MakeSrcAdr(mt, pix, &src);

    if(mt == ME_Mb16x16)
    {
        //calculate transforming costs
        Ipp32s c[4][4];
        for(Ipp32s blk=0; blk<4; blk++){
            m_cur.BlkIdx = blk; //it is for DC prediction calculation
            //luma
            src.channel=Y;
            MeAdr src1 = src;
            MeAdr ref1 = ref;
            for(Ipp32s t=0; t<4; t++)
            {
                ippiSAD4x4_8u32s(src1.ptr[Y], src1.step[Y], ref1.ptr[Y], ref1.step[Y], 
                    &c[blk][t], 0);
                c[blk][t] /= 64;
                Ipp32s dx=4, dy=0;
                if(t==1){dx=-dx; dy=4;}
                src1.move(Y,dx,dy);
                ref1.move(Y,dx,dy);
            }
            bool vert = (abs(c[blk][0]- c[blk][1]) >= m_par->Quant) ||
                        (abs(c[blk][2]- c[blk][3]) >= m_par->Quant);
            bool hor  = (abs(c[blk][0]- c[blk][2]) >= m_par->Quant) ||
                        (abs(c[blk][1]- c[blk][3]) >= m_par->Quant);
            if(!vert && !hor)
            {
                m_cur.InterTransf[type][blk] = ME_Tranform8x8;
            }
            else if(!vert && hor)
            {
                m_cur.InterTransf[type][blk] = ME_Tranform8x4;
            }
            else if(vert && !hor)
            {
                m_cur.InterTransf[type][blk] = ME_Tranform4x8;
            }
            else
            {
                m_cur.InterTransf[type][blk] = ME_Tranform4x4;
            }
            Ipp32s dx=8, dy=0;
            if(blk==1){dx=-dx; dy=8;}
            src.move(Y,dx,dy);
            ref.move(Y,dx,dy);
        }
        m_cur.BlkIdx =0; // TODO: restore original for 8x8 block

        m_cur.InterTransf[type][4] = ME_Tranform8x8;
        m_cur.InterTransf[type][5] = ME_Tranform8x8;
    }
    else if(mt == ME_Mb8x8)
    {
        //calculate transforming costs
        Ipp32s c[4];

        for(Ipp32s t=0; t<4; t++)
        {
            ippiSAD4x4_8u32s(src.ptr[Y], src.step[Y], ref.ptr[Y], ref.step[Y], 
                &c[t], 0);
            c[t] /= 64;
        }
        bool vert = (abs(c[0]- c[1]) >= m_par->Quant) ||
                    (abs(c[2]- c[3]) >= m_par->Quant);
        bool hor  = (abs(c[0]- c[2]) >= m_par->Quant) ||
                    (abs(c[1]- c[3]) >= m_par->Quant);
        if(!vert && !hor)
        {
            m_cur.InterTransf[type][m_cur.BlkIdx] = ME_Tranform8x8;
        }
        else if(!vert && hor)
        {
            m_cur.InterTransf[type][m_cur.BlkIdx] = ME_Tranform8x4;
        }
        else if(vert && !hor)
        {
            m_cur.InterTransf[type][m_cur.BlkIdx] = ME_Tranform4x8;
        }
        else
        {
            m_cur.InterTransf[type][m_cur.BlkIdx] = ME_Tranform4x4;
        }
    }
    else
    {
        VM_ASSERT(0);
    }
    return 0;
}

MeCostRD MeVC1::MakeTransformDecisionInter(MeMbPart mt, MePixelType pix,
                                           MeMbType type,
                                           MeMV mvF, MeMV mvB,
                                           Ipp32s IdxF, Ipp32s IdxB)
{
   // SetError((vm_char*)"Wrong SearchDirection in MakeTransformDecisionInter", m_par->SearchDirection != ME_ForwardSearch);
    SetError((vm_char*)"Wrong MbPart in MakeTransformDecisionInter", mt != ME_Mb16x16);
  //  SetError((vm_char*)"Wrong RefFrame in MakeTransformDecisionInter", m_cur.RefDir != frw || m_cur.RefIdx!=0||m_cur.BlkIdx!=0);

    //calculate adresses
    MeCostRD res=0;
    MeAdr src, ref; src.chroma=ref.chroma=true; //true means load also chroma plane addresses
    MeAdr ref0, ref1; ref0.chroma=ref1.chroma=true; //true means load also chroma plane addresses
    Ipp32s mvSize = 0;
    Ipp32s BestCost = ME_BIG_COST;
    Ipp32s CurCost  = ME_BIG_COST;

    if(type == ME_MbFrw)
    {
        m_cur.RefDir = frw;
        m_cur.RefIdx = IdxF;
        MakeRefAdrFromMV(mt, pix, mvF, &ref); 
    }
    else if(type == ME_MbBkw)
    {
        m_cur.RefDir = bkw;
        m_cur.RefIdx = IdxF;
        MakeRefAdrFromMV(mt, pix, mvB, &ref);
    }
    else if(type == ME_MbBidir || type == ME_MbDirect)
    {
        m_cur.RefDir = frw;
        m_cur.RefIdx = IdxF;
        MakeRefAdrFromMV(m_cur.MbPart, m_par->PixelType, mvF, &ref0);
        m_cur.RefDir = bkw;
        m_cur.RefIdx = IdxB;
        MakeRefAdrFromMV(m_cur.MbPart, m_par->PixelType, mvB, &ref1);

        ippiAverage16x16_8u_C1R(ref0.ptr[Y], ref0.step[Y], ref1.ptr[Y], ref1.step[Y], m_cur.buf[bidir].ptr[Y], 16);
        ref.set(Y, m_cur.buf[bidir].ptr[Y], 16);

        if(m_CRFormat == ME_YUV420)
        {
            ippiAverage8x8_8u_C1R(ref0.ptr[U], ref0.step[U], ref1.ptr[U], ref1.step[U], m_cur.buf[bidir].ptr[U], 8);
            ref.set(U, m_cur.buf[bidir].ptr[U], 8);
            ippiAverage8x8_8u_C1R(ref0.ptr[V], ref0.step[V], ref1.ptr[V], ref1.step[V], m_cur.buf[bidir].ptr[V], 8);
            ref.set(V, m_cur.buf[bidir].ptr[V], 8);
        }
        else
        {
            ippiAverage8x8_8u_C1R(ref0.ptr[U], ref0.step[U], ref1.ptr[U], ref1.step[U], m_cur.buf[bidir].ptr[U], 16);
            ippiAverage8x8_8u_C1R(ref0.ptr[U]+8, ref0.step[U], ref1.ptr[U]+8, ref1.step[U], m_cur.buf[bidir].ptr[U]+8, 16);
            ref.set(U, m_cur.buf[bidir].ptr[U], 16);
        }
    }
    else
    {
        VM_ASSERT(0);
    }
    MakeSrcAdr(mt, pix, &src);

    //calculate transforming costs
    MeTransformType tr[4]={ME_Tranform8x8, ME_Tranform8x4, ME_Tranform4x8,ME_Tranform4x4};
    MeCostRD c[6][4]; 
    Ipp32s num_blk = (m_CRFormat == ME_YUV420) ? 6 : 5;
    for(Ipp32s blk=0; blk<num_blk; blk++){
        m_cur.BlkIdx = blk; //it is for DC prediction calculation
        if(blk<4){
            //luma
            src.channel=Y;
            for(Ipp32s t=0; t<4; t++){
                c[blk][t] = GetCostRD(ME_InterRD,ME_Mb8x8,tr[t],&src,&ref);
            }
            Ipp32s dx=8, dy=0;
            if(blk==1){dx=-dx; dy=8;}
            src.move(Y,dx,dy);
            ref.move(Y,dx,dy);
        }else{
            //chroma
            src.channel=(blk==4?U:V);
            c[blk][0] = GetCostRD(ME_InterRD,ME_Mb8x8,ME_Tranform8x8,&src,&ref);
            if(num_blk == 5) c[5][0] = 0;
        }
    }
    m_cur.BlkIdx =0; // TODO: restore original for 8x8 block

    m_cur.InterTransf[type][4] = ME_Tranform8x8;
    m_cur.InterTransf[type][5] = ME_Tranform8x8;
    //Ipp32s BestCost=ME_BIG_COST;//!!!!!!!!!!!!!!!!!
    Ipp32s ContCounter=0;
    Ipp32s b0, b1, b2, b3;
    for(Ipp32s i0=0; i0<4; i0++){ //cycle for all transform types
        for(Ipp32s i1=0; i1<4; i1++){
            for(Ipp32s i2=0; i2<4; i2++){
                for(Ipp32s i3=0; i3<4; i3++){
                    CurCost=c[0][i0].J+c[1][i1].J+c[2][i2].J+c[3][i3].J;
                    if(CurCost>=BestCost)continue;
                    m_cur.InterTransf[type][0] = tr[i0];
                    m_cur.InterTransf[type][1] = tr[i1];
                    m_cur.InterTransf[type][2] = tr[i2];
                    m_cur.InterTransf[type][3] = tr[i3];
                    res=0;
                    res+=c[0][i0];
                    res+=c[1][i1];
                    res+=c[2][i2];
                    res+=c[3][i3];
                    res+=c[4][0];
                    res+=c[5][0];
                    AddHeaderCost(res, m_cur.InterTransf[type], type,0,0);
                    CurCost=(Ipp32s)(res.D+m_lambda*res.R);
                    if(CurCost<BestCost){
                        BestCost=CurCost;
                        b0=i0; b1=i1;b2=i2;b3=i3;
                    }
                }
            }
        }
    }
    

    m_cur.InterTransf[type][0] = tr[b0];
    m_cur.InterTransf[type][1] = tr[b1];
    m_cur.InterTransf[type][2] = tr[b2];
    m_cur.InterTransf[type][3] = tr[b3];
    res=0;
    res+=c[0][b0];
    res+=c[1][b1];
    res+=c[2][b2];
    res+=c[3][b3];
    res+=c[4][0];
    res+=c[5][0];

        //add header cost
    AddHeaderCost(res, m_cur.InterTransf[type], type,0,0);


    //it doesn't work. VST gives significanrly better quality and significantly bigger bit rate, so lambda control
    //is not accurate enough to separate small gain from loss
#if 0
    //calculate VST penalty
    MeCostRD hom=0; //homogenious 8x8 cost
    hom+=c[0][0]; hom+=c[1][0];  hom+=c[2][0];  hom+=c[3][0];  hom+=c[4][0];  hom+=c[5][0];
    AddHeaderCost(hom, NULL, ME_MbFrw);
    VstPenalty += res.D-hom.D+m_lambda*(res.R-hom.R);
    printf("[%4d] novst d=%5d r=%5d J=%9.1f vst d=%5d r=%5d J=%9.1f ratio=%3d%%\n",
        m_cur.adr, hom.D, hom.R, hom.D+m_lambda*hom.R, res.D, res.R, res.D+m_lambda*res.R, (int)(100.*(1.-(hom.D+m_lambda*hom.R)/(res.D+m_lambda*res.R))));
#endif

#ifdef ME_DEBUG
//    if(ME_MB_IF_ADR(m_cur.adr)){
//        printf("[%d] VST selected (%d:%d:%d:%d) costs(%d:%d:%d:%d)\n", m_cur.adr, m_cur.InterTransf[0],m_cur.InterTransf[1],m_cur.InterTransf[2],m_cur.InterTransf[3], c[0][b0].J, c[1][b1].J,c[2][b2].J,c[3][b3].J);
//    }
#endif
    return res;
}

Ipp8u MeVC1::GetCoeffMode( Ipp32s &run, Ipp32s &level, const Ipp8u *pTableDR, const Ipp8u *pTableDL)
{
    bool sign  = level < 0;
    level = (sign)? -level : level;
    if (run <= pTableDR[1])
    {
        Ipp8u maxLevel = pTableDL[run];
        if (level <= maxLevel)
        {
            return 0;
        }
        if (level <= 2*maxLevel)
        {
            level = level - maxLevel;
            return 1;
        }
    }
    if (level <= pTableDL[0])
    {
        Ipp8u maxRun = pTableDR[level];
        if (run <= (Ipp8u)(2*maxRun + 1))
        {
            run = run - (Ipp8u)maxRun - 1;
            return 2;
        }
    }
    return 3;
}

//DC here is predicted value, only for Intra
Ipp32s MeVC1::GetAcCoeffSize(Ipp16s *ptr, Ipp32s DC, MeTransformType transf, Ipp32s QP, bool luma, bool intra)
{
    static const int DCEscIdx=119;
    const Ipp32u*  pDC;
    if(luma)pDC=m_par->DcTableLuma;
    else pDC=m_par->DcTableChroma;

    MeACTablesSet *tblSet;
    if(!intra){
        if(luma)tblSet = (MeACTablesSet*)&ACTablesSet[CodingSetsInter[m_cur.qp8][m_cur.AcTableIndex]];
        else tblSet = (MeACTablesSet*)&ACTablesSet[CodingSetsInter[m_cur.qp8][m_cur.AcTableIndex]];
    }else{
        if(luma)tblSet = (MeACTablesSet*)&ACTablesSet[LumaCodingSetsIntra[m_cur.qp8][m_cur.AcTableIndex]];
        else tblSet = (MeACTablesSet*)&ACTablesSet[ChromaCodingSetsIntra[m_cur.qp8][m_cur.AcTableIndex]];
    }

    const Ipp8u *scan = m_par->ScanTable[0];
    Ipp32s NumOfSubBlocks=1;
    Ipp32s SubBlockSize=64;
    switch(transf){
        case ME_Tranform4x4:
            NumOfSubBlocks=4;
            scan = m_par->ScanTable[3];
            SubBlockSize=16;
            break;
        case ME_Tranform4x8:
            NumOfSubBlocks=2;
            scan = m_par->ScanTable[2];
            SubBlockSize=32;
            break;
        case ME_Tranform8x4:
            NumOfSubBlocks=2;
            scan = m_par->ScanTable[1];
            SubBlockSize=32;
            break;
        case ME_Tranform8x8:
            //NumOfSubBlocks=1;
            //scan = m_par->ScanTable[0];
            //SubBlockSize=64;
            break;
    }

    Ipp32s length=0;
    for(Ipp32s sb=0; sb<NumOfSubBlocks; sb++,ptr+=SubBlockSize){
        Ipp32s run=0, level;
        Ipp32s first=(intra?1:0); //don't take intra DC into account
        Ipp32s last=-1;
        Ipp32s i;
        int mode;
        const Ipp32u    *pEnc   = tblSet->pEncTable;
        const Ipp8u     *pDR    = tblSet->pTableDR;
        const Ipp8u     *pDL    = tblSet->pTableDL;
        const Ipp8u     *pIdx   = tblSet->pTableInd;
         
        //process Intra DC
        if(intra){
            DC=abs(DC);
            Ipp32s AddOn=1, EscAddOn=9; 
            if(QP/2==1){DC=(DC+3)>>2;AddOn=3;EscAddOn=11;}
            else if(QP/2==2){DC=(DC+1)>>1;AddOn=2;EscAddOn=10;}

            if(DC==0) length+=pDC[1];
            else if(DC<DCEscIdx) length+=pDC[2*DC+1]+AddOn;
            else length+=pDC[2*DCEscIdx+1]+EscAddOn;
        }

        //find last coeff
        for(i=first; i<SubBlockSize; i++){
            if(ptr[scan[i]]!=0)last=i;
        }

        //process coeff
        for(i=first; i<=last; i++){ 
            if((level=ptr[scan[i]])==0){
                run++;
                continue;
            }
            
            //switch tables
            if(i==last){
                pDR    = tblSet->pTableDRLast;
                pDL    = tblSet->pTableDLLast;
                pIdx   = tblSet->pTableIndLast;
            }

            mode=GetCoeffMode(run, level, pDR, pDL);
            switch(mode){
                case 3:{
                    //write down run and level size, it is performed one time for  frame, field or slice so skip it!
                    //static bool FirstTime = true; //this is for frame, field, slice
                    //if(FirstTime){
                    //    FirstTime=false;
                    //    if(QP<=15) length+=5; //this is for level size, see WriteBlockAC and 7.1.4.10 
                    //    else length+=6;
                    //    length +=2; //this is for run size
                    //}

                    length += pEnc[1] + 2 + 1; //escape + mode + last
                    int LevelSize=8;
                    if(QP<=15) LevelSize=11; //see WriteBlockAC and 7.1.4.10 
                    length+=LevelSize+6 + 1; //6 is for run size, 1 is for level sign
                    break;
                }
                case 2:
                case 1:
                    length += pEnc[1]+mode; //escape
                case 0:
                    Ipp32s index = pIdx[run] + level;
                    length += pEnc[2*index + 1]+1;
                    break;
            }
            run=0;
        }
    }
    return length;
}

Ipp32s MeVC1::GetOneAcCoeffSize(Ipp32s level, Ipp32s run, Ipp32s QP, bool dc, bool luma, bool intra, bool last)
{
    static const int DCEscIdx=119;
    const Ipp32u*  pDC;
    if(luma)pDC=m_par->DcTableLuma;
    else pDC=m_par->DcTableChroma;

    MeACTablesSet *tblSet;
    if(!intra){
        if(luma)tblSet = (MeACTablesSet*)&ACTablesSet[CodingSetsInter[m_cur.qp8][m_cur.AcTableIndex]];
        else tblSet = (MeACTablesSet*)&ACTablesSet[CodingSetsInter[m_cur.qp8][m_cur.AcTableIndex]];
    }else{
        if(luma)tblSet = (MeACTablesSet*)&ACTablesSet[LumaCodingSetsIntra[m_cur.qp8][m_cur.AcTableIndex]];
        else tblSet = (MeACTablesSet*)&ACTablesSet[ChromaCodingSetsIntra[m_cur.qp8][m_cur.AcTableIndex]];
    }

    Ipp32s length=0;
    int mode;

    const Ipp32u    *pEnc   = tblSet->pEncTable;
    const Ipp8u     *pDR    = tblSet->pTableDR;
    const Ipp8u     *pDL    = tblSet->pTableDL;
    const Ipp8u     *pIdx   = tblSet->pTableInd;

    if(last){
        pDR    = tblSet->pTableDRLast;
        pDL    = tblSet->pTableDLLast;
        pIdx   = tblSet->pTableIndLast;
    }
         
    if(intra && dc){
        //process Intra DC
        //1 don't take DC rate into account for now due to predicition
        level = 1;
        level=abs(level);
        Ipp32s AddOn=1, EscAddOn=9; 
        if(QP/2==1){level=(level+3)>>2;AddOn=3;EscAddOn=11;}
        else if(QP/2==2){level=(level+1)>>1;AddOn=2;EscAddOn=10;}

        if(level==0) length+=pDC[1];
        else if(level<DCEscIdx) length+=pDC[2*level+1]+AddOn;
        else length+=pDC[2*DCEscIdx+1]+EscAddOn;
    }else{
        //inter
        mode=GetCoeffMode(run, level, pDR, pDL);
        switch(mode){
            case 3:{
                //write down run and level size, it is performed one time for  frame, field or slice so skip it!
                //static bool FirstTime = true; //this is for frame, field, slice
                //if(FirstTime){
                //    FirstTime=false;
                //    if(QP<=15) length+=5; //this is for level size, see WriteBlockAC and 7.1.4.10 
                //    else length+=6;
                //    length +=2; //this is for run size
                //}

                length += pEnc[1] + 2 + 1; //escape + mode + last
                int LevelSize=8;
                if(QP<=15) LevelSize=11; //see WriteBlockAC and 7.1.4.10 
                length+=LevelSize+6 + 1; //6 is for run size, 1 is for level sign
                break;
            }
            case 2:
            case 1:
                length += pEnc[1]+mode; //escape
            case 0:
                Ipp32s index = pIdx[run] + level;
                length += pEnc[2*index + 1]+1;
                break;
        }
        run=0;
    }
    return length;
}

#define DecreaseLevel(x,n) (((x)<0?(x)+(n):(x)-(n)))
#define DivAndRound(x,d) (x<0?(x-d/2)/d:(x+d/2)/d)
#define SaveNode(x) \
                        cur=&m_cur.trellis[c][CurNodeIdx++];\
                        *cur=(*(x));\
                        cur->level=level;\
                        cur->run++;\
                        cur->rc=RC;\
                        cur->cost+=NormDif;\
                        cur->prev=(x);

void MeVC1::QuantTrellis(Ipp16s *ptr, Ipp8s* prc, Ipp32s /*DC*/, MeTransformType transf, Ipp32s QP, bool luma, bool intra)
{
    const Ipp32s scaling=16;
    const Ipp32s norm = (Ipp32s)(1.0*scaling); //it is to cast frequency domain distortion to spatial and to account for IntLambda scaling, 1.0 is good enough
    const Ipp32s IntLambda = (Ipp32s)(scaling*m_lambda);
    Ipp16s coeff[64];
    Ipp16s quant[64];
    Ipp8s rc[64];
    Ipp32s SubBlockSize=64;
    const Ipp8u *scan = m_par->ScanTable[0];
    switch(transf){
        case ME_Tranform4x4:
            scan = m_par->ScanTable[3];
            SubBlockSize=16;
            break;
        case ME_Tranform4x8:
            scan = m_par->ScanTable[2];
            SubBlockSize=32;
            break;
        case ME_Tranform8x4:
            scan = m_par->ScanTable[1];
            SubBlockSize=32;
            break;
        case ME_Tranform8x8:
            //scan = m_par->ScanTable[0];
            //SubBlockSize=64;
            break;
    }

    //rearrange coefficient according to scan matrix
    for(Ipp32s i=0; i<SubBlockSize; i++)
        coeff[i] = ptr[scan[i]];
    
    //init trellis
    //set two start points, first one for "not last" node, second for "last" in VC1 VLC AC coefficients coding sense
    m_cur.trellis[0][0].Reset();
    m_cur.trellis[0][1].Reset();
    m_cur.trellis[0][1].last=true;
    
    for(Ipp32s c=1; c<SubBlockSize+1; c++){ //for all coeff
        MeTrellisNode BestLast;
        BestLast.Reset();
        BestLast.last=true;
        Ipp32s BestLastCost=IPP_MAX_32S;
        Ipp32s CurNodeIdx=0;
        MeTrellisNode *cur;
        MeTrellisNode *prev;
        
        //3 for all levels of current coeff
        Ipp32s level=DivAndRound(coeff[c-1],QP);
        Ipp32s OrgLevel=level;
        Ipp32s RC = 0;
        Ipp32s cq = coeff[c-1]/QP;
        Ipp32s RCinit = 0;
        //bool sign = cq < 0 ? true: false;
        if(cq != OrgLevel)
        {
            RC = -1;
            RCinit = -1;
        }
        for(Ipp32s n=0; n<NumOfLevelInTrellis; n++){
        //while(RC < 2){
            Ipp32s dif=coeff[c-1] - QP*level;
            Ipp32s NormDif=norm*dif*dif;
           
            if(level == 0){
                if(OrgLevel!=0){
                        //3 coeff is zero
                        //copy all previous nodes excluding last, and nodes whose neighbour is better
                        //neighbour is better if its cost is less or eqaul and run is less or equal
                        MeTrellisNode*a=NULL, *b=NULL;
                        for(Ipp32s p=0; m_cur.trellis[c-1][p].active && !m_cur.trellis[c-1][p].last; p++){
                            if(a==NULL){
                                a=&m_cur.trellis[c-1][p];
                                continue;
                            }
                            b=&m_cur.trellis[c-1][p];

                            if(a->cost<=b->cost && a->run <=b->run){
                                //leave a
                                SaveNode(a);
                            }else if(a->cost>=b->cost && a->run>=b->run){
                                //leave b
                                SaveNode(b);
                                a=b;
                            }else{
                                //leave both
                                SaveNode(a);
                                a=b;
                                b=NULL;
                            }
                        }
                        if(b==NULL && a!=NULL) //there was only one previous node, save it
                            SaveNode(a);
                    }else{
                    //copy all previous nodes excluding last and increase their run and cost
                    for(Ipp32s p=0; m_cur.trellis[c-1][p].active && !m_cur.trellis[c-1][p].last; p++){
                        cur=&m_cur.trellis[c][CurNodeIdx++];
                        *cur=m_cur.trellis[c-1][p];
                        cur->level=level;
                        cur->run++;
                        cur->rc=RC;
                        cur->cost+=NormDif;
                        cur->prev=&m_cur.trellis[c-1][p];
                    }
                }
                break; //zero level was processed, there is nothing more to do, go to "last" node processing
            }else{
                //3 coeff is not zero
                //find best path
                Ipp32s BestCost=IPP_MAX_32S;
                //for all previous points
                cur=&m_cur.trellis[c][CurNodeIdx++];
                cur->Reset();
                cur->level=level;
                cur->rc = RC;
                for(Ipp32s p=0; m_cur.trellis[c-1][p].active && !m_cur.trellis[c-1][p].last; p++){
                    //"not last" path
                    prev = &m_cur.trellis[c-1][p];
                    Ipp32s cost=NormDif+IntLambda*GetOneAcCoeffSize(level, prev->run, QP, c==1, luma, intra, false) + prev->cost;
                    if(cost<BestCost){
                        BestCost=cost; 
                        cur->cost=cost;
                        cur->prev=prev;
                    }
                    //"last" path
                    Ipp32s LastCost=NormDif+IntLambda*GetOneAcCoeffSize(level, prev->run, QP, c==1, luma, intra, true) + prev->cost;
                    if(LastCost<BestLastCost){
                        BestLastCost=LastCost; 
                        BestLast.level=level;
                        BestLast.cost=LastCost;
                        BestLast.prev=prev;
                        BestLast.rc=RC; 
                    }
                }
            }
            level = DecreaseLevel(level,1);
            RC += 1;
        }

        //compare previous last with current last, leave the best
        cur=&m_cur.trellis[c][CurNodeIdx++];
        Ipp32s p;
        for(p=0; !m_cur.trellis[c-1][p].last; p++);
        if(m_cur.trellis[c-1][p].cost+norm*coeff[c-1]*coeff[c-1] < BestLastCost){
            //prev is better
            cur->active = true;
            cur->last = true;
            cur->level = 0;
            Ipp32s shift = OrgLevel < 0 ? -OrgLevel : OrgLevel;
            cur->rc = RCinit + shift;
            cur->cost =m_cur.trellis[c-1][p].cost+norm*coeff[c-1]*coeff[c-1];
            cur->prev = &m_cur.trellis[c-1][p];
        }else{
            //new is better
            *cur = BestLast;
        }
    }

    //find best "last" in last collumn, it is optimal solution 
    Ipp32s p;
    for(p=0; !m_cur.trellis[SubBlockSize][p].last; p++);
    MeTrellisNode *prev = &m_cur.trellis[SubBlockSize][p];

    for(int x=SubBlockSize-1; x>=0; x--){
        quant[x] = (Ipp16s)prev->level;
        rc[x] = (Ipp8s)prev->rc;
        prev=prev->prev;
    }

    //perform inverse scan
    for(Ipp32s i=0; i<SubBlockSize; i++)
    {
        ptr[scan[i]]=quant[i];
        prc[scan[i]]=rc[i];
    }
}

#if 0
void MeVC1::AddHeaderCost(MeCostRD &cost, MeTransformType *transf, MeMbType MbType)
{
    const Ipp16u*  pCBPCYTable=VLCTableCBPCY_PB[m_cur.CbpcyTableIndex];

    //Intra
    if(MbType==ME_MbIntra){
        Ipp32s         NotSkip =   (cost.BlkPat != 0)?1:0;
        const Ipp16u*   MVDiffTables=MVDiffTablesVLC[m_cur.MvTableIndex];
        cost.R+=MVDiffTables[2*(37*NotSkip + 36 - 1)+1];
        if(cost.BlkPat!=0){
            cost.R+=pCBPCYTable[2*cost.BlkPat+1];
        }
        return;
    }

    //Inter and there is no variable size transforming
    if((MbType==ME_MbFrw || MbType==ME_MbBkw || MbType==ME_MbBidir || MbType==ME_MbDirect)&& transf==NULL){
        if(cost.BlkPat!=0){
            cost.R+=pCBPCYTable[2*cost.BlkPat+1];
        }
        if(m_par->SearchDirection != ME_BidirSearch)
        return;
    }

    if(m_par->SearchDirection == ME_BidirSearch)
    {
        //temporary: Bfraction shold be transmitted!!!:
        Ipp32f Bfraction = 0.5;
        switch(MbType)
        {
        case ME_MbFrw:
            Bfraction < 0.5? cost.R+=3: cost.R+=4;
            break;
        case ME_MbBkw:
            Bfraction < 0.5? cost.R+=4: cost.R+=3;
            break;
        case ME_MbBidir:
            cost.R+=4;
            break;
        case ME_MbDirect:
            cost.R+=2;
            break;
        case ME_MbFrwSkipped:
            Bfraction < 0.5? cost.R=3: cost.R=4;
            break;
        case ME_MbBkwSkipped:
            Bfraction < 0.5? cost.R=4: cost.R=3;
            break;
        case ME_MbBidirSkipped:
            cost.R=4;
            break;
        case ME_MbDirectSkipped:
            cost.R=2;
            break;
        default:
            SetError((vm_char*)"Wrong macroblock type");
            break;
        }
        return;
    }//if(m_par->SearchDirection == ME_BidirSearch)

    //Inter, forward search:
    if(cost.BlkPat!=0){
        //3 CBPCY
        cost.R+=pCBPCYTable[2*cost.BlkPat+1];


        //3 VSTSMB
        const Ipp16s (*pTTMBVLC)[4][6]   = NULL;
        const Ipp8u  (* pTTBlkVLC)[6]    = NULL;
        const Ipp8u   *pSubPattern4x4VLC = NULL;

        if (m_par->Quant/2<5)
        {
            pTTMBVLC            =  TTMBVLC_HighRate;
            pTTBlkVLC           =  TTBLKVLC_HighRate;
            pSubPattern4x4VLC   =  SubPattern4x4VLC_HighRate;
        }
        else if (m_par->Quant/2<13)
        {
            pTTMBVLC            =  TTMBVLC_MediumRate;
            pTTBlkVLC           =  TTBLKVLC_MediumRate;
            pSubPattern4x4VLC   =  SubPattern4x4VLC_MediumRate;

        }
        else
        {
            pTTMBVLC            =  TTMBVLC_LowRate;
            pTTBlkVLC           =  TTBLKVLC_LowRate;
            pSubPattern4x4VLC   =  SubPattern4x4VLC_LowRate;
        }

        Ipp32s MbLevelTransf=(transf[0]==transf[1]&&transf[0]==transf[2]&&transf[0]==transf[3]&&transf[0]==transf[4]&& transf[0]==transf[5])?1:0;

        Ipp32s blk;
        for(blk=0; blk<6; blk++)
            if( cost.BlkPat & (1<<(5-blk)) ) break;
        Ipp32s FirstCodedBlk = blk;
        MeTransformType tr=transf[blk];    
    
        Ipp32s  subPat  = 0x3&(cost.SbPat>>(4*(5-blk))); //for 8x8 and 4x4 pattern doesn't matter so do it only for 8x4 4x8
        if(--subPat<0)subPat=0;

        cost.R+=pTTMBVLC[MbLevelTransf][tr][2*subPat+1];


        //3 VSTSBLK
        for(blk=0; blk<6; blk++){
            if((cost.BlkPat & (1<<(5-blk))) ==0)continue;
            tr=transf[blk]; 
            subPat  = 0xf&(cost.SbPat>>(4*(5-blk)));

            if (!MbLevelTransf && blk!= FirstCodedBlk){
                //block level transform type
                if(--subPat<0)subPat=0;
                cost.R+=pTTBlkVLC[tr][2*subPat+1];
            }
            subPat  = 0xf&(cost.SbPat>>(4*(5-blk)));
            if (tr == ME_Tranform4x4){
                cost.R+=pSubPattern4x4VLC[2*subPat+1];
            }
            else if (tr != ME_Tranform8x8 &&  (MbLevelTransf && blk!= FirstCodedBlk)){
                //if MB level or frame level
                subPat&=3;
                if(--subPat<0)subPat=0;
                cost.R+=SubPattern8x4_4x8VLC[2*subPat+1];
            }
        }
    }

}
#else
void MeVC1::AddHeaderCost(MeCostRD &cost, MeTransformType *transf, MeMbType MbType,Ipp32s RefIdxF,Ipp32s RefIdxB)
{
    const Ipp8u MBTypeFieldTable0_VLC[6*2] =
    {
        0,  5,
        1,  5,
        1,  1,
        1,  3,
        1,  2,
        1,  4
    };
    const Ipp16u*  pCBPCYTable=VLCTableCBPCY_PB[0];
    const Ipp32u*  pCBPCYTableF=CBPCYFieldTable_VLC[0];
    const Ipp8u*   pMBTypeFieldTable_VLC = MBTypeFieldTable0_VLC;
    bool IsFieldPic = (m_par->PredictionType == ME_VC1Field1 ||
                       m_par->PredictionType == ME_VC1Field2 || 
                       m_par->PredictionType == ME_VC1Field2Hybrid);

    if(!IsFieldPic)
    {
        //Intra
        if(MbType==ME_MbIntra)
        {
            //if(m_BitplanesRaw) cost.R+=1;//777
            Ipp32s         NotSkip =   (cost.BlkPat != 0)?1:0;
            //cost.R+=m_par->MVDiffTablesVLC[2*(37*NotSkip + 36 - 1)+1];
            const Ipp16u*   MVDiffTables=MVDiffTablesVLC[m_cur.MvTableIndex];
            cost.R+=MVDiffTables[2*(37*NotSkip + 36 - 1)+1];
            //cost.R+=1;//AC prediction;//777
            if(cost.BlkPat!=0){
                //cost.R+=pCBPCYTable[2*(cost.BlkPat&0x3F)+1];
                cost.R+=pCBPCYTable[2*cost.BlkPat+1];
            }
            return;
        }
        if(m_par->SearchDirection != ME_BidirSearch)
        {
          //  cost.R+=1;//skip/not skip???
         //   if(m_cur.HybridPredictor[0])
         //       cost.R+=1;//???
            if(MbType==ME_MbFrw && cost.BlkPat!=0)
                cost.R+=pCBPCYTable[2*cost.BlkPat+1];
        }
        //bidir search
        else
        {
            if(cost.BlkPat!=0 && 
                (MbType==ME_MbFrw || MbType==ME_MbBkw || 
                MbType==ME_MbBidir || MbType==ME_MbDirect)/*&& transf==NULL*/){
                    cost.R+=pCBPCYTable[2*cost.BlkPat+1];
            }

            switch(MbType)
            {
            case ME_MbFrw:
                m_par->Bfraction < 0.5? cost.R+=3: cost.R+=4;
                break;
            case ME_MbBkw:
                m_par->Bfraction < 0.5? cost.R+=4: cost.R+=3;
                break;
            case ME_MbBidir:
                cost.R+=4;
                break;
            case ME_MbDirect:
                cost.R+=2;
                break;
            case ME_MbFrwSkipped:
                m_par->Bfraction < 0.5? cost.R=3: cost.R=4;
                break;
            case ME_MbBkwSkipped:
                m_par->Bfraction < 0.5? cost.R=4: cost.R=3;
                break;
            case ME_MbBidirSkipped:
                cost.R=4;
                break;
            case ME_MbDirectSkipped:
                cost.R=2;
                break;
            default:
                SetError((vm_char*)"Wrong macroblock type");
                break;
            }
        }//bidir search
    }//!field pic
    else
    {
        if(MbType==ME_MbIntra){
            cost.R+=5;//MB-type
            cost.R+=1;//AC prediction;//777
            if(cost.BlkPat!=0){
                cost.R+=pCBPCYTableF[2*(cost.BlkPat&0x3F)+1];
            }
            return;
        }
        if(m_par->SearchDirection != ME_BidirSearch)
        {
            Ipp32s         NotSkip =   (cost.BlkPat != 0)?1:0;
            Ipp32s nMBType =   (((Ipp8u)NotSkip)<<1) + 3;
            cost.R += pMBTypeFieldTable_VLC[(nMBType<<1)+1];
           // if(m_cur.HybridPredictor[RefIdxF]) 
            //    cost.R+=1;//???
            cost.R+=pCBPCYTableF[2*(cost.BlkPat&0x3F)+1];
        }
        //bidir search:
        else
        {
            if(cost.BlkPat!=0 && 
                (MbType==ME_MbFrw || MbType==ME_MbBkw || 
                MbType==ME_MbBidir || MbType==ME_MbDirect)/*&& transf==NULL*/){
                    cost.R+=pCBPCYTableF[2*(cost.BlkPat&0x3F)+1];
            }
            if(m_BitplanesRaw) cost.R+=1;//777
            Ipp32s         NotSkip =   (cost.BlkPat != 0)?1:0;
            Ipp32s nMBType =   (((Ipp8u)NotSkip)<<1)+2;
            if(MbType==ME_MbFrw || MbType==ME_MbBkw || MbType==ME_MbBidir)
                nMBType += 1; 

            cost.R += pMBTypeFieldTable_VLC[(nMBType<<1)+1];

            switch(MbType)
            {
            case ME_MbFrw:
                break;
            case ME_MbBkw:
                cost.R+=1;
                break;
            case ME_MbBidir:
                cost.R+=3;
                break;
            case ME_MbDirect:
                cost.R+=2;
                break;
            case ME_MbFrwSkipped:
                cost.R=4;
                break;
            case ME_MbBkwSkipped:
                cost.R=4;
                break;
            case ME_MbBidirSkipped:
                cost.R=4;
                break;
            case ME_MbDirectSkipped:
                cost.R=4;
                break;
            default:
                SetError((vm_char*)"Wrong macroblock type");
                break;
            }
        }
    }//field pic

    if(transf==NULL) return;//no VST
    
    //Inter
    if(cost.BlkPat!=0){
        //3 VSTSMB
        const Ipp16s (*pTTMBVLC)[4][6]   = NULL;
        const Ipp8u  (* pTTBlkVLC)[6]    = NULL;
        const Ipp8u   *pSubPattern4x4VLC = NULL;

        if (m_par->Quant/2<5)
        {
            pTTMBVLC            =  TTMBVLC_HighRate;
            pTTBlkVLC           =  TTBLKVLC_HighRate;
            pSubPattern4x4VLC   =  SubPattern4x4VLC_HighRate;
        }
        else if (m_par->Quant/2<13)
        {
            pTTMBVLC            =  TTMBVLC_MediumRate;
            pTTBlkVLC           =  TTBLKVLC_MediumRate;
            pSubPattern4x4VLC   =  SubPattern4x4VLC_MediumRate;

        }
        else
        {
            pTTMBVLC            =  TTMBVLC_LowRate;
            pTTBlkVLC           =  TTBLKVLC_LowRate;
            pSubPattern4x4VLC   =  SubPattern4x4VLC_LowRate;
        }

        Ipp32s MbLevelTransf=(transf[0]==transf[1]&&transf[0]==transf[2]&&transf[0]==transf[3]&&transf[0]==transf[4]&& transf[0]==transf[5])?1:0;

        Ipp32s blk;
        for(blk=0; blk<6; blk++)
            if( cost.BlkPat & (1<<(5-blk)) ) break;
        Ipp32s FirstCodedBlk = blk;
        MeTransformType tr=transf[blk];

        Ipp32s  subPat  = 0x3&(cost.SbPat>>(4*(5-blk))); //for 8x8 and 4x4 pattern doesn't matter so do it only for 8x4 4x8
        if(--subPat<0)subPat=0;

        cost.R+=pTTMBVLC[MbLevelTransf][tr][2*subPat+1];


        //3 VSTSBLK
        for(blk=0; blk<6; blk++){
            if((cost.BlkPat & (1<<(5-blk))) ==0)continue;
            tr=transf[blk];
            subPat  = 0xf&(cost.SbPat>>(4*(5-blk)));

            if (!MbLevelTransf && blk!= FirstCodedBlk){
                //block level transform type
                if(--subPat<0)subPat=0;
                cost.R+=pTTBlkVLC[tr][2*subPat+1];
            }
            subPat  = 0xf&(cost.SbPat>>(4*(5-blk)));
            if (tr == ME_Tranform4x4){
                cost.R+=pSubPattern4x4VLC[2*subPat+1];
            }
            else if (tr != ME_Tranform8x8 &&  (MbLevelTransf && blk!= FirstCodedBlk)){
                //if MB level or frame level
                subPat&=3;
                if(--subPat<0)subPat=0;
                cost.R+=SubPattern8x4_4x8VLC[2*subPat+1];
            }
        }
    }

}
#endif


Ipp32s MeVC1::GetMvSize(Ipp32s dx, Ipp32s dy, bool bNonDominant, bool hybrid)
{
    Ipp32s rangeIndex=m_par->MVRangeIndex;
    Ipp32s m_bMVHalf = m_par->PixelType == ME_HalfPixel ? true : false;
    static const int VC1_ENC_HALF_MV_LIMIT=95;
    static const int VC1_ENC_MV_LIMIT=159;
    Ipp32s NotSkip=(m_par->pSrc->MBs[m_cur.adr].NumOfNZ==0?0:1);

    bool IsField1 = m_par->PredictionType == ME_VC1Field1;

    bool IsField2 = (m_par->PredictionType == ME_VC1Field2 || 
                     m_par->PredictionType == ME_VC1Field2Hybrid); 


    if(dx==0 && dy==0 && NotSkip==0)
        return 0; //special case, residual equal to 0, mv equal to 0 - pure skip

    const Ipp16u* table = MVDiffTablesVLC[m_cur.MvTableIndex];;
    const Ipp8u* MVSizeOffset = m_par->MVSizeOffset;
    const Ipp8u* longMVLength=m_par->MVLengthLong;

    Ipp32s length=0;

    Ipp16s          index   =   0;
    bool            signX   =   (dx<0);
    bool            signY   =   (dy<0);
    Ipp8u           limit   =   (m_bMVHalf)? VC1_ENC_HALF_MV_LIMIT : VC1_ENC_MV_LIMIT;
    Ipp16s          indexX  =   0;
    Ipp16s          indexY  =   0;

    if(!IsField1 && !IsField2)
    {
    dx = dx*(1 -2*signX);
    dy = dy*(1 -2*signY);

    if (m_bMVHalf)
    {
        dx = dx >> 1;
        dy = dy >> 1;
    }

    index = (dx < limit && dy < limit)? 6*MVSizeOffset[3*dy]+ MVSizeOffset[3*dx] - 1:35;

    if (index < 34)
    {
            Ipp8u sizeX = MVSizeOffset[3*dx+2];
            Ipp8u sizeY = MVSizeOffset[3*dy+2];

            if (m_bMVHalf)
            {
                sizeX -= sizeX>>3;
                sizeY -= sizeY>>3;
            }
            index = (Ipp16s)(index+37*NotSkip);
            length += table[2*index+1] +sizeX+sizeY;
        }
        else
        {
             // escape mode
            Ipp8u sizeX = longMVLength[2*rangeIndex];
            Ipp8u sizeY = longMVLength[2*rangeIndex+1];

            index= (Ipp16s)(35 + 37*NotSkip - 1);

            if (m_bMVHalf)
            {
                sizeX -= sizeX>>3;
                sizeY -= sizeY>>3;
            }
            length += table[2*index+1]+sizeX+sizeY;
        }
        if(m_cur.MbPart == ME_Mb8x8 && hybrid)length+=1;
        return length;
    }
    //field1:
    if(IsField1)
    {
        dx = (signX)? -dx:dx;
        dy = (signY)? -dy:dy;

        if (m_bMVHalf)
        {
            dx = dx>>1;
            dy = dy>>1;
        }

        index = (dx <  m_par->FieldInfo.ME_limitX && dy < m_par->FieldInfo.ME_limitY)?
            9*(indexY = m_par->FieldInfo.ME_pMVSizeOffsetFieldIndexY[dy])+
                      (indexX = m_par->FieldInfo.ME_pMVSizeOffsetFieldIndexX[dx]) - 1:71;

        if (index < 71)
        {
            length += m_par->FieldInfo.ME_pMVModeField1RefTable_VLC[2*index+1];
            length += indexX + m_par->FieldInfo.ME_bExtendedX;
            length += indexY + m_par->FieldInfo.ME_bExtendedY;
        }
        else
        {
             // escape mode
            index       = 71;

            indexX = longMVLength[2*rangeIndex];
            indexY = longMVLength[2*rangeIndex+1];

            length += m_par->FieldInfo.ME_pMVModeField1RefTable_VLC[2*index+1]+indexX+indexY;
        }
        if(hybrid)length+=1;
        return length;
    }
    //field2:
    dx = (signX)? -dx:dx;
    dy = (signY)? -dy:dy;

    if (m_bMVHalf)
    {
        dx = dx>>1;
        dy = dy>>1;
    }
    index = (dx < m_par->FieldInfo.ME_limitX && dy < m_par->FieldInfo.ME_limitY)?
                9*(((indexY = m_par->FieldInfo.ME_pMVSizeOffsetFieldIndexY[dy])<<1) + bNonDominant)+
                    (indexX = m_par->FieldInfo.ME_pMVSizeOffsetFieldIndexX[dx]) - 1:125;

    if (index < 125)
    {
        dx = dx - m_par->FieldInfo.ME_pMVSizeOffsetFieldX[indexX];
        dy = dy - m_par->FieldInfo.ME_pMVSizeOffsetFieldY[indexY];

        dx =  (dx <<1)+signX;
        dy =  (dy <<1)+signY;

        length += m_par->FieldInfo.ME_pMVModeField1RefTable_VLC[2*index+1]+
            indexX + m_par->FieldInfo.ME_bExtendedX + 
            indexY + m_par->FieldInfo.ME_bExtendedY;
    }
    else
    {
        // escape mode

        Ipp32s     y = ((((signY)? -dy:dy) - bNonDominant)<<1) + bNonDominant;

        index       = 125;

        indexX = longMVLength[2*rangeIndex];
        indexY = longMVLength[2*rangeIndex+1];

        dx = ((1<<indexX)-1)&  ((signX)? -dx: dx);
        dy = ((1<<indexY)-1)&  (y);

        length += m_par->FieldInfo.ME_pMVModeField1RefTable_VLC[2*index+1]+indexX+indexY;
    }
    if(hybrid)length+=1;
    return length;
}


MeCostRD MeVC1::GetCostRD(MeDecisionMetrics CostMetric, MeMbPart mt, MeTransformType transf, MeAdr* src, MeAdr* ref)
{
    SetError((vm_char*)"Wrong metric in MeVC1::GetCostRD", CostMetric!=ME_InterRD && CostMetric!=ME_IntraRD);
    SetError((vm_char*)"Wrong partitioning in MeVC1::GetCostRD", CostMetric==ME_InterRD && mt!=ME_Mb8x8&&mt!=ME_Mb16x16 || CostMetric==ME_IntraRD&& mt!=ME_Mb8x8&&mt!=ME_Mb16x16);

    static const Ipp8u DCQuantValues[32] =
    {
        0,  2,  4,  8,  8,  8,  9,  9,
        10, 10, 11, 11, 12, 12, 13, 13,
        14, 14, 15, 15, 16, 16, 17, 17,
        18, 18, 19, 19, 20, 20, 21, 21
    };

    Ipp32s qp=m_par->Quant;
    bool UnQp=m_par->UniformQuant;
    bool intra = (CostMetric==ME_IntraRD);
    bool luma=m_cur.BlkIdx<4;
    Ipp32s ch=src->channel;
    Ipp32s num_block_proc = 1;

    Ipp32s num_blocks = (m_CRFormat == ME_YUV420)? 6 : 5;

    //1 Inter/Intra 16x16
    if(mt ==ME_Mb16x16){
        MeCostRD res=0;
        for(Ipp32s blk=0; blk<num_blocks;blk++){
            src->channel = (blk<4?Y:(blk==4?U:V));
            m_cur.BlkIdx = blk; //it is for DC prediction calculation
            res+=GetCostRD(CostMetric, ME_Mb8x8, transf, src, ref);
            Ipp32s dx=8, dy=0;
            if(blk==1){dx=-dx; dy=8;}
            src->move(Y,dx,dy);
            if(!intra)ref->move(Y,dx,dy);
        }
        m_cur.BlkIdx =0;
        return res;
    }

    //1 Inter/Intra 8x8

    if(src->channel != Y)
    {
        num_block_proc = (m_CRFormat == ME_YUV420)?  1 : 2; 
    }
    else
    {
        num_block_proc = 1;
    }

    Ipp16s buf1[2][64]; // TODO: move it to m_cur and allign
    Ipp16s buf2[64];
    Ipp8s buf3[64];
    Ipp8u rec[128]; //reconstructed block 64 - for YUV420, 128 - for NV12
    MeCostRD cost=0;
    IppiSize roi={8,8};
    MeCostRD cost_p = 0;

    if(num_block_proc == 1)
    {
        //3 dif
        if(intra)
            ippiConvert_8u16s_C1R(src->ptr[ch],src->step[ch],buf1[0],16,roi);
        else
            ippiSub8x8_8u16s_C1R(src->ptr[ch],src->step[ch],ref->ptr[ch],ref->step[ch],buf1[0],16);
    }
    else
    {
        if(intra)
            copyChromaBlockNV12 (src->ptr[ch],  NULL,    src->step[ch], 
                                        buf1[0], buf1[1],    16, 0);
        else
            copyDiffChromaBlockNV12 (src->ptr[ch],   NULL,    src->step[ch], 
                                             ref->ptr[ch],   NULL,    ref->step[ch], 
                                             buf1[0], buf1[1],    16, 0, 0);

    }

    
    for(Ipp32s k = 0; k < num_block_proc; k++)
    {
        Ipp16s dc;
        cost=0;
        m_cur.BlkIdx += k;

        if(intra) for(int i=0; i<64; i++)buf1[k][i]-=128;  // TODO: add ipp function
        

        if(!m_cur.VlcTableSearchInProgress){
            //3 transforming
            //destination buffer is considered linear here, i.e. one dimensional, so for first 4x4 block coefficients will occupy positions from 0 to 15
            switch(transf){
                case ME_Tranform4x4:
                    ippiTransform4x4Fwd_VC1_16s_C1R(buf1[k]+0, 16, buf2+0,8);  
                    ippiTransform4x4Fwd_VC1_16s_C1R(buf1[k]+4, 16, buf2+16,8);  
                    ippiTransform4x4Fwd_VC1_16s_C1R(buf1[k]+32, 16, buf2+32,8);  
                    ippiTransform4x4Fwd_VC1_16s_C1R(buf1[k]+36, 16, buf2+48,8);  
                    break;
                case ME_Tranform8x4:
                    ippiTransform8x4Fwd_VC1_16s_C1R(buf1[k], 16, buf2,16);
                    ippiTransform8x4Fwd_VC1_16s_C1R(buf1[k]+32, 16, buf2+32,16);
                    break;
                case ME_Tranform4x8:
                    ippiTransform4x8Fwd_VC1_16s_C1R(buf1[k], 16, buf2,8);
                    ippiTransform4x8Fwd_VC1_16s_C1R(buf1[k]+4, 16, buf2+32,8);
                    break;
                case ME_Tranform8x8:
                    ippiTransform8x8Fwd_VC1_16s_C1R(buf1[k], 16, buf2,16);
                    break;
            }

            //3 quantize
            dc=buf2[0];
            if(!m_par->UseTrellisQuantization || !m_par->UseTrellisQuantizationChroma && m_cur.BlkIdx>3){
                if(UnQp)ippiQuantInterUniform_VC1_16s_C1IR(buf2, 16, qp, roi);
                else ippiQuantInterNonuniform_VC1_16s_C1IR(buf2, 16, qp, roi);
                //if(intra) buf2[0] = (dc-910)/DCQuantValues[qp/2]; //910 accounts for -128 in spatial domain, unfortunately it is not accurate enough
                if(intra) buf2[0] = dc/DCQuantValues[qp/2];
            }else{
                SetError((vm_char*)"Wrong quantization type!!! NonUniform!", !UnQp);
                switch(transf){
                    case ME_Tranform4x4:
                        QuantTrellis(buf2+0, buf3+0,0, transf, qp, luma, intra);
                        QuantTrellis(buf2+16,buf3+16, 0, transf, qp, luma, intra);
                        QuantTrellis(buf2+32,buf3+32, 0, transf, qp, luma, intra);
                        QuantTrellis(buf2+48,buf3+48, 0, transf, qp, luma, intra);
                        break;
                    case ME_Tranform8x4:
                        QuantTrellis(buf2, buf3,0, transf, qp, luma, intra);
                        QuantTrellis(buf2+32, buf3 +32, 0, transf, qp, luma, intra);
                        break;
                    case ME_Tranform4x8:
                        QuantTrellis(buf2, buf3,0, transf, qp, luma, intra);
                        QuantTrellis(buf2+32, buf3 +32,0, transf, qp, luma, intra);
                        break;
                    case ME_Tranform8x8:
                        QuantTrellis(buf2, buf3,0, transf, qp, luma, intra);
                        break;
                }
                if(intra)
                {
                    buf2[0] = DivAndRound(dc,DCQuantValues[qp/2]);
                    Ipp32s cq = dc/DCQuantValues[qp/2];
                    if(buf2[0] != cq)
                    {
                        buf3[0] = -1;
                    }
                }

            }

            //save quantized coefficients for future use
            if(m_par->UseTrellisQuantization)
            {
                MFX_INTERNAL_CPY(m_cur.TrellisCoefficients[transf+(intra?4:0)][m_cur.BlkIdx],buf2,64*sizeof(buf2[0]));
                if((m_cur.BlkIdx < 4) ||(m_par->UseTrellisQuantizationChroma && m_cur.BlkIdx>3))
                {
                    MFX_INTERNAL_CPY(m_cur.RoundControl[transf+(intra?4:0)][m_cur.BlkIdx],buf3,64*sizeof(buf3[0]));
                }
            }
        }else{
            //restore quantized coefficients
            MFX_INTERNAL_CPY(buf2,m_cur.TrellisCoefficients[transf+(intra?4:0)][m_cur.BlkIdx],64*sizeof(buf2[0]));
        }

        
        //3 predict DC for Intra
        Ipp32s DcPrediction=0;
        MeMB *mb=&m_par->pSrc->MBs[m_cur.adr];
        if(intra){
            mb->DcCoeff[m_cur.BlkIdx]=buf2[0];
            DcPrediction = buf2[0] - GetDcPredictor(m_cur.BlkIdx);
        }
        
        //3 pattern
        //set block and subblock patterns. They are used for rate calculation including MV rate calculation.
        Ipp32s flag[4]={0};
        Ipp16s tmp=buf2[0];
        if(intra)buf2[0]=0;//don't take DC into account for intra
        for(Ipp32s i=0; i<4; i++)
            for(Ipp32s j=0; j<16; j++)
                flag[i]|=buf2[16*i+j];
        buf2[0]=tmp;
        for(Ipp32s i=0; i<4; i++)
            flag[i]=flag[i]==0?0:1;
        switch(transf){
            case ME_Tranform4x4:cost.SbPat=(flag[0]<<3)|(flag[1]<<2)|(flag[2]<<1)|(flag[3]<<0);break;
            case ME_Tranform8x4:
            case ME_Tranform4x8:cost.SbPat=(flag[0]<<1)|(flag[1]<<1)|(flag[2]<<0)|(flag[3]<<0);break;
            case ME_Tranform8x8:cost.SbPat=(flag[0]<<0)|(flag[1]<<0)|(flag[2]<<0)|(flag[3]<<0);break;
        }
            
        if(cost.SbPat!=0){
            cost.NumOfCoeff++;
            cost.BlkPat=(1<<(5-(m_cur.BlkIdx)));
            cost.SbPat=cost.SbPat<<(4*(5-(m_cur.BlkIdx)));
        }

        //3 Rate    
        cost.R=GetAcCoeffSize(buf2, DcPrediction, transf, qp, luma, intra);

        //skip distortion calculation for chroma
        if(luma || m_par->UseChromaForMD){
            //3 inverse quantize
            IppiSize DstSizeNZ;
            dc=buf2[0];
            if(UnQp)ippiQuantInvInterUniform_VC1_16s_C1IR(buf2, 16, qp, roi, &DstSizeNZ);
            else ippiQuantInvInterNonuniform_VC1_16s_C1IR(buf2, 16, qp, roi, &DstSizeNZ);
            if(intra) buf2[0] =dc*DCQuantValues[qp/2];

            //3 inv transform
            switch(transf){
                case ME_Tranform4x4:
                    ippiTransform4x4Inv_VC1_16s_C1R(buf2+0, 8, buf1[k]+0,16,roi);  
                    ippiTransform4x4Inv_VC1_16s_C1R(buf2+16, 8, buf1[k]+4,16,roi);  
                    ippiTransform4x4Inv_VC1_16s_C1R(buf2+32, 8, buf1[k]+32,16,roi);  
                    ippiTransform4x4Inv_VC1_16s_C1R(buf2+48, 8, buf1[k]+36,16,roi);  
                    break;
                case ME_Tranform8x4:
                    ippiTransform8x4Inv_VC1_16s_C1R(buf2, 16, buf1[k],16,roi);
                    ippiTransform8x4Inv_VC1_16s_C1R(buf2+32, 16, buf1[k]+32,16,roi);
                    break;
                case ME_Tranform4x8:
                    ippiTransform4x8Inv_VC1_16s_C1R(buf2, 8, buf1[k],16,roi);
                    ippiTransform4x8Inv_VC1_16s_C1R(buf2+32, 8, buf1[k]+4,16,roi);
                    break;
                case ME_Tranform8x8:
                    ippiTransform8x8Inv_VC1_16s_C1R(buf2, 16, buf1[k],16,roi);
                    break;
            }
            if(intra) for(int i=0; i<64; i++)buf1[k][i]+=128; // TODO: add ipp function
        }

        if(k == 0) cost_p = cost;

    }//for(k = 0; k < num_block_proc; k++)

    if(luma || m_par->UseChromaForMD)
    {
        if(num_block_proc == 1)
        {
            //3 inv dif
            if(intra)
                ippiConvert_16s8u_C1R(buf1[0],16,rec,8,roi);
            else
                ippiMC8x8_8u_C1(ref->ptr[ch],ref->step[ch],buf1[0],16,rec,8,0,0);

            //3 Distortion
            ippiSSD8x8_8u32s_C1R(rec,8,src->ptr[ch],src->step[ch],&cost.D,0);
        }
        else
        {
            if(intra)
                pasteChromaBlockNV12 (rec,  NULL, 16, buf1[0], buf1[1],16,0);

            else
                pasteSumChromaBlockNV12(ref->ptr[ch],  NULL,   ref->step[ch], 
                                             rec,  NULL,   16,
                                             buf1[0],   buf1[1],   16,
                                             0,   0);
            Ipp32s dist;
            ippiSSD8x8_8u32s_C1R(rec,16,src->ptr[ch],src->step[ch],&dist,0);
            cost.D = dist;
            ippiSSD8x8_8u32s_C1R(rec+8,16,src->ptr[ch]+8,src->step[ch],&dist,0);
            cost.D += dist;
            cost += cost_p;
        }

        cost.J=(Ipp32s)(cost.D+m_lambda*cost.R); //This is not exact RD cost calculation because there is no feedback here

        //check overflow
        if(cost.J<0)cost.J=ME_BIG_COST;
    }

    return cost;
}

Ipp32s MeVC1::GetDcPredictor(Ipp32s BlkIdx)
{
    // TODO: add chroma here
    //B A
    //C . 
    MeMB *mbs=m_par->pSrc->MBs;
    Ipp32s w=m_par->pSrc->WidthMB;


    MeMB *MbA=&mbs[m_cur.adr-w];
    MeMB *MbB=&mbs[m_cur.adr-w-1];
    MeMB *MbC=&mbs[m_cur.adr-1];

    bool A=(m_cur.y>0 && MbA->MbType==ME_MbIntra);
    bool B=(m_cur.y>0 && m_cur.x>0 && MbB->MbType==ME_MbIntra);
    bool C=(m_cur.x>0 && MbC->MbType==ME_MbIntra);

    const Ipp32s unav = ME_BIG_COST; //available

    Ipp32s PredA = unav, PredB=unav, PredC=unav;


    switch(BlkIdx)
    {
    case 0:
        if(A) PredA = mbs[m_cur.adr-w].DcCoeff[2];
        if(B) PredB = mbs[m_cur.adr-w-1].DcCoeff[3];
        if(C) PredC = mbs[m_cur.adr-1].DcCoeff[1];
        break;
    case 1:
        if(A) PredA = mbs[m_cur.adr-w].DcCoeff[3];
        if(A) PredB = mbs[m_cur.adr-w].DcCoeff[2];
        PredC = mbs[m_cur.adr].DcCoeff[0];
        break;
    case 2:
        PredA = mbs[m_cur.adr].DcCoeff[0];
        if(C) PredB = mbs[m_cur.adr-1].DcCoeff[1];
        if(C) PredC = mbs[m_cur.adr-1].DcCoeff[3];
        break;
    case 3:
        PredA = mbs[m_cur.adr].DcCoeff[1];
        PredB = mbs[m_cur.adr].DcCoeff[0];
        PredC = mbs[m_cur.adr].DcCoeff[2];
        break;
    case 4:
    case 5:
        if(A) PredA = mbs[m_cur.adr-w].DcCoeff[BlkIdx];
        if(B) PredB = mbs[m_cur.adr-w-1].DcCoeff[BlkIdx];
        if(C) PredC = mbs[m_cur.adr-1].DcCoeff[BlkIdx];
        break;
    }

    Ipp32s pred;
    if(PredB==unav)PredB=0;
    if ((PredA!=unav) && (PredC!=unav)){
        if(abs(PredB-PredA)<=abs(PredB-PredC)) {
            pred=PredC;
        }else {
            pred=PredA;
        }
    }else if (PredA!=unav) {
        pred=PredA;
    }else if (PredC!=unav) {
        pred=PredC;
    }else {
        pred=0;
    }

    return pred;
       
}

MeMV MeVC1::GetChromaMV (MeMV mv)
{
    MeMV cmv;
    if(m_par->FastChroma) GetChromaMVFast(mv, &cmv);
    else GetChromaMV (mv, &cmv); 
    return cmv;
}

void MeVC1::GetChromaMV(MeMV LumaMV, MeMV* pChroma)
{
    static Ipp16s round[4]= {0,0,0,1};

    pChroma->x = (LumaMV.x + round[LumaMV.x&0x03])>>1;
    pChroma->y = (LumaMV.y + round[LumaMV.y&0x03])>>1;
}

void MeVC1::GetChromaMVFast(MeMV LumaMV, MeMV * pChroma)
{
    static Ipp16s round [4]= {0,0,0,1};
    static Ipp16s round1[2][2] = {
        {0, -1}, //sign = 0;
        {0,  1}  //sign = 1
    };

    pChroma->x = (LumaMV.x + round[LumaMV.x&0x03])>>1;
    pChroma->y = (LumaMV.y + round[LumaMV.y&0x03])>>1;
    pChroma->x = pChroma->x + round1[pChroma->x < 0][pChroma->x & 1];
    pChroma->y = pChroma->y + round1[pChroma->y < 0][pChroma->y & 1];
}

//*******************************************************************************************************
bool MeH264::EstimateSkip()
{
    if(!IsSkipEnabled())
        return false;

    //P pictures only
    if (ME_FrmFrw == m_par->pSrc->type)
    {
        SetReferenceFrame(frw,0);
        m_PredCalcH264.SetPredictionPSkip();
        m_cur.SkipCost[0] = EstimatePoint(ME_Mb16x16, m_par->PixelType, m_par->SkippedMetrics, m_cur.PredMV[frw][0][0]);
        m_cur.SkipType[0]= ME_MbFrwSkipped;
    }
    else if (ME_FrmBidir == m_par->pSrc->type)
    {
        //B
        SetReferenceFrame(bkw,0);
        m_PredCalcH264.SetPredictionSpatialDirectSkip16x16();

        if((m_par->SearchDirection == ME_ForwardSearch)&&
           (m_PredCalcH264.GetRestictedDirection() == 0))
        {
            m_cur.SkipCost[0] = EstimatePoint(ME_Mb16x16, m_par->PixelType, m_par->SkippedMetrics, m_cur.PredMV[bkw][0][0]);
            m_cur.SkipType[0]= ME_MbBkwSkipped;
        }
        else if ((m_par->SearchDirection == ME_ForwardSearch)&&
                  (m_PredCalcH264.GetRestictedDirection() == 1))

        {
            m_cur.SkipCost[0] = EstimatePointAverage(ME_Mb16x16, m_par->PixelType, m_par->SkippedMetrics, frw, 0, m_cur.PredMV[frw][0][0], bkw, 0, m_cur.PredMV[bkw][0][0]);
            m_cur.SkipType[0]= ME_MbBidirSkipped;
        }
    }
    return EarlyExitForSkip();
}



//*******************************************************************************************************
//1 feedback implementation
Ipp32s RegrFun::m_QP;

RegrFun::RegrFun(Ipp32s /*dx*/, Ipp32s N)
{

    if(N>NumOfRegInterval){
        printf("RegrFun::RegrFun N > NumOfRegInterval\n");
        abort();
    }

    // TODO: check this!
    //m_dx = dx;
    m_num=N;
    m_QP=0;

    for(Ipp32u i=0; i<MaxQpValue; i++){
        m_a[i][0] = -1; //it means that function is not initialized
    }
}


void RegrFun::ProcessFeedback(Ipp32s *x, Ipp32s *y, Ipp32s N)
{
    if(N==0)return;
    
    //compute regression history ver.2
    if(m_a[m_QP][0]<0){
        //regr is uninitialized. It is possible only during preset calculation.
        printf("Regr was not initialised!!!\n");

        //find out step
        Ipp32s xMax=0;
        for(Ipp32s i=0; i<N; i++) xMax = IPP_MAX(xMax,x[i]);
        m_dx[m_QP] = 1+xMax/m_num;

        //reset history
        for(Ipp32s i=0; i<m_num; i++){ //initialize
            m_ax[m_QP][i]=0;
            m_ay[m_QP][i]=0;
            m_an[m_QP][i]=0;
        }
    }else {
        //reduce history size
        Ipp64f size=0;
        for(int i=0; i<m_num; i++)
            size+= m_an[m_QP][i];

        // TODO: set TargetHistorySize from without
        Ipp64f reduce=0;
        //Ipp32s FrameSize=1350; //number of MB in frame
        //Ipp32s TargetHistorySize = 10*FrameSize; //number of MB in history
        reduce = (m_TargetHistorySize-N)/(size+0.01);
        //printf("reduce=%f",reduce);
        //printf("size=%8.1f reduce=%11.2f\n", size, reduce);
        if(reduce<1.0){
            //reduce
            for(Ipp32s i=0; i<m_num; i++){
                m_an[m_QP][i]*=reduce;
            }
        }
        //unaverage :-)
        for(Ipp32s i=0; i<m_num; i++){
            m_ax[m_QP][i]*=m_an[m_QP][i];
            m_ay[m_QP][i]*=m_an[m_QP][i];
        }
    }


    for(Ipp32s i=0; i<N; i++){ //sum
        Ipp32s k=x[i]/m_dx[m_QP];
        if(k<0)k=0;
        if(k>=m_num) k= m_num-1;
        m_ax[m_QP][k]+=x[i];
        m_ay[m_QP][k]+=y[i];
        m_an[m_QP][k]+=1;
    }


    m_FirstData=m_num;
    m_LastData=0;
    for(Ipp32s i=0; i<m_num; i++){ //avrg
        if(m_an[m_QP][i]!=0){
            m_ax[m_QP][i]/=m_an[m_QP][i];
            m_ay[m_QP][i]/=m_an[m_QP][i];
            if(i<m_FirstData)m_FirstData=i;
            if(i>m_LastData)m_LastData=i;
        }
        //printf("[%2d] n=%7.2f x=%7.2f y=%7.2f\n", i, m_an[m_QP][i], m_ax[m_QP][i], m_ay[m_QP][i]);
        //printf("%7.2f,%7.2f,%7.2f\n", m_ax[m_QP][i], m_ay[m_QP][i],m_an[m_QP][i]);
    }

    //process for all pieces
    for(Ipp32s i=0; i<m_num; i++)
        ComputeRegression(i, x, y, N);
    AfterComputation();
}


Ipp32s RegrFun::Weight(Ipp32s x)
{
    if(m_a[m_QP][0]<0){
        //printf("Error function is not initialized");
        return x; //function is not initialized
    }

    Ipp32s i=x/m_dx[m_QP];
    if(i<0)i=0;
    if(i>=m_num) i= m_num-1;

    Ipp32s y=(Ipp32s)(m_a[m_QP][i]*x + m_b[m_QP][i]);

    //printf("QP in RegrFun::Weight=%d dx=%d x=%d i=%d y=%d\n", m_QP,m_dx[m_QP],x, i, y);

    
    if(y<0)return 0;
    if(y>ME_BIG_COST)return ME_BIG_COST;
    return y;
}

void RegrFun::LoadPresets(MeRegrFunSet set, MeRegrFun id)
{
    Ipp32s adr;
    Ipp32s *p = MeVc1PreciseRegrTable;
    Ipp32s size=MeVc1PreciseRegrTableSize;

    if(set == ME_RG_VC1_FAST_SET){
        p=MeVc1FastRegrTable;
        size=MeVc1FastRegrTableSize;
    }
    else if (set == ME_RG_AVS_FAST_SET){
        p=MeAVSFastRegrTable;
        size=MeAVSFastRegrTableSize;
    }

    Ipp32s StepForId = 5+m_num+1;
    for(Ipp32s qp=0; qp<MaxQpValue; qp++){
        //find preset
        for(adr=0;  (p[adr]!=id ||p[adr+1]!=qp) && adr<size; adr += StepForId);
        if(adr>=size) continue;

        //check preset
        Ipp32s sum = 0;
        for(Ipp32s i=1; i<StepForId-1; i++)
            sum+=p[adr+i];
        if(p[adr+4]!=m_num || p[adr+StepForId-1]!=sum){
            printf("wrong preset data! id=%d qp=%d\n", id, qp);
            continue;
        }

        //load preset
        m_dx[qp]=p[adr+2];
        Ipp32s dy=p[adr+3];
        Ipp32s yPrev = p[adr+5];
        for(Ipp32s i=0; i<m_num-1; i++){
            Ipp64f x0=i*m_dx[qp];
            Ipp64f x1=(i+1)*m_dx[qp];
            Ipp64f y0=yPrev;
            yPrev=yPrev+p[adr+5+i+1]+dy;
            Ipp64f y1=yPrev;
            m_a[qp][i] = (y0-y1)/(x0-x1);
            m_b[qp][i] = y0 - m_a[qp][i] * x0;
            m_ax[qp][i] = (x0+x1)/2;
            m_ay[qp][i] = (y0+y1)/2;
            m_an[qp][i] = 1; // TODO: check this value, should it be related to m_TargetHistorySize
        }
        //filll in last bin
        m_a[qp][m_num-1] = m_a[qp][m_num-2];
        m_b[qp][m_num-1] = m_b[qp][m_num-2];
        m_ax[qp][m_num-1] = 0;
        m_ay[qp][m_num-1] = 0;
        m_an[qp][m_num-1] = 0;
    }
}


Ipp64f LowessRegrFun::WeightPoint(Ipp64f x)
{
    //tricube function
    Ipp64f t=x<0?-x:x;
    if(t>=1.) return 0.;
    t = 1 - t*t*t;
    return t*t*t;
}


// regression based on averaged subset of input samples
void LowessRegrFun::ComputeRegression(Ipp32s I, Ipp32s* /*x*/, Ipp32s* /*y*/, Ipp32s /*N*/)
{
    Ipp64f SSxx=0., SSyy=0., SSxy=0;
    Ipp64f avx =0., avy=0., r2=0., a=0., b=0.;
    Ipp64f sw=0; //weight and sum of all weights
    Ipp64f x0= I*m_dx[m_QP];


    //check if this bin is inside data range
    if(I<m_FirstData){
        m_a[m_QP][I] = 0;
        m_b[m_QP][I] = 0;
        return;
    }else if(I>m_LastData){
        m_a[m_QP][I]=m_a[m_QP][I-1];
        m_b[m_QP][I]=m_b[m_QP][I-1];
        return;
    }


    //find current history size
    Ipp64f size=0;
    for(int i=0; i<m_num; i++)
        size+= m_an[m_QP][i];
    if(size<10){ // TODO: is 10 ok?
        m_a[m_QP][0]=-1;
        return;
    }

    //find adaptive bandwidth, that include at least "Bandwidth" percents of samples.
    Ipp64f bw = 0, sn=0;
    for(Ipp32s i=0; i<m_num; i++)
        for(Ipp32s pass=0; pass<2; pass++){
            Ipp32s j;
            if(pass==0)
                j= I+i;
            else
                j= I-i-1;

            if(sn>Bandwidth*size/100.){
                //printf("break\n");
                break;
            }

            if(0<=j && j<m_num){
                sn+=m_an[m_QP][j];
                bw = m_dx[m_QP]*abs(j-I);
                //printf("sn=%f bw=%f const=%d\n",sn,bw,Bandwidth);
            }
        }

    //check bandwidth, it should not be too narrow. 3 here means about 6 bins span
    if(bw<3*m_dx[m_QP])
        bw=3*m_dx[m_QP];

    //find lowess for one point
    //find average and weight
    Ipp64f snL=0, snR=0, skewness;
    for(Ipp32s i=0; i<m_num; i++){
        Ipp64f w = m_an[m_QP][i]*WeightPoint((m_ax[m_QP][i]-x0)/bw);
        sw += w;
        if(i<I)snL+=w;
        if(i>I)snR+=w;

        avx += w*m_ax[m_QP][i];
        avy += w*m_ay[m_QP][i];
    }
    skewness = (snL+0.00001)/(snR+0.00001);
    skewness = IPP_MAX(skewness,1/skewness);

    if(sw>5.){ //sw<5 is extrimly rare case
        //enough points to compute regression
        avx /= sw;
        avy /= sw;

        for(Ipp32s i=0; i<m_num; i++){
            Ipp64f w = m_an[m_QP][i]*WeightPoint((m_ax[m_QP][i]-x0)/bw);

            SSxx += w*(m_ax[m_QP][i] - avx)*(m_ax[m_QP][i] - avx);
            SSyy += w*(m_ay[m_QP][i] - avy)*(m_ay[m_QP][i] - avy);
            SSxy += w*(m_ay[m_QP][i] - avy)*(m_ax[m_QP][i] - avx);
        }

        SSxx /= sw;
        SSyy /= sw;
        SSxy /= sw;

        a=SSxy/SSxx;
        b=avy-a*avx;
        r2=SSxy*SSxy/(SSxx*SSyy);
    }
//    if(sw<=5. || a<0.){
    if(sw<=5.){
        //bad regression in this bin, just repeat previous segment
        if(I==0){
            a=0; b=0;
        }else{
            a=m_a[m_QP][I-1];
            b=m_b[m_QP][I-1];
        }
    }

    m_a[m_QP][I] = a;
    m_b[m_QP][I] = b;

    //printf("[%3d] n=%d sn=%7.2f bw=%7.2f  a=%7.2f b=%9.2f r2=%7.2f  sw=%9.2f SSxx=%9.2f sk=%7.2f\n", I, N, sn, bw, a, b, r2, sw, SSxx, skewness);

}

void LowessRegrFun::AfterComputation()
{
    //remove all decreasing segments from the begining of the sequence
    for(int i=0; i<m_num; i++){
        if(m_a[m_QP][i]==0)continue;
        if(m_a[m_QP][i]>0)break;
        //printf("%d %f %f was zeroved\n", i, m_a[m_QP][i],m_b[m_QP][i]);
        m_a[m_QP][i]=0; m_b[m_QP][i]=0;
    }

    //fill in first empty bins by value from first bin with data
    int FirstData=0; //it is not nessesary equal to m_FirstData!
    for(; FirstData<m_num && m_a[m_QP][FirstData]==0; FirstData++);

    for(Ipp32s i=0; i<FirstData; i++){
        m_a[m_QP][i] = m_a[m_QP][FirstData];
        m_b[m_QP][i] = m_b[m_QP][FirstData];
    }

    //save org coefficients
    Ipp64f a[NumOfRegInterval], b[NumOfRegInterval];
    for(Ipp32s i=0; i<m_num; i++){
        a[i]=m_a[m_QP][i];
        b[i]=m_b[m_QP][i];
    }

    //remove spikes on bins borders
    for(Ipp32s i=0; i<m_num; i++){
        Ipp64f x0, x1, y0, y1;
        x0 = i*m_dx[m_QP];
        x1 = (i+1)*m_dx[m_QP];

        y0=a[i]*x0+b[i];
        if(i!=0)
            y0 = (y0+a[i-1]*x0+b[i-1])/2;

        y1=a[i]*x1+b[i];
        if(i!=m_num-1)
            y1 = (y1+a[i+1]*x1+b[i+1])/2;

        //printf("y0=%f y1=%f\n", y0, y1);
        m_a[m_QP][i] = (y0-y1)/(x0-x1);
        m_b[m_QP][i] = y0 - m_a[m_QP][i] * x0;

        //all functions in ME are increasing, so check that a>0
        if(m_a[m_QP][i]<0){
            //printf("a<0 corrected!\n");
            if(i==0){
                m_a[m_QP][i]=0; m_b[m_QP][i]=0;
            }else{
                //repead previous bin with decreased slope
                m_a[m_QP][i] = 0.7*m_a[m_QP][i-1];
                m_b[m_QP][i] = m_b[m_QP][i-1]+(m_a[m_QP][i-1]-m_a[m_QP][i])*x0;
            }
            //correct org coefficient
            if(i!=m_num-1){
                a[i]=0;
                b[i]=2*(m_a[m_QP][i]*x1+m_b[m_QP][i])-a[i+1]*x1-b[i+1];
                ///printf("a[i]=%f b[i]=%f\n", a[i], b[i]);
            }
        }

        //printf("adj [%3d] a=%7.2f b=%9.2f \n", i, m_a[m_QP][i], m_b[m_QP][i]);
    }

}

Ipp32s MvRegrFun::Weight(MeMV mv, MeMV pred)
{

    Ipp32s x = abs(mv.x-pred.x) + abs(mv.y-pred.y);
    return RegrFun::Weight(x);

}


}
#endif

