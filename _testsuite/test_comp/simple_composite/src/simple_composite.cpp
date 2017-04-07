//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2005-2017 Intel Corporation. All Rights Reserved.
//

#define ENABLE_OUTPUT    // Disabling this flag removes all YUV file writing
#define ENABLE_INPUT     // Disabling this flag removes all YUV file reading. Replaced by pre-initialized surface data. Workload runs for 1000 frames
//#define TEST_VPP_COMP_RESET

#if defined(_WIN32) || defined(_WIN64)
  #define ENABLE_BENCHMARK
#else
 #define fopen_s(FH,FNAME,FMODE) *(FH) = fopen((FNAME),(FMODE));
#endif

#define FRAME_NUM_50 50

#include <string>
#include <vector>
#include <algorithm>
#include <functional>
#include <cctype>
#include <locale>
#include <map>
#include <iostream>
#include <sstream>
#include <fstream>
#include <sstream>
#include "common_utils.h"
//#include "vaapi_utils.h"
#include <string>
#include <algorithm>
#include <functional>
#include <cctype>
#include <memory>
#include <locale>
#include <stdexcept>

#if ! defined(_WIN32) && ! defined(_WIN64)
  #include <dlfcn.h>
  #include <fcntl.h>
  #include <va/va.h>
  #include <va/va_drm.h>
  #include <unistd.h>
  #include <sys/types.h>
  #include <sys/stat.h>
  #include <unistd.h>
  #include <fcntl.h>

  #include <dirent.h>
  #include <stdexcept>

  #include <drm_fourcc.h>
  #include <intel_bufmgr.h>
#endif


using namespace std;

#ifdef LIBVA_SUPPORT
namespace MfxLoader
{

    class SimpleLoader
    {
    public:
        SimpleLoader(const char * name);

        void * GetFunction(const char * name);

        ~SimpleLoader();

    private:
        SimpleLoader(SimpleLoader&);
        void operator=(SimpleLoader&);

        void * so_handle;
    };
    
    SimpleLoader::SimpleLoader(const char * name)
    {
        so_handle = dlopen(name, RTLD_GLOBAL | RTLD_NOW);
    }
  
    void * SimpleLoader::GetFunction(const char * name)
    {
        void * fn_ptr = dlsym(so_handle, name);
        if (!fn_ptr)
            throw std::runtime_error("Can't find function");
        return fn_ptr;
    }
  
    SimpleLoader::~SimpleLoader()
    {
        dlclose(so_handle);
    }
   
    #define SIMPLE_LOADER_STRINGIFY1( x) #x
    #define SIMPLE_LOADER_STRINGIFY(x) SIMPLE_LOADER_STRINGIFY1(x)
    #define SIMPLE_LOADER_DECORATOR1(fun,suffix) fun ## _ ## suffix
    #define SIMPLE_LOADER_DECORATOR(fun,suffix) SIMPLE_LOADER_DECORATOR1(fun,suffix)


    // Following macro applied on vaInitialize will give:  vaInitialize((vaInitialize_type)lib.GetFunction("vaInitialize"))
    #define SIMPLE_LOADER_FUNCTION(name) name( (SIMPLE_LOADER_DECORATOR(name, type)) lib.GetFunction(SIMPLE_LOADER_STRINGIFY(name)) )

    class VA_Proxy
    {
    private:
        SimpleLoader lib; // should appear first in member list

    public:
        typedef VAStatus (*vaInitialize_type)(VADisplay, int *, int *);
        typedef VAStatus (*vaTerminate_type)(VADisplay);
        typedef VAStatus (*vaCreateSurfaces_type)(VADisplay, unsigned int,
            unsigned int, unsigned int, VASurfaceID *, unsigned int,
            VASurfaceAttrib *, unsigned int);
        typedef VAStatus (*vaDestroySurfaces_type)(VADisplay, VASurfaceID *, int);
        typedef VAStatus (*vaCreateBuffer_type)(VADisplay, VAContextID,
            VABufferType, unsigned int, unsigned int, void *, VABufferID *);
        typedef VAStatus (*vaDestroyBuffer_type)(VADisplay, VABufferID);
        typedef VAStatus (*vaMapBuffer_type)(VADisplay, VABufferID, void **pbuf);
        typedef VAStatus (*vaUnmapBuffer_type)(VADisplay, VABufferID);
        typedef VAStatus (*vaDeriveImage_type)(VADisplay, VASurfaceID, VAImage *);
        typedef VAStatus (*vaDestroyImage_type)(VADisplay, VAImageID);


        VA_Proxy();
        ~VA_Proxy();

        const vaInitialize_type      vaInitialize;
        const vaTerminate_type       vaTerminate;
        const vaCreateSurfaces_type  vaCreateSurfaces;
        const vaDestroySurfaces_type vaDestroySurfaces;
        const vaCreateBuffer_type    vaCreateBuffer;
        const vaDestroyBuffer_type   vaDestroyBuffer;
        const vaMapBuffer_type       vaMapBuffer;
        const vaUnmapBuffer_type     vaUnmapBuffer;
        const vaDeriveImage_type     vaDeriveImage;
        const vaDestroyImage_type    vaDestroyImage;
    };

    VA_Proxy::VA_Proxy()
    : lib("libva.so")
    , SIMPLE_LOADER_FUNCTION(vaInitialize)
    , SIMPLE_LOADER_FUNCTION(vaTerminate)
    , SIMPLE_LOADER_FUNCTION(vaCreateSurfaces)
    , SIMPLE_LOADER_FUNCTION(vaDestroySurfaces)
    , SIMPLE_LOADER_FUNCTION(vaCreateBuffer)
    , SIMPLE_LOADER_FUNCTION(vaDestroyBuffer)
    , SIMPLE_LOADER_FUNCTION(vaMapBuffer)
    , SIMPLE_LOADER_FUNCTION(vaUnmapBuffer)
    , SIMPLE_LOADER_FUNCTION(vaDeriveImage)
    , SIMPLE_LOADER_FUNCTION(vaDestroyImage)
    {
    }

    VA_Proxy::~VA_Proxy()
    {}


    //#if defined(LIBVA_DRM_SUPPORT)
    class VA_DRMProxy
    {
    private:
        SimpleLoader lib; // should appear first in member list

    public:
        typedef VADisplay (*vaGetDisplayDRM_type)(int);


        VA_DRMProxy();
        ~VA_DRMProxy();

        const vaGetDisplayDRM_type vaGetDisplayDRM;
    };
    
    VA_DRMProxy::VA_DRMProxy()
    : lib("libva-drm.so")
    , SIMPLE_LOADER_FUNCTION(vaGetDisplayDRM)
    {
    }

    VA_DRMProxy::~VA_DRMProxy()
    {}
    //#endif
    
    
} // namespace MfxLoader
#endif // #ifdef LIBVA_SUPPORT

bool isActivatedSW = false;

typedef struct {
    char* pStreamName;
    mfxU16 width;
    mfxU16 height;
} InputSubstream;

typedef struct {
    mfxU16 Y;
    mfxU16 U;
    mfxU16 V;
} Background;

/* total number streams for vpp composition is 64 now!
 * This is means 1 primary and 63 sub-streams */
#define MAX_INPUT_STREAMS 64

// Get free raw frame surface
int GetFreeSurfaceIndex(mfxFrameSurface1** pSurfacesPool, mfxU16 nPoolSize)
{
    if (pSurfacesPool)
        for (mfxU16 i = 0; i < nPoolSize; i++)
            if (0 == pSurfacesPool[i]->Data.Locked)
                return i;
    return MFX_ERR_NOT_FOUND;
}

enum ColorFormat
{
    kYV12 = 0,
    kNV12 = 1,
    kRGB4 = 2,
    kAYUV = 3,
    kY210 = 4,
    kY410 = 5
};

class ColorMap
{
private:
    map<const string, ColorFormat> map_;
public:
    ColorMap()
    {
        map_.insert(make_pair("YV12", kYV12));
        map_.insert(make_pair("NV12", kNV12));
        map_.insert(make_pair("RGB4", kRGB4));
        map_.insert(make_pair("AYUV", kAYUV));
        map_.insert(make_pair("Y210", kY210));
        map_.insert(make_pair("Y410", kY410));
    }
public:
    ColorFormat Get(const string &str) const
    {
        auto it = map_.find(str);
        if (map_.end() == it)
            throw std::runtime_error("Invalid color format name");
        return it->second;
    }

};

