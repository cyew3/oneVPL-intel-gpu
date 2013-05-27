/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2012 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#include "utest_common.h"
#include "mfx_jpeg_encode_wrap.h"
#include "mfx_extended_buffer.h"

SUITE(JpegEncodeWrap)
{
    struct Init
    {
        MockVideoEncode *pEnc;
        TEST_METHOD_TYPE(MockVideoEncode::Query) args;
        MFXJpegEncWrap jpege;
        
        mfxExtCodingOption extCO;
        mfxExtVPPProcAmp proAmp;

        mfxVideoParam params;
        mfxExtBuffer *buf[2];

        Init()
            : jpege(pEnc = new MockVideoEncode())
            , params()
        {
            mfx_init_ext_buffer(extCO);
            mfx_init_ext_buffer(proAmp);
            //buffers are same if all fields are same
            proAmp.Brightness = 1;
            proAmp.Contrast = 2;
            proAmp.Hue = 3;
            proAmp.Saturation = 4;

            buf[0] = (mfxExtBuffer*)&proAmp;
            buf[1] = (mfxExtBuffer*)&extCO;
            

            params.ExtParam = buf;
            params.NumExtParam = 2;
        }
    };
    TEST_FIXTURE(Init, mfx_query_DETACH_ext_coding_options_ONLY_FRAMEPICTURE_SET)
    {
        extCO.FramePicture = MFX_CODINGOPTION_ON;

        mfxVideoParam paramsOrig;
        memcpy(&paramsOrig, &params, sizeof(mfxVideoParam));

        mfxStatus sts = jpege.Query(&params, NULL);

        CHECK_EQUAL(MFX_ERR_NONE, sts);
        CHECK_EQUAL(0, memcmp(&paramsOrig, &params, sizeof(mfxVideoParam)));

        //extco buffer shouldnot reach real decoder
        CHECK(pEnc->_Query.WasCalled(&args));
        CHECK_EQUAL(1u, args.value0->NumExtParam);

        mfxExtVPPProcAmp *pStatPtr = (mfxExtVPPProcAmp *)args.value0->ExtParam[0];
     
        CHECK_EQUAL(proAmp.Brightness, pStatPtr->Brightness);
        CHECK_EQUAL(proAmp.Contrast, pStatPtr->Contrast);
        CHECK_EQUAL(proAmp.Hue, pStatPtr->Hue);
        CHECK_EQUAL(proAmp.Saturation, pStatPtr->Saturation);
    }

    TEST_FIXTURE(Init, mfx_query_DETACH_ext_coding_options_ONLY_FRAMEPICTURE_SET2)
    {
        //swap order
        buf[1] = (mfxExtBuffer*)&proAmp;
        buf[0] = (mfxExtBuffer*)&extCO;

        extCO.FramePicture = MFX_CODINGOPTION_ON;

        mfxVideoParam paramsOrig;
        memcpy(&paramsOrig, &params, sizeof(mfxVideoParam));

        mfxStatus sts = jpege.Query(&params, NULL);

        CHECK_EQUAL(MFX_ERR_NONE, sts);
        CHECK_EQUAL(0, memcmp(&paramsOrig, &params, sizeof(mfxVideoParam)));

        //extco buffer shouldnot reach real decoder
        CHECK(pEnc->_Query.WasCalled(&args));
        CHECK_EQUAL(1u, args.value0->NumExtParam);

        mfxExtVPPProcAmp *pStatPtr = (mfxExtVPPProcAmp *)args.value0->ExtParam[0];

        CHECK_EQUAL(proAmp.Brightness, pStatPtr->Brightness);
        CHECK_EQUAL(proAmp.Contrast, pStatPtr->Contrast);
        CHECK_EQUAL(proAmp.Hue, pStatPtr->Hue);
        CHECK_EQUAL(proAmp.Saturation, pStatPtr->Saturation);
    }

    TEST_FIXTURE(Init, mfx_query_DETACH_ext_coding_options_ONLY_FRAMEPICTURE_SET3)
    {
        //swap order
        buf[0] = (mfxExtBuffer*)&extCO;
        params.NumExtParam = 1;

        extCO.FramePicture = MFX_CODINGOPTION_ON;

        mfxVideoParam paramsOrig;
        memcpy(&paramsOrig, &params, sizeof(mfxVideoParam));

        mfxStatus sts = jpege.Query(&params, NULL);

        CHECK_EQUAL(MFX_ERR_NONE, sts);
        CHECK_EQUAL(0, memcmp(&paramsOrig, &params, sizeof(mfxVideoParam)));

        //extco buffer shouldnot reach real decoder
        CHECK(pEnc->_Query.WasCalled(&args));
        CHECK_EQUAL(0, args.value0->NumExtParam);
    }

    TEST_FIXTURE(Init, mfx_query_DONOTDETACH_ext_coding_options_IF_NOTONLY_FRAMEPICTURE_SET)
    {
        extCO.FramePicture  = MFX_CODINGOPTION_ON;
        extCO.EndOfSequence = MFX_CODINGOPTION_ON;

        mfxVideoParam paramsOrig;
        memcpy(&paramsOrig, &params, sizeof(mfxVideoParam));

        mfxStatus sts = jpege.Query(&params, NULL);

        CHECK_EQUAL(MFX_ERR_NONE, sts);
        CHECK_EQUAL(0, memcmp(&paramsOrig, &params, sizeof(mfxVideoParam)));

        //extco buffer shouldnot reach real decoder
        CHECK(pEnc->_Query.WasCalled(&args));
        CHECK_EQUAL(2u, args.value0->NumExtParam);

        mfxExtVPPProcAmp *pStatPtr = (mfxExtVPPProcAmp *)args.value0->ExtParam[0];
        CHECK_EQUAL(proAmp.Brightness, pStatPtr->Brightness);
        CHECK_EQUAL(proAmp.Contrast, pStatPtr->Contrast);
        CHECK_EQUAL(proAmp.Hue, pStatPtr->Hue);
        CHECK_EQUAL(proAmp.Saturation, pStatPtr->Saturation);

        mfxExtCodingOption *pExtCoPtr = (mfxExtCodingOption *)args.value0->ExtParam[1];
        CHECK_EQUAL(extCO.FramePicture, pExtCoPtr->FramePicture);
        CHECK_EQUAL(extCO.EndOfSequence, pExtCoPtr->EndOfSequence);
        CHECK_EQUAL(extCO.MESearchType, pExtCoPtr->MESearchType);
    }

    TEST_FIXTURE(Init, mfx_query_no_ext_coding_options)
    {
        params.NumExtParam = 1;

        mfxVideoParam paramsOrig;
        memcpy(&paramsOrig, &params, sizeof(mfxVideoParam));

        mfxStatus sts = jpege.Query(&params, NULL);

        CHECK_EQUAL(MFX_ERR_NONE, sts);
        CHECK_EQUAL(0, memcmp(&paramsOrig, &params, sizeof(mfxVideoParam)));
        
        CHECK(pEnc->_Query.WasCalled(&args));
        CHECK_EQUAL(1u, args.value0->NumExtParam);

        mfxExtVPPProcAmp *pStatPtr = (mfxExtVPPProcAmp *)args.value0->ExtParam[0];
        
        CHECK_EQUAL(proAmp.Brightness, pStatPtr->Brightness);
        CHECK_EQUAL(proAmp.Contrast, pStatPtr->Contrast);
        CHECK_EQUAL(proAmp.Hue, pStatPtr->Hue);
        CHECK_EQUAL(proAmp.Saturation, pStatPtr->Saturation);
    }

    TEST_FIXTURE(Init, GET_VIDEO_PARAMS_buffer_size_modification)
    {
        params.NumExtParam = 0;

        params.mfx.FrameInfo.CropW  = 10;
        params.mfx.FrameInfo.CropH = 10;
        params.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;

        mfxStatus sts = jpege.GetVideoParam(&params);

        CHECK_EQUAL(5, params.mfx.BufferSizeInKB);

        params.mfx.FrameInfo.CropW  = 100;
        params.mfx.FrameInfo.CropH = 100;

        sts = jpege.GetVideoParam(&params);

        CHECK_EQUAL(44, params.mfx.BufferSizeInKB);

        params.mfx.FrameInfo.CropW  = 5000;
        params.mfx.FrameInfo.CropH = 5000;

        sts = jpege.GetVideoParam(&params);

        CHECK_EQUAL(65535, params.mfx.BufferSizeInKB);

    }
}