//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2004-2016 Intel Corporation. All Rights Reserved.
//

#if defined(__GNUC__)
    #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

#if PIXBITS == 8

#define PIXTYPE Ipp8u
#define COEFFSTYPE Ipp16s
#define H264ENC_MAKE_NAME(NAME) NAME##_8u16s

#elif PIXBITS == 16

#define PIXTYPE Ipp16u
#define COEFFSTYPE Ipp32s
#define H264ENC_MAKE_NAME(NAME) NAME##_16u32s

#else

void H264EncoderFakeFunction() { UNSUPPORTED_PIXBITS; }

#endif //PIXBITS

#define Clip3(Min, Max, Value) ((Value) < (Min)) ? (Min) : ((Value) > (Max)) ? (Max) : (Value)

#define H264BsBaseType H264ENC_MAKE_NAME(H264BsBase)
#define H264BsRealType H264ENC_MAKE_NAME(H264BsReal)
#define H264BsFakeType H264ENC_MAKE_NAME(H264BsFake)

/*Status H264ENC_MAKE_NAME(H264BsReal_Create)(
    H264BsRealType* state)
{
    return H264ENC_MAKE_NAME(H264BsReal_Create)(state, NULL, 0);
}

Status H264ENC_MAKE_NAME(H264BsFake_Create)(
    H264BsFakeType* state);
{
    return H264ENC_MAKE_NAME(H264BsFake_Create)(state, NULL, 0);
}*/

// ---------------------------------------------------------------------------
//  CBaseBitstream::CBaseBitstream()
//      Constructs a new object.  Sets base pointer to point to the given
//      bitstream buffer.  Neither the encoder or the decoder allocates
//      memory for the bitstream buffer.
//      pb      : pointer to input bitstream
//      maxsize : size of bitstream
// ---------------------------------------------------------------------------

Status H264ENC_MAKE_NAME(H264BsReal_Create)(
    H264BsRealType* state,
    Ipp8u* const pb,
    const Ipp32u maxsize,
    Ipp32s chroma_format_idc,
    Status &plr)
{
    H264BsRealType* bs = (H264BsRealType *)state;
    bs->m_base.m_pbsBase   = pb;
    bs->m_base.m_pbs       = pb;
    bs->m_base.m_bitOffset = 0;
    bs->m_base.m_maxBsSize = maxsize;
    bs->m_base.pCabacState = 0;
    bs->m_base.m_pStoredCabacState = 0;
    bs->m_base.m_numBins = 0;
    bs->m_base.m_storedNumBins = 0;

    bs->m_pbsRBSPBase = pb;
    plr = UMC_OK;

    if (chroma_format_idc)
        bs->num8x8Cshift2 = chroma_format_idc - 1;
    else
        bs->num8x8Cshift2 = 0;

    // no need to allocate CABAC state if there is no real bitstream buffer
    if (pb == 0)
        return UMC_OK;

    Ipp32u sizeOfCabacState = 0;
    IppStatus ippSts = ippiCABACGetSize_H264(&sizeOfCabacState);
    if (ippSts != ippStsNoErr)
        return UMC_ERR_ALLOC;
    sizeOfCabacState += CABAC_CONTEXT_ARRAY_LEN - 460 + 16;

    bs->m_base.pCabacState = (IppvcCABACState *)ippMalloc(sizeOfCabacState);
    if (bs->m_base.pCabacState == 0)
        return UMC_ERR_ALLOC;

    bs->m_base.m_cabacStateSize = sizeOfCabacState;
    
    bs->m_base.m_pStoredCabacState = (IppvcCABACState *)ippMalloc(sizeOfCabacState);
    if (bs->m_base.m_pStoredCabacState == 0)
        return UMC_ERR_ALLOC;

    memset(bs->m_base.pCabacState, 0, sizeof(Ipp32u*) * 3);

    return UMC_OK;
}

