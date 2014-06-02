#pragma once

#include "mfxvideo.h"
#include "mfxvp8.h"
#include "mfxmvc.h"
#include "mfxjpeg.h"
#include "mfxpcp.h"
#include "mfxplugin.h"
#include "mfxcamera.h"
#include <iostream>

#define TYPEDEF_MEMBER(base, member, name) typedef std::remove_reference<decltype(((base*)0)->member)>::type name;
TYPEDEF_MEMBER(mfxExtOpaqueSurfaceAlloc,  In,                  mfxExtOpaqueSurfaceAlloc_InOut)
TYPEDEF_MEMBER(mfxExtAVCRefListCtrl,      PreferredRefList[0], mfxExtAVCRefListCtrl_Entry)
TYPEDEF_MEMBER(mfxExtPictureTimingSEI,    TimeStamp[0],        mfxExtPictureTimingSEI_TimeStamp)
TYPEDEF_MEMBER(mfxExtAvcTemporalLayers,   Layer[0],            mfxExtAvcTemporalLayers_Layer)
TYPEDEF_MEMBER(mfxExtAVCEncodedFrameInfo, UsedRefListL0[0],    mfxExtAVCEncodedFrameInfo_RefList)
TYPEDEF_MEMBER(mfxExtVPPVideoSignalInfo,  In,                  mfxExtVPPVideoSignalInfo_InOut)
TYPEDEF_MEMBER(mfxExtEncoderROI,          ROI[0],              mfxExtEncoderROI_Entry)
typedef union { mfxU32 n; char c[4]; } mfx4CC;

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
        *this << p.c[0] << p.c[1] << p.c[2] << p.c[3] << std::hex << "(0x" << p.n << ')' << std::dec; 
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

inline tsAutoTrace& operator<<(tsAutoTrace& os, const char* p)              { (std::ostream&)os << p; return os; }
inline tsAutoTrace& operator<<(tsAutoTrace& os, const unsigned char* p)     { (std::ostream&)os << p; return os; }
inline tsAutoTrace& operator<<(tsAutoTrace& os, const char p)               { (std::ostream&)os << p; return os; }
inline tsAutoTrace& operator<<(tsAutoTrace& os, const unsigned char p)      { (std::ostream&)os << p; return os; }
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
    tsTrace& operator << (const mfxExtEncoderROI& p);
    tsTrace& operator << (const mfxExtCamGammaCorrection& p);
    tsTrace& operator << (const mfxExtCamVignetteCorrection& p);
    tsTrace& operator << (const mfxInfoMFX& p);
    tsTrace& operator << (const mfxPluginUID& p);
    tsTrace& operator << (const mfxFrameData& p);
    tsTrace& operator << (mfxStatus& p);

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