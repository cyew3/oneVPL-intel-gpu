// Copyright (c) 2004-2018 Intel Corporation
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

#include "umc_defs.h"

#if defined (UMC_ENABLE_VC1_VIDEO_DECODER)

#ifndef __UMC_VC1_DEC_SKIPPING_H_
#define __UMC_VC1_DEC_SKIPPING_H_

namespace UMC
{
    namespace VC1Skipping
    {
        typedef enum
        {
            VC1Routine,
            VC1PostProcDisable,
            VC1MaxSpeed
        } VC1PerfFeatureMode;

        typedef enum
        {
            // routine pipeline
            Normal = 0,
            // no post processing on B frames
            B1,
            // no post processing and simple Quant + Inv.Transform on B frames
            B2,
            // no post processing and simple Quant + Inv.Transform on B frames. Skip every 3-th
            B3,
            // no post processing and simple Quant + Inv.Transform on B frames. Skip every 2-th
            B4,
            // no post processing and simple Quant + Inv.Transform on B frames. Skip all B frames
            B5,
            // all features are disabled for B frames. no post processing on Ref frames
            Ref1,
            // all features are disabled for B frames. no post processing and simple Quant + Inv.Transform on Ref frames
            Ref2,
            // all features are disabled for all frames. Skip all ref frame, all B frame
            Ref3
   
        } SkippingMode;


        // all skipping algorithms is described here
        class VC1SkipMaster
        {
        public:
            VC1SkipMaster():m_iRefSkipPeriod(0),
                            m_iRefSkipCounter(0),
                            m_iBSkipPeriod(0),
                            m_iBSkipCounter(0),
                            m_RefPerfMode(VC1Routine),
                            m_BPerfMode(VC1Routine),
                            m_SpeedMode(Normal),
                            m_bOnDbl(true)
            {
            };
            // skipping algorithms can be different for reference and predictive (B/BI) frames
            // For Ref frames minimum period is 2
            // For B minimum period is 1
            void SetSkipPeriod(bool ForRefFrames, Ipp32u period);
            void SetPerformMode(bool ForRefFrames, Ipp32u perfomMode);

            bool IsDeblockingOn();
            bool ChangeVideoDecodingSpeed(Ipp32s& speed_shift);

            bool IsNeedSkipFrame(Ipp32u picType);
            bool IsNeedPostProcFrame(Ipp32u picType);
            bool IsNeedSimlifyReconstruct(Ipp32u picType);


            void Reset();


            ~ VC1SkipMaster()
            {
            }
        private:
            void MoveToNextState();
            // Skip parameters for reference frames
            Ipp32u m_iRefSkipPeriod;
            Ipp32u m_iRefSkipCounter;
            // Skip parameters for predicted frames
            Ipp32u m_iBSkipPeriod;
            Ipp32u m_iBSkipCounter;

            Ipp32u  m_RefPerfMode;
            Ipp32u  m_BPerfMode;

            Ipp32s m_SpeedMode;

            bool   m_bOnDbl;

        };
    }
}

#endif
#endif