void H264ENC_MAKE_NAME(H264BsReal_Destroy)(H264BsRealType* bs)
{
    if (bs->m_base.pCabacState)
        ippiCABACFree_H264(bs->m_base.pCabacState);
    //{
    //    ippFree(bs->m_base.pCabacState);
    //    bs->m_base.pCabacState = NULL;
    //}
    if (bs->m_base.m_pStoredCabacState)
    {
        ippFree(bs->m_base.m_pStoredCabacState);
        bs->m_base.m_pStoredCabacState = 0;
    }
}

void H264ENC_MAKE_NAME(H264BsReal_StoreIppCABACState)(
    void* state)
{
    H264BsRealType* bs = (H264BsRealType *)state;
    Ipp32u* bsData      = *((Ipp32u**)bs->m_base.pCabacState) - 1;
    Ipp32u* bsDataStart = *((Ipp32u**)bs->m_base.pCabacState + 1);
    Ipp32u* bsDataEnd   = *((Ipp32u**)bs->m_base.pCabacState + 2);

    MFX_INTERNAL_CPY(bs->m_base.m_pStoredCabacState, bs->m_base.pCabacState, bs->m_base.m_cabacStateSize);
    bs->m_base.m_storedNumBins = bs->m_base.m_numBins;

    if(bsData)
    {
        if(bsData >= bsDataStart)
            bs->m_base.m_storedBsLastBytes[0] = bsData[0];
        if(bsData < bsDataEnd)
            bs->m_base.m_storedBsLastBytes[1] = bsData[1];
    }
}

void H264ENC_MAKE_NAME(H264BsReal_RestoreIppCABACState)(
    void* state)
{
    H264BsRealType* bs = (H264BsRealType *)state;
    Ipp32u* bsData      = *((Ipp32u**)bs->m_base.m_pStoredCabacState) - 1;
    Ipp32u* bsDataStart = *((Ipp32u**)bs->m_base.m_pStoredCabacState + 1);
    Ipp32u* bsDataEnd   = *((Ipp32u**)bs->m_base.m_pStoredCabacState + 2);

    MFX_INTERNAL_CPY(bs->m_base.pCabacState, bs->m_base.m_pStoredCabacState, bs->m_base.m_cabacStateSize);
    bs->m_base.m_numBins = bs->m_base.m_storedNumBins;
    
    if(bsData)
    {
        if(bsData >= bsDataStart)
            bsData[0] = bs->m_base.m_storedBsLastBytes[0];
        if(bsData < bsDataEnd)
            bsData[1] = bs->m_base.m_storedBsLastBytes[1];
    }
}

void H264ENC_MAKE_NAME(H264BsReal_InitializeAEE_CABAC)(
    void* state)
{
    H264BsRealType* bs = (H264BsRealType *)state;

    ippiCABACInitAEEOwn_H264(
        bs->m_base.pCabacState,
        bs->m_base.m_pbsBase,
        bs->m_base.m_bitOffset + (Ipp32u)(bs->m_base.m_pbs - bs->m_base.m_pbsBase) * 8,
        bs->m_base.m_maxBsSize);
}

void H264ENC_MAKE_NAME(H264BsFake_InitializeAEE_CABAC)(
    void* state)
{
    state;
}

Ipp32u H264ENC_MAKE_NAME(H264BsReal_GetNotStoredStreamSize_CABAC)(
    void* state)
{
    H264BsRealType* bs = (H264BsRealType *)state;
    Ipp32u n0 = 0, n1 = 0;

    ippiCABACGetStreamSize_H264(&n0, bs->m_base.m_pStoredCabacState);
    ippiCABACGetStreamSize_H264(&n1, bs->m_base.pCabacState);

    return (n1 - n0);
}

