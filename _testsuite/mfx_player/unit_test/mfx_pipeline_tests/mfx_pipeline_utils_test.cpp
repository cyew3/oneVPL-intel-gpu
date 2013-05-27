/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2012 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#include "utest_common.h"
#include "mfx_pipeline_utils.h"
#include "mfx_cmd_option_processor.h"

SUITE(mfx_utils)
{
    TEST(GetMinPlaneSize)
    {
        mfxFrameInfo info = {0};
        info.CropW = 5;
        info.CropH = 4;
        info.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
        info.FourCC = MFX_FOURCC_NV12;
        CHECK_EQUAL((mfxU32)5*4*3/2, GetMinPlaneSize(info));

        info.FourCC = MFX_FOURCC_YV12;
        CHECK_EQUAL((mfxU32)5*4*3/2, GetMinPlaneSize(info));

        info.FourCC = MFX_FOURCC_YUY2;
        CHECK_EQUAL((mfxU32)5*4*2, GetMinPlaneSize(info));

        info.FourCC = MFX_FOURCC_RGB3;
        CHECK_EQUAL((mfxU32)5*4*3, GetMinPlaneSize(info));

        info.FourCC = MFX_FOURCC_RGB4;
        CHECK_EQUAL((mfxU32)5*4*4, GetMinPlaneSize(info));

        info.ChromaFormat = MFX_CHROMAFORMAT_MONOCHROME;
        CHECK_EQUAL((mfxU32)5*4, GetMinPlaneSize(info));
    }
    
    TEST(GetMFXFrameInfoFromFOURCCPatternIdx)
    {
        mfxFrameInfo info = {0};
        
        CmdOptionProcessor  m_OptProc(false);
        vm_char* arg;
        int nPattern;
        
        arg = VM_STRING("nv12");
        nPattern = m_OptProc.Check(arg, VM_STRING(MFX_FOURCC_PATTERN()), VM_STRING(""), OPT_UNDEFINED);
        CHECK_EQUAL(MFX_ERR_NONE, GetMFXFrameInfoFromFOURCCPatternIdx(nPattern, info));
        CHECK_EQUAL(MFX_CHROMAFORMAT_YUV420, info.ChromaFormat);
        CHECK_EQUAL(MFX_FOURCC_NV12, (int)info.FourCC);

        arg = VM_STRING("yv12");
        nPattern = m_OptProc.Check(arg, VM_STRING(MFX_FOURCC_PATTERN()), VM_STRING(""), OPT_UNDEFINED);
        CHECK_EQUAL(MFX_ERR_NONE, GetMFXFrameInfoFromFOURCCPatternIdx(nPattern , info));
        CHECK_EQUAL(MFX_CHROMAFORMAT_YUV420, info.ChromaFormat);
        CHECK_EQUAL(MFX_FOURCC_YV12, (int)info.FourCC);

        arg = VM_STRING("yuy2:v");
        nPattern = m_OptProc.Check(arg, VM_STRING(MFX_FOURCC_PATTERN()), VM_STRING(""), OPT_UNDEFINED);
        CHECK_EQUAL(MFX_ERR_NONE, GetMFXFrameInfoFromFOURCCPatternIdx(nPattern , info));
        CHECK_EQUAL(MFX_CHROMAFORMAT_YUV422V, info.ChromaFormat);
        CHECK_EQUAL(MFX_FOURCC_YUY2, (int)info.FourCC);

        arg = VM_STRING("yuy2:h");
        nPattern = m_OptProc.Check(arg, VM_STRING(MFX_FOURCC_PATTERN()), VM_STRING(""), OPT_UNDEFINED);
        CHECK_EQUAL(MFX_ERR_NONE, GetMFXFrameInfoFromFOURCCPatternIdx(nPattern, info));
        CHECK_EQUAL(MFX_CHROMAFORMAT_YUV422H, info.ChromaFormat);
        CHECK_EQUAL(MFX_FOURCC_YUY2, (int)info.FourCC);

        arg = VM_STRING("yuy2:mono");
        nPattern = m_OptProc.Check(arg, VM_STRING(MFX_FOURCC_PATTERN()), VM_STRING(""), OPT_UNDEFINED);
        CHECK_EQUAL(MFX_ERR_NONE, GetMFXFrameInfoFromFOURCCPatternIdx(nPattern, info));
        CHECK_EQUAL(MFX_CHROMAFORMAT_YUV400, info.ChromaFormat);
        CHECK_EQUAL(MFX_FOURCC_YUY2, (int)info.FourCC);


        arg = VM_STRING("rgb24");
        nPattern = m_OptProc.Check(arg, VM_STRING(MFX_FOURCC_PATTERN()), VM_STRING(""), OPT_UNDEFINED);
        CHECK_EQUAL(MFX_ERR_NONE, GetMFXFrameInfoFromFOURCCPatternIdx(nPattern , info));
        CHECK_EQUAL(MFX_CHROMAFORMAT_YUV444, info.ChromaFormat);
        CHECK_EQUAL(MFX_FOURCC_RGB3, (int)info.FourCC);

        arg = VM_STRING("rgb32");
        nPattern = m_OptProc.Check(arg, VM_STRING(MFX_FOURCC_PATTERN()), VM_STRING(""), OPT_UNDEFINED);
        CHECK_EQUAL(MFX_ERR_NONE, GetMFXFrameInfoFromFOURCCPatternIdx(nPattern , info));
        CHECK_EQUAL(MFX_CHROMAFORMAT_YUV444, info.ChromaFormat);
        CHECK_EQUAL(MFX_FOURCC_RGB4, (int)info.FourCC);

        arg = VM_STRING("nv12:mono");
        nPattern = m_OptProc.Check(arg, VM_STRING(MFX_FOURCC_PATTERN()), VM_STRING(""), OPT_UNDEFINED);
        CHECK_EQUAL(MFX_ERR_NONE, GetMFXFrameInfoFromFOURCCPatternIdx(nPattern , info));
        CHECK_EQUAL(MFX_CHROMAFORMAT_YUV400, info.ChromaFormat);
        CHECK_EQUAL(MFX_FOURCC_NV12, (int)info.FourCC);

        arg = VM_STRING("yv12:mono");
        nPattern = m_OptProc.Check(arg, VM_STRING(MFX_FOURCC_PATTERN()), VM_STRING(""), OPT_UNDEFINED);
        CHECK_EQUAL(MFX_ERR_NONE, GetMFXFrameInfoFromFOURCCPatternIdx(nPattern , info));
        CHECK_EQUAL(MFX_CHROMAFORMAT_YUV400, info.ChromaFormat);
        CHECK_EQUAL(MFX_FOURCC_YV12, (int)info.FourCC);


        arg = VM_STRING("rgb24:mono");
        nPattern = m_OptProc.Check(arg, VM_STRING(MFX_FOURCC_PATTERN()), VM_STRING(""), OPT_UNDEFINED);
        CHECK_EQUAL(MFX_ERR_UNSUPPORTED, GetMFXFrameInfoFromFOURCCPatternIdx(nPattern, info));
        CHECK_EQUAL(0, nPattern);

        arg = VM_STRING("rgb32:mono");
        nPattern = m_OptProc.Check(arg, VM_STRING(MFX_FOURCC_PATTERN()), VM_STRING(""), OPT_UNDEFINED);
        CHECK_EQUAL(MFX_ERR_UNSUPPORTED, GetMFXFrameInfoFromFOURCCPatternIdx(nPattern, info));
        CHECK_EQUAL(0, nPattern);
    }
}