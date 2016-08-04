/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2008-2016 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#include "mfx_pipeline_defs.h"
#include "mfx_yuv_decoder.h"
#include "mfx_io_utils.h"
#include "outline_factory.h"

MFXYUVDecoder::MFXYUVDecoder(IVideoSession* session,
                             mfxVideoParam &frameParam,
                             mfxF64 dFramerate,
                             mfxU32 nInFourCC,
                             IMFXPipelineFactory * pFactory,
                             const vm_char *  outlineInput)
    : m_session(session)
    , m_vParam()
    //check for null faster that examinee string lenght each time frame gots constructed
    , m_internalBS()
    , m_syncPoint((mfxSyncPoint)&m_syncPoint)
    , m_nFrames(0)
    , m_pOutlineFileName(vm_string_strlen(outlineInput) ? outlineInput : NULL)
{
    mfxFrameInfo &info = m_vParam.mfx.FrameInfo;
    mfxFrameInfo &infoIn = frameParam.mfx.FrameInfo;

    info.ChromaFormat  = infoIn.ChromaFormat;
    info.FourCC        = infoIn.FourCC;
    info.CropX         = infoIn.CropX;
    info.CropY         = infoIn.CropY;

    // mynikols, bug VCSD100004627 fixed: 
    //info.CropX         = 0;
    //info.CropY         = 0;
    info.CropW         = infoIn.Width;
    info.CropH         = infoIn.Height;

    info.Shift         = infoIn.Shift;

    info.FrameRateExtD = 1000;
    info.FrameRateExtN = (int)(dFramerate * 1000.0);

    //progressive picstructure is default
    info.PicStruct     = (mfxU16)(MFX_PICSTRUCT_UNKNOWN == infoIn.PicStruct ? MFX_PICSTRUCT_PROGRESSIVE : infoIn.PicStruct);
    
    //native yuv file width
    m_yuvWidth         = infoIn.Width;
    m_yuvHeight        = infoIn.Height;
    
    //surface alignment requirements
    info.Width         = mfx_align((mfxU16)(infoIn.Width  + infoIn.CropX), 0x10);
    info.Height        = mfx_align((mfxU16)(infoIn.Height + infoIn.CropY), (info.PicStruct == MFX_PICSTRUCT_PROGRESSIVE)? 0x10 : 0x20);
    

    std::auto_ptr <IBitstreamConverterFactory > bsfac (pFactory->CreateBitstreamCVTFactory(NULL));
    m_pConverter.reset(bsfac->Create(nInFourCC, info.FourCC));
    if (NULL == m_pConverter.get())
    {
        MFX_TRACE_AND_THROW((VM_STRING("[MFXYUVDecoder] cannot create converter : %s -> %s\n"), 
            GetMFXFourccString(nInFourCC).c_str(), GetMFXFourccString(info.FourCC).c_str()));
    }
}

MFXYUVDecoder::~MFXYUVDecoder(void)
{
    Close();
}

mfxStatus MFXYUVDecoder::Close(void)
{
    MFX_DELETE_ARRAY(m_internalBS.Data);
    return MFX_ERR_NONE;
}

mfxStatus MFXYUVDecoder::Init(mfxVideoParam *par)
{
    MFX_CHECK_POINTER(par);

    m_vParam = *par;
    MFX_CHECK_STS(InitMfxBitstream(&m_internalBS, par));
    m_nFrames = 0;

    return MFX_ERR_NONE;
}

