/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011-2012 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#pragma once

#include "mfx_serial_core.h"
#include "mfxsvc.h"
#include "mfx_ext_buffers.h"
#include <stddef.h>

#define STRING_PAIR(obj, value) _TSTR(.value), obj.value

template <class T>
class StructureBuilder
    : public PODNode<T>
{
public:
    typedef typename PODNode<T>::element_type element_type;
    typedef typename PODNode<T>::init_type init_type;

    StructureBuilder(const tstring & name, const T &data, const init_type & aux = init_type())
        : PODNode<T>(name, data)
    {
        aux;
    }
};

//typedef struct {
//    mfxExtBuffer    Header;
//
//    mfxU16 TemporalScale[8]; /* ratio of this layer frame rate to base layer frame rate */
//    mfxU16 RefBaseDist;
//    mfxU16 reserved1[3];
//
//    struct mfxDependencyLayer{
//        mfxU16    Active;       /* if dependency layer present */
//
//        mfxU16    Width;
//        mfxU16    Height;
//
//        mfxU16    CropX;
//        mfxU16    CropY;
//        mfxU16    CropW;
//        mfxU16    CropH;
//
//        mfxU16    RefLayerDid;  /* reference layer dependency id, disabled if == layer_did */
//        mfxU16    RefLayerQid;  /* reference layer quality id - to be ignored, always best */
//        
//        mfxU16    GopPicSize;
//        mfxU16    GopRefDist;
//        mfxU16    GopOptFlag;
//        mfxU16    IdrInterval;
//
//        mfxU16    BasemodePred; /* four-state option, UNKNOWN/ON/OFF/ADAPTIVE */
//        mfxU16    MotionPred;   /* four-state option, UNKNOWN/ON/OFF/ADAPTIVE */
//        mfxU16    ResidualPred; /* four-state option, UNKNOWN/ON/OFF/ADAPTIVE */
//
//        mfxU16    DisableDeblockingFilter;
//        mfxI16    ScaledRefLayerOffsets[4];
//        mfxU16    ScanIdxPresent; /* tri -state option */
//        mfxU16    reserved2[8];
//
//        mfxU16   TemporalNum;
//        mfxU16   TemporalId[8];
//
//        mfxU16   QualityNum;
//        struct mfxQualityLayer{
//            mfxU16 ScanIdxStart; 
//            mfxU16 ScanIdxEnd;
//
//            mfxU16 TcoeffPredictionFlag; /* currently the same for the dependency layer */
//            mfxU16 reserved3[5];
//        } QualityLayer[16];
//    } DependencyLayer [8];              /* index==id */
//} mfxExtSVCSeqDesc;
//

template<>
struct SerialTypeTrait<mfxExtSVCSeqDesc::mfxDependencyLayer::mfxQualityLayer>
{
    typedef ListNode< PODNode<mfxU16>               // ScanIdxStart
                    , ListNode<PODNode<mfxU16>      // ScanIdxEnd
                    , ListNode<PODNode<mfxU16>      // TcoeffPredictionFlag
                    > > > value_type;  
};

template<>
class StructureBuilder <mfxExtSVCSeqDesc::mfxDependencyLayer::mfxQualityLayer>
    : public SerialTypeTrait< mfxExtSVCSeqDesc::mfxDependencyLayer::mfxQualityLayer >::value_type
{
public:
    typedef mfxExtSVCSeqDesc::mfxDependencyLayer::mfxQualityLayer element_type;
    typedef SerialSingleElement<NullData> init_type;
    
    StructureBuilder(const tstring & m_name, element_type &obj,  const init_type & aux)
        : SerialTypeTrait<mfxExtSVCSeqDesc::mfxDependencyLayer::mfxQualityLayer>::value_type (
               m_name,
               make_pod_node(STRING_PAIR(obj, ScanIdxStart)),
               make_list_node(make_pod_node(STRING_PAIR(obj, ScanIdxEnd)),
               make_list_node(make_pod_node(STRING_PAIR(obj, TcoeffPredictionFlag))
               )))
    {
        aux;
    }
};

