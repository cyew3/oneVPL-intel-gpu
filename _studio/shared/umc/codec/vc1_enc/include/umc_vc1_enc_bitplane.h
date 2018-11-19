// Copyright (c) 2008-2018 Intel Corporation
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

#if defined (UMC_ENABLE_VC1_VIDEO_ENCODER)

#ifndef _ENCODER_VC1_BITPLANE_H_
#define _ENCODER_VC1_BITPLANE_H_

#include "ippvc.h"
#include "umc_vc1_enc_def.h"
#include "umc_structures.h"

namespace UMC_VC1_ENCODER
{
    class VC1EncoderBitplane
    {
    private:
        bool        *pBitplane;
        uint16_t      m_uiWidth;
        uint16_t      m_uiHeight;

    protected:

        inline uint8_t  Get2x3Normal(uint16_t x,uint16_t y);
        inline uint8_t  Get3x2Normal(uint16_t x,uint16_t y);

    public:

        UMC::Status Init(uint16_t width, uint16_t height);
        void Close();
        UMC::Status SetValue(bool value, uint16_t x, uint16_t y);

        VC1EncoderBitplane():
          pBitplane(0),
          m_uiWidth(0),
          m_uiHeight(0)
          {}

        ~VC1EncoderBitplane()
        {
            Close();
        }

    };

}
#endif //_ENCODER_VC1_BITPLANE_H_
#endif //defined (UMC_ENABLE_VC1_VIDEO_ENCODER)
