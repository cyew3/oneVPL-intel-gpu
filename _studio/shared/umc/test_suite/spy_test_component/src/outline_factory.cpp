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

#include "outline_factory.h"
#include "xerces_outline_reader.h"
#include "xerces_outline_writer.h"
#include "checker.h"

#include "video_data_checker.h"

#include "outline_mfx_wrapper.h"
#include "outline_umc_wrapper.h"

class OutlineFactory : public OutlineFactoryAbstract
{
public:
    virtual OutlineReader * CreateReader();

    virtual VideoOutlineWriter * CreateVideoOutlineWriter();

    virtual CheckerBase * CreateChecker();

    virtual VideoDataChecker * GetUMCChecher();

    virtual OutlineVideoDecoder * GetUMCChecherWrapper(UMC::VideoDecoder * dec, const struct TestComponentParams * params);

    virtual VideoDataChecker_MFX * GetMFXChecher();

    virtual OutlineErrors * GetOutlineErrors();
};

OutlineReader * OutlineFactory::CreateReader()
{
    return new XercesOutlineReader();
}

VideoOutlineWriter * OutlineFactory::CreateVideoOutlineWriter()
{
    return new XercesVideoOutlineWriter();
}

CheckerBase * OutlineFactory::CreateChecker()
{
    return new Checker();
}

VideoDataChecker * OutlineFactory::GetUMCChecher()
{
    return new VideoDataChecker();
}

OutlineVideoDecoder * OutlineFactory::GetUMCChecherWrapper(UMC::VideoDecoder * dec, const struct TestComponentParams * params)
{
    return new OutlineVideoDecoder(dec, params);
}

VideoDataChecker_MFX * OutlineFactory::GetMFXChecher()
{
    return new VideoDataChecker_MFX();
}

OutlineErrors * OutlineFactory::GetOutlineErrors()
{
    static OutlineErrors errors;
    return &errors;
}

extern "C" OutlineFactoryAbstract * GetOutlineFactory()
{
    static OutlineFactory factory;
    return &factory;
}
