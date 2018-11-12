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

#include "umc_defs.h"

#ifdef UMC_ENABLE_FW_AUDIO_RENDER

#include "fw_audio_render.h"
#include "vm_time.h"

UMC::FWAudioRenderParams::FWAudioRenderParams():
    pOutFile(NULL)
{}

UMC::FWAudioRender::FWAudioRender():
    m_tickStartTime(0),
    m_tickPrePauseTime(0),
    m_dfStartPTS(-1.0)
{
    memset(&pOutFileName, 0, sizeof(pOutFileName));
}

UMC::Status UMC::FWAudioRender::Init(MediaReceiverParams* pInit)
{
    Status umcRes = UMC_OK;
    Ipp32s err;
    Ipp32s len;

    AudioRenderParams* pParams = DynamicCast<AudioRenderParams>(pInit);
    if (NULL == pParams)
    {   return UMC_ERR_NULL_PTR;   }

    m_tickStartTime = 0;
    m_tickPrePauseTime = 0;
    m_dfStartPTS = -1.0;

    FWAudioRenderParams* pFWParams = DynamicCast<FWAudioRenderParams>(pInit);

    if ((pFWParams) && (pFWParams->pOutFile)) {
        len = (Ipp32s)vm_string_strlen(pFWParams->pOutFile);
        if (len < MAXIMUM_PATH) {
            vm_string_strcpy(pOutFileName, pFWParams->pOutFile);
        } else {
            vm_string_strcpy(pOutFileName, VM_STRING("output.wav"));
        }
    } else {
        vm_string_strcpy(pOutFileName, VM_STRING("output.wav"));
    }

#ifdef UMC_RENDERS_LOG
    vm_string_printf(VM_STRING("FW AUDIO: Output filename is %s\n"), pOutFileName);
#endif

    err = m_wav_file.Open(pOutFileName, AudioFile::AFM_CREATE);
    if (err < 0) {
#ifdef UMC_RENDERS_LOG
        vm_string_printf(VM_STRING("FW AUDIO: Failed to Open/Create the output file!!!\n"));
#endif
        return UMC_ERR_INIT;
    }

    m_wav_info.format_tag          = 1;
    m_wav_info.sample_rate         = pParams->info.sample_frequency;
    m_wav_info.resolution          = pParams->info.bitPerSample;
    m_wav_info.channels_number     = pParams->info.channels;
    m_wav_info.channel_mask        = 0;

    m_wav_file.SetInfo(&m_wav_info);

    if (UMC_OK == umcRes)
    {   umcRes = BasicAudioRender::Init(pInit); }

    return umcRes;
}

UMC::Status UMC::FWAudioRender::SendFrame(MediaData* in)
{
    Status umcRes = UMC_OK;

    if (UMC_OK == umcRes && (NULL == in->GetDataPointer())) {
        umcRes = UMC_ERR_NULL_PTR;
    }

    if (-1.0 == m_dfStartPTS) {
        m_dfStartPTS = in->GetTime();
    }

    if (UMC_OK == umcRes) {
        if (0 == m_tickStartTime)
        {   m_tickStartTime = vm_time_get_tick();   }
        Ipp32s nbytes = (Ipp32s)in->GetDataSize();
        Ipp32s nwritten = m_wav_file.Write((Ipp8u *)in->GetDataPointer(), nbytes);
#ifdef UMC_RENDERS_LOG
        vm_string_printf(VM_STRING("FW AUDIO: Wrote %i bytes from %i\n"), nwritten, nbytes);

#endif
        if (nwritten != nbytes)
            return UMC_ERR_FAILED;
    }
    return umcRes;
}

UMC::Status UMC::FWAudioRender::Pause(bool /* pause */)
{
    m_tickPrePauseTime += vm_time_get_tick() - m_tickStartTime;
    m_tickStartTime = 0;
    return UMC_OK;
}

Ipp32f UMC::FWAudioRender::SetVolume(Ipp32f /* volume */)
{
    return -1;
}

Ipp32f UMC::FWAudioRender::GetVolume()
{
    return -1;
}

UMC::Status UMC::FWAudioRender::Close()
{
//    m_Thread.Wait();
    m_wav_file.Close();
    m_dfStartPTS = -1.0;
    return UMC_OK;
//    return BasicAudioRender::Close();
}

UMC::Status UMC::FWAudioRender::Reset()
{
    m_tickStartTime = 0;
    m_tickPrePauseTime = 0;
    m_dfStartPTS = -1.0;
    return UMC_OK;
//    return BasicAudioRender::Reset();
}

Ipp64f UMC::FWAudioRender::GetTimeTick()
{
    Ipp64f dfRes = -1;
//    Ipp32u uiBytesPlayed = 0;

    if (0 != m_tickStartTime) {
        dfRes = m_dfStartPTS + ((Ipp64f)(Ipp64s)(m_tickPrePauseTime +
            vm_time_get_tick() - m_tickStartTime)) / (Ipp64f)(Ipp64s)vm_time_get_frequency();
    } else {
        dfRes = m_dfStartPTS + ((Ipp64f)(Ipp64s)m_tickPrePauseTime) / (Ipp64f)(Ipp64s)vm_time_get_frequency();
    }
    return dfRes;
}

#endif // UMC_ENABLE_FW_AUDIO_RENDER
