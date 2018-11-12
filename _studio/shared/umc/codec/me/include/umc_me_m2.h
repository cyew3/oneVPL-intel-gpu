// Copyright (c) 2007-2018 Intel Corporation
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
