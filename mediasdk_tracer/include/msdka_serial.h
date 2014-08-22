/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2009-2014 Intel Corporation. All Rights Reserved.

File Name: msdka_serial.h

\* ****************************************************************************** */

#pragma once

#include <tchar.h>
#include "mfxstructures.h"
#include "mfx_dispatcher_log.h"
#include "msdka_structures.h"

//dump messages from dispatcher
class DispatcherLogRecorder : public IMsgHandler
{
    static DispatcherLogRecorder m_Instance;
public:
    static DispatcherLogRecorder& get(){return m_Instance;}
    virtual void Write(int level, int opcode, const char * msg, va_list argptr);
};


//better formating using spaces from subfunction call
class StackIncreaser
{
public:
    StackIncreaser();
    int get();
    ~StackIncreaser();
};


void dump_mfxVideoParam(FILE *fd, int level, TCHAR *prefix, Component c, mfxVideoParam *vp, bool bDumpExtBuffer = true);
void dump_mfxFrameAllocRequest(FILE *fd, int level, TCHAR *prefix, mfxFrameAllocRequest *request);
void dump_mfxVPPStat(FILE *fd,  int level, TCHAR *prefix, mfxVPPStat *stat);
void dump_mfxEncodeStat(FILE *fd, int level, TCHAR *prefix, mfxEncodeStat *stat);
void dump_mfxDecodeStat(FILE *fd, int level, TCHAR *prefix, mfxDecodeStat *stat);
void dump_mfxEncodeCtrl(FILE *fd, int level, TCHAR *prefix, mfxEncodeCtrl *ctl);
void dump_mfxFrameSurface1(FILE *fd, int level, TCHAR *prefix,mfxFrameSurface1 *fs);
void dump_mfxFrameSurface1Raw(FILE *fd, mfxFrameSurface1 *fs, mfxFrameAllocator *pAlloc);
void dump_mfxFrameData(FILE *fd, int level, TCHAR *prefix, TCHAR *prefix2, mfxFrameData *data);
void dump_mfxBitstream(FILE *fd, int level, TCHAR *prefix, mfxBitstream *bs);
void dump_mfxBitstreamRaw(FILE *fd, mfxBitstream *bs);
void dump_mfxStatus(FILE *fd, int level, TCHAR *prefix, mfxStatus sts);
void dump_StatusMap(FILE *fd, int level, TCHAR *prefix, StatusMap *sm);
void dump_mfxVersion(FILE *fd, int level, TCHAR *prefix, mfxVersion *ver);
void dump_mfxIMPL(FILE *fd, int level, TCHAR *prefix, mfxIMPL impl);
void dump_mfxPriority(FILE *fd, int level, TCHAR *prefix, mfxPriority priority);
//prefix constants stored separately
void dump_format_wprefix(FILE *fd, int level, int nprefix, TCHAR *format,...);
void dump_mfxENCInput(FILE *fd, int level, TCHAR *prefix, mfxENCInput *in);
void dump_mfxENCOutput(FILE *fd, int level, TCHAR *prefix, mfxENCOutput *out);

//////////////////////////////////////////////////////////////////////////
///desialization



struct ExtendedBufferOverride;
struct StatusOverride;

void scan_mfxVideoParam(TCHAR *line, Component c, mfxVideoParam *vp, ExtendedBufferOverride *ebo);
void scan_mfxFrameSurface1(TCHAR *line, mfxFrameSurface1 *fs);
void scan_mfxStatus(TCHAR *line, StatusOverride *so);
void scan_mfxIMPL(TCHAR *line,mfxIMPL *impl);
void scan_mfxVersion(TCHAR *line,mfxVersion *pVer);
void scan_mfxPriority(TCHAR *line,mfxPriority *pPty);