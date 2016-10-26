//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2004-2013 Intel Corporation. All Rights Reserved.
//

#ifndef __UMC_MP4_PARSER_H__
#define __UMC_MP4_PARSER_H__

#include "vm_types.h"
#include "umc_data_reader.h"

/*******************DECLARATIONS TYPES********************/

namespace UMC
{

#define MAXTRACKS 1024

typedef struct
{
  Ipp64u          start;          /* byte start in file */
  Ipp64u          end;            /* byte endpoint in file */
  Ipp64u          size;           /* number of byte in this box */
  bool            is_large_size;  /* true if actual size is in largesize field, false otherwise */
  char            type[4];
} T_atom_mp4;

typedef struct
{
  Ipp32f          values[9];
} T_matrix_data;

typedef struct
{
  Ipp8u           version;
  Ipp32u          flags;
  Ipp64u          creation_time;
  Ipp64u          modification_time;
  Ipp32u          time_scale;
  Ipp64u          duration;
  Ipp32f          preferred_rate;
  Ipp32f          preferred_volume;
  char            reserved[10];
  T_matrix_data   matrix;
  long            preview_time;
  long            preview_duration;
  long            poster_time;
  long            selection_time;
  long            selection_duration;
  long            current_time;
  Ipp32u          next_track_id;
} T_mvhd_data;

typedef struct
{
  Ipp8u           version;
  Ipp32u          flags;
  Ipp16u          objectDescriptorID;
  Ipp8u           OD_profileAndLevel;
  Ipp8u           scene_profileAndLevel;
  Ipp8u           audioProfileId;
  Ipp8u           videoProfileId;
  Ipp8u           graphics_profileAndLevel;
} T_iods_data;

typedef struct
{
  Ipp8u           version;
  Ipp32u          flags;
  Ipp64u          creation_time;
  Ipp64u          modification_time;
  Ipp32u          track_id;
  Ipp32u          reserved1;
  Ipp64u          duration;
  Ipp32u          reserved2[2];
  Ipp32s          layer;
  Ipp32s          alternate_group;
  Ipp32f          volume;
  Ipp32u          reserved3;
  T_matrix_data   matrix;
  Ipp32f          track_width;
  Ipp32f          track_height;
  Ipp32s          is_video;
  Ipp32s          is_audio;
  Ipp32s          is_hint;
} T_tkhd_data;

typedef struct
{
  Ipp8u           version;
  Ipp32u          flags;
  Ipp64u          creation_time;
  Ipp64u          modification_time;
  Ipp32u          time_scale;
  Ipp64u          duration;
  Ipp16u          language;
  Ipp16u          quality;
} T_mdhd_data;

typedef struct
{
  Ipp8u           version;
  Ipp32u          flags;
  Ipp32s          graphics_mode;
  Ipp32s          opcolor[3];
} T_vmhd_data;

typedef struct
{
  Ipp8u           version;
  Ipp32u          flags;
  Ipp32s          balance;
  Ipp32s          reserved;
} T_smhd_data;

/* hint media handler */
typedef struct
{
  Ipp8u           version;
  Ipp32u          flags;
  Ipp16u          maxPDUsize;
  Ipp16u          avgPDUsize;
  Ipp32u          maxbitrate;
  Ipp32u          avgbitrate;
  Ipp32u          slidingavgbitrate;
} T_hmhd_data;

typedef struct
{
  Ipp64u          start;
  Ipp8u           version;
  Ipp32s          flags;
  Ipp32u          decoderConfigLen;
  Ipp8u           *decoderConfig;

  Ipp8u           objectTypeID;
  Ipp8u           streamType;
  Ipp32u          bufferSizeDB;
  Ipp32u          maxBitrate;
  Ipp32u          avgBitrate;

  Ipp16u          es_ID;
  Ipp16u          ocr_ID;
  Ipp8u           streamDependenceFlag;
  Ipp8u           urlflag;
  Ipp8u           ocrflag;
  Ipp8u           streamPriority;
  Ipp8u           *URLString;
} T_esds_data;

typedef struct
{
  Ipp32s          decoderConfigLen;
  Ipp8u           *decoderConfig;
} T_damr_data;

typedef struct
{
  VideoStreamType type; // HEVC_VIDEO or H264_VIDEO
  Ipp32s          decoderConfigLen;
  Ipp8u           *decoderConfig;
//    Ipp8u   m_nH264FrameIntraSize;
} T_avcC_data; // this structure is used for hvcC as well at the moment

typedef struct
{
  char            format[4];
  Ipp8u           reserved[6];
  Ipp16u          data_reference;
/* common to audio and video */
  Ipp32s          version;
  Ipp32s          revision;
  char            vendor[4];
/* video description */
  Ipp32u          temporal_quality;
  Ipp32u          spatial_quality;
  Ipp16u          width;
  Ipp16u          height;
  Ipp32u          dpi_horizontal;
  Ipp32u          dpi_vertical;
  Ipp32u          data_size;
  Ipp32s          frames_per_sample;
  char            compressor_name[32];
  Ipp32s          depth;
  Ipp32s          ctab_id;
  Ipp32f          gamma;
  Ipp32s          fields;    /* 0, 1, or 2 */
  Ipp32s          field_dominance;   /* 0 - unknown     1 - top first     2 - bottom first */
/* audio description */
  Ipp16u          channels;
  Ipp16u          sample_size;
  Ipp32s          compression_id;
  Ipp32s          packet_size;
  Ipp32u          sample_rate;
  Ipp32u          is_protected;
/* hint description */
  Ipp32s          maxPktSize;
/* MP4 elementary stream descriptor */
  T_esds_data     esds;
/* ARM elementary stream descriptor */
  T_damr_data     damr;
/* avc elementary stream descriptor */
  T_avcC_data     avcC;
} T_stsd_table_data;

typedef struct
{
  Ipp8u           version;
  Ipp32u          flags;
  Ipp32u          total_entries;
  T_stsd_table_data *table;
} T_stsd_data;

typedef struct
{
  Ipp32u          sample_count;
  Ipp32u          sample_duration;
} T_stts_table_data;

typedef struct
{
  Ipp8u           version;
  Ipp32u          flags;
  Ipp32u          total_entries;
/////    long            entries_allocated;
  T_stts_table_data *table;
} T_stts_data;

/* sync sample */
typedef struct
{
  Ipp32u          sample_number;
} T_stss_table_data;

typedef struct
{
  Ipp8u           version;
  Ipp32u          flags;
  Ipp32u          total_entries;
/////    long entries_allocated;
  T_stss_table_data *table;
} T_stss_data;

/* sample to chunk */
typedef struct
{
  Ipp32u          chunk;
  Ipp32u          samples;
  Ipp32u          id;
  Ipp32u          first_sample;
} T_stsc_table_data;

typedef struct
{
  Ipp8u           version;
  Ipp32u          flags;
  Ipp32u          total_entries;
/////    Ipp32u     entries_allocated;
  T_stsc_table_data *table;
} T_stsc_data;

/* sample size */
typedef struct
{
  Ipp32u          size;
} T_stsz_table_data;

typedef struct
{
  Ipp8u           version;
  Ipp32u          flags;
  Ipp32u          sample_size;
  Ipp32u          total_entries;
  Ipp32u          max_sample_size;
  T_stsz_table_data *table;
} T_stsz_data;

/* chunk offset */
typedef struct
{
  Ipp32u          offset;
} T_stco_table_data;

typedef struct
{
  Ipp8u           version;
  Ipp32u          flags;
  Ipp32u          total_entries;
/////    long            entries_allocated;
  T_stco_table_data *table;
} T_stco_data;

typedef struct
{
  Ipp64u          offset;
} T_co64_table_data;

typedef struct
{
  Ipp8u           version;
  Ipp32u          flags;
  Ipp32u          total_entries;
  T_co64_table_data *table;
} T_co64_data;

/* composition time to sample */
typedef struct
{
  Ipp32u          sample_count;
  Ipp32s          sample_offset;
} T_ctts_table_data;

typedef struct
{
  Ipp8u           version;
  Ipp32u          flags;
  Ipp32u          total_entries;
/////    long            entries_allocated;
  T_ctts_table_data *table;
} T_ctts_data;

/* sample table */
typedef struct
{
  Ipp32s          version;
  Ipp32u          flags;
  T_stsd_data     stsd;
  T_stts_data     stts;
  T_stss_data     stss;
  T_stsc_data     stsc;
  T_stsz_data     stsz;
  T_stco_data     stco;
  T_co64_data     co64;
  T_ctts_data     ctts;
} T_stbl_data;

/* handler reference */
typedef struct
{
  Ipp8u           version;
  Ipp32u          flags;
  char            component_type[4];
  char            component_subtype[4];
  Ipp32u          component_manufacturer;
  Ipp32u          component_flags;
  Ipp32u          component_flag_mask;
  char            component_name[256];
} T_hdlr_data;

/* data reference */
typedef struct
{
  Ipp32u          size;
  char            type[4];
  Ipp8u           version;
  Ipp32u          flags;
  char *          data_reference;
} T_dref_table_data;

typedef struct
{
  Ipp8u           version;
  Ipp32u          flags;
  Ipp32u          total_entries;
  T_dref_table_data *table;
} T_dref_data;

/* data information */
typedef struct
{
  T_dref_data     dref;
} T_dinf_data;

typedef struct
{
  Ipp32s          is_video;
  Ipp32s          is_audio;
  Ipp32s          is_hint;
  T_vmhd_data     vmhd;
  T_smhd_data     smhd;
  T_hmhd_data     hmhd;
  T_stbl_data     stbl;
  T_hdlr_data     hdlr;
  T_dinf_data     dinf;
} T_minf_data;

typedef struct
{
  T_mdhd_data     mdhd;
  T_minf_data     minf;
  T_hdlr_data     hdlr;
} T_mdia_data;

/*
typedef struct
{
  Ipp32u          duration;
  Ipp32s          time;
  Ipp32f          rate;
} T_elst_table_data;

typedef struct
{
  Ipp32s          version;
  Ipp32u          flags;
  Ipp32u          total_entries;
  T_elst_table_data *table;
} T_elst_data;

typedef struct
{
  T_elst_data     elst;
} T_edts_data;
*/

typedef struct
{
  Ipp32s          numTracks;
  Ipp32u          trackIds[MAXTRACKS];
  void            *traks[MAXTRACKS];
} T_hint_data;

typedef struct
{
  Ipp32u          idTrak;
} T_dpnd_data;

typedef struct
{
  T_dpnd_data     dpnd;
  T_hint_data     hint;
} T_tref_data;

typedef struct
{
  T_tkhd_data     tkhd;
  T_mdia_data     mdia;
//  T_edts_data     edts;
  T_tref_data     tref;
} T_trak_data;

typedef struct
{
  char            *copyright;
  Ipp32s          copyright_len;
  char            *name;
  Ipp32s          name_len;
  char            *info;
  Ipp32s          info_len;
} T_udta_data;

typedef struct
{
  Ipp8u           version;
  Ipp32u          flags;
  Ipp32u          track_ID;
  Ipp32u          default_sample_description_index;
  Ipp32u          default_sample_duration;
  Ipp32u          default_sample_size;
  Ipp32u          default_sample_flags;
} T_trex_data;

typedef struct
{
  Ipp32u          size_atom;
  Ipp32u          total_tracks;
  T_trex_data     *trex[MAXTRACKS];
} T_mvex_data;

typedef struct
{
  Ipp32u          total_tracks;
  T_mvhd_data     mvhd;
  T_iods_data     iods;
  T_trak_data     *trak[MAXTRACKS];
  T_udta_data     udta;
  T_mvex_data     mvex;
} T_moov;

typedef struct
{
  Ipp32u          sample_duration;
  Ipp32u          sample_size;
  Ipp32u          sample_flags;
  Ipp32u          sample_composition_time_offset;
} T_trun_table_data;

typedef struct
{
  Ipp8u           version;
  Ipp32u          flags;
  Ipp32u          sequence_number;
} T_mfhd;

typedef struct
{
  Ipp8u           version;
  Ipp32u          flags;
  Ipp32u          sample_count;
  Ipp32s          data_offset;
  Ipp32u          first_sample_flags;
  T_trun_table_data *table_trun;
  Ipp64f          stream_end;
} T_trun;

typedef struct
{
  Ipp8u           version;
  Ipp32u          flags;
  Ipp32u          track_ID;
  Ipp32u          base_data_offset;
  Ipp32u          sample_description_index;
  Ipp32u          default_sample_duration;
  Ipp32u          default_sample_size;
  Ipp32u          default_sample_flags;
} T_tfhd;

typedef struct
{
  Ipp32u          entry_count;
  Ipp32u          max_truns;
  T_tfhd          tfhd;
  T_trun          *trun[MAXTRACKS];
} T_traf;

typedef struct
{
  Ipp32u          size_atom;
  Ipp32u          total_tracks;
  Ipp64u          end; /* byte endpoint in file */
  T_mfhd          mfhd;
  T_traf          *traf[MAXTRACKS];
} T_moof;

typedef struct
{
  T_moov          moov;
  T_moof          moof;
  T_atom_mp4      data;
  T_atom_mp4      esds;
  bool            is_fragment;
} info_atoms;

// track run flags
#define DATA_OFFSET_PRESENT                         0x000001
#define FIRST_SAMPLE_FLAGS_PRESENT                  0x000004
#define SAMPLE_DURATION_PRESENT                     0x000100
#define SAMPLE_SIZE_PRESENT                         0x000200
#define SAMPLE_FLAGS_PRESENT                        0x000400
#define SAMPLE_COMPOSITION_TIME_OFFSETS_PRESENT     0x000800

// track fragment header flags
#define BASE_DATA_OFFSET_PRESENT                    0x000001
#define SAMPLE_DESCRIPTION_INDEX_PRESENT            0x000002
#define DEFAULT_SAMPLE_DURATION_PRESENT             0x000008
#define DEFAULT_SAMPLE_SIZE_PRESENT                 0x000010
#define DEFAULT_SAMPLE_FLAGS_PRESENT                0x000020
#define DURATION_IS_EMPTY                           0x010000


class BitStreamReader
{
public:
    // Default constructor
    BitStreamReader(void)
    {
        m_pSource = NULL;
        m_nBits = 0;
        m_iReadyBits = 0;
    }

