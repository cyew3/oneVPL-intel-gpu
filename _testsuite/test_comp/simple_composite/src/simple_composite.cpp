//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2005-2014 Intel Corporation. All Rights Reserved.
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
#include <string>
#include <algorithm>
#include <functional>
#include <cctype>
#include <locale>
#include <stdexcept>


#if ! defined(_WIN32) && ! defined(_WIN64)
  #include <fcntl.h>
  #include <va/va.h>
  #include <va/va_drm.h>
  #include <unistd.h>
#endif


using namespace std;

typedef struct {
    char* pStreamName;
    mfxU16 width;
    mfxU16 height;
} InputSubstream;


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
    kRGB4 = 2
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

    return pa;
}

void PrintHelp(ostream &out)
{
    // Mandatory args
    out << "Mandatory args: " << endl;
    out << "-par <filename> path to the parameters file" << endl;
    out << "-o <filename>" << endl;
    out << "-scc <NV12 | YV12 | RGB4> (default: NV12 - source color format)" << endl;
    out << "-dcc <NV12 | YV12 | RGB4> (default: NV12 - dest color format)" << endl;
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

int main(int argc, char *argv[])
{
    mfxStatus sts = MFX_ERR_NONE;

    /* Again:
     * total number streams for vpp composition is 64 now!
     * This is means 1 primary and 63 sub-streams */

    mfxFrameInfo streams[MAX_INPUT_STREAMS] = {};

    auto args = ConvertInputString(argc, argv);
    ProgramArguments pa;
    map<string, string> streamParams[MAX_INPUT_STREAMS];


    try {
        pa = ParseInputString(args);
    } catch (const std::exception &e){
        cout << "Troubles: " << e.what() << endl;
        PrintHelp(cout);
    }

    int i = 0;
    int StreamCount = 0;
    unsigned int framesToProcess = 0;
    /* We needs this variables for memory VPP Init() and memory allocation */
    mfxU32 maxWidth = 0, maxHeight = 0;

    // Read parameterd from par file
    try {
        ifstream par(pa.par);
        string line;

        while (getline(par, line)) {
            istringstream iss(line);
            string key, value;
            getline(iss, key,   '=');
            getline(iss, value, '=');
            trim(key);
            trim(value);
            if ( key.compare("stream") == 0 || key.compare("primarystream") == 0){
                map<string, string> params = ParsePar(par, value);
                streamParams[StreamCount] = params;
                if (pa.scc == kNV12)
                {
                    streams[StreamCount].FourCC         = MFX_FOURCC_NV12;
                    streams[StreamCount].ChromaFormat   = MFX_CHROMAFORMAT_YUV420;
                }
                else if (pa.scc == kYV12)
                {
                    streams[StreamCount].FourCC         = MFX_FOURCC_YV12;
                    streams[StreamCount].ChromaFormat   = MFX_CHROMAFORMAT_YUV420;
                }
                else
                {
                    streams[StreamCount].FourCC         = MFX_FOURCC_RGB4;
                }

                if (StreamCount == 0)
                    framesToProcess = atoi(params["frames"].c_str());

                streams[StreamCount].CropX          = (mfxU16) atoi(params["cropx"].c_str());
                streams[StreamCount].CropY          = (mfxU16) atoi(params["cropy"].c_str());
                streams[StreamCount].CropW          = (mfxU16) atoi(params["cropw"].c_str());
                streams[StreamCount].CropH          = (mfxU16) atoi(params["croph"].c_str());
                streams[StreamCount].PicStruct      = MFX_PICSTRUCT_PROGRESSIVE;
                streams[StreamCount].FrameRateExtN  = 30;
                streams[StreamCount].FrameRateExtD  = 1;
                // width must be a multiple of 16
                // height must be a multiple of 16 in case of frame picture and a multiple of 32 in case of field picture
                streams[StreamCount].Width  = (mfxU16) MSDK_ALIGN16(atoi(params["width"].c_str()));
                streams[StreamCount].Height = (MFX_PICSTRUCT_PROGRESSIVE == streams[StreamCount].PicStruct)?
                                     (mfxU16) MSDK_ALIGN16(atoi(params["height"].c_str())) :
                                     (mfxU16) MSDK_ALIGN32(atoi(params["height"].c_str()));
                /* Update maximal Width & Height */
                if (maxWidth < streams[StreamCount].Width)
                    maxWidth = streams[StreamCount].Width;
                if (maxHeight < streams[StreamCount].Height)
                    maxHeight = streams[StreamCount].Height;

                ++StreamCount;
                if ( StreamCount > MAX_INPUT_STREAMS )
                {
                    throw std::runtime_error("Maximum number of input streams exceeded.");
                }
            }
        }
    } catch (const std::exception &e){
        cout << "Troubles: " << e.what() << endl;
        PrintHelp(cout);
        return -1;
    }

    FILE* fSource[MAX_INPUT_STREAMS];
    for (int cnt = 0; cnt < StreamCount; cnt++)
    {
        cout << "Open file " << streamParams[cnt]["stream"] << endl;
        fopen_s(&fSource[cnt], streamParams[cnt]["stream"].c_str(), "rb");
        MSDK_CHECK_POINTER(fSource[cnt], MFX_ERR_NULL_PTR);
    }

    // Create output NV12 file
    FILE* fSink;
    fopen_s(&fSink, pa.output.c_str(), "wb");
    MSDK_CHECK_POINTER(fSink, MFX_ERR_NULL_PTR);

    // Initialize Media SDK session
    // - MFX_IMPL_AUTO_ANY selects HW accelaration if available (on any adapter)
    // - Version 1.0 is selected for greatest backwards compatibility.
    //   If more recent API features are needed, change the version accordingly
    mfxIMPL impl = MFX_IMPL_HARDWARE;// | MFX_IMPL_VIA_D3D11;
    mfxVersion ver;
    ver.Major = 1;
    ver.Minor = 8;
    MFXVideoSession mfxSession;
    sts = mfxSession.Init(impl, &ver);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    /*For Linux, it is required to set libVA display handle
     * (see sample_common for more details*/
#if !(defined(_WIN32) || defined(_WIN64))

    int m_card_fd =0, major_version = 0, minor_version = 0;
    VADisplay m_va_display;
    VAStatus va_res;
    m_card_fd = open("/dev/dri/card0", O_RDWR);

    if (m_card_fd < 0)
    {
        sts = MFX_ERR_NOT_INITIALIZED;
        return sts;
    }
    if (MFX_ERR_NONE == sts)
    {
        m_va_display = vaGetDisplayDRM(m_card_fd);
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
        va_res = vaInitialize(m_va_display, &major_version, &minor_version);
        if (VA_STATUS_SUCCESS != va_res)
        {
            close(m_card_fd);
            m_card_fd = -1;
            sts = MFX_ERR_NOT_INITIALIZED;
            return sts;
        }
    }
    sts = mfxSession.SetHandle(MFX_HANDLE_VA_DISPLAY, m_va_display);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
#endif



    // Initialize VPP parameters
    // - For simplistic memory management, system memory surfaces are used to store the raw frames
    //   (Note that when using HW acceleration D3D surfaces are prefered, for better performance)
    mfxVideoParam VPPParams;
    memset(&VPPParams, 0, sizeof(VPPParams));
    // Input data (primary stream)
    if (pa.scc == kNV12)
    {
        VPPParams.vpp.In.FourCC         = MFX_FOURCC_NV12;
        VPPParams.vpp.In.ChromaFormat   = MFX_CHROMAFORMAT_YUV420;
    }
    else if (pa.scc == kYV12)
    {
        VPPParams.vpp.In.FourCC         = MFX_FOURCC_YV12;
        VPPParams.vpp.In.ChromaFormat   = MFX_CHROMAFORMAT_YUV420;
    }
    else
    {
        VPPParams.vpp.In.FourCC         = MFX_FOURCC_RGB4;
    }

    VPPParams.vpp.In.CropX          = 0;
    VPPParams.vpp.In.CropY          = 0;
    /* Does not do source cropping... */
    VPPParams.vpp.In.CropW          = (mfxU16) maxWidth;
    VPPParams.vpp.In.CropH          = (mfxU16) maxHeight;
    VPPParams.vpp.In.PicStruct      = MFX_PICSTRUCT_PROGRESSIVE;
    VPPParams.vpp.In.FrameRateExtN  = 30;
    VPPParams.vpp.In.FrameRateExtD  = 1;
    // width must be a multiple of 16
    // height must be a multiple of 16 in case of frame picture and a multiple of 32 in case of field picture
    // Resolution of surface
    VPPParams.vpp.In.Width  = maxWidth;
    VPPParams.vpp.In.Height = maxHeight;

    // Output data

    if (pa.dcc == kNV12)
    {
        VPPParams.vpp.Out.FourCC         = MFX_FOURCC_NV12;
        VPPParams.vpp.Out.ChromaFormat   = MFX_CHROMAFORMAT_YUV420;
    }
    else /*RGB case*/
    {
        VPPParams.vpp.Out.FourCC         = MFX_FOURCC_RGB4;
    }
    VPPParams.vpp.Out.CropX         = 0;
    VPPParams.vpp.Out.CropY         = 0;
    VPPParams.vpp.Out.CropW         = maxWidth;
    VPPParams.vpp.Out.CropH         = maxHeight;
    VPPParams.vpp.Out.PicStruct     = MFX_PICSTRUCT_PROGRESSIVE;
    VPPParams.vpp.Out.FrameRateExtN = 30;
    VPPParams.vpp.Out.FrameRateExtD = 1;
    // width must be a multiple of 16
    // height must be a multiple of 16 in case of frame picture and a multiple of 32 in case of field picture
    if (pa.dW != 0)
    {
        VPPParams.vpp.Out.Width = pa.dW;
    }
    else
    {
        cout << "Warning: Destination Width is not specified!"  << endl;
        cout << "Destination Width will be set to maximal width :" << maxWidth << endl;
        VPPParams.vpp.Out.Width  = pa.dW = maxWidth;
    }
    if (pa.dH != 0)
    {
        VPPParams.vpp.Out.Height = pa.dH;
    }
    else
    {
        cout << "Warning: Destination Height is not specified!"  << endl;
        cout << "Destination Height will be set to maximal height :" << maxHeight << endl;
        VPPParams.vpp.Out.Height = pa.dH = maxHeight;
    }

    VPPParams.IOPattern = MFX_IOPATTERN_IN_SYSTEM_MEMORY | MFX_IOPATTERN_OUT_SYSTEM_MEMORY;
    VPPParams.AsyncDepth = 1;

    // Create Media SDK VPP component
    MFXVideoVPP mfxVPP(mfxSession);

    // Initialize extended buffer for frame processing
    mfxExtVPPComposite comp;
    comp.Header.BufferId = MFX_EXTBUFF_VPP_COMPOSITE;
    comp.Header.BufferSz = sizeof(mfxExtVPPComposite);
    comp.NumInputStream  = (mfxU16) StreamCount;
    comp.InputStream     = (mfxVPPCompInputStream *)malloc( sizeof(mfxVPPCompInputStream)* comp.NumInputStream);

    // stream params
    /* if input streams in NV12 format background color should be in YUV format too
     * The same for RGB4 input, background color should be in ARGB format
     * */
    /* back color in YUV */
    comp.Y = 0x10;
    comp.U = 0x80;
    comp.V = 0x80;
    for (i = 0; i < StreamCount; ++i)
    {
        comp.InputStream[i].DstX = atoi(streamParams[i]["dstx"].c_str());
        comp.InputStream[i].DstY = atoi(streamParams[i]["dsty"].c_str());
        comp.InputStream[i].DstW = atoi(streamParams[i]["dstw"].c_str());
        comp.InputStream[i].DstH = atoi(streamParams[i]["dsth"].c_str());
        comp.InputStream[i].GlobalAlpha         = atoi(streamParams[i]["GlobalAlpha"].c_str()) ;
        comp.InputStream[i].GlobalAlphaEnable   = atoi(streamParams[i]["GlobalAlphaEnable"].c_str());
        comp.InputStream[i].PixelAlphaEnable    = atoi(streamParams[i]["PixelAlphaEnable"].c_str()) ;

        comp.InputStream[i].LumaKeyEnable = atoi(streamParams[i]["LumaKeyEnable"].c_str()) ;
        comp.InputStream[i].LumaKeyMin    = atoi(streamParams[i]["LumaKeyMin"].c_str()) ;
        comp.InputStream[i].LumaKeyMax    = atoi(streamParams[i]["LumaKeyMax"].c_str()) ;
        cout << "Set " << i << "->" << comp.InputStream[i].DstX << ":" << comp.InputStream[i].DstY << ":" << comp.InputStream[i].DstW  << ":" << comp.InputStream[i].DstH << ":" << endl;
        cout << " Alpha Blending Params:" << endl;
        cout << "  GlobalAlphaEnable=" << comp.InputStream[i].GlobalAlphaEnable << endl;
        cout << "  GlobalAlpha=" << comp.InputStream[i].GlobalAlpha << endl;
        cout << "  PixelAlphaEnable=" << comp.InputStream[i].PixelAlphaEnable << endl;
        cout << "  LumaKeyEnable=" << comp.InputStream[i].LumaKeyEnable << endl;
        cout << "  LumaKeyMin=" << comp.InputStream[i].LumaKeyMin << endl;
        cout << "  LumaKeyMax=" << comp.InputStream[i].LumaKeyMax << endl;
    }

#ifdef TEST_VPP_COMP_RESET
    comp.InputStream[1].GlobalAlphaEnable = 0;
    comp.InputStream[1].GlobalAlpha = 128;
    comp.InputStream[StreamCount - 1].GlobalAlphaEnable = 1;
    comp.InputStream[StreamCount - 1].GlobalAlpha = 128;

#endif

    mfxExtBuffer* ExtBuffer[1];
    ExtBuffer[0]          = (mfxExtBuffer*)&comp;
    VPPParams.NumExtParam = 1;
    VPPParams.ExtParam    = (mfxExtBuffer**)&ExtBuffer[0];

    // Initialize Media SDK VPP
    sts = mfxVPP.Init(&VPPParams);
    MSDK_IGNORE_MFX_STS(sts, MFX_WRN_PARTIAL_ACCELERATION);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    // Query number of required surfaces for VPP
    mfxFrameAllocRequest VPPRequest[2];// [0] - in, [1] - out
    memset(&VPPRequest, 0, sizeof(mfxFrameAllocRequest)*2);
    sts = mfxVPP.QueryIOSurf(&VPPParams, VPPRequest);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    mfxU16 nVPPSurfNumIn  = VPPRequest[0].NumFrameSuggested;
    mfxU16 nVPPSurfNumOut = VPPRequest[1].NumFrameSuggested;

    // Allocate surfaces for VPP: In
    // - Width and height of buffer must be aligned, a multiple of 32
    // - Frame surface array keeps pointers all surface planes and general frame info

    if (nVPPSurfNumIn != StreamCount )
        sts = MFX_ERR_UNSUPPORTED;
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    mfxFrameSurface1** pVPPSurfacesIn = new mfxFrameSurface1*[nVPPSurfNumIn];
    MSDK_CHECK_POINTER(pVPPSurfacesIn, MFX_ERR_MEMORY_ALLOC);

    mfxU8* surfaceBuffersIn[MAX_INPUT_STREAMS] = {0}; // 1 primary stream + max 63 substreams
    mfxU8  bitsPerPixel = 12;  // NV12 format is a 12 bits per pixel format

    /* IF RGB4 case */
    if ((pa.scc == kRGB4) || (pa.dcc == kRGB4))
        bitsPerPixel = 32;

    for (i = 0; i < nVPPSurfNumIn; i++)
    {
        mfxU16 width; //= (mfxU16)MSDK_ALIGN32(atoi(subparams[i]["w"].c_str()));
        mfxU16 height;// = (mfxU16)MSDK_ALIGN32(atoi(subparams[i]["h"].c_str()));

        width  = (mfxU16)MSDK_ALIGN32(atoi(streamParams[i]["width"].c_str()));
        height = (mfxU16)MSDK_ALIGN32(atoi(streamParams[i]["height"].c_str()));
        mfxU32 surfaceSize = width * height * bitsPerPixel / 8;

        surfaceBuffersIn[i] = new mfxU8[surfaceSize];
        MSDK_CHECK_POINTER(surfaceBuffersIn[i], MFX_ERR_MEMORY_ALLOC);

        pVPPSurfacesIn[i] = new mfxFrameSurface1;
        MSDK_CHECK_POINTER(pVPPSurfacesIn[i], MFX_ERR_MEMORY_ALLOC);
        memset(pVPPSurfacesIn[i], 0, sizeof(mfxFrameSurface1));
        memcpy(&(pVPPSurfacesIn[i]->Info), &streams[i], sizeof(mfxFrameInfo));

        pVPPSurfacesIn[i]->Data.Y = surfaceBuffersIn[i];
        if (pa.scc == kNV12)
        {
            pVPPSurfacesIn[i]->Data.U = pVPPSurfacesIn[i]->Data.Y + width*height;
            pVPPSurfacesIn[i]->Data.V = pVPPSurfacesIn[i]->Data.U + 1;
            pVPPSurfacesIn[i]->Data.Pitch = width;
        }
        else /* IF RGB4 case */
        {
            pVPPSurfacesIn[i]->Data.U = pVPPSurfacesIn[i]->Data.Y + width*height;
            pVPPSurfacesIn[i]->Data.V = pVPPSurfacesIn[i]->Data.Y + 2*width*height;
            pVPPSurfacesIn[i]->Data.Pitch = 4*width;
        }

#ifndef ENABLE_INPUT
        // In case simulating direct access to frames we initialize the allocated surfaces with default pattern
        memset(pVPPSurfacesIn[i]->Data.Y, 0, width * height);  // Y plane
        memset(pVPPSurfacesIn[i]->Data.U, 0, (width * height)/2);
        memset(pVPPSurfacesIn[i]->Data.V, 0, (width * height)/2);// UV plane
#endif
    }


/*
    mfxU16 width = (mfxU16)MSDK_ALIGN32(VPPParams.vpp.In.Width);
    mfxU16 height = (mfxU16)MSDK_ALIGN32(VPPParams.vpp.In.Height);
    mfxU8  bitsPerPixel = 12;  // NV12 format is a 12 bits per pixel format
    mfxU32 surfaceSize = width * height * bitsPerPixel / 8;
    mfxU8* surfaceBuffersIn = (mfxU8 *)new mfxU8[surfaceSize * nVPPSurfNumIn];

    mfxFrameSurface1** pVPPSurfacesIn = new mfxFrameSurface1*[nVPPSurfNumIn];
    MSDK_CHECK_POINTER(pVPPSurfacesIn, MFX_ERR_MEMORY_ALLOC);
    for (int i = 0; i < nVPPSurfNumIn; i++)
    {
        pVPPSurfacesIn[i] = new mfxFrameSurface1;
        memset(pVPPSurfacesIn[i], 0, sizeof(mfxFrameSurface1));
        memcpy(&(pVPPSurfacesIn[i]->Info), &(VPPParams.vpp.In), sizeof(mfxFrameInfo));
        pVPPSurfacesIn[i]->Data.Y = &surfaceBuffersIn[surfaceSize * i];
        pVPPSurfacesIn[i]->Data.U = pVPPSurfacesIn[i]->Data.Y + width * height;
        pVPPSurfacesIn[i]->Data.V = pVPPSurfacesIn[i]->Data.U + 1;
        pVPPSurfacesIn[i]->Data.Pitch = width;

#ifndef ENABLE_INPUT
        // In case simulating direct access to frames we initialize the allocated surfaces with default pattern
        memset(pVPPSurfacesIn[i]->Data.Y, 100, width * height);  // Y plane
        memset(pVPPSurfacesIn[i]->Data.U, 50, (width * height)/2);  // UV plane
#endif
    }
*/
    // Allocate surfaces for VPP: Out
    mfxU16 width = (mfxU16)MSDK_ALIGN32(VPPParams.vpp.Out.Width);
    mfxU16 height = (mfxU16)MSDK_ALIGN32(VPPParams.vpp.Out.Height);
    mfxU32 surfaceSize = width * height * bitsPerPixel / 8;
    mfxU8* surfaceBuffersOut = (mfxU8 *)new mfxU8[surfaceSize * nVPPSurfNumOut];

    mfxFrameSurface1** pVPPSurfacesOut = new mfxFrameSurface1*[nVPPSurfNumOut];
    MSDK_CHECK_POINTER(pVPPSurfacesOut, MFX_ERR_MEMORY_ALLOC);
    for (i = 0; i < nVPPSurfNumOut; i++)
    {
        pVPPSurfacesOut[i] = new mfxFrameSurface1;
        memset(pVPPSurfacesOut[i], 0, sizeof(mfxFrameSurface1));
        memcpy(&(pVPPSurfacesOut[i]->Info), &(VPPParams.vpp.Out), sizeof(mfxFrameInfo));
        pVPPSurfacesOut[i]->Data.Y = &surfaceBuffersOut[surfaceSize * i];
        if (pa.scc == kNV12)
        {
            pVPPSurfacesOut[i]->Data.U = pVPPSurfacesOut[i]->Data.Y + width * height;
            pVPPSurfacesOut[i]->Data.V = pVPPSurfacesOut[i]->Data.U + 1;
            pVPPSurfacesOut[i]->Data.Pitch = width;
        }
        else /* IF RGB4 case */
        {
            pVPPSurfacesOut[i]->Data.U = pVPPSurfacesOut[i]->Data.Y + width * height;
            pVPPSurfacesOut[i]->Data.V = pVPPSurfacesOut[i]->Data.Y + 2*width * height;
            pVPPSurfacesOut[i]->Data.Pitch = 4*width;
        }
    }


    // ===================================
    // Start processing the frames
    //

#ifdef ENABLE_BENCHMARK
    LARGE_INTEGER tStart, tEnd;
    QueryPerformanceFrequency(&tStart);
    double freq = (double)tStart.QuadPart;
    QueryPerformanceCounter(&tStart);
#endif

    int nSurfIdxIn = 0, nSurfIdxOut = 0;
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
            nSurfIdxIn = GetFreeSurfaceIndex(pVPPSurfacesIn, nVPPSurfNumIn); // Find free input frame surface
            if (MFX_ERR_NOT_FOUND == nSurfIdxIn)
                return MFX_ERR_MEMORY_ALLOC;

            sts = LoadRawFrame(pVPPSurfacesIn[nSurfIdxIn], fSource[nSource++]); // Load frame from file into surface
            MSDK_BREAK_ON_ERROR(sts);
            if (nSource >= (mfxU32)StreamCount)
                nSource = 0;
        }

        bMultipleOut = false;

        nSurfIdxOut = GetFreeSurfaceIndex(pVPPSurfacesOut, nVPPSurfNumOut); // Find free output frame surface
        if (MFX_ERR_NOT_FOUND == nSurfIdxOut)
            return MFX_ERR_MEMORY_ALLOC;

        // Process a frame asychronously (returns immediately)
        sts = mfxVPP.RunFrameVPPAsync(pVPPSurfacesIn[nSurfIdxIn], pVPPSurfacesOut[nSurfIdxOut], NULL, &syncp);
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
#ifdef ENABLE_OUTPUT
        sts = WriteRawFrame(pVPPSurfacesOut[nSurfIdxOut], fSink);
        MSDK_BREAK_ON_ERROR(sts);

        printf("Frame number: %d\r", nFrame);

#ifdef TEST_VPP_COMP_RESET
        //if (nFrame >= framesToProcess)
        if (nFrame >= FRAME_NUM_50)
            break;
#else
        if (nFrame >= framesToProcess)
            break;
#endif /*TEST_VPP_COMP_RESET*/

#endif
    }

    // MFX_ERR_MORE_DATA means that the input file has ended, need to go to buffering loop, exit in case of other errors
    MSDK_IGNORE_MFX_STS(sts, MFX_ERR_MORE_DATA);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    //
    // Stage 2: Retrieve the buffered VPP frames
    //
    while (MFX_ERR_NONE <= sts)
    {
        nSurfIdxOut = GetFreeSurfaceIndex(pVPPSurfacesOut, nVPPSurfNumOut); // Find free frame surface
        if (MFX_ERR_NOT_FOUND == nSurfIdxOut)
            return MFX_ERR_MEMORY_ALLOC;

        // Process a frame asychronously (returns immediately)
        sts = mfxVPP.RunFrameVPPAsync(NULL, pVPPSurfacesOut[nSurfIdxOut], NULL, &syncp);
        MSDK_IGNORE_MFX_STS(sts, MFX_ERR_MORE_SURFACE);
        MSDK_BREAK_ON_ERROR(sts);

        sts = mfxSession.SyncOperation(syncp, 60000); // Synchronize. Wait until frame processing is ready
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

        ++nFrame;
#ifdef ENABLE_OUTPUT
        sts = WriteRawFrame(pVPPSurfacesOut[nSurfIdxOut], fSink);
        MSDK_BREAK_ON_ERROR(sts);

        printf("Frame number: %d\r", nFrame);
#endif
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


#ifdef TEST_VPP_COMP_RESET
    /*!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/
    /**/
    comp.NumInputStream = 2;

    StreamCount = 2;
    comp.InputStream[0].DstX = 200;
    comp.InputStream[0].DstY = 200;
    comp.InputStream[1].DstX = 0;
    sts = mfxVPP.Reset(&VPPParams);
    memset(&VPPRequest, 0, sizeof(mfxFrameAllocRequest)*2);
    sts = mfxVPP.QueryIOSurf(&VPPParams, VPPRequest);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    nVPPSurfNumIn = VPPRequest[0].NumFrameSuggested;
    nVPPSurfNumOut = VPPRequest[1].NumFrameSuggested;

    if (nVPPSurfNumIn != StreamCount )
        sts = MFX_ERR_UNSUPPORTED;
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    /*---------------------------------------------------------------------------------------------*/
    //
    // Stage 1: Main processing loop
    //
    while (MFX_ERR_NONE <= sts || MFX_ERR_MORE_DATA == sts || bMultipleOut )
    {
        if (!bMultipleOut)
        {
            nSurfIdxIn = GetFreeSurfaceIndex(pVPPSurfacesIn, nVPPSurfNumIn); // Find free input frame surface
            if (MFX_ERR_NOT_FOUND == nSurfIdxIn)
                return MFX_ERR_MEMORY_ALLOC;

            sts = LoadRawFrame(pVPPSurfacesIn[nSurfIdxIn], fSource[nSource++]); // Load frame from file into surface

                MSDK_BREAK_ON_ERROR(sts);
            if (nSource >= (mfxU32)StreamCount)
                nSource = 0;
        }

        bMultipleOut = false;

        nSurfIdxOut = GetFreeSurfaceIndex(pVPPSurfacesOut, nVPPSurfNumOut); // Find free output frame surface
        if (MFX_ERR_NOT_FOUND == nSurfIdxOut)
            return MFX_ERR_MEMORY_ALLOC;

        // Process a frame asychronously (returns immediately)
        sts = mfxVPP.RunFrameVPPAsync(pVPPSurfacesIn[nSurfIdxIn], pVPPSurfacesOut[nSurfIdxOut], NULL, &syncp);
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
#ifdef ENABLE_OUTPUT
        sts = WriteRawFrame(pVPPSurfacesOut[nSurfIdxOut], fSink);
        MSDK_BREAK_ON_ERROR(sts);

        printf("Frame number: %d\r", nFrame);

        if (nFrame >= framesToProcess)
        //if (nFrame >= FRAME_NUM_50)
            break;
#endif
    }

    // MFX_ERR_MORE_DATA means that the input file has ended, need to go to buffering loop, exit in case of other errors
    MSDK_IGNORE_MFX_STS(sts, MFX_ERR_MORE_DATA);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    //
    // Stage 2: Retrieve the buffered VPP frames
    //
    while (MFX_ERR_NONE <= sts)
    {
        nSurfIdxOut = GetFreeSurfaceIndex(pVPPSurfacesOut, nVPPSurfNumOut); // Find free frame surface
        if (MFX_ERR_NOT_FOUND == nSurfIdxOut)
            return MFX_ERR_MEMORY_ALLOC;

        // Process a frame asychronously (returns immediately)
        sts = mfxVPP.RunFrameVPPAsync(NULL, pVPPSurfacesOut[nSurfIdxOut], NULL, &syncp);
        MSDK_IGNORE_MFX_STS(sts, MFX_ERR_MORE_SURFACE);
        MSDK_BREAK_ON_ERROR(sts);

        sts = mfxSession.SyncOperation(syncp, 60000); // Synchronize. Wait until frame processing is ready
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

        ++nFrame;
#ifdef ENABLE_OUTPUT
        sts = WriteRawFrame(pVPPSurfacesOut[nSurfIdxOut], fSink);
        MSDK_BREAK_ON_ERROR(sts);

        printf("Frame number: %d\r", nFrame);
#endif
    }

    printf("\n");
    printf("Frames processed %d\n", nFrame);


    // MFX_ERR_MORE_DATA indicates that there are no more buffered frames, exit in case of other errors
    MSDK_IGNORE_MFX_STS(sts, MFX_ERR_MORE_DATA);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    /*---------------------------------------------------------------------------------------------*/
#endif // for TEST_VPP_COMP_RESET
    // ===================================================================
    // Clean up resources
    //  - It is recommended to close Media SDK components first, before releasing allocated surfaces, since
    //    some surfaces may still be locked by internal Media SDK resources.

    free(comp.InputStream);
    mfxVPP.Close();
    // mfxSession closed automatically on destruction

    for (i = 0; i < nVPPSurfNumIn; i++)
    {
        if (pVPPSurfacesIn[i]) delete pVPPSurfacesIn[i];
        if (surfaceBuffersIn[i]) delete surfaceBuffersIn[i];
    }
    MSDK_SAFE_DELETE_ARRAY(pVPPSurfacesIn);
    for (int i = 0; i < nVPPSurfNumOut; i++)
        delete pVPPSurfacesOut[i];
    MSDK_SAFE_DELETE_ARRAY(pVPPSurfacesOut);
    MSDK_SAFE_DELETE_ARRAY(surfaceBuffersOut);

    int cnt = 0;
    for (cnt = 0; cnt < StreamCount; ++cnt)
    {
        fclose(fSource[cnt]);
    }
    fclose(fSink);

#if !(defined(_WIN32) || defined(_WIN64))

    if (m_va_display)
    {
        vaTerminate(m_va_display);
    }
    if (m_card_fd >= 0)
    {
        close(m_card_fd);
    }
#endif

    return 0;
}
