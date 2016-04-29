/******************************************************************************\
Copyright (c) 2005-2015, Intel Corporation
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

#include "mfx_samples_config.h"

#include "bitstream_pool.h"
#include "mfx_filter_defs.h"

using namespace std;

sBitstreamPoolEntry *BitstreamPool::findBitstream(int maxLength)
{
    for (list<sBitstreamPoolEntry>::iterator it = m_Bitstreams.begin(); it != m_Bitstreams.end(); ++it)
    {
        if ( (it->Locked == 0) && (it->pBitstream->pmfxBS->MaxLength == maxLength))
        {
            return &(*it);
        }
    }
    return NULL;
}

mfxStatus BitstreamPool::newBitstream(sOutputBitstream **bitstream, int maxLength)
{
    mfxStatus sts = MFX_ERR_NONE;
    sBitstreamPoolEntry *bs_entry = findBitstream(maxLength);

    if (!bs_entry)
    {
        if (m_Bitstreams.size() < m_Capacity)
        {
            sOutputBitstream *bs = new sOutputBitstream;
            MSDK_CHECK_POINTER(bs, MFX_ERR_MEMORY_ALLOC);

            bs->pmfxBS = new mfxBitstream;
            MSDK_CHECK_POINTER_SAFE(bs->pmfxBS, MFX_ERR_MEMORY_ALLOC, {delete bs;})

            memset(bs->pmfxBS, 0, sizeof(mfxBitstream));
            bs->pmfxBS->MaxLength = maxLength;
            bs->pmfxBS->Data = new mfxU8[bs->pmfxBS->MaxLength];
            MSDK_CHECK_POINTER_SAFE(bs->pmfxBS->Data, MFX_ERR_MEMORY_ALLOC, {delete bs->pmfxBS; delete bs;});

            bs->psyncP = new mfxSyncPoint;
            MSDK_CHECK_POINTER_SAFE(bs->psyncP, MFX_ERR_MEMORY_ALLOC, {delete [] bs->pmfxBS->Data; delete bs->pmfxBS; delete bs; });

            sBitstreamPoolEntry bs_entry(bs, 1);
            m_Bitstreams.push_back(bs_entry);

            *bitstream = bs;
        }
        else
        {
            *bitstream = NULL;
        }
    }
    else
    {
        bs_entry->Locked = 1;
        *bitstream = bs_entry->pBitstream;
    }

    return sts;
}

mfxStatus BitstreamPool::deleteBitstream(sOutputBitstream *bitstream)
{
    for (list<sBitstreamPoolEntry>::iterator it = m_Bitstreams.begin(); it != m_Bitstreams.end(); ++it)
    {
        if (it->pBitstream == bitstream)
        {
            it->Locked = 0;

            // clean up bitstream fields
            mfxU32 maxLength = it->pBitstream->pmfxBS->MaxLength;
            mfxU8 *data = it->pBitstream->pmfxBS->Data;
            memset(it->pBitstream->pmfxBS, 0, sizeof(mfxBitstream));
            it->pBitstream->pmfxBS->MaxLength = maxLength;
            it->pBitstream->pmfxBS->Data = data;

            return MFX_ERR_NONE;
        }
    }
    return MFX_ERR_UNDEFINED_BEHAVIOR;
}

mfxStatus BitstreamPool::FreeResources()
{
    for (list<sBitstreamPoolEntry>::iterator it = m_Bitstreams.begin(); it != m_Bitstreams.end(); ++it)
    {
        MSDK_SAFE_DELETE(it->pBitstream->psyncP);
        MSDK_SAFE_DELETE_ARRAY(it->pBitstream->pmfxBS->Data);
        MSDK_SAFE_DELETE(it->pBitstream->pmfxBS);
        MSDK_SAFE_DELETE(it->pBitstream);
    }
    m_Bitstreams.clear();
    return MFX_ERR_NONE;
}

BitstreamPool::BitstreamPool(unsigned capacity) : m_Capacity(capacity)
{
}

mfxStatus BitstreamPool::setCapacity(unsigned capacity)
{
    if (capacity < m_Bitstreams.size())
        return MFX_ERR_UNDEFINED_BEHAVIOR;

    m_Capacity = capacity;
    return MFX_ERR_NONE;
}

unsigned BitstreamPool::getCapacity()
{
    return m_Capacity;
}