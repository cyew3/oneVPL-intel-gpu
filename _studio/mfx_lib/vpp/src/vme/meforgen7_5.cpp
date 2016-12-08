//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2007-2016 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"
#include "ipps.h"
#include "meforgen7_5.h"

#pragma warning( disable : 4244 )
#pragma warning( disable : 4127 )
#pragma warning( disable : 4706 )

const char version[]="[VideoLib%$#@&*]=IntelGen75VME Version 1.00a -01.01.2010";

/*****************************************************************************************************\
Internal 16 bit Inter Mode Definition, which carries both Direction and Shape:
    2 bit direction = 1: forward, 2: backward, 3: bidirectional, 0: not a mv-top-left-corner.
    2 bit shape     = 1: two 8x4s, 2: two 4x8s, 3: four 4x4s, 0: not divided, i.e. 8x8 or larger.
Frame cases:
    Mode = 0xDDSS in hex     = ddddddddssssssss in binary, 
    mapped to 4 8x8 subblocks: 3322110033221100, 2 bits for each subblock for each direction and shape
    Therefore, for example:
        0x0100  = 16x16 forward
        0x0200  = 16x16 backward
        0x0300  = 16x16 bidirectional
        0x1200  = two 16x8s, top backward and bottom forward
        0x3300  = two 16x8s, both bidirectional
        0x0600  = two 8x16s, top backward and bottom forward
        0x0F00  = two 8x16s, both bidirectional
        0x5500  = four 8x8s, all forward
        0x7BAD  = 1st 8x8 subblock in 8x4 bidir, 2nd subblock in 4x4 backward, 3rd subblock in 4x8 bidir, 4th subblock in 4x8 forword
        0xFFFF  = all 4x4s, all bidirectional
Field cases:
    Since there are no minor shapes, we use the gap of 0x3301-0x54FF, and define
    Mode = 0x3FDD in hex    = 00111111dddddddd in binary,
    Therefore, for example:
        0x3F12  = two field blocks of 16x8, top field backward and bottom field forward
        0x3F33  = two field blocks of 16x8, both fields bidirectional
        0x3F55  = four field 8x8 blocks, all forward.

\*****************************************************************************************************/
MEforGen75::MEforGen75( )
/*****************************************************************************************************/
{
    memset(this, 0, sizeof(MEforGen75));

    RefPix[1] = (RefPix[0] = RefCache) + REFWINDOWWIDTH*REFWINDOWHEIGHT;
    RefPixCb[1] = (RefPixCb[0] = RefCacheCb) + REFWINDOWWIDTH*REFWINDOWHEIGHT;
    RefPixCr[1] = (RefPixCr[0] = RefCacheCr) + REFWINDOWWIDTH*REFWINDOWHEIGHT;
    MFX_INTERNAL_CPY(LutXY,version,16);//to figure out the version put a break point here, but value overwritten below
    memset(LutMode,0,12*sizeof(I16));
    memset(LutXY,0,66*sizeof(I16));
    memset(&Vsta,0,sizeof(mfxVME7_5UNIIn));
    memset(&Best, 0, 42*2*sizeof(I16PAIR));
    //memset(&Orig, 0, 42*2*sizeof(I16PAIR));
    memset(&BestForReducedMB, 0, 42*2*sizeof(I16PAIR));

    IntraSkipMask = 0;
    nullMV.x = 0;nullMV.y = 0;
    IsField = false;
    Pick8x8Minor = false;

    // Initialize the variables
    for (int i=0;i<256;i++) {
        SrcMB[i] = 0;
        SrcFieldMB[i] = 0;
    }


    Vsta.Ref[0].x = 0;
    Vsta.Ref[0].y = 0;
    Vsta.Ref[1].x = 0;
    Vsta.Ref[1].y = 0;
}
/*****************************************************************************************************/
MEforGen75::~MEforGen75( )
/*****************************************************************************************************/
{
}

Status MEforGen75::CheckVMEInput(int message_type)
{
    Status ret = 0;

    // reference width and height
    if (message_type == IME_MSG)
    {
        if (((Vsta.RefW % 4)!=0) && !(Vsta.SadType&INTER_CHROMA_MODE))
        {
            //printf("[%d, %d]: RefW must be a multiple of 4 for luma\n", Vsta.SrcMB.x, Vsta.SrcMB.y);
            return 1;
        }
        if (((Vsta.RefH % 4)!=0) && !(Vsta.SadType&INTER_CHROMA_MODE))
        {
            //printf("[%d, %d]: RefH must be a multiple of 4 for luma\n", Vsta.SrcMB.x, Vsta.SrcMB.y);
            return 1;
        }
        if (Vsta.SadType&INTER_CHROMA_MODE)
        {
            if (!((Vsta.RefW == 32 && Vsta.RefH == 20) || (Vsta.RefW == 24 && Vsta.RefH == 24) ||
                (Vsta.RefW == 16 && Vsta.RefH == 32) || (Vsta.RefW == 10 && Vsta.RefH == 20)))
            {
                /*printf("[%d, %d]: Chroma RefWindow size can only be 32x20, 24x24, 16x32 or 10x20\n", 
                    Vsta.SrcMB.x, Vsta.SrcMB.y);*/
                return 1;
            }
        }
        else if (Vsta.VmeModes&4) // dual reference
        {
            if ((Vsta.RefW > 32) || (Vsta.RefH > 32))
            {
                //printf("[%d, %d]: For dual ref, RefWindow must be subsets of 32x32\n", Vsta.SrcMB.x, Vsta.SrcMB.y);
                return 1;
            }
        }
        else // single reference
        {
            // accepts subsets of 64x32 and 32x64
            if (!((Vsta.RefW <= 64 && Vsta.RefH <= 32) || (Vsta.RefW <= 32 && Vsta.RefH <= 64)))
            {
                if (!((Vsta.RefW == 48 && Vsta.RefH == 40) || (Vsta.RefW == 44 && Vsta.RefH == 40) ||
                    (Vsta.RefW == 40 && Vsta.RefH == 40) || (Vsta.RefW == 36 && Vsta.RefH == 40) ||
                    (Vsta.RefW == 48 && Vsta.RefH == 36) || (Vsta.RefW == 44 && Vsta.RefH == 36) ||
                    (Vsta.RefW == 40 && Vsta.RefH == 36) || (Vsta.RefW == 36 && Vsta.RefH == 36)))
                {
                    /*printf("[%d, %d]: single ref accepts subsets of 64x32, 32x64, or any of the following:\n \
                        \t48x40, 44x40, 40x40, 36x40, 48x36, 44x36, 40x36, 36x36\n",
                        Vsta.SrcMB.x, Vsta.SrcMB.y);
                    */
                    return 1;
                }
            }
        }
    }

    // SubMbPartMask
    if (message_type == IME_MSG)
    {
        if ((Vsta.ShapeMask&0x7f) == 0x7f)
        {
            //printf("[%d, %d]: SubMbPartMask cannot have all shapes disabled\n", Vsta.SrcMB.x, Vsta.SrcMB.y);
            return 1;
        }
    }

    // IntraSAD
    if (message_type == SIC_MSG)
    {
        if (((Vsta.SadType>>6)&1) == 1)
        {
            //printf("[%d, %d]: IntraSAD only accepts none(0) or HAAR(2)\n", Vsta.SrcMB.x, Vsta.SrcMB.y);
            return 1;
        }
    }

    // InterSAD
    if (((Vsta.SadType>>4)&1) == 1)
    {
        //printf("[%d, %d]: InterSAD only accepts none(0) or HAAR(2)\n", Vsta.SrcMB.x, Vsta.SrcMB.y);
        return 1;
    }
    else if ((message_type == IME_MSG) && (Vsta.SadType&INTER_CHROMA_MODE) && (((Vsta.SadType>>4)&3) == 2))
    {
        //printf("[%d, %d]: InterSAD cannot be HAAR(2) in chroma IME mode\n", Vsta.SrcMB.x, Vsta.SrcMB.y);
        return 1;
    }

    // BMEDisableFBR
    if (message_type == FBR_MSG)
    {
        if ((Vsta.VmeFlags&VF_BIMIX_DISABLE) && (Vsta.FBRSubMbShape!=0) && ((Vsta.SadType&DISABLE_BME)==0) && 
            !(Vsta.SadType&INTER_CHROMA_MODE))
        {
            //printf("[%d, %d]: combination (Luma BiMixDisable and BMEEnable with minors) not acceptable\n", Vsta.SrcMB.x, Vsta.SrcMB.y);
            return 1;
        }
    }

    // InterChromaMode
    if (message_type == SIC_MSG)
    {
        if ((VSICsta.IntraComputeType!=INTRA_DISABLED) && (Vsta.SadType&INTER_CHROMA_MODE) && (Vsta.VmeFlags&VF_PB_SKIP_ENABLE))
        {
            //printf("[%d, %d]: cannot have intra enabled when doing chroma skip check\n", Vsta.SrcMB.x, Vsta.SrcMB.y);
            return 1;
        }
    }

    // SrcSize
    if (message_type == SIC_MSG)
    {
        if ((VSICsta.IntraComputeType!=INTRA_DISABLED) && (Vsta.SrcType&3))
        {
            //printf("[%d, %d]: cannot have intra enabled for reduced src size\n", Vsta.SrcMB.x, Vsta.SrcMB.y);
            return 1;
        }
    }
    if (!((message_type == SIC_MSG) && (VSICsta.IntraComputeType!=INTRA_DISABLED) && 
        !(Vsta.VmeFlags&VF_PB_SKIP_ENABLE)))
    {
        int width = 16, height = 16;
        if (Vsta.SrcType&ST_SRC_16X8) height = 8;
        if (Vsta.SrcType&ST_SRC_8X16) width = 8;
        if (Vsta.SadType&INTER_CHROMA_MODE)
        {
            width = 8;
            height = 8;
        }
        int chromamode = (Vsta.SadType&INTER_CHROMA_MODE) ? 1 : 0;
        int fieldmode = (Vsta.SrcType&ST_REF_FIELD) ? 1 : 0;
        if (((Vsta.SrcMB.x + width) > (Vsta.RefWidth>>chromamode)) ||
            ((Vsta.SrcMB.y + height) > ((Vsta.RefHeight>>chromamode)>>fieldmode)))
        {
            //printf("[%d, %d]: source pixels need to be within surf boundary\n", Vsta.SrcMB.x, Vsta.SrcMB.y);
            return 1;
        }
    }

    // BilinearEnable
    if ((message_type == FBR_MSG) ||
        ((message_type == SIC_MSG) && (Vsta.VmeFlags&VF_PB_SKIP_ENABLE)))
    {
        if ((!(Vsta.MvFlags&INTER_BILINEAR)) && (Vsta.SadType&INTER_CHROMA_MODE))
        {
            Vsta.MvFlags |= INTER_BILINEAR; //turn on bilinear for inter chroma
        }
    }

    // SubpelMode
    if (message_type == FBR_MSG)
    {
        if (!(Vsta.VmeModes&VM_QUARTER_PEL) && !(Vsta.VmeModes&VM_HALF_PEL) && (Vsta.SadType&DISABLE_BME))
        {
            //printf("[%d, %d]: FBR cannot disable hpel, qpel and bme all together\n", Vsta.SrcMB.x, Vsta.SrcMB.y);
            return 1;
        }
    }

    // SkipCenterEnables

    return ret;
}

/*****************************************************************************************************/
Status MEforGen75::LoadIntraSkipMask( )
/*****************************************************************************************************/
{
    IntraSkipMask = (VSICsta.IntraModeMask<<24) | (VSICsta.Intra8x8ModeMask<<12) | VSICsta.Intra4x4ModeMask;
    return 0;
}

/*****************************************************************************************************/
Status MEforGen75::RunIME(mfxVME7_5UNIIn *uni, mfxVME7_5IMEIn *ime, mfxVME7_5UNIOutput *uni_out, mfxVME7_5IMEOutput *ime_out)
/*****************************************************************************************************/
{
    Status status;
    MFX_INTERNAL_CPY(&Vsta,uni,sizeof(mfxVME7_5UNIIn));
    MFX_INTERNAL_CPY(&VIMEsta,ime,sizeof(mfxVME7_5IMEIn));
    if ((status = CheckVMEInput(IME_MSG)))
    {
        return status;
    }
    memset(Best, 0, 42*2*sizeof(I16PAIR));
    memset(MultiReplaced, 0, 20*sizeof(U8));
    if(Vsta.SadType&INTER_CHROMA_MODE){if((status = RunIMEChromaPre())) return status;}
    status = RunIME(uni_out, ime_out);
    if(Vsta.SadType&INTER_CHROMA_MODE){if((status = RunIMEChromaPost())) return status;}
    
#ifdef VMEPERF
    IMETime += uni_out->ClockCompute;
#endif
    return status;
}

/*****************************************************************************************************/
Status MEforGen75::RunIMEChromaPre()
/*****************************************************************************************************/
{
    /*
    Assumptions:
    OLD: RefLuma[] needs to be UVUV interleaved.  
    OLD: SrcLuma needs to be UVUV interleaved.
    NEW: RefCb[] and RefCr[] are planar U and planar V.
    NEW: SrcCb and SrcCr are planar U and planar V.
    Search Units are now 2x4 UV pairs.
    IME will only compute even integer locations to ensure |Uref-Usrc| and |Vref-Vsrc|.
    Ref window sizes (4): 32x20, 24x24, 16x32 (Dual or Single), 10x20 (Dual or Single).
    SrcType will be 8x8 (which HW interprets as 16x8).
    Only legal shape types for output will be 8x8UV or 4x4UV (HW sees as 16x8 or 8x8).

    MbType must be 8x8.  SubMBMode will denote 8x8 or 4x4.
    SubPredMode must be mapped to the corresponding major 8x8 blocks.
    MVs need to be mapped to corresponding major blocks (8x8 => 16x16, 4x4 => 8x8).
    Streamin\streamout also need to be remapped.


    HAS Rev1.05 Notes:
    -    All of these fields will be in context of the chroma Cb or Cr surface (e.g. nothing in relation to luma or CbCr pairs).
    -    This requires the HW to double the x integer component of the source, reference window location, skip center, and cost center [5/5/10: cost center should be unchanged] for working with data on NV12 surfaces.
    -    Regarding the reference window height, the user can now select multiples of 2 instead of multiples of 4.  However, the HW might overfetch what is required to simplify the design to match the luma processing mode (RefH will end up snapping to nearest larger multiple of 4 and not be lesser than 20 tall).
    -    The skip center and cost center will be S12.3 values.  The LSB, which denotes 8th pel precision, will be ignored by the HW (only quarter pel precision is supported) for both x and y components.
    -    The return MVs will also be S12.3 values.  Since the internal HW works on CbCr pairs instead of Y values in this mode, the integer x component of the motion vectors will need to be divided by 2 to denote the Cb motion vector.  Also, both the fractional x and y components will need to be multiplied by 2 to artificially produce the 3b fractional precision result.
    */

    if(!((Vsta.RefW == 32 && Vsta.RefH == 20) || (Vsta.RefW == 24 && Vsta.RefH == 24) ||
         (Vsta.RefW == 16 && Vsta.RefH == 32) || (Vsta.RefW == 10 && Vsta.RefH == 20))) return ERR_UNSUPPORTED;
    //Vsta.RefW <<= 1; // Double width.
    //Vsta.SrcType = 1; // 16x8
    Vsta.SrcType = Vsta.SrcType | 3; // chroma always 8x8,SrcType contains field flag.
    Vsta.SrcMB.x <<= 1;
    Vsta.SrcMB.y <<= 1;
    Vsta.MaxMvs <<= 2;
    //Vsta.Ref[0].x <<= 1;
    //Vsta.Ref[1].x <<= 1;
    //Vsta.CostCenter[0].x <<= 1;
    //Vsta.CostCenter[1].x <<= 1;
    //Vsta.ShapeMask |= (SHP_NO_16X16|SHP_NO_8X16|SHP_NO_8X8|SHP_NO_4X8|SHP_NO_4X4);
    Vsta.ShapeMask |= (SHP_NO_16X16|SHP_NO_16X8|SHP_NO_8X16|SHP_NO_8X4|SHP_NO_4X8);
    Vsta.VmeFlags &= 0xFD; // No adaptive
    Vsta.ModeCost[LUTMODE_INTER_BWD] = 0;
    return 0;
}