template<>
struct SerialTypeTrait<mfxExtSVCSeqDesc::mfxDependencyLayer>
{
    typedef ListNode<PODNode<tstring>
                    , ListNode<PODNode<mfxU16>                       // Active
                    , ListNode<PODNode<mfxU16>                       // Width
                    , ListNode<PODNode<mfxU16>                       // Height
                    , ListNode<PODNode<mfxU16>                       // CropX
                    , ListNode<PODNode<mfxU16>                       // CropY
                    , ListNode<PODNode<mfxU16>                       // CropW
                    , ListNode<PODNode<mfxU16>                       // CropH
                    , ListNode<PODNode<mfxU16>                       // RefLayerDid
                    , ListNode<PODNode<mfxU16>                       // RefLayerQid
                    , ListNode<PODNode<mfxU16>                       // GopPicSize;
                    , ListNode<PODNode<mfxU16>                       // GopRefDist;
                    , ListNode<PODNode<mfxU16>                       // GopOptFlag;
                    , ListNode<PODNode<mfxU16>                       // IdrInterval;
                    , ListNode<PODNode<mfxU16>                       // BasemodePred
                    , ListNode<PODNode<mfxU16>                       // MotionPred
                    , ListNode<PODNode<mfxU16>                       // ResidualPred
                    , ListNode<PODNode<mfxU16>                       // DisableDeblockingFilterIdc
                    , ListNode<ArrayNode<PODNode<mfxI16>, 4>         // ScaledRefLayerOffsets[4]
                    , ListNode<PODNode<mfxU16>                       // ScanIdxPresent
                    , ListNode<PODNode<mfxU16>                       // TemporalNum
                    , ListNode<ArrayNode<PODNode<mfxU16>, 8, mfxU16> // TemporalId[8]
                    , ListNode<PODNode<mfxU16>                       // QualityNum
                    , ListNode<ArrayNode<StructureBuilder<mfxExtSVCSeqDesc::mfxDependencyLayer::mfxQualityLayer>, 16, mfxU16> // mfxQualityLayer[16]
                     > > > > > > > > > > > > > > > > > > > > > > > > value_type;
};


template<>
class StructureBuilder <mfxExtSVCSeqDesc::mfxDependencyLayer>
    : public SerialTypeTrait<mfxExtSVCSeqDesc::mfxDependencyLayer>::value_type
{
public:
    typedef mfxExtSVCSeqDesc::mfxDependencyLayer element_type;
    typedef tstring init_type;

    StructureBuilder(const tstring & m_name, element_type &obj, tstring & inputFile)
        : SerialTypeTrait<mfxExtSVCSeqDesc::mfxDependencyLayer>::value_type(
            m_name,
            make_pod_node(VM_STRING(".InputFile"), inputFile), 
            make_list_node(make_pod_node(STRING_PAIR(obj, Active)), 
            make_list_node(make_pod_node(STRING_PAIR(obj, Width)), 
            make_list_node(make_pod_node(STRING_PAIR(obj, Height)), 
            make_list_node(make_pod_node(STRING_PAIR(obj, CropX)), 
            make_list_node(make_pod_node(STRING_PAIR(obj, CropY)), 
            make_list_node(make_pod_node(STRING_PAIR(obj, CropW)), 
            make_list_node(make_pod_node(STRING_PAIR(obj, CropH)), 
            make_list_node(make_pod_node(STRING_PAIR(obj, RefLayerDid)),
            make_list_node(make_pod_node(STRING_PAIR(obj, RefLayerQid)),
            make_list_node(make_pod_node(STRING_PAIR(obj, GopPicSize)),
            make_list_node(make_pod_node(STRING_PAIR(obj, GopRefDist)),
            make_list_node(make_pod_node(STRING_PAIR(obj, GopOptFlag)),
            make_list_node(make_pod_node(STRING_PAIR(obj, IdrInterval)),
            make_list_node(make_pod_node(STRING_PAIR(obj, BasemodePred)),
            make_list_node(make_pod_node(STRING_PAIR(obj, MotionPred)),
            make_list_node(make_pod_node(STRING_PAIR(obj, ResidualPred)),
            make_list_node(make_pod_node(STRING_PAIR(obj, DisableDeblockingFilter)),
            make_list_node(ArrayNode<PODNode<mfxI16>, 4>(STRING_PAIR(obj, ScaledRefLayerOffsets)),
            make_list_node(make_pod_node(STRING_PAIR(obj, ScanIdxPresent)), 
            make_list_node(make_pod_node(STRING_PAIR(obj, TemporalNum)), 
            make_list_node(ArrayNode<PODNode<mfxU16>, 8, mfxU16>(STRING_PAIR(obj, TemporalId), obj.TemporalNum), 
            make_list_node(make_pod_node(STRING_PAIR(obj, QualityNum)), 
            make_list_node(ArrayNode<StructureBuilder<element_type::mfxQualityLayer>, 16, mfxU16>(STRING_PAIR(obj, QualityLayer), obj.QualityNum)
            ))))))))))))))))))))))))
    {
    }
};

