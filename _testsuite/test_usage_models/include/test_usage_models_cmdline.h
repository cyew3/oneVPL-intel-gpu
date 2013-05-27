//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2010 Intel Corporation. All Rights Reserved.
//

#pragma once

#include <map>
#include "test_usage_models_utils.h"

class CommandLine
{
public:
    static void PrintUsage(const TCHAR* app);    

    CommandLine(int argc, TCHAR *argv[]);
    bool IsValid() const { return m_valid; }

    void GetParam( AppParam& param );
    void PrintInfo( void );

protected:    

private:

    mfxU32 m_srcVideoFormat;    
    
    mfxU32 m_dstVideoFormat;    
    mfxU16 m_width; // desired output width.  optional   
    mfxU16 m_height;// desired output height. optional
    mfxU32 m_bitRate;
    mfxF64 m_frameRate;
    mfxU16 m_targetUsage;    

    mfxU16 m_asyncDepth; // asyncronous queue
    mfxU16 m_usageModel;

    mfxU32 m_framesCount;

    TCHAR* m_pSrcFileName;
    TCHAR* m_pDstFileName;

    std::map<TCHAR*, mfxIMPL> m_impLib;    

    mfxU16 m_IOPattern; // DEC->(SYS/D3D frames)->VPP->(SYS/D3D frames)->ENC

    bool   m_valid;    
    bool   IsVPPEnable( void );
};
/* EOF */
