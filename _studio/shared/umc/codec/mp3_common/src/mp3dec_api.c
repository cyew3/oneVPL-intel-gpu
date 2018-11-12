// Copyright (c) 2006-2018 Intel Corporation
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

#include "umc_defs.h"
#if defined (UMC_ENABLE_MP3_AUDIO_DECODER)

#include "mp3dec_own.h"
#include "ipps.h"

MP3Status mp3decGetSize_com(Ipp32s *size)
{
  *size = MAINDATABUFSIZE;
  return MP3_OK;
}

MP3Status mp3decUpdateMemMap_com(MP3Dec_com *state, Ipp32s shift)
{
  MP3_UPDATE_PTR(Ipp32u, state->m_MainData.pBuffer, shift)
  return MP3_OK;
}

MP3Status mp3decInit_com(MP3Dec_com *state, void *mem)
{
    state->MAINDATASIZE = MAINDATABUFSIZE;

    ippsZero_8u((Ipp8u *)&(state->header), sizeof(IppMP3FrameHeader));
    ippsZero_8u((Ipp8u *)&(state->header_good), sizeof(IppMP3FrameHeader));
    state->mpg25 = state->mpg25_good = 0;
    state->stereo = 0;
    state->intensity = 0;
    state->ms_stereo = 0;
    state->mc_channel = 0;
    ippsZero_8u((Ipp8u *)&state->mc_header, sizeof(mp3_mc_header));

    state->si_main_data_begin = 0;
    state->si_private_bits = 0;
    state->si_part23Len[0][0] = 0;
    state->si_part23Len[0][1] = 0;
    state->si_part23Len[1][0] = 0;
    state->si_part23Len[1][1] = 0;

    state->si_bigVals[0][0] = 0;
    state->si_bigVals[0][1] = 0;
    state->si_bigVals[1][0] = 0;
    state->si_bigVals[1][1] = 0;

    state->si_globGain[0][0] = 0;
    state->si_globGain[0][1] = 0;
    state->si_globGain[1][0] = 0;
    state->si_globGain[1][1] = 0;

    state->si_sfCompress[0][0] = 0;
    state->si_sfCompress[0][1] = 0;
    state->si_sfCompress[1][0] = 0;
    state->si_sfCompress[1][1] = 0;

    state->si_winSwitch[0][0] = 0;
    state->si_winSwitch[0][1] = 0;
    state->si_winSwitch[1][0] = 0;
    state->si_winSwitch[1][1] = 0;

    state->si_blockType[0][0] = 0;
    state->si_blockType[0][1] = 0;
    state->si_blockType[1][0] = 0;
    state->si_blockType[1][1] = 0;

    state->si_mixedBlock[0][0] = 0;
    state->si_mixedBlock[0][1] = 0;
    state->si_mixedBlock[1][0] = 0;
    state->si_mixedBlock[1][1] = 0;

    state->si_pTableSelect[0][0][0] = 0;
    state->si_pTableSelect[0][0][1] = 0;
    state->si_pTableSelect[0][0][2] = 0;
    state->si_pTableSelect[0][1][0] = 0;
    state->si_pTableSelect[0][1][1] = 0;
    state->si_pTableSelect[0][1][2] = 0;
    state->si_pTableSelect[1][0][0] = 0;
    state->si_pTableSelect[1][0][1] = 0;
    state->si_pTableSelect[1][0][2] = 0;
    state->si_pTableSelect[1][1][0] = 0;
    state->si_pTableSelect[1][1][1] = 0;
    state->si_pTableSelect[1][1][2] = 0;

    state->si_pSubBlkGain[0][0][0] = 0;
    state->si_pSubBlkGain[0][0][1] = 0;
    state->si_pSubBlkGain[0][0][2] = 0;
    state->si_pSubBlkGain[0][1][0] = 0;
    state->si_pSubBlkGain[0][1][1] = 0;
    state->si_pSubBlkGain[0][1][2] = 0;
    state->si_pSubBlkGain[1][0][0] = 0;
    state->si_pSubBlkGain[1][0][1] = 0;
    state->si_pSubBlkGain[1][0][2] = 0;
    state->si_pSubBlkGain[1][1][0] = 0;
    state->si_pSubBlkGain[1][1][1] = 0;
    state->si_pSubBlkGain[1][1][2] = 0;

    state->si_reg0Cnt[0][0] = 0;
    state->si_reg0Cnt[0][1] = 0;
    state->si_reg0Cnt[1][0] = 0;
    state->si_reg0Cnt[1][1] = 0;

    state->si_reg1Cnt[0][0] = 0;
    state->si_reg1Cnt[0][1] = 0;
    state->si_reg1Cnt[1][0] = 0;
    state->si_reg1Cnt[1][1] = 0;

    state->si_preFlag[0][0] = 0;
    state->si_preFlag[0][1] = 0;
    state->si_preFlag[1][0] = 0;
    state->si_preFlag[1][1] = 0;

    state->si_sfScale[0][0] = 0;
    state->si_sfScale[0][1] = 0;
    state->si_sfScale[1][0] = 0;
    state->si_sfScale[1][1] = 0;

    state->si_cnt1TabSel[0][0] = 0;
    state->si_cnt1TabSel[0][1] = 0;
    state->si_cnt1TabSel[1][0] = 0;
    state->si_cnt1TabSel[1][1] = 0;

    state->si_scfsi[0] = 0;
    state->si_scfsi[1] = 0;

    ippsZero_8u((Ipp8u *)&(state->ScaleFactors), 2 * sizeof(sScaleFactors));
    ippsZero_8u((Ipp8u *)&(state->huff_table), 34 * sizeof(sHuffmanTable));
    state->part2_start = 0;

    ippsZero_8u((Ipp8u *)&(state->m_MainData), sizeof(sBitsreamBuffer));

    state->decodedBytes = 0;

    ippsZero_8u((Ipp8u *)(state->allocation), 2 * 32 * sizeof(Ipp16s));
    ippsZero_8u((Ipp8u *)(state->scfsi), 2 * 32 * sizeof(Ipp16s));
    ippsZero_8u((Ipp8u *)(state->scalefactor), 2 * 3 * 32 * sizeof(Ipp16s));
    ippsZero_8u((Ipp8u *)(state->scalefactor), 2 * 32 * 36 * sizeof(Ipp16u));
    state->nbal_alloc_table = NULL;
    state->alloc_table = NULL;

    state->m_StreamData.pBuffer = NULL;
    state->m_StreamData.pCurrent_dword = state->m_StreamData.pBuffer;
    state->m_StreamData.nDataLen = 0;
    state->m_StreamData.nBit_offset = 0;

    state->m_MainData.pBuffer = (Ipp32u *)mem;

    state->m_MainData.nBit_offset = 0;
    state->m_MainData.nDataLen = 0;
    state->m_MainData.pCurrent_dword = state->m_MainData.pBuffer;

    state->non_zero[0] = 0;
    state->non_zero[1] = 0;

    state->m_layer = 0;

    state->m_nBitrate = 0;
    state->m_frame_num = 0;
    state->m_bInit = 0;
    state->id3_size = 0;

    return MP3_OK;
}

