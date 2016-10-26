//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2014 Intel Corporation. All Rights Reserved.
//

#include "pfc.h"
#include "../telecine/telecine.h"
#include <assert.h> 

#if (defined(LINUX32) || defined(LINUX64))
#ifndef min
#define min(a,b)            (((a) < (b)) ? (a) : (b))
#endif
#ifndef max
#define max(a,b)            (((a) > (b)) ? (a) : (b))
#endif
#endif

static const float pi = 3.14159265f;
static const float sF0[] = {0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f};
static const float sF1[] = {0.127323954f, -0.212206591f, 0.636619772f, 0.636619772f, -0.212206591f, 0.127323954f};

static const int CCIR[] = {-29,0,88,138,88,0,-29,32767,65536};

#define CLIP(i) min(max(i, 0), 255)

void Filter_CCIR601(Plane *plaOut, Plane *plaIn, int *data)
{
    int    i, j, k;

    for (j = 0; j < (int)plaIn->uiHeight; ++j)
    {
        for (i = 0; i < (int)plaOut->uiWidth; ++i)
        {
            int Qi = 2 * i + j * plaIn->uiStride;
            int Qo = i + (j + 3) * plaOut->uiWidth;
            int total = 0;
            for (k = 0; k < 7; k++)
                total += plaIn->ucCorner[Qi + k - 3] * CCIR[k];
            data[Qo] = total;
        }
    }

    for (j = 0; j < (int)plaOut->uiHeight; ++j)
    {
        for (i = 0; i < (int)plaOut->uiWidth; ++i)
        {
            int Qi = i + 2 * j * plaOut->uiWidth;
            int total = 0;
            for (k = 0; k < 7; k++)
                total += data[Qi + k * plaOut->uiWidth] * CCIR[k];

            plaOut->ucCorner[i + j * plaOut->uiStride] = (BYTE)CLIP((total + CCIR[7]) / CCIR[8]);
        }
    }
}
static __inline float sinc(float x)
{
    return (x == 0) ? 1.0f : sinf(pi * x) / (pi * x);
}
static void Filter_sinc(Plane *plaOut, Plane *plaIn)
{
    int    i, j, m, n, r = 3;
    float scaleX = (float)plaIn->uiWidth / (float)plaOut->uiWidth;
    float scaleY = (float)plaIn->uiHeight / (float)plaOut->uiHeight;
    int mode = 0;
    if (scaleX == 1.5f && scaleY == 1.5f)
        mode = 1;

    for (j = 0; j < (int)plaOut->uiHeight; ++j)
    {
        for (i = 0; i < (int)plaOut->uiWidth; ++i)
        {
            int qOut = i + j * (int)plaOut->uiStride;
            float x = (float)i * scaleX;
            float y = (float)j * scaleY;
            int X = (int)x;
            int Y = (int)y;
            int sX = max(0, X - r + 1);
            int sY = max(0, Y - r + 1);
            int tX = min(X + r + 1, (int)plaIn->uiWidth);
            int tY = min(Y + r + 1, (int)plaIn->uiHeight);
            float total = 0.0f;
            float totalc = 0.0f;
            if (mode)
            {
                float *sincFX = (float *)sF1;
                float *sincFY = (float *)sF1;
                if (x == X)
                    sincFX = (float *)sF0;
                if (y == Y)
                    sincFY = (float *)sF0;

                for (n = sY; n < tY; ++n)
                {
                    for (m = sX; m < tX; ++m)
                    {
                        float coeff = sincFX[m - X + 2] * sincFY[n - Y + 2];
                        total += plaIn->ucCorner[m + n * (int)plaIn->uiStride] * coeff;
                        totalc += coeff;
                    }
                }
            }
            else
            {
                for (n = sY; n < tY; ++n)
                {
                    for (m = sX; m < tX; ++m)
                    {
                        float coeff = sinc((x - (float)m)) * sinc((y - (float)n));
                        total += plaIn->ucCorner[m + n * (int)plaIn->uiStride] * coeff;
                        totalc += coeff;
                    }
                }
            }
            plaOut->ucCorner[qOut] = (unsigned char)(max(0, min(total / totalc, 255)));
        }
    }
}
static void Filter_bilinear(Plane *plaOut, Plane *plaIn)
{
    int    i, j;
    for (j = 0; j < (int)plaOut->uiHeight; ++j)
    {
        for (i = 0; i < (int)plaOut->uiWidth; ++i)
        {
            int qOut = i + j * (int)plaOut->uiStride;
            float x = (float)plaIn->uiWidth * (float)i / (float)plaOut->uiWidth;
            float y = (float)plaIn->uiHeight * (float)j / (float)plaOut->uiHeight;
            int X = (int)x;
            int Y = (int)y;
            float s = x - X;
            float t = y - Y;
            int qIn = X + Y * (int)plaIn->uiStride;
            if (X + 1 < (int)plaIn->uiWidth && Y + 1 < (int)plaIn->uiHeight)
            {
                plaOut->ucCorner[qOut] = (unsigned char)(
                    plaIn->ucCorner[qIn] * (1.0f - s) * (1.0f - t) + 
                    plaIn->ucCorner[qIn + 1] * (s) * (1.0f - t) + 
                    plaIn->ucCorner[qIn + (int)plaIn->uiStride] * (1.0f - s) * (t) + 
                    plaIn->ucCorner[qIn + (int)plaIn->uiStride + 1] * (s) * (t));
            }
            else
            {
                if (X + 1 < (int)plaIn->uiWidth)
                {
                    plaOut->ucCorner[qOut] = (unsigned char)(
                        plaIn->ucCorner[qIn] * (1.0f - s) + 
                        plaIn->ucCorner[qIn + 1] * (s));
                }
                else if (Y + 1 < (int)plaIn->uiHeight)
                {
                    plaOut->ucCorner[qOut] = (unsigned char)(
                        plaIn->ucCorner[qIn] * (1.0f - t) + 
                        plaIn->ucCorner[qIn + (int)plaIn->uiStride] * (t));
                }
                else
                    plaOut->ucCorner[qOut] = plaIn->ucCorner[qIn];
            }
        }
    }
}
static void Filter_block(Plane *plaOut, Plane *plaIn)
{
    int    i, j, m, n;
    for (j = 0; j < (int)plaOut->uiHeight; ++j)
    {
        for (i = 0; i < (int)plaOut->uiWidth; ++i)
        {
            int qOut = i + j * (int)plaOut->uiStride;
            int x1 = (int)((float)plaIn->uiWidth * (float)i / (float)plaOut->uiWidth);
            int y1 = (int)((float)plaIn->uiHeight * (float)j / (float)plaOut->uiHeight);
            int x2 = (int)((float)plaIn->uiWidth * (float)(i+1) / (float)plaOut->uiWidth);
            int y2 = (int)((float)plaIn->uiHeight * (float)(j+1) / (float)plaOut->uiHeight);
            int total = 0;

            if (x2 == x1) x2 = x1 + 1;
            if (y2 == y1) y2 = y1 + 1;

            for (n = y1; n < min(y2, (int)plaIn->uiHeight); ++n)
            {
                for (m = x1; m < min(x2, (int)plaIn->uiWidth); ++m)
                {
                    int qIn = m + n * (int)plaIn->uiStride;
                    total += plaIn->ucCorner[qIn];
                }
            }
            plaOut->ucCorner[qOut] = total / ((x2 - x1) * (y2 - y1));
        }
    }
}
static __inline float lerp(float P0, float P1, float P2, float P3, float s, float is)
{
    return is * is * is * P0 + 3 * is * s * (is * P1 + s * P2) + s * s * s * P3;
}
static void Filter_gradient(Plane *plaOut, Plane *plaIn)
{
    const float w[] = {4.0f, 2.0f, 1.0f};
    const float tw = 1.0f / (7.0f * 3.0f);

    int    i, j;
    int s = plaIn->uiWidth * plaIn->uiHeight * sizeof(float);
    float *gradX = 0;
    float *gradY = 0;
    float rx = 0;
    float ry = 0;
    gradX = (float *)malloc(s);
    if(!gradX)
        return;
    gradY = (float *)malloc(s);
    if(!gradY)
    {
        free(gradX);
        return;
    }

    rx = (float)plaIn->uiWidth / (float)plaOut->uiWidth;
    ry = (float)plaIn->uiHeight / (float)plaOut->uiHeight;

    memset(gradX, 0, s);
    memset(gradY, 0, s);

    for (j = 3; j < (int)plaIn->uiHeight - 3; ++j)
    {
        for (i = 3; i < (int)plaIn->uiWidth - 3; ++i)
        {
            int qIn = i + j * (int)plaIn->uiWidth;
            int k;
            float sx = 0, sy = 0;
            for (k = 3; k > 0; --k)
            {
                sy -= w[k - 1] * plaIn->ucCorner[(i) + (j - k) * (int)plaIn->uiWidth];
                sx -= w[k - 1] * plaIn->ucCorner[(i - k) + (j) * (int)plaIn->uiWidth];
                sx += w[k - 1] * plaIn->ucCorner[(i + k) + (j) * (int)plaIn->uiWidth];
                sy += w[k - 1] * plaIn->ucCorner[(i) + (j + k) * (int)plaIn->uiWidth];
            }

            gradX[qIn] = sx * tw * rx;
            gradY[qIn] = sy * tw * ry;
        }
    }

    for (j = 0; j < (int)plaOut->uiHeight; ++j)
    {
        for (i = 0; i < (int)plaOut->uiWidth; ++i)
        {

            int qOut = i + j * (int)plaOut->uiStride;
            float x = rx * (float)i;
            float y = ry * (float)j;
            int X = (int)x;
            int Y = (int)y;
            int qIn = X + Y * (int)plaIn->uiStride;

            float s = x - X;
            float is = 1 - s;
            float t = y - Y;
            float it = 1 - t;

            float result1, result2, result;
            float P0, P1, P2, P3;
            if (X < (int)plaIn->uiWidth - 1 && Y < (int)plaIn->uiHeight - 1)
            {
                P0 = (float)plaIn->ucCorner[qIn];
                P3 = (float)plaIn->ucCorner[qIn + 1];
                P1 = P0 + gradX[qIn];
                P2 = P3 - gradX[qIn + 1];

                result1 = lerp(P0, P1, P2, P3, s, is);

                P0 = (float)plaIn->ucCorner[qIn + (int)plaIn->uiStride];
                P3 = (float)plaIn->ucCorner[qIn + 1 + (int)plaIn->uiStride];
                P1 = P0 + gradX[qIn + (int)plaIn->uiStride];
                P2 = P3 - gradX[qIn + 1 + (int)plaIn->uiStride];

                result2 = lerp(P0, P1, P2, P3, s, is);

                P0 = result1;
                P3 = result2;
                P1 = P0 + is * gradY[qIn] + s * gradY[qIn + 1];
                P2 = P3 - is * gradY[qIn + (int)plaIn->uiStride] - s * gradY[qIn + 1 + (int)plaIn->uiStride];

                result = lerp(P0, P1, P2, P3, t, it);
            }
            else if (X < (int)plaIn->uiWidth - 1)
            {
                P0 = (float)plaIn->ucCorner[qIn];
                P3 = (float)plaIn->ucCorner[qIn + 1];
                P1 = P0 + gradX[qIn];
                P2 = P3 - gradX[qIn + 1];

                result1 = lerp(P0, P1, P2, P3, s, is);

                P0 = result1;
                P1 = P0 + 3.0f * (is * gradY[qIn] + s * gradY[qIn + 1]);

                result = it * P0 + t * P1;
            }
            else if (Y < (int)plaIn->uiHeight - 1)
            {
                P0 = (float)plaIn->ucCorner[qIn];
                P1 = P0 + 3.0f * gradX[qIn];

                result1 = is * P0 + s * P1;

                P0 = (float)plaIn->ucCorner[qIn + (int)plaIn->uiStride];
                P1 = P0 + 3.0f * gradX[qIn + (int)plaIn->uiStride];

                result2 = is * P0 + s * P1;

                P0 = result1;
                P3 = result2;
                P1 = P0 + gradY[qIn];
                P2 = P3 - gradY[qIn + (int)plaIn->uiStride];

                result = lerp(P0, P1, P2, P3, t, it);
            }
            else
            {
                P0 = (float)plaIn->ucCorner[qIn];
                P1 = P0 + 3.0f * gradX[qIn];

                result1 = is * P0 + s * P1;

                P0 = result1;
                P1 = P0 + 3.0f * gradY[qIn];

                result = it * P0 + t * P1;
            }

            plaOut->ucCorner[qOut] = (unsigned char)max(0, min(result + 0.5f, 255));
        }
    }

    free(gradX);
    free(gradY);
}
static void Filter_cos(Plane *plaOut, Plane *plaIn)
{
    const float pi = 3.141592f;
    int    i, j;

    float scaleX = (float)plaIn->uiWidth / (float)plaOut->uiWidth;
    float scaleY = (float)plaIn->uiHeight / (float)plaOut->uiHeight;

    for (j = 0; j < (int)plaOut->uiHeight; ++j)
    {
        for (i = 0; i < (int)plaOut->uiWidth; ++i)
        {
            int qOut = i + j * (int)plaOut->uiStride;
            float x = (float)i * scaleX;
            float y = (float)j * scaleY;
            int X = (int)x;
            int Y = (int)y;
            int qIn = X + Y * (int)plaIn->uiStride;
            float s = cosf(pi * (x - (float)X)) * 0.5f + 0.5f;
            float t = cosf(pi * (y - (float)Y)) * 0.5f + 0.5f;

            if (X + 1 < (int)plaIn->uiWidth && Y + 1 < (int)plaIn->uiHeight)
            {
                plaOut->ucCorner[qOut] =(unsigned char)(
                    (s) * (t) * plaIn->ucCorner[qIn] + 
                    (1.0f - s) * (t) * plaIn->ucCorner[qIn + 1] + 
                    (s) * (1.0f - t) * plaIn->ucCorner[qIn + plaIn->uiWidth] + 
                    (1.0f - s) * (1.0f - t) * plaIn->ucCorner[qIn + 1 + plaIn->uiWidth]);
            }
            else if (X + 1 < (int)plaIn->uiWidth)
            {
                plaOut->ucCorner[qOut] = (unsigned char)(
                    plaIn->ucCorner[qIn] * (s) + 
                    plaIn->ucCorner[qIn + 1] * (1.0f - s));
            }
            else if (Y + 1 < (int)plaIn->uiHeight)
            {
                plaOut->ucCorner[qOut] = (unsigned char)(
                    plaIn->ucCorner[qIn] * (t) + 
                    plaIn->ucCorner[qIn + plaIn->uiWidth] * (1.0f - t));
            }
            else
                plaOut->ucCorner[qOut] = plaIn->ucCorner[qIn];
        }
    }
}
void ReSample(Frame *frmOut, Frame *frmIn)
{
    Plane plaTemp;
    int filter;

    if (frmOut->plaY.uiWidth > frmIn->plaY.uiWidth)
        filter = 3;
    else
        filter = 0;

    if (frmOut->plaY.uiWidth != frmIn->plaY.uiWidth || frmOut->plaY.uiHeight != frmIn->plaY.uiHeight)
    {
        Plane_Create(&plaTemp, frmOut->plaY.uiWidth, frmOut->plaY.uiHeight, 0);

        if (filter == 0)
            Filter_sinc(&plaTemp, &frmIn->plaY);
        else if (filter == 1)
            Filter_bilinear(&plaTemp, &frmIn->plaY);
        else if (filter == 2)
            Filter_block(&plaTemp, &frmIn->plaY);
        else if (filter == 3)
            Filter_gradient(&plaTemp, &frmIn->plaY);
        else if (filter == 4)
            Filter_cos(&plaTemp, &frmIn->plaY);

        AddBorders(&plaTemp, &frmOut->plaY, frmOut->plaY.uiBorder);
        Plane_Release(&plaTemp);
    }
    else
        AddBorders(&frmIn->plaY, &frmOut->plaY, frmOut->plaY.uiBorder);

    if (frmOut->plaU.uiWidth != frmIn->plaU.uiWidth || frmOut->plaU.uiHeight != frmIn->plaU.uiHeight)
    {
        Plane_Create(&plaTemp, frmOut->plaU.uiWidth, frmOut->plaU.uiHeight, 0);

        if (filter == 0)
            Filter_sinc(&plaTemp, &frmIn->plaU);
        else if (filter == 1)
            Filter_bilinear(&plaTemp, &frmIn->plaU);
        else if (filter == 2)
            Filter_block(&plaTemp, &frmIn->plaU);
        else if (filter == 3)
            Filter_gradient(&plaTemp, &frmIn->plaU);
        else if (filter == 4)
            Filter_cos(&plaTemp, &frmIn->plaU);

        AddBorders(&plaTemp, &frmOut->plaU, frmOut->plaU.uiBorder);
        Plane_Release(&plaTemp);
    }
    else
        AddBorders(&frmIn->plaU, &frmOut->plaU, frmOut->plaU.uiBorder);

    if (frmOut->plaV.uiWidth != frmIn->plaV.uiWidth || frmOut->plaV.uiHeight != frmIn->plaV.uiHeight)
    {
        Plane_Create(&plaTemp, frmOut->plaV.uiWidth, frmOut->plaV.uiHeight, 0);

        if (filter == 0)
            Filter_sinc(&plaTemp, &frmIn->plaV);
        else if (filter == 1)
            Filter_bilinear(&plaTemp, &frmIn->plaV);
        else if (filter == 2)
            Filter_block(&plaTemp, &frmIn->plaV);
        else if (filter == 3)
            Filter_gradient(&plaTemp, &frmIn->plaV);
        else if (filter == 4)
            Filter_cos(&plaTemp, &frmIn->plaV);

        AddBorders(&plaTemp, &frmOut->plaV, frmOut->plaV.uiBorder);
        Plane_Release(&plaTemp);
    }
    else
        AddBorders(&frmIn->plaV, &frmOut->plaV, frmOut->plaV.uiBorder);
    ptir_memcpy(frmOut->plaY.ucStats.ucSAD,frmIn->plaY.ucStats.ucSAD,sizeof(double) * 5);
    frmOut->frmProperties.tindex = frmIn->frmProperties.tindex;
}