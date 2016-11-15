/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014-2016 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#include "ts_vpp.h"
#include "ts_struct.h"
#include "ts_alloc.h"

#define EXT_BUF_PAR(eb) tsExtBufTypeToId<eb>::id, sizeof(eb)

namespace camera_query
{

typedef void (*callback)(tsExtBufType<mfxVideoParam>&, mfxU32, mfxU32);

void ext_buf(tsExtBufType<mfxVideoParam>& par, mfxU32 id, mfxU32 size)
{
    par.AddExtBuffer(id, size); 
}
void del_buf(tsExtBufType<mfxVideoParam>& par, mfxU32 id, mfxU32 /*size*/)
{
    par.RemoveExtBuffer(id); 
}

class TestSuite : tsVideoVPP
{
public:
    TestSuite() : tsVideoVPP(true, MFX_MAKEFOURCC('C','A','M','R')) {}
    ~TestSuite() {}
    int RunTest(unsigned int id);
    static const unsigned int n_cases;

    mfxStatus Init(mfxSession session, mfxVideoParam *par)
    {
        if(session && par)
        {
            if((MFX_FOURCC_A2RGB10 == par->vpp.In.FourCC) ||
               (MFX_FOURCC_ARGB16  == par->vpp.In.FourCC) ||
               (MFX_FOURCC_R16     == par->vpp.In.FourCC))
               par->vpp.In.BitDepthLuma = 10;
            if((MFX_FOURCC_A2RGB10 == par->vpp.Out.FourCC) ||
               (MFX_FOURCC_ARGB16  == par->vpp.Out.FourCC) ||
               (MFX_FOURCC_R16     == par->vpp.Out.FourCC))
               par->vpp.Out.BitDepthLuma = 10;
        }
        return tsVideoVPP::Init(session, par);
    }

    mfxStatus Query(mfxSession session, mfxVideoParam *in, mfxVideoParam *out)
    {
        if(session && in)
        {
            if((MFX_FOURCC_A2RGB10 == in->vpp.In.FourCC) ||
               (MFX_FOURCC_ARGB16  == in->vpp.In.FourCC) ||
               (MFX_FOURCC_R16     == in->vpp.In.FourCC))
               in->vpp.In.BitDepthLuma = 10;
            if((MFX_FOURCC_A2RGB10 == in->vpp.Out.FourCC) ||
               (MFX_FOURCC_ARGB16  == in->vpp.Out.FourCC) ||
               (MFX_FOURCC_R16     == in->vpp.Out.FourCC))
               in->vpp.Out.BitDepthLuma = 10;
        }
        return tsVideoVPP::Query(session, in, out);
    }

private:
    static const mfxU32 n_par = 10;

    enum QueryMode
    {
        NULL_SESSION  =   0x1 ,  //Query(0,p_in,p_out)
        NULL_IN       =   0x2 ,  //Query(ses,0,p_out)
        NULL_OUT      =   0x4 ,  //Query(ses,p_in,0)
        ZERO_UID      =   0x8 ,  //Zero plugin GUID
        DIFFERENT_IO  =   0x10,  //Query(ses,p_in,p_out), p_in and p_out are different structures with different extented buffers
        SAME_IO       =   0x20,  //Query(ses,p_in,p_out), p_in and p_out are different structures with the same extented buffers
        INPLACE       =   0x40,  //Query(ses,p_in,p_in)
        OPAQUE        =   0x80,
        ALLOC_LESS    = 0x1000,
        ALLOC_MORE    = 0x2000,
        SET_HANDLE_BEFOR =  0x100,
        SET_HANDLE_AFTER =  0x200

    };

    struct tc_struct
    {
        mfxStatus sts;
        mfxU32 mode;
        struct f_pair 
        {
            const  tsStruct::Field* f;
            mfxU32 v;
        } set_par[n_par];
        struct 
        {
            callback func;
            mfxU32 buf;
            mfxU32 size;
        } set_buf[2];
    };

