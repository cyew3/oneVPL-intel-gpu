//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2010 Intel Corporation. All Rights Reserved.
//

#include "test_usage_models_bitstream_writer.h"
#include "test_usage_models_exception.h"

TUMBitstreamWriter::TUMBitstreamWriter(const msdk_char *pFileName)
{
    mfxStatus sts1, sts2;

    sts1 = m_bitstreamWriter.Init( pFileName );

    MSDK_ZERO_MEMORY(m_bitstream);
    sts2 = InitMfxBitstream(&m_bitstream, 1024 * 1024);

    if(MFX_ERR_NONE == sts1 && MFX_ERR_NONE == sts2)
    {
        ;
    }
    else
    {
        throw TUMException(
                    "BitstreamWriter FAILED\n",
                    __FILE__,
                    __LINE__,
                    MY_FUNCSIG);
    }
    
} // TUMBitstreamWriter::TUMBitstreamWriter(const TCHAR *fileName)


TUMBitstreamWriter::~TUMBitstreamWriter()
{
    m_bitstreamWriter.Close();    
    WipeMfxBitstream(&m_bitstream);

} // TUMBitstreamWriter::~TUMBitstreamWriter()


mfxStatus TUMBitstreamWriter::WriteNextFrame( void )
{
    mfxStatus sts;

    sts = m_bitstreamWriter.WriteNextFrame( &m_bitstream );

    m_bitstream.DataLength = 0;
    m_bitstream.DataOffset = 0;
    
    return sts;

} // mfxStatus TUMBitstreamWriter::WriteNextFrame( void )


mfxStatus TUMBitstreamWriter::WriteNextFrame( mfxBitstream *pBS )
{
    bool bProgressBar = false;     

    mfxStatus sts = m_bitstreamWriter.WriteNextFrame( pBS, bProgressBar );    

    pBS->DataLength = 0;
    pBS->DataOffset = 0;
    
    return sts;

} // mfxStatus TUMBitstreamWriter::WriteNextFrame( mfxBitstream *pBS )


mfxBitstream* TUMBitstreamWriter::GetBitstreamPtr( void )
{
    return &m_bitstream;

} // mfxBitstream* TUMBitstreamWriter::GetBitstreamPtr( void )


/* EOF */