mfxStatus MFXYUVDecoder::DecodeFrame(mfxBitstream * bs, mfxFrameSurface1 *surface)
{
    MFX_CHECK_POINTER(surface);

    //will move bs data to internal
    MFX_CHECK_STS_SKIP(ConstructFrame(bs, &m_internalBS), MFX_ERR_MORE_DATA);

    //yuv decoder work with complete frames only
    if (m_internalBS.DataLength < GetMinPlaneSize(m_vParam.mfx.FrameInfo))
    {
        return MFX_ERR_MORE_DATA;
    }

    mfxBitstream bsTmp = m_internalBS;
    mfxStatus    sts   = MFX_ERR_NONE;

    MFXFrameAllocatorRW* pAlloc = NULL;
    if (!surface->Data.Y)
    {
        pAlloc = m_session->GetFrameAllocator();
        MFX_CHECK_POINTER(pAlloc); // no allocator means we can't get YUV pointers, so report an error
        MFX_CHECK_STS(pAlloc->LockFrameRW(surface->Data.MemId, &surface->Data, MFXReadWriteMid::write));
    }

    surface->Data.Corrupted   = 0;
    surface->Data.TimeStamp   = 0;
    surface->Info.CropW       = m_vParam.mfx.FrameInfo.CropW;
    surface->Info.CropH       = m_vParam.mfx.FrameInfo.CropH;
    surface->Info.Width       = m_vParam.mfx.FrameInfo.Width;
    surface->Info.Height      = m_vParam.mfx.FrameInfo.Height;

    //MFX_CHECK_STS_SKIP(sts = DecodeFrameInternal(&m_internalBS, surface), PIPELINE_ERROR_FILE_READ);
    //
    //free(surface->Data.MemId);

    MFX_CHECK_POINTER(m_pConverter.get());
    MFX_CHECK_STS_SKIP(sts = m_pConverter->Transform(&m_internalBS, surface), MFX_ERR_MORE_DATA);
    //free(surface->Data.MemId);
    
    if (MFX_ERR_MORE_DATA == sts )
    {
        m_internalBS = bsTmp;
    }else
    {
        //frameorders start from 0
        surface->Data.FrameOrder = m_nFrames;
        
        //frame readed
        m_nFrames++;

        if (NULL == m_pOutlineFileName)
        {
            surface->Data.TimeStamp = ConvertmfxF642MFXTime((mfxF64)m_nFrames * (mfxF64)m_vParam.mfx.FrameInfo.FrameRateExtD / 
                                                                                (mfxF64) m_vParam.mfx.FrameInfo.FrameRateExtN);
        }else
        {
            //read frameinfo from outline
            if (NULL == m_pOutlineReader.get())
            {
                OutlineFactoryAbstract * pFactory = NULL;
                
                MFX_CHECK_POINTER(pFactory = GetOutlineFactory());
                MFX_CHECK_POINTER((m_pOutlineReader.reset(pFactory->CreateReader()), m_pOutlineReader.get()));
                m_pOutlineReader->Init(m_pOutlineFileName);
            }

            //only 1 sequence
            Entry_Read *pEntry = NULL;
            MFX_CHECK_POINTER(pEntry = m_pOutlineReader->GetEntry(0, m_nFrames-1));
            
            TestVideoData vData;
            m_pOutlineReader->ReadEntryStruct(pEntry, &vData);

            switch(vData.GetPictureStructure())
            {
                case UMC::PS_TOP_FIELD :
                case UMC::PS_TOP_FIELD_FIRST :
                {
                    surface->Info.PicStruct = MFX_PICSTRUCT_FIELD_TFF;
                    break;
                }
                case UMC::PS_BOTTOM_FIELD :
                case UMC::PS_BOTTOM_FIELD_FIRST :
                {
                    surface->Info.PicStruct = MFX_PICSTRUCT_FIELD_BFF;
                    break;
                }
                case UMC::PS_FRAME :
                {
                    surface->Info.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
                    break;
                }
                default:
                    MFX_CHECK_TRACE(0, VM_STRING("invalid picstruct in outline file"));
            }

            surface->Data.TimeStamp = ConvertmfxF642MFXTime(vData.GetTime());
        }
    }

    if (NULL != pAlloc)
    {
        MFX_CHECK_STS(pAlloc->UnlockFrameRW( surface->Data.MemId
                                           , &surface->Data
                                           , MFXReadWriteMid::write));
    }

    return sts;
}

mfxStatus MFXYUVDecoder::DecodeFrameAsync( mfxBitstream2 & bs
                                         , mfxFrameSurface1 *surface_work
                                         , mfxFrameSurface1 **surface_out
                                         , mfxSyncPoint *syncp)
{
    MFX_CHECK_POINTER(surface_out);

    mfxStatus sts;

    if (MFX_ERR_NONE != (sts = DecodeFrame(&bs, surface_work)))
    {
        return sts;
    }
    surface_work->Info.FrameId.DependencyId = bs.DependencyId;
    *surface_out = surface_work;
    *syncp = m_syncPoint;
    
    return MFX_ERR_NONE;
}