template<>
struct SerialTypeTrait<mfxExtSVCSeqDesc>
{
    typedef ListNode<ArrayNode<PODNode<mfxU16>, 8>
          , ListNode<PODNode<mfxU16>
          , ListNode<ArrayNode<StructureBuilder<mfxExtSVCSeqDesc::mfxDependencyLayer>, 8 > > > > value_type;
};

template <>
class StructureBuilder <mfxExtSVCSeqDesc>
    : public  SerialTypeTrait<mfxExtSVCSeqDesc>::value_type

{
public:
    StructureBuilder(const tstring & name, mfxExtSVCSeqDesc &obj, SerialCollection<tstring> &inputFiles)
        : SerialTypeTrait<mfxExtSVCSeqDesc>::value_type(
        name, 
        ArrayNode<PODNode<mfxU16>, 8>(STRING_PAIR(obj, TemporalScale)),
        make_list_node(make_pod_node(STRING_PAIR(obj, RefBaseDist)),
        make_list_node(ArrayNode<StructureBuilder<mfxExtSVCSeqDesc::mfxDependencyLayer>, 8 >(STRING_PAIR(obj, DependencyLayer), inputFiles, true))))
    {
    }
};

//to make 1:1 relation to mfx_structures
//STATIC_ASSERT(sizeof(mfxExtSVCSeqDesc) == sizeof(mfxExtSVCSeqDesc), c1);
//
//#define CHECK_FIELD_SVC_SEQ_DESC(field) DYN_CHECK_FIELD(mfxExtSVCSeqDesc, mfxExtSVCSeqDesc, field);

//final structure to be used for mfxExtSVCSeqDesc
//template <>
//class StructureBuilder <mfxExtSVCSeqDesc>
//    : public  StructureBuilder <mfxExtSVCSeqDesc>
//{
//public:
//    StructureBuilder(const tstring & name, mfxExtSVCSeqDesc &obj, SerialCollection<tstring> &inputFiles)
//        : StructureBuilder <mfxExtSVCSeqDesc>(name, (mfxExtSVCSeqDesc&)obj, inputFiles)
//    {
//        /*CHECK_FIELD_SVC_SEQ_DESC(TemporalScale);
//        CHECK_FIELD_SVC_SEQ_DESC(RefBaseDist);
//        CHECK_FIELD_SVC_SEQ_DESC(DependencyLayer);
//        CHECK_FIELD_SVC_SEQ_DESC(DependencyLayer->Active);
//        CHECK_FIELD_SVC_SEQ_DESC(DependencyLayer->Width);
//        CHECK_FIELD_SVC_SEQ_DESC(DependencyLayer->Height);
//        CHECK_FIELD_SVC_SEQ_DESC(DependencyLayer->CropX);
//        CHECK_FIELD_SVC_SEQ_DESC(DependencyLayer->CropY);
//        CHECK_FIELD_SVC_SEQ_DESC(DependencyLayer->CropW);
//        CHECK_FIELD_SVC_SEQ_DESC(DependencyLayer->CropH);
//        CHECK_FIELD_SVC_SEQ_DESC(DependencyLayer->RefLayerDid);
//        CHECK_FIELD_SVC_SEQ_DESC(DependencyLayer->RefLayerQid);
//        CHECK_FIELD_SVC_SEQ_DESC(DependencyLayer->GopPicSize);
//        CHECK_FIELD_SVC_SEQ_DESC(DependencyLayer->GopRefDist);
//        CHECK_FIELD_SVC_SEQ_DESC(DependencyLayer->GopOptFlag);
//        CHECK_FIELD_SVC_SEQ_DESC(DependencyLayer->IdrInterval);
//        CHECK_FIELD_SVC_SEQ_DESC(DependencyLayer->BasemodePred);
//        CHECK_FIELD_SVC_SEQ_DESC(DependencyLayer->MotionPred);
//        CHECK_FIELD_SVC_SEQ_DESC(DependencyLayer->ResidualPred);
//        CHECK_FIELD_SVC_SEQ_DESC(DependencyLayer->DisableDeblockingFilter);
//        CHECK_FIELD_SVC_SEQ_DESC(DependencyLayer->ScaledRefLayerOffsets);
//        CHECK_FIELD_SVC_SEQ_DESC(DependencyLayer->ScanIdxPresent);
//        CHECK_FIELD_SVC_SEQ_DESC(DependencyLayer->TemporalNum);
//        CHECK_FIELD_SVC_SEQ_DESC(DependencyLayer->TemporalId);
//        CHECK_FIELD_SVC_SEQ_DESC(DependencyLayer->QualityNum);
//        CHECK_FIELD_SVC_SEQ_DESC(DependencyLayer->QualityLayer);
//        CHECK_FIELD_SVC_SEQ_DESC(DependencyLayer->QualityLayer->ScanIdxStart);
//        CHECK_FIELD_SVC_SEQ_DESC(DependencyLayer->QualityLayer->ScanIdxEnd);
//        CHECK_FIELD_SVC_SEQ_DESC(DependencyLayer->QualityLayer->TcoeffPredictionFlag);*/
//    }
//};

