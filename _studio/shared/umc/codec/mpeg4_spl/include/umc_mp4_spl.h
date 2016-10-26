//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2007-2016 Intel Corporation. All Rights Reserved.
//

#ifndef __UMC_MP4_SPL_H__
#define __UMC_MP4_SPL_H__

#include "umc_index_spl.h"
#include "umc_mp4_parser.h"

/*
#define NAL_UNITTYPE_BITS       0x1f
#define NAL_UT_IDR_SLICE        0x05
#define NAL_UT_SLICE            0x01
*/

#define NUMBEROFFRAMES          8
#define MINAUDIOBUFFERSIZE      1024 * 16
#define MINVIDEOBUFFERSIZE      1024 * 1024

namespace UMC
{

  struct MP4TrackInfo : public TrackInfo
{
    DYNAMIC_CAST_DECL(MP4TrackInfo, TrackInfo)

    MP4TrackInfo()
    {
        m_DpndPID = 0;
    }

    Ipp32u  m_DpndPID;  // id of referenced track; 0 - base track
};

  class MP4Splitter : public IndexSplitter
{

    DYNAMIC_CAST_DECL(MP4Splitter, IndexSplitter)

public:

    MP4Splitter();
    ~MP4Splitter();

    virtual Status  Init(SplitterParams& init);
    virtual Status  Close();

protected:
    static Ipp32u VM_THREAD_CALLCONVENTION InitMoofThreadCallback(void* pParam);
    Status          ParseMP4Header();
    Status          InitMoof();
    Status          SelectNextFragment();
    Status          CheckInit();
    Status          CheckMoofInit();

    Status          ParseAVCCHeader(T_trak_data *pTrak, Ipp32u nTrack);
    Status          ParseHVCCHeader(T_trak_data *pTrak, Ipp32u nTrack);
    Status          ParseESDSHeader(T_trak_data *pTrak, Ipp32u nTrack);

    Status          MapTrafIDToTrackID (Ipp32u trafID, Ipp32u &nTrack);
    Status          AddMoovToIndex(Ipp32u iES);
    Status          AddMoofRunToIndex(Ipp32u iES, T_trex_data *pTrex, T_traf *pTraf,
                                      T_trun *pTrun, Ipp64u &nBaseOffset);
    void            FillSampleSizeAndType(T_trak_data *trak, IndexFragment &frag);
    void            FillSamplePos(T_trak_data *trak, IndexFragment &frag);
    void            FillSampleTimeStamp(T_trak_data *trak, IndexFragment &frag);

    Status          FillAudioInfo(T_trak_data *pTrak, Ipp32u nTrack);
    Status          FillMPEGAudioInfo(Ipp32u nTrack);   // fill layer
    Status          FillVideoInfo(T_trak_data *pTrak, Ipp32u nTrack);

    vm_thread      *m_pInitMoofThread;
    Ipp64u          m_nFragPosEnd;  // Moov or Moof

    Ipp32u          Get_24(DataReader *dr);
    Ipp32s          Read_mp4_descr_length(DataReader *dr);
    T_trak_data*    Add_trak(T_moov *moov);
    Status          Clear_track(T_trak_data* pTrak);

    Status          ParseTracksDuration(Ipp64u* pDuration);
    Status          ParseTrackFragmentDuration(Ipp64u* dDuration, T_atom_mp4 *atom);


    bool Compare_Atom(T_atom_mp4 *atom, const char *type);
    Status Atom_Skip(UMC::DataReader *dr, T_atom_mp4 *atom);

