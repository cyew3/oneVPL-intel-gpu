// Copyright (c) 2019-2020 Intel Corporation
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

#include <bs_splitter.h>

namespace BS_Splitter
{

BSErr RAW::next_frame(Bs32u& size) 
{
    size = buf_size;
    return BS_ERR_NONE;
}

BSErr IVF::next_frame(Bs32u& size) 
{
    if (m_new_seq)
    {
        m_new_seq = false;
        BS_TRACE(rU32(), IVF::Signature);
        BS_TRACE(rU16(), IVF::Version);
        BS_TRACE(rU16(), IVF::Length);
        BS_TRACE(rU32(), IVF::FourCC);
        BS_TRACE(rU16(), IVF::Width);
        BS_TRACE(rU16(), IVF::Height);
        BS_TRACE(rU32(), IVF::FrameRate);
        BS_TRACE(rU32(), IVF::TimeScale);
        BS_TRACE(rU32(), IVF::NumFrames);
        BS_TRACE(rU32(), IVF::Unused);
    }
    BS_TRACE(size = rU32(), IVF::FrameSize);
    BS_TRACE(rU32(), IVF::TimeStampL);
    BS_TRACE(rU32(), IVF::TimeStampH);

    return last_err;
}


BSErr MKV::next_element(Bs32u& id, Bs64u& size)
{
    Bs16u n = 64-7;
    Bs32u tmp_buf = 0;
    size = 0;

    while (!(tmp_buf = rB()))
        BSCE;

    id = tmp_buf;
    tmp_buf <<= 24;

    while (!(0x80000000 & tmp_buf))
    {
        tmp_buf <<= 1;
        id = (id<<8)|rB();
        BSCE;
    }
    size = rB();
    tmp_buf = (Bs32u)size<<24;

    while (!(0x80000000 & tmp_buf))
    {
        tmp_buf <<= 1;
        size = (size<<8)|rB();
        BSCE;
        n -= 7;
    }

    size <<= n;
    size >>= n;

    BS_TRACE(id, MKV::<SegmentId>);
    BS_TRACE((Bs32u)size, MKV::<SegmentSz>);

    return last_err;
}

BSErr MKV::find_element(Bs32u* _id, Bs64u& size)
{
    Bs32u id = 0, target_id = 0;
    Bs64u rest = -1;

    for (Bs16u i = 0; _id[i]; i++) 
        target_id = _id[i];

    while (1)
    {
        next_element(id, size);BSCE;

        if (target_id == id) 
            return last_err;

        if (*_id == id)
        {
            _id++;
            rest = size;
        }
        else 
        {
            rsh(size);BSCE;
            BS_TRACE(id, MKV::</SegmentId>);

            if (size >= rest)
            {
                _id--;
                rest = -1;
                continue;
            }
            rest -= size;
        }
    }

    return last_err;
}

BSErr MKV::next_frame(Bs32u& size32) 
{
    Bs32u id[4] = {SEGMENT_ID,CLASTER_ID,SIMPLE_BLOCK_ID,0};
    Bs64u size = 0;

    find_element(id, size);BSCE;
    rsh(4);

    size32 = Bs32u(size - 4);

    return last_err;
}


Interface* NEW(Bs32u id, Reader& r) 
{
    switch (id)
    {
    case ID_RAW: return new RAW(r);
    case ID_IVF: return new IVF(r);
    case ID_MKV: return new MKV(r);
    default: break;
    }
    return 0;
}

}