Status H264ENC_MAKE_NAME(H264BsFake_Create)(
    H264BsFakeType* state,
    Ipp8u* const pb,
    const Ipp32u maxsize,
    Ipp32s chroma_format_idc,
    Status &plr)
{
    H264BsFakeType* bs = (H264BsFakeType *)state;
    bs->m_base.m_pbsBase   = pb;
    bs->m_base.m_pbs       = pb;
    bs->m_base.m_bitOffset = 0;
    bs->m_base.m_maxBsSize = maxsize;
    bs->m_base.pCabacState = 0;
    bs->m_base.m_pStoredCabacState = 0;
    bs->m_base.m_numBins = 0;

    bs->m_pbsRBSPBase = pb;
    plr = UMC_OK;

    if (chroma_format_idc)
        bs->num8x8Cshift2 = chroma_format_idc - 1;
    else
        bs->num8x8Cshift2 = 0;

    return UMC_OK;
}

// ---------------------------------------------------------------------------
//  [ENC] CBaseBitstream::PutBits()
//      This is a special debug version that Detects Start Code Emulations.
//      Appends bits into the bitstream buffer.  Supports only up to 24 bits.
//
//      code        : code to be inserted into the bitstream
//      code_length : length of the given code in number of bits
// ---------------------------------------------------------------------------

void H264ENC_MAKE_NAME(H264BsReal_PutBit)(
    void* state,
    Ipp32u code)
{
    H264BsRealType* bs = (H264BsRealType *)state;
    if (!bs->m_base.m_bitOffset) {
        bs->m_base.m_pbs[0] = (Ipp8u)(code<<7);
        bs->m_base.m_bitOffset = 1;
    } else
    if (bs->m_base.m_bitOffset == 7) {
        bs->m_base.m_pbs[0] =
            (Ipp8u)(bs->m_base.m_pbs[0] | (code & 1));
        bs->m_base.m_pbs++;
        bs->m_base.m_bitOffset = 0;
    } else {
        if (code & 1)
            bs->m_base.m_pbs[0] = (Ipp8u)(bs->m_base.m_pbs[0] | (0x01 << (7 - bs->m_base.m_bitOffset)));
        bs->m_base.m_bitOffset++;
    }
}

void H264ENC_MAKE_NAME(H264BsReal_PutBits)(
    void* state,
    Ipp32u code,
    Ipp32u length)
{
    H264BsRealType* bs = (H264BsRealType *)state;
    // make sure that the number of bits given is <= 24
    // clear any nonzero bits in upper part of code
    VM_ASSERT(length <= 24);
    code <<= (32 - length);

    //bs->m_base.m_pbs[bs->m_base.m_bitOffset] = 0; // need to pre-clean next untouched byte
    if (!bs->m_base.m_bitOffset) {
        bs->m_base.m_pbs[0] = (Ipp8u)(code >> 24);
        bs->m_base.m_pbs[1] = (Ipp8u)(code >> 16);
    } else {
        // shift field so that the given code begins at the current bit
        // offset in the most significant byte of the 32-bit word
        length += bs->m_base.m_bitOffset;
        code >>= bs->m_base.m_bitOffset;
        // write bytes back into memory, big-endian
        bs->m_base.m_pbs[0] = (Ipp8u)((code >> 24) | bs->m_base.m_pbs[0]);
        bs->m_base.m_pbs[1] = (Ipp8u)(code >> 16);
    }

    if (length > 16)
    {
        bs->m_base.m_pbs[2] = (Ipp8u)(code >> 8);
        bs->m_base.m_pbs[3] = (Ipp8u)(code);
    }

    // update bitstream pointer and bit offset
    bs->m_base.m_pbs += (length >> 3);
    bs->m_base.m_bitOffset = (length & 7);
}

// ---------------------------------------------------------------------------
//  [ENC] CBaseBitstream::PutVLCCode()
//      Writes one Exp-Golomb code to the bitstream.  Automatically calculates
//      the required code length.  Use only when this can not be implicitly
//      known to the calling code, requiring length calculation anyway.
//
//      code        : code to be inserted into the bitstream
// ---------------------------------------------------------------------------

