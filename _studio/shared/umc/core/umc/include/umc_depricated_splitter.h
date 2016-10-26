//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2011 Intel Corporation. All Rights Reserved.
//

#ifndef __UMC_DEPRICATED_SPLITTER_H__
#define __UMC_DEPRICATED_SPLITTER_H__

#include "umc_splitter.h"

#if 0
namespace UMC
{

class DepricatedSplitterInfo : public SplitterInfo
{
    DYNAMIC_CAST_DECL_BASE(DepricatedSplitterInfo)

public:

    DepricatedSplitterInfo();
    /******************* below is DEPRECATED fields ***********************/

    AudioStreamInfo m_audio_info;       // DEPRECATED!!! (AudioStreamInfo) audio track 0 info
    VideoStreamInfo m_video_info;       // DEPRECATED!!! (VideoStreamInfo) video track 0 info
    SystemStreamInfo m_system_info;     // DEPRECATED!!! (SystemStreamInfo) media stream info

    // memory for auxilary tracks will be allocated inside
    // GetInfo method user should free it then
    AudioStreamInfo *m_audio_info_aux;  // DEPRECATED!!! auxilary audio tracks 1..
    VideoStreamInfo *m_video_info_aux;  // DEPRECATED!!! auxilary video tracks 1..

    Ipp32s number_audio_tracks;         // DEPRECATED!!!  (Ipp32s) number of available audio tracks
    Ipp32s number_video_tracks;         // DEPRECATED!!!  (Ipp32s) number of available video tracks

};

/*
//  Class:       Splitter
//
//  Notes:       Base abstract class of splitter. Class describes
//               the high level interface of abstract splitter of media stream.
//               All specific ( avi, mpeg2, mpeg4 etc ) must be implemented in
//               derevied classes.
//               Splitter uses this class to obtain data
//
*/
class DepricatedSplitter: public Splitter
{
    DYNAMIC_CAST_DECL_BASE(DepricatedSplitter)

    /******************* below is DEPRECATED methods ***********************/
public:
    DepricatedSplitter();
    //  DEPRECATED!!! Get next video data from track
    virtual Status GetNextVideoData(MediaData* data, Ipp32u track_idx);
    //  DEPRECATED!!! Get next audio data from track
    virtual Status GetNextAudioData(MediaData* data, Ipp32u track_idx);
    //  DEPRECATED!!! Get next video data
    virtual Status GetNextVideoData(MediaData* data);
    //  DEPRECATED!!! Get next audio data
    virtual Status GetNextAudioData(MediaData* data);
    //  DEPRECATED!!! Get next video data
    virtual Status CheckNextVideoData(MediaData* data,Ipp32u track_idx=0);
    //  DEPRECATED!!! Get next audio data
    virtual Status CheckNextAudioData(MediaData* data,Ipp32u track_idx=0);
    //  DEPRECATED!!! Set position
    virtual Status SetPosition(Ipp64f) { return UMC_OK; }
    //  DEPRECATED!!! Get position
    virtual Status GetPosition(Ipp64f) { return UMC_OK; }
    //  DEPRECATED!!! Get splitter info
    virtual Status GetInfo(DepricatedSplitterInfo* pInfo);
    //  DEPRECATED!!!
    virtual Status PrepareForRePosition()
    {return UMC_OK;};
    // Get splitter info
    virtual Status GetInfo(DepricatedSplitterInfo** /*ppInfo*/)
    {
fprintf(stderr, "UMC_ERR_NOT_IMPLEMENTED\n");
      return UMC_ERR_NOT_IMPLEMENTED;
    }

protected:
    Status GetBaseNextData(MediaData* data, Ipp32u nTrack, bool bCheck);  // !!! TEMPORAL
    Ipp8u pFirstFrame[32];  // !!! TEMPORAL
    Ipp8u pAudioTrTbl[32];
    Ipp8u pVideoTrTbl[32];
    DepricatedSplitterInfo *pNewSplInfo;
};
} // namespace UMC
#endif

#endif /* __UMC_DEPRICATED_SPLITTER_H__ */
