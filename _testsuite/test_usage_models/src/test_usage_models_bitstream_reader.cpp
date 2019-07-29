//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2010-2020 Intel Corporation. All Rights Reserved.
//

#include "test_usage_models_bitstream_reader.h"
#include "test_usage_models_exception.h"

TUMBitstreamReader::TUMBitstreamReader(const msdk_char * pFileName)
{
    mfxStatus sts1;

    sts1 = m_bitstreamReader.Init( pFileName );   

    m_bitstream.Extend(1024 * 1024);

    m_bInited = false;
    m_bEOF    = false;
    if(MFX_ERR_NONE == sts1)
    {
        m_bInited = true;
    }
    else
    {
        throw TUMException(
                    "BitstreamReader FAILED\n",
                    __FILE__,
                    __LINE__,
                    MY_FUNCSIG);
    }

} // TUMBitstreamReader::TUMBitstreamReader(const TCHAR *fileName)


TUMBitstreamReader::~TUMBitstreamReader()
{    
    m_bitstreamReader.Close();

} // TUMBitstreamReader::~TUMBitstreamReader()


mfxStatus TUMBitstreamReader::ReadNextFrame( void )
{
    mfxStatus sts;

    sts = m_bitstreamReader.ReadNextFrame( &m_bitstream );

    if(MFX_ERR_MORE_DATA == sts)
    {
        m_bEOF = true;
    }

    return sts;

} // mfxBitstream* TUMBitstreamReader::ReadNextFrame()


mfxStatus TUMBitstreamReader::ExtendBitstream( void )
{
    MSDK_CHECK_ERROR(m_bInited, false, MFX_ERR_NOT_INITIALIZED);

    m_bitstream.Extend(m_bitstream.MaxLength * 2);

    return MFX_ERR_NONE;

} // mfxStatus TUMBitstreamReader::ExtendBitstream( void )


mfxBitstreamWrapper* TUMBitstreamReader::GetBitstreamPtr( void )
{
    if( m_bEOF || !m_bInited )
    {
        return NULL;
    }
    else
    {
        return &m_bitstream;
    }

} // mfxBitstreamWrapper* TUMBitstreamReader::GetBitstreamPtr( void )

/* EOF */
