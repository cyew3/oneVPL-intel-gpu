// Copyright (c) 2016-2018 Intel Corporation
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

#pragma once

#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define CLIPVAL(VAL, MINVAL, MAXVAL) MAX(MINVAL, MIN(MAXVAL, VAL))
#define CHECK_CM_ERR(ERR) if ((ERR) != CM_SUCCESS) return FAILED;
#define CHECK_ERR(ERR) if ((ERR) != PASSED) return (ERR);
#define DIVUP(a, b) ((a+b-1)/b)

#define BORDER          4
#define WIDTHB (WIDTH + BORDER*2)
#define HEIGHTB (HEIGHT + BORDER*2)

enum { PASSED, FAILED };

const int WIDTH  = 416;
const int HEIGHT = 256; // must be multiple of 32 for cases where FIELD2TFF, FIELD2BFF, TFF2FIELD, BFF2FIELD
const char YUV_NAME_W[] = "./test_data/inwhite_1fr_di.yuv";
const char YUV_NAME_B[] = "./test_data/inblack_1fr_di.yuv";
const char YUV_NAME_G[] = "./test_data/ingrey_1fr_di.yuv";

