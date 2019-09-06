// Copyright (c) 2019 Intel Corporation
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

#include "child.hxx"

namespace mocks { namespace dx11 { namespace winapi
{
    struct video_context_impl
        : child_impl<ID3D11VideoContext>
    {
        using type = ID3D11VideoContext;

        HRESULT GetDecoderBuffer(ID3D11VideoDecoder*, D3D11_VIDEO_DECODER_BUFFER_TYPE, UINT*, void**) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT ReleaseDecoderBuffer(ID3D11VideoDecoder*, D3D11_VIDEO_DECODER_BUFFER_TYPE) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT DecoderBeginFrame(ID3D11VideoDecoder*, ID3D11VideoDecoderOutputView*, UINT, const void*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT DecoderEndFrame(ID3D11VideoDecoder*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT SubmitDecoderBuffers(ID3D11VideoDecoder*, UINT, const D3D11_VIDEO_DECODER_BUFFER_DESC*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT DecoderExtension(ID3D11VideoDecoder*, const D3D11_VIDEO_DECODER_EXTENSION*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void VideoProcessorSetOutputTargetRect(ID3D11VideoProcessor*, BOOL, const RECT*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void VideoProcessorSetOutputBackgroundColor(ID3D11VideoProcessor*, BOOL, const D3D11_VIDEO_COLOR*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void VideoProcessorSetOutputColorSpace(ID3D11VideoProcessor*, const D3D11_VIDEO_PROCESSOR_COLOR_SPACE*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void VideoProcessorSetOutputAlphaFillMode(ID3D11VideoProcessor*, D3D11_VIDEO_PROCESSOR_ALPHA_FILL_MODE, UINT) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void VideoProcessorSetOutputConstriction(ID3D11VideoProcessor*, BOOL, SIZE) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void VideoProcessorSetOutputStereoMode(ID3D11VideoProcessor*, BOOL) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT VideoProcessorSetOutputExtension(ID3D11VideoProcessor*, const GUID*, UINT, void*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void VideoProcessorGetOutputTargetRect(ID3D11VideoProcessor*, BOOL*, RECT*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void VideoProcessorGetOutputBackgroundColor(ID3D11VideoProcessor*, BOOL*, D3D11_VIDEO_COLOR*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void VideoProcessorGetOutputColorSpace(ID3D11VideoProcessor*, D3D11_VIDEO_PROCESSOR_COLOR_SPACE*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void VideoProcessorGetOutputAlphaFillMode(ID3D11VideoProcessor*, D3D11_VIDEO_PROCESSOR_ALPHA_FILL_MODE*, UINT*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void VideoProcessorGetOutputConstriction(ID3D11VideoProcessor*, BOOL*, SIZE*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void VideoProcessorGetOutputStereoMode(ID3D11VideoProcessor*, BOOL*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT VideoProcessorGetOutputExtension(ID3D11VideoProcessor*, const GUID*, UINT, void*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void VideoProcessorSetStreamFrameFormat(ID3D11VideoProcessor*, UINT, D3D11_VIDEO_FRAME_FORMAT) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void VideoProcessorSetStreamColorSpace(ID3D11VideoProcessor*, UINT, const D3D11_VIDEO_PROCESSOR_COLOR_SPACE*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void VideoProcessorSetStreamOutputRate(ID3D11VideoProcessor*, UINT, D3D11_VIDEO_PROCESSOR_OUTPUT_RATE, BOOL, const DXGI_RATIONAL*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void VideoProcessorSetStreamSourceRect(ID3D11VideoProcessor*, UINT, BOOL, const RECT*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void VideoProcessorSetStreamDestRect(ID3D11VideoProcessor*, UINT, BOOL, const RECT*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void VideoProcessorSetStreamAlpha(ID3D11VideoProcessor*, UINT, BOOL, FLOAT) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void VideoProcessorSetStreamPalette(ID3D11VideoProcessor*, UINT, UINT, const UINT*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void VideoProcessorSetStreamPixelAspectRatio(ID3D11VideoProcessor*, UINT, BOOL, const DXGI_RATIONAL*, const DXGI_RATIONAL*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void VideoProcessorSetStreamLumaKey(ID3D11VideoProcessor*, UINT, BOOL, FLOAT, FLOAT) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void VideoProcessorSetStreamStereoFormat(ID3D11VideoProcessor*, UINT, BOOL, D3D11_VIDEO_PROCESSOR_STEREO_FORMAT, BOOL, BOOL, D3D11_VIDEO_PROCESSOR_STEREO_FLIP_MODE, int) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void VideoProcessorSetStreamAutoProcessingMode(ID3D11VideoProcessor*, UINT, BOOL) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void VideoProcessorSetStreamFilter(ID3D11VideoProcessor*, UINT, D3D11_VIDEO_PROCESSOR_FILTER, BOOL, int) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT VideoProcessorSetStreamExtension(ID3D11VideoProcessor*, UINT, const GUID*, UINT, void*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void VideoProcessorGetStreamFrameFormat(ID3D11VideoProcessor*, UINT, D3D11_VIDEO_FRAME_FORMAT*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void VideoProcessorGetStreamColorSpace(ID3D11VideoProcessor*, UINT, D3D11_VIDEO_PROCESSOR_COLOR_SPACE*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void VideoProcessorGetStreamOutputRate(ID3D11VideoProcessor*, UINT, D3D11_VIDEO_PROCESSOR_OUTPUT_RATE*, BOOL*, DXGI_RATIONAL*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void VideoProcessorGetStreamSourceRect(ID3D11VideoProcessor*, UINT, BOOL*, RECT*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void VideoProcessorGetStreamDestRect(ID3D11VideoProcessor*, UINT, BOOL*, RECT*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void VideoProcessorGetStreamAlpha(ID3D11VideoProcessor*, UINT, BOOL*, FLOAT*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void VideoProcessorGetStreamPalette(ID3D11VideoProcessor*, UINT, UINT, UINT*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void VideoProcessorGetStreamPixelAspectRatio(ID3D11VideoProcessor*, UINT, BOOL*, DXGI_RATIONAL*, DXGI_RATIONAL*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void VideoProcessorGetStreamLumaKey(ID3D11VideoProcessor*, UINT, BOOL*, FLOAT*, FLOAT*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void VideoProcessorGetStreamStereoFormat(ID3D11VideoProcessor*, UINT, BOOL*, D3D11_VIDEO_PROCESSOR_STEREO_FORMAT*, BOOL*, BOOL*, D3D11_VIDEO_PROCESSOR_STEREO_FLIP_MODE*, int*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void VideoProcessorGetStreamAutoProcessingMode(ID3D11VideoProcessor*, UINT, BOOL*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void VideoProcessorGetStreamFilter(ID3D11VideoProcessor*, UINT, D3D11_VIDEO_PROCESSOR_FILTER, BOOL*, int*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT VideoProcessorGetStreamExtension(ID3D11VideoProcessor*, UINT, const GUID*, UINT, void*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT VideoProcessorBlt(ID3D11VideoProcessor*, ID3D11VideoProcessorOutputView*, UINT, UINT, const D3D11_VIDEO_PROCESSOR_STREAM*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT NegotiateCryptoSessionKeyExchange(ID3D11CryptoSession*, UINT, void*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void EncryptionBlt(ID3D11CryptoSession*, ID3D11Texture2D*, ID3D11Texture2D*, UINT, void*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void DecryptionBlt(ID3D11CryptoSession*, ID3D11Texture2D*, ID3D11Texture2D*, D3D11_ENCRYPTED_BLOCK_INFO*, UINT, const void*, UINT, void*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void StartSessionKeyRefresh(ID3D11CryptoSession*, UINT, void*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void FinishSessionKeyRefresh(ID3D11CryptoSession*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT GetEncryptionBltKey(ID3D11CryptoSession*, UINT, void*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT NegotiateAuthenticatedChannelKeyExchange(ID3D11AuthenticatedChannel*, UINT, void*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT QueryAuthenticatedChannel(ID3D11AuthenticatedChannel*, UINT, const void*, UINT, void*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        HRESULT ConfigureAuthenticatedChannel(ID3D11AuthenticatedChannel*, UINT, const void*, D3D11_AUTHENTICATED_CONFIGURE_OUTPUT*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void VideoProcessorSetStreamRotation(ID3D11VideoProcessor*, UINT, BOOL, D3D11_VIDEO_PROCESSOR_ROTATION) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void VideoProcessorGetStreamRotation(ID3D11VideoProcessor*, UINT, BOOL*, D3D11_VIDEO_PROCESSOR_ROTATION*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
    };
} } }
