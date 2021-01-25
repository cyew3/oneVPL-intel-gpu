// Copyright (c) 2018-2020 Intel Corporation
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

#pragma once

#include <bs_reader.h>

namespace BS_Splitter
{
    
enum
{
    ID_RAW = 0,
    ID_IVF,
    ID_MKV,
    ID_WebM = ID_MKV
};

class Reader : public BS_Reader
{
friend class ReaderWrap;
public:
    Reader() : BS_Reader() {}

    virtual ~Reader() { }
};

class ReaderWrap
{
private:
    Reader& m_r;
public:
    const Bs32u & buf_size;
    const Bs32u & offset;
    const Bs8u  & bit;
    const bool  & trace_flag;
    const BS_FADDR_TYPE & file_pos;
    BSErr & last_err;


    ReaderWrap(Reader& r) 
        : m_r(r) 
        , buf_size  (m_r.buf_size)
        , offset    (m_r.offset)
        , bit       (m_r.bit)
        , trace_flag(m_r.trace_flag)
        , file_pos  (m_r.file_pos)
        , last_err  (m_r.last_err)
    {}

    virtual ~ReaderWrap() { }

    inline Bs32u rb() { return m_r.read_1_bit(); }
    inline Bs32u rB() { return m_r.read_1_byte(); }
    inline Bs32u rb(Bs32u n) { return m_r.read_bits(n); }
    inline Bs32u nb(Bs32u n) { return m_r.next_bits(n); }
    inline BSErr rsh(Bs64u n) { return m_r.shift_forward64(n); }

    inline Bs32u rU16() { return (rB() | (rB() << 8)); }
    inline Bs32u rU32() { return (rB() | (rB() << 8) | (rB() << 16) | (rB() << 24)); }
};

class Interface : public ReaderWrap
{
public:
    Interface(Reader& r) : ReaderWrap(r) {}

    virtual ~Interface() { }

    virtual BSErr next_frame(Bs32u& size) = 0;
};

class RAW : public Interface
{
public:
    RAW(Reader& r) : Interface(r) {}
    BSErr next_frame(Bs32u& size);

    virtual ~RAW() { }
};

class IVF : public Interface
{
public:
    bool m_new_seq;

    IVF(Reader& r) : Interface(r), m_new_seq(true) {}

    virtual ~IVF() { }

    BSErr next_frame(Bs32u& size);
};


class MKV : public Interface
{
public:
    MKV(Reader& r) : Interface(r) {}

    virtual ~MKV() { }

    enum ID
    {
        SEGMENT_ID      = 0x18538067,
        CLASTER_ID      = 0x1F43B675,
        SIMPLE_BLOCK_ID = 0xA3
    };

    BSErr next_element(Bs32u& id, Bs64u& size);
    BSErr find_element(Bs32u* _id, Bs64u& size);
    BSErr next_frame(Bs32u& size32);
};

typedef MKV WebM;

Interface* NEW(Bs32u id, Reader& r);

}