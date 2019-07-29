//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2010-2020 Intel Corporation. All Rights Reserved.
//

#pragma once

#include "test_usage_models_utils.h"

class TUMBitstreamWriter
{
public:
    TUMBitstreamWriter(const msdk_char *pFileName);
    ~TUMBitstreamWriter();

    mfxStatus WriteNextFrame( void );
    mfxStatus WriteNextFrame( mfxBitstream* pBS );

    mfxBitstreamWrapper* GetBitstreamPtr( void );

protected:    

private:
    CSmplBitstreamWriter        m_bitstreamWriter;
    mfxBitstreamWrapper         m_bitstream;
    
};
/* EOF */