/*****************************************************************************************************/
Status MEforGen75::RunIME(mfxVME7_5UNIOutput *uni_out, mfxVME7_5IMEOutput *ime_out)
/*****************************************************************************************************/
{
    int        i, e;

    Vout = uni_out;
    VIMEout = ime_out;

    if(VIMEsta.Multicall&STREAM_IN)
    {
        for(i=0;i<2;i++){
            VIMEout->RecordDst16[i] = VIMEsta.RecordDst16[i];
            VIMEout->RecordMvs16[i] = VIMEsta.RecordMvs16[i];
            VIMEout->RecordRefId16[i] = VIMEsta.RecRefID16[i];
        for(int j=0;j<8;j++){
            VIMEout->RecordDst[i][j] = VIMEsta.RecordDst[i][j];
            VIMEout->RecordMvs[i][j] = VIMEsta.RecordMvs[i][j];
            if(j<4)
                VIMEout->RecordRefId[i][j] = VIMEsta.RecRefID[i][j];
        }}
    }
    Vout->ImeStop_MaxMV_AltNumSU = 0;

    IsBottom[0] = IsBottom[1] = IsField = false;
    MBMaxDist = MBMAXDIST;
    if (Vsta.SrcType&1) {MBMaxDist = (MBMaxDist<<1);}
    if (Vsta.SrcType&2) {MBMaxDist = (MBMaxDist<<1);}

    if((Vsta.SrcType&1) || (Vsta.SrcType&2)) // source type is not 16x16 then ignore MaxMv i.e. make MaxMv = maximum allowed MV(32)
    {
        Vsta.MaxMvs = 32;
    }


    LoadSrcMB( );
    Vout->DistInter = MBMAXDIST;
    DistInter = MBMaxDist;
    
    EarlyImeStop   = (Vsta.EarlyImeStop  &15)<<(Vsta.EarlyImeStop  >>4);
    EarlyImeStop  &= MAXEARLYTHRESH; // JMHTODO: Is threshold same as SNB\IVB?

    if((e = SetupLutCosts( ))) return e;
    if(Vsta.SrcType&3){ // reduced macroblocks
        //Vsta.DoIntraInter &= 0xFFFD; // no intra;
        Vsta.SrcType &= (~(ST_FIELD_16X8|ST_FIELD_8X8)); // no field modes
        Vsta.ShapeMask |= SHP_NO_16X16;
        if ((Vsta.SrcType&1) && (Vsta.SrcType&2)){
            // 8x8 always have 4 MVP
            //if(Vsta.VmeFlags&VF_PB_SKIP_ENABLE) Vsta.VmeModes |= VM_SKIP_8X8;
        }
        if(Vsta.SrcType&1){ // 16x8 or 8x8
            Vsta.ShapeMask |= SHP_NO_8X16; 
        }
        if(Vsta.SrcType&2){ // 8x16 or 8x8
            Vsta.ShapeMask |= SHP_NO_16X8; 
        }
    }

    OrigShapeMask = Vsta.ShapeMask;
    ShapeValidMVCheck[1] = (!(Vsta.ShapeMask&SHP_NO_16X8)) ? 1 : 0;
    ShapeValidMVCheck[2] = (!(Vsta.ShapeMask&SHP_NO_8X16)) ? 1 : 0;
    ShapeValidMVCheck[3] = ((Vsta.ShapeMask&0x70)!=0x70) ? 1 : 0;

    switch(Vsta.MaxMvs){
        case 0:
            return ERR_ILLEGAL;  // JMHTODO: This should be illegal... what is IME to do with no MVs?
        break;
        case 1:
            Vsta.ShapeMask |= (SHP_NO_16X8|SHP_NO_8X16|SHP_NO_8X8|SHP_NO_8X4|SHP_NO_4X8|SHP_NO_4X4);
        break;
        case 2: case 3:
            Vsta.ShapeMask |= (SHP_NO_8X8|SHP_NO_8X4|SHP_NO_4X8|SHP_NO_4X4);
        break;
    }

    if(Vsta.SrcType&ST_REF_FIELD)
    {
        IsField = true;    
        if(Vsta.BlockRefId[0]&1)
            IsBottom[0] = true;
    }

    if((e = SetupSearchPath( ))) return e;

    // get correct ref ids for fetching
    // IME only uses 1st blk
    for(int i = 0; i < 10; i++){
        BestRef[0][i] = Vsta.BlockRefId[0]&0xf; // foward, 1st blk
        BestRef[1][i] = Vsta.BlockRefId[0]>>4; // backward, 1st blk
    }

    if(!(Vsta.SadType&INTER_CHROMA_MODE)){
        LoadReference(Vsta.RefLuma[0][BestRef[0][0]], RefPix[0], Vsta.RefW, Vsta.RefH, Vsta.Ref[0], REFWINDOWWIDTH, IsBottom[0]);
    }
    else{
        LoadReference(Vsta.RefCb[0][BestRef[0][0]], RefPixCb[0], Vsta.RefW, Vsta.RefH, Vsta.Ref[0], REFWINDOWWIDTH, IsBottom[0]);
        LoadReference(Vsta.RefCr[0][BestRef[0][0]], RefPixCr[0], Vsta.RefW, Vsta.RefH, Vsta.Ref[0], REFWINDOWWIDTH, IsBottom[0]);
    }
    
    if(Vsta.VmeModes&4){
        if((IsField) &&((Vsta.BlockRefId[0]>>4)&1))
            IsBottom[1] = true;

        if(!(Vsta.SadType&INTER_CHROMA_MODE)){
            LoadReference(Vsta.RefLuma[1][BestRef[1][0]], RefPix[1], Vsta.RefW, Vsta.RefH, Vsta.Ref[1], REFWINDOWWIDTH, IsBottom[1]);
        }
        else{
            LoadReference(Vsta.RefCb[1][BestRef[1][0]], RefPixCb[1], Vsta.RefW, Vsta.RefH, Vsta.Ref[1], REFWINDOWWIDTH, IsBottom[1]);
            LoadReference(Vsta.RefCr[1][BestRef[1][0]], RefPixCr[1], Vsta.RefW, Vsta.RefH, Vsta.Ref[1], REFWINDOWWIDTH, IsBottom[1]);
        }
    }

    SadMethod = ((Vsta.SadType>>4)&3); 

    Vout->MbSubShape = 0;
    Vout->MbSubPredMode = 0;
    SkipMask = 0;

    e = IntegerMESearchUnit( ); //INTEGER SEARCH
    if(e == 1){ 
        Vout->ImeStop_MaxMV_AltNumSU |= OCCURRED_IME_STOP; 
    }else if(e&0xF000) return e; //true error
    
    PartitionBeforeModeDecision();
    MajorModeDecision( );//disableAltMask, AltNumMVsRequired, disableAltCandidate);
    
    if(VIMEsta.Multicall&STREAM_IN){
        UpdateBest(); 
        ReplicateSadMVStreamIn();
        PartitionBeforeModeDecision();
        MajorModeDecision( ); //disableAltMask, AltNumMVsRequired, disableAltCandidate);
    }
    if(VIMEsta.Multicall&STREAM_OUT){ UpdateRecordOut(); }

    PreBMEPartition(&MajorDist[0],&MajorMode[0], true);

    DistInter = MajorDist[0];
    
    if((e = SummarizeFinalOutputIME( ))) return e;
    CleanRecordOut();

    if(Vsta.SrcType&1){ // 16x8 or 8x8
        Vout->NumPackedMv >>=1;
        if (!((Vsta.SadType & INTER_CHROMA_MODE) && (Vout->MbSubShape&3)== 3))
        Vout->MbSubPredMode &= 0xf;
        Vout->MbSubShape &= 0xf;
    }
    if(Vsta.SrcType&2){// 8x16 or 8x8
        Vout->NumPackedMv >>=1;
        if (!((Vsta.SadType & INTER_CHROMA_MODE) && (Vout->MbSubShape&3)== 3)) // chroma 4x4 8bits
        Vout->MbSubPredMode &= 0x3;
        Vout->MbSubShape &= 0x3;
    }

    // JMHTODO: Can this be dropped for IME?  Talk to Jason or Ning.
    if((Vsta.SrcType&3)==1){// 16x8 (convert skip MB type to MbType5Bits 16x8)
        if((Vout->MbMode&0x3)==0)
        {
            Vout->MbMode &= 0xfd;
            Vout->MbMode |= 0x01;
            if(Vout->InterMbType == 1)
            {
                Vout->InterMbType = 0x4; //BP_L0_L0_16x8
                Vout->NumPackedMv = 1;
            }
            else if(Vout->InterMbType == 2)
            {
                Vout->InterMbType = 0xa; //B_L1_L0_16x8
                Vout->NumPackedMv = 1;
            }
            else if(Vout->InterMbType == 3)
            {
                Vout->InterMbType = 0x10; //B_Bi_L0_16x8
                Vout->NumPackedMv = 2;
            }
        }
        if((Vout->MbMode&0x3)==1)
        {
            Vout->MbSubPredMode &= 0x3;
            Vout->InterMbType = 4 + 6*Vout->MbSubPredMode;
        }
    }

    if((!((Vout->InterMbType>>5)&0x1))&&(Vout->InterMbType != 0x16))
    {
        //MbType5Bits Remapping
        if(Vsta.SrcType&0x10)
        {
            Vout->InterMbType = (Vout->InterMbType<=3) ? 1 : ((Vout->InterMbType&0x1) ? 5 : 4);
        }else if(Vsta.SrcType&0x20)
        {
            Vout->InterMbType = (Vout->InterMbType<=3) ? 2 : ((Vout->InterMbType&0x1) ? 7 : 6);
        }
    }
    if((Vout->MbSubShape == 0) && ((Vsta.SrcType&3) == 0))//transform 8x8 is on only when src type is 16x16.
    {
        Vout->InterMbType |= Vsta.VmeFlags&VF_T8X8_FOR_INTER;
    }

    if(Vsta.SrcType&ST_SRC_FIELD)
    {
        Vout->InterMbType |= TYPE_IS_FIELD;
        if(Vsta.MvFlags & SRC_FIELD_POLARITY) Vout->MbMode |= MODE_BOTTOM_FIELD;
    }

//#ifdef VMEPERF
//    GetBEClockCount();
//#endif

    return 0;
}

/*****************************************************************************************************/
Status MEforGen75::RunIMEChromaPost()
/*****************************************************************************************************/
{
    // JMHTODO: Anything needed here?  After going to SrcType 8x8 assumption, shouldn't need anything here.
    return 0;
}

/*****************************************************************************************************/
Status MEforGen75::RunSIC(mfxVME7_5UNIIn *uni, mfxVME7_5SICIn *sic, mfxVME7_5UNIOutput *uni_out)
/*****************************************************************************************************/
{
    Status status;
    MFX_INTERNAL_CPY(&Vsta,uni,sizeof(mfxVME7_5UNIIn));
    MFX_INTERNAL_CPY(&VSICsta,sic,sizeof(mfxVME7_5SICIn));
    if ((status = CheckVMEInput(SIC_MSG)))
    {
        return status;
    }
    memset(Best, 0, 42*2*sizeof(I16PAIR));
    //memset(MultiReplaced, 0, 20*sizeof(U8));
    if((Vsta.SadType&INTER_CHROMA_MODE) && (VSICsta.IntraComputeType == INTRA_DISABLED))
    {
        if((status = RunSICChromaPre())) return status;
    }
    status = RunSIC(uni_out);
    
#ifdef VMEPERF
    SICTime += uni_out->ClockCompute;
#endif
    return status;
}

/*****************************************************************************************************/
Status MEforGen75::RunSICChromaPre()
/*****************************************************************************************************/
{
    /*
    Assumptions:
    SrcType will be 8x8 (which HW interprets as 16x8).
    Only legal shape types for output will be 8x8UV or 4x4UV (HW sees as 16x8 or 8x8).

    MbType must be 8x8.  SubMBMode will denote 8x8 or 4x4.
    SubPredMode must be mapped to the corresponding major 8x8 blocks.
    MVs need to be mapped to corresponding major blocks (8x8 => 16x16, 4x4 => 8x8).

    HAS Rev1.05 Notes:
    -    All of these fields will be in context of the chroma Cb or Cr surface (e.g. nothing in relation to luma or CbCr pairs).
    -    This requires the HW to double the x integer component of the source, reference window location, skip center, and cost center [5/5/10: cost center should be unchanged] for working with data on NV12 surfaces.
    -    Regarding the reference window height, the user can now select multiples of 2 instead of multiples of 4.  However, the HW might overfetch what is required to simplify the design to match the luma processing mode (RefH will end up snapping to nearest larger multiple of 4 and not be lesser than 20 tall).
    -    The skip center and cost center will be S12.3 values.  The LSB, which denotes 8th pel precision, will be ignored by the HW (only quarter pel precision is supported) for both x and y components.
    -    The return MVs will also be S12.3 values.  Since the internal HW works on CbCr pairs instead of Y values in this mode, the integer x component of the motion vectors will need to be divided by 2 to denote the Cb motion vector.  Also, both the fractional x and y components will need to be multiplied by 2 to artificially produce the 3b fractional precision result.
    */

    Vsta.SrcType |= 3; // 8x8
    Vsta.ModeCost[LUTMODE_INTER_BWD] = 0;
    Vsta.SrcMB.x <<= 1;
    Vsta.SrcMB.y <<= 1;

    // skip center format 12.3
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 2; j++)
        {
            SkipCenterOrigChr[i][j].x = VSICsta.SkipCenter[i][j].x;
            SkipCenterOrigChr[i][j].y = VSICsta.SkipCenter[i][j].y;
            VSICsta.SkipCenter[i][j].x >>= 1;
            VSICsta.SkipCenter[i][j].y >>= 1;
        }

    // more stuff...
    return 0;
}

/*****************************************************************************************************/
Status MEforGen75::RunSIC(mfxVME7_5UNIOutput *uni_out)
/*****************************************************************************************************/
{
    int        i, e;
    memset(&m_BBSChromaDistortion[0],0,sizeof(m_BBSChromaDistortion[0]));
    memset(&m_BBSChromaDistortion[1],0,sizeof(m_BBSChromaDistortion[1]));
    
    Vout = uni_out;
    //bool bAltPartitionValid = false;
    bDirectReplaced = false;
    //U8    disableAltMask = 0; 
    //U8  AltNumMVsRequired = 0;
    bSkipSbFailed = false;
    //bool disableAltCandidate = false;
    bSkipSbCheckMode = SKIP16X16;
    bool bBlockBasedSkipEn       = ((Vsta.SadType&BLOCK_BASED_SKIP) == BLOCK_BASED_SKIP);
    //bool SkipOnly = (Vsta.ShapeMask&0x7F) == 0x7F;
#ifdef VMETESTGEN
    Vsta.BidirMask |= ADAPTIVE_CTRL;
    Vout->Test.EarlyExitCriteria = NoEarlyExit;
    Vout->Test.QuitCriteria = NoQuitOnThreshold;
    Vout->Test.usedAdaptiveSU = 0;
    Vout->Test.DirectionSwitch = false;
    U8 currDirection = 0;
#endif
    
    OutSkipDist = 0;
    IntraSkipMask = 0;
    IsBottom[0] = IsBottom[1] = IsField = false;

    if(Vsta.SrcType&ST_REF_FIELD)
    {
        IsField = true;    
        if(Vsta.BlockRefId[0]&1)
            IsBottom[0] = true;
    }

    LoadSrcMB( );
    Vout->DistIntra = Vout->DistSkip = Vout->DistInter = MBMAXDIST;
    DistIntra4or8 = DistI16 = MBMAXDIST;
    DistInter = MBMAXDIST;
    EarlySkipSuc = MBMAXDIST;

    if((e = SetupLutCosts( ))) return e;
    if((Vsta.SrcType&3) && !(Vsta.SadType&INTER_CHROMA_MODE)){ // reduced macroblocks
        //Vsta.DoIntraInter &= 0xFFFD; // no intra;
        Vsta.SrcType &= (~(ST_FIELD_16X8|ST_FIELD_8X8)); // no field modes
        Vsta.ShapeMask |= SHP_NO_16X16;
        if ((Vsta.SrcType&1) && (Vsta.SrcType&2)){
            // 8x8 always have 4 MVP
            if(Vsta.VmeFlags&VF_PB_SKIP_ENABLE) Vsta.VmeModes |= VM_SKIP_8X8;
        }
        if(Vsta.SrcType&1){ // 16x8 or 8x8
            Vsta.ShapeMask |= SHP_NO_8X16; 
            VSICsta.SkipCenter[2][0] = VSICsta.SkipCenter[0][0];    VSICsta.SkipCenter[2][1] = VSICsta.SkipCenter[0][1];
            VSICsta.SkipCenter[3][0] = VSICsta.SkipCenter[1][0];    VSICsta.SkipCenter[3][1] = VSICsta.SkipCenter[1][1];
        }
        if(Vsta.SrcType&2){ // 8x16 or 8x8
            Vsta.ShapeMask |= SHP_NO_16X8; 
            VSICsta.SkipCenter[1][0] = VSICsta.SkipCenter[0][0];    VSICsta.SkipCenter[1][1] = VSICsta.SkipCenter[0][1];
            VSICsta.SkipCenter[3][0] = VSICsta.SkipCenter[2][0];    VSICsta.SkipCenter[3][1] = VSICsta.SkipCenter[2][1];
        }
    }

    SadMethod = ((Vsta.SadType>>4)&3); 

    Vout->MbSubShape = 0;
    Vout->MbSubPredMode = 0;
    SkipMask = 0;

    SubBlockMaxSad = 0; // HSW overloading: 16x16 dist OR max 4x4 dist OR max 8x8 dist

    memset(&RefID, 0, 8*sizeof(RefID[0][0]));
    for (int i = 0; i < 4; i++) // load RefIDs regardless of mode
    {
        RefID[0][i] = Vsta.BlockRefId[i]&0xF;
        RefID[1][i] = Vsta.BlockRefId[i]>>4;
    }

    if(Vsta.VmeFlags&VF_PB_SKIP_ENABLE)
    {
        if(CheckSkipUnit(Vsta.VmeModes&VM_SKIP_8X8)) //**
        {
            if(Vout->InterMbType == TYPE_INTER_8X8)
                Vout->MbMode = MODE_INTER_8X8;
            else
                Vout->MbMode = MODE_INTER_16X16; 
        
            // JMHTODO: Remove this\resolve this with summarizeSIC
            if(((!bBlockBasedSkipEn)&&(DistInter<=EarlySkipSuc))||
                ((bBlockBasedSkipEn)&&(!bSkipSbFailed))){
                Vout->Distortion[0] = DistInter;
                SkipMask |= 0x00C0;
            }
            else{
                SkipMask |= 0x0040;
                DistInter = Vout->Distortion[0] = SkipDistMb;
            }
        }
    }

    if(!(VSICsta.IntraComputeType&INTRA_DISABLED)){
        LoadIntraSkipMask();
        SadMethod = ((Vsta.SadType>>6)&3); 
        if(Vsta.IntraFlags&INTRA_TBSWAP_FLAG){ 
            i = VSICsta.LeftPels[15]; 
            VSICsta.LeftPels[15] = VSICsta.TopMinus2Pels[1]; 
            VSICsta.TopMinus2Pels[1] = i;
            Vsta.IntraFlags &= 0x7F;
        }
        //Vout->VmeDecisionLog |= PERFORM_INTRA;
        if(!(Vsta.IntraFlags&INTRA_NO_16X16)) Intra16x16SearchUnit( );
        if(!(Vsta.IntraFlags&INTRA_NO_8X8)) Intra8x8SearchUnit( );
        if(!(Vsta.IntraFlags&INTRA_NO_4X4)) Intra4x4SearchUnit( );
        if(VSICsta.IntraComputeType==INTRA_LUMA_CHROMA) ChromaPredMode = Intra8x8ChromaSearchUnit( );
    }
    
    if((e = SummarizeFinalOutputSIC( ))) return e;
    //CleanRecordOut();

    if(!(Vsta.VmeFlags&VF_PB_SKIP_ENABLE))
    {
        //Vout->DistSkip |= SKIP_DIST_INVALID; // JMHTODO: open, is Skip Dist Invalid gone for Gen7.5?
    }

    if(!(VSICsta.IntraComputeType&INTRA_DISABLED))
    {
        Vout->DistIntra  = BestIntraDist;
    }
    else
    {
        Vout->DistIntra  = 0;
    }

    if((Vsta.SrcType&1) && !(Vsta.SadType&INTER_CHROMA_MODE)){ // 16x8 or 8x8
        Vout->MbSubPredMode &= 0xf;
        Vout->MbSubShape &= 0xf;
    }
    if((Vsta.SrcType&2) && !(Vsta.SadType&INTER_CHROMA_MODE)){// 8x16 or 8x8
        Vout->MbSubPredMode &= 0x3;
        Vout->MbSubShape &= 0x3;
    }

    if((Vsta.SrcType&3)==1){// 16x8 (convert skip MB type to MbType5Bits 16x8)
        if((Vout->MbMode&0x3)==0)
        {
            Vout->MbMode &= 0xfd;
            Vout->MbMode |= 0x01;
            if(Vout->InterMbType == 1)
            {
                Vout->InterMbType = 0x4; //BP_L0_L0_16x8
            }
            else if(Vout->InterMbType == 2)
            {
                Vout->InterMbType = 0xa; //B_L1_L0_16x8
            }
            else if(Vout->InterMbType == 3)
            {
                Vout->InterMbType = 0x10; //B_Bi_L0_16x8
            }
        }
        if((Vout->MbMode&0x3)==1)
        {
            Vout->MbSubPredMode &= 0x3;
            Vout->InterMbType = 4 + 6*Vout->MbSubPredMode;
        }
    }

    // no InterMbType remap for Sic msg. Vout->InterMbType has upper bits Intra and lower 4 bits are inter mb type

    if(VSICsta.IntraComputeType&INTRA_DISABLED)
    {
        if(Vout->MbSubShape == 0)
        {
            Vout->InterMbType |= Vsta.VmeFlags&VF_T8X8_FOR_INTER;
        }
    }

    if(Vsta.SrcType&ST_SRC_FIELD)
    {
        Vout->InterMbType |= TYPE_IS_FIELD;
        if(Vsta.MvFlags & SRC_FIELD_POLARITY) Vout->MbMode |= MODE_BOTTOM_FIELD;
    }

    return 0;
}

