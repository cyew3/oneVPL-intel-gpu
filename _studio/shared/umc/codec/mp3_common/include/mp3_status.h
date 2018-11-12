// Copyright (c) 2005-2018 Intel Corporation
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

#ifndef __MP3_STATUS_H__
#define __MP3_STATUS_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    MP3_OK = 0,
    MP3_NOT_ENOUGH_DATA,
    MP3_BAD_FORMAT,
    MP3_ALLOC,
    MP3_BAD_STREAM,
    MP3_NULL_PTR,
    MP3_NOT_FIND_SYNCWORD,
    MP3_NOT_ENOUGH_BUFFER,
    MP3_FAILED_TO_INITIALIZE,
    MP3_UNSUPPORTED,
    MP3_END_OF_STREAM
} MP3Status;

#ifdef __cplusplus
}
#endif

#endif