    static const tc_struct test_case[];
};

const TestSuite::tc_struct TestSuite::test_case[] = 
{
    {/*00*/  MFX_ERR_INVALID_HANDLE, NULL_SESSION},
    {/*01*/  MFX_ERR_NONE,           NULL_IN},
    {/*02*/  MFX_ERR_NULL_PTR,       NULL_OUT},
    {/*03*/  MFX_ERR_NULL_PTR,       NULL_IN|NULL_OUT},
    {/*04*/  MFX_ERR_NONE,           SAME_IO},
    {/*05*/  MFX_ERR_NONE,           INPLACE},
    {/*06*/  MFX_ERR_UNDEFINED_BEHAVIOR, OPAQUE},            // In and Out has different number of ext buffers
    {/*07*/  MFX_ERR_UNDEFINED_BEHAVIOR, OPAQUE|ALLOC_LESS}, // In and Out has different number of ext buffers
    {/*08*/  MFX_ERR_UNDEFINED_BEHAVIOR, OPAQUE|ALLOC_MORE}, // In and Out has different number of ext buffers
    {/*09*/  MFX_ERR_NONE,               SET_HANDLE_AFTER},
    {/*10*/  MFX_ERR_UNSUPPORTED, SAME_IO, {}, {ext_buf, EXT_BUF_PAR(mfxExtVPPDoNotUse            )}}, // ext buffers
    {/*11*/  MFX_ERR_UNSUPPORTED, SAME_IO, {}, {ext_buf, EXT_BUF_PAR(mfxExtVppAuxData             )}},
    {/*12*/  MFX_ERR_NONE, SAME_IO, {}, {ext_buf, EXT_BUF_PAR(mfxExtVPPDenoise             )}},
    {/*13*/  MFX_ERR_UNSUPPORTED, SAME_IO, {}, {ext_buf, EXT_BUF_PAR(mfxExtVPPProcAmp             )}},
    {/*14*/  MFX_ERR_UNSUPPORTED, SAME_IO, {}, {ext_buf, EXT_BUF_PAR(mfxExtVPPDetail              )}},
    {/*15*/  MFX_ERR_UNSUPPORTED, SAME_IO, {}, {ext_buf, EXT_BUF_PAR(mfxExtVideoSignalInfo        )}},
    {/*16*/  MFX_ERR_UNDEFINED_BEHAVIOR, SAME_IO, {}, {ext_buf, EXT_BUF_PAR(mfxExtVPPDoUse               )}},
    {/*17*/  MFX_ERR_UNSUPPORTED, SAME_IO, {}, {ext_buf, EXT_BUF_PAR(mfxExtAVCRefListCtrl         )}},
    {/*18*/  MFX_ERR_UNSUPPORTED, SAME_IO, {}, {ext_buf, EXT_BUF_PAR(mfxExtVPPFrameRateConversion )}},
    {/*19*/  MFX_ERR_UNSUPPORTED, SAME_IO, {}, {ext_buf, EXT_BUF_PAR(mfxExtVPPImageStab           )}},
    {/*20*/  MFX_ERR_UNSUPPORTED, SAME_IO, {}, {ext_buf, EXT_BUF_PAR(mfxExtVPPComposite           )}},
    {/*21*/  MFX_ERR_UNSUPPORTED, SAME_IO, {}, {ext_buf, EXT_BUF_PAR(mfxExtVPPVideoSignalInfo     )}},
    {/*22*/  MFX_ERR_UNSUPPORTED, SAME_IO, {}, {ext_buf, EXT_BUF_PAR(mfxExtVPPDeinterlacing       )}},
    {/*23*/  MFX_ERR_UNSUPPORTED, SAME_IO, {}, {ext_buf, EXT_BUF_PAR(mfxExtCamGammaCorrection      )}},  // Incorrect gama mode
    {/*24*/  MFX_ERR_NONE,        SAME_IO, {}, {ext_buf, EXT_BUF_PAR(mfxExtCamWhiteBalance         )}},
    {/*25*/  MFX_ERR_NONE,        SAME_IO, {}, {ext_buf, EXT_BUF_PAR(mfxExtCamTotalColorControl    )}},
    {/*26*/  MFX_ERR_NONE,        SAME_IO, {}, {ext_buf, EXT_BUF_PAR(mfxExtCamCscYuvRgb            )}},
    {/*27*/  MFX_ERR_NONE,        SAME_IO, {}, {ext_buf, EXT_BUF_PAR(mfxExtCamHotPixelRemoval      )}},
    {/*28*/  MFX_ERR_NONE,        SAME_IO, {}, {ext_buf, EXT_BUF_PAR(mfxExtCamBlackLevelCorrection )}},
    {/*29*/  MFX_ERR_NONE,        SAME_IO, {}, {ext_buf, EXT_BUF_PAR(mfxExtCamColorCorrection3x3   )}},
    {/*30*/  MFX_ERR_NONE,        SAME_IO, {}, {ext_buf, EXT_BUF_PAR(mfxExtCamPadding              )}},
    {/*31*/  MFX_ERR_NONE, SAME_IO, {}, {ext_buf, EXT_BUF_PAR(mfxExtCamPipeControl          )}},
    {/*32*/  MFX_ERR_UNSUPPORTED,        INPLACE, {}, {ext_buf, EXT_BUF_PAR(mfxExtCamGammaCorrection      )}},  // Incorrect gamma mde
    {/*33*/  MFX_ERR_NONE,        INPLACE, {}, {ext_buf, EXT_BUF_PAR(mfxExtCamWhiteBalance         )}},
    {/*34*/  MFX_ERR_NONE,        INPLACE, {}, {ext_buf, EXT_BUF_PAR(mfxExtCamHotPixelRemoval      )}},
    {/*35*/  MFX_ERR_NONE,        INPLACE, {}, {ext_buf, EXT_BUF_PAR(mfxExtCamBlackLevelCorrection )}},
    {/*36*/  MFX_ERR_NONE,        INPLACE, {}, {ext_buf, EXT_BUF_PAR(mfxExtCamColorCorrection3x3   )}},
    {/*37*/  MFX_ERR_NONE,        INPLACE, {}, {ext_buf, EXT_BUF_PAR(mfxExtCamPadding              )}},
    {/*38*/  MFX_ERR_NONE,  INPLACE, {}, {ext_buf, EXT_BUF_PAR(mfxExtCamPipeControl          )}},
    {/*39*/  MFX_ERR_UNDEFINED_BEHAVIOR,  DIFFERENT_IO, {}, {ext_buf, EXT_BUF_PAR(mfxExtCamGammaCorrection      )}},
    {/*40*/  MFX_ERR_UNDEFINED_BEHAVIOR,  DIFFERENT_IO, {}, {ext_buf, EXT_BUF_PAR(mfxExtCamWhiteBalance         )}},
    {/*41*/  MFX_ERR_UNDEFINED_BEHAVIOR,  DIFFERENT_IO, {}, {ext_buf, EXT_BUF_PAR(mfxExtCamHotPixelRemoval      )}},
    {/*42*/  MFX_ERR_UNDEFINED_BEHAVIOR,  DIFFERENT_IO, {}, {ext_buf, EXT_BUF_PAR(mfxExtCamBlackLevelCorrection )}},
    {/*43*/  MFX_ERR_UNDEFINED_BEHAVIOR,  DIFFERENT_IO, {}, {ext_buf, EXT_BUF_PAR(mfxExtCamColorCorrection3x3   )}},
    {/*44*/  MFX_ERR_UNDEFINED_BEHAVIOR,  DIFFERENT_IO, {}, {ext_buf, EXT_BUF_PAR(mfxExtCamPadding              )}},
    {/*45*/  MFX_ERR_NONE,  DIFFERENT_IO, {}, {ext_buf, EXT_BUF_PAR(mfxExtCamPipeControl          )}},
    {/*46*/  MFX_ERR_UNDEFINED_BEHAVIOR,  DIFFERENT_IO, {}, {{ext_buf, EXT_BUF_PAR(mfxExtCamGammaCorrection      )},{ext_buf, EXT_BUF_PAR(mfxExtCamWhiteBalance         )} }},
    {/*47*/  MFX_ERR_UNDEFINED_BEHAVIOR,  DIFFERENT_IO, {}, {{ext_buf, EXT_BUF_PAR(mfxExtCamWhiteBalance         )},{ext_buf, EXT_BUF_PAR(mfxExtCamHotPixelRemoval      )} }},
    {/*48*/  MFX_ERR_UNDEFINED_BEHAVIOR,  DIFFERENT_IO, {}, {{ext_buf, EXT_BUF_PAR(mfxExtCamHotPixelRemoval      )},{ext_buf, EXT_BUF_PAR(mfxExtCamBlackLevelCorrection )} }},
    {/*49*/  MFX_ERR_UNDEFINED_BEHAVIOR,  DIFFERENT_IO, {}, {{ext_buf, EXT_BUF_PAR(mfxExtCamColorCorrection3x3   )},{ext_buf, EXT_BUF_PAR(mfxExtCamPadding              )} }},
    {/*50*/  MFX_ERR_UNDEFINED_BEHAVIOR,  DIFFERENT_IO, {}, {{ext_buf, EXT_BUF_PAR(mfxExtCamPadding              )},{ext_buf, EXT_BUF_PAR(mfxExtCamPipeControl          )} }},
    {/*51*/  MFX_ERR_UNDEFINED_BEHAVIOR,  DIFFERENT_IO, {}, {{ext_buf, EXT_BUF_PAR(mfxExtCamPipeControl          )},{ext_buf, EXT_BUF_PAR(mfxExtCamGammaCorrection      )} }},
    {/*52*/  MFX_ERR_UNSUPPORTED,   INPLACE,      {},  {del_buf, EXT_BUF_PAR(mfxExtCamPipeControl          )}},
    {/*53*/  MFX_ERR_UNSUPPORTED,   INPLACE,     { {&tsStruct::mfxVideoParam.vpp.In.Width,   2048},
                                                   {&tsStruct::mfxVideoParam.vpp.In.Height,  2048},
                                                   {&tsStruct::mfxVideoParam.vpp.Out.Width,  4096},
                                                   {&tsStruct::mfxVideoParam.vpp.Out.Height, 4096},
                                                   {&tsStruct::mfxVideoParam.vpp.In.CropW,   2048},
                                                   {&tsStruct::mfxVideoParam.vpp.In.CropH,   2048},
                                                   {&tsStruct::mfxVideoParam.vpp.Out.CropW,  4096},
                                                   {&tsStruct::mfxVideoParam.vpp.Out.CropH,  4096}} },
    {/*54*/  MFX_ERR_UNSUPPORTED,   INPLACE,     { {&tsStruct::mfxVideoParam.vpp.In.CropW,   2048},
                                                   {&tsStruct::mfxVideoParam.vpp.In.CropH,   2048},
                                                   {&tsStruct::mfxVideoParam.vpp.Out.CropW,  2048},
                                                   {&tsStruct::mfxVideoParam.vpp.Out.CropH,  2048}} },
    {/*55*/  MFX_ERR_UNSUPPORTED,   INPLACE,       {&tsStruct::mfxVideoParam.IOPattern,  MFX_IOPATTERN_IN_OPAQUE_MEMORY} },
    {/*56*/  MFX_ERR_UNSUPPORTED,   INPLACE,       {&tsStruct::mfxVideoParam.IOPattern,  MFX_IOPATTERN_IN_SYSTEM_MEMORY} },
    {/*57*/  MFX_ERR_UNSUPPORTED,   INPLACE,       {&tsStruct::mfxVideoParam.IOPattern,  MFX_IOPATTERN_IN_VIDEO_MEMORY}  },
    {/*58*/  MFX_ERR_UNSUPPORTED,   INPLACE,       {&tsStruct::mfxVideoParam.IOPattern,  MFX_IOPATTERN_OUT_OPAQUE_MEMORY} },
    {/*59*/  MFX_ERR_UNSUPPORTED,   INPLACE,       {&tsStruct::mfxVideoParam.IOPattern,  MFX_IOPATTERN_OUT_SYSTEM_MEMORY} },
    {/*60*/  MFX_ERR_UNSUPPORTED,   INPLACE,       {&tsStruct::mfxVideoParam.IOPattern,  MFX_IOPATTERN_OUT_VIDEO_MEMORY}  },
    {/*61*/  MFX_ERR_UNSUPPORTED,   INPLACE,       {&tsStruct::mfxVideoParam.IOPattern,  MFX_IOPATTERN_IN_OPAQUE_MEMORY|MFX_IOPATTERN_OUT_OPAQUE_MEMORY} },
    {/*62*/  MFX_ERR_UNSUPPORTED,   INPLACE,       {&tsStruct::mfxVideoParam.IOPattern,  MFX_IOPATTERN_IN_OPAQUE_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY} },
    {/*63*/  MFX_ERR_UNSUPPORTED,   INPLACE,       {&tsStruct::mfxVideoParam.IOPattern,  MFX_IOPATTERN_IN_OPAQUE_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY } },
    {/*64*/  MFX_ERR_UNSUPPORTED,   INPLACE,       {&tsStruct::mfxVideoParam.IOPattern,  MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_OPAQUE_MEMORY} },
    {/*65*/  MFX_ERR_NONE,          INPLACE,       {&tsStruct::mfxVideoParam.IOPattern,  MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY} },
    {/*66*/  MFX_ERR_NONE,          INPLACE,       {&tsStruct::mfxVideoParam.IOPattern,  MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY } },
    {/*67*/  /*MFX_ERR_NONE*/MFX_ERR_UNSUPPORTED,          INPLACE,       {&tsStruct::mfxVideoParam.IOPattern,  MFX_IOPATTERN_IN_VIDEO_MEMORY |MFX_IOPATTERN_OUT_OPAQUE_MEMORY} },
    {/*68*/  /*MFX_ERR_NONE*/MFX_ERR_UNSUPPORTED,          INPLACE,       {&tsStruct::mfxVideoParam.IOPattern,  MFX_IOPATTERN_IN_VIDEO_MEMORY |MFX_IOPATTERN_OUT_SYSTEM_MEMORY} },
    {/*69*/  /*MFX_ERR_NONE*/MFX_ERR_UNSUPPORTED,          INPLACE,       {&tsStruct::mfxVideoParam.IOPattern,  MFX_IOPATTERN_IN_VIDEO_MEMORY |MFX_IOPATTERN_OUT_VIDEO_MEMORY } },
    {/*70*/  MFX_ERR_UNSUPPORTED,   INPLACE,       {&tsStruct::mfxVideoParam.IOPattern,  MFX_IOPATTERN_IN_VIDEO_MEMORY |MFX_IOPATTERN_OUT_VIDEO_MEMORY | MFX_IOPATTERN_OUT_SYSTEM_MEMORY } },
    {/*71*/  MFX_ERR_UNSUPPORTED,   INPLACE,       {&tsStruct::mfxVideoParam.IOPattern,  MFX_IOPATTERN_IN_VIDEO_MEMORY |MFX_IOPATTERN_OUT_VIDEO_MEMORY | MFX_IOPATTERN_IN_SYSTEM_MEMORY } },

    {/*72*/  MFX_ERR_UNSUPPORTED,   INPLACE,     { {&tsStruct::mfxVideoParam.vpp.In.FourCC,       MFX_FOURCC_NV12},
                                                   {&tsStruct::mfxVideoParam.vpp.In.ChromaFormat, MFX_CHROMAFORMAT_YUV420}} },
    {/*73*/  MFX_ERR_UNSUPPORTED,   INPLACE,     { {&tsStruct::mfxVideoParam.vpp.In.FourCC,       MFX_FOURCC_YV12},
                                                   {&tsStruct::mfxVideoParam.vpp.In.ChromaFormat, MFX_CHROMAFORMAT_YUV420}} },
    {/*74*/  MFX_ERR_UNSUPPORTED,   INPLACE,     { {&tsStruct::mfxVideoParam.vpp.In.FourCC,       MFX_MAKEFOURCC('N','V','1','6')},
                                                   {&tsStruct::mfxVideoParam.vpp.In.ChromaFormat, MFX_CHROMAFORMAT_YUV420}} },
    {/*75*/  MFX_ERR_UNSUPPORTED,   INPLACE,     { {&tsStruct::mfxVideoParam.vpp.In.FourCC,       MFX_FOURCC_YUY2},
                                                   {&tsStruct::mfxVideoParam.vpp.In.ChromaFormat, MFX_CHROMAFORMAT_YUV422}} },
    {/*76*/  MFX_ERR_UNSUPPORTED,   INPLACE,     { {&tsStruct::mfxVideoParam.vpp.In.FourCC,       MFX_FOURCC_RGB3},
                                                   {&tsStruct::mfxVideoParam.vpp.In.ChromaFormat, MFX_CHROMAFORMAT_YUV444}} },
    {/*77*/  MFX_ERR_UNSUPPORTED,   INPLACE,     { {&tsStruct::mfxVideoParam.vpp.In.FourCC,       MFX_FOURCC_P8},
                                                   {&tsStruct::mfxVideoParam.vpp.In.ChromaFormat, MFX_CHROMAFORMAT_YUV444}} },
    {/*78*/  MFX_ERR_UNSUPPORTED,   INPLACE,     { {&tsStruct::mfxVideoParam.vpp.In.FourCC,       MFX_FOURCC_P010},
                                                   {&tsStruct::mfxVideoParam.vpp.In.ChromaFormat, MFX_CHROMAFORMAT_YUV444}} },
    {/*79*/  MFX_ERR_UNSUPPORTED,   INPLACE,     { {&tsStruct::mfxVideoParam.vpp.In.FourCC,       MFX_FOURCC_P8_TEXTURE},
                                                   {&tsStruct::mfxVideoParam.vpp.In.ChromaFormat, MFX_CHROMAFORMAT_YUV444}} },
    {/*80*/  MFX_ERR_UNSUPPORTED,   INPLACE,     { {&tsStruct::mfxVideoParam.vpp.In.FourCC,       MFX_FOURCC_BGR4},
                                                   {&tsStruct::mfxVideoParam.vpp.In.ChromaFormat, MFX_CHROMAFORMAT_YUV444}} },
    {/*81*/  MFX_ERR_UNSUPPORTED,   INPLACE,     { {&tsStruct::mfxVideoParam.vpp.In.FourCC,       MFX_FOURCC_A2RGB10},
                                                   {&tsStruct::mfxVideoParam.vpp.In.ChromaFormat, MFX_CHROMAFORMAT_YUV444}} },
    {/*82*/  MFX_ERR_UNSUPPORTED,   INPLACE,     { {&tsStruct::mfxVideoParam.vpp.In.FourCC,       MFX_FOURCC_RGB4},
                                                   {&tsStruct::mfxVideoParam.vpp.In.ChromaFormat, MFX_CHROMAFORMAT_YUV444}} },
    {/*83*/  MFX_ERR_NONE,          INPLACE,     { {&tsStruct::mfxVideoParam.vpp.In.FourCC,       MFX_FOURCC_R16},
                                                   {&tsStruct::mfxVideoParam.vpp.In.ChromaFormat, MFX_CHROMAFORMAT_MONOCHROME}} },
    {/*84*/  MFX_ERR_NONE,   INPLACE,     { {&tsStruct::mfxVideoParam.vpp.In.FourCC,       MFX_FOURCC_ARGB16},
                                                   {&tsStruct::mfxVideoParam.vpp.In.ChromaFormat, MFX_CHROMAFORMAT_YUV444}} },
                                    
    {/*85*/  MFX_ERR_NONE,          INPLACE,     { {&tsStruct::mfxVideoParam.vpp.Out.FourCC,       MFX_FOURCC_NV12},
                                                   {&tsStruct::mfxVideoParam.vpp.Out.ChromaFormat, MFX_CHROMAFORMAT_YUV420}} },
    {/*86*/  MFX_ERR_UNSUPPORTED,   INPLACE,     { {&tsStruct::mfxVideoParam.vpp.Out.FourCC,       MFX_FOURCC_YV12},
                                                   {&tsStruct::mfxVideoParam.vpp.Out.ChromaFormat, MFX_CHROMAFORMAT_YUV420}} },
    {/*87*/  MFX_ERR_UNSUPPORTED,   INPLACE,     { {&tsStruct::mfxVideoParam.vpp.Out.FourCC,       MFX_MAKEFOURCC('N','V','1','6')},
                                                   {&tsStruct::mfxVideoParam.vpp.Out.ChromaFormat, MFX_CHROMAFORMAT_YUV420}} },
    {/*88*/  MFX_ERR_UNSUPPORTED,   INPLACE,     { {&tsStruct::mfxVideoParam.vpp.Out.FourCC,       MFX_FOURCC_YUY2},
                                                   {&tsStruct::mfxVideoParam.vpp.Out.ChromaFormat, MFX_CHROMAFORMAT_YUV422}} },
    {/*89*/  MFX_ERR_UNSUPPORTED,   INPLACE,     { {&tsStruct::mfxVideoParam.vpp.Out.FourCC,       MFX_FOURCC_RGB3},
                                                   {&tsStruct::mfxVideoParam.vpp.Out.ChromaFormat, MFX_CHROMAFORMAT_YUV444}} },
    {/*90*/  MFX_ERR_UNSUPPORTED,   INPLACE,     { {&tsStruct::mfxVideoParam.vpp.Out.FourCC,       MFX_FOURCC_P8},
                                                   {&tsStruct::mfxVideoParam.vpp.Out.ChromaFormat, MFX_CHROMAFORMAT_YUV444}} },
    {/*91*/  MFX_ERR_UNSUPPORTED,   INPLACE,     { {&tsStruct::mfxVideoParam.vpp.Out.FourCC,       MFX_FOURCC_P010},
                                                   {&tsStruct::mfxVideoParam.vpp.Out.ChromaFormat, MFX_CHROMAFORMAT_YUV444}} },
    {/*92*/  MFX_ERR_UNSUPPORTED,   INPLACE,     { {&tsStruct::mfxVideoParam.vpp.Out.FourCC,       MFX_FOURCC_P8_TEXTURE},
                                                   {&tsStruct::mfxVideoParam.vpp.Out.ChromaFormat, MFX_CHROMAFORMAT_YUV444}} },
    {/*93*/  MFX_ERR_UNSUPPORTED,   INPLACE,     { {&tsStruct::mfxVideoParam.vpp.Out.FourCC,       MFX_FOURCC_BGR4},
                                                   {&tsStruct::mfxVideoParam.vpp.Out.ChromaFormat, MFX_CHROMAFORMAT_YUV444}} },
    {/*94*/  MFX_ERR_UNSUPPORTED,  INPLACE,     { {&tsStruct::mfxVideoParam.vpp.Out.FourCC,       MFX_FOURCC_A2RGB10},
                                                  {&tsStruct::mfxVideoParam.vpp.Out.ChromaFormat, MFX_CHROMAFORMAT_YUV444}} },
    {/*95*/  MFX_ERR_NONE,  INPLACE,            { {&tsStruct::mfxVideoParam.vpp.Out.FourCC,       MFX_FOURCC_RGB4},
                                                  {&tsStruct::mfxVideoParam.vpp.Out.ChromaFormat, MFX_CHROMAFORMAT_YUV444}} },
    {/*96*/  MFX_ERR_UNSUPPORTED,  INPLACE,     { {&tsStruct::mfxVideoParam.vpp.Out.FourCC,       MFX_FOURCC_R16},
                                                  {&tsStruct::mfxVideoParam.vpp.Out.ChromaFormat, MFX_CHROMAFORMAT_MONOCHROME}} },
    {/*97*/  MFX_ERR_NONE,  INPLACE,            { {&tsStruct::mfxVideoParam.vpp.Out.FourCC,       MFX_FOURCC_ARGB16},
                                                  {&tsStruct::mfxVideoParam.vpp.Out.ChromaFormat, MFX_CHROMAFORMAT_YUV444}} },

    {/*98*/  MFX_ERR_UNSUPPORTED,  INPLACE,       {&tsStruct::mfxVideoParam.Protected,  1} },
    {/*99*/  MFX_ERR_UNSUPPORTED,  INPLACE,       {&tsStruct::mfxVideoParam.Protected,  2} },

    {/*100*/  MFX_ERR_NONE,  INPLACE,       {&tsStruct::mfxVideoParam.AsyncDepth,  0} },
    {/*101*/  MFX_ERR_NONE,  INPLACE,       {&tsStruct::mfxVideoParam.AsyncDepth,  1} },
   {/*102*/  MFX_ERR_NONE,  INPLACE,       {&tsStruct::mfxVideoParam.AsyncDepth,  5} },
   {/*103*/  MFX_ERR_UNSUPPORTED,  INPLACE,       {&tsStruct::mfxVideoParam.AsyncDepth,  100} },
   {/*104*/  MFX_ERR_UNSUPPORTED,  INPLACE,    {
                                                 {&tsStruct::mfxVideoParam.vpp.In.CropH,   485},
                                                 {&tsStruct::mfxVideoParam.vpp.Out.CropH,  485}} },
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    const tc_struct& tc = test_case[id];

    if(tc.mode & ZERO_UID)
    {
        m_uid = 0;
    }

    if(tc.mode ^ NULL_SESSION)
    {
        MFXInit();
        if(m_uid)
        {
            Load();
        }
    }

    if (tc.mode & NULL_IN)
        m_pPar = 0;

    // set up parameters for case
    for(mfxU32 i = 0; i < n_par; i++)
    {
        if(tc.set_par[i].f)
        {
            tsStruct::set(m_pPar, *tc.set_par[i].f, tc.set_par[i].v);
            std::cout << "  Set field " << tc.set_par[i].f->name << " to " << tc.set_par[i].v << "\n";
        }
    }

    m_spool_in.UseDefaultAllocator(!!(m_par.IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY));
    if (tc.mode ^ NULL_SESSION)
    {
        SetFrameAllocator(m_session, m_spool_in.GetAllocator());
        m_spool_out.SetAllocator(m_spool_in.GetAllocator(), true);
    }

    if (tc.mode & OPAQUE)
    {
        m_par.vpp.In.PicStruct = MFX_PICSTRUCT_FIELD_TFF;
        mfxExtOpaqueSurfaceAlloc& osa = m_par;
        QueryIOSurf();
        if (tc.mode & ALLOC_LESS)
        {
            m_request[0].NumFrameSuggested = m_request[0].NumFrameMin = m_request[0].NumFrameMin - 1;
            m_request[1].NumFrameSuggested = m_request[1].NumFrameMin = m_request[1].NumFrameMin - 1;
        }
        else if (tc.mode & ALLOC_MORE)
        {
            m_request[0].NumFrameSuggested = m_request[0].NumFrameMin = m_request[0].NumFrameMin + 1;
            m_request[1].NumFrameSuggested = m_request[1].NumFrameMin = m_request[1].NumFrameMin + 1;
        }
        if (m_par.IOPattern & MFX_IOPATTERN_IN_OPAQUE_MEMORY)
            m_spool_in.AllocOpaque(m_request[0], osa);
        if (m_par.IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY)
            m_spool_out.AllocOpaque(m_request[1], osa);
    }

    tsExtBufType<mfxVideoParam> parOut;
    parOut.IOPattern = m_par.IOPattern;
    mfxVideoParam* pout = &parOut;
    mfxExtCamPipeControl& cam_ctrl = parOut;
    if(tc.mode & INPLACE)
        pout = m_pPar;
    else if(tc.mode & NULL_OUT)
        pout = 0;

    if(tc.set_buf[0].func)
    {
        (*tc.set_buf[0].func)(this->m_par, tc.set_buf[0].buf, tc.set_buf[0].size);
        if(tc.mode & SAME_IO)
            (*tc.set_buf[0].func)(parOut,  tc.set_buf[0].buf, tc.set_buf[0].size);
        if((tc.mode & DIFFERENT_IO) && *tc.set_buf[1].func)
            (*tc.set_buf[1].func)(parOut,  tc.set_buf[1].buf, tc.set_buf[1].size);
    }

    ///////////////////////////////////////////////////////////////////////////
    g_tsStatus.expect(tc.sts);
    Query(m_session, m_pPar, pout);

    if((tc.mode ^ NULL_SESSION) && (4 == id))
    {
        g_tsStatus.expect(MFX_ERR_NOT_INITIALIZED);
        GetVideoParam(m_session, m_pPar);
        g_tsStatus.expect(MFX_ERR_NOT_INITIALIZED);
        Close();
    }

    if(tc.mode & SET_HANDLE_AFTER)
    {
        mfxHDL hdl;
        mfxHandleType type; 
        m_spool_in.GetAllocator()->get_hdl(type, hdl); 
        g_tsStatus.expect(MFX_ERR_UNDEFINED_BEHAVIOR); //API 1.4 behavior is not acceptable since 1.6 API
        SetHandle(m_session, type, hdl);
    }

    if (tc.sts == MFX_ERR_UNSUPPORTED || tc.sts == MFX_WRN_INCOMPATIBLE_VIDEO_PARAM)
    {
        for(mfxU32 i = 0; i < n_par; i++)
        {
            if(tc.set_par[i].f)
            {
                if (tc.sts == MFX_WRN_INCOMPATIBLE_VIDEO_PARAM)
                    tsStruct::check_ne(pout, *tc.set_par[i].f, tc.set_par[i].v);
            }
        }
    }

    if (tc.mode == NULL_IN)
    {
        EXPECT_NE(0, pout->IOPattern);
        EXPECT_NE(0, pout->vpp.In.ChromaFormat);
        EXPECT_NE(0, pout->vpp.Out.ChromaFormat);
        EXPECT_NE((mfxU32)0, pout->vpp.In.FourCC);
        EXPECT_NE((mfxU32)0, pout->vpp.Out.FourCC);
        EXPECT_NE(0, pout->vpp.In.Width);
        EXPECT_NE(0, pout->vpp.Out.Width);
        EXPECT_NE(0, pout->vpp.In.Height);
        EXPECT_NE(0, pout->vpp.Out.Height);
    }

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(camera_query);

}