    // Destructor
    virtual ~BitStreamReader(void){}

    // Initialize bit stream reader
    void Init(Ipp8u *pStream)
    {
        m_pSource = pStream;
        m_nBits = 0;
        m_iReadyBits = 0;
    }

    // Copy next bit
    Ipp32u CopyBit(void)
    {
        if (0 == m_iReadyBits)
            Refresh();

        return ((m_nBits >> (m_iReadyBits - 1)) & 1);
    }

    // Get bit(s)
    Ipp32u GetBit(void)
    {
        if (0 == m_iReadyBits)
            Refresh();

        m_iReadyBits -= 1;
        return ((m_nBits >> m_iReadyBits) & 1);
    }
    Ipp32u GetBits(Ipp32s iNum)
    {
        if (iNum > m_iReadyBits)
            Refresh();

        m_iReadyBits -= iNum;
        return ((m_nBits >> m_iReadyBits) & ~(-1 << iNum));
    }
    Ipp32u GetUE(void)
    {
        Ipp32s iZeros;

        // count leading zeros
        iZeros = 0;
        while (0 == CopyBit())
        {
            iZeros += 1;
            GetBit();
        }

        // get value
        return (GetBits(iZeros + 1) - 1);
    }
    Ipp32s GetSE(void)
    {
        Ipp32s iZeros;
        Ipp32s iValue;

        // count leading zeros
        iZeros = 0;
        while (0 == CopyBit())
        {
            iZeros += 1;
            GetBit();
        }

        // get value
        iValue = GetBits(iZeros);
        return ((GetBit()) ? (-iValue) : (iValue));
    }

protected:
    // Refresh pre-read bits
    void Refresh(void)
    {
        while (24 > m_iReadyBits)
        {
            m_nBits = (m_nBits << 8) | m_pSource[0];
            m_iReadyBits += 8;
            m_pSource += 1;
        }
    }

    Ipp8u *m_pSource;                                           // (Ipp8u *) pointer to source stream
    Ipp32u m_nBits;                                             // (Ipp32u) pre-read stream bits
    Ipp32s m_iReadyBits;                                        // (Ipp32s) amount of pre-read bits
};


} // namespace UMC

#endif //__UMC_MP4_PARSER_H__
