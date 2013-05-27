/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#include "utest_common.h"
#include "mfx_serial_core.h"
#include "mfx_svc_serial.h"
#include "mfx_serializer_test.h"

SUITE(structure_builder)
{
    struct  A
    {
    public:
        mfxU8  u8;
        mfxU16 u16;
        mfxU32 u32;
    };

    TEST(POD)
    {
        A obj = {1,2,3};
        PODNode<mfxU8>  node1 (_T("u8"),  obj.u8);
        PODNode<mfxU16> node2 (_T("u16"), obj.u16);
        PODNode<mfxU32> node3 (_T("u32"), obj.u32);

        std::stringstream stream;
        PairSerializer stream2;
        stream<<(int)obj.u8;
        node1.Serialize(stream2);
        CHECK_EQUAL("u8", convert_w2s(stream2.type()));
        CHECK_EQUAL(stream.str().c_str(), convert_w2s(stream2.value()));
     
        stream.str("");
        stream<<(int)obj.u16;
        node2.Serialize(stream2);
        CHECK_EQUAL("u16", convert_w2s(stream2.type()));
        CHECK_EQUAL(stream.str().c_str(), convert_w2s(stream2.value()));
     
        stream.str("");
        stream<<(int)obj.u32;
        node3.Serialize(stream2);
        CHECK_EQUAL("u32", convert_w2s(stream2.type()));
        CHECK_EQUAL(stream.str().c_str(), convert_w2s(stream2.value()));
    }

    TEST(NO_HIERARCHY_NO_ARRAYS_CT_LIST)
    {
        A obj = {1,2,3};

        PODNode<mfxU8>  node1 (_T("u8"),  obj.u8);
        PODNode<mfxU16> node2 (_T("u16"), obj.u16);
        PODNode<mfxU32> node3 (_T("u32"), obj.u32);
        
        ListNode<PODNode<mfxU8>
            , ListNode<PODNode<mfxU16>
            , ListNode<PODNode<mfxU32> > > >lst (_T("obj."), 
                node1, 
                make_list_node(node2, 
                make_list_node(node3)));

        std::stringstream stream;
        tstringstream stream2;
        
        lst.Serialize(Formaters2::SerializedStream(stream2, 8));

        stream<<"obj.u8  : "<<(int)obj.u8<<std::endl;
        stream<<"obj.u16 : "<<obj.u16<<std::endl;
        stream<<"obj.u32 : "<<obj.u32<<std::endl;

        CHECK_EQUAL(stream.str().c_str(), convert_w2s(stream2.str().c_str()));

        obj.u8 = 44;
        obj.u16 = 444;
        obj.u32 = 4444;
        

        stream.str("");
        stream2.str(_T(""));
        lst.Serialize(Formaters2::SerializedStream(stream2, 8));

        stream<<"obj.u8  : "<<(int)obj.u8<<std::endl;
        stream<<"obj.u16 : "<<obj.u16<<std::endl;
        stream<<"obj.u32 : "<<obj.u32<<std::endl;

        CHECK_EQUAL(stream.str().c_str(), convert_w2s(stream2.str().c_str()));
    }

    //TEST(NO_HIERARCHY_NO_ARRAYS_RUNTIME_LIST)
    //{
    //    A obj = {1,2,3};
    //    PODNode<mfxU8>  node1 (_T("u8"),  obj.u8);
    //    PODNode<mfxU16> node2 (_T("u16"), obj.u16);
    //    PODNode<mfxU32> node3 (_T("u32"), obj.u32);

    //    TreeListNode lst (_T("obj."));
    //    lst.add_node(&node1);
    //    lst.add_node(&node2);
    //    lst.add_node(&node3);

    //    std::stringstream stream;
    //    stream<<"obj.u8 : "<<(int)obj.u8<<std::endl;
    //    stream<<"obj.u16 : "<<obj.u16<<std::endl;
    //    stream<<"obj.u32 : "<<obj.u32;

    //    CHECK_EQUAL(stream.str().c_str(), convert_w2s(lst.ToString()));
    //}


    //struct B 
    //{
    //    A obj1;
    //    A obj2;
    //    mfxU32 _value;
    //};

    //TEST(HIERARCHY_NO_ARRAYS)
    //{
    //    B obj = {1,2,3,4,5,6,7};
    //    PODNode<mfxU8>  node1 (_T("u8"),  obj.obj1.u8);
    //    PODNode<mfxU16> node2 (_T("u16"), obj.obj1.u16);
    //    PODNode<mfxU32> node3 (_T("u32"), obj.obj1.u32);

    //    PODNode<mfxU8>  node4 (_T("u8"),  obj.obj2.u8);
    //    PODNode<mfxU16> node5 (_T("u16"), obj.obj2.u16);
    //    PODNode<mfxU32> node6 (_T("u32"), obj.obj2.u32);
    //    
    //    PODNode<mfxU32> node7 (_T("_value"), obj._value);


    //    TreeListNode _lst(_T("obj1.")) ;
    //    _lst.add_node(&node1);
    //    _lst.add_node(&node2);
    //    _lst.add_node(&node3);

    //    TreeListNode _lst2(_T("obj2.")) ;
    //    _lst2.add_node(&node4);
    //    _lst2.add_node(&node5);
    //    _lst2.add_node(&node6);

    //    TreeListNode _lst3(_T("obj.")) ;
    //    _lst3.add_node(&_lst);
    //    _lst3.add_node(&_lst2);
    //    _lst3.add_node(&node7);


    //    std::stringstream stream;
    //    stream<<"obj.obj1.u8 : "<<(int)obj.obj1.u8<<std::endl;
    //    stream<<"obj.obj1.u16 : "<<obj.obj1.u16<<std::endl;
    //    stream<<"obj.obj1.u32 : "<<obj.obj1.u32<<std::endl;
    //    stream<<"obj.obj2.u8 : "<<(int)obj.obj2.u8<<std::endl;
    //    stream<<"obj.obj2.u16 : "<<obj.obj2.u16<<std::endl;
    //    stream<<"obj.obj2.u32 : "<<obj.obj2.u32<<std::endl;
    //    stream<<"obj._value : "<<obj._value;

    //    CHECK_EQUAL(stream.str().c_str(), convert_w2s(_lst3.ToString()));
    //}

    struct C 
    {
        int r[5];
    };

    TEST(NO_HIERARCHY_ARRAYS_OF_PODS)
    {
        C obj = {1,2,3,4,5};
        ArrayNode <PODNode<int>, 5 > ar(_T("r"), obj.r);

        std::stringstream stream;
        stream<<"r[0] : "<<obj.r[0]<<std::endl;
        stream<<"r[1] : "<<obj.r[1]<<std::endl;
        stream<<"r[2] : "<<obj.r[2]<<std::endl;
        stream<<"r[3] : "<<obj.r[3]<<std::endl;
        stream<<"r[4] : "<<obj.r[4]<<std::endl;;
        
        tstringstream stream2;
        ar.Serialize(Formaters2::SerializedStream(stream2, 5));

        CHECK_EQUAL(stream.str().c_str(), convert_w2s(stream2.str().c_str()));
    }

    struct C1
    {
        int r[5];
        int numbr;
    };

    TEST(NO_HIERARCHY_ARRAYS_OF_PODS_limited_len)
    {
        C1 obj = {1,2,3,4,5,3};
        ArrayNode <PODNode<int>,  5, int > ar(_T("r"), obj.r, obj.numbr);

        std::stringstream stream;
        stream<<"r[0] : "<<obj.r[0]<<std::endl;
        stream<<"r[1] : "<<obj.r[1]<<std::endl;
        stream<<"r[2] : "<<obj.r[2]<<std::endl;;

        tstringstream stream2;
        ar.Serialize(Formaters2::SerializedStream(stream2, 5));

        CHECK_EQUAL(stream.str().c_str(), convert_w2s(stream2.str().c_str()));
    }

    struct D
    {
        struct H
        {
            mfxU32 a;
            mfxU32 b;
        }m_hhh[3];
    };

    class Builder 
        : public ListNode<PODNode<mfxU32>, ListNode<PODNode<mfxU32> > >  
    {
    public:
        typedef D::H element_type;
        typedef SerialSingleElement<NullData> init_type;

        Builder (tstring name, D::H & _value, const SerialSingleElement<NullData> &data = SerialSingleElement<NullData>())
            : ListNode(name, make_pod_node(_T(".a"), _value.a), make_list_node(make_pod_node(_T(".b"), _value.b)))
        {
            data;
        }
    };

    TEST(NO_HIERARCHY_ARRAYS_OF_NON_PODS)
    {
        D obj = {1,2,3,4,5};
        ArrayNode <Builder, 3> ar(_T("m_hhh"), obj.m_hhh);

        std::stringstream stream;
        stream<<"m_hhh[0].a : "<<obj.m_hhh[0].a<<std::endl;
        stream<<"m_hhh[0].b : "<<obj.m_hhh[0].b<<std::endl;
        stream<<"m_hhh[1].a : "<<obj.m_hhh[1].a<<std::endl;
        stream<<"m_hhh[1].b : "<<obj.m_hhh[1].b<<std::endl;
        stream<<"m_hhh[2].a : "<<obj.m_hhh[2].a<<std::endl;
        stream<<"m_hhh[2].b : "<<obj.m_hhh[2].b<<std::endl;;


        tstringstream stream2;
        ar.Serialize(Formaters2::SerializedStream(stream2, 11));

        CHECK_EQUAL(stream.str().c_str(), convert_w2s(stream2.str().c_str()));

        //CHECK_EQUAL(stream.str().c_str(), convert_w2s(ar.ToString()));
    }
    
    TEST(StructureBuilder_mfxQualityLayer)
    {
        mfxExtSVCSeqDesc::mfxDependencyLayer::mfxQualityLayer qlt;
        qlt.ScanIdxStart = 12;
        qlt.ScanIdxEnd = 23;
        qlt.TcoeffPredictionFlag = 7;

        StructureBuilder <mfxExtSVCSeqDesc::mfxDependencyLayer::mfxQualityLayer> bld(_T("qlt"), qlt, NullData());

        std::stringstream stream;
        stream<<"qlt.ScanIdxStart: " << qlt.ScanIdxStart << std::endl;
        stream<<"qlt.ScanIdxEnd: "<< qlt.ScanIdxEnd << std::endl;
        stream<<"qlt.TcoeffPredictionFlag: " << qlt.TcoeffPredictionFlag << std::endl;

        tstringstream stream2;
        bld.Serialize(Formaters2::SerializedStream(stream2, 1));

        CHECK_EQUAL(stream.str().c_str(), convert_w2s(stream2.str().c_str()));

        //CHECK_EQUAL(stream.str().c_str(), convert_w2s(bld.ToString()));
    }

    TEST(StructureBuilder_mfxDependencyLayer)
    {
        mfxExtSVCSeqDesc::mfxDependencyLayer dep_layer = 
        { 1,2,3,4,5,6,7,8,9,11,12,13,14,2};
        
        dep_layer.QualityNum=2;
        dep_layer.QualityLayer[0].ScanIdxStart = 15;
        dep_layer.QualityLayer[1].ScanIdxEnd = 17;
        dep_layer.QualityLayer[2].ScanIdxStart = 191;
        dep_layer.TemporalNum=4;
        dep_layer.TemporalId[0]=1;
        dep_layer.TemporalId[1]=2;
        dep_layer.TemporalId[2]=3;
        dep_layer.TemporalId[3]=4;

        tstring fileName = _T("file1.yuv");
        StructureBuilder <mfxExtSVCSeqDesc::mfxDependencyLayer> sdep_layer(_T("dep"), dep_layer, fileName);
        //SerialCollection <tstring> stringCollection;
        //stringCollection.resize(8);
        
        std::stringstream stream;

        stream<<"dep.InputFile: " << convert_w2s(fileName) << std::endl;
        stream<<"dep.Active: " << dep_layer.Active << std::endl;
        stream<<"dep.Width: "<< dep_layer.Width << std::endl;
        stream<<"dep.Height: " << dep_layer.Height << std::endl;
        stream<<"dep.CropX: "<< dep_layer.CropX << std::endl;
        stream<<"dep.CropY: " << dep_layer.CropY << std::endl;
        stream<<"dep.CropW: "<< dep_layer.CropW << std::endl;
        stream<<"dep.CropH: " << dep_layer.CropH << std::endl;
        stream<<"dep.RefLayerDid: " << dep_layer.RefLayerDid << std::endl;
        stream<<"dep.RefLayerQid: " << dep_layer.RefLayerQid << std::endl;
        stream<<"dep.GopPicSize: " << dep_layer.GopPicSize << std::endl;
        stream<<"dep.GopRefDist: " << dep_layer.GopRefDist << std::endl;
        stream<<"dep.GopOptFlag: " << dep_layer.GopOptFlag << std::endl;
        stream<<"dep.IdrInterval: " << dep_layer.IdrInterval << std::endl;
        stream<<"dep.BasemodePred: " << dep_layer.BasemodePred << std::endl;
        stream<<"dep.MotionPred: " << dep_layer.MotionPred << std::endl;
        stream<<"dep.ResidualPred: " << dep_layer.ResidualPred << std::endl;

        stream<<"dep.DisableDeblockingFilter: " << dep_layer.DisableDeblockingFilter << std::endl;
        stream<<"dep.ScaledRefLayerOffsets[0]: " << dep_layer.ScaledRefLayerOffsets[0] << std::endl;
        stream<<"dep.ScaledRefLayerOffsets[1]: " << dep_layer.ScaledRefLayerOffsets[1] << std::endl;
        stream<<"dep.ScaledRefLayerOffsets[2]: " << dep_layer.ScaledRefLayerOffsets[2] << std::endl;
        stream<<"dep.ScaledRefLayerOffsets[3]: " << dep_layer.ScaledRefLayerOffsets[3] << std::endl;

        stream<<"dep.ScanIdxPresent: " << dep_layer.ScanIdxPresent<< std::endl;

        stream<<"dep.TemporalNum: " << dep_layer.TemporalNum << std::endl;
        stream<<"dep.TemporalId[0]: " << dep_layer.TemporalId[0] << std::endl;
        stream<<"dep.TemporalId[1]: " << dep_layer.TemporalId[1] << std::endl;
        stream<<"dep.TemporalId[2]: " << dep_layer.TemporalId[2] << std::endl;
        stream<<"dep.TemporalId[3]: " << dep_layer.TemporalId[3] << std::endl;
        stream<<"dep.QualityNum: " << dep_layer.QualityNum << std::endl;
        stream<<"dep.QualityLayer[0].ScanIdxStart: " << dep_layer.QualityLayer[0].ScanIdxStart << std::endl;
        stream<<"dep.QualityLayer[0].ScanIdxEnd: " << dep_layer.QualityLayer[0].ScanIdxEnd << std::endl;
        stream<<"dep.QualityLayer[0].TcoeffPredictionFlag: " << dep_layer.QualityLayer[0].TcoeffPredictionFlag << std::endl;
        stream<<"dep.QualityLayer[1].ScanIdxStart: " << dep_layer.QualityLayer[1].ScanIdxStart << std::endl;
        stream<<"dep.QualityLayer[1].ScanIdxEnd: " << dep_layer.QualityLayer[1].ScanIdxEnd << std::endl;
        stream<<"dep.QualityLayer[1].TcoeffPredictionFlag: " << dep_layer.QualityLayer[1].TcoeffPredictionFlag << std::endl;

        tstringstream stream2;
        sdep_layer.Serialize(Formaters2::SerializedStream(stream2, 1));

        CHECK_EQUAL(stream.str().c_str(), convert_w2s(stream2.str().c_str()));

        //CHECK_EQUAL(stream.str().c_str(), convert_w2s(sdep_layer.ToString()));
    }

    TEST(Deserialization_NON_POD_Arrays)
    {
        std::stringstream s("[44]");
        s.ignore(1,'[');
        int y;
        s>>y;

        D obj = {0};
        ArrayNode <Builder, 3> ar(_T("m_hhh"), obj.m_hhh);
        SerialNode * nodes[6];

        //stream<<"m_hhh[0].a : "<<obj.m_hhh[0].a<<std::endl;
        int n;
        CHECK(ar.IsDeserialPossible(_T("m_hhh[0].a"),  n, nodes[0]));
        CHECK_EQUAL(1, n);
        nodes[0]->Parse(_T("5"));
        CHECK(ar.IsDeserialPossible(_T("m_hhh[0].b"),  n, nodes[1]));
        CHECK_EQUAL(1, n);
        nodes[1]->Parse(_T("6"));
        CHECK(ar.IsDeserialPossible(_T("m_hhh[1].a"),  n, nodes[2]));
        CHECK_EQUAL(1, n);
        nodes[2]->Parse(_T("7"));
        
        //check delayed parsing
        CHECK(ar.IsDeserialPossible(_T("m_hhh[1].b"),  n, nodes[3]));
        CHECK_EQUAL(1, n);
        
        CHECK(ar.IsDeserialPossible(_T("m_hhh[2].a"),  n, nodes[4]));
        CHECK_EQUAL(1, n);
        CHECK(ar.IsDeserialPossible(_T("m_hhh[2].b"),  n, nodes[5]));
        CHECK_EQUAL(1, n);

        CHECK(!ar.IsDeserialPossible(_T("m_hhh(0].a"),  n, nodes[0]));
        CHECK_EQUAL(1, n);
        CHECK(!ar.IsDeserialPossible(_T("m_hhh[0].c"),  n, nodes[0]));
        CHECK_EQUAL(1, n);
        CHECK(!ar.IsDeserialPossible(_T("m_hhh[3].a"),  n, nodes[0]));
        CHECK_EQUAL(1, n);
        nodes[3]->Parse(_T("8"));
        nodes[4]->Parse(_T("9"));
        nodes[5]->Parse(_T("15"));


        CHECK_EQUAL(5u, obj.m_hhh[0].a);
        CHECK_EQUAL(6u, obj.m_hhh[0].b);
        CHECK_EQUAL(7u, obj.m_hhh[1].a);
        CHECK_EQUAL(8u, obj.m_hhh[1].b);
        CHECK_EQUAL(9u, obj.m_hhh[2].a);
        CHECK_EQUAL(15u, obj.m_hhh[2].b);
    }

    TEST(StructureBuilder_DESERIALIZE_mfxDependencyLayer)
    {
        mfxExtSVCSeqDesc svc_sd = {0};
        SerialCollection<tstring>files;
        StructureBuilder <mfxExtSVCSeqDesc> ssvc_sd(_T("-svc"), svc_sd, files);
        SerialNode *pNode = NULL;

        int n;
        //fileName
        CHECK(ssvc_sd.IsDeserialPossible(_T("-svc.DependencyLayer[7].inputFile"),  n, pNode));
        pNode->Parse(_T("c:\\fileName.avi"));
        CHECK_EQUAL("c:\\fileName.avi", convert_w2s(files[7]));

        CHECK(!ssvc_sd.IsDeserialPossible(_T("-svc.TemporalScale"),  n, pNode));
        CHECK(ssvc_sd.IsDeserialPossible(_T("-svc.TemporalScale[0]"),  n, pNode));
        pNode->Parse(_T("2"));
        CHECK_EQUAL(2u, svc_sd.TemporalScale[0]);
        
        CHECK(ssvc_sd.IsDeserialPossible(_T("-svc.TemporalScale[7]"),  n, pNode));
        pNode->Parse(_T("12"));
        CHECK_EQUAL(12u, svc_sd.TemporalScale[7]);
        
        CHECK(!ssvc_sd.IsDeserialPossible(_T("-svc.TemporalScale[8]"),  n, pNode));
        CHECK(!ssvc_sd.IsDeserialPossible(_T("-svc.DependencyLayer[0]"),  n, pNode));
        
        CHECK(ssvc_sd.IsDeserialPossible(_T("-svc.DependencyLayer[0].Active"),  n, pNode));
        pNode->Parse(_T("2"));
        CHECK_EQUAL(2u, svc_sd.DependencyLayer[0].Active);
        
        CHECK(ssvc_sd.IsDeserialPossible(_T("-svc.DependencyLayer[7].Active"),  n, pNode));
        pNode->Parse(_T("12"));
        CHECK_EQUAL(12u, svc_sd.DependencyLayer[7].Active);
        
        CHECK(!ssvc_sd.IsDeserialPossible(_T("-svc.DependencyLayer[8].Active"),  n, pNode));
        CHECK(ssvc_sd.IsDeserialPossible(_T("-svc.DependencyLayer[0].ScaledRefLayerOffsets[0]"),  n, pNode));
        pNode->Parse(_T("-12"));
        CHECK_EQUAL(-12, svc_sd.DependencyLayer[0].ScaledRefLayerOffsets[0]);

        CHECK(ssvc_sd.IsDeserialPossible(_T("-svc.DependencyLayer[0].ScaledRefLayerOffsets[3]"),  n, pNode));
        pNode->Parse(_T("2"));
        CHECK_EQUAL(2, svc_sd.DependencyLayer[0].ScaledRefLayerOffsets[3]);

        CHECK(!ssvc_sd.IsDeserialPossible(_T("-svc.DependencyLayer[0].ScaledRefLayerOffsets[4]"),  n, pNode));
        CHECK(!ssvc_sd.IsDeserialPossible(_T("-svc.DependencyLayer[0].ScaledRefLayerOffsets[0].l"),  n, pNode));
        CHECK(!ssvc_sd.IsDeserialPossible(_T("-svc.DependencyLayer[0].QualityLayer[0]"),  n, pNode));
        
        CHECK(ssvc_sd.IsDeserialPossible(_T("-svc.DependencyLayer[0].QualityLayer[0].ScanIdxEnd"),  n, pNode));
        pNode->Parse(_T("12"));
        CHECK_EQUAL(12u, svc_sd.DependencyLayer[0].QualityLayer[0].ScanIdxEnd);
        CHECK(ssvc_sd.IsDeserialPossible(_T("-svc.DependencyLayer[0].QualityLayer[15].ScanIdxEnd"),  n, pNode));
        pNode->Parse(_T("2"));
        CHECK_EQUAL(2u, svc_sd.DependencyLayer[0].QualityLayer[15].ScanIdxEnd);
        
        CHECK(!ssvc_sd.IsDeserialPossible(_T("-svc.DependencyLayer[0].QualityLayer[16].ScanIdxEnd"),  n, pNode));
    }

    TEST(StructureBuilder_DESERIALIZE_mfxDependencyLayer_NON_AUTO_ARRAY_LEN)
    {
        mfxExtSVCSeqDesc::mfxDependencyLayer dep_layer = {0};
        tstring fileName = _T("myfile");
        StructureBuilder <mfxExtSVCSeqDesc::mfxDependencyLayer> sdep_layer(_T(""), dep_layer, fileName);
        SerialNode *pNode = NULL;

        int n;
        CHECK(sdep_layer.IsDeserialPossible(_T(".QualityNum"),  n, pNode));
        pNode->Parse(_T("2"));
        CHECK_EQUAL(2u, dep_layer.QualityNum);

        CHECK(sdep_layer.IsDeserialPossible(_T(".TemporalNum"),  n, pNode));
        pNode->Parse(_T("3"));
        CHECK_EQUAL(3u, dep_layer.TemporalNum);

        //check that 2 elements will be serialized
        std::stringstream stream;
        stream<<".InputFile: " << convert_w2s(fileName) << std::endl;
        stream<<".Active: " << dep_layer.Active << std::endl;
        stream<<".Width: "<< dep_layer.Width << std::endl;
        stream<<".Height: " << dep_layer.Height << std::endl;
        stream<<".CropX: "<< dep_layer.CropX << std::endl;
        stream<<".CropY: " << dep_layer.CropY << std::endl;
        stream<<".CropW: "<< dep_layer.CropW << std::endl;
        stream<<".CropH: " << dep_layer.CropH << std::endl;
        stream<<".RefLayerDid: " << dep_layer.RefLayerDid << std::endl;
        stream<<".RefLayerQid: " << dep_layer.RefLayerQid << std::endl;
        stream<<".GopPicSize: " << dep_layer.GopPicSize << std::endl;
        stream<<".GopRefDist: " << dep_layer.GopRefDist << std::endl;
        stream<<".GopOptFlag: " << dep_layer.GopOptFlag << std::endl;
        stream<<".IdrInterval: " << dep_layer.IdrInterval << std::endl;
        stream<<".BasemodePred: " << dep_layer.BasemodePred << std::endl;
        stream<<".MotionPred: " << dep_layer.MotionPred << std::endl;
        stream<<".ResidualPred: " << dep_layer.ResidualPred << std::endl;
        
        stream<<".DisableDeblockingFilter: " << dep_layer.DisableDeblockingFilter << std::endl;
        stream<<".ScaledRefLayerOffsets[0]: " << dep_layer.ScaledRefLayerOffsets[0] << std::endl;
        stream<<".ScaledRefLayerOffsets[1]: " << dep_layer.ScaledRefLayerOffsets[1] << std::endl;
        stream<<".ScaledRefLayerOffsets[2]: " << dep_layer.ScaledRefLayerOffsets[2] << std::endl;
        stream<<".ScaledRefLayerOffsets[3]: " << dep_layer.ScaledRefLayerOffsets[3] << std::endl;
        
        stream<<".ScanIdxPresent: " << dep_layer.ScanIdxPresent<< std::endl;

        stream<<".TemporalNum: " << dep_layer.TemporalNum << std::endl;
        stream<<".TemporalId[0]: " << dep_layer.TemporalId[0] << std::endl;
        stream<<".TemporalId[1]: " << dep_layer.TemporalId[1] << std::endl;
        stream<<".TemporalId[2]: " << dep_layer.TemporalId[2] << std::endl;

        stream<<".QualityNum: " << dep_layer.QualityNum << std::endl;
        stream<<".QualityLayer[0].ScanIdxStart: " << dep_layer.QualityLayer[0].ScanIdxStart << std::endl;
        stream<<".QualityLayer[0].ScanIdxEnd: " << dep_layer.QualityLayer[0].ScanIdxEnd << std::endl;
        stream<<".QualityLayer[0].TcoeffPredictionFlag: " << dep_layer.QualityLayer[0].TcoeffPredictionFlag << std::endl;
        stream<<".QualityLayer[1].ScanIdxStart: " << dep_layer.QualityLayer[1].ScanIdxStart << std::endl;
        stream<<".QualityLayer[1].ScanIdxEnd: " << dep_layer.QualityLayer[1].ScanIdxEnd << std::endl;
        stream<<".QualityLayer[1].TcoeffPredictionFlag: " << dep_layer.QualityLayer[1].TcoeffPredictionFlag << std::endl;

        tstringstream stream2;
        sdep_layer.Serialize(Formaters2::SerializedStream(stream2, 1));
        CHECK_EQUAL(stream.str().c_str(), convert_w2s(stream2.str().c_str()));

    }
    TEST(StructureBuilder_DESERIALIZE_mfxExtSVCRateControl_t_NON_AUTO_ARRAY_LEN)
    {
        mfxExtSVCRateControl rate_ctrl = {0};

        StructureBuilder <mfxExtSVCRateControl> srate_ctrl(_T(""), rate_ctrl);
        SerialNode *pNode = NULL;


        int n;
        CHECK(srate_ctrl.IsDeserialPossible(_T(".NumLayers"),  n, pNode));
        pNode->Parse(_T("2"));
        CHECK_EQUAL(2u, rate_ctrl.NumLayers);

        CHECK(srate_ctrl.IsDeserialPossible(_T(".RateControlMethod"),  n, pNode));
        pNode->Parse(_T("1"));

        std::stringstream stream;
        stream<<".RateControlMethod: " << 1 << std::endl;
        stream<<".NumLayers: " << 2 << std::endl;
        stream<<".Layer[0].TemporalId: " << 0 << std::endl;
        stream<<".Layer[0].DependencyId: "<< 0 << std::endl;
        stream<<".Layer[0].QualityId: " << 0 << std::endl;
        stream<<".Layer[0].CbrVbr.TargetKbps: " << 0 << std::endl;
        stream<<".Layer[0].CbrVbr.InitialDelayInKB: " << 0 << std::endl;
        stream<<".Layer[0].CbrVbr.BufferSizeInKB: " << 0 << std::endl;
        stream<<".Layer[0].CbrVbr.MaxKbps: " << 0 << std::endl;

        stream<<".Layer[1].TemporalId: " << 0 << std::endl;
        stream<<".Layer[1].DependencyId: "<< 0 << std::endl;
        stream<<".Layer[1].QualityId: " << 0 << std::endl;
        stream<<".Layer[1].CbrVbr.TargetKbps: " << 0 << std::endl;
        stream<<".Layer[1].CbrVbr.InitialDelayInKB: " << 0 << std::endl;
        stream<<".Layer[1].CbrVbr.BufferSizeInKB: " << 0 << std::endl;
        stream<<".Layer[1].CbrVbr.MaxKbps: " << 0 << std::endl;

        tstringstream stream2;
        srate_ctrl.Serialize(Formaters2::SerializedStream(stream2, 1));
        CHECK_EQUAL(stream.str().c_str(), convert_w2s(stream2.str().c_str()));

        //CBR and VBR should be displayed as max bitrate, bitrate, etc
        stream2.str(VM_STRING(""));
        stream.str("");

        CHECK(srate_ctrl.IsDeserialPossible(_T(".NumLayers"),  n, pNode));
        pNode->Parse(_T("1"));

        CHECK(srate_ctrl.IsDeserialPossible(_T(".RateControlMethod"),  n, pNode));
        pNode->Parse(_T("2"));

        stream<<".RateControlMethod: " << 2 << std::endl;
        stream<<".NumLayers: " << 1 << std::endl;
        stream<<".Layer[0].TemporalId: " << 0 << std::endl;
        stream<<".Layer[0].DependencyId: "<< 0 << std::endl;
        stream<<".Layer[0].QualityId: " << 0 << std::endl;
        stream<<".Layer[0].CbrVbr.TargetKbps: " << 0 << std::endl;
        stream<<".Layer[0].CbrVbr.InitialDelayInKB: " << 0 << std::endl;
        stream<<".Layer[0].CbrVbr.BufferSizeInKB: " << 0 << std::endl;
        stream<<".Layer[0].CbrVbr.MaxKbps: " << 0 << std::endl;

        srate_ctrl.Serialize(Formaters2::SerializedStream(stream2, 1));
        CHECK_EQUAL(stream.str().c_str(), convert_w2s(stream2.str().c_str()));

        //CQP mode
        stream2.str(VM_STRING(""));
        stream.str("");

        CHECK(srate_ctrl.IsDeserialPossible(_T(".RateControlMethod"),  n, pNode));
        pNode->Parse(_T("3"));

        stream<<".RateControlMethod: " << 3 << std::endl;
        stream<<".NumLayers: " << 1 << std::endl;
        stream<<".Layer[0].TemporalId: " << 0 << std::endl;
        stream<<".Layer[0].DependencyId: "<< 0 << std::endl;
        stream<<".Layer[0].QualityId: " << 0 << std::endl;
        stream<<".Layer[0].Cqp.QPI: " << 0 << std::endl;
        stream<<".Layer[0].Cqp.QPP: " << 0 << std::endl;
        stream<<".Layer[0].Cqp.QPB: " << 0 << std::endl;

        srate_ctrl.Serialize(Formaters2::SerializedStream(stream2, 1));
        CHECK_EQUAL(stream.str().c_str(), convert_w2s(stream2.str().c_str()));

        //AVBR mode
        stream2.str(VM_STRING(""));
        stream.str("");

        CHECK(srate_ctrl.IsDeserialPossible(_T(".RateControlMethod"),  n, pNode));
        pNode->Parse(_T("4"));

        stream<<".RateControlMethod: " << 4 << std::endl;
        stream<<".NumLayers: " << 1 << std::endl;
        stream<<".Layer[0].TemporalId: " << 0 << std::endl;
        stream<<".Layer[0].DependencyId: "<< 0 << std::endl;
        stream<<".Layer[0].QualityId: " << 0 << std::endl;
        stream<<".Layer[0].Avbr.TargetKbps: " << 0 << std::endl;
        stream<<".Layer[0].Avbr.Convergence: " << 0 << std::endl;
        stream<<".Layer[0].Avbr.Accuracy: " << 0 << std::endl;

        srate_ctrl.Serialize(Formaters2::SerializedStream(stream2, 1));
        CHECK_EQUAL(stream.str().c_str(), convert_w2s(stream2.str().c_str()));


        //unknown rate control method 5
        stream2.str(VM_STRING(""));
        stream.str("");

        CHECK(srate_ctrl.IsDeserialPossible(_T(".RateControlMethod"),  n, pNode));
        pNode->Parse(_T("5"));

        stream<<".RateControlMethod: " << 5 << std::endl;
        stream<<".NumLayers: " << 1 << std::endl;
        stream<<".Layer[0].TemporalId: " << 0 << std::endl;
        stream<<".Layer[0].DependencyId: "<< 0 << std::endl;
        stream<<".Layer[0].QualityId: " << 0 << std::endl;

        srate_ctrl.Serialize(Formaters2::SerializedStream(stream2, 1));
        CHECK_EQUAL(stream.str().c_str(), convert_w2s(stream2.str().c_str()));

        //unknown rate control method 0
        stream2.str(VM_STRING(""));
        stream.str("");

        CHECK(srate_ctrl.IsDeserialPossible(_T(".RateControlMethod"),  n, pNode));
        pNode->Parse(_T("0"));

        stream<<".RateControlMethod: " << 0 << std::endl;
        stream<<".NumLayers: " << 1 << std::endl;
        stream<<".Layer[0].TemporalId: " << 0 << std::endl;
        stream<<".Layer[0].DependencyId: "<< 0 << std::endl;
        stream<<".Layer[0].QualityId: " << 0 << std::endl;

        srate_ctrl.Serialize(Formaters2::SerializedStream(stream2, 1));
        CHECK_EQUAL(stream.str().c_str(), convert_w2s(stream2.str().c_str()));

    }


    struct X
    {
        int is_a;//flag switch
        struct 
        {
            mfxU32 a;
            mfxU32 b;
        }x1;
        struct 
        {
            mfxU32 aa;
            mfxU32 bb;
        }x2;
        struct //suppozed to be a union, for tests it doesnt matter 
        {
            mfxU32 aaa;
            mfxU32 bbb;
            mfxU32 always;
        }x3;
    };

    struct IntSwitch
    {
        int & _is;
        bool _bInvert;
        IntSwitch(int &is,  bool bInvert = false)
            : _is(is)
            , _bInvert(bInvert)
        {

        }
        IntSwitch(const IntSwitch & that)
            : _is (that._is)
            , _bInvert(that._bInvert)
        {
        }
        IntSwitch & operator = (IntSwitch & /*that*/)
        {
            return *this;
        }
        bool operator ()()
        {
            return (_is == 1) ^ _bInvert ;
        }
    };
    struct AlwaysTrue
    {
        bool operator()()
        {
            return true;
        }
    };

    class Builder2 
        : public ListNode<PODNode<int>
                    , ListNode<ConditionalNode<IntSwitch, PODNode<mfxU32> >
                    , ListNode<ConditionalNode<IntSwitch, PODNode<mfxU32> > 
                    , ListNode<ConditionalNode<IntSwitch, PODNode<mfxU32> >
                    , ListNode<ConditionalNode<IntSwitch, PODNode<mfxU32> >
                    , ListNode<ConditionalNode<IntSwitch, PODNode<mfxU32> >
                    , ListNode<ConditionalNode<IntSwitch, PODNode<mfxU32> >
                    , ListNode<PODNode<mfxU32> > > > > > > > >
    {
    public:
        //typedef D::H element_type;

        Builder2 (tstring name, X & _value)
            : ListNode(name, make_pod_node(_T(".is_a"), _value.is_a)
            , make_list_node(make_conditional_node(_T(".x1"), IntSwitch(_value.is_a), make_pod_node(_T(".a"), _value.x1.a))
            , make_list_node(make_conditional_node(_T(".x1"), IntSwitch(_value.is_a, true), make_pod_node(_T(".b"), _value.x1.b))
            , make_list_node(make_conditional_node(_T(".x2"), IntSwitch(_value.is_a), make_pod_node(_T(".aa"), _value.x2.aa))
            , make_list_node(make_conditional_node(_T(".x2"), IntSwitch(_value.is_a, true), make_pod_node(_T(".bb"), _value.x2.bb))
            , make_list_node(make_conditional_node(_T(".x3"), IntSwitch(_value.is_a), make_pod_node(_T(".aaa"), _value.x3.aaa))
            , make_list_node(make_conditional_node(_T(".x3"), IntSwitch(_value.is_a, true), make_pod_node(_T(".bbb"), _value.x3.bbb))
            , make_list_node(make_pod_node(_T(".x3.always"), _value.x3.always)))))))))
        {
        }
    };
    
    TEST(UnionNode_switch_1)
    {
        X xstruct={1,2,3,4,5,6,7,8};
        Builder2 b2(_T("X"), xstruct);

        std::stringstream stream;
        stream<<"X.is_a: "<<xstruct.is_a<<std::endl;
        stream<<"X.x1.a: "<<xstruct.x1.a<<std::endl;
        stream<<"X.x2.aa: "<<xstruct.x2.aa<<std::endl;
        stream<<"X.x3.aaa: "<<xstruct.x3.aaa<<std::endl;
        stream<<"X.x3.always: "<<xstruct.x3.always<<std::endl;;

        tstringstream stream2;
        b2.Serialize(Formaters2::SerializedStream(stream2, 1));

        CHECK_EQUAL(stream.str().c_str(), convert_w2s(stream2.str().c_str()));


       // CHECK_EQUAL(stream.str().c_str(), convert_w2s(b2.ToString()));
    }

    TEST(UnionNode_switch_0)
    {
        X xstruct={0,2,3,4,5,6,7,8};
        Builder2 b2(_T("X"), xstruct);

        std::stringstream stream;
        stream<<"X.is_a: "<<xstruct.is_a<<std::endl;
        stream<<"X.x1.b: "<<xstruct.x1.b<<std::endl;
        stream<<"X.x2.bb: "<<xstruct.x2.bb<<std::endl;
        stream<<"X.x3.bbb: "<<xstruct.x3.bbb<<std::endl;
        stream<<"X.x3.always: "<<xstruct.x3.always<<std::endl;

        tstringstream stream2;
        b2.Serialize(Formaters2::SerializedStream(stream2, 1));

        CHECK_EQUAL(stream.str().c_str(), convert_w2s(stream2.str().c_str()));

        //CHECK_EQUAL(stream.str().c_str(), convert_w2s(b2.ToString()));
    }

    class Builder3
        : public ConditionalNode<Switcher<int> , PODNode<mfxU32> > 
    {
    public:
        typedef mfxU32 element_type;
        typedef SerialSingleElement<Switcher<int> > init_type;
        Builder3(const tstring & name, mfxU32 & ref_obj, const init_type& data)
            : ConditionalNode(_T(""), data.m_element, make_pod_node(name, ref_obj))
        {
        }
    };


    TEST (ArrayWithUnions)
    {
        mfxU32 array[4] = {3,4,6,2};
        int bOn = 1;
        ArrayNode<Builder3, 4 > array2 (_T("array"), array, make_serial_single_element(Switcher<int>(bOn, 1)));

        std::stringstream stream;
        stream<<"array[0]: "<<array[0]<<std::endl;
        stream<<"array[1]: "<<array[1]<<std::endl;
        stream<<"array[2]: "<<array[2]<<std::endl;
        stream<<"array[3]: "<<array[3]<<std::endl;

        tstringstream stream2;
        array2.Serialize(Formaters2::SerializedStream(stream2, 1));
        CHECK_EQUAL(stream.str().c_str(), convert_w2s(stream2.str().c_str()));

        stream.str();
        stream2.str();
        bOn = 0;
        
        array2.Serialize(Formaters2::SerializedStream(stream2, 1));
        CHECK_EQUAL(stream.str().c_str(), convert_w2s(stream2.str().c_str()));
    }

    //TODO:add test for actual deserialisations
    TEST(StructureBuilder_DESERIALIZE_mfxExtSVCRateControl)
    {
        mfxExtSVCRateControl svc_rc = {0};
        StructureBuilder <mfxExtSVCRateControl> ssvc_rc(_T("-svc_rc"), svc_rc);
        SerialNode *pNode = NULL;

        int n;
        CHECK(ssvc_rc.IsDeserialPossible(_T("-svc_rc.RateControlMethod"),  n, pNode));
        pNode->Parse(_T("2"));
        CHECK_EQUAL(2u, svc_rc.RateControlMethod);

        CHECK(ssvc_rc.IsDeserialPossible(_T("-svc_rc.Layer[0].TemporalId"),  n, pNode));
        pNode->Parse(_T("12"));
        CHECK_EQUAL(12u, svc_rc.Layer[0].TemporalId);

        CHECK(ssvc_rc.IsDeserialPossible(_T("-svc_rc.Layer[0].DependencyId"),  n, pNode));
        pNode->Parse(_T("13"));
        CHECK_EQUAL(13u, svc_rc.Layer[0].DependencyId);

        CHECK(ssvc_rc.IsDeserialPossible(_T("-svc_rc.Layer[0].QualityId"),  n, pNode));
        pNode->Parse(_T("14"));
        CHECK_EQUAL(14u, svc_rc.Layer[0].QualityId);


        CHECK(ssvc_rc.IsDeserialPossible(_T("-svc_rc.Layer[1023].TemporalId"),  n, pNode));
        pNode->Parse(_T("114"));
        CHECK_EQUAL(114u, svc_rc.Layer[1023].TemporalId);
        //automatic array lenght deserialisation
        CHECK_EQUAL(1024u, svc_rc.NumLayers);


        CHECK(!ssvc_rc.IsDeserialPossible(_T("-svc_Rc.Layer[1024].TemporalId"),  n, pNode));

        CHECK(ssvc_rc.IsDeserialPossible(_T("-svc_rc.Layer[0].CbrVbr.TargetKbps"),  n, pNode));
        pNode->Parse(_T("15"));
        CHECK_EQUAL(15u, svc_rc.Layer[0].CbrVbr.TargetKbps);

        CHECK(ssvc_rc.IsDeserialPossible(_T("-svc_rc.Layer[1023].CbrVbr.InitialDelayInKB"),  n, pNode));
        pNode->Parse(_T("16"));
        CHECK_EQUAL(16u, svc_rc.Layer[1023].CbrVbr.InitialDelayInKB);

        CHECK(ssvc_rc.IsDeserialPossible(_T("-svc_rc.layer[0].Cbrvbr.BufferSizeInKB"),  n, pNode));
        pNode->Parse(_T("17"));
        CHECK_EQUAL(17u, svc_rc.Layer[0].CbrVbr.BufferSizeInKB);

        CHECK(ssvc_rc.IsDeserialPossible(_T("-svc_rc.Layer[0].CbrVbr.maxKbps"),  n, pNode));
        pNode->Parse(_T("18"));
        CHECK_EQUAL(18u, svc_rc.Layer[0].CbrVbr.MaxKbps);

        CHECK(ssvc_rc.IsDeserialPossible(_T("-svc_rc.Layer[0].Cqp.qpi"),  n, pNode));
        pNode->Parse(_T("17"));
        CHECK_EQUAL(17u, svc_rc.Layer[0].Cqp.QPI);

        CHECK(ssvc_rc.IsDeserialPossible(_T("-svc_rc.Layer[0].Cqp.qpp"),  n, pNode));
        pNode->Parse(_T("18"));
        CHECK_EQUAL(18u, svc_rc.Layer[0].Cqp.QPP);

        CHECK(ssvc_rc.IsDeserialPossible(_T("-svc_rc.Layer[0].Cqp.qpb"),  n, pNode));
        pNode->Parse(_T("19"));
        CHECK_EQUAL(19u, svc_rc.Layer[0].Cqp.QPB);
    }
}

