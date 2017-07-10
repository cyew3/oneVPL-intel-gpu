/******************************************************************************\
Copyright (c) 2017, Intel Corporation
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

This sample was distributed or derived from the Intel's Media Samples package.
The original version of this sample may be obtained from https://software.intel.com/en-us/intel-media-server-studio
or https://software.intel.com/en-us/media-client-solutions-support.
\**********************************************************************************/

#ifndef __SAMPLE_HEVC_FEI_DEFS_H__
#define __SAMPLE_HEVC_FEI_DEFS_H__

#include <assert.h>
#include "sample_defs.h"
#include "mfxvideo.h"
#include "mfxvideo++.h"
#include <mfxfei.h>

struct sInputParams
{
    msdk_char  strSrcFile[MSDK_MAX_FILENAME_LEN];
    msdk_char  strDstFile[MSDK_MAX_FILENAME_LEN];

    bool bENCODE;
    bool bPREENC;
    bool bEncodedOrder;      // use EncodeOrderControl for external reordering
    mfxU32 ColorFormat;
    mfxU16 nPicStruct;
    mfxU8  QP;
    mfxU16 nWidth;          // source picture width
    mfxU16 nHeight;         // source picture height
    mfxF64 dFrameRate;
    mfxU32 nNumFrames;
    mfxU16 nNumSlices;
    mfxU16 nRefDist;        // number of frames to next I,P
    mfxU16 nGopSize;        // number of frames to next I
    mfxU16 nIdrInterval;    // distance between IDR frames in GOPs
    mfxU16 BRefType;        // B-pyramid ON/OFF/UNKNOWN (default, let MSDK lib to decide)
    mfxU16 nNumRef;         // number of reference frames (DPB size)
    mfxU16 nGopOptFlag;     // STRICT | CLOSED, default is OPEN GOP
    mfxU16 GPB;             // indicates that HEVC encoder use regular P-frames or GPB
    mfxU16 NumRefActiveP;   // maximal number of references for P frames
    mfxU16 NumRefActiveBL0; // maximal number of backward references for B frames
    mfxU16 NumRefActiveBL1; // maximal number of forward references for B frames

    sInputParams()
        : bENCODE(false)
        , bPREENC(false)
        , bEncodedOrder(false)
        , ColorFormat(MFX_FOURCC_I420)
        , nPicStruct(MFX_PICSTRUCT_PROGRESSIVE)
        , QP(26)
        , nWidth(0)
        , nHeight(0)
        , dFrameRate(30.0)
        , nNumFrames(0xffff)
        , nNumSlices(1)
        , nRefDist(1)
        , nGopSize(1)
        , nIdrInterval(0xffff)         // Infinite IDR interval
        , BRefType(MFX_B_REF_UNKNOWN)  // MSDK library make decision itself about enabling B-pyramid or not
        , nNumRef(1)
        , nGopOptFlag(0)               // OPEN GOP
        , GPB(MFX_CODINGOPTION_ON)     // use GPB frames
        , NumRefActiveP(4)
        , NumRefActiveBL0(2)
        , NumRefActiveBL1(1)
    {
        MSDK_ZERO_MEMORY(strSrcFile);
        MSDK_ZERO_MEMORY(strDstFile);
    }

};

/**********************************************************************************/

template <typename T, typename T1>
// `T1& lower` is bound below which `T& val` is considered invalid
T tune(const T& val, const T1& lower, const T1& default_val)
{
    if (val <= lower)
        return default_val;
    return val;
}

#define STATIC_ASSERT(ASSERTION, MESSAGE) char MESSAGE[(ASSERTION) ? 1 : -1]; MESSAGE
template<class T, class U> inline void Copy(T & dst, U const & src)
{
    STATIC_ASSERT(sizeof(T) == sizeof(U), copy_objects_of_different_size);
    MSDK_MEMCPY(&dst, &src, sizeof(dst));
}

/**********************************************************************************/
template<>struct mfx_ext_buffer_id<mfxExtFeiParam> {
    enum { id = MFX_EXTBUFF_FEI_PARAM };
};
// TODO: rework implementation for wrapper class
union MfxExtBuffer
{
    mfxExtBuffer header;
    mfxExtCodingOption opt;
    mfxExtCodingOption2 opt2;
    mfxExtCodingOption3 opt3;
    mfxExtFeiParam feipar;
};
const mfxI32 HEVC_FEI_ENCODE_VIDEOPARAM_EXTBUF_MAX_NUM = 5;

/** MfxParamsWrapper is an utility class which
 * encapsulates mfxVideoParam and a list of 'attached' mfxExtBuffer objects.
 */
template<typename T, size_t N>
struct MfxParamsWrapper: public T
{
    mfxExtBuffer* ext_buf_ptrs[N];
    MfxExtBuffer  ext_buf[N];
    struct
    {
        bool enabled;
        mfxI32  idx;
    } ext_buf_idxmap[HEVC_FEI_ENCODE_VIDEOPARAM_EXTBUF_MAX_NUM];