mfxStatus MFXYUVDecoder::DecodeHeader(mfxBitstream *bs, mfxVideoParam *par)
{
    UNREFERENCED_PARAMETER(bs);
    MFX_CHECK_POINTER(par);

    par->mfx.FrameInfo    = m_vParam.mfx.FrameInfo;
    
    return MFX_ERR_NONE;
}

mfxStatus MFXYUVDecoder::ConstructFrame(mfxBitstream *in, mfxBitstream *out)
{
    //EOS case no error should be reported
    if (NULL == in)
        return MFX_ERR_MORE_DATA;

    MFX_CHECK_POINTER(out);
    
    if (in->DataLength == 0)
    {
        if (out->DataLength == 0)
        {
            return MFX_ERR_MORE_DATA;
        }

        return MFX_ERR_NONE;
    }

    int bytes2copy = (std::min)(in->DataLength, out->MaxLength - out->DataLength);

    memmove(out->Data, out->Data + out->DataOffset, out->DataLength);
    out->DataOffset = 0;

    memcpy(out->Data + out->DataLength, in->Data + in->DataOffset, bytes2copy);
    in->DataOffset  += bytes2copy;

    in->DataLength  -= bytes2copy;
    out->DataLength += bytes2copy;

    return MFX_ERR_NONE;
}

mfxU32  MFXYUVDecoder::ReadBs(mfxU8 * pData, int /*n*/, mfxU32 nSize, mfxBitstream * pBs)
{
    if (nSize > pBs->DataLength)
    {
        return 0;
    }
    memcpy(pData, pBs->Data + pBs->DataOffset, nSize);
    pBs->DataLength -= nSize;
    pBs->DataOffset += nSize;
    return nSize;
}

mfxStatus MFXYUVDecoder::Query(mfxVideoParam *in, mfxVideoParam *out)
{
    MFX_CHECK_POINTER(out);

    if (in == 0)
        memset(out, 0, sizeof(*out));
    else
        memcpy(out, in, sizeof(*out));

    return MFX_ERR_NONE;
}

mfxStatus MFXYUVDecoder::QueryIOSurf(mfxVideoParam * par, mfxFrameAllocRequest *request)
{
    MFX_CHECK_POINTER(request);
    MFX_CHECK_POINTER(par);

    memcpy(&request->Info, &par->mfx.FrameInfo, sizeof(par->mfx.FrameInfo));
    
    request->NumFrameSuggested = 1;
    request->NumFrameMin       = 1;
    return MFX_ERR_NONE;
}

mfxStatus MFXYUVDecoder::GetDecodeStat(mfxDecodeStat *stat)
{
    MFX_CHECK_POINTER(stat);
    MFX_ZERO_MEM(*stat);
    stat->NumFrame = m_nFrames;
    return MFX_ERR_NONE;
}

mfxStatus MFXYUVDecoder::Reset(mfxVideoParam* par) 
{
    MFX_CHECK_POINTER(par);
    mfxFrameInfo &info = par->mfx.FrameInfo;

    //resolution can be increased easily, just have to increase buffer size to store complete frames
    MFX_CHECK_STS(InitMfxBitstream(&m_internalBS, par));
    size_t maxSz = par->mfx.FrameInfo.Height * par->mfx.FrameInfo.Width * 4;
    if (maxSz < (size_t)par->mfx.BufferSizeInKB * 1024)
        maxSz = par->mfx.BufferSizeInKB * 1024;
    
    BSUtil::Extend(&m_internalBS, maxSz);


    m_yuvWidth                      = info.CropW;
    m_yuvHeight                     = info.CropH;
    m_vParam.mfx.FrameInfo.Width    = info.Width; 
    m_vParam.mfx.FrameInfo.Height   = info.Height;
    m_vParam.mfx.FrameInfo.CropW    = info.CropW; 
    m_vParam.mfx.FrameInfo.CropH    = info.CropH;
    m_internalBS.DataLength         = 0;
    m_internalBS.DataOffset         = 0;


    return MFX_ERR_NONE;
}

mfxStatus MFXYUVDecoder::GetVideoParam(mfxVideoParam* par)
{
    MFX_CHECK_POINTER(par);

    par->mfx.FrameInfo    = m_vParam.mfx.FrameInfo;

    return MFX_ERR_NONE;
}