Ipp32u H264ENC_MAKE_NAME(H264BsReal_PutVLCCode)(
    void* state,
    Ipp32u code)
{
    Ipp32s i;
    if (code == 0) {
      H264ENC_MAKE_NAME(H264BsReal_PutBit)(state, 1);
      return 1;
    }

    code += 1;

#if defined(__i386__) && defined(__GNUC__) && (__GNUC__ > 3) && !defined(__INTEL_COMPILER)
    i = 31 - __builtin_clz(code);
#elif defined(__INTEL_COMPILER) && (defined(__i386__) || defined(WIN32)) && !defined(WIN64)
    i = _bit_scan_reverse( code );
#elif defined(_MSC_VER) && (_MSC_FULL_VER >= 140050110) && !defined(__INTEL_COMPILER)
    unsigned long idx;
    _BitScanReverse(&idx, (unsigned long)code);
    i = (Ipp32s)idx;
#else
    i = -1;
    while (code >> (i + 1)) {
        i++;
    }
#endif

    H264ENC_MAKE_NAME(H264BsReal_PutBits)(state, 0, i);
    H264ENC_MAKE_NAME(H264BsReal_PutBits)(state, code, i + 1);
    return 2*i+1;

}

void H264ENC_MAKE_NAME(H264BsReal_SaveCABACState)(
    void* state)
{
    H264BsRealType* bs = (H264BsRealType *)state;
    MFX_INTERNAL_CPY(&bs->context_array_copy[0], &bs->m_base.context_array[0], CABAC_CONTEXT_ARRAY_LEN * sizeof(CABAC_CONTEXT));

    bs->m_pbs_copy = bs->m_base.m_pbs;
    bs->m_bitOffset_copy = bs->m_base.m_bitOffset;
}

void H264ENC_MAKE_NAME(H264BsReal_RestoreCABACState)(
    void* state)
{
    H264BsRealType* bs = (H264BsRealType *)state;
    MFX_INTERNAL_CPY(&bs->m_base.context_array[0], &bs->context_array_copy[0], CABAC_CONTEXT_ARRAY_LEN*sizeof(CABAC_CONTEXT));
    bs->m_base.m_pbs = bs->m_pbs_copy;
    bs->m_base.m_bitOffset =  bs->m_bitOffset_copy;
}

void H264ENC_MAKE_NAME(H264BsReal_ResetBitStream_CABAC)(
    void* state)
{
    state;
}

