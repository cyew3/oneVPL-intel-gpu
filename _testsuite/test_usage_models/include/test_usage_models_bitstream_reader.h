//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2010-2020 Intel Corporation. All Rights Reserved.
//

#pragma once

#include "vm/strings_defs.h"
#include "test_usage_models_utils.h"

class TUMBitstreamReader
{
public:
    TUMBitstreamReader(const msdk_char *pFileName);
    ~TUMBitstreamReader();

    mfxStatus ReadNextFrame( void );
    mfxStatus ExtendBitstream( void );

    mfxBitstreamWrapper* GetBitstreamPtr( void );

protected:    

private:
    CSmplBitstreamReader        m_bitstreamReader;
    mfxBitstreamWrapper         m_bitstream;

    bool                 m_bInited;
    bool                 m_bEOF;
    
};
/* EOF */
