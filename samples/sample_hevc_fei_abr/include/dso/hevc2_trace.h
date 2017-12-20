#pragma once

#include "hevc2_struct.h"

namespace BS_HEVC2
{

extern const char NaluTraceMap[64][16];
extern const char PicTypeTraceMap[3][4];
extern const char ChromaFormatTraceMap[4][12];
extern const char VideoFormatTraceMap[6][12];
extern const char PicStructTraceMap[13][12];
extern const char ScanTypeTraceMap[4][12];
extern const char SliceTypeTraceMap[4][2];
extern const char PredModeTraceMap[3][6];
extern const char PartModeTraceMap[8][12];
extern const char IntraPredModeTraceMap[35][9];
extern const char SAOTypeTraceMap[3][5];
extern const char EOClassTraceMap[4][5];

struct ProfileTraceMap
{
    static const char map[11][12];
    const char* operator[] (unsigned int i);
};

struct ARIdcTraceMap
{
    static const char map[19][16];
    const char* operator[] (unsigned int i);
};

struct SEITypeTraceMap
{
    const char* operator[] (unsigned int i);
};

struct RPLTraceMap
{
    char m_buf[80];
    const char* operator[] (const RefPic& r);
};

}