//////////////////////////////////////////////////////////////////////////

//typedef struct {
//    mfxExtBuffer    Header;
//
//    mfxU16  RateControlMethod;
//    mfxU16  reserved1[10]; //VaryQpShift, VaryNumCoeff
//
//
//    mfxU16  NumLayers;
//    struct mfxLayer{
//        mfxU16  TemporalId;
//        mfxU16  DependencyId;
//        mfxU16  QualityId;
//        mfxU16  reserved2[5];
//
//        union{
//            struct{
//                mfxU32  TargetKbps;
//                mfxU32  InitialDelayInKB;
//                mfxU32  BufferSizeInKB;
//                mfxU32  MaxKbps;
//                mfxU32  reserved3[4];
//            } CbrVbr;
//
//            struct{
//                mfxU16  QPI;
//                mfxU16  QPP;
//                mfxU16  QPB;
//            }Cqp;
//
//            struct{
//                mfxU32  TargetKbps;
//                mfxU32  Convergence;
//                mfxU32  Accuracy;
//            }Avbr;
//        };
//    }Layer[1024];
//} mfxExtSVCRateControl;

template<>
struct SerialTypeTrait<mfxExtSVCRateControl::mfxLayer>
{
    typedef ListNode<PODNode<mfxU16>  //TemporalId
          , ListNode<PODNode<mfxU16>  //DependencyId
          , ListNode<PODNode<mfxU16>  //QualityId
          , ListNode<ConditionalNode<Switcher<mfxU16, set_ext<mfxU16> >  //CbrVbr
          , ListNode<PODNode<mfxU32>  // TargetKbps
          , ListNode<PODNode<mfxU32>  //InitialDelayInKB
          , ListNode<PODNode<mfxU32>  //BufferSizeInKB
          , ListNode<PODNode<mfxU32> > > > > >  //MaxKbps
          , ListNode<ConditionalNode<Switcher<mfxU16, mfxU16 > //cqp
          , ListNode<PODNode<mfxU16>  //QPI
          , ListNode<PODNode<mfxU16>  //QPP
          , ListNode<PODNode<mfxU16> > > > >  //QPB
          , ListNode<ConditionalNode<Switcher<mfxU16, mfxU16 > //Avbr
          , ListNode<PODNode<mfxU32>  //TargetKbps
          , ListNode<PODNode<mfxU32>  //Convergence
          , ListNode<PODNode<mfxU32> > > > >  //Accuracy
          > > > > > > value_type;
};


