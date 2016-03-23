//* ////////////////////////////////////////////////////////////////////////////// */
//*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2010-2016 Intel Corporation. All Rights Reserved.
//
//
//*/

#ifndef __PROTECTED_H__
#define __PROTECTED_H__

#include "sample_defs.h"

#if defined(_WIN32) || defined(_WIN64)
#include <d3d9.h>

#pragma warning(disable : 4201)
#include <dxva2api.h>
#pragma warning(default : 4201)
#endif

#pragma warning(disable: 4995)
#include <memory>
#pragma warning(default : 4995)

#include "pcp.h"
#include "sample_utils.h"

namespace ProtectedLibrary
{
class CProtectedSmplBitstreamWriter: public CSmplBitstreamWriter
{
public :

    CProtectedSmplBitstreamWriter(const pcpEncryptor& processor)
        :m_processor(processor)
    {
        CSmplBitstreamWriter();
    }

    virtual ~CProtectedSmplBitstreamWriter() {};

    virtual mfxStatus WriteNextFrame(mfxBitstream *pMfxBitstream, bool isPrint = true);
private:
        pcpEncryptor m_processor;
};


/**
   Class is implementation of stream reader for protected content. It has access to wrapper
   of PAVP device PAVPWrapper.
*/
class CProtectedSmplBitstreamReader : public CSmplBitstreamReader
{
public :
    /** Constructor returns pointer to interface IPAVPDevice to give access to PAVP device
    @param[out] pavp pointer to have access to PAVP device
    */
    CProtectedSmplBitstreamReader(const pcpEncryptor& encryptor, mfxU32 codecID, bool isFullEncrypt, bool sparse = false);
    virtual ~CProtectedSmplBitstreamReader();

    virtual void SetEncryptor(const pcpEncryptor& encryptor)
    {
        m_encryptor = encryptor;
        PCPVideoDecode_SetEncryptor(m_sessionPCP, &m_encryptor);
    }

    /** Free resources.*/
    virtual void      Close();
    /** Initialize bit stream reader, set basic parameters.
    PAVP device initialization is in ReadNextframe function on the first call.
    @param[in] strFileName name of input file for play
    */
    virtual mfxStatus Init(const msdk_char *strFileName);
    /** Read frame and prepare for protected decode. PAVP device initialization is done
    on the first call of function.
    @param[in] pBS input bit stream
    @param[in] codecID codec
    */
    virtual mfxStatus ReadNextFrame(mfxBitstream *pBS);

private:
    //!Flag to define if we want to pack slices as in input stream (isSparse=TRUE) or continusly
    bool m_isSparse;
    /** Copy constructor, is not used.
    @param[in] reader bit stream reader to be copied
    */
    CProtectedSmplBitstreamReader(const CProtectedSmplBitstreamReader &reader);
    /** Assign operator, is not used.
    @param[in] reader bit stream reader to be copied
    */
    CProtectedSmplBitstreamReader& operator =(const CProtectedSmplBitstreamReader &reader);
    // protected session
    pcpSession m_sessionPCP;
    // processed temporary stream
    mfxBitstream *m_processedBS;
    // input bit stream
    std::auto_ptr<mfxBitstream>  m_originalBS;

    // is stream ended
    bool m_isEndOfStream;
    mfxU32 m_codecID;

    pcpEncryptor m_encryptor;
    bool m_isFullEncrypt;
};
//@}

class AbstractSplitter;
struct FrameSplitterInfo;
class CSplitterBitstreamReader : public CSmplBitstreamReader
{
public :
    CSplitterBitstreamReader(mfxU32 codecID);
    virtual ~CSplitterBitstreamReader();

    /** Free resources.*/
    virtual void      Close();
    virtual mfxStatus Init(const msdk_char *strFileName);
    virtual mfxStatus ReadNextFrame(mfxBitstream *pBS);

private:
    mfxBitstream *m_processedBS;
    // input bit stream
    std::auto_ptr<mfxBitstream>  m_originalBS;

    mfxStatus PrepareNextFrame(mfxBitstream *in, mfxBitstream **out);

    // is stream ended
    bool m_isEndOfStream;
    mfxU32 m_codecID;

    std::auto_ptr<AbstractSplitter> m_pNALSplitter;
    FrameSplitterInfo *m_frame;
    mfxU8 *m_plainBuffer;
    mfxU32 m_plainBufferSize;
    mfxBitstream m_outBS;
};

} // namespace ProtectedLibrary

#endif // __PROTECTED_H__