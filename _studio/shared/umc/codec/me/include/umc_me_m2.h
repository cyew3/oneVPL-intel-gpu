//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2007-2008 Intel Corporation. All Rights Reserved.
//

/* LIMITATIONS
    From MeBase:
      Only frame mode, only Y is used.
    From MPEG
      Only 16x16, only half-pixel.
    Only frame size, search range and BlockVarThresh parameters are used in this ME.
  Changes in MeBase:
    Added to MeParams BlockVarThresh field, which is quantizer value of mpeg.
    Added to ME_MB skipped field, which signals that current MB with provided MV can be skipped.
  Difference in returned results:
    returned MV are half-pixel precision.
    FW vector always returned in [0] and BW always in [1], even when no FW vector.
    When is skipped, all required vectors are set and cost returned in [0].
    For bidir prediction 3 costs are returned - [0]-FW, [1]-BW, [2]-bidir.

*/
#if 0
#include "umc_me.h"

#ifndef _UMC_ME_M2_H_
#define _UMC_ME_M2_H_

namespace UMC
{

class MeMPEG2 : public MeBase
{
    DYNAMIC_CAST_DECL(MeMPEG2,MeBase);

    public:
        MeMPEG2();
        ~MeMPEG2();
        //top level functions
        virtual bool EstimateSkip() {return false;}

        bool Init(MeInitParams *par); //allocates memory
        bool EstimateFrame(MeParams *par); //par and res should be allocated by caller, except ME_Frame::MBs
        void Close(); //frees memory, also called from destructor
    protected:
        void me_block(const Ipp8u* src, Ipp32s sstep, const Ipp8u* ref, Ipp32s rstep,
            Ipp32s bh, Ipp32s bx, Ipp32s by,
            Ipp32s ll, Ipp32s lt, Ipp32s lr, Ipp32s lb,
            MeMV* mv, Ipp32s* cost, MeMV* mvleft, MeMV* mvtop);
};

}//namespace
#endif /*_UMC_ME_M2_H_*/

#endif