/*****************************************************************************************************/
Status MEforGen75::RunFBR(mfxVME7_5UNIIn *uni, mfxVME7_5FBRIn *fbr, mfxVME7_5UNIOutput *uni_out)
/*****************************************************************************************************/
{
    Status status;
    MFX_INTERNAL_CPY(&Vsta,uni,sizeof(mfxVME7_5UNIIn));
    MFX_INTERNAL_CPY(&VFBRsta,fbr,sizeof(mfxVME7_5FBRIn));
    if ((status = CheckVMEInput(FBR_MSG)))
    {
        return status;
    }
    memset(Best, 0, 42*2*sizeof(I16PAIR));
    Vsta.FBRMbInfo &= 0x3;
    //memset(MultiReplaced, 0, 20*sizeof(U8));
    if(Vsta.SadType&INTER_CHROMA_MODE){if((status = RunFBRChromaPre())) return status;}
    status = RunFBR(uni_out);
    
#ifdef VMEPERF
    FBRTime += uni_out->ClockCompute;
#endif
    return status;
}

/*****************************************************************************************************/
Status MEforGen75::RunFBRChromaPre()
/*****************************************************************************************************/
{
    /*
    Assumptions:
    SrcType will be 8x8 (which HW interprets as 16x8).
    Only legal shape types for output will be 8x8UV or 4x4UV (HW sees as 16x8 or 8x8).

    MbType must be 8x8.  SubMBMode will denote 8x8 or 4x4.
    SubPredMode must be mapped to the corresponding major 8x8 blocks.
    MVs need to be mapped to corresponding major blocks (8x8 => 16x16, 4x4 => 8x8).

    HAS Rev1.05 Notes:
    -    All of these fields will be in context of the chroma Cb or Cr surface (e.g. nothing in relation to luma or CbCr pairs).
    -    This requires the HW to double the x integer component of the source, reference window location, skip center, and cost center [5/5/10: cost center should be unchanged] for working with data on NV12 surfaces.
    -    Regarding the reference window height, the user can now select multiples of 2 instead of multiples of 4.  However, the HW might overfetch what is required to simplify the design to match the luma processing mode (RefH will end up snapping to nearest larger multiple of 4 and not be lesser than 20 tall).
    -    The skip center and cost center will be S12.3 values.  The LSB, which denotes 8th pel precision, will be ignored by the HW (only quarter pel precision is supported) for both x and y components.
    -    The return MVs will also be S12.3 values.  Since the internal HW works on CbCr pairs instead of Y values in this mode, the integer x component of the motion vectors will need to be divided by 2 to denote the Cb motion vector.  Also, both the fractional x and y components will need to be multiplied by 2 to artificially produce the 3b fractional precision result.
    */

    Vsta.SrcType |= 3; // 8x8
    Vsta.ModeCost[LUTMODE_INTER_BWD] = 0;
    Vsta.FBRMbInfo = MODE_INTER_8X8;
    Vsta.SrcMB.x <<= 1;
    Vsta.SrcMB.y <<= 1;

    // more stuff...
    return 0;
}

/*****************************************************************************************************/
Status MEforGen75::RunFBR(mfxVME7_5UNIOutput *uni_out)
/*****************************************************************************************************/
{
    int        e;
    
    Vout = uni_out;
    
    Vout->ImeStop_MaxMV_AltNumSU = 0;
    Vout->NumPackedMv = 0;
    IsBottom[0] = IsBottom[1] = IsField = false;

    // Mask upper bits of each 8x8 subpredmode 
    // valid subpredmode is 0,1.  2,3 will be treated as 0,1
    Vsta.FBRSubPredMode = Vsta.FBRSubPredMode&0x55;

    LoadSrcMB( );
    Vout->DistIntra = Vout->DistSkip = Vout->DistInter = MBMAXDIST;
    DistIntra4or8 = DistI16 = MBMAXDIST;
    DistInter = MBMAXDIST;

    Vsta.VmeModes |= VM_MODE_DDD; //For FBR, SearchControl is a don't care; So always Dual Reference
    
    if((e = SetupLutCosts( ))) return e;
    if(Vsta.SrcType&3){ // reduced macroblocks
        //Vsta.DoIntraInter &= 0xFFFD; // no intra;
        Vsta.SrcType &= (~(ST_FIELD_16X8|ST_FIELD_8X8)); // no field modes
        Vsta.ShapeMask |= SHP_NO_16X16;
        if ((Vsta.SrcType&1) && (Vsta.SrcType&2)){
            // 8x8 always have 4 MVP
            //if(Vsta.VmeFlags&VF_PB_SKIP_ENABLE) Vsta.VmeModes |= VM_SKIP_8X8;
        }
        if(Vsta.SrcType&1){ // 16x8 or 8x8
            Vsta.ShapeMask |= SHP_NO_8X16; 
        }
        if(Vsta.SrcType&2){ // 8x16 or 8x8
            Vsta.ShapeMask |= SHP_NO_16X8; 
        }
    }

    OrigShapeMask = Vsta.ShapeMask;
    ShapeValidMVCheck[1] = (!(Vsta.ShapeMask&SHP_NO_16X8)) ? 1 : 0;
    ShapeValidMVCheck[2] = (!(Vsta.ShapeMask&SHP_NO_8X16)) ? 1 : 0;
    ShapeValidMVCheck[3] = ((Vsta.ShapeMask&0x70)!=0x70) ? 1 : 0;

    //switch(Vsta.MaxMvs){
    //    case 0:
    //        // Vsta.DoIntraInter &= (~DO_INTER); // JMHTODO what do we do for MaxMV == 0 on FBR call?
    //    break;
    //    case 1:
    //        Vsta.ShapeMask |= (SHP_NO_16X8|SHP_NO_8X16|SHP_NO_8X8|SHP_NO_8X4|SHP_NO_4X8|SHP_NO_4X4);
    //        Vsta.BidirMask |= BID_NO_16X16;
    //    break;
    //    case 2: case 3:
    //        Vsta.ShapeMask |= (SHP_NO_8X8|SHP_NO_8X4|SHP_NO_4X8|SHP_NO_4X4);
    //        Vsta.BidirMask |= (BID_NO_16X8|BID_NO_8X16);
    //    break;
    //}

    if(Vsta.SrcType&ST_REF_FIELD)
        IsField = true;
    
    SadMethod = ((Vsta.SadType>>4)&3); 

    Vout->MbSubShape = Vsta.FBRSubMbShape;
    Vout->MbSubPredMode = Vsta.FBRSubPredMode;
    bool chroma_mode = ((Vsta.SadType&INTER_CHROMA_MODE)!=0);

    // Reset record
    memset(&Best, 0, 42*2*sizeof(I16PAIR));
    memset(&BestForReducedMB, 0, 42*2*sizeof(I16PAIR));
    for(int i=0;i<42;i++){ Dist[1][i] = Dist[0][i] = DistForReducedMB[1][i] = DistForReducedMB[0][i] = MBMAXDIST; }

    memset(&RefID, 0, 8*sizeof(RefID[0][0]));
    for (int i = 0; i < 4; i++) // load RefIDs regardless of mode
    {
        RefID[0][i] = Vsta.BlockRefId[i]&0xF;
        RefID[1][i] = Vsta.BlockRefId[i]>>4;
    }

    // Handling non integer MV
    // mask Luma   with 0xfffc (luma   is 13.2 format)
    // mask chroma with 0xfff8 (chroma is 12.3 format) 
    for(int i=0; i<16; i++){
        for(int j=0; j<2; j++){
            if(Vsta.SadType&INTER_CHROMA_MODE){ //Chroma
                VFBRsta.SubblockMVs[i][j].x &= 0xfff8;
                VFBRsta.SubblockMVs[i][j].y &= 0xfff8;
            }
            else{  //Luma
                VFBRsta.SubblockMVs[i][j].x &= 0xfffc;
                VFBRsta.SubblockMVs[i][j].y &= 0xfffc;
            }
        }
    }

    // Load MVs based on mode
    if(Vsta.FBRMbInfo == MODE_INTER_16X16){ //16x16
        Best[0][BHXH] = VFBRsta.SubblockMVs[0][0];
        Best[1][BHXH] = VFBRsta.SubblockMVs[0][1];
    }
    else if(Vsta.FBRMbInfo == MODE_INTER_16X8){ // 16x8
        Best[0][BHX8+0]    = VFBRsta.SubblockMVs[0][0];
        Best[0][BHX8+1] = VFBRsta.SubblockMVs[8][0];
        Best[1][BHX8+0]    = VFBRsta.SubblockMVs[0][1];
        Best[1][BHX8+1]    = VFBRsta.SubblockMVs[8][1];
    }
    else if(Vsta.FBRMbInfo == MODE_INTER_8X16){ // 8x16
        Best[0][B8XH+0]    = VFBRsta.SubblockMVs[0][0];
        Best[0][B8XH+1] = VFBRsta.SubblockMVs[4][0];
        Best[1][B8XH+0]    = VFBRsta.SubblockMVs[0][1];
        Best[1][B8XH+1]    = VFBRsta.SubblockMVs[4][1];
    }
    else
    {    // 8x8 and minors
        U8 m = Vsta.FBRSubMbShape;

        int loops = 4;
        if(Vsta.SrcType & 0x1){ loops >>= 1; }
        if(Vsta.SrcType & 0x2){ loops >>= 1; }

        for(int k=0;k<4;k++){
            if (k>=loops)
            {
                break;
            }
            switch(m&3){
            case 0: // 8x8
                if(chroma_mode)
                {
                    Best[0][B8X8+k].x = VFBRsta.SubblockMVs[(k<<2)][0].x>>1;
                    Best[0][B8X8+k].y = VFBRsta.SubblockMVs[(k<<2)][0].y>>1;
                    Best[1][B8X8+k].x = VFBRsta.SubblockMVs[(k<<2)][1].x>>1;
                    Best[1][B8X8+k].y = VFBRsta.SubblockMVs[(k<<2)][1].y>>1;
                }
                else
                {
                    Best[0][B8X8+k] = VFBRsta.SubblockMVs[(k<<2)][0];
                    Best[1][B8X8+k] = VFBRsta.SubblockMVs[(k<<2)][1];
                }
                break;
            case 1: // 8x4                
                Best[0][B8X4+(k<<1)+0] = VFBRsta.SubblockMVs[(k<<2)+0][0];
                Best[0][B8X4+(k<<1)+1] = VFBRsta.SubblockMVs[(k<<2)+2][0];
                Best[1][B8X4+(k<<1)+0] = VFBRsta.SubblockMVs[(k<<2)+0][1];
                Best[1][B8X4+(k<<1)+1] = VFBRsta.SubblockMVs[(k<<2)+2][1];
                break;
            case 2: // 4x8
                Best[0][B4X8+(k<<1)+0] = VFBRsta.SubblockMVs[(k<<2)+0][0];
                Best[0][B4X8+(k<<1)+1] = VFBRsta.SubblockMVs[(k<<2)+1][0];
                Best[1][B4X8+(k<<1)+0] = VFBRsta.SubblockMVs[(k<<2)+0][1];
                Best[1][B4X8+(k<<1)+1] = VFBRsta.SubblockMVs[(k<<2)+1][1];
                break;
            case 3: // 4x4
                if(chroma_mode){
                    Best[0][B4X4+(k<<2)+0].x = VFBRsta.SubblockMVs[0 ][0].x>>1;
                    Best[0][B4X4+(k<<2)+0].y = VFBRsta.SubblockMVs[0 ][0].y>>1;
                    Best[0][B4X4+(k<<2)+1].x = VFBRsta.SubblockMVs[4 ][0].x>>1;
                    Best[0][B4X4+(k<<2)+1].y = VFBRsta.SubblockMVs[4 ][0].y>>1;
                    Best[0][B4X4+(k<<2)+2].x = VFBRsta.SubblockMVs[8 ][0].x>>1;
                    Best[0][B4X4+(k<<2)+2].y = VFBRsta.SubblockMVs[8 ][0].y>>1;
                    Best[0][B4X4+(k<<2)+3].x = VFBRsta.SubblockMVs[12][0].x>>1;
                    Best[0][B4X4+(k<<2)+3].y = VFBRsta.SubblockMVs[12][0].y>>1;
                    Best[1][B4X4+(k<<2)+0].x = VFBRsta.SubblockMVs[0 ][1].x>>1;
                    Best[1][B4X4+(k<<2)+0].y = VFBRsta.SubblockMVs[0 ][1].y>>1;
                    Best[1][B4X4+(k<<2)+1].x = VFBRsta.SubblockMVs[4 ][1].x>>1;
                    Best[1][B4X4+(k<<2)+1].y = VFBRsta.SubblockMVs[4 ][1].y>>1;
                    Best[1][B4X4+(k<<2)+2].x = VFBRsta.SubblockMVs[8 ][1].x>>1;
                    Best[1][B4X4+(k<<2)+2].y = VFBRsta.SubblockMVs[8 ][1].y>>1;
                    Best[1][B4X4+(k<<2)+3].x = VFBRsta.SubblockMVs[12][1].x>>1;
                    Best[1][B4X4+(k<<2)+3].y = VFBRsta.SubblockMVs[12][1].y>>1;
                }else{
                    Best[0][B4X4+(k<<2)+0] = VFBRsta.SubblockMVs[(k<<2)+0][0];
                    Best[0][B4X4+(k<<2)+1] = VFBRsta.SubblockMVs[(k<<2)+1][0];
                    Best[0][B4X4+(k<<2)+2] = VFBRsta.SubblockMVs[(k<<2)+2][0];
                    Best[0][B4X4+(k<<2)+3] = VFBRsta.SubblockMVs[(k<<2)+3][0];
                    Best[1][B4X4+(k<<2)+0] = VFBRsta.SubblockMVs[(k<<2)+0][1];
                    Best[1][B4X4+(k<<2)+1] = VFBRsta.SubblockMVs[(k<<2)+1][1];
                    Best[1][B4X4+(k<<2)+2] = VFBRsta.SubblockMVs[(k<<2)+2][1];
                    Best[1][B4X4+(k<<2)+3] = VFBRsta.SubblockMVs[(k<<2)+3][1];
                }
                break;
            }
            m>>=2;
        }
    }

    FractionalMESearchUnit(Vsta.FBRMbInfo, Vsta.FBRSubMbShape, Vsta.FBRSubPredMode, 0, chroma_mode); // 0 == integer
    if(Vsta.VmeModes&VM_HALF_PEL)
    { 
        FractionalMESearchUnit(Vsta.FBRMbInfo, Vsta.FBRSubMbShape, Vsta.FBRSubPredMode, 2, chroma_mode); // 2 == half pel

        if(Vsta.VmeModes&(VM_QUARTER_PEL-VM_HALF_PEL)){
            FractionalMESearchUnit(Vsta.FBRMbInfo, Vsta.FBRSubMbShape, Vsta.FBRSubPredMode, 1, chroma_mode); // 1 == quarter pel
        }
    }

    MajorMode[0] = Vsta.FBRMbInfo;
    U8 SubPredMode[] = {Vsta.FBRSubPredMode};
    if(!(Vsta.SadType&INTER_CHROMA_MODE))
        e = BidirectionalMESearchUnit(&MajorDist[0],&SubPredMode[0]); // Check is done internally if BiDir is to run
    else
        e = BidirectionalMESearchUnitChroma(&MajorDist[0],&SubPredMode[0]); // Check is done internally if BiDir is to run
    
    MajorMode[0] = ((SubPredMode[0]<<8)|MajorMode[0]);

    if((e = SummarizeFinalOutputFBR( ))) return e;
    //CleanRecordOut();

    if(Vsta.SrcType&1){ // 16x8 or 8x8
        Vout->NumPackedMv >>=1;
        if (!((Vsta.SadType & INTER_CHROMA_MODE) && (Vout->MbSubShape&3)== 3)) // chroma 4x4 8bits
            Vout->MbSubPredMode &= 0xf;
        //Vout->MbSubShape &= 0xf;
    }
    if(Vsta.SrcType&2){// 8x16 or 8x8
        Vout->NumPackedMv >>=1;
        if (!((Vsta.SadType & INTER_CHROMA_MODE) && (Vout->MbSubShape&3)== 3)) // chroma 4x4 8bits
            Vout->MbSubPredMode &= 0x3;
        //Vout->MbSubShape &= 0x3;
    }

    if((Vsta.SrcType&3)==1){// 16x8 (convert skip MB type to MbType5Bits 16x8)
        if((Vout->MbMode&0x3)==0)
        {
            Vout->MbMode &= 0xfd;
            Vout->MbMode |= 0x01;
            if(Vout->InterMbType == 1)
            {
                Vout->InterMbType = 0x4; //BP_L0_L0_16x8
                Vout->NumPackedMv = 1;
            }
            else if(Vout->InterMbType == 2)
            {
                Vout->InterMbType = 0xa; //B_L1_L0_16x8
                Vout->NumPackedMv = 1;
            }
            else if(Vout->InterMbType == 3)
            {
                Vout->InterMbType = 0x10; //B_Bi_L0_16x8
                Vout->NumPackedMv = 2;
            }
        }
        if((Vout->MbMode&0x3)==1)
        {
            Vout->MbSubPredMode &= 0x3;
            Vout->InterMbType = 4 + 6*Vout->MbSubPredMode;
        }
    }

    if((!((Vout->InterMbType>>5)&0x1))&&(Vout->InterMbType != 0x16))
    {
        //MbType5Bits Remapping
        if(Vsta.SrcType&0x10)
        {
            Vout->InterMbType = (Vout->InterMbType<=3) ? 1 : ((Vout->InterMbType&0x1) ? 5 : 4);
        }else if(Vsta.SrcType&0x20)
        {
            Vout->InterMbType = (Vout->InterMbType<=3) ? 2 : ((Vout->InterMbType&0x1) ? 7 : 6);
        }
    }
    if(Vout->MbSubShape == 0)
    {
        Vout->InterMbType |= Vsta.VmeFlags&VF_T8X8_FOR_INTER;
    }

    if(Vsta.SrcType&ST_SRC_FIELD)
    {
        Vout->InterMbType |= TYPE_IS_FIELD;
        if(Vsta.MvFlags & SRC_FIELD_POLARITY) Vout->MbMode |= MODE_BOTTOM_FIELD;
    }

    return 0;
}

