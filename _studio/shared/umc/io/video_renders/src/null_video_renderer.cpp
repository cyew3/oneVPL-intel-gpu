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

#include "null_video_renderer.h"

namespace UMC
{

#define MODULE          "NULLVideoRender:"
#define FUNCTION        "<Undefined Function>"
#define DBG_SET(msg)    VIDEO_DRV_DBG_SET   (MODULE, FUNCTION, msg)

NULLVideoRender::NULLVideoRender(void)
{
#undef  FUNCTION
#define FUNCTION "NULLVideoRender"
    DBG_SET("+");
    DBG_SET("-");
}

NULLVideoRender::~NULLVideoRender(void)
{
#undef  FUNCTION
#define FUNCTION "~NULLVideoRender"
    DBG_SET("+");
    Close();
    DBG_SET("-");
}

Status NULLVideoRender::Init(MediaReceiverParams* pInit)
{
#undef  FUNCTION
#define FUNCTION "Init"
    DBG_SET("+");
    Status                      umcRes  = UMC_OK;
    VideoRenderParams*          pParams = DynamicCast<VideoRenderParams>(pInit);
    UnifiedVideoRenderParams    UnifiedParams;
    NULLVideoDrvParams          DrvParams;
    VideoDrvVideoMemInfo        sMemInfo;
    size_t                      nPitch = 0, nImageSize = 0, nAlignment = VIDEO_DRV_ALIGN_BYTE;

    if (NULL == pParams)
    {
        umcRes = UMC_ERR_NULL_PTR;
    }
    if (UMC_OK == umcRes)
    {
        // NOTE: next Init will be removed in future.
        if (NONE == pParams->out_data_template.GetColorFormat())
            umcRes = m_OutDataTemplate.Init(pParams->info.width,
                                            pParams->info.height,
                                            pParams->color_format);
        else
            m_OutDataTemplate = pParams->out_data_template;
        nPitch     = m_OutDataTemplate.GetPlanePitch(0);
        nImageSize = m_OutDataTemplate.GetMappingSize();
        nAlignment = m_OutDataTemplate.GetAlignment();
    }
    if (UMC_OK == umcRes)
    {
        /* Copying parameters specified in <pParams> to <UnifiedParams>. */
        UnifiedParams = *pParams;
        /* Setting SDL driver specification. */
        UnifiedParams.m_pDrvSpec    = (VideoDrvSpec*) &NULLVideoDrvSpec;
        DrvParams.common.img_width  = m_OutDataTemplate.GetWidth();
        DrvParams.common.img_height = m_OutDataTemplate.GetHeight();
        DrvParams.common.win_rect.x = pParams->disp.left;
        DrvParams.common.win_rect.y = pParams->disp.top;
        DrvParams.common.win_rect.w = pParams->disp.right  - pParams->disp.left;
        DrvParams.common.win_rect.h = pParams->disp.bottom - pParams->disp.top;
        /* Setting SDL driver parameters. */
        UnifiedParams.m_pDrvParams  = &DrvParams;
        /* Setting video memory specification. */
        UnifiedParams.m_VideoMemType    = VideoDrvVideoMemInternal;
        sMemInfo.m_pVideoMem            = NULL;
        sMemInfo.m_nMemSize             = 0;
        sMemInfo.m_nAlignment           = nAlignment;
        sMemInfo.m_nPitch               = nPitch;
        sMemInfo.m_nBufSize             = nImageSize;
        UnifiedParams.m_pVideoMemInfo   = &sMemInfo;
    }
    if (UMC_OK == umcRes)
    {
        umcRes = UnifiedVideoRender::Init(&UnifiedParams);
    }
    DBG_SET("-");
    return umcRes;

} // Status NULLVideoRender::Init(MediaReceiverParams* pInit)

} // namespace UMC