    MfxParamsWrapper()
    {
        memset(static_cast<T*>(this), 0, sizeof(T));
        if (!N) return;

        MSDK_ZERO_MEMORY(ext_buf_ptrs);
        MSDK_ZERO_MEMORY(ext_buf);
        MSDK_ZERO_MEMORY(ext_buf_idxmap);

        this->ExtParam = ext_buf_ptrs;
        for (size_t i = 0; i < N; ++i)
        {
            ext_buf_ptrs[i] = (mfxExtBuffer*)&ext_buf[i];
        }
    }
    explicit MfxParamsWrapper(const MfxParamsWrapper& ref)
    {
        *this = ref; // call to operator=
    }
    MfxParamsWrapper& operator=(const MfxParamsWrapper& ref)
    {
        T* dst = this;
        const T* src = &ref;

        Copy(*dst, *src);
        if (!N) return *this;

        MSDK_ZERO_MEMORY(ext_buf_ptrs);
        Copy(ext_buf, ref.ext_buf);
        Copy(ext_buf_idxmap, ref.ext_buf_idxmap);

        this->ExtParam = ext_buf_ptrs;
        for (size_t i = 0; i < N; ++i)
        {
            ext_buf_ptrs[i] = (mfxExtBuffer*)&ext_buf[i];
        }
        return *this;
    }
    explicit MfxParamsWrapper(const T& ref)
    {
        *this = ref; // call to operator=
    }
    MfxParamsWrapper& operator=(const T& ref)
    {
        T* dst = this;
        const T* src = &ref;

        Copy(*dst, *src);
        if (!N) return *this;

        MSDK_ZERO_MEMORY(ext_buf_ptrs);
        MSDK_ZERO_MEMORY(ext_buf);
        MSDK_ZERO_MEMORY(ext_buf_idxmap);

        this->ResetExtParams();
        for (size_t i = 0; i < N; ++i)
        {
            ext_buf_ptrs[i] = (mfxExtBuffer*)&ext_buf[i];
        }

        if (!src->NumExtParam) return *this;

        for (size_t i = 0; i < src->NumExtParam; ++i)
        {
            mfxI32 idx = this->enableExtParam(src->ExtParam[i]->BufferId);
            if (idx < 0) throw std::exception();
            MSDK_MEMCPY(ext_buf_ptrs[idx], src->ExtParam[i], src->ExtParam[i]->BufferSz);
        }

        return *this;
    }
    void ResetExtParams()
    {
        this->NumExtParam = 0;
        this->ExtParam = (N)? ext_buf_ptrs: NULL;
    }
    /** Function returns index of the already enabled buffer */
    mfxI32 getExtParamIdx(mfxU32 bufferid) const
    {
        mfxI32 idx_map = getEnabledMapIdx(bufferid);

        if (idx_map < 0) return -1;

        if (ext_buf_idxmap[idx_map].enabled) return ext_buf_idxmap[idx_map].idx;
        return -1;
    }
    /** Function returns index of the enabled buffer
     * and enables it if it is not yet enabled.
     */
    mfxI32 enableExtParam(mfxU32 bufferid)
    {
        mfxI32 idx_map = getEnabledMapIdx(bufferid);

        if (idx_map < 0) return -1;

        if (ext_buf_idxmap[idx_map].enabled) return ext_buf_idxmap[idx_map].idx;

        if (this->NumExtParam >= N)
        {
            return -1;
        }

        mfxI32 idx = this->NumExtParam++;

        ext_buf_idxmap[idx_map].enabled = true;
        ext_buf_idxmap[idx_map].idx = idx;

        MSDK_ZERO_MEMORY(ext_buf[idx]);
        switch (bufferid)
        {
          case MFX_EXTBUFF_CODING_OPTION:
            ext_buf[idx].opt.Header.BufferId = MFX_EXTBUFF_CODING_OPTION;
            ext_buf[idx].opt.Header.BufferSz = sizeof(mfxExtCodingOption);
            return idx;
          case MFX_EXTBUFF_CODING_OPTION2:
            ext_buf[idx].opt2.Header.BufferId = MFX_EXTBUFF_CODING_OPTION2;
            ext_buf[idx].opt2.Header.BufferSz = sizeof(mfxExtCodingOption2);
            return idx;
          case MFX_EXTBUFF_CODING_OPTION3:
            ext_buf[idx].opt3.Header.BufferId = MFX_EXTBUFF_CODING_OPTION3;
            ext_buf[idx].opt3.Header.BufferSz = sizeof(mfxExtCodingOption3);
            return idx;
          case MFX_EXTBUFF_FEI_PARAM:
            ext_buf[idx].feipar.Header.BufferId = MFX_EXTBUFF_FEI_PARAM;
            ext_buf[idx].feipar.Header.BufferSz = sizeof(mfxExtFeiParam);
            return idx;
          default:
            assert(!"add index to getEnabledMapIdx!");
            return -1;
        };
    }

    template<typename ExtBufType>
    ExtBufType* get() const
    {
        mfxI32 idx_map = getEnabledMapIdx(mfx_ext_buffer_id<ExtBufType>::id);

        if (idx_map < 0) {
            return NULL;
        }
        if (ext_buf_idxmap[idx_map].enabled) {
            return (ExtBufType*)&this->ext_buf[ext_buf_idxmap[idx_map].idx];
        }
        return NULL;
    }

protected:
    mfxI32 getEnabledMapIdx(mfxU32 bufferid) const
    {
        mfxI32 idx = 0;

        if (!N) return -1;
        switch (bufferid)
        {
        case MFX_EXTBUFF_FEI_PARAM:
            ++idx;
        case MFX_EXTBUFF_CODING_OPTION3:
            ++idx;
        case MFX_EXTBUFF_CODING_OPTION2:
            ++idx;
        case MFX_EXTBUFF_CODING_OPTION:
            if (idx >= HEVC_FEI_ENCODE_VIDEOPARAM_EXTBUF_MAX_NUM) return -1;
            return idx;
        default:
            return -1;
        };
    }
};

typedef
    MfxParamsWrapper<mfxVideoParam, HEVC_FEI_ENCODE_VIDEOPARAM_EXTBUF_MAX_NUM>
    MfxVideoParamsWrapper;

#endif // #define __SAMPLE_HEVC_FEI_DEFS_H__