/*****************************************************************************************************/
int MEforGen75::SummarizeFinalOutputIME( )
/*****************************************************************************************************/
{
    const U8 MbType16x8[4][4] = {{4,8,12,0},{10,6,14,0},{16,18,20,0},{0,0,0,0}};
    const U8 MbType8x16[16]   = { 5,11,17,0,  9,7,19,0,  13,15,21,0,  0,0,0,0 };
    int        k, m, s;
    I16PAIR    *mv;


    //bool intraWinner = true;
    Vout->IntraAvail = Vsta.IntraAvail;
    Vout->NumPackedMv = 0;
    Vout->RefBorderMark = 0;
    //Vout->D8x8Pattern = 0;// JMHTODO: This was removed.  Investigate.
    MoreSU[1] = (Vsta.VmeModes&4) ? MoreSU[1] : Vsta.MaxNumSU;
    Vout->NumSUinIME = (Vsta.MaxNumSU - MoreSU[0]) + (Vsta.MaxNumSU - MoreSU[1]);
    
#ifdef VMEPERF    
    //Calculate backend compute clocks
    GetIMEComputeClkCount();
#endif

    int altNumSUinIME = Vout->ImeStop_MaxMV_AltNumSU>>2;
    int decisionLog = Vout->ImeStop_MaxMV_AltNumSU&0x3;
    altNumSUinIME -= AltMoreSU;
    Vout->ImeStop_MaxMV_AltNumSU = (altNumSUinIME<<2) | decisionLog;

    Vout->DistInter = 0;
    I32 SkipDistortion = MBMAXDIST;
    U8 SkipMode = Vout->MbMode;
    BestIntraDist = DistI16;
    U8 IntraMbMode = MODE_INTRA_16X16;
    bool bBlockBasedSkipEn = ((Vsta.SadType&BLOCK_BASED_SKIP) == BLOCK_BASED_SKIP);
    int replFactor = 1;
    //int finalMVCount = 4;
    int index = 0;    
    memset(Vout->Mvs, 0, 16*2*sizeof(I16PAIR));
    memset(Vout->Distortion, 0, 16*sizeof(U16));

    for(int i=0; i<4; i++){
        Vout->RefId[i] = Vsta.BlockRefId[0];
    }



    //if(!bBlockBasedSkipEn)
    //{
    //    if((Vout->MinDist > EarlySkipSuc)&&((Vsta.SrcType&0x3)!=0))
    //    {
    //        if(Vsta.ShapeMask != 0x7F) {Vout->MinDist = MBMAXDIST; SkipDistortion = MBMAXDIST;}
    //    }
    //}
    //else
    //{
    //    if((bSkipSbFailed)&&((Vsta.SrcType&0x3)!=0))
    //    {
    //        if(Vsta.ShapeMask != 0x7F){Vout->MinDist = MBMAXDIST; SkipDistortion = MBMAXDIST;}
    //    }
    //}

    if((DistIntra4or8&MBMAXDIST)<BestIntraDist){
        BestIntraDist = DistIntra4or8&MBMAXDIST; 
        IntraMbMode = (DistIntra4or8&0x10000)? MODE_INTRA_8X8 : MODE_INTRA_4X4;
    }

    //if((Vout->VmeDecisionLog&PERFORM_SKIP)&&(SkipDistortion<=BestIntraDist)) {Vout->MinDist = SkipDistortion; intraWinner = false;}
    //else {Vout->MbMode = IntraMbMode;  Vout->MinDist = 0;}


    // check if its skip and skip is the winner.
    if((SkipMask & 0x40)&&(SkipDistortion <= DistInter)) 
    {
        //survived or skip won
        Vout->DistInter = SkipDistortion;
        Vout->MbMode = SkipMode;
        if(SkipMask & 0x40) //survived or skip won
        {
            if(Vout->InterMbType == TYPE_INTER_8X8){
                //Vout->D8x8Pattern = ((SkipMask&15)<<4);// JMHTODO: This was removed.  Investigate.
                m = (SkipMask>>8);//-0x55;
                Vout->NumPackedMv = 4 + (!!(m&2)) + (!!(m&8)) + (!!(m&32)) + (!!(m&128));
            }
            else{
                //Vout->D8x8Pattern = 0xF0;// JMHTODO: This was removed.  Investigate.
                Vout->NumPackedMv = 1 + (Vout->InterMbType==3);
            }
            SetupSkipMV();
            if(!(true))//Vsta.MvFlags&EXTENDED32MV_FLAG)) // Flag removed on HSW
                Replicate8MVs(Vout->MbMode);
            if (((!bBlockBasedSkipEn)&&(SkipDistortion <= EarlySkipSuc))||((bBlockBasedSkipEn)&&(!bSkipSbFailed)))
            {
                //Vout->MbMode |= MODE_SKIP_FLAG;
            }
            else if(Vout->InterMbType != TYPE_INTER_8X8)
            {
                Vout->Distortion[0] = SkipDistAddMV16x16;
            }
            else if(Vout->InterMbType == TYPE_INTER_8X8)
            {
                for(U8 blkNum = 0; blkNum < 4; blkNum++)
                {
                    Vout->Distortion[4*blkNum] = SkipDistAddMV8x8[blkNum];
                }
            }
        }
        goto GOTO_DONE_IME_SUMMARIZE;
    }
    
    //reaching here means that skip is not the winner. the choice is between intra and inter.
    SkipMask &= 0xFF3F;
    
    //Vout->D8x8Pattern = bDirectReplaced ? ((SkipMask&15)<<4) : Vout->D8x8Pattern; //set here so that AssignMV can use it// JMHTODO: This was removed.  Investigate.
    m = MajorMode[0];
    if(m<0x0400){ // 16x16
        Vout->MbMode = MODE_INTER_16X16; 
        Vout->InterMbType = (m>>=8);
        Vout->MbSubPredMode = (((m-1)*0x55)&0x03);
        Vout->NumPackedMv = 1 + (m==3);
        Vout->Distortion[0] = Dist[0][BHXH]; 
        for (k = 0;k<4;k++)
            Vout->RefId[k] = (BestRef[1][BHXH]<<4)|BestRef[0][BHXH];
        replFactor = 16;    
        for(k=0;k<replFactor;k++){
            AssignMV((m&0x2) | ((~m)&0x1), k, BHXH);
        }
    }
    else if(m<0x1000){ // 8x16
        Vout->MbMode = MODE_INTER_8X16; 
        m = (m>>8)-5;
        Vout->InterMbType = MbType8x16[m];
        Vout->MbSubPredMode = (m&0x0F);
        Vout->NumPackedMv = 2 + (!!(m&2)) + (!!(m&8));
        Vout->Distortion[0] = Dist[0][B8XH]; 
        Vout->Distortion[4] = Dist[0][B8XH+1]; 
        Vout->RefId[0] = Vout->RefId[2]=(BestRef[1][B8XH  ]<<4)|BestRef[0][B8XH  ];
        Vout->RefId[1] = Vout->RefId[3]=(BestRef[1][B8XH+1]<<4)|BestRef[0][B8XH+1];
        if(true)//Vsta.MvFlags&EXTENDED32MV_FLAG) // Flag removed on HSW
        {
            for(index=0; index<4;index++)
            {
                AssignMV(m>>((index%2)*2), 4*index,      B8XH+index%2);
                AssignMV(m>>((index%2)*2), 4*index+1, B8XH+index%2);
                AssignMV(m>>((index%2)*2), 4*index+2, B8XH+index%2);
                AssignMV(m>>((index%2)*2), 4*index+3, B8XH+index%2);
            }
        }
        else
        {
        AssignMV(m, 0, B8XH);
        AssignMV(m, 2, B8XH);
        AssignMV(m>>2, 1, B8XH+1);
        AssignMV(m>>2, 3, B8XH+1);
                Replicate8MVs(MODE_INTER_8X16);
        }
    }
    else if(m<0x3400){ // 16x8
        Vout->MbMode = MODE_INTER_16X8; 
        m = (m>>8)-17;
        Vout->InterMbType = MbType16x8[m&3][m>>4];
//            Vout->MbSubPredMode = ((m*0x05)&0x0F);
        Vout->MbSubPredMode = (m&0x03)|(((m>>4)&0x03)<<2);
        Vout->NumPackedMv = 2 + (!!(m&2)) + (!!(m&32));
        Vout->Distortion[0] = Dist[0][BHX8]; 
        Vout->Distortion[8] = Dist[0][BHX8+1]; 
        Vout->RefId[0] = Vout->RefId[2]=(BestRef[1][BHX8  ]<<4)|BestRef[0][BHX8  ];
        Vout->RefId[1] = Vout->RefId[3]=(BestRef[1][BHX8+1]<<4)|BestRef[0][BHX8+1];
        if(true)//Vsta.MvFlags&EXTENDED32MV_FLAG) // Flag removed on HSW
        {
            for(index=0 ;index<16; index++)
            {
                AssignMV(m>>(4*(index/8)),index,BHX8+index/8);
            }
        }
        else
        {
            AssignMV(m, 0, BHX8);
            AssignMV(m, 1, BHX8);
            AssignMV(m>>4, 2, BHX8+1);
            AssignMV(m>>4, 3, BHX8+1);
            Replicate8MVs(MODE_INTER_16X8);
        }
    }
    else if(m<0x3F40){ // 16x8 Field
        Vout->MbMode = MODE_INTER_16X8_FIELD; 
        m = (m&0xFF)-17;
        Vout->InterMbType = MbType16x8[m&3][m>>4];
        Vout->MbSubPredMode = m*0x05;
        Vout->NumPackedMv = 2 + (!!(m&2)) + (!!(m&32));
        Vout->Distortion[0] = Dist[0][FHX8]; 
        Vout->Distortion[8] = Dist[0][FHX8+1]; 
        Vout->RefId[0] = Vout->RefId[2]=(BestRef[1][BHX8  ]<<4)|BestRef[0][BHX8  ];
        Vout->RefId[1] = Vout->RefId[3]=(BestRef[1][BHX8+1]<<4)|BestRef[0][BHX8+1];
        if(true)//Vsta.MvFlags&EXTENDED32MV_FLAG) // Flag removed on HSW
        {
            for(index=0; index<16; index++)
            {
                AssignMV(m>>(4*(index/8)),index,FHX8+index/8);
            }
        }
        else
        {
            AssignMV(m, 0, FHX8);
            AssignMV(m, 1, FHX8);
            AssignMV(m>>4, 2, FHX8+1);
            AssignMV(m>>4, 3, FHX8+1);
        }
    }
    else if(m<0x4000){ // 8x8 Field
        Vout->MbMode = MODE_INTER_8X8_FIELD; 
        Vout->InterMbType = 0x16;
        Vout->MbSubPredMode = m = (m - 0x55)&0xFF;
        Vout->NumPackedMv = 4 + (!!(m&2)) + (!!(m&4)) + (!!(m&32)) + (!!(m&128));
        Vout->Distortion[ 0] = Dist[0][F8X8]; 
        Vout->Distortion[ 4] = Dist[0][F8X8+1]; 
        Vout->Distortion[ 8] = Dist[0][F8X8+2]; 
        Vout->Distortion[12] = Dist[0][F8X8+3];
        Vout->RefId[0] = (BestRef[1][B8X8  ]<<4)|BestRef[0][B8X8  ];
        Vout->RefId[1] = (BestRef[1][B8X8+1]<<4)|BestRef[0][B8X8+1];
        Vout->RefId[2] = (BestRef[1][B8X8+2]<<4)|BestRef[0][B8X8+2];
        Vout->RefId[3] = (BestRef[1][B8X8+3]<<4)|BestRef[0][B8X8+3];
        if(true)//Vsta.MvFlags&EXTENDED32MV_FLAG) // Flag removed on HSW
        {
            for(index=0;index<4;index++)
            {
                AssignMV(m>>(index*2), index*4, F8X8+index);
                AssignMV(m>>(index*2), index*4+1, F8X8+index);
                AssignMV(m>>(index*2), index*4+2, F8X8+index);
                AssignMV(m>>(index*2), index*4+3, F8X8+index);
            }
        }
        else
        {
            AssignMV(m, 0, F8X8);
            AssignMV(m>>2, 1, F8X8+1);
            AssignMV(m>>4, 2, F8X8+2);
            AssignMV(m>>6, 3, F8X8+3);
        }
    }
    else if(!(m&0xFF)){ // 8x8s 
        Vout->MbMode = MODE_INTER_8X8; 
        Vout->InterMbType = TYPE_INTER_8X8;
        Vout->MbSubPredMode = m = (m>>8) - 0x55;
        Vout->NumPackedMv = 4 + (!!(m&2)) + (!!(m&8)) + (!!(m&32)) + (!!(m&128));
        Vout->Distortion[ 0] = Dist[0][B8X8]; 
        Vout->Distortion[ 4] = Dist[0][B8X8+1]; 
        Vout->Distortion[ 8] = Dist[0][B8X8+2]; 
        Vout->Distortion[12] = Dist[0][B8X8+3]; 
        if (!(Vsta.SadType&INTER_CHROMA_MODE)) 
        {
            Vout->RefId[0] = (BestRef[1][B8X8  ]<<4)|BestRef[0][B8X8  ];
            Vout->RefId[1] = (BestRef[1][B8X8+1]<<4)|BestRef[0][B8X8+1];
            Vout->RefId[2] = (BestRef[1][B8X8+2]<<4)|BestRef[0][B8X8+2];
            Vout->RefId[3] = (BestRef[1][B8X8+3]<<4)|BestRef[0][B8X8+3];
        }
        else
        {//chroma 8x8
            for(k=0;k<4;k++)
                Vout->RefId[k] = (BestRef[1][BHXH]<<4)|BestRef[0][BHXH];
        }        
        if(true)//Vsta.MvFlags&EXTENDED32MV_FLAG) // Flag removed on HSW
        {
            for(index=0;index<4;index++)
            {
                AssignMV(m>>(index*2), index*4+0, B8X8+index);
                AssignMV(m>>(index*2), index*4+1, B8X8+index);
                AssignMV(m>>(index*2), index*4+2, B8X8+index);
                AssignMV(m>>(index*2), index*4+3, B8X8+index);
            }
        }
        else
        {
            AssignMV(m, 0, B8X8);
            AssignMV(m>>2, 1, B8X8+1);
            AssignMV(m>>4, 2, B8X8+2);
            AssignMV(m>>6, 3, B8X8+3);
            Replicate8MVs(MODE_INTER_8X8);
        }
    }
    else { // true combination of 8x8s and minors
        Vout->MbMode = MODE_INTER_MINOR; 
        Vout->InterMbType = TYPE_INTER_OTHER;
        Vout->MbSubShape = s = (m&255);
        Vout->MbSubPredMode = m = (m>>8) - 0x55;
        mv = &Vout->Mvs[0][0];
        for(k=0;k<4;k++){
            switch(s&3){
            case 0:
                Vout->Distortion[(k<<2)] = Dist[0][B8X8+k]; 
                Vout->NumPackedMv += (m&2) ? 2 : 1;
                AssignMV(m, k<<2, B8X8+k);
                AssignMV(m, (k<<2)+1, B8X8+k);
                AssignMV(m, (k<<2)+2, B8X8+k);
                AssignMV(m, (k<<2)+3, B8X8+k);
                Vout->RefId[k] = (BestRef[1][B8X8+k]<<4)|BestRef[0][B8X8+k];
                break;
            case 1:
                Vout->Distortion[(k<<2)  ] = Dist[0][B8X4+(k<<1)]; 
                Vout->Distortion[(k<<2)+2] = Dist[0][B8X4+(k<<1)+1]; 
                Vout->NumPackedMv += (m&2) ? 4 : 2;
                AssignMV(m, k<<2, B8X4+(k<<1));
                AssignMV(m, (k<<2)+1, B8X4+(k<<1));
                AssignMV(m, (k<<2)+2, B8X4+(k<<1)+1);
                AssignMV(m, (k<<2)+3, B8X4+(k<<1)+1);
                Vout->RefId[k] = Vsta.BlockRefId[0];
                break;
            case 2:
                Vout->Distortion[(k<<2)  ] = Dist[0][B4X8+(k<<1)]; 
                Vout->Distortion[(k<<2)+1] = Dist[0][B4X8+(k<<1)+1]; 
                Vout->NumPackedMv += (m&2) ? 4 : 2;
                AssignMV(m, k<<2, B4X8+(k<<1));
                AssignMV(m, (k<<2)+2, B4X8+(k<<1));
                AssignMV(m, (k<<2)+1, B4X8+(k<<1)+1);
                AssignMV(m, (k<<2)+3, B4X8+(k<<1)+1);
                Vout->RefId[k] = Vsta.BlockRefId[0];
                break;
            case 3:
                Vout->Distortion[(k<<2)  ] = Dist[0][B4X4+(k<<2)]; 
                Vout->Distortion[(k<<2)+1] = Dist[0][B4X4+(k<<2)+1]; 
                Vout->Distortion[(k<<2)+2] = Dist[0][B4X4+(k<<2)+2]; 
                Vout->Distortion[(k<<2)+3] = Dist[0][B4X4+(k<<2)+3]; 
                Vout->NumPackedMv += (m&2) ? 8: 4;
                if (!(Vsta.SadType&INTER_CHROMA_MODE)) {
                AssignMV(m, k<<2, B4X4+(k<<2));
                AssignMV(m, (k<<2)+1, B4X4+(k<<2)+1);
                AssignMV(m, (k<<2)+2, B4X4+(k<<2)+2);
                AssignMV(m, (k<<2)+3, B4X4+(k<<2)+3);
                Vout->RefId[k] = Vsta.BlockRefId[0];
                }
                else { // inter chroma 4x4 partition
                    AssignMV(m, k<<2, B4X4+(k<<2));
                    AssignMV(m>>2, (k<<2)+1, B4X4+(k<<2)+1);
                    AssignMV(m>>4, (k<<2)+2, B4X4+(k<<2)+2);
                    AssignMV(m>>6, (k<<2)+3, B4X4+(k<<2)+3);
                    Vout->RefId[k] = (BestRef[1][B8X8+k]<<4)|BestRef[0][B8X8+k];
                }
                break;
            }
            mv += 8;
            if (!((Vsta.SadType&INTER_CHROMA_MODE) && ((s&3)==3))) m>>=2; //inter chroma 4x4 partition
            s>>=2;
        }
    }
    mv = &Vout->Mvs[0][0];

    if (Vsta.SadType&INTER_CHROMA_MODE) // need to remap MVs
    {
        mfxI16PAIR tempMvs[4];
        int chromaMvShift[4];
        for (m = 0; m < 2; m++)
        {
            for (k = 0; k < 4; k++)
            {
                tempMvs[k] = Vout->Mvs[k][m];
            }
            for (k = 0; k < 4; k++)
            {
                if (Vout->MbSubShape==0){//chroma 8x8, if global wins, pass through mv
                    chromaMvShift[k] = (MultiReplaced[m][BHXH])? 0:1;
                }
                else {//chroma 4x4,if global wins, pass through mv 
                    chromaMvShift[k] = (MultiReplaced[m][B8X8+k])? 0:1;
                }

                for(int j=0;j<4;j++){
                    Vout->Mvs[4*k+j][m].x = tempMvs[k].x << chromaMvShift[k];
                    Vout->Mvs[4*k+j][m].y = tempMvs[k].y << chromaMvShift[k];
                }
            }
        }
        for (k = 4; k < 16; k++) // zero out unnecessary distortions
        {
            Vout->Distortion[k] = 0;
        }
        for (k = 1; k<4; k++)// cut 1 2 3 distortion to 4 8 12
        {
            Vout->Distortion[4*k] = Vout->Distortion[k];
            Vout->Distortion[k] = 0;
        }

    }
    else 
    {
        if (Vsta.SrcType&1) //16x8 or 8x8 luma
        {
            for (m = 0; m < 2; m++)
                for (k = 8; k<16; k++)//zero out un-necessary Distortion
                {
                    Vout->Distortion[k] = 0;
                    Vout->Mvs[k][m].x = 0;
                    Vout->Mvs[k][m].y = 0;
                }

            if (Vout->MbMode == MODE_INTER_16X8)//src size 16x8 and partition 16x8
            {//RefId[1] is for par 16x8+1,needs to be zeroed out
                Vout->RefId[1] = Vout->RefId[0];
            }
            Vout->RefId[2] = Vout->RefId[0];
            Vout->RefId[3] = Vout->RefId[1];
        }
        if(Vsta.SrcType&2)//8x8 luma
        {
            for (m = 0; m < 2; m++)
                for (k = 4; k<8; k++)
                {
                    Vout->Distortion[k+8] = Vout->Distortion[k]= 0;
                    Vout->Mvs[k+8][m].x = Vout->Mvs[k][m].x = 0;
                    Vout->Mvs[k+8][m].y = Vout->Mvs[k][m].y = 0;
                }
            Vout->RefId[1] = Vout->RefId[0];
            Vout->RefId[3] = Vout->RefId[1];
        }
    }
    if (!(Vsta.VmeModes&4)) // single ref, zero out bwd ref id.
    {
        for (k = 0; k<4; k++)
            Vout->RefId[k] &= 0xf;
    }

    Vout->DistInter = DistInter;
    //if((DistInter<Vout->MinDist)||((DistInter==Vout->MinDist)&&(intraWinner))) 
    //{
    //    Vout->MinDist = DistInter;
    //    SkipMask &= 0xFF3F;
    //    intraWinner = false;
    //}

GOTO_DONE_IME_SUMMARIZE:
    //zero out intra output data if intra is not enabled
    Vout->IntraPredModes[3] =
    Vout->IntraPredModes[2] =
    Vout->IntraPredModes[1] = 
    Vout->IntraPredModes[0] = 0;
    Vout->IntraAvail = 0;
    BestIntraDist = 0;
    IntraMbMode = 0;
    Vout->MbMode &= 0x0F;
    Vout->DistIntra = 0;
    return 0;

}

