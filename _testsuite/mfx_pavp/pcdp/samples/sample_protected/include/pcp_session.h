//* ////////////////////////////////////////////////////////////////////////////// */
//*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2010-2013 Intel Corporation. All Rights Reserved.
//
//
//*/

#ifndef __PCP_SESSION_H__
#define __PCP_SESSION_H__

#include "pcp.h"

#include <vector>
#include <memory>

#include "mfxvideo.h"
#if defined(_WIN32) || defined(_WIN64)
#endif
#include "abstract_splitter.h"
/**
\addtogroup pcp
*/
//@{
/** Protected session structure */
struct _pcpSession
{
    /**pointer to splitter: mpeg2, h264, vc1 */
    ProtectedLibrary::AbstractSplitter *m_pNALSplitter;
    /**information about frame */
    ProtectedLibrary::FrameSplitterInfo *m_frame;
    /**buffer for prepared encrypted slices */
    mfxU8 *m_encryptedSliceBuffer;
    /**size of m_encryptedSliceBuffer */
    mfxU32 m_encryptedSliceBufferSize;
    /**buffer for prepared plain data (for example,headers) */
    mfxU8 *m_plainBuffer;
    /**size of m_plainBuffer */
    mfxU32 m_plainBufferSize;
    /**Structure with encryption function */
    pcpEncryptor m_encryptor;
    /**Encryption type*/
    bool m_isFullEncryption;
    /**Output bit stream */
    mfxBitstream m_outBS;
};
//@}
#endif// __PCP_SESSION_H__