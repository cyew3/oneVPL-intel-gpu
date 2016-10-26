//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2008-2011 Intel Corporation. All Rights Reserved.
//

#ifndef __MFX_TIMING_H__
#define __MFX_TIMING_H__

//uncomment this define for timing instrumentation
//#define MFX_TIMING

#define MFX_TIMING_CHECK_REGISTRY   0x10000000
#define MFX_TIMING_PROXY_LIBSW      0x20000000
#define MFX_TIMING_PROXY_LIBHW      0x40000000

#define MFX_INVALID_STATUS_CODE     (int)0xffff

// for SetInput/SetOutput
enum
{
    MFX_COMP_DECODE  = 1,
    MFX_COMP_VPP     = 2,
    MFX_COMP_ENCODE  = 3,
};

#ifndef NULL
#ifdef __cplusplus
#define NULL    0
#else
#define NULL    ((void *)0)
#endif
#endif

namespace MFX
{

#ifndef MFX_TIMING

// empty declaration for normal build
class AutoTimer
{
public:
    static int Init(const void *filename, int level = -1) { filename; level; return 0; }
    static unsigned int CreateUniqTaskId() {return 0;}

    AutoTimer(const char *name){name;}
    AutoTimer(const char *name, const char *param, int parami = 0)    { name; param; parami;}
    AutoTimer(const char *name, bool bCreateId, unsigned int child_of, unsigned int parent_of)  { name; bCreateId; parent_of;child_of;}
    ~AutoTimer() {};
    AutoTimer() {};

    void SetInput(void *handle, int component_index = -1) { component_index; handle; };
    void SetOutput(void *handle, int component_index = -1) { component_index; handle; };
    void AddParam(const char *param_name, const char *param_value) { param_name; param_value; };
    void AddParam(const char *param_name, int param_value) { param_name; param_value; };
    void Start(const char *name) { name; };
    void Stop(int return_code = MFX_INVALID_STATUS_CODE) { return_code; };
};

#else // MFX_TIMING

class AutoTimer
{
public:
    // static method to initialize timing capture
    static int Init(const void *filename, int level = -1);
    //returned uniq task ID
    static unsigned int CreateUniqTaskId();

    AutoTimer(const char *name); // start timer
    AutoTimer(const char *name, const char *param, int parami = 0); // start timer
    AutoTimer(const char *name, bool bCreateId, unsigned int child_of, unsigned int parent_of); // start timer
    ~AutoTimer(); // stop timer
    AutoTimer(); // default constructor. Aftewards call SetInput/SetOutput/AddParam, then Start


    void SetInput(void *handle, int component_index = -1);
    void SetOutput(void *handle, int component_index = -1);
    void AddParam(const char *param_name, const char *param_value);
    void AddParam(const char *param_name, int param_value);
    void Start(const char *name);
    void Stop(int return_code = MFX_INVALID_STATUS_CODE);
    unsigned int ID(){return m_timerid;}

protected:
    void Reset();
    void SendNamedEvent(int opcode);

    const char  *m_Name;
    unsigned int m_timerid;
    unsigned int m_parentid;
    unsigned int m_childid;
};

#endif // MFX_TIMING

} // namespace UMC

#endif // __MFX_TIMING_H__
