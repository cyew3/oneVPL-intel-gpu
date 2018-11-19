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

#ifndef _UMC_VME_H_
#define _UMC_VME_H_

//#define ME_VME
#ifdef ME_VME

#include "IntelVideoVME.h"
#include "umc_me.h"

typedef struct
{
    short    x,y;        // 16x16 macroblock coordinates
    I16PAIR    c;          // prediction center as absolute coordinates in Q-pel
    int        mode;        // 0: one 16x16, 1: two 16x8s, 2: two 8x16s, 3: four 8x8s
    int        dist;       // adjusted SAD distortion
    I16PAIR    mv[4];      // best motion vectors in differences from the prediction center for four 8x8 sub-blocks
} Macroblock;

namespace UMC
{
    class MeVme : public MeBase
    {
        DYNAMIC_CAST_DECL(MeVme, MeBase)
    public:
        MeVme();
        virtual ~MeVme();

        virtual bool Init(MeInitParams *par); //allocates memory
        virtual bool EstimateFrame(MeParams *par); //par and res should be allocated by caller, except MeFrame::MBs
        virtual void Close(); //frees memory, also called from destructor
    protected:
        IntelVideoVME    *vme;
        VMEOutput    vout;
        void EstimateMB(int16_t x, int16_t y, int32_t SKIP_TYPE);
    };

}//namespase UMC
#endif//ME_VME
#endif //_UMC_VME_H_
