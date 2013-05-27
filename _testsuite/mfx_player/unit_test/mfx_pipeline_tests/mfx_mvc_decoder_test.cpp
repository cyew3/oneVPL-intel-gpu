/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011-2012 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#include "utest_common.h"
#include "mfx_mvc_decoder.h"
#include "mfx_serializer.h"

SUITE(MFXMVCDecoder)
{
    struct InitParams : public CleanTestEnvironment
    {
        MFXExtBufferVector m_InputExtBuffers;
        mfxVideoParam vParam;
        mfxBitstream2 zero;

        InitParams()
            : vParam()
            , zero()
        {
            zero.isNull = true;

            m_InputExtBuffers.push_back(new mfxExtMVCSeqDesc());
            MFXExtBufferPtr<mfxExtMVCSeqDesc> seqDesc(m_InputExtBuffers);

            bool bDeserial = false;
            //creating dependency structures
            {
                MFXStructure<mfxMVCViewDependency>  viewDependency;
                bDeserial = viewDependency.DeSerialize(VM_STRING("0 0 0 0 0"));
                seqDesc->View.push_back(viewDependency);
            }

            {
                MFXStructure<mfxMVCViewDependency>  viewDependency;
                bDeserial = viewDependency.DeSerialize(VM_STRING("1 1 0 0 0 0"));
                seqDesc->View.push_back(viewDependency);
            }

            {
                MFXStructure<mfxMVCViewDependency>  viewDependency;
                bDeserial = viewDependency.DeSerialize(VM_STRING("2 2 0 0 0 0 1"));
                seqDesc->View.push_back(viewDependency);
            }

            vParam.NumExtParam = (mfxU16)m_InputExtBuffers.size();
            vParam.ExtParam    = &m_InputExtBuffers;//vector is stored linear in memory
        }
    };

    TEST_FIXTURE(InitParams, DECODE_HEADER_HANDLE_MORE_DATA)
    {
        vParam.NumExtParam = 0;
        vParam.ExtParam = NULL;

        MockYUVSource *mockSource = new MockYUVSource();
        MVCDecoder * pDecoder = new MVCDecoder(false, vParam, mockSource);

        mfxVideoParam vParamLocal = {0};

        TEST_METHOD_TYPE(MockYUVSource::DecodeHeader) dec_header;
        //TODO:add check for no error output
        dec_header.ret_val = MFX_ERR_MORE_DATA;
        mockSource->_DecodeHeader.WillReturn(dec_header);
        CHECK_EQUAL(MFX_ERR_MORE_DATA, pDecoder->DecodeHeader(NULL, &vParamLocal));

        dec_header.ret_val = MFX_ERR_NOT_ENOUGH_BUFFER;
        mockSource->_DecodeHeader.WillReturn(dec_header);
        CHECK_EQUAL(MFX_ERR_NOT_ENOUGH_BUFFER, pDecoder->DecodeHeader(NULL, &vParamLocal));
    }

    TEST_FIXTURE(InitParams, DECODE_HEADER_WORKS_AS_NON_MVC)
    {
        vParam.NumExtParam = 0;
        vParam.ExtParam = NULL;

        MockYUVSource *mockSource = new MockYUVSource();
        MVCDecoder * pDecoder = new MVCDecoder(false, vParam, mockSource);

        mfxVideoParam vParamLocal = {0};

        CHECK_EQUAL(MFX_ERR_NONE, pDecoder->DecodeHeader(NULL, &vParamLocal));
        CHECK_EQUAL(0, vParamLocal.mfx.CodecProfile);
    }

    TEST_FIXTURE(InitParams, DECODE_HEADER_NO_BUFFERS)
    {
        MockYUVSource *mockSource = new MockYUVSource();
        MVCDecoder * pDecoder = new MVCDecoder(false, vParam, mockSource);

        mfxVideoParam vParamLocal = {0};

        CHECK_EQUAL(MFX_ERR_NONE, pDecoder->DecodeHeader(NULL, &vParamLocal));
        CHECK_EQUAL(MFX_PROFILE_AVC_MULTIVIEW_HIGH, vParamLocal.mfx.CodecProfile);
    }

    TEST_FIXTURE(InitParams, DECODE_HEADER_EXTMVC_NOT_ENOUGH_ALLOCATED)
    {
        MockYUVSource *mockSource = new MockYUVSource();
        MVCDecoder * pDecoder = new MVCDecoder(false, vParam, mockSource);

        mfxVideoParam vParamLocal = {0};

        MFXExtBufferVector extBuffers;
        extBuffers.push_back(new mfxExtMVCSeqDesc());

        vParamLocal.NumExtParam = (mfxU16)extBuffers.size();
        vParamLocal.ExtParam    = &extBuffers;

        CHECK_EQUAL(MFX_ERR_NOT_ENOUGH_BUFFER, pDecoder->DecodeHeader(NULL, &vParamLocal));
    }

    TEST_FIXTURE(InitParams, DECODE_HEADER_EXTMVC_ENOUGH_BUFERS)
    {
        MockYUVSource *mockSource = new MockYUVSource();
        MVCDecoder * pDecoder = new MVCDecoder(false, vParam, mockSource);

        mfxVideoParam vParamLocal = {0};

        MFXExtBufferVector extBuffers;
        extBuffers.push_back(new mfxExtMVCSeqDesc());

        vParamLocal.NumExtParam = (mfxU16)extBuffers.size();
        vParamLocal.ExtParam    = &extBuffers;

        CHECK_EQUAL(MFX_ERR_NOT_ENOUGH_BUFFER, pDecoder->DecodeHeader(NULL, &vParamLocal));
        
        MFXExtBufferPtr<mfxExtMVCSeqDesc> sequence(extBuffers);

        CHECK_EQUAL(3u, sequence.get()->NumView);
        
        {
            MFXExtBufferPtrRef<mfxExtMVCSeqDesc> seqDescriptionRef(*sequence);

            //we cannot create two ref objects because after commiting it is possible to overwrite parameters
            seqDescriptionRef->View.reserve(seqDescriptionRef->NumView);
            seqDescriptionRef->ViewId.reserve(seqDescriptionRef->NumViewId);
            seqDescriptionRef->OP.reserve(seqDescriptionRef->NumOP);
        }

        CHECK_EQUAL(MFX_ERR_NONE, pDecoder->DecodeHeader(NULL, &vParamLocal));

        MFXExtBufferPtr<mfxExtMVCSeqDesc> sequence2(extBuffers);


        CHECK_EQUAL(0, sequence2.get()->View[0].ViewId);
        CHECK_EQUAL(1, sequence2.get()->View[1].ViewId);
        CHECK_EQUAL(2, sequence2.get()->View[2].ViewId);

        CHECK_EQUAL(1, sequence2.get()->View[1].NumAnchorRefsL0);
        CHECK_EQUAL(2, sequence2.get()->View[2].NumAnchorRefsL0);

        CHECK_EQUAL(0, sequence2.get()->View[1].AnchorRefL0[0]);
        CHECK_EQUAL(0, sequence2.get()->View[2].AnchorRefL0[0]);
        CHECK_EQUAL(1, sequence2.get()->View[2].AnchorRefL0[1]);

        CHECK_EQUAL(1u, sequence2.get()->NumOP);
        CHECK_EQUAL(3, sequence2.get()->OP->NumTargetViews);
        
        //pointer for targetviews should be points into viewid array
        CHECK_EQUAL(sequence2.get()->ViewId, sequence2.get()->OP->TargetViewId);

        CHECK_EQUAL(0, sequence2.get()->OP->TargetViewId[0]);
        CHECK_EQUAL(1, sequence2.get()->OP->TargetViewId[1]);
        CHECK_EQUAL(2, sequence2.get()->OP->TargetViewId[2]);
    }

    TEST_FIXTURE(InitParams, DEcodeFrameAsync_GenerateViewIds)
    {
        MockYUVSource *mockSource = new MockYUVSource();
        MVCDecoder * pDecoder = new MVCDecoder(true, vParam, mockSource);

        TEST_METHOD_TYPE(MockYUVSource::DecodeFrameAsync) args;

        mfxFrameSurface1 srf, *psrf = &srf, *psrf2 = NULL;

        args.value0 =  NULL;
        args.value1 =  NULL;
        args.value2 = &psrf;
        args.value3 =  NULL;

        //4 return surfaces to wrap viewid index
        mockSource->_DecodeFrameAsync.WillReturn(args);
        mockSource->_DecodeFrameAsync.WillReturn(args);
        mockSource->_DecodeFrameAsync.WillReturn(args);
        mockSource->_DecodeFrameAsync.WillReturn(args);

        CHECK_EQUAL(MFX_ERR_NONE, pDecoder->DecodeFrameAsync(zero,NULL, &psrf2, NULL));
        CHECK_EQUAL(0, psrf2->Info.FrameId.ViewId);

        CHECK_EQUAL(MFX_ERR_NONE, pDecoder->DecodeFrameAsync(zero,NULL, &psrf2, NULL));
        CHECK_EQUAL(1, psrf2->Info.FrameId.ViewId);

        CHECK_EQUAL(MFX_ERR_NONE, pDecoder->DecodeFrameAsync(zero,NULL, &psrf2, NULL));
        CHECK_EQUAL(2, psrf2->Info.FrameId.ViewId);

        CHECK_EQUAL(MFX_ERR_NONE, pDecoder->DecodeFrameAsync(zero,NULL, &psrf2, NULL));
        CHECK_EQUAL(0, psrf2->Info.FrameId.ViewId);
    }

    TEST_FIXTURE(InitParams, DEcodeFrameAsync_NONGenerateViewIds)
    {
        MockYUVSource *mockSource = new MockYUVSource();
        MVCDecoder * pDecoder = new MVCDecoder(false, vParam, mockSource);

        TEST_METHOD_TYPE(MockYUVSource::DecodeFrameAsync) args;

        mfxFrameSurface1 *psrf2 = NULL;
        mfxFrameSurface1 srf[4], *psrf[4] = {&srf[0], &srf[1], &srf[2], &srf[3]};

        srf[0].Info.FrameId.ViewId = 9;
        srf[1].Info.FrameId.ViewId = 8;
        srf[2].Info.FrameId.ViewId = 7;
        srf[3].Info.FrameId.ViewId = 6;

        args.value0 = &zero;
        args.value1 =  NULL;
        args.value2 = &psrf[0];
        args.value3 =  NULL;

        //4 return surfaces to wrap viewid index
        mockSource->_DecodeFrameAsync.WillReturn(args);
        args.value2 = &psrf[1];
        mockSource->_DecodeFrameAsync.WillReturn(args);
        args.value2 = &psrf[2];
        mockSource->_DecodeFrameAsync.WillReturn(args);
        args.value2 = &psrf[3];
        mockSource->_DecodeFrameAsync.WillReturn(args);

        CHECK_EQUAL(MFX_ERR_NONE, pDecoder->DecodeFrameAsync(zero, NULL, &psrf2, NULL));
        CHECK_EQUAL(9, psrf2->Info.FrameId.ViewId);

        CHECK_EQUAL(MFX_ERR_NONE, pDecoder->DecodeFrameAsync(zero, NULL, &psrf2, NULL));
        CHECK_EQUAL(8, psrf2->Info.FrameId.ViewId);

        CHECK_EQUAL(MFX_ERR_NONE, pDecoder->DecodeFrameAsync(zero, NULL, &psrf2, NULL));
        CHECK_EQUAL(7, psrf2->Info.FrameId.ViewId);

        CHECK_EQUAL(MFX_ERR_NONE, pDecoder->DecodeFrameAsync(zero, NULL, &psrf2, NULL));
        CHECK_EQUAL(6, psrf2->Info.FrameId.ViewId);
    }

}