void H264ENC_MAKE_NAME(H264BsReal_EncodeSingleBin_CABAC)(
    void* state,
    Ipp32u ctx,
    Ipp32s code)
{
    H264BsRealType* bs = (H264BsRealType *)state;
    ippiCABACEncodeBin_H264(ctx, code, bs->m_base.pCabacState);
    bs->m_base.m_numBins++;

    /* old implementation
    Ipp8u pStateIdx = *ctx;
    Ipp8u state;
    register Ipp32u codIOffset = bs->m_base.m_lcodIOffset;
    register Ipp32u codIRange = bs->m_base.m_lcodIRange;
    Ipp32u codIRangeLPS = rangeTabLPS[pStateIdx][((codIRange >> 6) & 0x03)];

    codIRange -= codIRangeLPS;

    state = pStateIdx>>6;
    pStateIdx = transTbl[code][pStateIdx];
    if (code != state ){
        Ipp8u Renorm;
        codIOffset += (codIRange<<bs->m_base.m_nReadyBits);
        codIRange = codIRangeLPS;

        Renorm = renormTAB[(codIRangeLPS>>3)&0x1f];
        bs->m_base.m_nReadyBits -= Renorm;
        codIRange <<= Renorm;
        if( codIOffset >= ENC_M_FULL ){ //carry
            codIOffset -= ENC_M_FULL;
            bs->m_base.m_nRegister++;
            while( bs->m_base.m_nOutstandingChunks > 0 ){
                H264ENC_MAKE_NAME(H264BsReal_WriteTwoBytes_CABAC)(bs, 0);
                bs->m_base.m_nOutstandingChunks--;
            }
        }
        if( bs->m_base.m_nReadyBits > 0 ){
            *ctx = pStateIdx;
            bs->m_base.m_lcodIOffset = codIOffset;
            bs->m_base.m_lcodIRange = codIRange;
            return;  //no output
        }
    }else{
        if( codIRange >= ENC_M_QUARTER ){
            *ctx = pStateIdx;
            bs->m_base.m_lcodIOffset = codIOffset;
            bs->m_base.m_lcodIRange = codIRange;
            return;
        }
        codIRange <<= 1;
        bs->m_base.m_nReadyBits--;
        if( bs->m_base.m_nReadyBits > 0 ){
            *ctx = pStateIdx;
            bs->m_base.m_lcodIOffset = codIOffset;
            bs->m_base.m_lcodIRange = codIRange;
            return;
        }
    }

    Ipp32s L = (codIOffset>>ENC_B_BITS)& ((1<<ENC_M_BITS)-1);
    codIOffset = (codIOffset<<ENC_M_BITS)&(ENC_M_FULL-1);
    if( L < ((1<<ENC_M_BITS)-1) ){
        while( bs->m_base.m_nOutstandingChunks > 0 ){
            H264ENC_MAKE_NAME(H264BsReal_WriteTwoBytes_CABAC)(state_, 0xffff);
            bs->m_base.m_nOutstandingChunks--;
        }
        H264ENC_MAKE_NAME(H264BsReal_WriteTwoBytes_CABAC)(state_, L);
    }else{
        bs->m_base.m_nOutstandingChunks++;
    }
    bs->m_base.m_nReadyBits += ENC_M_BITS;

    *ctx = pStateIdx;
    bs->m_base.m_lcodIOffset = codIOffset;
    bs->m_base.m_lcodIRange = codIRange;
    */
}

void H264ENC_MAKE_NAME(H264BsReal_EncodeSingleBinExtra_CABAC)(
    void* state,
    Ipp32u ctx,
    Ipp32s code)
{
    const int position = 459; // ctx to be used
    H264BsRealType* bs = (H264BsRealType *)state;
    // hacking to avoid 460 contents limitation: copy extra ctx to valid area
    Ipp32u sizeOfCabacState = 0;
    ippiCABACGetSize_H264(&sizeOfCabacState);
    Ipp8u* contexts = (Ipp8u*)(bs->m_base.pCabacState) + sizeOfCabacState - 460;

    Ipp8u savedCtx = contexts[position];
    contexts[position] = contexts[ctx];

    ippiCABACEncodeBin_H264(position, code, bs->m_base.pCabacState);
    bs->m_base.m_numBins++;

    contexts[ctx] = contexts[position];
    contexts[position] = savedCtx;
}

void H264ENC_MAKE_NAME(H264BsReal_EncodeBins_CABAC)(
    void* state,
    Ipp32u ctx,
    Ipp32u code,
    Ipp32s len)
{
    H264BsRealType* bs = (H264BsRealType *)state;
    bs->m_base.m_numBins += len;
    while (len > 0)
    {
        len--;
        Ipp32u c = (code >> len) & 1;
        ippiCABACEncodeBin_H264(ctx, c, bs->m_base.pCabacState);
    }
}

void H264ENC_MAKE_NAME(H264BsFake_EncodeBins_CABAC)(
    H264BsFakeType* state,
    Ipp8u* ctx,
    Ipp32u code,
    Ipp32s len)
{
    H264BsFakeType* bs = (H264BsFakeType *)state;
    register Ipp8u pStateIdx = *ctx;
    register Ipp32s bits=0;

    while (len>0)
    {
        len--;
        Ipp32u c = (code>>len)&1;
        bits += ( c ? p_bits[pStateIdx^64] : p_bits[pStateIdx] );
        pStateIdx = transTbl[c][pStateIdx];
    }

    *ctx = pStateIdx;
    bs->m_base.m_bitOffset += bits;
}