struct ProgramArguments
{
    // Path to file with parameters
    string par;
    // Output file
    string output;

    // src color format
    ColorFormat scc;
    // dest color format
    ColorFormat dcc;
    //  width  of dst video
    mfxU32 dW;
    // height of dst video
    mfxU32 dH;

    mfxU32 frames_to_process;
    mfxU32 maxWidth;
    mfxU32 maxHeight;
    // Num tiles
    mfxU32 iNumTiles;

    // Restart related
    bool   need_reset;
    mfxU32 restart_frame;
    string reset_par;

    // MFX Implementation
    mfxIMPL impl;
};

bool from_string(const std::string& str, ColorFormat &obj)
{
    std::istringstream iss(str);
    ColorMap cm;
    obj = cm.Get(str);
    return !iss.fail();
}

template <typename T>
bool from_string(const std::string& str, T &obj)
{
    std::istringstream iss(str);
    iss >> obj;
    return !iss.fail();
}

inline std::vector<std::string> ConvertInputString(int argc, char *argv[])
{
    std::vector<std::string> ret(argc-1);
    int i = 0;
    std::for_each(ret.begin(),ret.end(), [&](std::string &s)
             {
                 s = std::string(argv[++i]);
             });
    return ret;
}

template <typename T>
bool ParseValue(const vector<string> &args,
                const string &prefix, T &value)
{
    auto arg = find(args.begin(), args.end(), prefix);
    if (arg != args.end() && (arg + 1) != args.end())
    {
        from_string(*(++arg), value);
        return true;
    }
    return false;
}

template <typename T>
bool ParseValueWithDefault(const vector<string> &args,
                           const string &prefix, T &value, const T &def)
{
    auto arg = find(args.begin(), args.end(), prefix);
    if (arg != args.end() && (arg + 1) != args.end())
    {
        return from_string(*(++arg), value);
    }
    value = def;
    return true;
}


ProgramArguments ParseInputString(const vector<string> &args)
{
    ProgramArguments pa = {};

    pa.need_reset = false;
    pa.frames_to_process = 0;
    pa.maxHeight = 0;
    pa.maxWidth  = 0;
    pa.impl      = MFX_IMPL_HARDWARE;

    if (!ParseValue(args, "-par", pa.par))
        throw std::runtime_error("Par file is not specified or invalid (-par)");
    if (!ParseValue(args, "-o", pa.output))
        throw std::runtime_error("output file not specified or invalid (-o)");
    if (!ParseValueWithDefault(args, "-scc", pa.scc, kNV12))
        throw std::runtime_error("-scc value invalid");
    if (!ParseValueWithDefault(args, "-dcc", pa.dcc, kNV12))
        throw std::runtime_error("-dcc value invalid");
    if (!ParseValue(args, "-dW", pa.dW))
        pa.dW = 0;
    if (!ParseValue(args, "-dH", pa.dH))
        pa.dH = 0;
    if (!ParseValue(args, "-numTiles", pa.iNumTiles))
        pa.iNumTiles = 0;
    if (ParseValue(args, "-reset_par", pa.reset_par))
    {
        pa.need_reset = true;
        if (!ParseValue(args, "-reset_start", pa.restart_frame))
        {
            throw std::runtime_error("Specify # of frame for reset (-reset_start)");
        }
    }

    if (find(args.begin(), args.end(), "-d3d11") != args.end())
        pa.impl |= MFX_IMPL_VIA_D3D11;
    
    if (find(args.begin(), args.end(), "-sw") != args.end())
    {
        isActivatedSW = true;
        pa.impl |= MFX_IMPL_SOFTWARE;
    }

    return pa;
}

void PrintHelp(ostream &out)
{
    // Mandatory args
    out << "Mandatory args: " << endl;
    out << "-par <filename> path to the parameters file" << endl;
    out << "-o <filename>" << endl;
    out << "-scc <NV12 | YV12 | RGB4 | AYUV | Y210 | Y410> (default: NV12 - source color format)" << endl;
    out << "-dcc <NV12 | YV12 | RGB4 | AYUV | Y210 | Y410> (default: NV12 - dest color format)" << endl;
    out << "-reset_par <filename> path to the parameters file" << endl;
    out << "-reset_start index if the start frame for reset" << endl;
    out << "-d3d11 flag enables MFX_IMPL_VIA_D3D11 implementation"<< endl;
    out << "-sw flag enables MFX_IMPL_SOFTWARE implementation"<< endl;
    out << "-numTiles <numTiles> quantity of tiles (means enabling tiles)"<< endl;
}

// trim from start
static inline std::string &ltrim(std::string &s) {
        s.erase(s.begin(), std::find_if(s.begin(), s.end(), std::not1(std::ptr_fun<int, int>(std::isspace))));
        return s;
}

// trim from end
static inline std::string &rtrim(std::string &s) {
        s.erase(std::find_if(s.rbegin(), s.rend(), std::not1(std::ptr_fun<int, int>(std::isspace))).base(), s.end());
        return s;
}

// trim from both ends
static inline std::string &trim(std::string &s) {
        return ltrim(rtrim(s));
}


map<string, string> ParsePar(ifstream &stream, string stream_name) {
    map<string, string> params;

    string line;
    int counter = 128; /*This is maximal number of all params per one stream */
    params["stream"] = stream_name;

    while (counter--) {
        getline(stream, line);
        istringstream iss(line);
        if (line.compare("") == 0) /* We have reach EOF...*/
        {
            break;
        }
        string key, value;
        getline(iss, key,   '=');
        if (key.compare("\r") == 0)
        {
            break;
        }
        getline(iss, value, '=');
        params[key] = value;
    }

    return params;
}

class Composition {
public:
    Composition(ProgramArguments *pa)
    {
        m_bInited  = false;
        m_nStreams = 0;
        m_pVPPSurfacesIn    = 0;
        m_pVPPSurfacesOut   = 0;
        m_surfaceBuffersOut = 0;
        m_nVPPSurfNumIn  = 0;
        m_nVPPSurfNumOut = 0;
        m_Arguments = pa;
        m_pVPPSurfacesIn = 0;

        if (m_Arguments->dcc == kNV12 || m_Arguments->dcc == kYV12 || m_Arguments->dcc == kAYUV)
        {
            m_Color.Y = 16;
            m_Color.U = 128;
            m_Color.V = 128;
        }
        else if (m_Arguments->dcc == kY210 || m_Arguments->dcc == kY410)
        {
            m_Color.Y = 64;
            m_Color.U = 512;
            m_Color.V = 512;
        }
        else
        {
            m_Color.Y = 0;
            m_Color.U = 0;
            m_Color.V = 0;
        }
#if MFX_VERSION > 1023
        m_VPPComp.NumTiles = pa->iNumTiles;
#endif

        memset(m_RealWidths,  0, sizeof(m_RealWidths));
        memset(m_RealHeights, 0, sizeof(m_RealHeights));
    }
    ~Composition()
    {
        ReleaseSurfaces();
    }

    mfxStatus Init(string par_file)
    {
        mfxStatus sts = MFX_ERR_NONE;

        sts = ParseParFile(par_file);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

        sts = FillVPPParams();
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

        sts = FillCompositionParams();
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

        m_ExtBuffer[0]          = (mfxExtBuffer*)&m_VPPComp;
        m_VPPParams.NumExtParam = 1;
        m_VPPParams.ExtParam    = (mfxExtBuffer**)m_ExtBuffer;

        return sts;
    }

    mfxStatus Reset()
    {
        for(int i = 0; i < m_nStreams; i++)
        {
            if ( m_Files[i] )
                fclose(m_Files[i]);
        }
        m_bInited  = false;
        m_nStreams = 0;
        return MFX_ERR_NONE;
    }

    mfxStatus Reset(string par_file)
    {
        mfxStatus sts = MFX_ERR_NONE;
        sts = Reset();
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
        return Init(par_file);
    }

    void SetVPP(MFXVideoVPP *vpp)
    {
        m_VPP = vpp;
    }