bool MEforGen75::SetupSkipMV()
{
    int i,j;
    int m = 0;
    int chroma = (Vsta.SadType&INTER_CHROMA_MODE) ? 1 : 0;

    if (bDirectReplaced)
    {
        return false;
    }

    int replCount = 16;
    if((Vsta.SrcType&0x1) && !(Vsta.SadType&INTER_CHROMA_MODE)) replCount = 16;
    if((Vsta.SrcType&0x2) && !(Vsta.SadType&INTER_CHROMA_MODE)) replCount = 16;
    
    int divFactor = 4;

    if((Vout->InterMbType == TYPE_INTER_8X8) &&
        ((Vsta.VmeModes&VM_SKIP_8X8) || !(Vsta.SadType&INTER_CHROMA_MODE))) // not chroma8x8
    {    
        // 4 MVP
        for (i=0; i<replCount; i++)
        {
            // always populate both forward and back Mv to match RTL
            m = (Vsta.SkipCenterEnables>>(i/divFactor)*2)&3;
            // 8x8 without minor
            if(/*MajorMode[0]<0x4000 || !(MajorMode[0]&0xFF)*/1)
            {
                if(chroma)
                {
                    Vout->Mvs[i][0].x = SkipCenterOrigChr[i/divFactor][0].x;
                    Vout->Mvs[i][0].y = SkipCenterOrigChr[i/divFactor][0].y;
                    Vout->Mvs[i][1].x = SkipCenterOrigChr[i/divFactor][1].x;
                    Vout->Mvs[i][1].y = SkipCenterOrigChr[i/divFactor][1].y;
                }
                else
                {
                    Vout->Mvs[i][0].x = VSICsta.SkipCenter[i/divFactor][0].x/*<<chroma*/;
                    Vout->Mvs[i][0].y = VSICsta.SkipCenter[i/divFactor][0].y/*<<chroma*/;
                    Vout->Mvs[i][1].x = VSICsta.SkipCenter[i/divFactor][1].x/*<<chroma*/;
                    Vout->Mvs[i][1].y = VSICsta.SkipCenter[i/divFactor][1].y/*<<chroma*/;
                }
                /*if (m&1)
                {
                    Vout->Mvs[i][0].x = VSICsta.SkipCenter[i/divFactor][0].x;
                    Vout->Mvs[i][0].y = VSICsta.SkipCenter[i/divFactor][0].y;
                }            
                if (m&2)
                {
                    Vout->Mvs[i][1].x =  VSICsta.SkipCenter[i/divFactor][1].x;
                    Vout->Mvs[i][1].y =  VSICsta.SkipCenter[i/divFactor][1].y;
                }*/
            }
            else
            {
                // 8x8 with minor             
                for(j = 0; j < 4; j++)
                {
                    // always populate both forward and back Mv to match RTL
                    Vout->Mvs[(4*i)+j][0].x = VSICsta.SkipCenter[j][0].x<<chroma;
                    Vout->Mvs[(4*i)+j][0].y = VSICsta.SkipCenter[j][0].y<<chroma;
                    Vout->Mvs[(4*i)+j][1].x =  VSICsta.SkipCenter[j][1].x<<chroma;
                    Vout->Mvs[(4*i)+j][1].y =  VSICsta.SkipCenter[j][1].y<<chroma;

                    /*if (m&1)
                    {
                        Vout->Mvs[(4*i)+j][0].x = VSICsta.SkipCenter[j][0].x;
                        Vout->Mvs[(4*i)+j][0].y = VSICsta.SkipCenter[j][0].y;
                    }            
                    if (m&2)
                    {
                        Vout->Mvs[(4*i)+j][1].x =  VSICsta.SkipCenter[j][1].x;
                        Vout->Mvs[(4*i)+j][1].y =  VSICsta.SkipCenter[j][1].y;
                    }*/
                }                         
            }
        }
    }
    else
    {
        m = (Vsta.SkipCenterEnables)&3;
        for (i =0; i<replCount; i++)
        {
            // always populate both forward and back Mv to match RTL
            //Vout->Mvs[i][0].x = VSICsta.SkipCenter[0][0].x<<chroma;
            //Vout->Mvs[i][0].y = VSICsta.SkipCenter[0][0].y<<chroma;
            //Vout->Mvs[i][1].x =  VSICsta.SkipCenter[0][1].x<<chroma;
            //Vout->Mvs[i][1].y =  VSICsta.SkipCenter[0][1].y<<chroma;
            if(chroma)
            {
                Vout->Mvs[i][0].x = SkipCenterOrigChr[0][0].x;
                Vout->Mvs[i][0].y = SkipCenterOrigChr[0][0].y;
                Vout->Mvs[i][1].x = SkipCenterOrigChr[0][1].x;
                Vout->Mvs[i][1].y = SkipCenterOrigChr[0][1].y;
            }
            else
            {
                Vout->Mvs[i][0].x = VSICsta.SkipCenter[0][0].x/*<<chroma*/;
                Vout->Mvs[i][0].y = VSICsta.SkipCenter[0][0].y/*<<chroma*/;
                Vout->Mvs[i][1].x = VSICsta.SkipCenter[0][1].x/*<<chroma*/;
                Vout->Mvs[i][1].y = VSICsta.SkipCenter[0][1].y/*<<chroma*/;
            }
            /*if (m&1)
            {
                Vout->Mvs[i][0].x = VSICsta.SkipCenter[0][0].x;
                Vout->Mvs[i][0].y = VSICsta.SkipCenter[0][0].y;
            }            
            if (m&2)
            {
                Vout->Mvs[i][1].x =  VSICsta.SkipCenter[0][1].x;
                Vout->Mvs[i][1].y =  VSICsta.SkipCenter[0][1].y;
            }*/
        } 
    }

    return true;
}

