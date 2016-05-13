/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014-2016 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#pragma once

#include "mfxvideo.h"
#include "mfxvp8.h"
#include "mfxmvc.h"
#include "mfxjpeg.h"
#include "mfxpcp.h"
#include "mfxplugin.h"
#include "mfxcamera.h"
#include "mfxfei.h"
#include "mfxfeih265.h"
#include "mfxla.h"
#include "mfxsc.h"

#include "bs_parser.h"
#include <iostream>

#define TYPEDEF_MEMBER(base, member, name) typedef std::remove_reference<decltype(((base*)0)->member)>::type name;
TYPEDEF_MEMBER(mfxExtOpaqueSurfaceAlloc,  In,                  mfxExtOpaqueSurfaceAlloc_InOut)
TYPEDEF_MEMBER(mfxExtAVCRefListCtrl,      PreferredRefList[0], mfxExtAVCRefListCtrl_Entry)
TYPEDEF_MEMBER(mfxExtPictureTimingSEI,    TimeStamp[0],        mfxExtPictureTimingSEI_TimeStamp)
TYPEDEF_MEMBER(mfxExtAvcTemporalLayers,   Layer[0],            mfxExtAvcTemporalLayers_Layer)
TYPEDEF_MEMBER(mfxExtAVCEncodedFrameInfo, UsedRefListL0[0],    mfxExtAVCEncodedFrameInfo_RefList)
TYPEDEF_MEMBER(mfxExtVPPVideoSignalInfo,  In,                  mfxExtVPPVideoSignalInfo_InOut)
TYPEDEF_MEMBER(mfxExtEncoderROI,          ROI[0],              mfxExtEncoderROI_Entry)
TYPEDEF_MEMBER(mfxExtDirtyRect,           Rect[0],             mfxExtDirtyRect_Entry)
TYPEDEF_MEMBER(mfxExtMoveRect,            Rect[0],             mfxExtMoveRect_Entry)
typedef union { mfxU32 n; char c[4]; } mfx4CC;
typedef mfxExtAVCRefLists::mfxRefPic mfxExtAVCRefLists_mfxRefPic;
typedef mfxExtFeiEncMV::mfxExtFeiEncMVMB mfxExtFeiEncMV_MB;

class tsAutoTrace : public std::ostream
{
public:
    std::string m_off;

    tsAutoTrace(std::streambuf* sb) 
        : std::ostream(sb)
        , m_off("")
    {}

    inline void inc_offset() { m_off += "  "; }
    inline void dec_offset() { m_off.erase(m_off.size() - 2, 2); }

    inline tsAutoTrace& operator<<(void* p) { (std::ostream&)*this << p; return *this; }
    inline tsAutoTrace& operator<<(mfxU8* p) { *this << (void*)p; return *this; }
    inline tsAutoTrace& operator<<(mfxEncryptedData*& p){ *this << (void*)p; return *this; }
    inline tsAutoTrace& operator<<(mfx4CC& p) 
    {
        for(size_t i = 0; i < 4; ++i)
        {
            if(p.c[i])  (std::ostream&)*this << p.c[i];
            else        (std::ostream&)*this << "\\0";
        }
        (std::ostream&)*this << std::hex << "(0x" << p.n << ')' << std::dec; 
        return *this;
    }

#define STRUCT(name, fields) virtual tsAutoTrace &operator << (const name& p); virtual tsAutoTrace &operator << (const name* p);
#define FIELD_T(type, _name)
#define FIELD_S(type, _name)

#include "ts_struct_decl.h"

#undef STRUCT
#undef FIELD_T
#undef FIELD_S
};

