//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2013-2014 Intel Corporation. All Rights Reserved.
//

#include "watermark.h"

#if defined(_WIN32) || defined(_WIN64)
#include "watermark_resource.h"
#else
#include "watermark_base64.h"
#endif

#include "vm_debug.h"
#include "ippcc.h"
#include "ippi.h"

#ifdef MFX_ENABLE_WATERMARK

#define WATERMARK_PERCENTAGE 10

#if defined(_WIN32) || defined(_WIN64)
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
#else  // Linux

struct __attribute__((packed)) BitmapHeader
{
    mfxU16  Type;          // signature - 'BM'
    mfxU32  Size;          // file size in bytes
    mfxU16  Reserved1;     // 0
    mfxU16  Reserved2;     // 0
    mfxU32  OffBits;       // offset to bitmap
    mfxU32  StructSize;    // size of this struct (40)
    mfxU32  Width;         // bmap width in pixels
    mfxU32  Height;        // bmap height in pixels
    mfxU16  Planes;        // num planes - always 1
    mfxU16  BitCount;      // bits per pixel
    mfxU32  Compression;   // compression flag
    mfxU32  SizeImage;     // image size in bytes
    mfxI32  XPelsPerMeter; // horz resolution
    mfxI32  YPelsPerMeter; // vert resolution
    mfxU32  ClrUsed;       // 0 -> color table size
    mfxU32  ClrImportant;  // important color count
};

#define BASE64STRING_BUF_SIZE(byteSize) (((byteSize + 2)/3)*4)

#define BASE64BINARY_BUF_SIZE(strLength) ((strLength/4)*3)

mfxStatus BASE64ToBinary(
    const char *str, unsigned int strLength,
    unsigned char *buf, unsigned int *bufSize)
{
    const unsigned char BASE64ToInt[256] = {
        255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
        255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
        255,255,255,255,255,255,255,255,255,255,255, 62,255,255,255, 63,
        52, 53, 54, 55, 56, 57, 58, 59, 60, 61,255,255,255,  0,255,255,
        255,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
        15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,255,255,255,255,255,
        255, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
        41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51,255,255,255,255,255,
        255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
        255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
        255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
        255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
        255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
        255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
        255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
        255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255
    };
    int i;
    int count4 = strLength/4;
    unsigned char *out = buf;
    const unsigned char *in = (const unsigned char *)str;
    unsigned int resultSize;

    resultSize = BASE64BINARY_BUF_SIZE(strLength);
    if (strLength > 1 &&
        '=' == str[strLength - 1])
    {
        resultSize -= 1;
        if (strLength > 2 &&
            '=' == str[strLength - 2])
            resultSize -= 1;
    }

    if (NULL == buf)
    {
        *bufSize = resultSize;
        return MFX_ERR_NONE;
    }


    if (resultSize > *bufSize)
    {
        mfxStatus ires = MFX_ERR_INVALID_HANDLE;
//        ERROR_MSG( 1, ires, (( "BASE64ToBinary: resultSize > *bufSize): %d > %d, strLength=%d\n" ),
//            resultSize, *bufSize, strLength ));
        return ires;
    }

    count4--;
    for (i = 0; i < count4; i++)
    {
        if (BASE64ToInt[in[0]] == 255 || BASE64ToInt[in[1]] == 255 ||
            BASE64ToInt[in[2]] == 255 || BASE64ToInt[in[3]] == 255)
        {
            mfxStatus ires = MFX_ERR_INVALID_HANDLE;
            //ERROR_MSG( 1, ires, (( "BASE64ToBinary: Restricted symbol found.\n" ) ));
            return ires;
        }
        out[0] = (BASE64ToInt[in[0]] << 2) | (BASE64ToInt[in[1]] >> 4);
        out[1] = (BASE64ToInt[in[1]] << 4) | (BASE64ToInt[in[2]] >> 2);
        out[2] = (BASE64ToInt[in[2]] << 6) | BASE64ToInt[in[3]];
        out += 3;
        in += 4;
    }

    *bufSize = BASE64BINARY_BUF_SIZE(strLength);

    if (BASE64ToInt[in[0]] == 255 || BASE64ToInt[in[1]] == 255 ||
        BASE64ToInt[in[2]] == 255 || BASE64ToInt[in[3]] == 255)
    {
        mfxStatus  ires = MFX_ERR_INVALID_HANDLE;
        // ERROR_MSG( 1, ires, (( "BASE64ToBinary: Restricted symbol found.\n" ) ));
        return ires;
    }

    out[0] = (BASE64ToInt[in[0]] << 2) | (BASE64ToInt[in[1]] >> 4);
    if (strLength > 1 &&
        '=' == str[strLength - 1])
    {
        *bufSize -= 1;
        if (strLength > 2 &&
            '=' == str[strLength - 2])
            *bufSize -= 1;
        else
            out[1] = (BASE64ToInt[in[1]] << 4) | (BASE64ToInt[in[2]] >> 2);
    }
    else
    {
        out[1] = (BASE64ToInt[in[1]] << 4) | (BASE64ToInt[in[2]] >> 2);
        out[2] = (BASE64ToInt[in[2]] << 6) | BASE64ToInt[in[3]];
    }

    return MFX_ERR_NONE;
}

Watermark *Watermark::CreateFromResource(void)
{
    mfxU32 binSize = BASE64BINARY_BUF_SIZE(sizeof(g_coded_logo_bytes));
    Ipp8u *rawImage = new Ipp8u[binSize];
    VM_ASSERT(NULL != rawImage);
    if (!rawImage)
    {
        return NULL;
    }

    mfxStatus status = BASE64ToBinary(g_coded_logo_bytes, sizeof(g_coded_logo_bytes), rawImage, &binSize);
    VM_ASSERT(MFX_ERR_NONE == status);

    BitmapHeader & header = (BitmapHeader &)*rawImage;

    mfxU32 imageSize = binSize - sizeof(BitmapHeader);

    Ipp8u *buffer = new Ipp8u[imageSize];
    VM_ASSERT(NULL != buffer);
    if (!buffer)
    {
        delete[] rawImage;
        return NULL;
    }

    ippsCopy_8u(rawImage + header.OffBits, buffer, imageSize);

    VM_ASSERT(MFX_ERR_NONE == status);
    if (MFX_ERR_NONE != status)
    {
        delete[] rawImage;
        delete[] buffer;
        return NULL;
    }
    
    mfxI32 watermarkWidth = header.Width;
    mfxI32 watermarkHeight = header.Height;

    delete[] rawImage;

    // Turn image upside down
    IppiSize roi = {watermarkWidth, watermarkHeight};
    IppStatus ipp_status = ippiMirror_8u_C4IR(buffer, watermarkWidth * 4, roi, ippAxsHorizontal);
    if (ippStsNoErr != ipp_status)
    {
        delete[] buffer;
        return NULL;
    }

    // Set transparency
    ipp_status = ippiSet_8u_C4CR(0x7f, buffer + 3, watermarkWidth * 4, roi);
    if (ippStsNoErr != ipp_status)
    {
        delete[] buffer;
        return NULL;
    }

    Watermark *obj = new Watermark;
    if (NULL == obj)
    {
        delete[] buffer;
        buffer = NULL;
        return NULL;
    }

    obj->m_watermarkData = buffer;
    obj->m_watermarkWidth = watermarkWidth;
    obj->m_watermarkHeight = watermarkHeight;

    return obj;
}

#endif



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
#if defined(_WIN32) || defined(_WIN64)
        VirtualFree(m_watermarkData, 0, MEM_RELEASE);
#else
        delete [] m_watermarkData;
#endif
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