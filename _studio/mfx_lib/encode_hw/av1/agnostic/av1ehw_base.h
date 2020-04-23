// Copyright (c) 2019-2020 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#pragma once

#include "mfx_common.h"
#if defined(MFX_ENABLE_AV1_VIDEO_ENCODE)

#include "mfxvideo.h"
#include "mfxla.h"
#include "mfxbrc.h"
#include "mfx_ext_buffers.h"

#if !defined(MFX_VA_LINUX)
#include "mfxpcp.h"
#include "mfxwidi.h"
#endif //!defined(MFX_VA_LINUX)
#include "mfxvideo++int.h"

#include "mfx_utils_extbuf.h"

#include "feature_blocks/mfx_feature_blocks_init_macros.h"
#include "feature_blocks/mfx_feature_blocks_base.h"
#include "av1ehw_utils.h"

namespace AV1EHW
{
using namespace MfxFeatureBlocks;

namespace ExtBuffer
{
    using namespace MfxExtBuffer;
}

// This is internal logger for the development stage (it shouldn't appear in public releases)
#define AV1E_LOGGING
#if defined(AV1E_LOGGING) && (defined(WIN32) || defined(WIN64))
//#define AV1E_LOG(string, ...) do { printf("(AV1E) %s ",__FUNCTION__); printf(string, ##__VA_ARGS__); printf("\n"); fflush(0); } \
//                              while (0)
#define AV1E_LOG(string, ...) do { printf("[AV1E] "); printf(string, ##__VA_ARGS__); fflush(0); } \
                              while (0)
#else
#define AV1E_LOG(string, ...)
#endif

#define MFX_CHECK_WITH_LOG(EXPR, ERR, ERR_DESCR)    { if (!(EXPR)) { AV1E_LOG(ERR_DESCR); MFX_RETURN(ERR); }}

class BlockTracer
    : public ID
{
public:
    using TFeatureTrace = std::pair<std::string, const std::map<mfxU32, const std::string>>;

    BlockTracer(
        ID id
        , const char* fName = nullptr
        , const char* bName = nullptr);

    ~BlockTracer();

    const char* m_featureName;
    const char* m_blockName;
};


struct FeatureBlocks
    : FeatureBlocksCommon<BlockTracer>
{
MFX_FEATURE_BLOCKS_DECLARE_BQ_UTILS_IN_FEATURE_BLOCK
#define DEF_BLOCK_Q MFX_FEATURE_BLOCKS_DECLARE_QUEUES_IN_FEATURE_BLOCK
    #include "av1ehw_block_queues.h"
#undef DEF_BLOCK_Q

    virtual const char* GetFeatureName(mfxU32 featureID) override;
    virtual const char* GetBlockName(ID id) override;

    std::map<mfxU32, const BlockTracer::TFeatureTrace*> m_trace;
};

#define DEF_BLOCK_Q MFX_FEATURE_BLOCKS_DECLARE_QUEUES_EXTERNAL
    #include "av1ehw_block_queues.h"
#undef DEF_BLOCK_Q

class FeatureBase
    : public FeatureBaseCommon<FeatureBlocks>
{
public:
    FeatureBase() = delete;
    virtual ~FeatureBase() {}

    virtual void Init(
        mfxU32 mode/*eFeatureMode*/
        , FeatureBlocks& blocks) override;

protected:
#define DEF_BLOCK_Q MFX_FEATURE_BLOCKS_DECLARE_QUEUES_IN_FEATURE_BASE
#include "av1ehw_block_queues.h"
#undef DEF_BLOCK_Q
    typedef TPushQ1NC TPushQ1;

    FeatureBase(mfxU32 id)
        : FeatureBaseCommon<FeatureBlocks>(id)
    {}

    virtual const BlockTracer::TFeatureTrace* GetTrace() { return nullptr; }
    virtual void SetTraceName(std::string&& /*name*/) {}
};

}; //namespace AV1EHW

#endif