template <>
class StructureBuilder<mfxExtSVCRateControl::mfxLayer>
    : public SerialTypeTrait<mfxExtSVCRateControl::mfxLayer>::value_type
{
public:
    typedef mfxExtSVCRateControl::mfxLayer element_type;
    typedef SerialSingleElement<std::pair<Switcher<mfxU16, set_ext<mfxU16> >, //vbr or cbr
                                std::pair<Switcher<mfxU16>,                   //cqp
                                          Switcher<mfxU16>                    //avbr
                                > > > init_type;

    StructureBuilder( const tstring & name
                    , mfxExtSVCRateControl::mfxLayer &obj
                    , const init_type & vbr_cqp_or_avbr)
        : SerialTypeTrait<mfxExtSVCRateControl::mfxLayer>::value_type(name
        , make_pod_node(STRING_PAIR(obj, TemporalId))
        , make_list_node(make_pod_node(STRING_PAIR(obj, DependencyId))
        , make_list_node(make_pod_node(STRING_PAIR(obj, QualityId))
        , make_list_node(make_conditional_node(VM_STRING(".CbrVbr"), vbr_cqp_or_avbr.m_element.first
            , make_list_node(make_pod_node(STRING_PAIR(obj.CbrVbr, TargetKbps))
            , make_list_node(make_pod_node(STRING_PAIR(obj.CbrVbr, InitialDelayInKB))
            , make_list_node(make_pod_node(STRING_PAIR(obj.CbrVbr, BufferSizeInKB))
            , make_list_node(make_pod_node(STRING_PAIR(obj.CbrVbr, MaxKbps)))))))
        , make_list_node(make_conditional_node(VM_STRING(".Cqp"), vbr_cqp_or_avbr.m_element.second.first
            , make_list_node(make_pod_node(STRING_PAIR(obj.Cqp, QPI))
            , make_list_node(make_pod_node(STRING_PAIR(obj.Cqp, QPP))
            , make_list_node(make_pod_node(STRING_PAIR(obj.Cqp, QPB))))))
        , make_list_node(make_conditional_node(VM_STRING(".Avbr"), vbr_cqp_or_avbr.m_element.second.second
            , make_list_node(make_pod_node(STRING_PAIR(obj.Avbr, TargetKbps))
            , make_list_node(make_pod_node(STRING_PAIR(obj.Avbr, Convergence))
            , make_list_node(make_pod_node(STRING_PAIR(obj.Avbr, Accuracy))))))
            ))))))
    {
    }
};

template<>
struct SerialTypeTrait<mfxExtSVCRateControl>
{
    typedef ListNode<PODNode<mfxU16>  //RateControlMethod
          , ListNode<PODNode<mfxU16>  // NumLayers
          , ListNode<ArrayNode<StructureBuilder<mfxExtSVCRateControl::mfxLayer>, 1024, mfxU16> >  //Layer
          > > value_type;
};

template<>
class StructureBuilder<mfxExtSVCRateControl>
    : public  SerialTypeTrait<mfxExtSVCRateControl>::value_type
{
public:
    StructureBuilder(const tstring &name, mfxExtSVCRateControl &obj)
        : SerialTypeTrait<mfxExtSVCRateControl>::value_type(name, make_pod_node(STRING_PAIR(obj, RateControlMethod))
        , make_list_node(make_pod_node(STRING_PAIR(obj, NumLayers))
        , make_list_node(ArrayNode<StructureBuilder<mfxExtSVCRateControl::mfxLayer>, 1024, mfxU16>(STRING_PAIR(obj, Layer), obj.NumLayers
            , make_serial_single_element(std::make_pair( make_equal_switch(obj.RateControlMethod, make_set_ex((mfxU16)MFX_RATECONTROL_CBR, (mfxU16)MFX_RATECONTROL_VBR)), 
                                         std::make_pair( make_equal_switch(obj.RateControlMethod, (mfxU16)MFX_RATECONTROL_CQP),
                                                         make_equal_switch(obj.RateControlMethod, (mfxU16)MFX_RATECONTROL_AVBR))))))))
    {
    }
};

////to make 1:1 relation to mfx_structures
//STATIC_ASSERT(sizeof(mfxExtSVCRateControl) == sizeof(mfxExtSVCRateControl), c2);
//
//#define CHECK_FIELD_RATE_CTRL(field) DYN_CHECK_FIELD(mfxExtSVCRateControl, mfxExtSVCRateControl, field);

