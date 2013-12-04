/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2013 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#include "watermark.h"
#include "watermark_resource.h"
#include "vm_debug.h"
#include "ippcc.h"
#include "ippi.h"

#ifdef MFX_ENABLE_WATERMARK

#define WATERMARK_PERCENTAGE 10

Watermark *Watermark::CreateFromResource(void)
{
    // Get current library
    HMODULE currentModule = NULL;
    BOOL status = GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(Watermark::CreateFromResource), &currentModule);
    if (!status)
        return NULL;

    // Load bitmap from resource
    HBITMAP bitmap = LoadBitmap(currentModule, MAKEINTRESOURCE(WATERMARK_INTEL_LOGO));
    if (NULL == bitmap)
        return NULL;

    // Get some device context
    HDC hdc = GetDC(NULL);
    if (NULL == hdc)
        return NULL;

    // Get bitmap dimentions
    BITMAPINFO bmi = { 0 };
    bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);
    status = GetDIBits(hdc, bitmap, 0, 0, NULL, &bmi, DIB_RGB_COLORS);
    if (!status)
        return NULL;
    VM_ASSERT(bmi.bmiHeader.biBitCount == 32);
    mfxI32 m_watermarkWidth = bmi.bmiHeader.biWidth;
    mfxI32 m_watermarkHeight = bmi.bmiHeader.biHeight;

    // Allocate bitmap data
    mfxU8 *m_watermarkData =
        static_cast<mfxU8 *>(VirtualAlloc(NULL, bmi.bmiHeader.biWidth * bmi.bmiHeader.biHeight * 4, MEM_COMMIT, PAGE_READWRITE));
    if (NULL == m_watermarkData)
        return NULL;

    // Get bitmap data
    bmi.bmiHeader.biCompression = BI_RGB;
    status = GetDIBits(hdc, bitmap, 0, bmi.bmiHeader.biHeight, m_watermarkData, &bmi, DIB_RGB_COLORS);
    if (!status)
    {
        VirtualFree(m_watermarkData, 0, MEM_RELEASE);
        m_watermarkData = NULL;
        return NULL;
    }

    // Turn image upside down
    IppiSize roi = {m_watermarkWidth, m_watermarkHeight};
    IppStatus ipp_status = ippiMirror_8u_C4IR(m_watermarkData, m_watermarkWidth * 4, roi, ippAxsHorizontal);
    if (ippStsNoErr != ipp_status)
    {
        VirtualFree(m_watermarkData, 0, MEM_RELEASE);
        m_watermarkData = NULL;
        return NULL;
    }
/*
    FILE *out = fopen("out.c", "wt");
    VM_ASSERT(out);
    fprintf(out, "#define WIDTH %d\n", m_watermarkWidth);
    fprintf(out, "#define HEIGHT %d\n", m_watermarkHeight);
    mfxI32 count = 0;
    for (mfxI32 y = 0; y < m_watermarkHeight; y++)
    {
        for (mfxI32 x = 0; x < m_watermarkWidth; x++)
        {
            for (mfxI32 c = 0; c < 4; c++)
            {
                if (count % 16 == 0)
                    fprintf(out, "\n   ");
                fprintf(out, " %02x,", m_watermarkData[count++]);
            }
        }
    }
    fclose(out);
    return NULL;
*/
    // Set transparency
    ipp_status = ippiSet_8u_C4CR(0x7f, m_watermarkData + 3, m_watermarkWidth * 4, roi);
    if (ippStsNoErr != ipp_status)
    {
        VirtualFree(m_watermarkData, 0, MEM_RELEASE);
        m_watermarkData = NULL;
        return NULL;
    }

/*
    // Calculate transparency from inverted Red channel
    for (mfxI32 y = 0; y < m_watermarkHeight; y++)
    {
        for (mfxI32 x = 0; x < m_watermarkWidth; x++)
        {
            m_watermarkData[(y * m_watermarkWidth + x) * 4 + 3] = 0xff - m_watermarkData[(y * m_watermarkWidth + x) * 4];
        }
    }
    */
    Watermark *obj = new Watermark;
    if (NULL == obj)
    {
        VirtualFree(m_watermarkData, 0, MEM_RELEASE);
        m_watermarkData = NULL;
        return NULL;
    }
    obj->m_watermarkData = m_watermarkData;
    obj->m_watermarkWidth = m_watermarkWidth;
    obj->m_watermarkHeight = m_watermarkHeight;
    return obj;
}