    mfxVideoParam *GetVPPParams()
    {
        return &m_VPPParams;
    }

    mfxStatus GetInSurfaces(mfxFrameSurface1  *** pVPPSurfacesIn, mfxU16 *nVPPSurfNumIn)
    {
        *pVPPSurfacesIn = m_pVPPSurfacesIn;
        *nVPPSurfNumIn  = m_nVPPSurfNumIn;
        return MFX_ERR_NONE;
    }

    mfxStatus GetOutSurfaces(mfxFrameSurface1  *** pVPPSurfacesOut, mfxU16  *nVPPSurfNumOut)
    {
        *pVPPSurfacesOut = m_pVPPSurfacesOut;
        *nVPPSurfNumOut  = m_nVPPSurfNumOut;
        return MFX_ERR_NONE;
    }

    mfxU16 GetStreamCount()
    {
        return m_nStreams;
    }

    FILE * GetFileHandle(int index)
    {
        if ( index > m_nStreams )
            return NULL;

        return m_Files[index];
    }

    mfxU16 GetRealWidth(mfxU16 idx)
    {
        return m_RealWidths[idx];
    }

    mfxU16 GetRealHeight(mfxU16 idx)
    {
        return m_RealHeights[idx];
    }

    mfxStatus RequestSurfaces()
    {
        mfxStatus sts = ReleaseSurfaces();
        memset(m_VPPRequest, 0, sizeof(mfxFrameAllocRequest)*2);
        sts = m_VPP->QueryIOSurf(&m_VPPParams, m_VPPRequest);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

        m_nVPPSurfNumIn  = m_VPPRequest[0].NumFrameSuggested;
        m_nVPPSurfNumOut = m_VPPRequest[1].NumFrameSuggested;

        // Allocate surfaces for VPP: In
        // - Width and height of buffer must be aligned, a multiple of 32
        // - Frame surface array keeps pointers all surface planes and general frame info
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

        m_pVPPSurfacesIn = new mfxFrameSurface1*[m_nVPPSurfNumIn];
        MSDK_CHECK_POINTER(m_pVPPSurfacesIn, MFX_ERR_MEMORY_ALLOC);

        mfxU8* surfaceBuffersIn[MAX_INPUT_STREAMS] = {0}; // 1 primary stream + max 63 substreams
        mfxU8  bitsPerPixel = 12;  // NV12 format is a 12 bits per pixel format

        /* IF RGB4/AYUV/Y210/Y410 case */
        if (m_Arguments->dcc == kRGB4 || m_Arguments->dcc == kAYUV || m_Arguments->dcc == kY210 || m_Arguments->dcc == kY410)
            bitsPerPixel = 32;

        for (int i = 0; i < m_nVPPSurfNumIn; i++)
        {
            mfxU16 width; //= (mfxU16)MSDK_ALIGN32(atoi(subparams[i]["w"].c_str()));
            mfxU16 height;// = (mfxU16)MSDK_ALIGN32(atoi(subparams[i]["h"].c_str()));

            width  = (mfxU16)MSDK_ALIGN32(atoi(m_Params[i]["width"].c_str()));
            height = (mfxU16)MSDK_ALIGN32(atoi(m_Params[i]["height"].c_str()));
            mfxU32 surfaceSize = width * height * bitsPerPixel / 8;

            surfaceBuffersIn[i] = new mfxU8[surfaceSize];
            MSDK_CHECK_POINTER(surfaceBuffersIn[i], MFX_ERR_MEMORY_ALLOC);

            m_pVPPSurfacesIn[i] = new mfxFrameSurface1;
            MSDK_CHECK_POINTER(m_pVPPSurfacesIn[i], MFX_ERR_MEMORY_ALLOC);
            memset(m_pVPPSurfacesIn[i], 0, sizeof(mfxFrameSurface1));
            memcpy(&(m_pVPPSurfacesIn[i]->Info), &m_Streams[i], sizeof(mfxFrameInfo));

            m_pVPPSurfacesIn[i]->Data.Y = surfaceBuffersIn[i];
            if (m_Arguments->scc == kNV12)
            {
                m_pVPPSurfacesIn[i]->Data.U = m_pVPPSurfacesIn[i]->Data.Y + width*height;
                m_pVPPSurfacesIn[i]->Data.V = m_pVPPSurfacesIn[i]->Data.U + 1;
                m_pVPPSurfacesIn[i]->Data.Pitch = width;
            }
            else if (m_Arguments->scc == kY210)
            {
                m_pVPPSurfacesIn[i]->Data.U = m_pVPPSurfacesIn[i]->Data.Y + 2;
                m_pVPPSurfacesIn[i]->Data.V = m_pVPPSurfacesIn[i]->Data.Y + 6;
                m_pVPPSurfacesIn[i]->Data.Pitch = 4*width;
            }
            else if (m_Arguments->scc == kY410)
            {
                m_pVPPSurfacesIn[i]->Data.U = m_pVPPSurfacesIn[i]->Data.Y;
                m_pVPPSurfacesIn[i]->Data.V = m_pVPPSurfacesIn[i]->Data.Y;
                m_pVPPSurfacesIn[i]->Data.A = m_pVPPSurfacesIn[i]->Data.Y;
                m_pVPPSurfacesIn[i]->Data.Pitch = 4*width;
            }
            else /* IF RGB4/AYUV case */
            {
                m_pVPPSurfacesIn[i]->Data.B = m_pVPPSurfacesIn[i]->Data.Y;
                m_pVPPSurfacesIn[i]->Data.G = m_pVPPSurfacesIn[i]->Data.B + 1;
                m_pVPPSurfacesIn[i]->Data.R = m_pVPPSurfacesIn[i]->Data.B + 2;
                m_pVPPSurfacesIn[i]->Data.A = m_pVPPSurfacesIn[i]->Data.B + 3;
                m_pVPPSurfacesIn[i]->Data.Pitch = 4*width;
            }
        }


        // Allocate surfaces for VPP: Out
        mfxU16 width  = (mfxU16)MSDK_ALIGN32(m_VPPParams.vpp.Out.Width);
        mfxU16 height = (mfxU16)MSDK_ALIGN32(m_VPPParams.vpp.Out.Height);
        mfxU32 surfaceSize = width * height * bitsPerPixel / 8;
        m_surfaceBuffersOut = (mfxU8 *)new mfxU8[surfaceSize * m_nVPPSurfNumOut];

        m_pVPPSurfacesOut = new mfxFrameSurface1*[m_nVPPSurfNumOut];
        MSDK_CHECK_POINTER(m_pVPPSurfacesOut, MFX_ERR_MEMORY_ALLOC);
        for (int i = 0; i < m_nVPPSurfNumOut; i++)
        {
            m_pVPPSurfacesOut[i] = new mfxFrameSurface1;
            memset(m_pVPPSurfacesOut[i], 0, sizeof(mfxFrameSurface1));
            memcpy(&(m_pVPPSurfacesOut[i]->Info), &(m_VPPParams.vpp.Out), sizeof(mfxFrameInfo));
            m_pVPPSurfacesOut[i]->Data.Y = &m_surfaceBuffersOut[surfaceSize * i];
            if (m_Arguments->dcc == kNV12)
            {
                m_pVPPSurfacesOut[i]->Data.U = m_pVPPSurfacesOut[i]->Data.Y + width * height;
                m_pVPPSurfacesOut[i]->Data.V = m_pVPPSurfacesOut[i]->Data.U + 1;
                m_pVPPSurfacesOut[i]->Data.Pitch = width;
            }
            else if (m_Arguments->dcc == kY210)
            {
                m_pVPPSurfacesOut[i]->Data.U = m_pVPPSurfacesOut[i]->Data.Y + 2;
                m_pVPPSurfacesOut[i]->Data.V = m_pVPPSurfacesOut[i]->Data.Y + 6;
                m_pVPPSurfacesOut[i]->Data.Pitch = 4*width;
            }
            else if (m_Arguments->dcc == kY410)
            {
                m_pVPPSurfacesOut[i]->Data.U = m_pVPPSurfacesOut[i]->Data.Y;
                m_pVPPSurfacesOut[i]->Data.V = m_pVPPSurfacesOut[i]->Data.Y;
                m_pVPPSurfacesOut[i]->Data.A = m_pVPPSurfacesOut[i]->Data.Y;
                m_pVPPSurfacesOut[i]->Data.Pitch = 4*width;
            }
            else /* IF RGB4/AYUV case */
            {
                m_pVPPSurfacesOut[i]->Data.B = m_pVPPSurfacesOut[i]->Data.Y;
                m_pVPPSurfacesOut[i]->Data.G = m_pVPPSurfacesOut[i]->Data.B + 1;
                m_pVPPSurfacesOut[i]->Data.R = m_pVPPSurfacesOut[i]->Data.B + 2;
                m_pVPPSurfacesOut[i]->Data.A = m_pVPPSurfacesOut[i]->Data.B + 3;
                m_pVPPSurfacesOut[i]->Data.Pitch = 4*width;
            }
        }
        return MFX_ERR_NONE;
    }

protected:

    mfxStatus ReleaseSurfaces()
    {
        if (m_pVPPSurfacesIn)
        {
            for (int i = 0; i < m_nVPPSurfNumIn; i++)
            {
                if (m_pVPPSurfacesIn[i]->Info.FourCC == MFX_FOURCC_NV12
                 || m_pVPPSurfacesIn[i]->Info.FourCC == MFX_FOURCC_YV12
                 || m_pVPPSurfacesIn[i]->Info.FourCC == MFX_FOURCC_Y210
                 || m_pVPPSurfacesIn[i]->Info.FourCC == MFX_FOURCC_Y410)
                {
                    MSDK_SAFE_DELETE_ARRAY(m_pVPPSurfacesIn[i]->Data.Y);
                }
                else
                {
                    MSDK_SAFE_DELETE_ARRAY(m_pVPPSurfacesIn[i]->Data.B);
                }

                MSDK_SAFE_DELETE(m_pVPPSurfacesIn[i]);
            }

            MSDK_SAFE_DELETE_ARRAY(m_pVPPSurfacesIn);
        }


        for (int i = 0; i < m_nVPPSurfNumOut; i++)
        {
            MSDK_SAFE_DELETE(m_pVPPSurfacesOut[i]);
        }

        MSDK_SAFE_DELETE_ARRAY(m_pVPPSurfacesOut);
        MSDK_SAFE_DELETE_ARRAY(m_surfaceBuffersOut);

        return MFX_ERR_NONE;
    }

    mfxStatus FillCompositionParams()
    {
        m_VPPComp.Header.BufferId = MFX_EXTBUFF_VPP_COMPOSITE;
        m_VPPComp.Header.BufferSz = sizeof(mfxExtVPPComposite);
        m_VPPComp.NumInputStream  = (mfxU16) m_nStreams;

        // TODO: eliminate mem leak
        m_VPPComp.InputStream     = (mfxVPPCompInputStream *)malloc( sizeof(mfxVPPCompInputStream)* m_VPPComp.NumInputStream);

        // stream params
        /* if input streams in NV12 format background color should be in YUV format too
         * The same for RGB4 input, background color should be in ARGB format
         * */
        m_VPPComp.Y = m_Color.Y;
        m_VPPComp.U = m_Color.U;
        m_VPPComp.V = m_Color.V;

        for (int i = 0; i < m_nStreams; ++i)
        {
            m_VPPComp.InputStream[i].DstX = atoi(m_Params[i]["dstx"].c_str());
            m_VPPComp.InputStream[i].DstY = atoi(m_Params[i]["dsty"].c_str());
            m_VPPComp.InputStream[i].DstW = atoi(m_Params[i]["dstw"].c_str());
            m_VPPComp.InputStream[i].DstH = atoi(m_Params[i]["dsth"].c_str());
            m_VPPComp.InputStream[i].GlobalAlpha         = (mfxU16)atoi(m_Params[i]["GlobalAlpha"].c_str()) ;
            m_VPPComp.InputStream[i].GlobalAlphaEnable   = (mfxU16)atoi(m_Params[i]["GlobalAlphaEnable"].c_str());
            m_VPPComp.InputStream[i].PixelAlphaEnable    = (mfxU16)atoi(m_Params[i]["PixelAlphaEnable"].c_str()) ;
            m_VPPComp.InputStream[i].LumaKeyEnable       = (mfxU16)atoi(m_Params[i]["LumaKeyEnable"].c_str()) ;
            m_VPPComp.InputStream[i].LumaKeyMin          = (mfxU16)atoi(m_Params[i]["LumaKeyMin"].c_str()) ;
            m_VPPComp.InputStream[i].LumaKeyMax          = (mfxU16)atoi(m_Params[i]["LumaKeyMax"].c_str()) ;
#if MFX_VERSION > 1023
            m_VPPComp.InputStream[i].TileId         = (mfxU16)atoi(m_Params[i]["TileId"].c_str()) ;
#endif
            cout << "Set " << i << "->" << m_VPPComp.InputStream[i].DstX << ":" << m_VPPComp.InputStream[i].DstY << ":" << m_VPPComp.InputStream[i].DstW  << ":" << m_VPPComp.InputStream[i].DstH << ":" << endl;
            cout << " Alpha Blending Params:" << endl;
            cout << "  GlobalAlphaEnable="    << m_VPPComp.InputStream[i].GlobalAlphaEnable << endl;
            cout << "  GlobalAlpha="          << m_VPPComp.InputStream[i].GlobalAlpha << endl;
            cout << "  PixelAlphaEnable="     << m_VPPComp.InputStream[i].PixelAlphaEnable << endl;
            cout << "  LumaKeyEnable="        << m_VPPComp.InputStream[i].LumaKeyEnable << endl;
            cout << "  LumaKeyMin="           << m_VPPComp.InputStream[i].LumaKeyMin << endl;
            cout << "  LumaKeyMax="           << m_VPPComp.InputStream[i].LumaKeyMax << endl;
#if MFX_VERSION > 1023
            cout << "  TileId="           << m_VPPComp.InputStream[i].TileId << endl;
#endif
        }
        return MFX_ERR_NONE;
    }