MP3Status mp3decReset_com(MP3Dec_com *state)
{
    //Reposition support
    state->IsReset = 1;
    state->copy_m_MainData_nBit_offset = state->m_MainData.nBit_offset;
    state->copy_m_MainData_nDataLen = state->m_MainData.nDataLen;
    state->copy_m_MainData_pCurrent_dword = state->m_MainData.pCurrent_dword;
    
    state->MAINDATASIZE = MAINDATABUFSIZE;

    ippsZero_8u((Ipp8u *)&(state->header), sizeof(IppMP3FrameHeader));
    state->mpg25 = 0;
    state->stereo = 0;
    state->intensity = 0;
    state->ms_stereo = 0;
    state->mc_channel = 0;
    state->mc_header.lfe = 0;

    state->si_main_data_begin = 0;
    state->si_private_bits = 0;
    state->si_part23Len[0][0] = 0;
    state->si_part23Len[0][1] = 0;
    state->si_part23Len[1][0] = 0;
    state->si_part23Len[1][1] = 0;

    state->si_bigVals[0][0] = 0;
    state->si_bigVals[0][1] = 0;
    state->si_bigVals[1][0] = 0;
    state->si_bigVals[1][1] = 0;

    state->si_globGain[0][0] = 0;
    state->si_globGain[0][1] = 0;
    state->si_globGain[1][0] = 0;
    state->si_globGain[1][1] = 0;

    state->si_sfCompress[0][0] = 0;
    state->si_sfCompress[0][1] = 0;
    state->si_sfCompress[1][0] = 0;
    state->si_sfCompress[1][1] = 0;

    state->si_winSwitch[0][0] = 0;
    state->si_winSwitch[0][1] = 0;
    state->si_winSwitch[1][0] = 0;
    state->si_winSwitch[1][1] = 0;

    state->si_blockType[0][0] = 0;
    state->si_blockType[0][1] = 0;
    state->si_blockType[1][0] = 0;
    state->si_blockType[1][1] = 0;

    state->si_mixedBlock[0][0] = 0;
    state->si_mixedBlock[0][1] = 0;
    state->si_mixedBlock[1][0] = 0;
    state->si_mixedBlock[1][1] = 0;

    state->si_pTableSelect[0][0][0] = 0;
    state->si_pTableSelect[0][0][1] = 0;
    state->si_pTableSelect[0][0][2] = 0;
    state->si_pTableSelect[0][1][0] = 0;
    state->si_pTableSelect[0][1][1] = 0;
    state->si_pTableSelect[0][1][2] = 0;
    state->si_pTableSelect[1][0][0] = 0;
    state->si_pTableSelect[1][0][1] = 0;
    state->si_pTableSelect[1][0][2] = 0;
    state->si_pTableSelect[1][1][0] = 0;
    state->si_pTableSelect[1][1][1] = 0;
    state->si_pTableSelect[1][1][2] = 0;

    state->si_pSubBlkGain[0][0][0] = 0;
    state->si_pSubBlkGain[0][0][1] = 0;
    state->si_pSubBlkGain[0][0][2] = 0;
    state->si_pSubBlkGain[0][1][0] = 0;
    state->si_pSubBlkGain[0][1][1] = 0;
    state->si_pSubBlkGain[0][1][2] = 0;
    state->si_pSubBlkGain[1][0][0] = 0;
    state->si_pSubBlkGain[1][0][1] = 0;
    state->si_pSubBlkGain[1][0][2] = 0;
    state->si_pSubBlkGain[1][1][0] = 0;
    state->si_pSubBlkGain[1][1][1] = 0;
    state->si_pSubBlkGain[1][1][2] = 0;

    state->si_reg0Cnt[0][0] = 0;
    state->si_reg0Cnt[0][1] = 0;
    state->si_reg0Cnt[1][0] = 0;
    state->si_reg0Cnt[1][1] = 0;

    state->si_reg1Cnt[0][0] = 0;
    state->si_reg1Cnt[0][1] = 0;
    state->si_reg1Cnt[1][0] = 0;
    state->si_reg1Cnt[1][1] = 0;

    state->si_preFlag[0][0] = 0;
    state->si_preFlag[0][1] = 0;
    state->si_preFlag[1][0] = 0;
    state->si_preFlag[1][1] = 0;

    state->si_sfScale[0][0] = 0;
    state->si_sfScale[0][1] = 0;
    state->si_sfScale[1][0] = 0;
    state->si_sfScale[1][1] = 0;

    state->si_cnt1TabSel[0][0] = 0;
    state->si_cnt1TabSel[0][1] = 0;
    state->si_cnt1TabSel[1][0] = 0;
    state->si_cnt1TabSel[1][1] = 0;

    state->si_scfsi[0] = 0;
    state->si_scfsi[1] = 0;

    ippsZero_8u((Ipp8u *)&(state->ScaleFactors), 2 * sizeof(sScaleFactors));
    state->part2_start = 0;

    state->decodedBytes = 0;

    ippsZero_8u((Ipp8u *)(state->allocation), 2 * 32 * sizeof(Ipp16s));
    ippsZero_8u((Ipp8u *)(state->scfsi), 2 * 32 * sizeof(Ipp16s));
    ippsZero_8u((Ipp8u *)(state->scalefactor), 2 * 3 * 32 * sizeof(Ipp16s));
    ippsZero_8u((Ipp8u *)(state->scalefactor), 2 * 32 * 36 * sizeof(Ipp16u));

    state->m_StreamData.pBuffer = NULL;
    state->m_StreamData.pCurrent_dword = state->m_StreamData.pBuffer;
    state->m_StreamData.nDataLen = 0;
    state->m_StreamData.nBit_offset = 0;

    state->m_MainData.nBit_offset = 0;
    state->m_MainData.nDataLen = 0;
    state->m_MainData.pCurrent_dword = state->m_MainData.pBuffer;

    state->non_zero[0] = 0;
    state->non_zero[1] = 0;

    state->m_layer = 0;

//    state->m_nBitrate = 0;
    state->m_frame_num = 0;
//    state->m_bInit = 0;
    state->id3_size = -1;

    return MP3_OK;
}

MP3Status mp3decClose_com(/*MP3Dec_com *state*/)
{
    return MP3_OK;
}

#else
# pragma warning( disable: 4206 )
#endif //UMC_ENABLE_XXX
