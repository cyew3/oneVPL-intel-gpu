/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2012 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#include "utest_common.h"
#include "mfx_bitstream_converters.h"
#include "mfx_destructor_counter.h"
#include "mfx_factory_default.h"
 
SUITE(BitstreamConverter)
{
    struct Init
    {
        BitstreamConverterFactory fac1;
        TEST_METHOD_TYPE(MockBitstreamConverter::GetTransformType) params_00;
        
        Init()
        {
            params_00.ret_val  = std::make_pair(0u,0u);
        }
        
        destructor_counter_tmpl<MockBitstreamConverter> *createBSC(int &refi, int a, int b)
        {
            destructor_counter_tmpl<MockBitstreamConverter> *d1 
                = new destructor_counter_tmpl<MockBitstreamConverter>(refi);

            d1->_GetTransformType.WillDefaultReturn(&params_00);

            TEST_METHOD_TYPE(MockBitstreamConverter::GetTransformType) params_ab(std::make_pair(a,b), 0);
            d1->_GetTransformType.WillReturn(params_ab);
            
            return d1;
        }
    };

    TEST_FIXTURE(Init, RELEASE_previous)
    {
        int nTimesCalled = 0;
        int nTimesCalled2 = 0;
        {
            BitstreamConverterFactory fac1;

            fac1.Register(createBSC(nTimesCalled, 1,2));
            fac1.Register(createBSC(nTimesCalled2, 1,2));

            CHECK_EQUAL(1, nTimesCalled);
        }
        CHECK_EQUAL(1, nTimesCalled2);
    }

    TEST_FIXTURE(Init, RELEASE_all)
    {
        int nTimesCalled = 0;
        int nTimesCalled2 = 0;
        {
            BitstreamConverterFactory fac1;

            fac1.Register(createBSC(nTimesCalled, 1,2));
            fac1.Register(createBSC(nTimesCalled2, 1,3));

            CHECK_EQUAL(0, nTimesCalled);
        }
        CHECK_EQUAL(1, nTimesCalled);
        CHECK_EQUAL(1, nTimesCalled2);
    }

    TEST_FIXTURE(Init, create)
    {
        int nTimesCalled  = 0;
        int nTimesCalled2 = 0;

        destructor_counter_tmpl<MockBitstreamConverter> *p1 = createBSC(nTimesCalled, 1,2);
        destructor_counter_tmpl<MockBitstreamConverter> *p2 = createBSC(nTimesCalled2, 1,3);

        {
            BitstreamConverterFactory fac1;

            fac1.Register(p1);
            fac1.Register(p2);

            CHECK(NULL != fac1.Create(1, 2));
            CHECK(NULL != fac1.Create(1, 3));
            CHECK_EQUAL((IBitstreamConverter*)NULL, fac1.Create(1, 4));
        }
    }

    struct DataInit
    {
        mfxU8 buf[40];
        mfxU8 srf_bufY[40];
        mfxU8 srf_bufUV[20];
        mfxU8 srf_bufV[20];
        mfxBitstream bs;
        mfxFrameSurface1 srf;

        DataInit()
            : bs()
            , srf()
        {
            mfxU8 _buf[] = {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23};
            memcpy(&buf, &_buf, (std::min)(sizeof(_buf), sizeof(buf)));
            MFX_ZERO_MEM(srf_bufY);
            MFX_ZERO_MEM(srf_bufUV);
            
            bs.Data = buf;
            bs.MaxLength = sizeof(buf);
            bs.DataLength = sizeof(buf);
            bs.DataOffset = 0;

            srf.Info.CropX = 2;
            srf.Info.CropY = 4;

            srf.Info.CropH = 4;
            srf.Info.CropW = 2;

            //cropw and croph testing
            srf.Info.Width = 21;
            srf.Info.Height = 41;

            srf.Data.Pitch  = 2;
            srf.Data.Y = srf_bufY;
            srf.Data.UV = srf_bufUV;
            srf.Data.U = srf_bufUV;
            srf.Data.V = srf_bufV;

        }
    };

    TEST_FIXTURE(DataInit, converters_registration)
    {
        struct converter_pair
        {
            int in_fmt;
            int out_fmt;
        } types_of_conversion[] = {
            {MFX_FOURCC_NV12, MFX_FOURCC_NV12},
            {MFX_FOURCC_YV12, MFX_FOURCC_NV12},
            {MFX_FOURCC_YV12, MFX_FOURCC_YV12},
            {MFX_FOURCC_RGB4, MFX_FOURCC_RGB4},
            {MFX_FOURCC_RGB3, MFX_FOURCC_RGB4},
            {MFX_FOURCC_RGB3, MFX_FOURCC_RGB3},
            {MFX_FOURCC_YUY2, MFX_FOURCC_YUY2}
        };

        MFXPipelineFactory pipeline_fac;
        std::auto_ptr<IBitstreamConverterFactory> ifac(pipeline_fac. CreateBitstreamCVTFactory(NULL));

        for (size_t i = 0; i < MFX_ARRAY_SIZE(types_of_conversion); i++)
        {
            CHECK((IBitstreamConverter*)NULL != ifac->Create(types_of_conversion[i].in_fmt, types_of_conversion[i].out_fmt));

            //notsupported converters
            CHECK_EQUAL(ifac->Create(MFX_FOURCC_RGB4, MFX_FOURCC_RGB3), (IBitstreamConverter*)NULL);
        }
    }

    TEST_FIXTURE(DataInit, NV12_NV12)
    {
        BSConvert<MFX_FOURCC_NV12, MFX_FOURCC_NV12> converter;

        //check not enoght bs
        mfxBitstream tmp = bs;
        tmp.DataLength = srf.Info.CropW * srf.Info.CropH * 3 /2 - 1;
        bs.DataLength = tmp.DataLength + 1;
        CHECK_EQUAL(MFX_ERR_MORE_DATA, converter.Transform(&tmp, &srf));
        
        int offset = 2 + 4 * srf.Data.Pitch;
        
        //same pitch
        CHECK_EQUAL(MFX_ERR_NONE, converter.Transform(&bs, &srf));
        CHECK_EQUAL(1, srf_bufY[0 + offset]);
        CHECK_EQUAL(2, srf_bufY[1 + offset]);
        CHECK_EQUAL(3, srf_bufY[2 + offset]);
        CHECK_EQUAL(4, srf_bufY[3 + offset]);
        CHECK_EQUAL(5, srf_bufY[4 + offset]);
        CHECK_EQUAL(6, srf_bufY[5 + offset]);
        CHECK_EQUAL(7, srf_bufY[6 + offset]);
        CHECK_EQUAL(8, srf_bufY[7 + offset]);
        
        offset = 2 + 2 * srf.Data.Pitch;

        CHECK_EQUAL(9, srf_bufUV[0 +  offset]);
        CHECK_EQUAL(10, srf_bufUV[1 + offset]);
        CHECK_EQUAL(11, srf_bufUV[2 + offset]);
        CHECK_EQUAL(12, srf_bufUV[3 + offset]);
    }

    TEST_FIXTURE(DataInit, NV12_NV12_DiffPitch)
    {
        BSConvert<MFX_FOURCC_NV12, MFX_FOURCC_NV12> converter;
        //diff pitch
        srf.Data.Pitch = 4;

        //check not enoght bs
        mfxBitstream tmp = bs;
        tmp.DataLength = srf.Info.CropW * srf.Info.CropH * 3 /2 - 1;
        bs.DataLength = tmp.DataLength + 1;
        CHECK_EQUAL(MFX_ERR_MORE_DATA, converter.Transform(&tmp, &srf));
        CHECK_EQUAL(MFX_ERR_NONE, converter.Transform(&bs, &srf));

        int offset = 2 + 4 * srf.Data.Pitch;

        CHECK_EQUAL(1, srf_bufY[0 + offset]);
        CHECK_EQUAL(2, srf_bufY[1 + offset]);
        CHECK_EQUAL(3, srf_bufY[4 + offset]);
        CHECK_EQUAL(4, srf_bufY[5 + offset]);
        CHECK_EQUAL(5, srf_bufY[8 + offset]);
        CHECK_EQUAL(6, srf_bufY[9 + offset]);
        CHECK_EQUAL(7, srf_bufY[12 + offset]);
        CHECK_EQUAL(8, srf_bufY[13 + offset]);

        offset = 2 + 2 * srf.Data.Pitch;
        
        CHECK_EQUAL(9,  srf_bufUV[0 + offset]);
        CHECK_EQUAL(10, srf_bufUV[1 + offset]);
        CHECK_EQUAL(11, srf_bufUV[4 + offset]);
        CHECK_EQUAL(12, srf_bufUV[5 + offset]);
    }

    TEST_FIXTURE(DataInit, YV12_NV12)
    {
        BSConvert<MFX_FOURCC_YV12, MFX_FOURCC_NV12> converter;

        //check not enought bs
        mfxBitstream tmp = bs;
        tmp.DataLength = srf.Info.CropW * srf.Info.CropH * 3 /2 - 1;
        bs.DataLength = tmp.DataLength + 1;
        CHECK_EQUAL(MFX_ERR_MORE_DATA, converter.Transform(&tmp, &srf));
        CHECK_EQUAL(MFX_ERR_NONE, converter.Transform(&bs, &srf));

        int offset = 2 + 4 * srf.Data.Pitch;
        //y same
        CHECK_EQUAL(1, srf_bufY[0 + offset]);
        CHECK_EQUAL(2, srf_bufY[1 + offset]);
        CHECK_EQUAL(3, srf_bufY[2 + offset]);
        CHECK_EQUAL(4, srf_bufY[3 + offset]);
        CHECK_EQUAL(5, srf_bufY[4 + offset]);
        CHECK_EQUAL(6, srf_bufY[5 + offset]);
        CHECK_EQUAL(7, srf_bufY[6 + offset]);
        CHECK_EQUAL(8, srf_bufY[7 + offset]);

        offset = 2 + 2 * srf.Data.Pitch;

        //uv combined
        CHECK_EQUAL(9,  srf_bufUV[0 + offset]);
        CHECK_EQUAL(11, srf_bufUV[1 + offset]);
        CHECK_EQUAL(10, srf_bufUV[2 + offset]);
        CHECK_EQUAL(12, srf_bufUV[3 + offset]);
    }
    
    TEST_FIXTURE(DataInit, YV12_NV12_DIFF_PITCH)
    {
        BSConvert<MFX_FOURCC_YV12, MFX_FOURCC_NV12> converter;
        //diff pitch
        srf.Data.Pitch = 4;
        //check not enoght bs
        mfxBitstream tmp = bs;
        tmp.DataLength = srf.Info.CropW * srf.Info.CropH * 3 /2 - 1;
        bs.DataLength  = tmp.DataLength + 1;
        CHECK_EQUAL(MFX_ERR_MORE_DATA, converter.Transform(&tmp, &srf));
        //now it should be enouf
        CHECK_EQUAL(MFX_ERR_NONE, converter.Transform(&bs, &srf));

        int offset = 2 + 4 * srf.Data.Pitch;
        //y same
        CHECK_EQUAL(1, srf_bufY[0 + offset]);
        CHECK_EQUAL(2, srf_bufY[1 + offset]);
        CHECK_EQUAL(3, srf_bufY[4 + offset]);
        CHECK_EQUAL(4, srf_bufY[5 + offset]);
        CHECK_EQUAL(5, srf_bufY[8 + offset]);
        CHECK_EQUAL(6, srf_bufY[9 + offset]);
        CHECK_EQUAL(7, srf_bufY[12 + offset]);
        CHECK_EQUAL(8, srf_bufY[13 + offset]);

        offset = 2 + 2 * srf.Data.Pitch;

        //uv combined
        CHECK_EQUAL(9,  srf_bufUV[0 + offset]);
        CHECK_EQUAL(11, srf_bufUV[1 + offset]);
        CHECK_EQUAL(10, srf_bufUV[4 + offset]);
        CHECK_EQUAL(12, srf_bufUV[5 + offset]);
    }

    TEST_FIXTURE(DataInit, YV12_YV12)
    {
        BSConvert<MFX_FOURCC_YV12, MFX_FOURCC_YV12> converter;
        
        //check not enoght bs
        mfxBitstream tmp = bs;
        tmp.DataLength = srf.Info.CropW * srf.Info.CropH * 3 /2 - 1;
        bs.DataLength  = tmp.DataLength + 1;
        CHECK_EQUAL(MFX_ERR_MORE_DATA, converter.Transform(&tmp, &srf));
        //now it should be enouf
        CHECK_EQUAL(MFX_ERR_NONE, converter.Transform(&bs, &srf));

        int offset = 2 + 4 * srf.Data.Pitch;
        //y same
        CHECK_EQUAL(1, srf_bufY[0 + offset]);
        CHECK_EQUAL(2, srf_bufY[1 + offset]);
        CHECK_EQUAL(3, srf_bufY[2 + offset]);
        CHECK_EQUAL(4, srf_bufY[3 + offset]);
        CHECK_EQUAL(5, srf_bufY[4 + offset]);
        CHECK_EQUAL(6, srf_bufY[5 + offset]);
        CHECK_EQUAL(7, srf_bufY[6 + offset]);
        CHECK_EQUAL(8, srf_bufY[7 + offset]);

        offset = 1 + 2 * (srf.Data.Pitch >> 1);

        //u same
        CHECK_EQUAL(9,  srf_bufUV[0 + offset]);
        CHECK_EQUAL(10, srf_bufUV[1 + offset]);
        
        //v same
        CHECK_EQUAL(11, srf_bufV[0 + offset]);
        CHECK_EQUAL(12, srf_bufV[1 + offset]);
    }

    TEST_FIXTURE(DataInit, YV12_YV12_DIFF_PITCH)
    {
        BSConvert<MFX_FOURCC_YV12, MFX_FOURCC_YV12> converter;
        //diff pitch
        srf.Data.Pitch = 4;
        //check not enoght bs
        mfxBitstream tmp = bs;
        tmp.DataLength = srf.Info.CropW * srf.Info.CropH * 3 /2 - 1;
        bs.DataLength  = tmp.DataLength + 1;
        CHECK_EQUAL(MFX_ERR_MORE_DATA, converter.Transform(&tmp, &srf));
        //now it should be enouf
        CHECK_EQUAL(MFX_ERR_NONE, converter.Transform(&bs, &srf));

        int offset = 2 + 4 * srf.Data.Pitch;
        //y 
        CHECK_EQUAL(1, srf_bufY[0 + offset]);
        CHECK_EQUAL(2, srf_bufY[1 + offset]);
        CHECK_EQUAL(3, srf_bufY[4 + offset]);
        CHECK_EQUAL(4, srf_bufY[5 + offset]);
        CHECK_EQUAL(5, srf_bufY[8 + offset]);
        CHECK_EQUAL(6, srf_bufY[9 + offset]);
        CHECK_EQUAL(7, srf_bufY[12 + offset]);
        CHECK_EQUAL(8, srf_bufY[13 + offset]);

        offset = 1 + 2 * (srf.Data.Pitch >> 1);

        //u 
        CHECK_EQUAL(9,  srf_bufUV[0 + offset]);
        CHECK_EQUAL(10, srf_bufUV[2 + offset]);

        //v 
        CHECK_EQUAL(11, srf_bufV[0 + offset]);
        CHECK_EQUAL(12, srf_bufV[2 + offset]);
    }    

    struct DataInitRgb : public DataInit
    {
        DataInitRgb ()
        {
            srf.Info.CropH  = 2;
            srf.Info.Height = 2;        

            srf.Data.Pitch  = 2 * 4;
            
            
            srf.Data.B = &srf_bufY[0];
            srf.Data.G = &srf_bufY[1];
            srf.Data.R = &srf_bufY[2];
            srf.Data.A = &srf_bufY[3];
            
        }
    };

    TEST_FIXTURE(DataInitRgb, RGB32_RGB32)
    {
        BSConvert<MFX_FOURCC_RGB4, MFX_FOURCC_RGB4> converter;
        
        //check not enoght bs
        mfxBitstream tmp = bs;
        tmp.DataLength = srf.Info.CropW * srf.Info.CropH * 4 - 1;
        bs.DataLength = tmp.DataLength + 1;
        CHECK_EQUAL(MFX_ERR_MORE_DATA, converter.Transform(&tmp, &srf));
        CHECK_EQUAL(MFX_ERR_NONE, converter.Transform(&bs, &srf));

        int offset = 2 * 4 + 4 * srf.Data.Pitch;

        CHECK_EQUAL(1, srf_bufY[0 + offset]);
        CHECK_EQUAL(2, srf_bufY[1 + offset]);
        CHECK_EQUAL(3, srf_bufY[2 + offset]);
        CHECK_EQUAL(4, srf_bufY[3 + offset]);
        
        CHECK_EQUAL(5, srf_bufY[4 + offset]);
        CHECK_EQUAL(6, srf_bufY[5 + offset]);
        CHECK_EQUAL(7, srf_bufY[6 + offset]);
        CHECK_EQUAL(8, srf_bufY[7 + offset]);

        CHECK_EQUAL(9, srf_bufY[8 + offset]);
        CHECK_EQUAL(10, srf_bufY[9 + offset]);
        CHECK_EQUAL(11, srf_bufY[10 + offset]);
        CHECK_EQUAL(12,  srf_bufY[11 + offset]);

        CHECK_EQUAL(13, srf_bufY[12 + offset]);
        CHECK_EQUAL(14, srf_bufY[13 + offset]);
        CHECK_EQUAL(15, srf_bufY[14 + offset]);
        CHECK_EQUAL(16, srf_bufY[15 + offset]);
    }

    TEST_FIXTURE(DataInitRgb, RGB32_RGB32_DIFF_PITCH)
    {
        BSConvert<MFX_FOURCC_RGB4, MFX_FOURCC_RGB4> converter;

        srf.Data.Pitch  = 3 * 4;

        //check not enoght bs
        mfxBitstream tmp = bs;
        tmp.DataLength = srf.Info.CropW * srf.Info.CropH * 4 - 1;
        bs.DataLength = tmp.DataLength + 1;
        CHECK_EQUAL(MFX_ERR_MORE_DATA, converter.Transform(&tmp, &srf));
        CHECK_EQUAL(MFX_ERR_NONE, converter.Transform(&bs, &srf));

        int offset = 2*4 + 4 * srf.Data.Pitch;

        CHECK_EQUAL(1, srf_bufY[0 + offset]);
        CHECK_EQUAL(2, srf_bufY[1 + offset]);
        CHECK_EQUAL(3, srf_bufY[2 + offset]);
        CHECK_EQUAL(4, srf_bufY[3 + offset]);

        CHECK_EQUAL(5, srf_bufY[4 + offset]);
        CHECK_EQUAL(6, srf_bufY[5 + offset]);
        CHECK_EQUAL(7, srf_bufY[6 + offset]);
        CHECK_EQUAL(8, srf_bufY[7 + offset]);

        CHECK_EQUAL(9, srf_bufY[4+8 + offset]);
        CHECK_EQUAL(10, srf_bufY[4+9 + offset]);
        CHECK_EQUAL(11, srf_bufY[4+10 + offset]);
        CHECK_EQUAL(12,  srf_bufY[4+11 + offset]);

        CHECK_EQUAL(13, srf_bufY[4+12 + offset]);
        CHECK_EQUAL(14, srf_bufY[4+13 + offset]);
        CHECK_EQUAL(15, srf_bufY[4+14 + offset]);
        CHECK_EQUAL(16, srf_bufY[4+15 + offset]);
    }

    TEST_FIXTURE(DataInitRgb, RGB24_RGB24)
    {
        BSConvert<MFX_FOURCC_RGB3, MFX_FOURCC_RGB3> converter;
        
        srf.Data.Pitch  = 2 * 3;

        //check not enoght bs
        mfxBitstream tmp = bs;
        tmp.DataLength = srf.Info.CropW * srf.Info.CropH * 3 - 1;
        bs.DataLength = tmp.DataLength + 1;
        CHECK_EQUAL(MFX_ERR_MORE_DATA, converter.Transform(&tmp, &srf));
        CHECK_EQUAL(MFX_ERR_NONE, converter.Transform(&bs, &srf));

        int offset = 2*3 + 4 * srf.Data.Pitch;

        CHECK_EQUAL(1, srf_bufY[0 + offset]);
        CHECK_EQUAL(2, srf_bufY[1 + offset]);
        CHECK_EQUAL(3, srf_bufY[2 + offset]);

        CHECK_EQUAL(4, srf_bufY[3 + offset]);
        CHECK_EQUAL(5, srf_bufY[4 + offset]);
        CHECK_EQUAL(6, srf_bufY[5 + offset]);

        CHECK_EQUAL(7, srf_bufY[6 + offset]);
        CHECK_EQUAL(8, srf_bufY[7 + offset]);
        CHECK_EQUAL(9, srf_bufY[8 + offset]);
        
        CHECK_EQUAL(10, srf_bufY[9 + offset]);
        CHECK_EQUAL(11, srf_bufY[10 + offset]);
        CHECK_EQUAL(12,  srf_bufY[11 + offset]);
    }

    TEST_FIXTURE(DataInitRgb, RGB24_RGB24_DIFF_PITCH)
    {
        BSConvert<MFX_FOURCC_RGB3, MFX_FOURCC_RGB3> converter;

        srf.Data.Pitch  = 3 * 3;

        //check not enoght bs
        mfxBitstream tmp = bs;
        tmp.DataLength = srf.Info.CropW * srf.Info.CropH * 3 - 1;
        bs.DataLength = tmp.DataLength + 1;
        CHECK_EQUAL(MFX_ERR_MORE_DATA, converter.Transform(&tmp, &srf));
        CHECK_EQUAL(MFX_ERR_NONE, converter.Transform(&bs, &srf));

        int offset = 2*3 + 4 * srf.Data.Pitch;

        CHECK_EQUAL(1, srf_bufY[0 + offset]);
        CHECK_EQUAL(2, srf_bufY[1 + offset]);
        CHECK_EQUAL(3, srf_bufY[2 + offset]);

        CHECK_EQUAL(4, srf_bufY[3 + offset]);
        CHECK_EQUAL(5, srf_bufY[4 + offset]);
        CHECK_EQUAL(6, srf_bufY[5 + offset]);

        CHECK_EQUAL(7, srf_bufY[3+6 + offset]);
        CHECK_EQUAL(8, srf_bufY[3+7 + offset]);
        CHECK_EQUAL(9, srf_bufY[3+8 + offset]);

        CHECK_EQUAL(10, srf_bufY[3+9 + offset]);
        CHECK_EQUAL(11, srf_bufY[3+10 + offset]);
        CHECK_EQUAL(12, srf_bufY[3+11 + offset]);
    }

    TEST_FIXTURE(DataInitRgb, RGB24_RGB32)
    {
        BSConvert<MFX_FOURCC_RGB3, MFX_FOURCC_RGB4> converter;

        //check not enoght bs
        mfxBitstream tmp = bs;
        tmp.DataLength = srf.Info.CropW * srf.Info.CropH * 3 - 1;
        bs.DataLength = tmp.DataLength + 1;
        CHECK_EQUAL(MFX_ERR_MORE_DATA, converter.Transform(&tmp, &srf));
        CHECK_EQUAL(MFX_ERR_NONE, converter.Transform(&bs, &srf));

        int offset = 2*4 + 4 * srf.Data.Pitch;

        CHECK_EQUAL(1, srf_bufY[0 + offset]);
        CHECK_EQUAL(2, srf_bufY[1 + offset]);
        CHECK_EQUAL(3, srf_bufY[2 + offset]);
        CHECK_EQUAL(0, srf_bufY[3 + offset]);

        CHECK_EQUAL(4, srf_bufY[4 + offset]);
        CHECK_EQUAL(5, srf_bufY[5 + offset]);
        CHECK_EQUAL(6, srf_bufY[6 + offset]);
        CHECK_EQUAL(0, srf_bufY[7 + offset]);

        CHECK_EQUAL(7, srf_bufY[8 + offset]);
        CHECK_EQUAL(8, srf_bufY[9 + offset]);
        CHECK_EQUAL(9, srf_bufY[10 + offset]);
        CHECK_EQUAL(0, srf_bufY[11 + offset]);

        CHECK_EQUAL(10, srf_bufY[12 + offset]);
        CHECK_EQUAL(11, srf_bufY[13 + offset]);
        CHECK_EQUAL(12, srf_bufY[14 + offset]);
        CHECK_EQUAL(0, srf_bufY[15 + offset]);
    }

    TEST_FIXTURE(DataInitRgb, RGB24_RGB32_DIFF_PITCH)
    {
        BSConvert<MFX_FOURCC_RGB3, MFX_FOURCC_RGB4> converter;

        srf.Data.Pitch  = 4 * 3;

        //check not enoght bs
        mfxBitstream tmp = bs;
        tmp.DataLength = srf.Info.CropW * srf.Info.CropH * 3 - 1;
        bs.DataLength = tmp.DataLength + 1;
        CHECK_EQUAL(MFX_ERR_MORE_DATA, converter.Transform(&tmp, &srf));
        CHECK_EQUAL(MFX_ERR_NONE, converter.Transform(&bs, &srf));

        int offset = 2 * 4 + 4 * srf.Data.Pitch;

        CHECK_EQUAL(1, srf_bufY[0 + offset]);
        CHECK_EQUAL(2, srf_bufY[1 + offset]);
        CHECK_EQUAL(3, srf_bufY[2 + offset]);
        CHECK_EQUAL(0, srf_bufY[3 + offset]);

        CHECK_EQUAL(4, srf_bufY[4 + offset]);
        CHECK_EQUAL(5, srf_bufY[5 + offset]);
        CHECK_EQUAL(6, srf_bufY[6 + offset]);
        CHECK_EQUAL(0, srf_bufY[7 + offset]);

        CHECK_EQUAL(7, srf_bufY[4+8 + offset]);
        CHECK_EQUAL(8, srf_bufY[4+9 + offset]);
        CHECK_EQUAL(9, srf_bufY[4+10 + offset]);
        CHECK_EQUAL(0, srf_bufY[4+11 + offset]);

        CHECK_EQUAL(10, srf_bufY[4+12 + offset]);
        CHECK_EQUAL(11, srf_bufY[4+13 + offset]);
        CHECK_EQUAL(12, srf_bufY[4+14 + offset]);
        CHECK_EQUAL(0, srf_bufY[15 + offset]);
    }
}
