/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2009-2017 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#ifndef __MFX_BISTREAM_BUFFER_H
#define __MFX_BISTREAM_BUFFER_H

#include "umc_linear_buffer.h"
#include "mfx_bitstream2.h"

//lightweight analog to sample buffer
class MFXBistreamBuffer : public mfxBitstream2
{
public:
    MFXBistreamBuffer();
    virtual ~MFXBistreamBuffer();

    virtual mfxStatus Init( mfxU32 nBufferSizeMin
                          , mfxU32 nBufferSizeMax);
    
    //allows to change minBuffersize during execution
    virtual mfxStatus SetMinBuffSize(mfxU32 nBufferSizeMin);
    virtual mfxStatus GetMinBuffSize(mfxU32& nBufferSizeMin);

    virtual mfxStatus Reset();//reset will empties buffer without fully closing it
    virtual mfxStatus Close();
    virtual mfxStatus PutBuffer(bool bEos = false);
    virtual mfxStatus LockOutput(mfxBitstream2 * pBs);
    virtual mfxStatus UnLockOutput(mfxBitstream2 * pBs);

    mfxStatus UndoInputBS();

public:
    //copies one bitstream to another extends destination if necessary
    static mfxStatus CopyBsExtended(mfxBitstream2 *dest, mfxBitstream2 *src);
    //same as copy but say input bs that it is empty now
    static mfxStatus MoveBsExtended(mfxBitstream2 *dest, mfxBitstream2 *src);
    //Just extend Buffer to new size keeping data
    static mfxStatus ExtendBs(mfxU32 nNewLen, mfxBitstream *src);
    
protected:

    bool          m_bDonotUseLinear;
    bool          m_bStartedBuffering;
    mfxU32        m_nBufferSizeMax; 
    mfxU32        m_nBufferSizeMin; 
    mfxU32        m_nMinBufferSize;
    bool          m_bEnable;
    mfxBitstream  m_allocatedBuf;
    bool          m_bLocked;
    bool          m_bEos;//eos flag set by upstream block
    mfxU32        m_nMinSize;//buffer wilnot lock untill it reached this size

    mfxBitstream2  m_inputBS;//stored last bitstream 
    
    UMC::LinearBuffer m_UMCBuffer;
};

#endif//__MFX_BISTREAM_BUFFER_H