    mfxStatus FillVPPParams()
    {
        // Initialize VPP parameters
        // - For simplistic memory management, system memory surfaces are used to store the raw frames
        //   (Note that when using HW acceleration D3D surfaces are prefered, for better performance)
        memset(&m_VPPParams, 0, sizeof(m_VPPParams));
        // Input data (primary stream)
        if (m_Arguments->scc == kNV12)
        {
            m_VPPParams.vpp.In.FourCC         = MFX_FOURCC_NV12;
            m_VPPParams.vpp.In.ChromaFormat   = MFX_CHROMAFORMAT_YUV420;
        }
        else if (m_Arguments->scc == kYV12)
        {
            m_VPPParams.vpp.In.FourCC         = MFX_FOURCC_YV12;
            m_VPPParams.vpp.In.ChromaFormat   = MFX_CHROMAFORMAT_YUV420;
        }
        else if (m_Arguments->scc == kAYUV)
        {
            m_VPPParams.vpp.In.FourCC         = MFX_FOURCC_AYUV;
            m_VPPParams.vpp.In.ChromaFormat   = MFX_CHROMAFORMAT_YUV444;
        }
        else if (m_Arguments->scc == kY210)
        {
            m_VPPParams.vpp.In.FourCC         = MFX_FOURCC_Y210;
            m_VPPParams.vpp.In.ChromaFormat   = MFX_CHROMAFORMAT_YUV422;
            m_VPPParams.vpp.In.BitDepthLuma   = 10;
            m_VPPParams.vpp.In.BitDepthChroma = 10;
        }
        else if (m_Arguments->scc == kY410)
        {
            m_VPPParams.vpp.In.FourCC         = MFX_FOURCC_Y410;
            m_VPPParams.vpp.In.ChromaFormat   = MFX_CHROMAFORMAT_YUV444;
            m_VPPParams.vpp.In.BitDepthLuma   = 10;
            m_VPPParams.vpp.In.BitDepthChroma = 10;
        }
        else
        {
            m_VPPParams.vpp.In.FourCC         = MFX_FOURCC_RGB4;
        }

        m_VPPParams.vpp.In.CropX          = 0;
        m_VPPParams.vpp.In.CropY          = 0;
        /* Does not do source cropping... */
        m_VPPParams.vpp.In.CropW          = (mfxU16) m_Arguments->maxWidth;
        m_VPPParams.vpp.In.CropH          = (mfxU16) m_Arguments->maxHeight;
        m_VPPParams.vpp.In.PicStruct      = MFX_PICSTRUCT_PROGRESSIVE;
        m_VPPParams.vpp.In.FrameRateExtN  = 30;
        m_VPPParams.vpp.In.FrameRateExtD  = 1;
        // width must be a multiple of 16
        // height must be a multiple of 16 in case of frame picture and a multiple of 32 in case of field picture
        // Resolution of surface
        m_VPPParams.vpp.In.Width          = (mfxU16) m_Arguments->maxWidth;
        m_VPPParams.vpp.In.Height         = (mfxU16) m_Arguments->maxHeight;

        // Output data
        if (m_Arguments->dcc == kNV12)
        {
            m_VPPParams.vpp.Out.FourCC         = MFX_FOURCC_NV12;
            m_VPPParams.vpp.Out.ChromaFormat   = MFX_CHROMAFORMAT_YUV420;
        }
        else if (m_Arguments->dcc == kAYUV)
        {
            m_VPPParams.vpp.Out.FourCC         = MFX_FOURCC_AYUV;
            m_VPPParams.vpp.Out.ChromaFormat   = MFX_CHROMAFORMAT_YUV444;
        }
        else if (m_Arguments->scc == kY210)
        {
            m_VPPParams.vpp.Out.FourCC         = MFX_FOURCC_Y210;
            m_VPPParams.vpp.Out.ChromaFormat   = MFX_CHROMAFORMAT_YUV422;
            m_VPPParams.vpp.Out.BitDepthLuma   = 10;
            m_VPPParams.vpp.Out.BitDepthChroma = 10;
        }
        else if (m_Arguments->scc == kY410)
        {
            m_VPPParams.vpp.Out.FourCC         = MFX_FOURCC_Y410;
            m_VPPParams.vpp.Out.ChromaFormat   = MFX_CHROMAFORMAT_YUV444;
            m_VPPParams.vpp.Out.BitDepthLuma   = 10;
            m_VPPParams.vpp.Out.BitDepthChroma = 10;
        }
        else /*RGB case*/
        {
            m_VPPParams.vpp.Out.FourCC         = MFX_FOURCC_RGB4;
        }
        m_VPPParams.vpp.Out.CropX         = 0;
        m_VPPParams.vpp.Out.CropY         = 0;
        m_VPPParams.vpp.Out.CropW         = (mfxU16) m_Arguments->maxWidth;
        m_VPPParams.vpp.Out.CropH         = (mfxU16) m_Arguments->maxHeight;
        m_VPPParams.vpp.Out.PicStruct     = MFX_PICSTRUCT_PROGRESSIVE;
        m_VPPParams.vpp.Out.FrameRateExtN = 30;
        m_VPPParams.vpp.Out.FrameRateExtD = 1;
        // width must be a multiple of 16
        // height must be a multiple of 16 in case of frame picture and a multiple of 32 in case of field picture
        if (m_Arguments->dW != 0)
        {
            m_VPPParams.vpp.Out.CropW = (mfxU16) m_Arguments->dW;
            m_VPPParams.vpp.Out.Width = (mfxU16) MSDK_ALIGN16(m_Arguments->dW);
        }
        else
        {
            cout << "Warning: Destination Width is not specified!"  << endl;
            cout << "Destination Width will be set to maximal width :" << m_Arguments->maxWidth << endl;
            m_Arguments->dW = m_Arguments->maxWidth;
            m_VPPParams.vpp.Out.Width = (mfxU16) m_Arguments->dW;
        }
        if (m_Arguments->dH != 0)
        {
            m_VPPParams.vpp.Out.CropH  = (mfxU16) m_Arguments->dH;
            m_VPPParams.vpp.Out.Height = (mfxU16) MSDK_ALIGN16(m_Arguments->dH);
        }
        else
        {
            cout << "Warning: Destination Height is not specified!"  << endl;
            cout << "Destination Height will be set to maximal height :" << m_Arguments->maxHeight << endl;
            m_Arguments->dH = m_Arguments->maxHeight;
            m_VPPParams.vpp.Out.Height = (mfxU16) m_Arguments->dH;
        }

        m_VPPParams.IOPattern = MFX_IOPATTERN_IN_SYSTEM_MEMORY | MFX_IOPATTERN_OUT_SYSTEM_MEMORY;
        m_VPPParams.AsyncDepth = 1;
        return MFX_ERR_NONE;
    }

    mfxStatus ParseParFile(string par_file)
    {
        string    line;
        ifstream par(par_file);

        while (getline(par, line)) {
            istringstream iss(line);
            string key, value;
            getline(iss, key,   '=');
            getline(iss, value, '=');
            trim(key);
            trim(value);

            if ( key.compare("stream") == 0 || key.compare("primarystream") == 0){
                if (value.find("background") != string::npos)
                {
#if defined(WIN32) || defined(WIN64)
                    if (!sscanf_s(value.c_str(), "background(%hu,%hu,%hu)", &m_Color.Y, &m_Color.U, &m_Color.V))
#else
                    if (!sscanf(value.c_str(), "background(%hu,%hu,%hu)", &m_Color.Y, &m_Color.U, &m_Color.V))
#endif
                    {
                        m_Color.Y = 16;
                        m_Color.U = 128;
                        m_Color.V = 128;
                    }
                }
                else
                {
                    map<string, string> params = ParsePar(par, value);
                    m_Params[m_nStreams] = params;
                    if (m_Arguments->scc == kNV12)
                    {
                        m_Streams[m_nStreams].FourCC         = MFX_FOURCC_NV12;
                        m_Streams[m_nStreams].ChromaFormat   = MFX_CHROMAFORMAT_YUV420;
                    }
                    else if (m_Arguments->scc == kYV12)
                    {
                        m_Streams[m_nStreams].FourCC         = MFX_FOURCC_YV12;
                        m_Streams[m_nStreams].ChromaFormat   = MFX_CHROMAFORMAT_YUV420;
                    }
                    else if (m_Arguments->scc == kAYUV)
                    {
                        m_Streams[m_nStreams].FourCC         = MFX_FOURCC_AYUV;
                        m_Streams[m_nStreams].ChromaFormat   = MFX_CHROMAFORMAT_YUV444;
                    }
                    else if (m_Arguments->scc == kY210)
                    {
                        m_Streams[m_nStreams].FourCC         = MFX_FOURCC_Y210;
                        m_Streams[m_nStreams].ChromaFormat   = MFX_CHROMAFORMAT_YUV422;
                        m_Streams[m_nStreams].BitDepthLuma   = 10;
                        m_Streams[m_nStreams].BitDepthChroma = 10;
                    }
                    else if (m_Arguments->scc == kY410)
                    {
                        m_Streams[m_nStreams].FourCC         = MFX_FOURCC_Y410;
                        m_Streams[m_nStreams].ChromaFormat   = MFX_CHROMAFORMAT_YUV444;
                        m_Streams[m_nStreams].BitDepthLuma   = 10;
                        m_Streams[m_nStreams].BitDepthChroma = 10;
                    }
                    else
                    {
                        m_Streams[m_nStreams].FourCC         = MFX_FOURCC_RGB4;
                    }

                    if (m_nStreams == 0)
                        m_Arguments->frames_to_process = atoi(params["frames"].c_str());

                    m_Streams[m_nStreams].CropX          = (mfxU16) atoi(params["cropx"].c_str());
                    m_Streams[m_nStreams].CropY          = (mfxU16) atoi(params["cropy"].c_str());
                    m_Streams[m_nStreams].CropW          = (mfxU16) atoi(params["cropw"].c_str());
                    m_Streams[m_nStreams].CropH          = (mfxU16) atoi(params["croph"].c_str());
                    m_Streams[m_nStreams].PicStruct      = MFX_PICSTRUCT_PROGRESSIVE;
                    m_Streams[m_nStreams].FrameRateExtN  = 30;
                    m_Streams[m_nStreams].FrameRateExtD  = 1;
                    // width must be a multiple of 16
                    // height must be a multiple of 16 in case of frame picture and a multiple of 32 in case of field picture
                    m_Streams[m_nStreams].Width  = (mfxU16) MSDK_ALIGN16(atoi(params["width"].c_str()));
                    m_Streams[m_nStreams].Height = (MFX_PICSTRUCT_PROGRESSIVE == m_Streams[m_nStreams].PicStruct)?
                                         (mfxU16) MSDK_ALIGN16(atoi(params["height"].c_str())) :
                                         (mfxU16) MSDK_ALIGN32(atoi(params["height"].c_str()));

                    // We need to save real sizes for correct reading from input files
                    m_RealWidths[m_nStreams]  = (mfxU16) atoi(params["width"].c_str());
                    m_RealHeights[m_nStreams] = (mfxU16) atoi(params["height"].c_str());

                    /* Update maximal Width & Height */
                    if (m_Arguments->maxWidth < m_Streams[m_nStreams].Width)
                        m_Arguments->maxWidth = m_Streams[m_nStreams].Width;
                    if (m_Arguments->maxHeight < m_Streams[m_nStreams].Height)
                        m_Arguments->maxHeight = m_Streams[m_nStreams].Height;

                    ++m_nStreams;
                    if ( m_nStreams > MAX_INPUT_STREAMS )
                    {
                        return MFX_ERR_UNKNOWN;
                    }
                }
            }
        }

        return OpenFiles();
    }

