// Copyright (c) 2017-2018 Intel Corporation
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

#include "umc_defs.h"
#ifdef UMC_ENABLE_AV1_VIDEO_DECODER

#ifndef __UMC_AV1_VA_PACKER_H
#define __UMC_AV1_VA_PACKER_H

#include "umc_va_base.h"
#include "umc_av1_frame.h"

#ifdef UMC_VA_DXVA
    #include "umc_av1_ddi.h"
#endif

#ifdef UMC_VA_LINUX
    #include <va/va_dec_av1.h>
#endif

namespace UMC
{ class MediaData; }

namespace UMC_AV1_DECODER
{
    class VP9Bitstream;
    class VP9DecoderFrame;

class Packer
{

public:

    Packer(UMC::VideoAccelerator * va);
    virtual ~Packer();

    virtual UMC::Status GetStatusReport(void* pStatusReport, size_t size) = 0;

    virtual void BeginFrame() = 0;
    virtual void EndFrame() = 0;

    virtual void PackAU(std::vector<TileSet>&, AV1DecoderFrame const&, bool) = 0;

    static Packer* CreatePacker(UMC::VideoAccelerator * va);

protected:

    UMC::VideoAccelerator *m_va;
};

#ifdef UMC_VA_DXVA

class PackerDXVA
    : public Packer
{
public:

    PackerDXVA(UMC::VideoAccelerator * va);

    void BeginFrame();
    void EndFrame();

    UMC::Status GetStatusReport(void* pStatusReport, size_t size);

protected:

    uint32_t  m_report_counter;
};

class PackerIntel
    : public PackerDXVA
{

public:

    PackerIntel(UMC::VideoAccelerator * va);

    void PackAU(std::vector<TileSet>&, AV1DecoderFrame const&, bool) override;

private:

    void PackPicParams(DXVA_Intel_PicParams_AV1&, AV1DecoderFrame const&);
    void PackTileControlParams(DXVA_Intel_Tile_AV1&, TileLocation const&);
};

#endif // UMC_VA_DXVA

#if defined(UMC_VA_LINUX)

class PackerVA
    : public Packer
{

public:

    PackerVA(UMC::VideoAccelerator * va);

    UMC::Status GetStatusReport(void * pStatusReport, size_t size);

    void BeginFrame();
    void EndFrame();

    void PackAU(std::vector<TileSet>&, AV1DecoderFrame const&, bool) override;

 private:

    void PackPicParams(VADecPictureParameterBufferAV1&, AV1DecoderFrame const&);
    void PackTileControlParams(VABitStreamParameterBufferAV1&, TileLocation const&);
};

#endif // UMC_VA_LINUX

} // namespace UMC_AV1_DECODER

#endif /* __UMC_AV1_VA_PACKER_H */
#endif // UMC_ENABLE_AV1_VIDEO_DECODER
