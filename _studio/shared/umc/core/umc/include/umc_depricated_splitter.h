// Copyright (c) 2003-2018 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

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

    int32_t number_audio_tracks;         // DEPRECATED!!!  (int32_t) number of available audio tracks
    int32_t number_video_tracks;         // DEPRECATED!!!  (int32_t) number of available video tracks

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
    virtual Status GetNextVideoData(MediaData* data, uint32_t track_idx);
    //  DEPRECATED!!! Get next audio data from track
    virtual Status GetNextAudioData(MediaData* data, uint32_t track_idx);
    //  DEPRECATED!!! Get next video data
    virtual Status GetNextVideoData(MediaData* data);
    //  DEPRECATED!!! Get next audio data
    virtual Status GetNextAudioData(MediaData* data);
    //  DEPRECATED!!! Get next video data
    virtual Status CheckNextVideoData(MediaData* data,uint32_t track_idx=0);
    //  DEPRECATED!!! Get next audio data
    virtual Status CheckNextAudioData(MediaData* data,uint32_t track_idx=0);
    //  DEPRECATED!!! Set position
    virtual Status SetPosition(double) { return UMC_OK; }
    //  DEPRECATED!!! Get position
    virtual Status GetPosition(double) { return UMC_OK; }
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
    Status GetBaseNextData(MediaData* data, uint32_t nTrack, bool bCheck);  // !!! TEMPORAL
    uint8_t pFirstFrame[32];  // !!! TEMPORAL
    uint8_t pAudioTrTbl[32];
    uint8_t pVideoTrTbl[32];
    DepricatedSplitterInfo *pNewSplInfo;
};
} // namespace UMC
#endif

#endif /* __UMC_DEPRICATED_SPLITTER_H__ */