    mfxStatus OpenFiles()
    {
        for (int i = 0; i < m_nStreams; i++)
        {
            cout << "Open file " << m_Params[i]["stream"] << endl;
            fopen_s(&m_Files[i], m_Params[i]["stream"].c_str(), "rb");
            MSDK_CHECK_POINTER(m_Files[i], MFX_ERR_NULL_PTR);
        }
        return MFX_ERR_NONE;
    }

private:
    mfxU16              m_nStreams;
    map<string, string> m_Params[MAX_INPUT_STREAMS];
    FILE               *m_Files[MAX_INPUT_STREAMS];
    mfxFrameInfo        m_Streams[MAX_INPUT_STREAMS];

    ProgramArguments    *m_Arguments;
    MFXVideoVPP         *m_VPP;

    bool                 m_bInited;
    mfxVideoParam        m_VPPParams;
    mfxExtVPPComposite   m_VPPComp;
    mfxFrameAllocRequest m_VPPRequest[2];
    mfxFrameSurface1  ** m_pVPPSurfacesIn;
    mfxFrameSurface1  ** m_pVPPSurfacesOut;
    mfxU16               m_nVPPSurfNumIn;
    mfxU16               m_nVPPSurfNumOut;
    mfxU8              * m_surfaceBuffersOut;
    mfxExtBuffer       * m_ExtBuffer[1];
    mfxU16               m_RealWidths[MAX_INPUT_STREAMS];
    mfxU16               m_RealHeights[MAX_INPUT_STREAMS];
    Background           m_Color;
};

#if !defined(_WIN32) && !defined(_WIN64)
#define MFX_PCI_DIR "/sys/bus/pci/devices"
#define MFX_DRI_DIR "/dev/dri/"
#define MFX_PCI_DISPLAY_CONTROLLER_CLASS 0x03

struct mfx_disp_adapters
{
    mfxU32 vendor_id;
    mfxU32 device_id;
};

static int mfx_dir_filter(const struct dirent* dir_ent)
{
    if (!dir_ent) return 0;
    if (!strcmp(dir_ent->d_name, ".")) return 0;
    if (!strcmp(dir_ent->d_name, "..")) return 0;
    return 1;
}

typedef int (*fsort)(const struct dirent**, const struct dirent**);

static mfxU32 mfx_init_adapters(struct mfx_disp_adapters** p_adapters)
{
    mfxU32 adapters_num = 0;
    int i = 0;
    struct mfx_disp_adapters* adapters = NULL;
    struct dirent** dir_entries = NULL;
    int entries_num = scandir(MFX_PCI_DIR, &dir_entries, mfx_dir_filter, (fsort)alphasort);

    char file_name[300] = {};
    char str[16] = {0};
    FILE* file = NULL;

    for (i = 0; i < entries_num; ++i)
    {
        long int class_id = 0, vendor_id = 0, device_id = 0;

        if (!dir_entries[i])
            continue;

        // obtaining device class id
        snprintf(file_name, sizeof(file_name)/sizeof(file_name[0]), "%s/%s/%s", MFX_PCI_DIR, dir_entries[i]->d_name, "class");
        file = fopen(file_name, "r");
        if (file)
        {
            if (fgets(str, sizeof(str), file))
            {
                class_id = strtol(str, NULL, 16);
            }
            fclose(file);

            if (MFX_PCI_DISPLAY_CONTROLLER_CLASS == (class_id >> 16))
            {
                // obtaining device vendor id
                snprintf(file_name, sizeof(file_name)/sizeof(file_name[0]), "%s/%s/%s", MFX_PCI_DIR, dir_entries[i]->d_name, "vendor");
                file = fopen(file_name, "r");
                if (file)
                {
                    if (fgets(str, sizeof(str), file))
                    {
                        vendor_id = strtol(str, NULL, 16);
                    }
                    fclose(file);
                }
                // obtaining device id
                snprintf(file_name, sizeof(file_name)/sizeof(file_name[0]), "%s/%s/%s", MFX_PCI_DIR, dir_entries[i]->d_name, "device");
                file = fopen(file_name, "r");
                if (file)
                {
                    if (fgets(str, sizeof(str), file))
                    {
                        device_id = strtol(str, NULL, 16);
                    }
                    fclose(file);
                }
                // adding valid adapter to the list
                if (vendor_id && device_id)
                {
                    struct mfx_disp_adapters* tmp_adapters = NULL;

                    tmp_adapters = (mfx_disp_adapters*)realloc(adapters,
                                                               (adapters_num+1)*sizeof(struct mfx_disp_adapters));

                    if (tmp_adapters)
                    {
                        adapters = tmp_adapters;
                        adapters[adapters_num].vendor_id = vendor_id;
                        adapters[adapters_num].device_id = device_id;

                        ++adapters_num;
                    }
                }
            }
        }
        free(dir_entries[i]);
    }
    if (entries_num) free(dir_entries);
    if (p_adapters) *p_adapters = adapters;

    return adapters_num;
}

mfxStatus va_to_mfx_status(VAStatus va_res)
{
    mfxStatus mfxRes = MFX_ERR_NONE;

    switch (va_res)
    {
    case VA_STATUS_SUCCESS:
        mfxRes = MFX_ERR_NONE;
        break;
    case VA_STATUS_ERROR_ALLOCATION_FAILED:
        mfxRes = MFX_ERR_MEMORY_ALLOC;
        break;
    case VA_STATUS_ERROR_ATTR_NOT_SUPPORTED:
    case VA_STATUS_ERROR_UNSUPPORTED_PROFILE:
    case VA_STATUS_ERROR_UNSUPPORTED_ENTRYPOINT:
    case VA_STATUS_ERROR_UNSUPPORTED_RT_FORMAT:
    case VA_STATUS_ERROR_UNSUPPORTED_BUFFERTYPE:
    case VA_STATUS_ERROR_FLAG_NOT_SUPPORTED:
    case VA_STATUS_ERROR_RESOLUTION_NOT_SUPPORTED:
        mfxRes = MFX_ERR_UNSUPPORTED;
        break;
    case VA_STATUS_ERROR_INVALID_DISPLAY:
    case VA_STATUS_ERROR_INVALID_CONFIG:
    case VA_STATUS_ERROR_INVALID_CONTEXT:
    case VA_STATUS_ERROR_INVALID_SURFACE:
    case VA_STATUS_ERROR_INVALID_BUFFER:
    case VA_STATUS_ERROR_INVALID_IMAGE:
    case VA_STATUS_ERROR_INVALID_SUBPICTURE:
        mfxRes = MFX_ERR_NOT_INITIALIZED;
        break;
    case VA_STATUS_ERROR_INVALID_PARAMETER:
        mfxRes = MFX_ERR_INVALID_VIDEO_PARAM;
    default:
        mfxRes = MFX_ERR_UNKNOWN;
        break;
    }
    return mfxRes;
}
#endif

