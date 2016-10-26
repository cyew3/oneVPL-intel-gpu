//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2012 Intel Corporation. All Rights Reserved.
//

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
