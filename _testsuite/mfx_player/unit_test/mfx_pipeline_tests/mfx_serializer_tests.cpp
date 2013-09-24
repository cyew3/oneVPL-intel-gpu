/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2010 - 2013 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#include "utest_common.h"
#include "mfx_serializer.h"
#include "mfx_serializer_test.h"

SUITE(MFXSerializer)
{
     TEST(EncoderInitReport)
     {
         Items vParam ;
         vParam.resize(5);

         vParam[0] = 0;
         vParam[1] = 10;
         vParam[2] = 100;
         vParam[3] = 0;
         vParam[4] = 0;

         Items vParamNew;
         vParamNew.resize(5);

         vParamNew[0] = 0;
         vParamNew[1] = 11;
         vParamNew[2] = 101;

         Items vParamUser ;
         vParamUser.resize(4);

         vParamUser[1] = 1;//it is only a flag
         vParamUser[3] = 1;//it is only a flag

         MFXStructureThree<Items> structWrp(vParam, vParamNew, vParamUser);
         tstring str_to_print = structWrp.Serialize(MockFormater());
         tstringstream stream;

         stream<<VM_STRING("0:0    ")<<std::endl;
         stream<<VM_STRING("1:10   (10) - user")<<std::endl;
         stream<<VM_STRING("2:101  (100)")<<std::endl;
         stream<<VM_STRING("3:0    (0) - user")<<std::endl;
         stream<<VM_STRING("4:0    ")<<std::endl;

         CHECK_EQUAL(convert_w2s(stream.str().c_str()), convert_w2s(str_to_print.c_str()));
     }

     TEST(LeftAligment)
     {
         Items vParam ;
         vParam.push_back(10);

         MFXStructureRef<Items> structWrp(vParam);
         tstring str_to_print = structWrp.Serialize(Formater::SimpleString(10));
         tstringstream stream;

         stream<<VM_STRING("0         : 10")<<std::endl;

         CHECK_EQUAL(convert_w2s(stream.str().c_str()), convert_w2s(str_to_print.c_str()));
     }

     template <int val>
     struct InitmfxVideoParam
     {
         mfxVideoParam m_infoEmpty;
         mfxExtCodingOptionDDI m_ddi;
         mfxExtCodingOption m_extco; 
         mfxExtBuffer *m_bufs[2];

         InitmfxVideoParam()
         {
             mfxVideoParam infoEmpty = {val,val,val,val,val,val,val,val,val,val,val,val,val,val,val,val,val,val,val,val,val,val,val,val,val,val,val,val,val,val,val,val,val,val,val,val,val,val,val,val,val,val,val,val,val,val,val,val,val,val,val,val,val,val,val,val,val,val};
             mfxExtCodingOptionDDI ddi = {MFX_EXTBUFF_DDI, sizeof(mfxExtCodingOptionDDI),val,val,val,val,val,val,val,val,val,val,val,val,val,val,val,val,val,val,val,val,val,val,val,val,val,val,val,val,val,val,val,val,val,val};
             mfxExtCodingOption extco = {MFX_EXTBUFF_CODING_OPTION, sizeof(mfxExtCodingOption),val,val,val,val,val,val,val,val,val,val,val,val,val,val,val,val,val,val,val,val,val,val,val,val,val,val,val,val};
             m_bufs[0] = (mfxExtBuffer*)&m_ddi;
             m_bufs[1] = (mfxExtBuffer*)&m_extco;
             infoEmpty.NumExtParam = 2;
             infoEmpty.ExtParam = m_bufs;

             memcpy(&m_infoEmpty, &infoEmpty, sizeof(m_infoEmpty));
             memcpy(&m_ddi, &ddi, sizeof(ddi));
             memcpy(&m_extco, &extco, sizeof(extco));
         }
         tstring serialize()
         {
             tstringstream stream;

             stream<<VM_STRING("AsyncDepth:")<<val<<std::endl;
             stream<<VM_STRING("FrameInfo.FrameId.TemporalId:")<<val<<std::endl;
             stream<<VM_STRING("FrameInfo.FrameId.PriorityId:")<<val<<std::endl;
             stream<<VM_STRING("FrameInfo.FrameId.ViewId:")<<val<<std::endl;
             mfxU32 cid = val;
             vm_char sFourcc[5] = 
             {
                 ((char*)&cid)[0],
                 ((char*)&cid)[1],
                 ((char*)&cid)[2],
                 ((char*)&cid)[3],
                 0
             };
             stream<<VM_STRING("FrameInfo.FourCC:")<<sFourcc<<std::endl;
             stream<<VM_STRING("FrameInfo.Width:")<<val<<std::endl;
             stream<<VM_STRING("FrameInfo.Height:")<<val<<std::endl;
             stream<<VM_STRING("FrameInfo.CropX:")<<val<<std::endl;
             stream<<VM_STRING("FrameInfo.CropY:")<<val<<std::endl;
             stream<<VM_STRING("FrameInfo.CropW:")<<val<<std::endl;
             stream<<VM_STRING("FrameInfo.CropH:")<<val<<std::endl;
             stream<<VM_STRING("FrameInfo.FrameRateExtN:")<<val<<std::endl;
             stream<<VM_STRING("FrameInfo.FrameRateExtD:")<<val<<std::endl;
             stream<<VM_STRING("FrameInfo.AspectRatioW:")<<val<<std::endl;
             stream<<VM_STRING("FrameInfo.AspectRatioH:")<<val<<std::endl;
             stream<<VM_STRING("FrameInfo.PicStruct:")<<val<<std::endl;
             stream<<VM_STRING("FrameInfo.ChromaFormat:")<<GetMFXChromaString(val)<<std::endl;
             stream<<VM_STRING("CodecId:")<<sFourcc<<std::endl;
             stream<<VM_STRING("CodecProfile:")<<val<<std::endl;
             stream<<VM_STRING("CodecLevel:")<<val<<std::endl;
             stream<<VM_STRING("NumThread:")<<val<<std::endl;
             stream<<VM_STRING("TargetUsage:")<<val<<std::endl;
             stream<<VM_STRING("GopPicSize:")<<val<<std::endl;
             stream<<VM_STRING("GopRefDist:")<<val<<std::endl;
             stream<<VM_STRING("GopOptFlag:")<<val<<std::endl;
             stream<<VM_STRING("IdrInterval:")<<val<<std::endl;
             stream<<VM_STRING("RateControlMethod:")<<val<<std::endl;
             stream<<VM_STRING("InitialDelayInKB:")<<val<<std::endl;
             stream<<VM_STRING("BufferSizeInKB:")<<val<<std::endl;
             
             stream<<VM_STRING("TargetKbps:")<<val<<std::endl;
             //mfxU16  QPP;
             stream<<VM_STRING("MaxKbps:")<<val<<std::endl;
             //mfxU16  QPB;
             stream<<VM_STRING("NumSlice:")<<val<<std::endl;
             stream<<VM_STRING("NumRefFrame:")<<val<<std::endl;
             stream<<VM_STRING("EncodedOrder:")<<val<<std::endl;
             stream<<VM_STRING("Protected:")<<val<<std::endl;
             stream<<VM_STRING("IOPattern:")<<val<<std::endl;

             stream<<VM_STRING("DDI.IntraPredCostType:")<<val<<std::endl;
             stream<<VM_STRING("DDI.MEInterpolationMethod:")<<val<<std::endl;
             stream<<VM_STRING("DDI.MEFractionalSearchType:")<<val<<std::endl;
             stream<<VM_STRING("DDI.MaxMVs:")<<val<<std::endl;
             stream<<VM_STRING("DDI.SkipCheck:")<<val<<std::endl;
             stream<<VM_STRING("DDI.DirectCheck:")<<val<<std::endl;
             stream<<VM_STRING("DDI.BiDirSearch:")<<val<<std::endl;
             stream<<VM_STRING("DDI.MBAFF:")<<val<<std::endl;
             stream<<VM_STRING("DDI.FieldPrediction:")<<val<<std::endl;
             stream<<VM_STRING("DDI.RefOppositeField:")<<val<<std::endl;
             stream<<VM_STRING("DDI.ChromaInME:")<<val<<std::endl;
             stream<<VM_STRING("DDI.WeightedPrediction:")<<val<<std::endl;
             stream<<VM_STRING("DDI.MVPrediction:")<<val<<std::endl;
             stream<<VM_STRING("DDI.IntraPredBlockSize:")<<val<<std::endl;
             stream<<VM_STRING("DDI.InterPredBlockSize:")<<val<<std::endl;
             stream<<VM_STRING("DDI.SWPAK:")<<val<<std::endl;          
             stream<<VM_STRING("DDI.RefRaw:")<<val<<std::endl;         
             stream<<VM_STRING("DDI.AsyncMode:")<<val<<std::endl;      
             stream<<VM_STRING("DDI.ConstQP:")<<val<<std::endl;        
             stream<<VM_STRING("DDI.AddDelay:")<<val<<std::endl;       
             stream<<VM_STRING("DDI.QueryInterval:")<<val<<std::endl;  
             stream<<VM_STRING("DDI.MultipleEnc:")<<val<<std::endl;     
             stream<<VM_STRING("DDI.MultiplePak:")<<val<<std::endl;     
             stream<<VM_STRING("DDI.EncPakSeparately:")<<val<<std::endl;
             stream<<VM_STRING("DDI.MergePakPB:")<<val<<std::endl;      
             stream<<VM_STRING("DDI.MaxEncodeThreads:")<<val<<std::endl;
             stream<<VM_STRING("DDI.MergeLists:")<<val<<std::endl;      
             stream<<VM_STRING("DDI.SyncDecodeEncode:")<<val<<std::endl;

             stream<<VM_STRING("DDI.DisablePSubMBPartition:")<<val<<std::endl;
             stream<<VM_STRING("DDI.DisableBSubMBPartition:")<<val<<std::endl;
             stream<<VM_STRING("DDI.WeightedBiPredIdc:")<<val<<std::endl;
             stream<<VM_STRING("DDI.DirectSpatialMvPredFlag:")<<val<<std::endl;
             stream<<VM_STRING("DDI.SeqParameterSetId:")<<val<<std::endl;
             stream<<VM_STRING("DDI.PicParameterSetId:")<<val<<std::endl;

             stream<<VM_STRING("ExtCO.RateDistortionOpt:")<<val<<std::endl;
             stream<<VM_STRING("ExtCO.MECostType:")<<val<<std::endl;
             stream<<VM_STRING("ExtCO.MESearchType:")<<val<<std::endl;
             stream<<VM_STRING("ExtCO.MVSearchWindow.x:")<<val<<std::endl;
             stream<<VM_STRING("ExtCO.MVSearchWindow.y:")<<val<<std::endl;
             stream<<VM_STRING("ExtCO.EndOfSequence:")<<val<<std::endl;
             stream<<VM_STRING("ExtCO.FramePicture:")<<val<<std::endl;
             stream<<VM_STRING("ExtCO.CAVLC:")<<val<<std::endl;
             stream<<VM_STRING("ExtCO.NalHrdConformance:")<<val<<std::endl;
             stream<<VM_STRING("ExtCO.SingleSeiNalUnit:")<<val<<std::endl;
             stream<<VM_STRING("ExtCO.VuiVclHrdParameters:")<<val<<std::endl;
             
             stream<<VM_STRING("ExtCO.RefPicListReordering:")<<val<<std::endl;
             stream<<VM_STRING("ExtCO.ResetRefList:")<<val<<std::endl;
             stream<<VM_STRING("ExtCO.RefPicMarkRep:")<<val<<std::endl;
             stream<<VM_STRING("ExtCO.FieldOutput:")<<val<<std::endl;

             stream<<VM_STRING("ExtCO.IntraPredBlockSize:")<<val<<std::endl;
             stream<<VM_STRING("ExtCO.InterPredBlockSize:")<<val<<std::endl;
             stream<<VM_STRING("ExtCO.MVPrecision:")<<val<<std::endl;
             stream<<VM_STRING("ExtCO.MaxDecFrameBuffering:")<<val<<std::endl;
             stream<<VM_STRING("ExtCO.AUDelimiter:")<<val<<std::endl;
             stream<<VM_STRING("ExtCO.EndOfStream:")<<val<<std::endl;
             stream<<VM_STRING("ExtCO.PicTimingSEI:")<<val<<std::endl;
             stream<<VM_STRING("ExtCO.VuiNalHrdParameters:")<<val<<std::endl;

             return stream.str();
         }
         
     };

     typedef InitmfxVideoParam<0> InitmfxVideoParam0;

     TEST_FIXTURE(InitmfxVideoParam0, mfxVideoParam_serialization_NULL)
     { 
         MFXStructureRef<mfxVideoParam> infoMfx(m_infoEmpty);

//         CHECK_EQUAL(convert_w2s(serialize().c_str()), convert_w2s(infoMfx.Serialize(MockFormater()).c_str()));
     }

     typedef InitmfxVideoParam<1> InitmfxVideoParam1;

     TEST_FIXTURE(InitmfxVideoParam1, mfxVideoParam_serialization_ONE)
     {
         MFXStructureRef<mfxVideoParam> infoMfx(m_infoEmpty);

//         CHECK_EQUAL(convert_w2s(serialize().c_str()), convert_w2s(infoMfx.Serialize(MockFormater()).c_str()));
     }


     TEST(mfxInfoMFX_Serialization_NULL)
     {
         mfxInfoMFX infoEmpty = {0};
         MFXStructureRef<mfxInfoMFX> infoMfx(infoEmpty);
         tstring str_to_print = infoMfx.Serialize(MockFormater());

         tstringstream stream;

         stream<<VM_STRING("FrameInfo.FrameId.TemporalId:0")<<std::endl;
         stream<<VM_STRING("FrameInfo.FrameId.PriorityId:0")<<std::endl;
         stream<<VM_STRING("FrameInfo.FrameId.ViewId:0")<<std::endl;
         stream<<VM_STRING("FrameInfo.FourCC:")<<std::endl;
         stream<<VM_STRING("FrameInfo.Width:0")<<std::endl;
         stream<<VM_STRING("FrameInfo.Height:0")<<std::endl;
         stream<<VM_STRING("FrameInfo.CropX:0")<<std::endl;
         stream<<VM_STRING("FrameInfo.CropY:0")<<std::endl;
         stream<<VM_STRING("FrameInfo.CropW:0")<<std::endl;
         stream<<VM_STRING("FrameInfo.CropH:0")<<std::endl;
         stream<<VM_STRING("FrameInfo.FrameRateExtN:0")<<std::endl;
         stream<<VM_STRING("FrameInfo.FrameRateExtD:0")<<std::endl;
         stream<<VM_STRING("FrameInfo.AspectRatioW:0")<<std::endl;
         stream<<VM_STRING("FrameInfo.AspectRatioH:0")<<std::endl;
         stream<<VM_STRING("FrameInfo.PicStruct:0")<<std::endl;
         stream<<VM_STRING("FrameInfo.ChromaFormat:MFX_CHROMAFORMAT_MONOCHROME")<<std::endl;
         stream<<VM_STRING("CodecId:")<<std::endl;
         stream<<VM_STRING("CodecProfile:0")<<std::endl;
         stream<<VM_STRING("CodecLevel:0")<<std::endl;
         stream<<VM_STRING("NumThread:0")<<std::endl;
         stream<<VM_STRING("TargetUsage:0")<<std::endl;
         stream<<VM_STRING("GopPicSize:0")<<std::endl;
         stream<<VM_STRING("GopRefDist:0")<<std::endl;
         stream<<VM_STRING("GopOptFlag:0")<<std::endl;
         stream<<VM_STRING("IdrInterval:0")<<std::endl;
         stream<<VM_STRING("RateControlMethod:0")<<std::endl;
         stream<<VM_STRING("InitialDelayInKB:0")<<std::endl;
         //mfxU16  QPI;
         stream<<VM_STRING("BufferSizeInKB:0")<<std::endl;
         stream<<VM_STRING("TargetKbps:0")<<std::endl;
         //mfxU16  QPP;
         stream<<VM_STRING("MaxKbps:0")<<std::endl;
         //mfxU16  QPB;
         stream<<VM_STRING("NumSlice:0")<<std::endl;
         stream<<VM_STRING("NumRefFrame:0")<<std::endl;
         stream<<VM_STRING("EncodedOrder:0")<<std::endl;

//         CHECK_EQUAL(convert_w2s(stream.str().c_str()), convert_w2s(str_to_print.c_str()));
     }

     TEST(mfxInfoMFX_Serialization_CPQ_MODE)
     {
         mfxInfoMFX infoEmpty = {0};
         infoEmpty.RateControlMethod = 3;
         MFXStructureRef<mfxInfoMFX> infoMfx(infoEmpty);
         tstring str_to_print = infoMfx.Serialize(MockFormater());

         tstringstream stream;

         stream<<VM_STRING("FrameInfo.FrameId.TemporalId:0")<<std::endl;
         stream<<VM_STRING("FrameInfo.FrameId.PriorityId:0")<<std::endl;
         stream<<VM_STRING("FrameInfo.FrameId.ViewId:0")<<std::endl;
         stream<<VM_STRING("FrameInfo.FourCC:")<<std::endl;
         stream<<VM_STRING("FrameInfo.Width:0")<<std::endl;
         stream<<VM_STRING("FrameInfo.Height:0")<<std::endl;
         stream<<VM_STRING("FrameInfo.CropX:0")<<std::endl;
         stream<<VM_STRING("FrameInfo.CropY:0")<<std::endl;
         stream<<VM_STRING("FrameInfo.CropW:0")<<std::endl;
         stream<<VM_STRING("FrameInfo.CropH:0")<<std::endl;
         stream<<VM_STRING("FrameInfo.FrameRateExtN:0")<<std::endl;
         stream<<VM_STRING("FrameInfo.FrameRateExtD:0")<<std::endl;
         stream<<VM_STRING("FrameInfo.AspectRatioW:0")<<std::endl;
         stream<<VM_STRING("FrameInfo.AspectRatioH:0")<<std::endl;
         stream<<VM_STRING("FrameInfo.PicStruct:0")<<std::endl;
         stream<<VM_STRING("FrameInfo.ChromaFormat:MFX_CHROMAFORMAT_MONOCHROME")<<std::endl;
         stream<<VM_STRING("CodecId:")<<std::endl;
         stream<<VM_STRING("CodecProfile:0")<<std::endl;
         stream<<VM_STRING("CodecLevel:0")<<std::endl;
         stream<<VM_STRING("NumThread:0")<<std::endl;
         stream<<VM_STRING("TargetUsage:0")<<std::endl;
         stream<<VM_STRING("GopPicSize:0")<<std::endl;
         stream<<VM_STRING("GopRefDist:0")<<std::endl;
         stream<<VM_STRING("GopOptFlag:0")<<std::endl;
         stream<<VM_STRING("IdrInterval:0")<<std::endl;
         stream<<VM_STRING("RateControlMethod:3")<<std::endl;
         stream<<VM_STRING("QPI:0")<<std::endl;
         stream<<VM_STRING("BufferSizeInKB:0")<<std::endl;
         stream<<VM_STRING("QPP:0")<<std::endl;
         stream<<VM_STRING("QPB:0")<<std::endl;
         stream<<VM_STRING("NumSlice:0")<<std::endl;
         stream<<VM_STRING("NumRefFrame:0")<<std::endl;
         stream<<VM_STRING("EncodedOrder:0")<<std::endl;

//         CHECK_EQUAL(convert_w2s(stream.str().c_str()), convert_w2s(str_to_print.c_str()));
     }
     
     TEST(mfxInfoMFX_Serialization_AVBR_MODE)
     {
         mfxInfoMFX infoEmpty = {0};
         infoEmpty.RateControlMethod = 4;
         MFXStructureRef<mfxInfoMFX> infoMfx(infoEmpty);
         tstring str_to_print = infoMfx.Serialize(MockFormater());

         tstringstream stream;

         stream<<VM_STRING("FrameInfo.FrameId.TemporalId:0")<<std::endl;
         stream<<VM_STRING("FrameInfo.FrameId.PriorityId:0")<<std::endl;
         stream<<VM_STRING("FrameInfo.FrameId.ViewId:0")<<std::endl;
         stream<<VM_STRING("FrameInfo.FourCC:")<<std::endl;
         stream<<VM_STRING("FrameInfo.Width:0")<<std::endl;
         stream<<VM_STRING("FrameInfo.Height:0")<<std::endl;
         stream<<VM_STRING("FrameInfo.CropX:0")<<std::endl;
         stream<<VM_STRING("FrameInfo.CropY:0")<<std::endl;
         stream<<VM_STRING("FrameInfo.CropW:0")<<std::endl;
         stream<<VM_STRING("FrameInfo.CropH:0")<<std::endl;
         stream<<VM_STRING("FrameInfo.FrameRateExtN:0")<<std::endl;
         stream<<VM_STRING("FrameInfo.FrameRateExtD:0")<<std::endl;
         stream<<VM_STRING("FrameInfo.AspectRatioW:0")<<std::endl;
         stream<<VM_STRING("FrameInfo.AspectRatioH:0")<<std::endl;
         stream<<VM_STRING("FrameInfo.PicStruct:0")<<std::endl;
         stream<<VM_STRING("FrameInfo.ChromaFormat:MFX_CHROMAFORMAT_MONOCHROME")<<std::endl;
         stream<<VM_STRING("CodecId:")<<std::endl;
         stream<<VM_STRING("CodecProfile:0")<<std::endl;
         stream<<VM_STRING("CodecLevel:0")<<std::endl;
         stream<<VM_STRING("NumThread:0")<<std::endl;
         stream<<VM_STRING("TargetUsage:0")<<std::endl;
         stream<<VM_STRING("GopPicSize:0")<<std::endl;
         stream<<VM_STRING("GopRefDist:0")<<std::endl;
         stream<<VM_STRING("GopOptFlag:0")<<std::endl;
         stream<<VM_STRING("IdrInterval:0")<<std::endl;
         stream<<VM_STRING("RateControlMethod:4")<<std::endl;
         stream<<VM_STRING("Accuracy:0")<<std::endl;
         stream<<VM_STRING("BufferSizeInKB:0")<<std::endl;
         stream<<VM_STRING("TargetKbps:0")<<std::endl;
         stream<<VM_STRING("Convergence:0")<<std::endl;
         stream<<VM_STRING("NumSlice:0")<<std::endl;
         stream<<VM_STRING("NumRefFrame:0")<<std::endl;
         stream<<VM_STRING("EncodedOrder:0")<<std::endl;

//         CHECK_EQUAL(convert_w2s(stream.str().c_str()), convert_w2s(str_to_print.c_str()));
     }

    TEST(mfxFrameInfo_Serialization_NULL)
    {
         mfxFrameInfo infoEmpty = {0};
         MFXStructureRef<mfxFrameInfo> vParam(infoEmpty);
         tstring str_to_print = vParam.Serialize(MockFormater());
         
         tstringstream stream;

         stream<<VM_STRING("FrameId.TemporalId:0")<<std::endl;
         stream<<VM_STRING("FrameId.PriorityId:0")<<std::endl;
         stream<<VM_STRING("FrameId.ViewId:0")<<std::endl;
         stream<<VM_STRING("FourCC:")<<std::endl;
         stream<<VM_STRING("Width:0")<<std::endl;
         stream<<VM_STRING("Height:0")<<std::endl;
         stream<<VM_STRING("CropX:0")<<std::endl;
         stream<<VM_STRING("CropY:0")<<std::endl;
         stream<<VM_STRING("CropW:0")<<std::endl;
         stream<<VM_STRING("CropH:0")<<std::endl;
         stream<<VM_STRING("FrameRateExtN:0")<<std::endl;
         stream<<VM_STRING("FrameRateExtD:0")<<std::endl;
         stream<<VM_STRING("AspectRatioW:0")<<std::endl;
         stream<<VM_STRING("AspectRatioH:0")<<std::endl;
         stream<<VM_STRING("PicStruct:0")<<std::endl;
         stream<<VM_STRING("ChromaFormat:MFX_CHROMAFORMAT_MONOCHROME")<<std::endl;

         CHECK_EQUAL(convert_w2s(stream.str().c_str()), convert_w2s(str_to_print.c_str()));
     }

    
    TEST(mfxmfxMVCViewDependency_Deserialization_NULL_BUFFERS)
    {
        //valid cases
        tstring format = VM_STRING("1 0 0 0 0");
        mfxMVCViewDependency inDependency = {2,2,3,4,5};
        mfxMVCViewDependency refDependency = {1};
        mfxMVCViewDependency refDependency2 = {1,16,0,1};
        mfxMVCViewDependency refDependency3 = {1,2,2,
                                                {1,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
                                                {3,4,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
                                                2,2,
                                                {5,6,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
                                                {7,8,0,0,0,0,0,0,0,0,0,0,0,0,0,0}};


        MFXStructureRef<mfxMVCViewDependency> viewStruct(inDependency);
        bool deserial_sts = viewStruct.DeSerialize(format);

        CHECK(deserial_sts);
        
        CHECK(0 == memcmp(&inDependency, &refDependency, sizeof(mfxMVCViewDependency)));

        //valid cases
        format = VM_STRING("1 16 0 0 0 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0");
        CHECK(true == viewStruct.DeSerialize(format));

        CHECK(0 == memcmp(&inDependency, &refDependency2, sizeof(mfxMVCViewDependency)));

        format = VM_STRING("1 2 2 2 2 1 2 3 4 5 6 7 8");
        CHECK(true == viewStruct.DeSerialize(format));

        CHECK(0 == memcmp(&inDependency, &refDependency3, sizeof(mfxMVCViewDependency)));



        //////////////////////////////////////////////////////////////////////////
        //invalid format cases
        format = VM_STRING("1 2 0 0 0");
        CHECK(false == viewStruct.DeSerialize(format));

        //dependencyis arrays only of 16 lenght
        format = VM_STRING("1 17 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0");
        CHECK(false == viewStruct.DeSerialize(format));

        format = VM_STRING("0 1 0 0 0 a");
        CHECK(false == viewStruct.DeSerialize(format));

        format = VM_STRING("1 1 0 0 0 a");
        CHECK(false == viewStruct.DeSerialize(format));
        
        format = VM_STRING("c");
        CHECK(false == viewStruct.DeSerialize(format));
    }

    struct DeSerializeHelperTester : public DeSerializeHelper<int>
    {
        static tstring StringFromPPChar(vm_char ** start, vm_char **end)
        {
            return DeSerializeHelper<int>::StringFromPPChar(start, end);
        }
        static int PPCharOffsetFromStringOffset(vm_char ** start, int nOffset)
        {
            return DeSerializeHelper<int>::PPCharOffsetFromStringOffset(start, nOffset);
        }
    };

    TEST(SerializeHelper_StringFromPPChar)
    {
        vm_char astr[] = VM_STRING("a");
        vm_char bstr[] = VM_STRING("b");
        vm_char cstr[] = VM_STRING("c");
        vm_char dstr[] = VM_STRING("d");
        vm_char estr[] = VM_STRING("e");
        vm_char *a_test[] = {astr, bstr, cstr, dstr, estr};

        tstring str_expected = VM_STRING("a b c d e");
        tstring res_str = DeSerializeHelperTester::StringFromPPChar((vm_char**)a_test, (vm_char**)(a_test + sizeof(a_test) / sizeof(a_test[0])));

        CHECK_EQUAL(convert_w2s(str_expected.c_str()), convert_w2s(res_str.c_str()));
    }

    TEST(SerializerHelper_PPCharOffsetFromStringOffset)
    {
        vm_char astr[] = VM_STRING("aa");
        vm_char bstr[] = VM_STRING("bbb");
        vm_char cstr[] = VM_STRING("c");
        vm_char dstr[] = VM_STRING("d");
        vm_char estr[] = VM_STRING("e");
        vm_char *a_test[] = {astr, bstr, cstr, dstr, estr};
        //a bbb c d e
        
        CHECK_EQUAL( 0 , DeSerializeHelperTester::PPCharOffsetFromStringOffset(a_test, 0));
        CHECK_EQUAL( 0 , DeSerializeHelperTester::PPCharOffsetFromStringOffset(a_test, 1));
        CHECK_EQUAL( 1 , DeSerializeHelperTester::PPCharOffsetFromStringOffset(a_test, 2));
        CHECK_EQUAL( 1 , DeSerializeHelperTester::PPCharOffsetFromStringOffset(a_test, 3));
        CHECK_EQUAL( 1 , DeSerializeHelperTester::PPCharOffsetFromStringOffset(a_test, 4));
        CHECK_EQUAL( 1 , DeSerializeHelperTester::PPCharOffsetFromStringOffset(a_test, 5));
        CHECK_EQUAL( 2 , DeSerializeHelperTester::PPCharOffsetFromStringOffset(a_test, 6));
        CHECK_EQUAL( 2 , DeSerializeHelperTester::PPCharOffsetFromStringOffset(a_test, 7));
        CHECK_EQUAL( 4 , DeSerializeHelperTester::PPCharOffsetFromStringOffset(a_test, 11));
        CHECK_EQUAL( 5 , DeSerializeHelperTester::PPCharOffsetFromStringOffset(a_test, 12));
    }

    TEST(MFXVieDependency_serial_NULL)
    {
        mfxMVCViewDependency refDependency3 = {1,2,2,
                                                {1,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
                                                {3,4,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
                                                2,3,
                                                {5,6,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
                                                {7,8,9,0,0,0,0,0,0,0,0,0,0,0,0,0}};

        MFXStructureRef<mfxMVCViewDependency> refview(refDependency3);
        tstringstream expected;

        expected<<VM_STRING("ViewId:1")<<std::endl;
        expected<<VM_STRING("AnchorRefL0:1,2")<<std::endl;
        expected<<VM_STRING("AnchorRefL1:3,4")<<std::endl;
        
        expected<<VM_STRING("NonAnchorRefL0:5,6")<<std::endl;
        expected<<VM_STRING("NonAnchorRefL1:7,8,9")<<std::endl;

        CHECK_EQUAL(convert_w2s(expected.str()), convert_w2s(refview.Serialize(MockFormater())));
    }

    TEST(mfxOperationPoint)
    {
        mfxU16 target_views[] = {1,2,3,4};
        mfxMVCOperationPoint refDependency3 = {1,2,3,4, target_views};
        MFXStructureRef<mfxMVCOperationPoint> refview(refDependency3);
        tstringstream expected;

        expected<<VM_STRING("TemporalId:1")<<std::endl;
        expected<<VM_STRING("LevelIdc:2")<<std::endl;
        expected<<VM_STRING("NumViews:3")<<std::endl;
        expected<<VM_STRING("TargetViewId:1,2,3,4")<<std::endl;

        CHECK_EQUAL(convert_w2s(expected.str()), convert_w2s(refview.Serialize(MockFormater())));
    }
    
    TEST(mfxOperationPoint_NULL_targetView)
    {
        mfxU16 target_views[] = {0};
        mfxMVCOperationPoint refDependency3 = {1,2,3,0, target_views};
        MFXStructureRef<mfxMVCOperationPoint> refview(refDependency3);
        tstringstream expected;

        expected<<VM_STRING("TemporalId:1")<<std::endl;
        expected<<VM_STRING("LevelIdc:2")<<std::endl;
        expected<<VM_STRING("NumViews:3")<<std::endl;
        expected<<VM_STRING("TargetViewId:empty")<<std::endl;

        CHECK_EQUAL(convert_w2s(expected.str()), convert_w2s(refview.Serialize(MockFormater())));
    }

    TEST(mfxExtMVCSeqDesc)
    {
        mfxMVCViewDependency refDependency3 = {1,2,2,
        {1,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {3,4,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        2,3,
        {5,6,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {7,8,9,0,0,0,0,0,0,0,0,0,0,0,0,0}};

        MFXStructureRef<mfxMVCViewDependency> refview(refDependency3);

        mfxU16 target_views[] = {1,2,3,4};
        mfxMVCOperationPoint refOperationPoint = {1,2,3,4, target_views};
        MFXStructureRef<mfxMVCOperationPoint> refOP(refOperationPoint);

        mfxU16 viewID[] = {10,11,12,13};

        mfxExtMVCSeqDesc refSequence = {0,1,1,1,&refDependency3,4,4,viewID, 1,1, &refOperationPoint, 49};
        MFXStructureRef<mfxExtMVCSeqDesc> refSQ(refSequence);

        tstringstream expected;

        expected<<VM_STRING("View[0].ViewId:1")<<std::endl;
        expected<<VM_STRING("View[0].AnchorRefL0:1,2")<<std::endl;
        expected<<VM_STRING("View[0].AnchorRefL1:3,4")<<std::endl;
        expected<<VM_STRING("View[0].NonAnchorRefL0:5,6")<<std::endl;
        expected<<VM_STRING("View[0].NonAnchorRefL1:7,8,9")<<std::endl;

        expected<<VM_STRING("ViewId:10,11,12,13")<<std::endl;

        expected<<VM_STRING("OP[0].TemporalId:1")<<std::endl;
        expected<<VM_STRING("OP[0].LevelIdc:2")<<std::endl;
        expected<<VM_STRING("OP[0].NumViews:3")<<std::endl;
        expected<<VM_STRING("OP[0].TargetViewId:1,2,3,4")<<std::endl;

        //expected<<VM_STRING("CompatibilityMode:49")<<std::endl;

        CHECK_EQUAL(convert_w2s(expected.str()), convert_w2s(refSQ.Serialize(MockFormater())));
    }

    TEST(mfxExtMVCSeqDesc_empty)
    {
        mfxMVCViewDependency refDependency3 = {0};
        MFXStructureRef<mfxMVCViewDependency> refview(refDependency3);

        mfxU16 target_views[] = {0};
        mfxMVCOperationPoint refOperationPoint = {1,2,3,0, target_views};
        MFXStructureRef<mfxMVCOperationPoint> refOP(refOperationPoint);

        mfxU16 viewID[] = {0};

        mfxExtMVCSeqDesc refSequence = {0,0,0,0,&refDependency3,0,0,viewID, 0,0, &refOperationPoint, 49};
        MFXStructureRef<mfxExtMVCSeqDesc> refSQ(refSequence);

        tstringstream expected;

        expected<<VM_STRING("View:empty")<<std::endl;
        expected<<VM_STRING("ViewId:empty")<<std::endl;
        expected<<VM_STRING("OP:empty")<<std::endl;
        //expected<<VM_STRING("CompatibilityMode:49")<<std::endl;

        CHECK_EQUAL(convert_w2s(expected.str()), convert_w2s(refSQ.Serialize(MockFormater())));
    }

    TEST(mfxVersion)
    {
        mfxVersion v;
        v.Major = 2;
        v.Minor = 7;
        MFXStructureRef<mfxVersion> refVersion (v);
        CHECK_EQUAL(convert_w2s(VM_STRING(":2.7\n")), convert_w2s(refVersion.Serialize(MockFormater())));

        MFXStructure<mfxVersion> refVersionIn;
        CHECK(refVersionIn.DeSerialize(VM_STRING("2.3"), NULL));
        CHECK(((mfxVersion)refVersionIn).Major == 2 && ((mfxVersion)refVersionIn).Minor==3);
        CHECK(refVersionIn.DeSerialize(VM_STRING("2.3 "), NULL));
        CHECK(((mfxVersion)refVersionIn).Major == 2 && ((mfxVersion)refVersionIn).Minor==3);
        CHECK(refVersionIn.DeSerialize(VM_STRING("2.3 -"), NULL));
        CHECK(((mfxVersion)refVersionIn).Major == 2 && ((mfxVersion)refVersionIn).Minor==3);

        CHECK(0 == refVersionIn.DeSerialize(VM_STRING("2"), NULL));
        CHECK(0 == refVersionIn.DeSerialize(VM_STRING("2 3"), NULL));
        CHECK(0 == refVersionIn.DeSerialize(VM_STRING("2s3"), NULL));
        CHECK(0 == refVersionIn.DeSerialize(VM_STRING("2."), NULL));
        CHECK(0 == refVersionIn.DeSerialize(VM_STRING(".2"), NULL));
        CHECK(0 == refVersionIn.DeSerialize(VM_STRING(".2.3"), NULL));
        CHECK(0 == refVersionIn.DeSerialize(VM_STRING("2.3."), NULL));
        CHECK(0 == refVersionIn.DeSerialize(VM_STRING("2. "), NULL));
    }

    TEST(mfxExtAVCRefListCtrl_serial)
    {
        mfxU32 m1=(mfxU32)-1;
        mfxExtAVCRefListCtrl reflist = 
        {
            0,0,0,0,
            {
                {m1,1,0,0},{m1,1,0,0},{m1,1,0,0},{m1,1,0,0},
                {m1,1,0,0},{m1,1,0,0},{m1,1,0,0},{m1,1,0,0},
                {m1,1,0,0},{m1,1,0,0},{m1,1,0,0},{m1,1,0,0},
                {m1,1,0,0},{m1,1,0,0},{m1,1,0,0},{m1,1,0,0},
                {m1,1,0,0},{m1,1,0,0},{m1,1,0,0},{m1,1,0,0},
                {m1,1,0,0},{m1,1,0,0},{m1,1,0,0},{m1,1,0,0},
                {m1,1,0,0},{m1,1,0,0},{m1,1,0,0},{m1,1,0,0},
                {m1,1,0,0},{m1,1,0,0},{m1,1,0,0},{m1,1,0,0},
            }
        };
        MFXStructureRef<mfxExtAVCRefListCtrl> reflistStructure(reflist);

        reflist.NumRefIdxL0Active = 9;
        reflist.NumRefIdxL1Active = 10;

        reflist.PreferredRefList[0].FrameOrder = 5;
        reflist.PreferredRefList[1].FrameOrder = 6;
        reflist.PreferredRefList[2].FrameOrder = 7;

        reflist.RejectedRefList[0].FrameOrder = 15;
        reflist.RejectedRefList[1].FrameOrder = 16;

        reflist.LongTermRefList[0].FrameOrder = 21;
        reflist.LongTermRefList[1].FrameOrder = 23;
        reflist.LongTermRefList[2].FrameOrder = 43;

        tstringstream expected;

        expected<<VM_STRING("NumRefIdxL0Active:9")<<std::endl;
        expected<<VM_STRING("NumRefIdxL1Active:10")<<std::endl;
        expected<<VM_STRING("PreferredRefList:5,6,7,UNK,UNK,UNK,UNK,UNK,UNK,UNK,UNK,UNK,UNK,UNK,UNK,UNK,UNK,UNK,UNK,UNK,UNK,UNK,UNK,UNK,UNK,UNK,UNK,UNK,UNK,UNK,UNK,UNK")<<std::endl;
        expected<<VM_STRING("RejectedRefList:15,16,0,0,0,0,0,0,0,0,0,0,0,0,0,0")<<std::endl;
        expected<<VM_STRING("LongTermRefList:{FrameOrder,LongTermIdx}:{21,0},{23,0},{43,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0}")<<std::endl;

        CHECK_EQUAL(convert_w2s(expected.str()), convert_w2s(reflistStructure.Serialize(MockFormater())));
    } 

    TEST(mfxExtAVCRefListCtrl_deserial_valid)
    {
        MFXStructure<mfxExtAVCRefListCtrl> reflistStructure;
        int nPos = 0;

        tstring input_str = VM_STRING("8 9 1 1 3 2 23 11 12 78 91 1 88 3");
        CHECK(reflistStructure.DeSerialize(input_str, &nPos));
        CHECK_EQUAL(input_str.length(), (size_t)nPos);
        CHECK_EQUAL((mfxU32)MFX_EXTBUFF_AVC_REFLIST_CTRL, reflistStructure.operator mfxExtAVCRefListCtrl &().Header.BufferId);
        CHECK_EQUAL(sizeof(mfxExtAVCRefListCtrl), reflistStructure.operator mfxExtAVCRefListCtrl &().Header.BufferSz);
        CHECK_EQUAL(8, reflistStructure.operator mfxExtAVCRefListCtrl &().NumRefIdxL0Active);
        CHECK_EQUAL(9, reflistStructure.operator mfxExtAVCRefListCtrl &().NumRefIdxL1Active); 

        CHECK_EQUAL(23u, reflistStructure.operator mfxExtAVCRefListCtrl &().PreferredRefList[0].FrameOrder);
        CHECK_EQUAL(11u, reflistStructure.operator mfxExtAVCRefListCtrl &().RejectedRefList[0].FrameOrder);
        CHECK_EQUAL(12u, reflistStructure.operator mfxExtAVCRefListCtrl &().RejectedRefList[1].FrameOrder);
        CHECK_EQUAL(78u, reflistStructure.operator mfxExtAVCRefListCtrl &().RejectedRefList[2].FrameOrder);

        CHECK_EQUAL(91u, reflistStructure.operator mfxExtAVCRefListCtrl &().LongTermRefList[0].FrameOrder);
        CHECK_EQUAL(1u, reflistStructure.operator mfxExtAVCRefListCtrl &().LongTermRefList[0].LongTermIdx);
        CHECK_EQUAL(88u, reflistStructure.operator mfxExtAVCRefListCtrl &().LongTermRefList[1].FrameOrder);
        CHECK_EQUAL(3u, reflistStructure.operator mfxExtAVCRefListCtrl &().LongTermRefList[1].LongTermIdx);


        int i;
        for (i = 0; i < MFX_ARRAY_SIZE(reflistStructure.operator mfxExtAVCRefListCtrl &().PreferredRefList); i++)
        {
            CHECK_EQUAL((mfxU32)MFX_PICSTRUCT_PROGRESSIVE, reflistStructure.operator mfxExtAVCRefListCtrl &().PreferredRefList[i].PicStruct);
            if(i >= 1)
            {
                CHECK_EQUAL((mfxU32)MFX_FRAMEORDER_UNKNOWN, reflistStructure.operator mfxExtAVCRefListCtrl &().PreferredRefList[i].FrameOrder);
            }
        }

        for (i = 0; i < MFX_ARRAY_SIZE(reflistStructure.operator mfxExtAVCRefListCtrl &().RejectedRefList); i++)
        {
            CHECK_EQUAL((mfxU32)MFX_PICSTRUCT_PROGRESSIVE, reflistStructure.operator mfxExtAVCRefListCtrl &().RejectedRefList[i].PicStruct);
            if(i >= 3)
            {
                CHECK_EQUAL((mfxU32)MFX_FRAMEORDER_UNKNOWN, reflistStructure.operator mfxExtAVCRefListCtrl &().RejectedRefList[i].FrameOrder);
            }
        }

        for (i = 0; i < MFX_ARRAY_SIZE(reflistStructure.operator mfxExtAVCRefListCtrl &().LongTermRefList); i++)
        {
            CHECK_EQUAL((mfxU32)MFX_PICSTRUCT_PROGRESSIVE, reflistStructure.operator mfxExtAVCRefListCtrl &().LongTermRefList[i].PicStruct);
            if(i >= 2)
            {
                CHECK_EQUAL((mfxU32)MFX_FRAMEORDER_UNKNOWN, reflistStructure.operator mfxExtAVCRefListCtrl &().LongTermRefList[i].FrameOrder);
            }
        }
    }

    TEST(mfxExtAVCRefListCtrl_deserial_invalid)
    {
        MFXStructure<mfxExtAVCRefListCtrl> reflistStructure;
        int nPos = 0;

        //1st array boundary overflow
        CHECK_EQUAL(true, reflistStructure.DeSerialize(VM_STRING("1 1 0 32 0 0 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1"), &nPos));
        CHECK_EQUAL(false, reflistStructure.DeSerialize(VM_STRING("1 1 0 33 0 0 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1"), &nPos));

        //2nd array boundary overflow
        CHECK_EQUAL(true, reflistStructure.DeSerialize(VM_STRING("1 1 0 0 16 0 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1"), &nPos));
        CHECK_EQUAL(false, reflistStructure.DeSerialize(VM_STRING("1 1 0 0 17 0 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1"), &nPos));

        //3rd array boundary overflow
         CHECK_EQUAL(true, reflistStructure.DeSerialize(VM_STRING("1 1 0 0 0 16 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1 0"), &nPos));
        CHECK_EQUAL(false, reflistStructure.DeSerialize(VM_STRING("1 1 0 0 0 17 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1 0"), &nPos));

        //1rst array not enough data
        CHECK_EQUAL(true, reflistStructure.DeSerialize(VM_STRING("1 1 0 3 0 0 1 1 1"), &nPos));
        CHECK_EQUAL(false, reflistStructure.DeSerialize(VM_STRING("1 1 0 3 0 0 1 1"), &nPos));

        //2nd array not enough data
        CHECK_EQUAL(true, reflistStructure.DeSerialize(VM_STRING("1 1 0 0 3 0 1 1 1"), &nPos));
        CHECK_EQUAL(false, reflistStructure.DeSerialize(VM_STRING("1 1 0 0 3 0 1 1"), &nPos));

        //3rd array not enough data
        CHECK_EQUAL(true, reflistStructure.DeSerialize(VM_STRING("1 1 0 0 3 0 1 1 1"), &nPos));
        CHECK_EQUAL(false, reflistStructure.DeSerialize(VM_STRING("1 1 0 0 3 0 1 1"), &nPos));

        //1rst +2nd array not enough data
        CHECK_EQUAL(true, reflistStructure.DeSerialize(VM_STRING("1 1 0 3 3 0 1 1 1 1 1 1"), &nPos));
        CHECK_EQUAL(false, reflistStructure.DeSerialize(VM_STRING("1 1 0 3 3 0 1 1 1 1 1"), &nPos));

        //2nd +3rd array not enough data
        CHECK_EQUAL(true, reflistStructure.DeSerialize(VM_STRING("1 1 0 0 3 3 1 1 1 1 0 1 0 1 0"), &nPos));
        CHECK_EQUAL(false, reflistStructure.DeSerialize(VM_STRING("1 1 0 0 3 3 1 1 1 1 1"), &nPos));

        //1st+3rd array not enough data
        CHECK_EQUAL(true, reflistStructure.DeSerialize(VM_STRING("1 1 0 3 0 3 1 1 1 1 0 1 0 1 0"), &nPos));
        CHECK_EQUAL(false, reflistStructure.DeSerialize(VM_STRING("1 1 0 3 0 3 1 1 1 1 1"), &nPos));

        //1st+2nd+3rd array not enough data
        CHECK_EQUAL(true, reflistStructure.DeSerialize(VM_STRING("1 1 0 3 3 3 1 1 1 1 1 1 1 1 1"), &nPos));
        CHECK_EQUAL(false, reflistStructure.DeSerialize(VM_STRING("1 1 0 3 3 3 1 1 1 1 1 1 1 1"), &nPos));
    }

    TEST(mfxExtAVCRefListCtrl_deserial_apply_ltr_index)
    {
        MFXStructure<mfxExtAVCRefListCtrl> reflistStructure;
        int nPos = 0;

        //apply ltr index
        CHECK_EQUAL(true, reflistStructure.DeSerialize(VM_STRING("1 1 1 0 0 3 1 11 1 13 1 12"), &nPos));
        CHECK_EQUAL(1,  reflistStructure.operator mfxExtAVCRefListCtrl &().ApplyLongTermIdx);
        CHECK_EQUAL(11, reflistStructure.operator mfxExtAVCRefListCtrl &().LongTermRefList[0].LongTermIdx);
        CHECK_EQUAL(13, reflistStructure.operator mfxExtAVCRefListCtrl &().LongTermRefList[1].LongTermIdx);
        CHECK_EQUAL(12, reflistStructure.operator mfxExtAVCRefListCtrl &().LongTermRefList[2].LongTermIdx);

        for (size_t i = 0; i < MFX_ARRAY_SIZE(reflistStructure.operator mfxExtAVCRefListCtrl &().LongTermRefList); i++) {
            if(i > 2)
            {
                CHECK_EQUAL((mfxU32)MFX_FRAMEORDER_UNKNOWN, reflistStructure.operator mfxExtAVCRefListCtrl &().LongTermRefList[i].FrameOrder);
            } 
        }
    }

    TEST(mfxExtAvcTemporalLayers_deserial)
    {
        MFXStructure<mfxExtAvcTemporalLayers> temporalLayer;
        int nPos = 0;

        //apply ltr index
        CHECK_EQUAL(true, temporalLayer.DeSerialize(VM_STRING("13 1 2 3 4 5 6 7 8"), &nPos));
        mfxExtAvcTemporalLayers & layer = temporalLayer.operator mfxExtAvcTemporalLayers&() ;
        CHECK_EQUAL(13u, layer.BaseLayerPID);
        CHECK_EQUAL(1u, layer.Layer[0].Scale);
        CHECK_EQUAL(2u, layer.Layer[1].Scale);
        CHECK_EQUAL(3u, layer.Layer[2].Scale);
        CHECK_EQUAL(4u, layer.Layer[3].Scale);
        CHECK_EQUAL(5u, layer.Layer[4].Scale);
        CHECK_EQUAL(6u, layer.Layer[5].Scale);
        CHECK_EQUAL(7u, layer.Layer[6].Scale);
        CHECK_EQUAL(8u, layer.Layer[7].Scale);
        CHECK_EQUAL(18, nPos);
    }



}