//printing "\0" symbol in unicode causes text file to be mailformed
inline tsAutoTrace& operator<<(tsAutoTrace& os, const char p)
{
    if(p)  (std::ostream&)os << p;
    else   (std::ostream&)os << "\\0";
    (std::ostream&)os << " (" << +p << ')';  //operator+(T v) casts v to printable numerical type
    return os;
}
inline tsAutoTrace& operator<<(tsAutoTrace& os, const unsigned char p)
{
    if(p)  (std::ostream&)os << p;
    else   (std::ostream&)os << "\\0";
    (std::ostream&)os << " (" << +p << ')'; 
    return os;
}
inline tsAutoTrace& operator<<(tsAutoTrace& os, const char* p)              { (std::ostream&)os << p; return os; }
inline tsAutoTrace& operator<<(tsAutoTrace& os, const unsigned char* p)     { (std::ostream&)os << p; return os; }
inline tsAutoTrace& operator<<(tsAutoTrace& os, const short p)              { (std::ostream&)os << p; return os; }
inline tsAutoTrace& operator<<(tsAutoTrace& os, const unsigned short p)     { (std::ostream&)os << p; return os; }
inline tsAutoTrace& operator<<(tsAutoTrace& os, const int p)                { (std::ostream&)os << p; return os; }
inline tsAutoTrace& operator<<(tsAutoTrace& os, const unsigned int p)       { (std::ostream&)os << p; return os; }
inline tsAutoTrace& operator<<(tsAutoTrace& os, const long p)               { (std::ostream&)os << p; return os; }
inline tsAutoTrace& operator<<(tsAutoTrace& os, const unsigned long p)      { (std::ostream&)os << p; return os; }
inline tsAutoTrace& operator<<(tsAutoTrace& os, const long long p)          { (std::ostream&)os << p; return os; }
inline tsAutoTrace& operator<<(tsAutoTrace& os, const unsigned long long p) { (std::ostream&)os << p; return os; }
inline tsAutoTrace& operator<<(tsAutoTrace& os, const float p)              { (std::ostream&)os << p; return os; }
inline tsAutoTrace& operator<<(tsAutoTrace& os, const double p)             { (std::ostream&)os << p; return os; }
inline tsAutoTrace& operator<<(tsAutoTrace& os, const std::string p)        { (std::ostream&)os << p.c_str(); return os; }

class tsTrace : public tsAutoTrace
{
private:
    bool    m_interpret_ext_buf;
    mfxU32  m_flags;
public:
    tsTrace(std::streambuf* sb) : tsAutoTrace(sb), m_interpret_ext_buf(true) {}

    
    tsTrace& operator << (const mfxExtBuffer& p);
    tsTrace& operator << (const mfxVideoParam& p);
    tsTrace& operator << (const mfxEncodeCtrl& p);
    tsTrace& operator << (const mfxExtAVCRefLists& p);
    tsTrace& operator << (const mfxExtEncoderROI& p);
    tsTrace& operator << (const mfxExtDirtyRect& p);
    tsTrace& operator << (const mfxExtMoveRect& p);
    tsTrace& operator << (const mfxExtDirtyRect_Entry& p);
    tsTrace& operator << (const mfxExtMoveRect_Entry& p);
    tsTrace& operator << (const mfxExtCamGammaCorrection& p);
    tsTrace& operator << (const mfxExtCamVignetteCorrection& p);
    tsTrace& operator << (const mfxInfoMFX& p);
    tsTrace& operator << (const mfxPluginUID& p);
    tsTrace& operator << (const mfxFrameData& p);
    tsTrace& operator << (mfxStatus& p);
    tsTrace& operator << (BSErr& p);
    tsTrace& operator<<(const mfxExtFeiEncMV& p);

    template<typename T> tsTrace& operator << (T& p) { (tsAutoTrace&)*this << p; return *this; }
    template<typename T> tsTrace& operator << (T* p) { (tsAutoTrace&)*this << p; return *this; }
};

typedef struct 
{
    unsigned char* data;
    unsigned int size;
} rawdata;

rawdata hexstr(const void* d, unsigned int s);
std::ostream &operator << (std::ostream &os, rawdata p);
