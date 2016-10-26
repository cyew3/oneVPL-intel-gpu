//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2013 Intel Corporation. All Rights Reserved.
//

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