void H264ENC_MAKE_NAME(H264BsReal_EncodeFinalSingleBin_CABAC)(
    void* state,
    Ipp32s code)
{
    H264ENC_MAKE_NAME(H264BsReal_EncodeSingleBin_CABAC)( state, 276, code );
}

void H264ENC_MAKE_NAME(H264BsReal_EncodeBypass_CABAC)(
    void* state,
    Ipp32s code)
{
    H264BsRealType* bs = (H264BsRealType *)state;
    ippiCABACEncodeBinBypass_H264(code, bs->m_base.pCabacState);
    bs->m_base.m_numBins++;

    /*
     * old EncodeBypass_CABAC implementation
     *

    Ipp32u codIOffset = bs->m_base.m_lcodIOffset;
    codIOffset *= 2;

    if (code)
        codIOffset += bs->m_base.m_lcodIRange;

    if (codIOffset >= ENC_FULL_RANGE)
    {
        H264ENC_MAKE_NAME(H264BsReal_WriteOutstandingOneBit_CABAC)(bs);
        codIOffset -= ENC_FULL_RANGE;
    }
    else if (codIOffset < ENC_HALF_RANGE)
    {
        H264ENC_MAKE_NAME(H264BsReal_WriteOutstandingZeroBit_CABAC)(bs);
    }
    else
    {
        bs->m_base.m_nOutstandingBits++;
        codIOffset -= ENC_HALF_RANGE;
    }

    bs->m_base.m_lcodIOffset = codIOffset;
    */
}

void H264ENC_MAKE_NAME(H264BsReal_TerminateEncode_CABAC)(
    void* state)
{
    H264BsRealType* bs = (H264BsRealType *)state;

    Ipp32u streamBytes;
    VM_ASSERT(bs->m_base.pCabacState);
    ippiCABACTerminateSlice_H264( &streamBytes, bs->m_base.pCabacState );

    bs->m_base.m_pbs = bs->m_base.m_pbsBase + streamBytes;
    bs->m_base.m_bitOffset = 0;
}

void H264ENC_MAKE_NAME(H264BsFake_TerminateEncode_CABAC)(
    void* state)
{
    H264ENC_UNREFERENCED_PARAMETER(state);
}

void H264ENC_MAKE_NAME(H264BsReal_EncodeUnaryRepresentedSymbol_CABAC)(
    H264BsRealType* state,
    Ipp32u ctx,
    Ipp32s ctxIdx,
    Ipp32s code,
    Ipp32s suppremum)
{
    if (code == 0)
    {
        H264ENC_MAKE_NAME(H264BsReal_EncodeSingleBin_CABAC)(state, ctx, 0);
    }
    else
    {
        H264ENC_MAKE_NAME(H264BsReal_EncodeSingleBin_CABAC)(state, ctx, 1);
        Ipp32s temp=code;
        while ((--temp) > 0)
            H264ENC_MAKE_NAME(H264BsReal_EncodeSingleBin_CABAC)(state, ctx + ctxIdx, 1);
        if (code < suppremum)
            H264ENC_MAKE_NAME(H264BsReal_EncodeSingleBin_CABAC)(state, ctx + ctxIdx, 0);
    }
}