/*****************************************************************************************************/
int MEforGen75::SummarizeFinalOutputSIC( )
/*****************************************************************************************************/
{
    //const U8 MbType16x8[4][4] = {{4,8,12,0},{10,6,14,0},{16,18,20,0},{0,0,0,0}};
    //const U8 MbType8x16[16]   = { 5,11,17,0,  9,7,19,0,  13,15,21,0,  0,0,0,0 };
    //int        k, m, s;
    int m;
    //I16PAIR    *mv;

#ifdef VMEPERF
    //Calculate backend compute clocks
    GetCREComputeClkCount(SIC_MSG);
#endif

    bool intraWinner = true;
    Vout->IntraAvail = Vsta.IntraAvail;
    Vout->NumPackedMv = 0;
    Vout->RefBorderMark = 0;
    //Vout->D8x8Pattern = 0;// JMHTODO: This was removed.  Investigate.
    MoreSU[1] = (Vsta.VmeModes&4) ? MoreSU[1] : Vsta.MaxNumSU;
    Vout->NumSUinIME = 0;
    Vout->ImeStop_MaxMV_AltNumSU = 0;
    Vout->DistInter = 0;
    Vout->DistSkip = 0;
    Vout->IntraMbType = 0;
    I32 SkipDistortion = DistInter;
    U8 SkipMode = Vout->MbMode;
    BestIntraDist = DistI16;
    U8 IntraMbMode = MODE_INTRA_16X16;
    bool bBlockBasedSkipEn = ((Vsta.SadType&BLOCK_BASED_SKIP) == BLOCK_BASED_SKIP);
    //int replFactor = 1;
    //int finalMVCount = 4;
    //int index = 0;    
    memset(Vout->Mvs, 0, 16*2*sizeof(I16PAIR));
    memset(Vout->Distortion, 0, 16*sizeof(U16));
    
    // output all refids
    for(int i=0; i<4; i++)
    {
        Vout->RefId[i] = (RefID[1][i]<<4 | RefID[0][i]); 
    }

    //if(!bBlockBasedSkipEn)
    //{
    //    if((Vout->MinDist > EarlySkipSuc)&&((Vsta.SrcType&0x3)!=0))
    //    {
    //        if(Vsta.ShapeMask != 0x7F) {Vout->MinDist = MBMAXDIST; SkipDistortion = MBMAXDIST;}
    //    }
    //}
    //else
    //{
    //    if((bSkipSbFailed)&&((Vsta.SrcType&0x3)!=0))
    //    {
    //        if(Vsta.ShapeMask != 0x7F){Vout->MinDist = MBMAXDIST; SkipDistortion = MBMAXDIST;}
    //    }
    //}

    if((DistIntra4or8&MBMAXDIST)<BestIntraDist){
        BestIntraDist = DistIntra4or8&MBMAXDIST; //or 0x10000 to distinguish intra8x8 and intra4x4
        IntraMbMode = (DistIntra4or8&0x10000)? MODE_INTRA_8X8 : MODE_INTRA_4X4;
    }

    if((Vsta.VmeFlags&VF_PB_SKIP_ENABLE)) Vout->DistSkip = SubBlockMaxSad;
    if((VSICsta.IntraComputeType != INTRA_DISABLED)) {
        Vout->MbMode = IntraMbMode; Vout->DistIntra = BestIntraDist;
        if(VSICsta.IntraComputeType == INTRA_LUMA_CHROMA){
            Vout->IntraAvail = (Vout->IntraAvail&0xfc) | ChromaPredMode;  
            Vout->DistChromaIntra = BestIntraChromaDist;
        }
    }

    //if((Vsta.VmeFlags&VF_PB_SKIP_ENABLE)&&(SkipDistortion<=BestIntraDist)) {Vout->MinDist = SkipDistortion; intraWinner = false;}
    //else{ Vout->MinDist = BestIntraDist; }

    // check if its skip and skip is the winner.
    if((SkipMask & 0x40)&&(SkipDistortion <= DistInter)) 
    {
        //survived or skip won
        Vout->DistInter = SkipDistMb;
        Vout->MbMode = SkipMode;
        if(SkipMask & 0x40) //survived or skip won
        {
            if (Vsta.SadType&INTER_CHROMA_MODE)
            {
                m = (SkipMask>>8);//-0x55;
                if (Vsta.VmeModes&VM_SKIP_8X8) // 4x4
                {
                    Vout->MbSubShape = 3;
                    Vout->NumPackedMv = 4 + (!!(m&2)) + (!!(m&8)) + (!!(m&32)) + (!!(m&128));
                }
                else
                {
                    Vout->NumPackedMv = 1 + (Vout->MbSubPredMode==2);
                }
                for(int i=0; i<4; i++)
                {
                    Vout->RefId[i] = (RefID[1][i]<<4 | RefID[0][i]); 
                }
            }
            else // luma
            {
                if(Vout->InterMbType == TYPE_INTER_8X8){ //8x8
                    //Vout->D8x8Pattern = ((SkipMask&15)<<4);// JMHTODO: This was removed.  Investigate.
                    m = (SkipMask>>8);//-0x55;
                    Vout->NumPackedMv = 4 + (!!(m&2)) + (!!(m&8)) + (!!(m&32)) + (!!(m&128));
                    if(((Vsta.SrcType&0x3)==0x3) && !(Vsta.SadType&INTER_CHROMA_MODE)){ Vout->NumPackedMv=1 + (!!(m&2)); }
                    if(((Vsta.SrcType&0x3)==0x1) && !(Vsta.SadType&INTER_CHROMA_MODE)){ Vout->NumPackedMv=2 + (!!(m&2)) + (!!(m&8)); }
                }
                else{//16x16
                    //Vout->D8x8Pattern = 0xF0;// JMHTODO: This was removed.  Investigate.
                    Vout->NumPackedMv = 1 + (Vout->InterMbType==3);
                }
            }
            SetupSkipMV();
            if(!(true))//Vsta.MvFlags&EXTENDED32MV_FLAG)) // Flag removed on HSW
                Replicate8MVs(Vout->MbMode);
            if (((!bBlockBasedSkipEn)&&(SkipDistortion <= EarlySkipSuc))||((bBlockBasedSkipEn)&&(!bSkipSbFailed)))
            {
                //Vout->MbMode |= MODE_SKIP_FLAG;
            }
            if(Vout->InterMbType != TYPE_INTER_8X8)
            {
                Vout->Distortion[0] = SkipDistAddMV16x16;
            }
            else if(Vout->InterMbType == TYPE_INTER_8X8)
            {
                for(U8 blkNum = 0; blkNum < 4; blkNum++)
                {
                    Vout->Distortion[4*blkNum] = SkipDistAddMV8x8[blkNum];
                    if ((!(Vsta.VmeModes&VM_SKIP_8X8)) && (Vsta.SadType&INTER_CHROMA_MODE))
                        break; // chroma8x8
                }
            }
        }
    }

    if(VSICsta.IntraComputeType == INTRA_DISABLED) 
    {
        //zero out intra output data if intra is not enabled
        Vout->IntraPredModes[3] =
        Vout->IntraPredModes[2] =
        Vout->IntraPredModes[1] = 
        Vout->IntraPredModes[0] = 0;
        Vout->IntraAvail = 0;
        BestIntraDist = 0;
        IntraMbMode = 0;
        Vout->MbMode &= 0x0F;
        Vout->DistIntra = 0;
        Vout->DistChromaIntra = 0;
        return 0;
    }

    Vout->MbMode = Vout->MbMode | IntraMbMode;

    switch(IntraMbMode){
        case MODE_INTRA_16X16:            
            
                Vout->IntraPredModes[0] = ((Vout->IntraPredModes[0])|(Vout->IntraPredModes[0]<<4)|(Vout->IntraPredModes[0]<<8)|(Vout->IntraPredModes[0]<<12));
                Vout->IntraPredModes[3] =
                Vout->IntraPredModes[2] =
                Vout->IntraPredModes[1] = Vout->IntraPredModes[0];
            
            break;
        case MODE_INTRA_8X8:
            if(intraWinner)
                Vout->InterMbType |= (TYPE_INTRA_8X8&0x80); // transform 8x8
            
                Vout->IntraPredModes[0] = ((Intra8x8PredMode[0])|(Intra8x8PredMode[0]<<4)|(Intra8x8PredMode[0]<<8)|(Intra8x8PredMode[0]<<12));
                Vout->IntraPredModes[1] = ((Intra8x8PredMode[1])|(Intra8x8PredMode[1]<<4)|(Intra8x8PredMode[1]<<8)|(Intra8x8PredMode[1]<<12));
                Vout->IntraPredModes[2] = ((Intra8x8PredMode[2])|(Intra8x8PredMode[2]<<4)|(Intra8x8PredMode[2]<<8)|(Intra8x8PredMode[2]<<12));
                Vout->IntraPredModes[3] = ((Intra8x8PredMode[3])|(Intra8x8PredMode[3]<<4)|(Intra8x8PredMode[3]<<8)|(Intra8x8PredMode[3]<<12));
            
            break;
        case MODE_INTRA_4X4:
            Vout->IntraPredModes[0] = Intra4x4PredMode[0]|(Intra4x4PredMode[1]<<4)|(Intra4x4PredMode[2]<<8)|(Intra4x4PredMode[3]<<12);
            Vout->IntraPredModes[1] = Intra4x4PredMode[4]|(Intra4x4PredMode[5]<<4)|(Intra4x4PredMode[6]<<8)|(Intra4x4PredMode[7]<<12);
            Vout->IntraPredModes[2] = Intra4x4PredMode[8]|(Intra4x4PredMode[9]<<4)|(Intra4x4PredMode[10]<<8)|(Intra4x4PredMode[11]<<12);
            Vout->IntraPredModes[3] = Intra4x4PredMode[12]|(Intra4x4PredMode[13]<<4)|(Intra4x4PredMode[14]<<8)|(Intra4x4PredMode[15]<<12);
            break;
        default:  
            //return ERR_UNSUPPORTED;
            //Vout->MbMode = MODE_INTRA_16X16;
            //Vout->MbType5Bits = TYPE_INTRA_16X16;
            //Vout->IntraPredModes[3] =
            //Vout->IntraPredModes[2] =
            //Vout->IntraPredModes[1] =
            //Vout->IntraPredModes[0] = 0;
            //return 0;
            break;
    }

    if (VSICsta.IntraComputeType != INTRA_DISABLED)    // 2 = CRE_INTRA_NONE
    {
        Vout->IntraMbType = 0x15 + (Vout->IntraPredModes[0]&0x3);        // CRE_MBTYPE_INTRA_16x16_0_2_1 = 0x15
        if (((Vout->MbMode&0x30)>>4) != 0)    // if not intra 16x16 partition, then tie IntraMbType to 0
            Vout->IntraMbType = 0;
    }
    else
    {
        Vout->IntraMbType = 0;    
    }
    Vout->IntraMbType |=  0x80;

    return 0;
}