void Watermark::Apply(mfxU8 *srcY, mfxU8 *srcUV, mfxI32 stride, mfxI32 width, mfxI32 height)
{
    VM_ASSERT(width > m_watermarkWidth);
    VM_ASSERT(height > m_watermarkHeight);

    mfxI32 size = IPP_MIN(width, height) / WATERMARK_PERCENTAGE;
    if (0 != (size & 1))
        size++;

    if (NULL == m_bitmapBuffer)
    {
        InitBuffers(size);
    }

    mfxI32 srcOffsetY = (height - m_roi.height * 2) * stride + width - m_roi.width * 2;
    mfxI32 srcOffsetUV = (height / 2 - m_roi.height) * stride + width - m_roi.width * 2;
    IppStatus status;

    // NV12 to YUV
    status = ippiYCbCr420_8u_P2P3R(srcY + srcOffsetY, stride, srcUV + srcOffsetUV, stride, m_yuvBuffer, m_yuvSize, m_roi);
    VM_ASSERT(ippStsNoErr == status);
    // YUV to ARGB
    status = ippiYCrCb420ToRGB_8u_P3C4R(const_cast<const Ipp8u **>(m_yuvBuffer), m_yuvSize, m_rgbaBuffer, m_roi.width * 4, m_roi, 0x7f);
    VM_ASSERT(ippStsNoErr == status);
    // Blend
    status = ippiAlphaComp_8u_AC4R(m_bitmapBuffer, m_roi.width * 4, m_rgbaBuffer, m_roi.width * 4, m_rgbaBuffer, m_roi.width * 4, m_roi, ippAlphaPlus);
    VM_ASSERT(ippStsNoErr == status);
    // ARGB to YUV
    status = ippiRGBToYCrCb420_8u_AC4P3R(m_rgbaBuffer, m_roi.width * 4, m_yuvBuffer, m_yuvSize, m_roi);
    VM_ASSERT(ippStsNoErr == status);
    // YUV to NV12
    status = ippiYCbCr420_8u_P3P2R(const_cast<const Ipp8u **>(m_yuvBuffer), m_yuvSize, srcY + srcOffsetY, stride, srcUV + srcOffsetUV, stride, m_roi);
    VM_ASSERT(ippStsNoErr == status);
}

void Watermark::Release(void)
{
    ReleaseBuffers();

    if (m_watermarkData)
    {
        VirtualFree(m_watermarkData, 0, MEM_RELEASE);
        m_watermarkData = NULL;
    }
}

void Watermark::InitBuffers(mfxI32 size)
{
    ReleaseBuffers();

    if (m_watermarkHeight == m_watermarkWidth)
    {
        m_roi.width = m_roi.height = size;
    }
    else if (m_watermarkHeight < m_watermarkWidth)
    {
        m_roi.width = size;
        m_roi.height = (int)((double)size / m_watermarkWidth * m_watermarkHeight + 0.5);
        if (m_roi.height & 1)
            m_roi.height++;
    }
    else
    {
        m_roi.width = (int)((double)size / m_watermarkHeight * m_watermarkWidth + 0.5);
        if (m_roi.width & 1)
            m_roi.width++;
        m_roi.height = size;
    }
   
    m_yuvBuffer[0] = new mfxU8[m_roi.width * m_roi.height * 3 / 2];
    VM_ASSERT(NULL != m_yuvBuffer);

    m_yuvBuffer[1] = m_yuvBuffer[0] + m_roi.width * m_roi.height;
    m_yuvBuffer[2] = m_yuvBuffer[1] + m_roi.width * m_roi.height / 4;
    m_yuvSize[0] = m_roi.width;
    m_yuvSize[1] = m_yuvSize[2] = m_roi.width / 2;

    m_rgbaBuffer = new mfxU8[m_roi.width * m_roi.height * 4];
    VM_ASSERT(NULL != m_rgbaBuffer);

    m_bitmapBuffer = new mfxU8[m_roi.width * m_roi.height * 4];
    VM_ASSERT(NULL != m_bitmapBuffer);

    IppiSize watermarkSize = {m_watermarkWidth, m_watermarkHeight};
    IppiRect srcROI = {0, 0, m_watermarkWidth, m_watermarkHeight};
    IppiRect dstROI = {0, 0, m_roi.width, m_roi.height};
    int bufferSize;

    IppStatus status = ippiResizeGetBufSize(srcROI, dstROI, 4, IPPI_INTER_CUBIC, &bufferSize);
    VM_ASSERT(ippStsNoErr == status);

    Ipp8u *buffer = new Ipp8u[bufferSize];
    VM_ASSERT(NULL != buffer);

    status = ippiResizeSqrPixel_8u_C4R(m_watermarkData, watermarkSize, m_watermarkWidth * 4, srcROI, m_bitmapBuffer, m_roi.width * 4, dstROI,
        (double)m_roi.width / m_watermarkWidth, (double)m_roi.height / m_watermarkHeight, 0.0, 0.0, IPPI_INTER_CUBIC, buffer);
    VM_ASSERT(ippStsNoErr == status);

    delete [] buffer;
}

void Watermark::ReleaseBuffers(void)
{
    if (m_yuvBuffer[0])
    {
        delete [] m_yuvBuffer[0];
        m_yuvBuffer[0] = NULL;
    }
    if (m_rgbaBuffer)
    {
        delete [] m_rgbaBuffer;
        m_rgbaBuffer = NULL;
    }
    if (m_bitmapBuffer)
    {
        delete [] m_bitmapBuffer;
        m_bitmapBuffer = NULL;
    }
}

#endif // MFX_ENABLE_WATERMARK