void H264ENC_MAKE_NAME(H264BsReal_EncodeExGRepresentedLevels_CABAC)(
    void* state,
    Ipp32u ctx,
    Ipp32s code)
{
    /* 2^(i+1)-2 except last=2^(i+1)-1 */
    //static Ipp32u c[14] = {0,2,6,14,30,62,126,254,510,1022,2046,4094,8190,16383};

    if (code == 0)
    {
        H264ENC_MAKE_NAME(H264BsReal_EncodeSingleBin_CABAC)(state, ctx, 0);
    }
    else
    {
        if (code < 13)
        {
            H264ENC_MAKE_NAME(H264BsReal_EncodeBins_CABAC)(state, ctx, (2 << code) - 2, code + 1);
        }
        else
        {
            H264ENC_MAKE_NAME(H264BsReal_EncodeBins_CABAC)(state, ctx, 16383, 13);
            H264ENC_MAKE_NAME(H264BsReal_EncodeExGRepresentedSymbol_CABAC)(state, code - 13, 0);
        }
    }
}

void H264ENC_MAKE_NAME(H264BsReal_EncodeExGRepresentedMVS_CABAC)(
    void* state,
    Ipp32u ctx,
    Ipp32s code)
{

    if (code == 0)
    {
        H264ENC_MAKE_NAME(H264BsReal_EncodeSingleBin_CABAC)(state, ctx, 0);
        return;
    }
    else
    {
        Ipp32s inc = 1;
        H264ENC_MAKE_NAME(H264BsReal_EncodeSingleBin_CABAC)(state, ctx, 1);
        if(code > 1) {
            H264ENC_MAKE_NAME(H264BsReal_EncodeSingleBin_CABAC)(state, ctx + 1, 1);
            inc = 2;
            if(code > 2) {
                H264ENC_MAKE_NAME(H264BsReal_EncodeSingleBin_CABAC)(state, ctx + 2, 1);
                inc = 3;
                if (code > 3)
                {
                    if (code < 8)
                    {
                        H264ENC_MAKE_NAME(H264BsReal_EncodeBins_CABAC)(state, ctx+3, (1 << (code-2)) - 2, code - 2); // '10' for code==4, 110 for 5, ..., 11110 for 7
                        return;
                    }
                    else
                    {
                        H264ENC_MAKE_NAME(H264BsReal_EncodeBins_CABAC)(state, ctx+3, 0x1f, 5);
                        H264ENC_MAKE_NAME(H264BsReal_EncodeExGRepresentedSymbol_CABAC)(state, code - 8, 3);
                        return;
                    }
                }
            }
        }
        H264ENC_MAKE_NAME(H264BsReal_EncodeSingleBin_CABAC)(state, ctx + inc, 0);
    }
}

void H264ENC_MAKE_NAME(H264BsFake_EncodeExGRepresentedMVS_CABAC)(
    void* state,
    Ipp32u ctx,
    Ipp32s code)
{
    H264BsFakeType* bs = (H264BsFakeType *)state;
    if (code == 0)
    {
        H264ENC_MAKE_NAME(H264BsFake_EncodeSingleBin_CABAC)(state, ctx, 0);
        return;
    }
    else
    {
        Ipp32s tempval, tempindex;
        //Ipp32s bin=1;
        Ipp32s inc;
        H264ENC_MAKE_NAME(H264BsFake_EncodeSingleBin_CABAC)(state, ctx, 1);
        tempval = code;
        tempindex = 1;
        inc = 1;
        while (((--tempval) > 0) && (++tempindex <= 8))
        {
            H264ENC_MAKE_NAME(H264BsFake_EncodeSingleBin_CABAC)(state, ctx + inc, 1);
            if (inc < 3)
                inc++;
            //if ((++bin) == 2)
                //inc++;
            //if (bin == 3)
                //inc++;
        }

        if (code < 8)
        {
            H264ENC_MAKE_NAME(H264BsFake_EncodeSingleBin_CABAC)(state, ctx + inc, 0);
        }
        else
        {
            if (code >= 65536 - 1 + 8)
            {
                bs->m_base.m_bitOffset += 256 * 32;
                code >>= 16;
            }

            if (code >= 256 - 1 + 8)
            {
                bs->m_base.m_bitOffset += 256 * 16;
                code >>= 8;
            }

            bs->m_base.m_bitOffset += bitcount_EG3[code];
        }
    }
}