/*****************************************************************************************************/
int MEforGen75::SummarizeFinalOutputFBR( )
/*****************************************************************************************************/
{
    const U8 MbType8x16[16]   = { 5,11,17,0,  9,7,19,0,  13,15,21,0,  0,0,0,0 };
    const U8 MbType16x8[16]   = { 4,10,16,0,  8,6,18,0,  12,14,20,0,  0,0,0,0 };

#ifdef VMEPERF    
    //Calculate backend compute clocks
    GetCREComputeClkCount(FBR_MSG);
#endif

    int        m, k, s;
    s = 0; // just to compile
    I16PAIR    *mv;
    bool intraWinner = false;
    Vout->DistInter = 0;
    
    int replFactor = 1;
    //int finalMVCount = 4;
    int index = 0;    
    memset(Vout->Mvs, 0, 16*2*sizeof(I16PAIR));
    memset(Vout->Distortion, 0, 16*sizeof(U16));
    Vout->DistSkip = 0;

    // output all refids
    for(int i=0; i<4; i++)
    {
        Vout->RefId[i] = (RefID[1][i]<<4 | RefID[0][i]); 
    }

    //reaching here means that skip is not the winner. the choice is between intra and inter.
    SkipMask &= 0xFF3F;
    
    //Vout->D8x8Pattern = bDirectReplaced ? ((SkipMask&15)<<4) : Vout->D8x8Pattern; //set here so that AssignMV can use it// JMHTODO: This was removed.  Investigate.
    m = MajorMode[0];
    if(Vsta.FBRMbInfo == MODE_INTER_16X16){ // 16x16
        Vout->MbMode = MODE_INTER_16X16; 
        switch((m>>8)&0x3){
            case 0: Vout->InterMbType = TYPE_INTER_16X16_0; break;
            case 1: Vout->InterMbType = TYPE_INTER_16X16_1; break;
            case 2: Vout->InterMbType = TYPE_INTER_16X16_2; break;
        }
        m = (m>>8);
        m = m&0x3; // Make Sure only First 2 bits are used.
        Vout->MbSubPredMode = m;
        Vout->NumPackedMv = 1 + (m==2);
        Vout->Distortion[0] = Dist[(Vsta.FBRSubPredMode>>0)&3][BHXH];
        Vout->DistInter = MajorDist[0];
        Vout->RefId[0] = (RefID[1][0]<<4 | RefID[0][0]); 
        replFactor = 16;    
        
      /*  for(k=0;k<replFactor;k++){
        if( ((m>>0)&3) != 2){
            Best[!((m>>0)&3)][k].x =0;
            Best[!((m>>0)&3)][k].y =0;}
        }*/
        for(k=0;k<replFactor;k++){
            AssignMV(Vsta.FBRSubPredMode, k, BHXH, m);
        }
    }
    else if(Vsta.FBRMbInfo == MODE_INTER_8X16){ // 8x16
        Vout->MbMode = MODE_INTER_8X16; 
        m = (m>>8)&0xf;//-5;
        Vout->InterMbType = MbType8x16[m];
        Vout->MbSubPredMode = m;
        Vout->NumPackedMv = 2 + ((m&0x3)==2) + (((m>>2)&0x3)==2);
        Vout->Distortion[0] = Dist[(Vsta.FBRSubPredMode>>0)&0x03][B8XH]; 
        Vout->Distortion[4] = Dist[(Vsta.FBRSubPredMode>>2)&0x03][B8XH+1]; 
        Vout->DistInter = MajorDist[0];
        Vout->RefId[0] = (RefID[1][0]<<4 | RefID[0][0]); 
        Vout->RefId[1] = (RefID[1][1]<<4 | RefID[0][1]); 
        if(true)//Vsta.MvFlags&EXTENDED32MV_FLAG)
        {
            for(index=0; index<4;index++)
            {   
                AssignMV(m>>((index%2)*2), 4*index,      B8XH+index%2, m>>((index%2)*2)&3);
                AssignMV(m>>((index%2)*2), 4*index+1, B8XH+index%2, m>>((index%2)*2)&3);
                AssignMV(m>>((index%2)*2), 4*index+2, B8XH+index%2, m>>((index%2)*2)&3);
                AssignMV(m>>((index%2)*2), 4*index+3, B8XH+index%2, m>>((index%2)*2)&3);
            }
        }
    }
    else if(Vsta.FBRMbInfo == MODE_INTER_16X8){ // 16x8
        U8 SrcSzCount = (Vsta.SrcType&ST_SRC_16X8) ? 2 : 1;
        Vout->MbMode = MODE_INTER_16X8; 
        m = (m>>8)&0xf;//-17;
        Vout->InterMbType = MbType16x8[m];
        Vout->MbSubPredMode = m;
        if (!(Vsta.SrcType&ST_SRC_16X8)) // in case of reduced size
        {
            Vout->NumPackedMv = 2 + ((m&0x3)==2) + (((m>>2)&0x3)==2);
        }
        else
        {
            Vout->NumPackedMv = 2 + 2 * ((m&0x3)==2); // to be divided by 2 later in RunFBR()
        }
        Vout->Distortion[0] = Dist[(Vsta.FBRSubPredMode>>0)&0x03][BHX8]; 
        if (!(Vsta.SrcType&ST_SRC_16X8)) // in case of reduced size
        {
            Vout->Distortion[8] = Dist[(Vsta.FBRSubPredMode>>2)&0x03][BHX8+1];
        }
        Vout->DistInter = MajorDist[0];
        Vout->RefId[0] = (RefID[1][0]<<4 | RefID[0][0]); 
        Vout->RefId[2] = (RefID[1][2]<<4 | RefID[0][2]);
        if(true)//Vsta.MvFlags&EXTENDED32MV_FLAG)
        {
            for(index=0 ;index<16/SrcSzCount; index++)
            {
                AssignMV(m>>(2*(index/8)),index,BHX8+index/8, m>>(2*(index/8))&3);
            }
        }
    }
    else if(Vsta.FBRMbInfo == MODE_INTER_16X8 && Vsta.SrcType&ST_REF_FIELD){ // 16x8 Field
        Vout->MbMode = MODE_INTER_16X8_FIELD; 
        m = m>>8;//(m&0xFF)//-17;
        Vout->InterMbType = MbType16x8[m];//&3][m>>4];
        Vout->MbSubPredMode = m;//*0x05;
        Vout->NumPackedMv = 2 + ((m&0x3)==2) + (((m>>2)&0x3)==2);
        Vout->Distortion[0] = Dist[0][FHX8]; 
        Vout->Distortion[8] = Dist[0][FHX8+1]; 
        Vout->DistInter = MajorDist[0];
        Vout->RefId[0] = (RefID[1][0]<<4 | RefID[0][0]); 
        Vout->RefId[2] = (RefID[1][2]<<4 | RefID[0][2]);
        if(true)//Vsta.MvFlags&EXTENDED32MV_FLAG)
        {
            for(index=0; index<16; index++)
            {
                AssignMV(m>>(2*(index/8)),index,FHX8+index/8, m>>(2*(index/8))&3);
            }
        }
    }
    else if(!(Vsta.FBRSubMbShape&0xFF)){ // 8x8s 
        U8 SrcSzCount = 1;
        if(!(Vsta.SadType&INTER_CHROMA_MODE)){ SrcSzCount = (Vsta.SrcType&3)+1;}

        Vout->MbMode = (Vsta.SrcType&ST_REF_FIELD) ? MODE_INTER_8X8_FIELD : MODE_INTER_8X8; 
        Vout->InterMbType = TYPE_INTER_8X8;
        Vout->MbSubPredMode = m = (m>>8);// - 0x55;
        if (!(Vsta.SadType&INTER_CHROMA_MODE)&&((Vsta.SrcType&0x3)==ST_SRC_16X8))
            Vout->NumPackedMv = 4 + ((m&0x3)==2)*2 + (((m>>2)&0x3)==2)*2;
        else if (!(Vsta.SadType&INTER_CHROMA_MODE)&&(!(Vsta.SrcType & 0x2)))
            Vout->NumPackedMv = 4 + ((m&0x3)==2) + (((m>>2)&0x3)==2) + (((m>>4)&0x3)==2) + (((m>>6)&0x3)==2);
        else //chroma 8x8 bi wins, NumPackedMv=8, final output is 2
            Vout->NumPackedMv = 4 + ((m&0x3)==2)*4;
        

        Vout->Distortion[ 0] = Dist[(Vsta.FBRSubPredMode>>0)&0x03][B8X8]; 
        if((!(Vsta.SrcType & 0x2)) && (!(Vsta.SadType&INTER_CHROMA_MODE)))
        {
            Vout->Distortion[ 4] = Dist[(Vsta.FBRSubPredMode>>2)&0x03][B8X8+1]; 
            if((!(Vsta.SrcType & 0x1)) && (!(Vsta.SadType&INTER_CHROMA_MODE)))
            {
                Vout->Distortion[ 8] = Dist[(Vsta.FBRSubPredMode>>4)&0x03][B8X8+2]; 
                Vout->Distortion[12] = Dist[(Vsta.FBRSubPredMode>>6)&0x03][B8X8+3]; 
            }
            

        }
        Vout->DistInter = MajorDist[0];
        Vout->RefId[0] = (RefID[1][0]<<4 | RefID[0][0]); 
        Vout->RefId[1] = (RefID[1][1]<<4 | RefID[0][1]);
        Vout->RefId[2] = (RefID[1][2]<<4 | RefID[0][2]); 
        Vout->RefId[3] = (RefID[1][3]<<4 | RefID[0][3]);
        if(true)//Vsta.MvFlags&EXTENDED32MV_FLAG)
        {   
            for(index=0;index<4/SrcSzCount;index++)
            {
                AssignMV(m>>(index*2), index*4+0, B8X8+index, m>>(index*2)&3);
                AssignMV(m>>(index*2), index*4+1, B8X8+index, m>>(index*2)&3);
                AssignMV(m>>(index*2), index*4+2, B8X8+index, m>>(index*2)&3);
                AssignMV(m>>(index*2), index*4+3, B8X8+index, m>>(index*2)&3);
            }
        }
    }
    else { // true combination of 8x8s and minors
        Vout->MbMode = (Vsta.SrcType&ST_REF_FIELD) ? MODE_INTER_8X8_FIELD : MODE_INTER_MINOR; 
        Vout->InterMbType = TYPE_INTER_OTHER;
        Vout->MbSubShape = s = Vsta.FBRSubMbShape;//(m&255);
        Vout->MbSubPredMode = m = (m>>8);// - 0x55;
        Vout->DistInter = MajorDist[0];
        Vout->RefId[0] = (RefID[1][0]<<4 | RefID[0][0]); 
        Vout->RefId[1] = (RefID[1][1]<<4 | RefID[0][1]);
        Vout->RefId[2] = (RefID[1][2]<<4 | RefID[0][2]); 
        Vout->RefId[3] = (RefID[1][3]<<4 | RefID[0][3]);
        mv = &Vout->Mvs[0][0];
        U8 loops = 4;
        U8 MvFactor = 1;
        if((Vsta.SrcType & 0x1) && !(Vsta.SadType&INTER_CHROMA_MODE)){ loops >>= 1;MvFactor=2; }
        if((Vsta.SrcType & 0x2) && !(Vsta.SadType&INTER_CHROMA_MODE)){ loops >>= 1;MvFactor=4; }

        
            
        for(k=0;k<4;k++){

            if (k>=loops) break;
            switch(s&3){
            case 0:
                Vout->Distortion[(k<<2)] = Dist[(Vsta.FBRSubPredMode>>(2*k))&0x3][B8X8+k];
                Vout->NumPackedMv += ((m&2) ? 2 : 1 )*MvFactor;
                AssignMV(m, k<<2, B8X8+k, m&3);
                AssignMV(m, (k<<2)+1, B8X8+k, m&3);
                AssignMV(m, (k<<2)+2, B8X8+k, m&3);
                AssignMV(m, (k<<2)+3, B8X8+k, m&3);
                break;
            case 1:
                Vout->Distortion[(k<<2)  ] = Dist[(Vsta.FBRSubPredMode>>(2*k))&0x3][B8X4+(k<<1)]; 
                Vout->Distortion[(k<<2)+2] = Dist[(Vsta.FBRSubPredMode>>(2*k))&0x3][B8X4+(k<<1)+1]; 
                Vout->NumPackedMv += ((m&2) ? 4 : 2 )*MvFactor;
                AssignMV(m, k<<2, B8X4+(k<<1), m&3);
                AssignMV(m, (k<<2)+1, B8X4+(k<<1), m&3);
                AssignMV(m, (k<<2)+2, B8X4+(k<<1)+1, m&3);
                AssignMV(m, (k<<2)+3, B8X4+(k<<1)+1, m&3);
                break;
            case 2:
                Vout->Distortion[(k<<2)  ] = Dist[(Vsta.FBRSubPredMode>>(2*k))&0x3][B4X8+(k<<1)]; 
                Vout->Distortion[(k<<2)+1] = Dist[(Vsta.FBRSubPredMode>>(2*k))&0x3][B4X8+(k<<1)+1]; 
                Vout->NumPackedMv += ((m&2) ? 4 : 2 )*MvFactor;
                AssignMV(m, k<<2, B4X8+(k<<1), m&3);
                AssignMV(m, (k<<2)+2, B4X8+(k<<1), m&3);
                AssignMV(m, (k<<2)+1, B4X8+(k<<1)+1, m&3);
                AssignMV(m, (k<<2)+3, B4X8+(k<<1)+1, m&3);
                break;
            case 3:
                if (!(Vsta.SadType&INTER_CHROMA_MODE))
                {
                    Vout->Distortion[(k<<2)  ] = Dist[(Vsta.FBRSubPredMode>>(2*k))&0x3][B4X4+(k<<2)]; 
                    Vout->Distortion[(k<<2)+1] = Dist[(Vsta.FBRSubPredMode>>(2*k))&0x3][B4X4+(k<<2)+1]; 
                    Vout->Distortion[(k<<2)+2] = Dist[(Vsta.FBRSubPredMode>>(2*k))&0x3][B4X4+(k<<2)+2]; 
                    Vout->Distortion[(k<<2)+3] = Dist[(Vsta.FBRSubPredMode>>(2*k))&0x3][B4X4+(k<<2)+3]; 
                    AssignMV(m, k<<2, B4X4+(k<<2), m&3);
                    AssignMV(m, (k<<2)+1, B4X4+(k<<2)+1, m&3);
                    AssignMV(m, (k<<2)+2, B4X4+(k<<2)+2, m&3);
                    AssignMV(m, (k<<2)+3, B4X4+(k<<2)+3, m&3);
                    Vout->NumPackedMv += ((m&2) ? 8: 4 )*MvFactor;
                }
                else
                {
                    if(Vsta.VmeFlags&VF_BIMIX_DISABLE)    //Jay - copied from BidirectionalMESearchUnitChroma
                    {
                        if(!(Vsta.SadType&DISABLE_BME)&&(Vsta.FBRSubPredMode==0xaa))  
                        {
                            Vout->Distortion[(k<<2)  ] = Dist[0][B8X8+(k<<2)]; 
                            Vout->Distortion[(k<<2)+1] = Dist[0][B8X8+(k<<2)+1]; 
                            Vout->Distortion[(k<<2)+2] = Dist[0][B8X8+(k<<2)+2]; 
                            Vout->Distortion[(k<<2)+3] = Dist[0][B8X8+(k<<2)+3]; 
                        }
                        else
                        {
                            Vout->Distortion[(k<<2)  ] = Dist[(Vsta.FBRSubPredMode>>0)&0x3][B4X4+(k<<2)]; 
                            Vout->Distortion[(k<<2)+1] = Dist[(Vsta.FBRSubPredMode>>2)&0x3][B4X4+(k<<2)+1]; 
                            Vout->Distortion[(k<<2)+2] = Dist[(Vsta.FBRSubPredMode>>4)&0x3][B4X4+(k<<2)+2]; 
                            Vout->Distortion[(k<<2)+3] = Dist[(Vsta.FBRSubPredMode>>6)&0x3][B4X4+(k<<2)+3]; 
                        }
                    }
                    else
                    {
                        Vout->Distortion[(k<<2)  ] = Dist[(Vsta.FBRSubPredMode>>0)&0x3][B4X4+(k<<2)]; 
                        Vout->Distortion[(k<<2)+1] = Dist[(Vsta.FBRSubPredMode>>2)&0x3][B4X4+(k<<2)+1]; 
                        Vout->Distortion[(k<<2)+2] = Dist[(Vsta.FBRSubPredMode>>4)&0x3][B4X4+(k<<2)+2]; 
                        Vout->Distortion[(k<<2)+3] = Dist[(Vsta.FBRSubPredMode>>6)&0x3][B4X4+(k<<2)+3]; 
                    }
                    AssignMV(m, k<<2, B4X4+(k<<2), m&3);
                    AssignMV(m>>2, (k<<2)+1, B4X4+(k<<2)+1, (m>>2)&3);
                    AssignMV(m>>4, (k<<2)+2, B4X4+(k<<2)+2, (m>>4)&3);
                    AssignMV(m>>6, (k<<2)+3, B4X4+(k<<2)+3, (m>>6)&3);
                    Vout->NumPackedMv += (m&2) ? 2: 1;
                    Vout->NumPackedMv += ((m>>2)&2) ? 2: 1;
                    Vout->NumPackedMv += ((m>>4)&2) ? 2: 1;
                    Vout->NumPackedMv += ((m>>6)&2) ? 2: 1;
                }
                break;
            }
            mv += 8;
            m>>=2; s>>=2;
            if (Vsta.SadType&INTER_CHROMA_MODE) 
            {
                Vout->NumPackedMv <<= 2;
                break;
            }
        }
    }
    mv = &Vout->Mvs[0][0];

    if (Vsta.SadType&INTER_CHROMA_MODE) // need to remap MVs
    {
        mfxI16PAIR tempMvs[4];
        for (m = 0; m < 2; m++)
        {
            for (k = 0; k < 4; k++)
            {
                tempMvs[k] = Vout->Mvs[k][m];
            }
            for (k = 0; k < 4; k++)
            {
                Vout->Mvs[4*k][m].x = tempMvs[k].x << 1;
                Vout->Mvs[4*k][m].y = tempMvs[k].y << 1;
                Vout->Mvs[4*k+1][m].x = tempMvs[k].x << 1;
                Vout->Mvs[4*k+1][m].y = tempMvs[k].y << 1;
                Vout->Mvs[4*k+2][m].x = tempMvs[k].x << 1;
                Vout->Mvs[4*k+2][m].y = tempMvs[k].y << 1;
                Vout->Mvs[4*k+3][m].x = tempMvs[k].x << 1;
                Vout->Mvs[4*k+3][m].y = tempMvs[k].y << 1;
            }
        }
        for (k = 4; k < 16; k++) // zero out unnecessary distortions
        {
            Vout->Distortion[k] = 0;
        }
        // remap debug distortions
        Vout->Distortion[4] = Vout->Distortion[1]; Vout->Distortion[1] = 0;
        Vout->Distortion[8] = Vout->Distortion[2]; Vout->Distortion[2] = 0;
        Vout->Distortion[12] = Vout->Distortion[3]; Vout->Distortion[3] = 0;
    }

    Vout->RefBorderMark = 0;

    intraWinner = false;
    // set the extended flag as a part of intra Mb Type
    Vout->IntraMbType |=  0x80 ;
    
    //zero out intra output data if intra is not enabled
    Vout->IntraPredModes[3] =
    Vout->IntraPredModes[2] =
    Vout->IntraPredModes[1] = 
    Vout->IntraPredModes[0] = 0;
    Vout->IntraAvail = 0;
    BestIntraDist = 0;
    Vout->MbMode &= 0x0F;
    Vout->DistIntra = 0;

    return 0;
}

/*****************************************************************************************************/
void MEforGen75::AssignMV(int mode, int k, int shape, int bi)
/*****************************************************************************************************/
{
    bool InterChromaEn = Vsta.SadType&INTER_CHROMA_MODE;
    int xoff = InterChromaEn? 9:17;
    int yoff = InterChromaEn? 9:17;
    int base = 0;
    int numSubPartition = 0;
    int blockNum = 0;
    int streamInShape = shape;
    bool checkRefBorderMark = true;
    //mode = (mode>>(2*(k/4)))&3;
    //printf("mode=%d     Search=%d   bi=%d\n",mode,Vsta.VmeModes,bi);
    if((shape >= B8X8)&&(shape <= (B8X8 + 3)))
    {
        base = B8X8;
        numSubPartition = 1;
        blockNum = shape - base;
        /*if(((Vout->D8x8Pattern >> 4)&(1<<blockNum))==(1<<blockNum))
        {
            checkRefBorderMark = false;
        }*/// JMHTODO: This was removed.  Investigate.
    }
    else if(shape == BHXH)
    {
        /*if((Vout->D8x8Pattern >> 4) == 0xf)
        {
            checkRefBorderMark = false;
        }*/// JMHTODO: This was removed.  Investigate.
    }

    if (InterChromaEn)
    {
        // chroma streamin remap
        if((shape >= B8X8)&&(shape <= (B8X8 + 3)))
            streamInShape = BHXH;
        else
            streamInShape = B8X8 + ((shape-B4X4)%4);
    }
    if((Vsta.SrcType&0x1)&& !InterChromaEn) yoff -= 4;
    if((Vsta.SrcType&0x2)&& !InterChromaEn) xoff -= 4;


     // dual ref, or single record, or dual record and L0 wins, output 0
    if( (Vsta.VmeModes&4)  ||  ((Vsta.VmeModes&2) && (!(mode&3))) || (!(Vsta.VmeModes&2)))
    {// REF 0
        if(bi==0||bi==2)
            Vout->Mvs[k][0] = Best[0][shape];
        else
            Vout->Mvs[k][0] = nullMV;

        if(streamInShape < FHX8)
        {
            // for major shapes, do border check when the record is not replaced 
            // by multicall streamin
            if(MultiReplaced[0][streamInShape] == 1) checkRefBorderMark = false;
        }

        if(checkRefBorderMark)
        {
            if(((Vout->Mvs[k][0].x-(Vsta.Ref[0].x<<2))>>2) == 0)
                Vout->RefBorderMark |= 1;
            else if(((Vout->Mvs[k][0].x-(Vsta.Ref[0].x<<2))>>2) == Vsta.RefW-xoff)
                Vout->RefBorderMark |= 2;
            if(((Vout->Mvs[k][0].y-(Vsta.Ref[0].y<<2))>>2) == 0)
                Vout->RefBorderMark |= 4;
            else if(((Vout->Mvs[k][0].y-(Vsta.Ref[0].y<<2))>>2) == Vsta.RefH-yoff)
                Vout->RefBorderMark |= 8;
        }
    }
    
    checkRefBorderMark = true;

    // REF 1
    if ( (Vsta.VmeModes&4) || ((Vsta.VmeModes&2) && (mode&3)) ) // dual reference windows or dual record and L1 wins
    {
        if(streamInShape < FHX8)
        {
            // for major shapes, do border check when the record is not replaced 
            // by multicall streamin
            if(MultiReplaced[1][streamInShape] == 1) checkRefBorderMark = false;    
        }
        if(bi==1||bi==2)
            Vout->Mvs[k][1] = Best[1][shape];
        else
            Vout->Mvs[k][1] = nullMV;
        if(checkRefBorderMark)
        {
            if(((Vout->Mvs[k][1].x-(Vsta.Ref[1].x<<2))>>2) == 0)
                Vout->RefBorderMark |= 0x10;
            else if(((Vout->Mvs[k][1].x-(Vsta.Ref[1].x<<2))>>2) == Vsta.RefW-xoff)
                Vout->RefBorderMark |= 0x20;
            if(((Vout->Mvs[k][1].y-(Vsta.Ref[1].y<<2))>>2) == 0)
                Vout->RefBorderMark |= 0x40;
            else if(((Vout->Mvs[k][1].y-(Vsta.Ref[1].y<<2))>>2) == Vsta.RefH-yoff)
                Vout->RefBorderMark |= 0x80;
        }
    }
}




#ifdef VMEPERF
/*****************************************************************************************************/
Status MEforGen75::ResetPerf()
/*****************************************************************************************************/
{
    SICTime = 0;
    IMETime = 0;
    FBRTime = 0;
    return 0;
}

/*****************************************************************************************************/
Status MEforGen75::GetPerf(mfxVMEPerf *perf)
/*****************************************************************************************************/
{
    perf->SICTime = SICTime;
    perf->IMETime = IMETime;
    perf->FBRTime = FBRTime;
    return 0;
}
#endif


