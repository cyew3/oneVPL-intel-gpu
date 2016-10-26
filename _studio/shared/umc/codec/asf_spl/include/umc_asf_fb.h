//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2008-2013 Intel Corporation. All Rights Reserved.
//

#ifndef __UMC_ASF_FB_H__
#define __UMC_ASF_FB_H__

#include "umc_sample_buffer.h"

namespace UMC
{

  class ASFFrameBuffer : public SampleBuffer
{

    DYNAMIC_CAST_DECL(ASFFrameBuffer, SampleBuffer)

public:

    ASFFrameBuffer();
    ~ASFFrameBuffer();

    // inherited from SampleBuffer
    Status LockInputBuffer(MediaData* in);
    Status UnLockInputBuffer(MediaData *in, Status streamStat = UMC_OK);


protected:

    SampleInfo *m_pCurFrame;

private:
    ASFFrameBuffer(const ASFFrameBuffer &);
    void operator=(ASFFrameBuffer &);

};

} // namespace UMC

#endif //__UMC_ASF_FB_H__
