#include "hevce_mock_videocore.h"
#include "hevce_common_api.h"

using namespace ApiTestCommon;

MockVideoCORE::MockVideoCORE()
    : paramset()
{
    Zero(memIds);
}

void MockVideoCORE::SetParamSet(const ParamSet &param)
{ 
    paramset = &param;
}

mfxStatus MockVideoCORE::AllocFramesImpl1(mfxFrameAllocRequest *request, mfxFrameAllocResponse *response, bool)
{
    if (request == nullptr || response == nullptr ||
        request->Info.Width != paramset->videoParam.mfx.FrameInfo.Width ||
        request->Info.Height != paramset->videoParam.mfx.FrameInfo.Height ||
        request->Info.FourCC != paramset->videoParam.mfx.FrameInfo.FourCC) {
        throw std::runtime_error("");
    }

    response->mids = std::find(memIds, memIds + sizeof(memIds) / sizeof(memIds[0]), nullptr);
    if (response->mids == memIds + sizeof(memIds) / sizeof(memIds[0])) {
        throw std::runtime_error("");
    }

    response->mids[0] = (mfxMemId)0x77777777;
    response->NumFrameActual = request->NumFrameMin;
    return MFX_ERR_NONE;
}

mfxStatus MockVideoCORE::AllocFramesImpl2(mfxFrameAllocRequest *request, mfxFrameAllocResponse *response, mfxFrameSurface1 **pOpaqueSurface, mfxU32 NumOpaqueSurface)
{
    if (pOpaqueSurface != paramset->extOpaqueSurfaceAlloc.In.Surfaces ||
        NumOpaqueSurface != paramset->extOpaqueSurfaceAlloc.In.NumSurface) {
        throw std::runtime_error("");
    }
    return MockVideoCORE::AllocFramesImpl1(request, response, true);
}

mfxStatus MockVideoCORE::FreeFramesImpl(mfxFrameAllocResponse *response, bool)
{
    if (response == nullptr || response->mids < memIds ||
        response->mids > memIds + sizeof(memIds) / sizeof(memIds[0]))
        throw std::runtime_error("");
    response->mids[0] = nullptr;
    return MFX_ERR_NONE;
}