/*****************************************************************************************************/
void MEforGen75::GetCREComputeClkCount(U8 message_type)
/*****************************************************************************************************/
{
    
    U32 creLatency = 0;

    U8 MsgType = message_type;
    U8 SrcType = Vsta.SrcType&3;
    U8 InterChromaEn = (Vsta.SadType&INTER_CHROMA_MODE) ? 1 : 0;

    //Read from Message for FBR
    U8 MBMode = Vsta.FBRMbInfo;
    U8 SubMBShape = Vsta.FBRSubMbShape;
    U8 hpelEn = (Vsta.VmeModes&VM_HALF_PEL) ? 1 : 0;
    U8 qpelEn = (Vsta.VmeModes&(VM_QUARTER_PEL-VM_HALF_PEL)) ? 1 : 0;
    U8 bmeEn = (Vsta.SadType&DISABLE_BME) ? 0 : 1;

    //derived config for FBR
    U8 numMinors = 0;
    U8 subshape = 0;
    //U8 subMode = 0;

    //Read from Message for SIC
    U8 skipEn = (Vsta.VmeFlags&VF_PB_SKIP_ENABLE) ? 1 : 0;
    U8 intraEn = (VSICsta.IntraComputeType&INTRA_DISABLED) ? 0 : 1;
    U8 ftqEn = (Vsta.SadType&FWD_TX_SKIP_CHECK) ? 1 : 0;
    U8 intraChromaEn = (VSICsta.IntraComputeType==INTRA_LUMA_CHROMA) ? 1 : 0;
    U8 mode16x16En = (Vsta.IntraFlags&INTRA_NO_16X16) ? 0 : 1;
    U8 skipType = (Vsta.VmeModes&VM_SKIP_8X8) ? 1 : 0;
    U8 skipCenterEnables = Vsta.SkipCenterEnables;

    //derived config for SIC
    U8 scEnable = 0;
    U8 nBi8x8SC = 0;

    U8 skipLatency = 0;
    U8 intraLatency = 0;

    CreFlags CREScenarioFlags;

    memset(&CREScenarioFlags, 0, sizeof(CREScenarioFlags));

    U8 scenarioID = 0;
    U8 skipScenarioID = 0;

    if(MsgType == FBR_MSG)
    {
        if(MBMode == MODE_INTER_8X8)
        {
            for(int i = 0; i < 4; i++)
            {
                subshape = (SubMBShape >> (2*i))&0x3;
                if(subshape!=0)
                {
                    numMinors++;
                }
            }
        }

        CREScenarioFlags.hpelEn = hpelEn;
        CREScenarioFlags.messageType = FBR_MSG;
        CREScenarioFlags.bmeEn = (numMinors == 4) ? 0 : bmeEn;
        if(CREScenarioFlags.bmeEn)
        {
            //RTL always steps into HPel state for BME only messages.
            CREScenarioFlags.hpelEn = 1;
        }
        CREScenarioFlags.numMinors = numMinors;
        CREScenarioFlags.qpelEn = qpelEn;
        CREScenarioFlags.chromaInter = InterChromaEn;
        CREScenarioFlags.sourceType = SrcType;

        scenarioID = FindScenarioIdx(CREScenarioFlags);

        creLatency = computeLatencyValue[scenarioID];
    }
    else
    {
        CREScenarioFlags.messageType = SIC_MSG;

        if(skipEn)
        {
            for(int i = 0; i < 4; i++)
            {
                scEnable = (skipCenterEnables >> (2*i))&0x3;
                if(scEnable==0x3)
                {
                    nBi8x8SC++;
                }
            }

            CREScenarioFlags.intraChromaEn = 0;
            CREScenarioFlags.mode16x16En = 0;
            CREScenarioFlags.skipEn = 1;
            CREScenarioFlags.intraEn = 0;
            CREScenarioFlags.ftqEn = ftqEn;
            CREScenarioFlags.numBi8x8SC = nBi8x8SC;
            CREScenarioFlags.skipType = skipType;
            if((skipType == 0)&&(nBi8x8SC==1))
            {
                CREScenarioFlags.numBi8x8SC = 4;
            }
            CREScenarioFlags.chromaInter = InterChromaEn;
            CREScenarioFlags.sourceType = SrcType;

            scenarioID = FindScenarioIdx(CREScenarioFlags);
            skipScenarioID = scenarioID;

            skipLatency = computeLatencyValue[scenarioID];
        }

        if(intraEn)
        {
            CREScenarioFlags.intraChromaEn = intraChromaEn;
            CREScenarioFlags.mode16x16En = mode16x16En;
            CREScenarioFlags.skipEn = 0;
            CREScenarioFlags.skipType = 0;
            CREScenarioFlags.intraEn = 1;
            CREScenarioFlags.ftqEn = 0;
            CREScenarioFlags.numBi8x8SC = 0;

            scenarioID = FindScenarioIdx(CREScenarioFlags);

            intraLatency = computeLatencyValue[scenarioID];
        }

        if(skipEn && intraEn)
        {
            creLatency = (skipLatency > intraLatency) ? skipLatency : intraLatency;
        }
        else
        {
            creLatency = skipEn ? skipLatency : intraLatency;
        }

    }

    Vout->ClockCompute = creLatency;

}

/*****************************************************************************************************/
U8 MEforGen75::FindScenarioIdx(CreFlags& in_CREScenarioFlags)
/*****************************************************************************************************/
{
    // TODO: Need a better search!!!
    for(U8 i = 0; i < NUM_DEFINED_SCENARIOS; i++)
    {
        CreFlags curr_CREScenarioFlags = CREScenarios[i];
        if((in_CREScenarioFlags.messageType    ==    curr_CREScenarioFlags.messageType)&&
            (in_CREScenarioFlags.sourceType    ==    curr_CREScenarioFlags.sourceType)&&
            (in_CREScenarioFlags.intraChromaEn    ==    curr_CREScenarioFlags.intraChromaEn)&&
            (in_CREScenarioFlags.mode16x16En    ==    curr_CREScenarioFlags.mode16x16En)&&
            (in_CREScenarioFlags.skipEn    ==    curr_CREScenarioFlags.skipEn)&&
            (in_CREScenarioFlags.intraEn    ==    curr_CREScenarioFlags.intraEn)&&
            (in_CREScenarioFlags.ftqEn    ==    curr_CREScenarioFlags.ftqEn)&&
            (in_CREScenarioFlags.numBi8x8SC    ==    curr_CREScenarioFlags.numBi8x8SC)&&
            (in_CREScenarioFlags.skipType    ==    curr_CREScenarioFlags.skipType)&&
            (in_CREScenarioFlags.chromaInter    ==    curr_CREScenarioFlags.chromaInter)&&
            (in_CREScenarioFlags.hpelEn    ==    curr_CREScenarioFlags.hpelEn)&&
            (in_CREScenarioFlags.qpelEn    ==    curr_CREScenarioFlags.qpelEn)&&
            (in_CREScenarioFlags.bmeEn    ==    curr_CREScenarioFlags.bmeEn)&&
            (in_CREScenarioFlags.numMinors    ==    curr_CREScenarioFlags.numMinors))
        {
            return i;
        }
    }

    return 0;
}

/*****************************************************************************************************/
void MEforGen75::GetIMEComputeClkCount()
/*****************************************************************************************************/
{
    U32 imeLatency = 0;
    
    //SAD compute
    imeLatency = Vout->NumSUinIME * IME_COMPUTE_CLKS_PER_SU;
    
    //Partitioning
    U8 SearchControl = Vsta.VmeModes&7;
    imeLatency += ((SearchControl == VM_MODE_DDS)||(SearchControl == VM_MODE_DDD)) ? IME_CLKS_BI_PARTITIONING : IME_CLKS_UNI_PARTITIONING;
    
    Vout->ClockCompute = imeLatency;
}

void MEforGen75::Replicate8MVs(U8 interMbMode)
{
     unsigned char i, j;
    U8 MbMode = interMbMode&0x3; //to be used only for source type 16x8
    I16PAIR tempMVArray[16][2];

    //copy over all the MVs from the outmsg to this temp array
    for(i=0;i<2;i++)
    {
        for(j=0;j<16;j++)
        {
            tempMVArray[j][i].x = Vout->Mvs[j][i].x;
            tempMVArray[j][i].y = Vout->Mvs[j][i].y;
        }
    }
    if((Vsta.SrcType&0x3)==3)
    {
        for(i=0;i<2;i++)
        {
            tempMVArray[1][i].x = tempMVArray[0][i].x;
            tempMVArray[1][i].y    = tempMVArray[0][i].y;
            tempMVArray[2][i].x = tempMVArray[0][i].x;
            tempMVArray[2][i].y    = tempMVArray[0][i].y;
            tempMVArray[3][i].x = tempMVArray[0][i].x;
            tempMVArray[3][i].y    = tempMVArray[0][i].y;
        }
    }

    if((Vsta.SrcType&0x3)==1)
    {
        if(MbMode == MODE_INTER_16X8)
        {
            for (i=0; i<16; i++)
            {
                Vout->Mvs[i][0].x = tempMVArray[i/4][0].x;
                Vout->Mvs[i][0].y = tempMVArray[i/4][0].y;
                Vout->Mvs[i][1].x = tempMVArray[i/4][1].x;
                Vout->Mvs[i][1].y = tempMVArray[i/4][1].y;
            }
        }
        else
        {   //MODE_INTER_8X8
            for (i=0; i<16; i++)
            {
                Vout->Mvs[i][0].x = tempMVArray[i%2][0].x;
                Vout->Mvs[i][0].y = tempMVArray[i%2][0].y;
                Vout->Mvs[i][1].x = tempMVArray[i%2][1].x;
                Vout->Mvs[i][1].y = tempMVArray[i%2][1].y;
            }
        }
    }
    else
    {
        for (i=0; i<16; i++)
        {
            Vout->Mvs[i][0].x = tempMVArray[i%4][0].x;
            Vout->Mvs[i][0].y = tempMVArray[i%4][0].y;
            Vout->Mvs[i][1].x = tempMVArray[i%4][1].x;
            Vout->Mvs[i][1].y = tempMVArray[i%4][1].y;
        }
    }
}

void MEforGen75::ReplicateSadMV()
{
    int i;
    int j;
    for (i = 0; i<2; i++) 
    {    
        if(Vsta.SrcType&1)
        {
            // src 16x8
            // 4x4
            for(j=0;j<8;j++)
            {
                Dist[i][B4X4+8+j] = Dist[i][B4X4+j];
                Best[i][B4X4+8+j].x = Best[i][B4X4+j].x;
                Best[i][B4X4+8+j].y = Best[i][B4X4+j].y;
            }

            // 8x4
            for(j=0;j<4;j++)
            {
                Dist[i][B8X4+4+j] = Dist[i][B8X4+j];
                Best[i][B8X4+4+j].x = Best[i][B8X4+j].x;
                Best[i][B8X4+4+j].y = Best[i][B8X4+j].y;
            }
         
            // 4x8
            for(j=0;j<4;j++)
            {
                Dist[i][B4X8+4+j] = Dist[i][B4X8+j];
                Best[i][B4X8+4+j].x = Best[i][B4X8+j].x;
                Best[i][B4X8+4+j].y = Best[i][B4X8+j].y;
            }

            // 8x8
            for(j=0;j<2;j++)
            {
                Dist[i][B8X8+2+j] = Dist[i][B8X8+j];
                Best[i][B8X8+2+j].x = Best[i][B8X8+j].x;
                Best[i][B8X8+2+j].y = Best[i][B8X8+j].y;
            }

            // 16x8
            Dist[i][BHX8+1] = Dist[i][BHX8];
            Best[i][BHX8+1].x = Best[i][BHX8].x;
            Best[i][BHX8+1].y = Best[i][BHX8].y;
            
        }
        if(Vsta.SrcType&2)
        {
            // src 8x16
            // 4x4
            for(j=0;j<12;j++){
                if(j==4) j = 8;
                Dist[i][B4X4+4+j] = Dist[i][B4X4+j];
                Best[i][B4X4+4+j].x = Best[i][B4X4+j].x;
                Best[i][B4X4+4+j].y = Best[i][B4X4+j].y;
            }

            // 8x4
            for(j=0;j<6;j++)
            {
                if(j==2) j = 4;
                Dist[i][B8X4+2+j] = Dist[i][B8X4+j];
                Best[i][B8X4+2+j].x = Best[i][B8X4+j].x;
                Best[i][B8X4+2+j].y = Best[i][B8X4+j].y;
            }

            // 4x8
            for(j=0;j<6;j++)
            {
                if(j==2) j = 4;
                Dist[i][B4X8+2+j] = Dist[i][B4X8+j];
                Best[i][B4X8+2+j].x = Best[i][B4X8+j].x;
                Best[i][B4X8+2+j].y = Best[i][B4X8+j].y;
            }
            
            // 8x8
            for(j=0;j<3;j++)
            {
                if(j==1) j = 2;
                Dist[i][B8X8+1+j] = Dist[i][B8X8+j];
                Best[i][B8X8+1+j].x = Best[i][B8X8+j].x;
                Best[i][B8X8+1+j].y = Best[i][B8X8+j].y;
            }

            // 8x16
            Dist[i][B8XH+1] = Dist[i][B8XH];
            Best[i][B8XH+1].x = Best[i][B8XH].x;
            Best[i][B8XH+1].y = Best[i][B8XH].y;
        }
    }
}

#ifdef VMETESTGEN

void MEforGen75::SetUpDisabledMaskDistortion(int in_MajorMode)
{
    U16 disMaskedDistortion = Vout->MinDist;
    U16 sbDistortion = 0;
    U16 sbDisMaskedDistortion = 0;
    U16 totSbDisMaskedDistortion = 0;
    U8 k, s;

    Vout->Test.IsMaskedDistMin = false;
    if(Vsta.ShapeMask&1)
    { 
        disMaskedDistortion = Dist[0][BHXH] + LutMode[LUTMODE_INTER_16x16];
        disMaskedDistortion = (disMaskedDistortion > 0x3FFF) ? 0x3FFF : disMaskedDistortion;
        Vout->Test.DisMaskIdx = 0;
    }

    if(Vsta.ShapeMask&2)
    { 
        disMaskedDistortion = Dist[0][BHX8] + Dist[0][BHX8+1] + (LutMode[LUTMODE_INTER_16x8]>>(Vsta.SrcType&0x1));
        disMaskedDistortion = (disMaskedDistortion > 0x3FFF) ? 0x3FFF : disMaskedDistortion;
        Vout->Test.DisMaskIdx = 1;
    }

    if(Vsta.ShapeMask&4)
    { 
        disMaskedDistortion = Dist[0][B8XH] + Dist[0][B8XH+1] + LutMode[LUTMODE_INTER_8x16];
        disMaskedDistortion = (disMaskedDistortion > 0x3FFF) ? 0x3FFF : disMaskedDistortion;
        Vout->Test.DisMaskIdx = 2;
    }

    if(Vsta.ShapeMask&8)
    { 
        disMaskedDistortion = Dist[0][B8X8] + Dist[0][B8X8+1] + Dist[0][B8X8+2]+ Dist[0][B8X8+3] + (LutMode[LUTMODE_INTER_8x8q]<<2);
        disMaskedDistortion = (disMaskedDistortion > 0x3FFF) ? 0x3FFF : disMaskedDistortion;
        Vout->Test.DisMaskIdx = 3;
    }

    if (disMaskedDistortion < Vout->MinDist)
    {
        Vout->Test.IsMaskedDistMin = true;
    }

    s = (in_MajorMode&255);
    if (Vsta.ShapeMask&0x70)
    {
        totSbDisMaskedDistortion = 0;
        //Chaitanya - TODO - How to handle this??
        for(k=0;k<4;k++)
        {
            if (Vsta.ShapeMask&0x10)
            {
                sbDisMaskedDistortion = Dist[0][B8X4+(k<<1)] + Dist[0][B8X4+(k<<1)+1];
            }
            else if (Vsta.ShapeMask&0x20)
            {
                sbDisMaskedDistortion = Dist[0][B4X8+(k<<1)] + Dist[0][B4X8+(k<<1)+1];
            }
            else if (Vsta.ShapeMask&0x40)
            {
                sbDisMaskedDistortion = Dist[0][B4X4+(k<<2)] + Dist[0][B4X4+(k<<2)+1] + Dist[0][B4X4+(k<<2)+2] + Dist[0][B4X4+(k<<2)+3];
            }

            totSbDisMaskedDistortion += sbDisMaskedDistortion;
            if (Vout->MbSubShape)
            {
                GetSbDistortion(k, s, sbDistortion);
                s>>=2;
                Vout->Test.IsMaskedDistMin |= (sbDisMaskedDistortion < sbDistortion) ? true : false;
            }
        }

    }

    if ((Vsta.ShapeMask&0x70) && (!Vout->MbSubShape))
    {
        Vout->Test.IsMaskedDistMin |= (totSbDisMaskedDistortion < Vout->MinDist) ? true : false;
    }
}

void MEforGen75::GetSbDistortion(U8 in_SubBlk, U8 in_SubShape, U16 &in_SubDistortion)
{
    switch(in_SubShape&3)
    {
    case 0:
        in_SubDistortion = Dist[0][B8X8+in_SubBlk]; 
        break;
    case 1:
        in_SubDistortion = Dist[0][B8X4+(in_SubBlk<<1)] + Dist[0][B8X4+(in_SubBlk<<1)+1];
        break;
    case 2:
        in_SubDistortion = Dist[0][B4X8+(in_SubBlk<<1)] + Dist[0][B4X8+(in_SubBlk<<1)+1];
        break;
    case 3:
        in_SubDistortion = Dist[0][B4X4+(in_SubBlk<<2)] + Dist[0][B4X4+(in_SubBlk<<2)+1] 
                            + Dist[0][B4X4+(in_SubBlk<<2)+2] + Dist[0][B4X4+(in_SubBlk<<2)+3];
        break;
    }
}
#endif