//final structure to be used for mfxExtSVCRateControl
//template <>
//class StructureBuilder <mfxExtSVCRateControl>
//    : public  StructureBuilder <mfxExtSVCRateControl>
//{
//public:
//    StructureBuilder(const tstring & name, mfxExtSVCRateControl &obj)
//        : StructureBuilder <mfxExtSVCRateControl>(name, (mfxExtSVCRateControl&)obj)
//    {
//        ////lets check whole structure for alignment with mediasdk structure
//        //CHECK_FIELD_RATE_CTRL(RateControlMethod);
//        //CHECK_FIELD_RATE_CTRL(NumLayers);
//        //CHECK_FIELD_RATE_CTRL(Layer);
//        //CHECK_FIELD_RATE_CTRL(Layer->TemporalId);
//        //CHECK_FIELD_RATE_CTRL(Layer->DependencyId);
//        //CHECK_FIELD_RATE_CTRL(Layer->QualityId);
//        //CHECK_FIELD_RATE_CTRL(Layer->CbrVbr.TargetKbps);
//        //CHECK_FIELD_RATE_CTRL(Layer->CbrVbr.InitialDelayInKB);
//        //CHECK_FIELD_RATE_CTRL(Layer->CbrVbr.BufferSizeInKB); 
//        //CHECK_FIELD_RATE_CTRL(Layer->CbrVbr.MaxKbps);
//        //CHECK_FIELD_RATE_CTRL(Layer->Cqp.QPI);
//        //CHECK_FIELD_RATE_CTRL(Layer->Cqp.QPP);
//        //CHECK_FIELD_RATE_CTRL(Layer->Cqp.QPB);
//        //CHECK_FIELD_RATE_CTRL(Layer->Avbr.TargetKbps);
//        //CHECK_FIELD_RATE_CTRL(Layer->Avbr.Convergence);
//        //CHECK_FIELD_RATE_CTRL(Layer->Avbr.Accuracy);
//    }
//};


template<>
struct SerialTypeTrait <mfxExtCodingOptionQuantMatrix>
{
    typedef           ListNode<PODNode<mfxU16>                       // bIntraQM
                    , ListNode<PODNode<mfxU16>                       // bInterQM
                    , ListNode<PODNode<mfxU16>                       // bChromaIntraQM
                    , ListNode<PODNode<mfxU16>                       // bChromaInterQM
                    , ListNode<ArrayNode<PODNode<mfxU8>, 64>         // IntraQM[64]
                    , ListNode<ArrayNode<PODNode<mfxU8>, 64>         // InterQM[64]
                    , ListNode<ArrayNode<PODNode<mfxU8>, 64>         // ChromaIntraQM[64]
                    , ListNode<ArrayNode<PODNode<mfxU8>, 64>         // ChromaInterQM[64]
                     > > > > > > > >   value_type;
};

template<>
class StructureBuilder<mfxExtCodingOptionQuantMatrix>
    : public  SerialTypeTrait<mfxExtCodingOptionQuantMatrix>::value_type
{
public:
    StructureBuilder(const tstring &name, mfxExtCodingOptionQuantMatrix &obj)
        : SerialTypeTrait<mfxExtCodingOptionQuantMatrix>::value_type(name        
        , make_pod_node(STRING_PAIR(obj, bIntraQM))
        , make_list_node(make_pod_node(STRING_PAIR(obj, bInterQM))
        , make_list_node(make_pod_node(STRING_PAIR(obj, bChromaIntraQM))
        , make_list_node(make_pod_node(STRING_PAIR(obj, bChromaInterQM))
        , make_list_node(ArrayNode<PODNode<mfxU8>, 64>(STRING_PAIR(obj, IntraQM))
        , make_list_node(ArrayNode<PODNode<mfxU8>, 64>(STRING_PAIR(obj, InterQM))
        , make_list_node(ArrayNode<PODNode<mfxU8>, 64>(STRING_PAIR(obj, ChromaIntraQM))
        , make_list_node(ArrayNode<PODNode<mfxU8>, 64>(STRING_PAIR(obj, ChromaInterQM))
        ))))
        ))))
    {
    }
};

