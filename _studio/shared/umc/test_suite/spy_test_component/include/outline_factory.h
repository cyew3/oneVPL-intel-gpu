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

#ifndef __OUTLINE_FACTORY_H__
#define __OUTLINE_FACTORY_H__

#include "outline.h"

class OutlineReader;
class VideoOutlineWriter;
class CheckerBase;
class VideoDataChecker;
class VideoDataChecker_MFX;
class OutlineVideoDecoder;

class OutlineFactoryAbstract
{
public:
    virtual ~OutlineFactoryAbstract() {};

    virtual OutlineReader * CreateReader() = 0;

    virtual VideoOutlineWriter * CreateVideoOutlineWriter() = 0;

    virtual CheckerBase * CreateChecker() = 0;

    virtual VideoDataChecker * GetUMCChecher() = 0;

    virtual OutlineVideoDecoder * GetUMCChecherWrapper(UMC::VideoDecoder * dec, const struct TestComponentParams * params) = 0;

    virtual VideoDataChecker_MFX * GetMFXChecher() = 0;

    virtual OutlineErrors * GetOutlineErrors() = 0;
};

extern "C" OutlineFactoryAbstract * GetOutlineFactory();

#endif // __OUTLINE_FACTORY_H__