/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2013-2020 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */
#pragma once
#include "msdk_ts.h"
#include "mfxvideo.h"
#include "mfxaudio.h"
#include "mfxvp8.h"
#include "mfxsvc.h"
#include "mfxmvc.h"
#include "mfxjpeg.h"
#include "mfxplugin.h"
#include "test_trace.h"
#include "vm_file.h"
#include "bs_parser++.h"
#include <vector>

#define RETERR(sts, msg) {std::cout << __FILE__ << ":" << __LINE__ << ": FAILED: " << msg << "\n"; return sts;}
#define CHECK(cond, msg) if(!(cond))RETERR(msdk_ts::resFAIL, msg)
#define CHECK_STS(res, expected)\
    if((res) != (expected)){\
        std::cout << #res << " = " << (res) << "\n" << #expected << " = " << (expected) << "\n";\
        CHECK(false, __FUNCTION__ << ": Returned status " << (res) <<" is wrong! Expected " << (expected) << "\n");\
    }
#define MAX(x,y) ((x)>(y) ? (x) : (y))
#define MIN(x,y) ((x)<(y) ? (x) : (y))
#define TRACE_PAR(par) if(var_def<bool>("trace", false)) {INC_PADDING(); std::cout << print_param.padding << #par " = " << par << '\n'; DEC_PADDING();}

typedef std::vector<mfxFrameSurface1> FrameSurfPool;

class VMFileHolder{
public:
    VMFileHolder() : f(0), file_attr(0), file_size(0){};
    ~VMFileHolder(){ close(); };
    void close() { if(f) {vm_file_close(f); f = NULL;}};
    bool open(const char* name, const vm_char* mode){
        if(sizeof(char) == sizeof(vm_char))
            f = vm_file_open((vm_char*)name, mode);
        else{
            size_t i;
            vm_char* v = new vm_char[strlen(name)+1];
            for(i = 0; name[i]; i++) v[i] = name[i];
            v[i] = 0;
            f = vm_file_open(v, mode);
            delete[] v;
        }
        return !!f;
    };
    bool is_open() { return !!f; };
    int read(void* dst, unsigned int n) { return vm_file_read(dst, 1, n, f); };
    int write(void* src, unsigned int n) { return vm_file_write(src, 1, n, f); };
    int getinfo(const char* name) { 
        Ipp32s res;
        if(sizeof(char) == sizeof(vm_char))
            res = vm_file_getinfo((vm_char*)name, &file_size, &file_attr);
        else{
            size_t i;
            vm_char* v = new vm_char[strlen(name)+1];
            for(i = 0; name[i]; i++) v[i] = name[i];
            v[i] = 0;
            res = vm_file_getinfo(v, &file_size, &file_attr);
            delete[] v;
        }
        return res;
    }
    Ipp64u ftell() { return vm_file_ftell(f); };
    Ipp64u fseek(Ipp64s position, VM_FILE_SEEK_MODE mode){return vm_file_fseek(f, position, mode); };
    bool eof()
    {
        //eof flag set if only app tried to _read data beyond the file_
        Ipp8u tempBuf;
        Ipp32u readData = read(&tempBuf, 1);
        if(0 != readData)
            fseek(-1, VM_FILE_SEEK_CUR);
        return !!vm_file_feof(f);
    }
    Ipp64u file_size;
    Ipp32u file_attr;
private:
    vm_file* f;
};

class CRC32{
public:
    CRC32() : value(0xFFFFFFFFL){};
    ~CRC32(){};
    void update(unsigned char *buf, int len);
    unsigned long get() { return value; };
private:
    unsigned long value;
    static const unsigned long table[256];
};

#if defined(LINUX32) || defined (LINUX64)
#if defined(LIBVA_SUPPORT)
#include <va/va.h>
#if defined(LIBVA_X11_SUPPORT)
  #include <X11/Xlib.h>
  #undef Status /*Xlib.h: #define Status int*/
  #include <va/va_x11.h>
  #define VAAPI_X_DEFAULT_DISPLAY ":0.0"
  #define VAAPI_GET_X_DISPLAY(_display) (Display*) (_display)
#endif //#if defined(LIBVA_X11_SUPPORT)
#if defined(LIBVA_DRM_SUPPORT)
  #include <va/va_drm.h>
  #include <fcntl.h>
  #include <unistd.h>
#endif //#if defined(LIBVA_DRM_SUPPORT)

/*
class LinDisplayHolder
{
public:
    LinDisplayHolder();
    ~LinDisplayHolder();

    static LinDisplayHolder & get_instance();

    mfxHDL* GetVADisplayHandle();
    mfxStatus CloseVADisplay();

private:
    static LinDisplayHolder* single_instance;
#if defined(LIBVA_X11_SUPPORT)
    void* x_display;
#endif
#if defined(LIBVA_DRM_SUPPORT)
    int m_fd;
#endif
    mfxHDL va_display;
};*/
#endif //defined(LIBVA_SUPPORT)
#endif //(defined(LINUX32) || defined(LINUX64))

