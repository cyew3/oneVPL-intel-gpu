// Copyright (c) 2002-2018 Intel Corporation
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

#ifndef __AAC_STATUS_H__
#define __AAC_STATUS_H__

#ifdef __cplusplus
extern "C" {
#endif

  typedef enum
  {
      AAC_OK = 0,
      AAC_NOT_ENOUGH_DATA,
      AAC_BAD_FORMAT,
      AAC_ALLOC,
      AAC_BAD_STREAM,
      AAC_NULL_PTR,
      AAC_NOT_FIND_SYNCWORD,
      AAC_NOT_ENOUGH_BUFFER,
      AAC_BAD_PARAMETER,
      AAC_UNSUPPORTED,
      HEAAC_UNSUPPORTED
  } AACStatus;

#ifdef __cplusplus
}
#endif

#endif
