//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2010-2020 Intel Corporation. All Rights Reserved.
//

#include "test_usage_models_bitstream_writer.h"
#include "test_usage_models_exception.h"

TUMBitstreamWriter::TUMBitstreamWriter(const msdk_char *pFileName)
{
    mfxStatus sts1;

    sts1 = m_bitstreamWriter.Init( pFileName );

    m_bitstream.Extend(1024 * 1024);

    if(MFX_ERR_NONE == sts1)
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


mfxBitstreamWrapper* TUMBitstreamWriter::GetBitstreamPtr( void )
{
    return &m_bitstream;

} // mfxBitstreamWrapper* TUMBitstreamWriter::GetBitstreamPtr( void )


/* EOF */