int main(int argc, char *argv[])
{
#if !defined(_WIN32) && !defined(_WIN64)
    std::auto_ptr<MfxLoader::VA_Proxy> m_libva;
    std::auto_ptr<MfxLoader::VA_DRMProxy> m_vadrmlib;

    int m_card_fd = 0, major_version = 0, minor_version = 0;
    VADisplay m_va_display = NULL;
    VAStatus va_res;

    const mfxU32 IntelVendorID = 0x8086;
    //the first Intel adapter is only required now, the second - in the future
    const mfxU32 numberOfRequiredIntelAdapter = 1;
    const char nodesNames[][8] = {"renderD", "card"};
#endif

    mfxStatus sts = MFX_ERR_NONE;
    ProgramArguments pa;

    auto args = ConvertInputString(argc, argv);

    try
    {
        pa = ParseInputString(args);
    }


    catch (const std::exception &e)
    {
        cout << "Troubles: " << e.what() << endl;
        PrintHelp(cout);
    }

#if !defined(_WIN32) && !defined(_WIN64)
    if(!isActivatedSW)
    {
        m_libva.reset(new MfxLoader::VA_Proxy);
        m_vadrmlib.reset(new MfxLoader::VA_DRMProxy);
    }

    mfx_disp_adapters* adapters = NULL;
    int adapters_num = mfx_init_adapters(&adapters);

    // Search for the required display adapter
    int i = 0, nFoundAdapters = 0;
    int nodesNumbers[] = {0,0};
    
    while ((i < adapters_num) && (nFoundAdapters != numberOfRequiredIntelAdapter))
    {
        if (adapters[i].vendor_id == IntelVendorID)
        {
            nFoundAdapters++;
            nodesNumbers[0] = i+128; //for render nodes
            nodesNumbers[1] = i;     //for card
        }
        i++;
    }
    if (adapters_num) free(adapters);
    // If Intel adapter with specified number wasn't found, throws exception
    if (nFoundAdapters != numberOfRequiredIntelAdapter)
        throw std::range_error("The Intel adapter with a specified number wasn't found");

    // Initialization of paths to the device nodes
    char** adapterPaths = new char* [2];
    for (int i=0; i<2; i++)
    {
        /*if ((i == 0) && (type == MFX_LIBVA_DRM_MODESET)) {
          adapterPaths[i] = NULL;
          continue;
        }*/
        adapterPaths[i] = new char[sizeof(MFX_DRI_DIR) + sizeof(nodesNames[i]) + 3];
        sprintf(adapterPaths[i], "%s%s%d", MFX_DRI_DIR, nodesNames[i], nodesNumbers[i]);
    }
#endif

    Composition composition(&pa);

    // Create output NV12 file
    FILE* fSink;
    fopen_s(&fSink, pa.output.c_str(), "wb");
    MSDK_CHECK_POINTER(fSink, MFX_ERR_NULL_PTR);

    // Initialize Media SDK session
    // - MFX_IMPL_AUTO_ANY selects HW accelaration if available (on any adapter)
    // - Version 1.0 is selected for greatest backwards compatibility.
    //   If more recent API features are needed, change the version accordingly
    mfxVersion ver;
    ver.Major = 1;
    ver.Minor = 8;
    MFXVideoSession mfxSession;
    sts = mfxSession.Init(pa.impl, &ver);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    /*For Linux, it is required to set libVA display handle
     * (see sample_common for more details*/
#if !(defined(_WIN32) || defined(_WIN64))
    if (!isActivatedSW)
    {
        // Loading display. At first trying to open render nodes, then card.
        for (int i=0; i<2; i++)
        {
            if (!adapterPaths[i]) {
              sts = MFX_ERR_UNSUPPORTED;
              continue;
            }
            sts = MFX_ERR_NONE;
            m_card_fd = open(adapterPaths[i], O_RDWR);

            if (m_card_fd < 0)
            {
                sts = MFX_ERR_NOT_INITIALIZED;
                return sts;
            }

            if (MFX_ERR_NONE == sts)
            {
                m_va_display = m_vadrmlib->vaGetDisplayDRM(m_card_fd);

                if (!m_va_display)
                {
                    close(m_card_fd);
                    m_card_fd = -1;
                    sts = MFX_ERR_NULL_PTR;
                    return sts;
                }
            }

            if (MFX_ERR_NONE == sts)
            {
                va_res = m_libva->vaInitialize(m_va_display, &major_version, &minor_version);
                sts = va_to_mfx_status(va_res);
                if (MFX_ERR_NONE != sts)
                {
                    close(m_card_fd);
                    m_card_fd = -1;
                    sts = MFX_ERR_NOT_INITIALIZED;
                    return sts;
                }

            }

            if (MFX_ERR_NONE == sts) break;
        }

        for (int i=0; i<2; i++)
        {
            delete [] adapterPaths[i];
        }
        delete [] adapterPaths;

        if (MFX_ERR_NONE != sts)
            throw std::bad_alloc();

         sts = mfxSession.SetHandle(MFX_HANDLE_VA_DISPLAY, m_va_display);
         MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    }
#endif

    // Create Media SDK VPP component
    MFXVideoVPP* mfxVPP = new MFXVideoVPP(mfxSession);
    composition.SetVPP(mfxVPP);

    composition.Init(pa.par);

    // Initialize Media SDK VPP
    sts = mfxVPP->Init(composition.GetVPPParams());
    MSDK_IGNORE_MFX_STS(sts, MFX_WRN_PARTIAL_ACCELERATION);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    // Query number of required surfaces for VPP
    sts = composition.RequestSurfaces();
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    mfxFrameSurface1  ** pVPPSurfacesIn;
    mfxFrameSurface1  ** pVPPSurfacesOut;
    mfxU16               nVPPSurfNumIn;
    mfxU16               nVPPSurfNumOut;
    mfxU16               StreamCount;
    composition.GetInSurfaces(&pVPPSurfacesIn, &nVPPSurfNumIn);
    composition.GetOutSurfaces(&pVPPSurfacesOut, &nVPPSurfNumOut);
    StreamCount = composition.GetStreamCount();

    // ===================================
    // Start processing the frames
    //

#ifdef ENABLE_BENCHMARK
    LARGE_INTEGER tStart, tEnd;
    QueryPerformanceFrequency(&tStart);
    double freq = (double)tStart.QuadPart;
    QueryPerformanceCounter(&tStart);
#endif

    mfxU16 nSurfIdxIn = 0, nSurfIdxOut = 0;
    mfxSyncPoint syncp;
    mfxU32 nFrame = 0, nSource = 0;
    bool bMultipleOut = false;

    //
    // Stage 1: Main processing loop
    //
    while (MFX_ERR_NONE <= sts || MFX_ERR_MORE_DATA == sts || bMultipleOut )
    {
        if (!bMultipleOut)
        {
            nSurfIdxIn = (mfxU16)GetFreeSurfaceIndex(pVPPSurfacesIn, nVPPSurfNumIn); // Find free input frame surface
            if (MFX_ERR_NOT_FOUND == nSurfIdxIn)
                return MFX_ERR_MEMORY_ALLOC;

            sts = LoadRawFrame(pVPPSurfacesIn[nSurfIdxIn], composition.GetFileHandle(nSource++), composition.GetRealWidth(nSurfIdxIn), composition.GetRealHeight(nSurfIdxIn)); // Load frame from file into surface
            MSDK_BREAK_ON_ERROR(sts);
            if (nSource >= (mfxU32)StreamCount)
                nSource = 0;
        }

        bMultipleOut = false;

        nSurfIdxOut = (mfxU16)GetFreeSurfaceIndex(pVPPSurfacesOut, nVPPSurfNumOut); // Find free output frame surface
        if (MFX_ERR_NOT_FOUND == nSurfIdxOut)
            return MFX_ERR_MEMORY_ALLOC;

        // Process a frame asychronously (returns immediately)
        sts = mfxVPP->RunFrameVPPAsync(pVPPSurfacesIn[nSurfIdxIn], pVPPSurfacesOut[nSurfIdxOut], NULL, &syncp);
        if (MFX_ERR_MORE_DATA == sts)
            continue;

        //MFX_ERR_MORE_SURFACE means output is ready but need more surface
        //because VPP produce multiple out. example: Frame Rate Conversion 30->60
        if (MFX_ERR_MORE_SURFACE == sts)
        {
            sts = MFX_ERR_NONE;
            bMultipleOut = true;
        }

        MSDK_BREAK_ON_ERROR(sts);

        sts = mfxSession.SyncOperation(syncp, 60000); // Synchronize. Wait until frame processing is ready
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

        ++nFrame;

        sts = WriteRawFrame(pVPPSurfacesOut[nSurfIdxOut], fSink);
        MSDK_BREAK_ON_ERROR(sts);

        printf("Frame number: %d\r", nFrame);

        if (pa.need_reset && nFrame >= pa.restart_frame)
            break;

        if (nFrame >= pa.frames_to_process)
            break;
    }

    // MFX_ERR_MORE_DATA means that the input file has ended, need to go to buffering loop, exit in case of other errors
    MSDK_IGNORE_MFX_STS(sts, MFX_ERR_MORE_DATA);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    //
    // Stage 2: Retrieve the buffered VPP frames
    //
    while (MFX_ERR_NONE <= sts)
    {
        nSurfIdxOut = (mfxU16)GetFreeSurfaceIndex(pVPPSurfacesOut, nVPPSurfNumOut); // Find free frame surface
        if (MFX_ERR_NOT_FOUND == nSurfIdxOut)
            return MFX_ERR_MEMORY_ALLOC;

        // Process a frame asychronously (returns immediately)
        sts = mfxVPP->RunFrameVPPAsync(NULL, pVPPSurfacesOut[nSurfIdxOut], NULL, &syncp);
        MSDK_IGNORE_MFX_STS(sts, MFX_ERR_MORE_SURFACE);
        MSDK_BREAK_ON_ERROR(sts);

        sts = mfxSession.SyncOperation(syncp, 60000); // Synchronize. Wait until frame processing is ready
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

        ++nFrame;
        sts = WriteRawFrame(pVPPSurfacesOut[nSurfIdxOut], fSink);
        MSDK_BREAK_ON_ERROR(sts);

        printf("Frame number: %d\r", nFrame);
    }

    printf("\n");
    printf("Frames processed %d\n", nFrame);


    // MFX_ERR_MORE_DATA indicates that there are no more buffered frames, exit in case of other errors
    MSDK_IGNORE_MFX_STS(sts, MFX_ERR_MORE_DATA);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

#ifdef ENABLE_BENCHMARK
    QueryPerformanceCounter(&tEnd);
    double duration = ((double)tEnd.QuadPart - (double)tStart.QuadPart)  / freq;
    printf("\nExecution time: %3.2fs (%3.2ffps)\n", duration, nFrame/duration);
#endif


    if ( pa.need_reset )
    {
        composition.Reset(pa.reset_par);
        sts = mfxVPP->Reset(composition.GetVPPParams());
        MSDK_IGNORE_MFX_STS(sts, MFX_WRN_PARTIAL_ACCELERATION);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

        sts = composition.RequestSurfaces();
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

        composition.GetInSurfaces(&pVPPSurfacesIn, &nVPPSurfNumIn);
        composition.GetOutSurfaces(&pVPPSurfacesOut, &nVPPSurfNumOut);
        StreamCount = composition.GetStreamCount();

        if (nVPPSurfNumIn != StreamCount )
            sts = MFX_ERR_UNSUPPORTED;
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

        /*---------------------------------------------------------------------------------------------*/
        //
        // Stage 1: Main processing loop
        //
        nFrame = 0;

        while (MFX_ERR_NONE <= sts || MFX_ERR_MORE_DATA == sts || bMultipleOut )
        {
            if (!bMultipleOut)
            {
                nSurfIdxIn = (mfxU16)GetFreeSurfaceIndex(pVPPSurfacesIn, nVPPSurfNumIn); // Find free input frame surface
                if (MFX_ERR_NOT_FOUND == nSurfIdxIn)
                    return MFX_ERR_MEMORY_ALLOC;

                sts = LoadRawFrame(pVPPSurfacesIn[nSurfIdxIn], composition.GetFileHandle(nSource++), composition.GetRealWidth(nSurfIdxIn), composition.GetRealHeight(nSurfIdxIn)); // Load frame from file into surface

                    MSDK_BREAK_ON_ERROR(sts);
                if (nSource >= (mfxU32)StreamCount)
                    nSource = 0;
            }

            bMultipleOut = false;

            nSurfIdxOut = (mfxU16)GetFreeSurfaceIndex(pVPPSurfacesOut, nVPPSurfNumOut); // Find free output frame surface
            if (MFX_ERR_NOT_FOUND == nSurfIdxOut)
                return MFX_ERR_MEMORY_ALLOC;

            // Process a frame asychronously (returns immediately)
            sts = mfxVPP->RunFrameVPPAsync(pVPPSurfacesIn[nSurfIdxIn], pVPPSurfacesOut[nSurfIdxOut], NULL, &syncp);
            if (MFX_ERR_MORE_DATA == sts)
                continue;

            //MFX_ERR_MORE_SURFACE means output is ready but need more surface
            //because VPP produce multiple out. example: Frame Rate Conversion 30->60
            if (MFX_ERR_MORE_SURFACE == sts)
            {
                sts = MFX_ERR_NONE;
                bMultipleOut = true;
            }

            MSDK_BREAK_ON_ERROR(sts);

            sts = mfxSession.SyncOperation(syncp, 60000); // Synchronize. Wait until frame processing is ready
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

            ++nFrame;

            sts = WriteRawFrame(pVPPSurfacesOut[nSurfIdxOut], fSink);
            MSDK_BREAK_ON_ERROR(sts);

            printf("Frame number: %d\r", nFrame);
            if (nFrame >= pa.frames_to_process)
            //if (nFrame >= FRAME_NUM_50)
                break;

        }

        // MFX_ERR_MORE_DATA means that the input file has ended, need to go to buffering loop, exit in case of other errors
        MSDK_IGNORE_MFX_STS(sts, MFX_ERR_MORE_DATA);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

        //
        // Stage 2: Retrieve the buffered VPP frames
        //
        while (MFX_ERR_NONE <= sts)
        {
            nSurfIdxOut = (mfxU16)GetFreeSurfaceIndex(pVPPSurfacesOut, nVPPSurfNumOut); // Find free frame surface
            if (MFX_ERR_NOT_FOUND == nSurfIdxOut)
                return MFX_ERR_MEMORY_ALLOC;

            // Process a frame asychronously (returns immediately)
            sts = mfxVPP->RunFrameVPPAsync(NULL, pVPPSurfacesOut[nSurfIdxOut], NULL, &syncp);
            MSDK_IGNORE_MFX_STS(sts, MFX_ERR_MORE_SURFACE);
            MSDK_BREAK_ON_ERROR(sts);

            sts = mfxSession.SyncOperation(syncp, 60000); // Synchronize. Wait until frame processing is ready
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

            ++nFrame;

            sts = WriteRawFrame(pVPPSurfacesOut[nSurfIdxOut], fSink);
            MSDK_BREAK_ON_ERROR(sts);

            printf("Frame number: %d\r", nFrame);

        }

        printf("\n");
        printf("Frames processed %d\n", nFrame);

        // MFX_ERR_MORE_DATA indicates that there are no more buffered frames, exit in case of other errors
        MSDK_IGNORE_MFX_STS(sts, MFX_ERR_MORE_DATA);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

        /*---------------------------------------------------------------------------------------------*/
    }

    // ===================================================================
    // Clean up resources
    //  - It is recommended to close Media SDK components first, before releasing allocated surfaces, since
    //    some surfaces may still be locked by internal Media SDK resources.

    delete mfxVPP;
    // If CM enabled application have to close Media SDK session before vaTerminate() call
    // Else you will have crash at CM's destructor
    mfxSession.Close();
    fclose(fSink);

#if !(defined(_WIN32) || defined(_WIN64))
    if(!isActivatedSW)
    {
        if (m_va_display)
        {
            m_libva->vaTerminate(m_va_display);
        }
        if (m_card_fd >= 0)
        {
            close(m_card_fd);
        }
    }
#endif

    return 0;
}