void H264ENC_MAKE_NAME(H264BsReal_EncodeExGRepresentedSymbol_CABAC)(
    void* state,
    Ipp32s code,
    Ipp32s log2ex)
{
    for (;;)
    {
        if (code>= (1<<log2ex))
        {
            H264ENC_MAKE_NAME(H264BsReal_EncodeBypass_CABAC)(state, 1);
            code -= (1<<log2ex);
            log2ex++;
        }
        else
        {
            H264ENC_MAKE_NAME(H264BsReal_EncodeBypass_CABAC)(state, 0);
            while (log2ex--)
                H264ENC_MAKE_NAME(H264BsReal_EncodeBypass_CABAC)(state, (code >> log2ex) & 1);
            return;
        }
    }
}

void H264ENC_MAKE_NAME(H264BsReal_InitializeContextVariablesIntra_CABAC)(
    void* state,
    Ipp32s SliceQPy)
{
    H264BsRealType* bs = (H264BsRealType *)state;
    bs->m_base.m_numBins = 0;

    ippiCABACInit_H264(
        bs->m_base.pCabacState,
        bs->m_base.m_pbsBase,
        bs->m_base.m_bitOffset + (Ipp32u)(bs->m_base.m_pbs - bs->m_base.m_pbsBase) * 8,
        bs->m_base.m_maxBsSize,
        SliceQPy,
        -1);

    if (bs->m_base.m_bitOffset)
    {
        bs->m_base.m_bitOffset = 0;
        bs->m_base.m_pbs++;
    }

    // hacking to avoid 460 contents limitation: init svc contexts
    Ipp32u sizeOfCabacState = 0;
    ippiCABACGetSize_H264(&sizeOfCabacState);
    Ipp8u* extra_contexts = (Ipp8u*)bs->m_base.pCabacState + sizeOfCabacState;
    InitIntraContextExtra_H264( extra_contexts, SliceQPy);

    ippiCABACGetContextsWrap_H264(bs->m_base.context_array, 0, CABAC_CONTEXT_ARRAY_LEN, bs->m_base.pCabacState);
}

// ---------------------------------------------------------------------------

void H264ENC_MAKE_NAME(H264BsReal_InitializeContextVariablesInter_CABAC)(
    void* state,
    Ipp32s SliceQPy,
    Ipp32s cabac_init_idc)
{
    H264BsRealType* bs = (H264BsRealType *)state;
    bs->m_base.m_numBins = 0;

    ippiCABACInit_H264(
        bs->m_base.pCabacState,
        bs->m_base.m_pbsBase,
        bs->m_base.m_bitOffset + (Ipp32u)(bs->m_base.m_pbs - bs->m_base.m_pbsBase) * 8,
        bs->m_base.m_maxBsSize,
        SliceQPy,
        cabac_init_idc);

    if (bs->m_base.m_bitOffset)
    {
        bs->m_base.m_bitOffset = 0;
        bs->m_base.m_pbs++;
    }

    // hacking to avoid 460 contents limitation: init svc contexts
    Ipp32u sizeOfCabacState = 0;
    ippiCABACGetSize_H264(&sizeOfCabacState);
    Ipp8u* extra_contexts = (Ipp8u*)bs->m_base.pCabacState + sizeOfCabacState;
    InitInterContextExtra_H264(extra_contexts, SliceQPy);

    ippiCABACGetContextsWrap_H264(bs->m_base.context_array, 0, CABAC_CONTEXT_ARRAY_LEN, bs->m_base.pCabacState);
}


#undef H264BsFakeType
#undef H264BsBaseType
#undef H264BsType
#undef H264ENC_MAKE_NAME
#undef COEFFSTYPE
#undef PIXTYPE