    Status Read_Atom(DataReader *dr, T_atom_mp4 *atom);
    Status Read_moov(DataReader *dr, T_moov *moov, T_atom_mp4 *atom);
    Status Read_mvhd(DataReader *dr, T_mvhd_data *mvhd_data);
    Status Read_mdhd(DataReader *dr, T_mdhd_data *mdhd);
    Status Read_iods(DataReader *dr, T_iods_data *iods);
    Status Read_tkhd(DataReader *dr, T_tkhd_data *tkhd);
    Status Read_mdia(DataReader *dr, T_mdia_data *mdia, T_atom_mp4 *trak_atom);
    Status Read_minf(DataReader *dr, T_minf_data *minf, T_atom_mp4 *parent_atom);
    Status Read_tref(DataReader *dr, T_tref_data *tref, T_atom_mp4 *atom);
    Status Read_dpnd(DataReader *dr, T_dpnd_data *dpnd);
    Status Read_hdlr(DataReader *dr, T_hdlr_data *hdlr);
    Status Read_hmhd(DataReader *dr, T_hmhd_data *hmhd);
    Status Read_vmhd(DataReader *dr, T_vmhd_data *vmhd);
    Status Read_smhd(DataReader *dr, T_smhd_data *smhd);
    Status Read_stbl(DataReader *dr, T_minf_data *minf, T_stbl_data *stbl, T_atom_mp4 *parent_atom);
    Status Read_stts(DataReader *dr, T_stts_data *stts);
    Status Read_stss(DataReader *dr, T_stss_data *stss);
    Status Read_stsd(DataReader *dr, T_minf_data *minf, T_stsd_data *stsd);
    Status Read_stsc(DataReader *dr, T_stsc_data *stsc);
    Status Read_stco(DataReader *dr, T_stco_data *stco);
    Status Read_co64(DataReader *dr, T_co64_data *co64);
    Status Read_stsz(DataReader *dr, T_stsz_data *stsz);
    Status Read_ctts(DataReader *dr, T_ctts_data *ctts);
    Status Read_stsd_table(DataReader *dr, T_minf_data *minf, T_stsd_table_data *table);
    Status Read_stsd_drms(DataReader *dr, T_stsd_table_data *table, T_atom_mp4 *parent_atom);
    Status Read_trak(DataReader *dr, T_trak_data *trak, T_atom_mp4 *trak_atom);
    Status Read_stsd_audio(DataReader *dr, T_stsd_table_data *table, T_atom_mp4 *parent_atom);
    Status Read_stsd_video(DataReader *dr, T_stsd_table_data *table, T_atom_mp4 *parent_atom);
    Status Read_h263_video(DataReader *dr, T_stsd_table_data *table);
    Status Read_h264_video(DataReader *dr, T_stsd_table_data *table, T_atom_mp4 *parent_atom);
    Status Read_stsd_hint(DataReader *dr, T_stsd_table_data *table, T_atom_mp4 *parent_atom);
    Status Read_esds(DataReader *dr, T_esds_data *esds);
    Status Read_dinf(DataReader *dr, T_dinf_data *dinf, T_atom_mp4 *dinf_atom);
    Status Read_dref(DataReader *dr, T_dref_data *dref);
    Status Read_dref_table(DataReader *dr, T_dref_table_data *table);
    Status Read_mvex(DataReader *dr, T_mvex_data *mvex, T_atom_mp4 *mvex_atom);
    Status Read_trex(DataReader *dr, T_trex_data *trex);

    Status Read_moof(DataReader *dr, T_moof *moof, T_atom_mp4 *atom);
    Status Clear_moof(T_moof &moof);
    Status Read_mfhd(DataReader *dr, T_mfhd *mfhd);
    Status Read_traf(DataReader *dr, T_traf *traf, T_atom_mp4 *atom);
    Status Read_tfhd(DataReader *dr, T_tfhd *tfhd);
    Status Read_trun(DataReader *dr, T_trun *trun);
    Status Read_trun_table(DataReader *dr, T_trun* trun, T_trun_table_data* table);

    Status Read_samr_audio(DataReader *dr, T_stsd_table_data *table);
    Status Read_damr_audio(DataReader *dr, T_stsd_table_data *table, T_atom_mp4 *parent_atom);

////    virtual void GetPictureType(int nOverallTrackIndex, SplSample* pSample);

    void SetH264FrameIntraSize(Ipp8u* decoderConfig);

protected:
    info_atoms        m_headerMPEG4;
    Ipp64u            m_nMoovAtomEnd;

    VideoStreamType   m_eVideoStreamType;
    Ipp8u             m_nH264FrameIntraSize;

    Ipp64u*           m_pFirstSegmentDuration;
    Ipp64f           *m_pLastPTS;
    bool              m_bFlagStopInitMoof;  /*** stops reading fragments ***/

};

} // namespace UMC

#endif //__UMC_MP4_SPL_H__
