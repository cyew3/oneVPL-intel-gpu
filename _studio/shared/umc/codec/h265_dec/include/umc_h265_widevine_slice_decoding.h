//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2012-2016 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"
#if defined (UMC_ENABLE_H265_VIDEO_DECODER)

#ifndef __UMC_H265_WIDEVINE_SLICE_DECODING_H
#define __UMC_H265_WIDEVINE_SLICE_DECODING_H

#include "umc_h265_slice_decoding.h"
#include "umc_h265_widevine_decrypter.h"

namespace UMC_HEVC_DECODER
{

class H265WidevineSlice : public H265Slice
{
public:
    // Default constructor
    H265WidevineSlice();
    // Destructor
    virtual ~H265WidevineSlice(void);

    virtual void SetDecryptParameters(DecryptParametersWrapper* pDecryptParameters);

    // Decode slice header and initializ slice structure with parsed values
    virtual bool Reset(PocDecoding * pocDecoding);
/*    // Set current slice number
    void SetSliceNumber(Ipp32s iSliceNumber);

    // Initialize CABAC context depending on slice type
    void InitializeContexts();
*/
    // Parse beginning of slice header to get PPS ID
    virtual Ipp32s RetrievePicParamSetNumber();
/*
    //
    // method(s) to obtain slice specific information
    //

    // Obtain pointer to slice header
    const H265SliceHeader *GetSliceHeader() const {return &m_SliceHeader;}
    H265SliceHeader *GetSliceHeader() {return &m_SliceHeader;}
    // Obtain bit stream object
    H265Bitstream *GetBitStream(){return &m_BitStream;}
    Ipp32s GetFirstMB() const {return m_iFirstMB;}
    // Obtain current picture parameter set
    const H265PicParamSet *GetPicParam() const {return m_pPicParamSet;}
    void SetPicParam(const H265PicParamSet * pps)
    {
        m_pPicParamSet = pps;
        if (m_pPicParamSet)
            m_pPicParamSet->IncrementReference();
    }
    // Obtain current sequence parameter set
    const H265SeqParamSet *GetSeqParam(void) const {return m_pSeqParamSet;}
    void SetSeqParam(const H265SeqParamSet * sps)
    {
        m_pSeqParamSet = sps;
        if (m_pSeqParamSet)
            m_pSeqParamSet->IncrementReference();
    }

    // Obtain current destination frame
    H265DecoderFrame *GetCurrentFrame(void) const {return m_pCurrentFrame;}
    void SetCurrentFrame(H265DecoderFrame * pFrame){m_pCurrentFrame = pFrame;}

    // Obtain slice number
    Ipp32s GetSliceNum(void) const {return m_iNumber;}
    // Obtain maximum of macroblock
    Ipp32s GetMaxMB(void) const {return m_iMaxMB;}
    void SetMaxMB(Ipp32s x) {m_iMaxMB = x;}
*/
    // Build reference lists from slice reference pic set. HEVC spec 8.3.2
    UMC::Status UpdateReferenceList(H265DBPList *dpb);
/*
    bool IsError() const {return m_bError;}

public:

    MemoryPiece m_source;                                 // (MemoryPiece *) pointer to owning memory piece
*/
public:  // DEBUG !!!! should remove dependence

    // Initialize slice structure to default values
    virtual void Reset();

    // Release resources
    virtual void Release();

    // Decoder slice header and calculate POC
    virtual bool DecodeSliceHeader(PocDecoding * pocDecoding);
/*
    H265SliceHeader m_SliceHeader;                              // (H265SliceHeader) slice header
    H265Bitstream m_BitStream;                                  // (H265Bitstream) slice bit stream
*/
private:
    DecryptParametersWrapper m_DecryptParams;

    /*const H265PicParamSet* m_pPicParamSet;                      // (H265PicParamSet *) pointer to array of picture parameters sets
    const H265SeqParamSet* m_pSeqParamSet;                      // (H265SeqParamSet *) pointer to array of sequence parameters sets
public:
    H265DecoderFrame *m_pCurrentFrame;        // (H265DecoderFrame *) pointer to destination frame

    Ipp32s m_iNumber;                                           // (Ipp32s) current slice number
    Ipp32s m_iFirstMB;                                          // (Ipp32s) first MB number in slice
    Ipp32s m_iMaxMB;                                            // (Ipp32s) last unavailable  MB number in slice

    Ipp16u m_WidevineStatusReportNumber;

    CUProcessInfo processInfo;

    bool m_bError;                                              // (bool) there is an error in decoding

    // memory management tools
    DecodingContext        *m_context;

public:

    ReferencePictureSet*  getRPS() { return &m_SliceHeader.m_rps; }

    const ReferencePictureSet*  getRPS() const { return &GetSliceHeader()->m_rps; }

    int getNumRefIdx(EnumRefPicList e) const    { return m_SliceHeader.m_numRefIdx[e]; }

    // Returns number of used references in RPS
    int getNumRpsCurrTempList() const;

    Ipp32s m_tileCount;
    Ipp32u *m_tileByteLocation;

    Ipp32s getTileLocationCount() const   { return m_tileCount; }
    void allocateTileLocation(Ipp32s val)
    {
        if (m_tileCount < val)
            delete[] m_tileByteLocation;

        m_tileCount = val;
        m_tileByteLocation = new Ipp32u[val];
    }

    // For dependent slice copy data from another slice
    void CopyFromBaseSlice(const H265Slice * slice);*/
};

} // namespace UMC_HEVC_DECODER

#endif // __UMC_H265_WIDEVINE_SLICE_DECODING_H
#endif // UMC_ENABLE_H265_VIDEO_DECODER
