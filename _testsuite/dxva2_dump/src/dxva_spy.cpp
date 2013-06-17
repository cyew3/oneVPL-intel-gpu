/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2003-2012 Intel Corporation. All Rights Reserved.
//
*/

#include <string>
#include <iostream>
#include <initguid.h>
#include <stdio.h>

#include "umc_va_dxva2.h"

#include "encoding_ddi.h"
#include "dxva2_spy.h"
#include "dx11_spy.h"
#include "dxva2_buffers_ext.h"
#include <dxva.h>
#include <dxva2api.h>
#include "dxva2_log.h"
#include "mfx_ddi_enc_dump.h"

#include "umc_jpeg_ddi.h"
#include "umc_mvc_ddi.h"
#include "umc_svc_ddi.h"

#include <sdkddkver.h>
#if (NTDDI_VERSION >= NTDDI_VERSION_FROM_WIN32_WINNT2(0x0602)) // >= _WIN32_WINNT_WIN8
#define MVC_SUPPORT 1 // Enable D3D11 support if SDK allows
#else
#define MVC_SUPPORT 0
#endif


using namespace UMC;

//annoing string functions warnings
#pragma warning (disable : 4018 4996 4995)
///////////////////////////////////////////////////////////////////////

#define MPEG4_VLD 0

DEFINE_GUID(DXVADDI_Intel_Decode_PrivateData_Report, 0x49761bec, 0x4b63, 0x4349, 0xa5, 0xff, 0x87, 0xff, 0xdf, 0x8, 0x84, 0x66);
DEFINE_GUID(DXVA2_ModeMPEG4_VLD, 0x8ccf025a, 0xbacf, 0x4e44, 0x81, 0x16, 0x55, 0x21, 0xd9, 0xb3, 0x94, 0x7);

int g_bSkipExecute = -1;
int g_bDumpSurface = 1;
int MaxFrames = 0;

void SkipDXVAExecute(bool bSkipExecute)
{
    g_bSkipExecute = bSkipExecute;
}

///////////////////////////////////////////////////////////////////////
void dumpJPEGBuffer(int BufferType, void *buffer);

void dumpH264PictureParameters(void *pPictureParameters);
void dumpH264SliceParameter(void * sliceParam, bool isLong);
void dumpH264SliceParameters(void * sliceParam, size_t bufSize, bool isLong);

void dumpH264PictureParameters_MS_MVC(void *pPictureParameters);
void dumpH264MVCPictureParameters(void *pPictureParameters);

void dumpH264SVCPictureParameters(void *pPictureParameters);
void dumpH264SVCSliceParameters(void * sliceParam, size_t bufSize, bool isLong);

void dumpMPEG2PictureParameters(void *pPictureParameters);
void dumpMPEG4PictureParameters(void *pPictureParameters);
void dumpVC1PictureParametersExt(void *pPictureParameters);
void dumpD3DSurface(IDirect3DSurface9 *pRenderTarget);
///////////////////////////////////////////////////////////////////////

void dumpVideoDesc(const D3D11_VIDEO_DECODER_DESC *pVideoDesc)
{
    logi(pVideoDesc->SampleWidth);
    logi(pVideoDesc->SampleHeight);
    logi(pVideoDesc->OutputFormat);
}

void dumpVideoDesc(const DXVA2_VideoDesc *pVideoDesc)
{
    //((DXVA2_VideoDesc *)pVideoDesc)->Format = (D3DFORMAT)875714126;
    logi(pVideoDesc->SampleWidth);
    logi(pVideoDesc->SampleHeight);
    logi(pVideoDesc->SampleFormat.SampleFormat);
    logi(pVideoDesc->SampleFormat.VideoChromaSubsampling);
    logi(pVideoDesc->SampleFormat.NominalRange);
    logi(pVideoDesc->SampleFormat.VideoTransferMatrix);
    logi(pVideoDesc->SampleFormat.VideoLighting);
    logi(pVideoDesc->SampleFormat.VideoPrimaries);
    logi(pVideoDesc->SampleFormat.VideoTransferFunction);
    logi(pVideoDesc->Format);
    logcc(pVideoDesc->Format);
    //logi(pVideoDesc->InputSampleFreq);
    //logi(pVideoDesc->OutputFrameFreq);
    logi(pVideoDesc->UABProtectionLevel);
    logi(pVideoDesc->Reserved);
}

template<typename T>
void dumpConfig(const T *pConfig)
{
    //LOG_GUID(DXVA2_NoEncrypt);
    LOG_GUID(pConfig->guidConfigBitstreamEncryption);
    LOG_GUID(pConfig->guidConfigMBcontrolEncryption);
    LOG_GUID(pConfig->guidConfigResidDiffEncryption);
    logi(pConfig->ConfigBitstreamRaw);
    logi(pConfig->ConfigMBcontrolRasterOrder);
    logi(pConfig->ConfigResidDiffHost);
    logi(pConfig->ConfigSpatialResid8);
    logi(pConfig->ConfigResid8Subtraction);
    logi(pConfig->ConfigSpatialHost8or9Clipping);
    logi(pConfig->ConfigSpatialResidInterleaved);
    logi(pConfig->ConfigIntraResidUnsigned);
    logi(pConfig->ConfigResidDiffAccelerator);
    logi(pConfig->ConfigHostInverseScan);
    logi(pConfig->ConfigSpecificIDCT);
    logi(pConfig->Config4GroupedCoefs);
    logi(pConfig->ConfigMinRenderTargetBuffCount);
    logi(pConfig->ConfigDecoderSpecific);
};

UMC::VideoAccelerationProfile GetProfile(REFGUID Guid)
{
    UMC::VideoAccelerationProfile profile = UNKNOWN;

    if (Guid == DXVA2_ModeMPEG2_VLD)
    {
        profile = MPEG2_VLD;
    }
    else if (Guid == DXVA2_ModeMPEG2_IDCT)
    {
        profile = MPEG2_VLD;
    }
    else if (Guid == DXVA2_ModeH264_VLD_NoFGT || Guid == sDXVA2_Intel_EagleLake_ModeH264_VLD_NoFGT || Guid == sDXVA_Intel_ModeH264_VLD_MVC ||
        Guid == sDXVA2_ModeH264_VLD_NoFGT ||
        Guid == sDXVA_ModeH264_VLD_Multiview_NoFGT || Guid == sDXVA_ModeH264_VLD_Stereo_NoFGT || Guid == sDXVA_ModeH264_VLD_Stereo_Progressive_NoFGT ||
        Guid == sDXVA_ModeH264_VLD_SVC_Scalable_Baseline || Guid == sDXVA_ModeH264_VLD_SVC_Scalable_High || Guid == sDXVA_ModeH264_VLD_SVC_Scalable_Constrained_High ||
        Guid == sDXVA_ModeH264_VLD_SVC_Scalable_Constrained_Baseline)
    {
        profile = H264_VLD;
    }
    else if (Guid == DXVA2_ModeMPEG4_VLD)
    {
        profile = (UMC::VideoAccelerationProfile)MPEG4_VLD;
    }
    else if (Guid == DXVA2_ModeVC1_VLD)
    {
        profile = VC1_VLD;
    }
    else if (Guid == sDXVA2_Intel_IVB_ModeJPEG_VLD_NoFGT)
    {
        profile = JPEG_VLD;
    }

    return profile;
}

template<typename ExecuteParams>
DWORD GetBufferType(const ExecuteParams * buffer)
{
    return buffer->CompressedBufferType;
}


template<>
DWORD GetBufferType<D3D11_VIDEO_DECODER_BUFFER_DESC>(const D3D11_VIDEO_DECODER_BUFFER_DESC * buffer)
{
    return buffer->BufferType;
}

template<typename ExecuteParams, typename ConfigPictureDecodeType, typename SurfaceType>
class Dumper
{
public:
    Dumper()
        : m_cFrameNumber(0)
        , m_surface(0)
        , m_Profile(UNKNOWN)
    {
          memset(m_pBuffers, 0, sizeof(m_pBuffers));
          memset(m_cBuffersSize, 0, sizeof(m_cBuffersSize));
    }

    virtual void Init(REFGUID Guid, const ConfigPictureDecodeType * config)
    {
        m_guid = Guid;
        m_Config = *config;
        m_Profile = GetProfile(Guid);
    }

    virtual bool GetSkipExecute()
    {
        static int initialized = 0;

        if (!initialized)
        {
            char * dxvaEnv = getenv("DXVA_SKIP");

            int option = dxvaEnv ? atoi(dxvaEnv) : 0;
            g_bSkipExecute = g_bSkipExecute == -1 ? option != 0 : g_bSkipExecute;
            if (option)
                MaxFrames = option + 1;
            initialized = 1;
        }

        return g_bSkipExecute > 0;
    }

    virtual bool GetDumpSurface()
    {
        static int initialized = 0;

        if (!initialized)
        {
            char * dxvaEnv = getenv("DXVA_DUMP_SURFACE");

            int option = dxvaEnv ? atoi(dxvaEnv) : 1;
            g_bDumpSurface = g_bDumpSurface == -1 ? option != 0 : g_bDumpSurface;
            initialized = 1;
        }

        return g_bDumpSurface > 0;
    }

    virtual void GetBuffer(UINT BufferType, void **ppBuffer, UINT *pBufferSize)
    {
        if (GetSkipExecute())
        {
            // Alloc and return general memory
            if (!m_pBuffers[BufferType])
            {
                m_pBuffers[BufferType] = malloc(BUFFER_SIZE);
            }
            m_cBuffersSize[BufferType] = BUFFER_SIZE;
            *ppBuffer = m_pBuffers[BufferType];
            *pBufferSize = BUFFER_SIZE;
            return;
        }

        m_pBuffers[BufferType] = *ppBuffer;
        m_cBuffersSize[BufferType] = *pBufferSize;
    }

    virtual void BeginFrame(SurfaceType * pRenderTarget)
    {
        m_surface = pRenderTarget;
    }

    virtual void EndFrame()
    {
        dumpD3DSurface(m_surface);
    }

    virtual void Execute(Ipp32u NumCompBuffers, const ExecuteParams * pCompressedBuffers)
    {
        if (!NumCompBuffers || !pCompressedBuffers)
            return;

        m_cFrameNumber++;

        if (MaxFrames && m_cFrameNumber >= MaxFrames)
            return;

        {
#if 0
            if (params->pCompressedBuffers == 0)
            {
                if (params->pExtensionData->Function == ENCODE_ENC_ID ||
                    params->pExtensionData->Function == ENCODE_PAK_ID ||
                    params->pExtensionData->Function == ENCODE_ENC_PAK_ID)
                {
                    ENCODE_EXECUTE_PARAMS* encodeExec = (ENCODE_EXECUTE_PARAMS *)params->pExtensionData->pPrivateInputData;
                    char fname[1024];
                    BYTE *pMBData = NULL;
                    BYTE *pMVData = NULL;
                    int numMB = 0;

                    for (UINT i = 0; i < encodeExec->NumCompBuffers; i++)
                    {
                        ENCODE_COMPBUFFERDESC& buffer = encodeExec->pCompressedBuffers[i];
                        sprintf(fname, "%s\\frame%d_buffer%d.log", pDir, m_cFrameNumber, buffer.CompressedBufferType);

                        if (buffer.CompressedBufferType == D3DDDIFMT_INTELENCODE_SPSDATA)
                        {
                            f = fopen(fname, "wt");
                            ENCODE_SET_SEQUENCE_PARAMETERS_H264 *pSPS = (ENCODE_SET_SEQUENCE_PARAMETERS_H264 *)((BYTE *)buffer.pCompBuffer + buffer.DataOffset);
                            ddiDumpH264SPS(f, pSPS);
                            numMB = ((pSPS->FrameWidth + 15)/16) * ((pSPS->FrameHeight + 15)/16);
                        }
                        else if (buffer.CompressedBufferType == D3DDDIFMT_INTELENCODE_PPSDATA)
                        {
                            f = fopen(fname, "wt");
                            ddiDumpH264PPS(f, (ENCODE_SET_PICTURE_PARAMETERS_H264 *)((BYTE *)buffer.pCompBuffer + buffer.DataOffset));
                        }
                        else if (buffer.CompressedBufferType == D3DDDIFMT_INTELENCODE_SLICEDATA)
                        {
                            f = fopen(fname, "wt");
                            ddiDumpH264SliceHeader(f, (ENCODE_SET_SLICE_HEADER_H264 *)((BYTE *)buffer.pCompBuffer + buffer.DataOffset), buffer.DataSize / sizeof(ENCODE_SET_SLICE_HEADER_H264));
                        }
                        else if (buffer.CompressedBufferType == D3DDDIFMT_INTELENCODE_BITSTREAMDATA)
                        {
                            DWORD bitsteamSurfaceIdx = *((DWORD *)((BYTE *)buffer.pCompBuffer + buffer.DataOffset));
                            f = fopen(fname, "wt");
                            logi_flush(bitsteamSurfaceIdx);
                        }
                        else if (buffer.CompressedBufferType == D3DDDIFMT_INTELENCODE_MBDATA)
                        {
                            DWORD MBSurfaceIdx = *((DWORD *)((BYTE *)buffer.pCompBuffer + buffer.DataOffset));
                            f = fopen(fname, "wt");
                            logi_flush(MBSurfaceIdx);
                            // TODO: lock surface here
                        }

                        if (f)
                        {
                            fclose(f);
                            f = 0;
                        }
                    }
                    /*if (pMBData && pMVData)
                    {
                        sprintf(fname, "%s\\frame%d_mbdata.log", pDir, m_cFrameNumber);
                        f = fopen(fname, "wt");
                        ddiDumpH264MBData(f, 0, 0, 0, 0, 0, numMB, pMBData, pMVData);
                    }*/
                    LLLVDEC; // workaround
                }
            }
            else
#endif
            {
                char fname[1024];
                sprintf(fname, "%s\\frame%d_buffer_params.log", pDir, m_cFrameNumber, NumCompBuffers);
                fclose(f);
                f = fopen(fname, m_Profile == JPEG_VLD ? "a": "wt");

                int i;
                for (i = NumCompBuffers - 1; i >= 0; i--)
                {
                    int BufferType = GetBufferType(&pCompressedBuffers[i]);
                    logs("-----------------------------");
                    logi(BufferType);
                    logi(pCompressedBuffers[i].BufferIndex);
                    logi(pCompressedBuffers[i].DataOffset);
                    logi(pCompressedBuffers[i].DataSize);
                    logi(pCompressedBuffers[i].FirstMBaddress);
                    logi(pCompressedBuffers[i].NumMBsInBuffer);
                    logi(pCompressedBuffers[i].Width);
                    logi(pCompressedBuffers[i].Height);
                    logi(pCompressedBuffers[i].Stride);
                    logi(m_cBuffersSize[BufferType]);
                    logi(pCompressedBuffers[i].DataSize);
                }
                fclose(f);
                f = NULL;

                for (i = 0; i < NumCompBuffers; i++)
                {
                    int BufferType = GetBufferType(&pCompressedBuffers[i]);
                    m_cBuffersSize[BufferType] = pCompressedBuffers[i].DataSize;

                    char fname[1024];
                    sprintf(fname, "%s\\frame%d_buffer%d.bin", pDir, m_cFrameNumber, BufferType);
                    f = fopen(fname, "wb");
                    fwrite(m_pBuffers[BufferType], m_cBuffersSize[BufferType], 1, f);
                    if (f)
                    {
                        fclose(f);
                        f = NULL;
                    }

                    sprintf(fname, "%s\\frame%d_buffer%d.log", pDir, m_cFrameNumber, BufferType);

                    if (m_Profile == JPEG_VLD)
                    {
                        f = fopen(fname, "a");
                        dumpJPEGBuffer(BufferType, m_pBuffers[BufferType]);
                    }

                    if (m_Profile == MPEG2_VLD || m_Profile == VC1_VLD)
                    {
                        if (BufferType == DXVA2_PictureParametersBufferType)
                        {
                            f = fopen(fname, "wt");
                            dumpMPEG2PictureParameters(m_pBuffers[BufferType]);
                        }
                    }
                    if (m_Profile == VC1_VLD)
                    {
                        if (BufferType == DXVA2_VC1PictureParametersExtBufferType)
                        {
                            f = fopen(fname, "wt");
                            dumpVC1PictureParametersExt(m_pBuffers[BufferType]);
                        }
                    }
                    if (m_Profile == MPEG4_VLD)
                    {
                        if (BufferType == DXVA2_PictureParametersBufferType)
                        {
                            f = fopen(fname, "wt");
                            dumpMPEG4PictureParameters(m_pBuffers[BufferType]);
                        }
                    }
                    if ((m_Profile == MPEG2_VLD || m_Profile == MPEG4_VLD) &&
                        BufferType == DXVA2_SliceControlBufferType)
                    {
                        f = fopen(fname, "wt");
                        DXVA_SliceInfo *pSlice = (DXVA_SliceInfo*)m_pBuffers[BufferType];
                        int i;
                        for (i = 0; i < m_cBuffersSize[BufferType]/sizeof(DXVA_SliceInfo); i++)
                        {
                            logi(i);
                            logi(pSlice[i].wHorizontalPosition);
                            logi(pSlice[i].wVerticalPosition);
                            logi(pSlice[i].dwSliceBitsInBuffer);
                            logi(pSlice[i].dwSliceDataLocation);
                            logi(pSlice[i].bStartCodeBitOffset);
                            logi(pSlice[i].bReservedBits);
                            logi(pSlice[i].wMBbitOffset);
                            logi(pSlice[i].wNumberMBsInSlice);
                            logi(pSlice[i].wQuantizerScaleCode);
                            logi(pSlice[i].wBadSliceChopping);
                        }
                    }

                    if (m_Profile == H264_VLD)
                    {
                        switch(BufferType)
                        {
                        /*case DXVA_PICTURE_DECODE_BUFFER_SVC:
                            f = fopen(fname, "wt");
                            dumpH264SVCPictureParameters(m_pBuffers[BufferType]);
                            break;

                        case DXVA_SLICE_CONTROL_BUFFER_SVC:
                            f = fopen(fname, "wt");
                            break;*/

                        case DXVA2_PictureParametersBufferType:
                            f = fopen(fname, "wt");
                            if (m_guid == sDXVA_ModeH264_VLD_Multiview_NoFGT || m_guid == sDXVA_ModeH264_VLD_Stereo_NoFGT ||
                                m_guid == sDXVA_ModeH264_VLD_Stereo_Progressive_NoFGT ||

                                m_cBuffersSize[BufferType] == sizeof(DXVA_PicParams_H264_MVC))
                            {
                                dumpH264PictureParameters_MS_MVC(m_pBuffers[BufferType]);
                            }
                            else if (m_guid == sDXVA_ModeH264_VLD_SVC_Scalable_High || m_guid == sDXVA_ModeH264_VLD_SVC_Scalable_Baseline ||
                                m_guid == sDXVA_ModeH264_VLD_SVC_Scalable_Constrained_Baseline ||
                                m_guid == sDXVA_ModeH264_VLD_SVC_Scalable_Constrained_High ||
                                m_cBuffersSize[BufferType] == sizeof(DXVA_PicParams_H264_SVC))
                            {
                                dumpH264SVCPictureParameters(m_pBuffers[BufferType]);
                            }
                            else
                            {
                                dumpH264PictureParameters(m_pBuffers[BufferType]);
                            }
                            break;

                        case DXVA_MVCPictureParametersExtBufferType - 1:
                            f = fopen(fname, "wt");
                            dumpH264MVCPictureParameters(m_pBuffers[BufferType]);
                            break;

                        case DXVA2_SliceControlBufferType:
                            f = fopen(fname, "wt");
                            bool isLong = m_Config.ConfigBitstreamRaw == 1 || m_Config.ConfigBitstreamRaw == 3 || m_Config.ConfigBitstreamRaw == 5;

                            bool isSVCBufSize = isLong ? (m_cBuffersSize[BufferType] == sizeof(DXVA_Slice_H264_SVC_Long)) : 
                                (m_cBuffersSize[BufferType] == sizeof(DXVA_Slice_H264_SVC_Short));

                            if (m_guid == sDXVA_ModeH264_VLD_SVC_Scalable_High || m_guid == sDXVA_ModeH264_VLD_SVC_Scalable_Baseline ||
                                m_guid == sDXVA_ModeH264_VLD_SVC_Scalable_Constrained_High ||
                                m_guid == sDXVA_ModeH264_VLD_SVC_Scalable_Constrained_Baseline ||
                                isSVCBufSize)
                            {
                                dumpH264SVCSliceParameters(m_pBuffers[BufferType], m_cBuffersSize[BufferType], isLong);
                            }
                            else
                            {
                                dumpH264SliceParameters(m_pBuffers[BufferType], m_cBuffersSize[BufferType], isLong);
                            }
                            break;

                        }; // switch
                    } // if (m_Profile == H264_VLD)

                    if (f)
                    {
                        fclose(f);
                        f = NULL;
                    }
                }
            }
        }
    }

private:
    GUID                        m_guid;
    ConfigPictureDecodeType     m_Config;
    void    *m_pBuffers[32];
    int     m_cBuffersSize[32];

    SurfaceType          *m_surface;
    UMC::VideoAccelerationProfile    m_Profile;
    int     m_cFrameNumber;
};

Dumper<DXVA2_DecodeBufferDesc, DXVA2_ConfigPictureDecode, IDirect3DSurface9> dumpDx9;
Dumper<D3D11_VIDEO_DECODER_BUFFER_DESC, D3D11_VIDEO_DECODER_CONFIG, IDirect3DSurface9> dumpDx11;

static GUID g_guids[] =
{
    sDXVA2_ModeMPEG2_VLD,
    sDXVA2_ModeH264_VLD_NoFGT,

    sDXVA2_Intel_EagleLake_ModeH264_VLD_NoFGT,

    sDXVA_Intel_ModeH264_VLD_MVC,

    sDXVA2_ModeVC1_VLD,
    sDXVA2_ModeVC1_MoComp,
    sDXVA2_Intel_ModeVC1_D_Super,

    sDXVA2_Intel_IVB_ModeJPEG_VLD_NoFGT,
    sDXVA_Intel_ModeVP8_VLD,

    sDXVA_ModeH264_VLD_SVC_Scalable_High,
    sDXVA_ModeH264_VLD_SVC_Scalable_Baseline,
    sDXVA_ModeH264_VLD_SVC_Scalable_Constrained_High,
    sDXVA_ModeH264_VLD_SVC_Scalable_Constrained_Baseline,

    sDXVA_ModeH264_VLD_Multiview_NoFGT,
    sDXVA_ModeH264_VLD_Stereo_NoFGT,
    sDXVA_ModeH264_VLD_Stereo_Progressive_NoFGT,

    DXVA2_Intel_Auxiliary_Device,
    DXVADDI_Intel_Decode_PrivateData_Report,

    DXVA_ModeHEVC_VLD_MainProfile
};

static DXVA2_ConfigPictureDecode configs[]=
{
    {DXVA_NoEncrypt, DXVA_NoEncrypt, DXVA_NoEncrypt, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {DXVA_NoEncrypt, DXVA_NoEncrypt, DXVA_NoEncrypt, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},

    {sDXVA2_ModeH264_VLD_NoFGT, DXVA_NoEncrypt, DXVA_NoEncrypt, 2, 5000, 5000, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {sDXVA_ModeH264_VLD_SVC_Scalable_Constrained_High, DXVA_NoEncrypt, DXVA_NoEncrypt, 2, 5000, 5000, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {sDXVA_ModeH264_VLD_SVC_Scalable_Constrained_Baseline, DXVA_NoEncrypt, DXVA_NoEncrypt, 2, 5000, 5000, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
};

char pDir[256] = ".\\dxva2_log";

void SetDumpingDirectory(char *dir)
{
    strcpy(pDir, dir);
}


///////////////////////////////////////////////////////////////////////

bool f_init = false;
FILE *f = NULL;

///////////////////////////////////////////////////////////////////////

using namespace std;

void RemoveDir(wstring dir_name)
{
    HANDLE hFind;
    WIN32_FIND_DATA FindFileData; 

    ZeroMemory(&FindFileData,sizeof(FindFileData));

    if (dir_name[dir_name.length()-1]!='\\')
        dir_name += L"\\";

    wstring filter = dir_name + wstring(L"*.*");

    hFind = FindFirstFile(filter.c_str(),&FindFileData);

    if (hFind != NULL && hFind != INVALID_HANDLE_VALUE)
    { 
        while (FindNextFile(hFind, &FindFileData) != 0)
        {
            if (wcscmp(FindFileData.cFileName,L".")!=0 &&
                wcscmp(FindFileData.cFileName,L"..")!=0)
            {
                if (FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                {
                    RemoveDir(dir_name + wstring(FindFileData.cFileName));
                }else{
                    DeleteFile(wstring(dir_name + wstring(FindFileData.cFileName)).c_str());
                }
            }
        }
        FindClose(hFind);
    }

    RemoveDirectory(dir_name.c_str());
}

////////////////////////////////////////////////////////////////////

IDirectXVideoDecoder *CreateDXVASpy(IDirectXVideoDecoder *pObject)
{
    LLLCREATE;
    IDirectXVideoDecoder * decoder = new CSpyVideoDecoder(pObject);
    decoder->AddRef();
    return decoder;
}

IDirectXVideoDecoderService *CreateDXVASpy(IDirectXVideoDecoderService *pObject)
{
    LLLCREATE;
    IDirectXVideoDecoderService * decoder = new CSpyVideoDecoderService(pObject);
    decoder->AddRef();
    return decoder;
}

IDirectXVideoProcessorService *CreateDXVASpy(IDirectXVideoProcessorService *pObject)
{
    LLLCREATE;
    IDirectXVideoProcessorService * decoder = new CSpyVideoProcessorService(pObject);
    decoder->AddRef();
    return decoder;
}

IDirect3DDeviceManager9 *CreateDXVASpy(IDirect3DDeviceManager9* pObject)
{
    wchar_t wDir[256];
    memset(wDir, 0, sizeof(wDir));
    int i;
    for (i = 0; pDir[i]; i++) wDir[i] = pDir[i];
    RemoveDir(wDir);
    LLLCREATE;
    IDirect3DDeviceManager9 * decoder = new CSpyDirect3DDeviceManager9(pObject);
    decoder->AddRef();
    return decoder;
}

IDirectXVideoProcessor *CreateDXVASpy(IDirectXVideoProcessor* pObject)
{
    LLLCREATE;
    IDirectXVideoProcessor * decoder = new CSpyVideoProcessor(pObject);
    decoder->AddRef();
    return decoder;
}

ID3D11Device *CreateDXVASpy(ID3D11Device* pObject)
{
    wchar_t wDir[256];
    memset(wDir, 0, sizeof(wDir));
    int i;
    for (i = 0; pDir[i]; i++) wDir[i] = pDir[i];
    RemoveDir(wDir);
    LLLCREATE;
    ID3D11Device * decoder = new SpyID3D11Device(pObject);
    decoder->AddRef();
    return decoder;
}

CSpyDirect3DDeviceManager9::CSpyDirect3DDeviceManager9(IDirect3DDeviceManager9 *pObject) :
      m_pObject(pObject)
      , m_nRefCount(0)
  {
      LLLD3DDEV;
  };

HRESULT STDMETHODCALLTYPE CSpyDirect3DDeviceManager9::ResetDevice(
    /* [in] */
    __in  IDirect3DDevice9 *pDevice,
    /* [in] */
    __in  UINT resetToken) 
{ 
    LLLD3DDEV;
    return m_pObject->ResetDevice(pDevice, resetToken); 
}

HRESULT STDMETHODCALLTYPE CSpyDirect3DDeviceManager9::OpenDeviceHandle(
    /* [out] */
    __out  HANDLE *phDevice) 
{ 
    LLLD3DDEV;
    return m_pObject->OpenDeviceHandle(phDevice); 
}

HRESULT STDMETHODCALLTYPE CSpyDirect3DDeviceManager9::CloseDeviceHandle(
    /* [in] */
    __in  HANDLE hDevice) 
{ 
    LLLD3DDEV;
    return m_pObject->CloseDeviceHandle(hDevice); 
}

HRESULT STDMETHODCALLTYPE CSpyDirect3DDeviceManager9::TestDevice(
    /* [in] */
    __in  HANDLE hDevice) 
{ 
    LLLD3DDEV;
    return m_pObject->TestDevice(hDevice); 
}

HRESULT STDMETHODCALLTYPE CSpyDirect3DDeviceManager9::LockDevice(
    /* [in] */
    __in  HANDLE hDevice,
    /* [out] */
    __deref_out  IDirect3DDevice9 **ppDevice,
    /* [in] */
    __in  BOOL fBlock) 
{ 
    LLLD3DDEV;
    return m_pObject->LockDevice(hDevice, ppDevice, fBlock); 
}

HRESULT STDMETHODCALLTYPE CSpyDirect3DDeviceManager9::UnlockDevice(
    /* [in] */
    __in  HANDLE hDevice,
    /* [in] */
    __in  BOOL fSaveState) 
{ 
    LLLD3DDEV;
    return m_pObject->UnlockDevice(hDevice, fSaveState); 
}

HRESULT CSpyDirect3DDeviceManager9::GetVideoService(
    /* [in] */
    __in  HANDLE hDevice,
    /* [in] */
    __in  REFIID riid,
    /* [out] */
    __deref_out  void **ppService)
{
    LLLD3DDEV;
    LOG_GUID(riid);
    HRESULT hr = m_pObject->GetVideoService(hDevice, riid, ppService);
    if (SUCCEEDED(hr) && riid == __uuidof(IDirectXVideoDecoderService))
    {
        *ppService = CreateDXVASpy((IDirectXVideoDecoderService*)*ppService);
    }else if (SUCCEEDED(hr) && riid == __uuidof(IDirectXVideoProcessorService))
    {
        *ppService = CreateDXVASpy((IDirectXVideoProcessorService*)*ppService);
    }

    return hr;
}

HRESULT CSpyDirect3DDeviceManager9::QueryInterface(const IID &id, void **pp)
{
    LLLD3DDEV;

    if (id == __uuidof(IDirect3DDeviceManager9))
    {
        *(IDirect3DDeviceManager9**)pp = this;
        m_pObject->AddRef();
        return S_OK;
    }
    return m_pObject->QueryInterface(id, pp);
}
///////////////////////////////////////////////////////////////////////
CSpyVideoProcessorService::CSpyVideoProcessorService(IDirectXVideoProcessorService *pService):
m_pObject(pService)
, m_nRefCount(0)
{
    LLLVPROCSRV;
}

HRESULT STDMETHODCALLTYPE CSpyVideoProcessorService:: RegisterVideoProcessorSoftwareDevice( 
        /* [in] */ 
        __in  void *pCallbacks) 
{
    LLLVPROCSRV;
    return m_pObject->RegisterVideoProcessorSoftwareDevice(pCallbacks);
}
    
HRESULT STDMETHODCALLTYPE CSpyVideoProcessorService:: GetVideoProcessorDeviceGuids( 
    /* [in] */ 
    __in  const DXVA2_VideoDesc *pVideoDesc,
    /* [out] */ 
    __out  UINT *pCount,
    /* [size_is][unique][out] */ 
    __deref_out_ecount_opt(*pCount)  GUID **pGuids) 
{
    LLLVPROCSRV;
    return m_pObject->GetVideoProcessorDeviceGuids(pVideoDesc, pCount, pGuids);
}

HRESULT STDMETHODCALLTYPE CSpyVideoProcessorService:: GetVideoProcessorRenderTargets( 
    /* [in] */ 
    __in  REFGUID VideoProcDeviceGuid,
    /* [in] */ 
    __in  const DXVA2_VideoDesc *pVideoDesc,
    /* [out] */ 
    __out  UINT *pCount,
    /* [size_is][unique][out] */ 
    __deref_out_ecount_opt(*pCount)  D3DFORMAT **pFormats) 
{
    LLLVPROCSRV;
    return m_pObject->GetVideoProcessorRenderTargets(VideoProcDeviceGuid, pVideoDesc, pCount, pFormats);
}

HRESULT STDMETHODCALLTYPE CSpyVideoProcessorService:: GetVideoProcessorSubStreamFormats( 
    /* [in] */ 
    __in  REFGUID VideoProcDeviceGuid,
    /* [in] */ 
    __in  const DXVA2_VideoDesc *pVideoDesc,
    /* [in] */ 
    __in  D3DFORMAT RenderTargetFormat,
    /* [out] */ 
    __out  UINT *pCount,
    /* [size_is][unique][out] */ 
    __deref_out_ecount_opt(*pCount)  D3DFORMAT **pFormats) 
{
    LLLVPROCSRV;
    return m_pObject->GetVideoProcessorSubStreamFormats(VideoProcDeviceGuid, pVideoDesc, RenderTargetFormat, pCount, pFormats);
}

HRESULT STDMETHODCALLTYPE CSpyVideoProcessorService:: GetVideoProcessorCaps( 
    /* [in] */ 
    __in  REFGUID VideoProcDeviceGuid,
    /* [in] */ 
    __in  const DXVA2_VideoDesc *pVideoDesc,
    /* [in] */ 
    __in  D3DFORMAT RenderTargetFormat,
    /* [out] */ 
    __out  DXVA2_VideoProcessorCaps *pCaps)
{
    LLLVPROCSRV;
    return m_pObject->GetVideoProcessorCaps(VideoProcDeviceGuid, pVideoDesc, RenderTargetFormat, pCaps);
}

HRESULT STDMETHODCALLTYPE CSpyVideoProcessorService:: GetProcAmpRange( 
    /* [in] */ 
    __in  REFGUID VideoProcDeviceGuid,
    /* [in] */ 
    __in  const DXVA2_VideoDesc *pVideoDesc,
    /* [in] */ 
    __in  D3DFORMAT RenderTargetFormat,
    /* [in] */ 
    __in  UINT ProcAmpCap,
    /* [out] */ 
    __out  DXVA2_ValueRange *pRange) 
{
    LLLVPROCSRV;
    return m_pObject->GetProcAmpRange(VideoProcDeviceGuid, pVideoDesc, RenderTargetFormat, ProcAmpCap, pRange);
}

HRESULT STDMETHODCALLTYPE CSpyVideoProcessorService:: GetFilterPropertyRange( 
    /* [in] */ 
    __in  REFGUID VideoProcDeviceGuid,
    /* [in] */ 
    __in  const DXVA2_VideoDesc *pVideoDesc,
    /* [in] */ 
    __in  D3DFORMAT RenderTargetFormat,
    /* [in] */ 
    __in  UINT FilterSetting,
    /* [out] */ 
    __out  DXVA2_ValueRange *pRange) 
{
    LLLVPROCSRV;
    return m_pObject->GetFilterPropertyRange(VideoProcDeviceGuid, pVideoDesc, RenderTargetFormat, FilterSetting, pRange);
}

HRESULT STDMETHODCALLTYPE CSpyVideoProcessorService:: CreateVideoProcessor( 
    /* [in] */ 
    __in  REFGUID VideoProcDeviceGuid,
    /* [in] */ 
    __in  const DXVA2_VideoDesc *pVideoDesc,
    /* [in] */ 
    __in  D3DFORMAT RenderTargetFormat,
    /* [in] */ 
    __in  UINT MaxNumSubStreams,
    /* [out] */ 
    __deref_out  IDirectXVideoProcessor **ppVidProcess) 
{
    LLLVPROCSRV;
     
    HRESULT hr = m_pObject->CreateVideoProcessor(VideoProcDeviceGuid, pVideoDesc, RenderTargetFormat, MaxNumSubStreams, ppVidProcess);
    if (SUCCEEDED(hr))
    {
        *ppVidProcess = CreateDXVASpy(*ppVidProcess);
    }
    return hr;
}

HRESULT STDMETHODCALLTYPE CSpyVideoProcessorService::CreateSurface(
    /* [in] */
    __in  UINT Width,
    /* [in] */
    __in  UINT Height,
    /* [in] */
    __in  UINT BackBuffers,
    /* [in] */
    __in  D3DFORMAT Format,
    /* [in] */
    __in  D3DPOOL Pool,
    /* [in] */
    __in  DWORD Usage,
    /* [in] */
    __in  DWORD DxvaType,
    /* [size_is][out] */
    __out_ecount(BackBuffers+1)  IDirect3DSurface9 **ppSurface,
    /* [out][in] */
    __inout_opt  HANDLE *pSharedHandle)
{
    LLLVPROCSRV;

    OPEN_SPEC_FILE;
    logi(Width);
    logi(Height);
    logi(BackBuffers);
    logi(Format);
    logi(Pool);
    logi(Usage);
    logi(DxvaType);
    CLOSE_SPEC_FILE;

    return m_pObject->CreateSurface(Width, Height, BackBuffers, Format, Pool, Usage, DxvaType, ppSurface, pSharedHandle);
}
///////////////////////////////////////////////////////////////////////
CSpyVideoProcessor::CSpyVideoProcessor(IDirectXVideoProcessor *pObject) 
: m_pObject(pObject)
, m_nRefCount(0)
{
    LLLVPROC;
}

HRESULT STDMETHODCALLTYPE CSpyVideoProcessor::GetVideoProcessorService( 
    /* [out] */ 
    __deref_out  IDirectXVideoProcessorService **ppService) 
{
    LLLVPROC;
    return m_pObject->GetVideoProcessorService(ppService);
}

HRESULT STDMETHODCALLTYPE CSpyVideoProcessor ::GetCreationParameters( 
    /* [out] */ 
    __out_opt  GUID *pDeviceGuid,
    /* [out] */ 
    __out_opt  DXVA2_VideoDesc *pVideoDesc,
    /* [out] */ 
    __out_opt  D3DFORMAT *pRenderTargetFormat,
    /* [out] */ 
    __out_opt  UINT *pMaxNumSubStreams) 
{
    LLLVPROC;
    return m_pObject->GetCreationParameters(pDeviceGuid, pVideoDesc, pRenderTargetFormat, pMaxNumSubStreams);
}

HRESULT STDMETHODCALLTYPE CSpyVideoProcessor ::GetVideoProcessorCaps( 
    /* [out] */ 
    __out  DXVA2_VideoProcessorCaps *pCaps) 
{
    LLLVPROC;
    return m_pObject->GetVideoProcessorCaps(pCaps);
}

HRESULT STDMETHODCALLTYPE CSpyVideoProcessor ::GetProcAmpRange( 
    /* [in] */ 
    __in  UINT ProcAmpCap,
    /* [out] */ 
    __out  DXVA2_ValueRange *pRange) 
{
    LLLVPROC;
    return m_pObject->GetProcAmpRange(ProcAmpCap, pRange);
}

HRESULT STDMETHODCALLTYPE CSpyVideoProcessor ::GetFilterPropertyRange( 
    /* [in] */ 
    __in  UINT FilterSetting,
    /* [out] */ 
    __out  DXVA2_ValueRange *pRange) 
{
    LLLVPROC;
    return m_pObject->GetFilterPropertyRange(FilterSetting, pRange);
}

HRESULT STDMETHODCALLTYPE CSpyVideoProcessor ::VideoProcessBlt( 
    /* [in] */ 
    __in  IDirect3DSurface9 *pRenderTarget,
    /* [in] */ 
    __in  const DXVA2_VideoProcessBltParams *pBltParams,
    /* [size_is][in] */ 
    __in_ecount(NumSamples)  const DXVA2_VideoSample *pSamples,
    /* [in] */ 
    __in  UINT NumSamples,
    /* [out] */ 
    __inout_opt  HANDLE *pHandleComplete) 
{
    LLLVPROC;
    HRESULT hr = m_pObject->VideoProcessBlt(pRenderTarget, pBltParams, pSamples, NumSamples, pHandleComplete);

    dumpD3DSurface(pSamples->SrcSurface);

    return hr;
}

 
///////////////////////////////////////////////////////////////////////
HRESULT STDMETHODCALLTYPE CSpyVideoDecoderService::GetDecoderDeviceGuids(
    /* [out] */
    __out  UINT *pCount,
    /* [size_is][unique][out] */
    __deref_out_ecount_opt(*pCount)  GUID **pGuids)
{
    LLLVDECSRV;

    HRESULT hr = S_OK;

    if (dumpDx9.GetSkipExecute())
    {
        *pGuids = (GUID *)CoTaskMemAlloc(sizeof(g_guids));
        *pCount = sizeof(g_guids)/sizeof(g_guids[0]);
        memcpy(*pGuids, g_guids, sizeof(g_guids));
        return hr; 
    }

    
    hr = m_pObject->GetDecoderDeviceGuids(pCount, pGuids);

    logi(*pCount);
    int i;
    for (i = 0; i < *pCount; i++)
    {
        LOG_GUID((*pGuids)[i]);
    }
    flush;
    return hr;
}

HRESULT STDMETHODCALLTYPE CSpyVideoDecoderService::GetDecoderRenderTargets(
    /* [in] */
    __in  REFGUID Guid,
    /* [out] */
    __out  UINT *pCount,
    /* [size_is][unique][out] */
    __deref_out_ecount_opt(*pCount)  D3DFORMAT **pFormats)
{
    LLLVDECSRV;
    HRESULT hr = S_OK;
    if (dumpDx9.GetSkipExecute())
    {
        hr = m_pObject->GetDecoderRenderTargets(sDXVA2_ModeH264_VLD_NoFGT, pCount, pFormats);
        if (FAILED(hr))
            hr = m_pObject->GetDecoderRenderTargets(sDXVA2_Intel_EagleLake_ModeH264_VLD_NoFGT, pCount, pFormats);
        return hr;
    }

    LOG_GUID(Guid);
    hr = m_pObject->GetDecoderRenderTargets(Guid, pCount, pFormats);
    logi(*pCount);
    int i;
    for (i = 0; i < *pCount; i++)
    {
        logi((*pFormats)[i]);
        logcc((*pFormats)[i]);
    }
    flush;
    return hr;
}

HRESULT STDMETHODCALLTYPE CSpyVideoDecoderService::GetDecoderConfigurations(
    /* [in] */
    __in  REFGUID Guid,
    /* [in] */
    __in  const DXVA2_VideoDesc *pVideoDesc,
    /* [in] */
    __reserved  void *pReserved,
    /* [out] */
    __out  UINT *pCount,
    /* [size_is][unique][out] */
    __deref_out_ecount_opt(*pCount)  DXVA2_ConfigPictureDecode **ppConfigs)
{
    LLLVDECSRV;

    OPEN_SPEC_FILE;
    LOG_GUID(Guid);
    if (dumpDx9.GetSkipExecute())
    {
        *ppConfigs = (DXVA2_ConfigPictureDecode *)CoTaskMemAlloc(sizeof(configs));
        *pCount = sizeof(configs)/sizeof(configs[0]);
        memcpy(*ppConfigs, configs, sizeof(configs));
        return S_OK;
    }

    dumpVideoDesc(pVideoDesc);

    HRESULT hr = m_pObject->GetDecoderConfigurations(Guid, pVideoDesc, pReserved, pCount, ppConfigs);

    logi(*pCount);
    //int i;
    /*for (i = 0; i < *pCount; i++)
    {
    logs("-----------------------------------\n");
    dumpConfig(ppConfigs[i]);
    }*/
    CLOSE_SPEC_FILE;

    return hr;
}

HRESULT STDMETHODCALLTYPE CSpyVideoDecoderService::CreateVideoDecoder(
    /* [in] */
    __in  REFGUID Guid,
    /* [in] */
    __in  const DXVA2_VideoDesc *pVideoDesc,
    /* [in] */
    __in  const DXVA2_ConfigPictureDecode *pConfig,
    /* [size_is][in] */
    __in_ecount(NumRenderTargets)  IDirect3DSurface9 **ppDecoderRenderTargets,
    /* [in] */
    __in  UINT NumRenderTargets,
    /* [out] */
    __deref_out  IDirectXVideoDecoder **ppDecode)
{
    LLLVDECSRV;

    OPEN_SPEC_FILE;
    LOG_GUID(Guid);
    dumpVideoDesc(pVideoDesc);
    dumpConfig(pConfig);

    HRESULT hr = S_OK;
    if (!dumpDx9.GetSkipExecute())
    {
        m_pObject->CreateVideoDecoder(Guid, pVideoDesc, pConfig, ppDecoderRenderTargets, NumRenderTargets, ppDecode);
    }

    if (SUCCEEDED(hr))
    {
        //__asm int 3;
        CSpyVideoDecoder *pSpy = new CSpyVideoDecoder(*ppDecode);
        *ppDecode = pSpy;
        
        dumpDx9.Init(Guid, pConfig);
    }

    CLOSE_SPEC_FILE;
    return hr;
}

HRESULT STDMETHODCALLTYPE CSpyVideoDecoderService::CreateSurface(
    /* [in] */
    __in  UINT Width,
    /* [in] */
    __in  UINT Height,
    /* [in] */
    __in  UINT BackBuffers,
    /* [in] */
    __in  D3DFORMAT Format,
    /* [in] */
    __in  D3DPOOL Pool,
    /* [in] */
    __in  DWORD Usage,
    /* [in] */
    __in  DWORD DxvaType,
    /* [size_is][out] */
    __out_ecount(BackBuffers+1)  IDirect3DSurface9 **ppSurface,
    /* [out][in] */
    __inout_opt  HANDLE *pSharedHandle)
{
    LLLVDECSRV;

    OPEN_SPEC_FILE;
    logi(Width);
    logi(Height);
    logi(BackBuffers);
    logi(Format);
    logi(Pool);
    logi(Usage);
    logi(DxvaType);
    CLOSE_SPEC_FILE;

    return m_pObject->CreateSurface(Width, Height, BackBuffers, Format, Pool, Usage, DxvaType, ppSurface, pSharedHandle);
}

///////////////////////////////////////////////////////////////////////

HRESULT STDMETHODCALLTYPE CSpyVideoDecoder::GetVideoDecoderService(
    /* [out] */
    __deref_out  IDirectXVideoDecoderService **ppService)
{
    LLLVDEC;
    return m_pObject->GetVideoDecoderService(ppService);
}

HRESULT STDMETHODCALLTYPE CSpyVideoDecoder::GetCreationParameters(
    /* [out] */
    __out_opt  GUID *pDeviceGuid,
    /* [out] */
    __out_opt  DXVA2_VideoDesc *pVideoDesc,
    /* [out] */
    __out_opt  DXVA2_ConfigPictureDecode *pConfig,
    /* [size_is][unique][out] */
    __out_ecount(*pNumSurfaces)  IDirect3DSurface9 ***pDecoderRenderTargets,
    /* [out] */
    __out_opt  UINT *pNumSurfaces)
{
    LLLVDEC;
    return m_pObject->GetCreationParameters(pDeviceGuid, pVideoDesc, pConfig, pDecoderRenderTargets, pNumSurfaces);
}

HRESULT STDMETHODCALLTYPE CSpyVideoDecoder::GetBuffer(
    /* [in] */
    __in  UINT BufferType,
    /* [out] */
    __out  void **ppBuffer,
    /* [out] */
    __out  UINT *pBufferSize)
{
    LLLVDEC;
    logi(BufferType);
    HRESULT hr;

    if (dumpDx9.GetSkipExecute())
    {
        dumpDx9.GetBuffer(BufferType, ppBuffer, pBufferSize);
        return S_OK;
    }

    hr = m_pObject->GetBuffer(BufferType, ppBuffer, pBufferSize);
    logi(*pBufferSize);
    dumpDx9.GetBuffer(BufferType, ppBuffer, pBufferSize);
    logi_flush(hr);
    return hr;
}

HRESULT STDMETHODCALLTYPE CSpyVideoDecoder::ReleaseBuffer(
    /* [in] */
    __in  UINT BufferType)
{
    LLLVDEC;
    logi_flush(BufferType);
    if (dumpDx9.GetSkipExecute()) return S_OK;
    return m_pObject->ReleaseBuffer(BufferType);
}

HRESULT STDMETHODCALLTYPE CSpyVideoDecoder::BeginFrame(
    /* [in] */
    __in  IDirect3DSurface9 *pRenderTarget,
    /* [in] */
    __in_opt  void *pvPVPData)
{
    LLLVDEC;
    logi(pRenderTarget);
    logi(pvPVPData);

    dumpDx9.BeginFrame(pRenderTarget);

    if (dumpDx9.GetSkipExecute())
    {
        return S_OK;
    }

    HRESULT hr = m_pObject->BeginFrame(pRenderTarget, pvPVPData);
    logi_flush(hr);
    return hr;
}

HRESULT STDMETHODCALLTYPE CSpyVideoDecoder::EndFrame(
    /* [out] */
    __inout_opt  HANDLE *pHandleComplete)
{
    LLLVDEC;
    if (dumpDx9.GetSkipExecute())
    {
        return S_OK;
    }

    HRESULT hr = m_pObject->EndFrame(pHandleComplete);    
    logi_flush(hr);
    dumpDx9.EndFrame();
    return hr;
}

//////////////////////////////////////////////////////////////////////////////////

HRESULT STDMETHODCALLTYPE CSpyVideoDecoder::Execute(
    /* [in] */
    __in  const DXVA2_DecodeExecuteParams *pExecuteParams)
{
    dumpDx9.Execute(pExecuteParams->NumCompBuffers, pExecuteParams->pCompressedBuffers);

    if (dumpDx9.GetSkipExecute())
    {
        if (pExecuteParams->NumCompBuffers)
        {
            LLLVDEC;
            logs("Execute: Skip\n");
        }
        return S_OK;
    }

    if (pExecuteParams->NumCompBuffers)
        LLLVDEC;
    HRESULT hr = m_pObject->Execute(pExecuteParams);
    if (pExecuteParams->NumCompBuffers)
        logi_flush(hr);
    return hr;
}


#define BUFFER_D3DDDIFMT_INTEL_JPEGDECODE_PPSDATA       (D3DDDIFMT_INTEL_JPEGDECODE_PPSDATA - 1)
#define BUFFER_D3DDDIFMT_INTEL_JPEGDECODE_QUANTDATA     (D3DDDIFMT_INTEL_JPEGDECODE_QUANTDATA - 1)
#define BUFFER_D3DDDIFMT_INTEL_JPEGDECODE_HUFFTBLDATA   (D3DDDIFMT_INTEL_JPEGDECODE_HUFFTBLDATA - 1)
#define BUFFER_D3DDDIFMT_INTEL_JPEGDECODE_SCANDATA      (D3DDDIFMT_INTEL_JPEGDECODE_SCANDATA - 1)     
#define BUFFER_D3DDDIFMT_BITSTREAMDATA                  (D3DDDIFMT_BITSTREAMDATA - 1)

void dumpJPEGBuffer(int BufferType, void *buffer)
{
    switch(BufferType)
    {
    case BUFFER_D3DDDIFMT_INTEL_JPEGDECODE_PPSDATA: 
        {
        JPEG_DECODE_PICTURE_PARAMETERS * ptr = (JPEG_DECODE_PICTURE_PARAMETERS *)buffer;
        logi(ptr->FrameWidth);
        logi(ptr->FrameHeight);
        logi(ptr->NumCompInFrame);

        for (int i = 0; i < 4; i++)
        {
            logi(ptr->ComponentIdentifier[i]);
        }

        for (int i = 0; i < 4; i++)
        {
            logi(ptr->QuantTableSelector[i]);
        }

        logi(ptr->ChromaType);
        logi(ptr->Rotation);
        logi(ptr->TotalScans);
        }
        break;
    case BUFFER_D3DDDIFMT_INTEL_JPEGDECODE_HUFFTBLDATA: 
        {
            JPEG_DECODE_HUFFMAN_TABLE * ptr = (JPEG_DECODE_HUFFMAN_TABLE *)buffer;
            logi(ptr->TableClass);
            logi(ptr->TableIndex);

            for (int i = 0; i < sizeof(ptr->BITS); i++)
            {
                logi(ptr->BITS[i]);
            }

            for (int i = 0; i < sizeof(ptr->HUFFVAL); i++)
            {
                logi(ptr->HUFFVAL[i]);
            }
        }
        break;

    case BUFFER_D3DDDIFMT_INTEL_JPEGDECODE_QUANTDATA: 
        {
            JPEG_DECODE_QM_TABLE * ptr = (JPEG_DECODE_QM_TABLE*)buffer;
            logi(ptr->TableIndex);
            logi(ptr->Precision);

            for (int i = 0; i < sizeof(ptr->Qm); i++)
            {
                logi(ptr->Qm[i]);
            }
        }
        break;

    case BUFFER_D3DDDIFMT_INTEL_JPEGDECODE_SCANDATA:
        {
        JPEG_DECODE_SCAN_PARAMETER * ptr = (JPEG_DECODE_SCAN_PARAMETER *)buffer;
        logi(ptr->NumComponents);

        for (int i = 0; i < 4; i++)
        {
            logi(ptr->ComponentSelector[i]);
        }

        for (int i = 0; i < 4; i++)
        {
            logi(ptr->DcHuffTblSelector[i]);
        }

        for (int i = 0; i < 4; i++)
        {
            logi(ptr->AcHuffTblSelector[i]);
        }

        logi(ptr->RestartInterval);
        logi(ptr->MCUCount);
        logi(ptr->ScanHoriPosition);
        logi(ptr->ScanVertPosition);
        logi(ptr->DataOffset);
        logi(ptr->DataLength);
        }
        break;
    };
}

void dumpH264MVCPictureParameters(void *pPictureParameters)
{
    DXVA_Intel_PicParams_MVC *pParams = (DXVA_Intel_PicParams_MVC*)pPictureParameters;

    logi(pParams->CurrViewID);
    logi(pParams->anchor_pic_flag);

    logi(pParams->inter_view_flag);
    logi(pParams->NumInterViewRefsL0);
    logi(pParams->NumInterViewRefsL1);

    logi(pParams->bPicFlags);
    logi(pParams->SwitchToAVC);
    logi(pParams->Reserved7Bits);

    logi(pParams->Reserved8Bits);

    for (int i = 0; i < 16; i++)
    {
        logi(pParams->ViewIDList[i]);
    }

    for (int i = 0; i < 16; i++)
    {
        logi(pParams->InterViewRefList[0][i]);
        logi(pParams->InterViewRefList[1][i]);
    }
}

void dumpH264SVCPictureParameters(void *pPictureParameters)
{
    DXVA_PicParams_H264_SVC *pParams = (DXVA_PicParams_H264_SVC*)pPictureParameters;

    for (int i = 0; i < 16; i++)
    {
        logi(pParams->RefBasePicFlag[i]);
    }

    logi(pParams->inter_layer_deblocking_filter_control_present_flag);
    logi(pParams->chroma_phase_x_plus_flag);
    logi(pParams->seq_ref_layer_chroma_phase_x_plus1_flag);
    logi(pParams->adaptive_tcoeff_level_prediction_flag);
    logi(pParams->slice_header_restriction_flag);
    logi(pParams->store_ref_base_pic_flag);
    logi(pParams->ShiftXY16Flag);
    logi(pParams->constrained_intra_resampling_flag);
    logi(pParams->ref_layer_chroma_phase_x_plus1_flag);
    logi(pParams->tcoeff_level_prediction_flag);
    logi(pParams->IdrPicFlag);
    logi(pParams->NextLayerSpatialResolutionChangeFlag);
    logi(pParams->NextLayerMaxTXoeffLevelPredFlag);

    logi(pParams->extended_spatial_scalability_idc);
    logi(pParams->chroma_phase_y_plus1);
    logi(pParams->seq_ref_layer_chroma_phase_y_plus1);

    logi(pParams->seq_scaled_ref_layer_left_offset);
    logi(pParams->seq_scaled_ref_layer_top_offset);
    logi(pParams->seq_scaled_ref_layer_right_offset);
    logi(pParams->seq_scaled_ref_layer_bottom_offset);

    logi(pParams->seq_ref_layer_chroma_phase_y_plus1);

    logi(pParams->LayerType);
    logi(pParams->dependency_id);
    logi(pParams->quality_id);

    logi(pParams->ref_layer_dq_id);

    logi(pParams->disable_inter_layer_deblocking_filter_idc);
    logi(pParams->inter_layer_slice_alpha_c0_offset_div2);
    logi(pParams->inter_layer_slice_beta_offset_div2);
    logi(pParams->ref_layer_chroma_phase_y_plus1);

    logi(pParams->NextLayerScaledRefLayerLeftOffset);
    logi(pParams->NextLayerScaledRefLayerRightOffset);
    logi(pParams->NextLayerScaledRefLayerTopOffset);
    logi(pParams->NextLayerScaledRefLayerBottomOffset);
    logi(pParams->NextLayerPicWidthInMbs);
    logi(pParams->NextLayerPicHeightInMbs);
    logi(pParams->NextLayerDisableInterLayerDeblockingFilterIdc);
    logi(pParams->NextLayerInterLayerSliceAlphaC0OffsetDiv2);
    logi(pParams->NextLayerInterLayerSliceBetaOffsetDiv2);

    logi(pParams->DeblockingFilterMode);

    dumpH264PictureParameters(pParams);
}

void dumpH264SVCSliceParameters(void * sliceParam, size_t bufSize, bool isLong)
{
    if (isLong)
    {
        DXVA_Slice_H264_SVC_Long *pSlice = (DXVA_Slice_H264_SVC_Long*)sliceParam;
        for (int i = 0; i < bufSize/sizeof(DXVA_Slice_H264_SVC_Long); i++)
        {
            logi(i);
            logi(pSlice[i].no_inter_layer_pred_flag);
            logi(pSlice[i].base_pred_weight_table_flag);
            logi(pSlice[i].slice_skip_flag);
            logi(pSlice[i].adaptive_base_mode_flag);
            logi(pSlice[i].default_base_mode_flag);
            logi(pSlice[i].adaptive_motion_prediction_flag);
            logi(pSlice[i].default_motion_prediction_flag);
            logi(pSlice[i].adaptive_residual_prediction_flag);
            logi(pSlice[i].default_residual_prediction_flag);

            logi(pSlice[i].num_mbs_in_slice_minus1);
            logi(pSlice[i].scan_idx_start);
            logi(pSlice[i].scan_idx_end);

            dumpH264SliceParameter(&pSlice[i], isLong);
        }
    }
    else
    {
        DXVA_Slice_H264_SVC_Short *pSlice = (DXVA_Slice_H264_SVC_Short*)sliceParam;
        for (int i = 0; i < bufSize/sizeof(DXVA_Slice_H264_SVC_Short); i++)
        {
            logi(i);
            logi(pSlice[i].no_inter_layer_pred_flag);

            dumpH264SliceParameter(&pSlice[i], isLong);
        }
    }
}

void dumpH264PictureParameters_MS_MVC(void *pPictureParameters)
{
    DXVA_PicParams_H264_MVC *pParams = (DXVA_PicParams_H264_MVC*)pPictureParameters;

    dumpH264PictureParameters(pPictureParameters);

    logs("\nMVC picture params\n");

    logi(pParams->num_views_minus1);

    logi(pParams->curr_view_id);
    logi(pParams->anchor_pic_flag);
    logi(pParams->inter_view_flag);
 
    for (int i = 0; i < 16; i++)
    {
        logi(pParams->view_id[i]);
    }

    for (int i = 0; i < 16; i++)
    {
        logi(pParams->ViewIDList[i]);
    }

    for (int i = 0; i < 16; i++)
    {
        logi(pParams->num_anchor_refs_l0[i]);
    }

    for (int i = 0; i < 16; i++)
    {
        logi(i);
        for (int j = 0; j < 16; j++)
            logi(pParams->anchor_ref_l0[i][j]);
    }


    for (int i = 0; i < 16; i++)
    {
        logi(pParams->num_anchor_refs_l1[i]);
    }

    for (int i = 0; i < 16; i++)
    {
        logi(i);
        for (int j = 0; j < 16; j++)
            logi(pParams->anchor_ref_l1[i][j]);
    }


    for (int i = 0; i < 16; i++)
    {
        logi(pParams->num_non_anchor_refs_l0[i]);
    }

    for (int i = 0; i < 16; i++)
    {
        logi(i);
        for (int j = 0; j < 16; j++)
            logi(pParams->non_anchor_ref_l0[i][j]);
    }

    for (int i = 0; i < 16; i++)
    {
        logi(pParams->num_non_anchor_refs_l1[i]);
    }

    for (int i = 0; i < 16; i++)
    {
        logi(i);
        for (int j = 0; j < 16; j++)
            logi(pParams->non_anchor_ref_l1[i][j]);
    }
}

void dumpH264PictureParameters(void *pPictureParameters)
{
    DXVA_PicParams_H264 *pParams = (DXVA_PicParams_H264*)pPictureParameters;
    logi(pParams->wFrameWidthInMbsMinus1);
    logi(pParams->wFrameHeightInMbsMinus1);
    logi(pParams->CurrPic.Index7Bits);
    logi(pParams->CurrPic.AssociatedFlag);
    logi(pParams->num_ref_frames);

    //logi(pParams->wBitFields);
    logi(pParams->field_pic_flag);
    logi(pParams->MbaffFrameFlag);
    logi(pParams->residual_colour_transform_flag);
    logi(pParams->sp_for_switch_flag);
    logi(pParams->chroma_format_idc);
    logi(pParams->RefPicFlag);
    logi(pParams->constrained_intra_pred_flag);
    logi(pParams->weighted_pred_flag);
    logi(pParams->weighted_bipred_idc);
    logi(pParams->MbsConsecutiveFlag);
    logi(pParams->frame_mbs_only_flag);
    logi(pParams->transform_8x8_mode_flag);
    logi(pParams->MinLumaBipredSize8x8Flag);
    logi(pParams->IntraPicFlag);

    logi(pParams->bit_depth_luma_minus8);
    logi(pParams->bit_depth_chroma_minus8);
    logi(pParams->Reserved16Bits);
    logui(pParams->StatusReportFeedbackNumber);
    logi(pParams->CurrFieldOrderCnt[0]);
    logi(pParams->CurrFieldOrderCnt[1]);
    logi(pParams->pic_init_qs_minus26);
    logi(pParams->chroma_qp_index_offset);
    logi(pParams->second_chroma_qp_index_offset);
    logi(pParams->ContinuationFlag);
    logi(pParams->pic_init_qp_minus26);
    logi(pParams->num_ref_idx_l0_active_minus1);
    logi(pParams->num_ref_idx_l1_active_minus1);
    logi(pParams->Reserved8BitsA);
    logi(pParams->UsedForReferenceFlags);
    logi(pParams->NonExistingFrameFlags);
    logi(pParams->frame_num);
    logi(pParams->log2_max_frame_num_minus4);
    logi(pParams->pic_order_cnt_type);
    logi(pParams->log2_max_pic_order_cnt_lsb_minus4);
    logi(pParams->delta_pic_order_always_zero_flag);
    logi(pParams->direct_8x8_inference_flag);
    logi(pParams->entropy_coding_mode_flag);
    logi(pParams->pic_order_present_flag);
    logi(pParams->num_slice_groups_minus1);
    logi(pParams->slice_group_map_type);
    logi(pParams->deblocking_filter_control_present_flag);
    logi(pParams->redundant_pic_cnt_present_flag);
    logi(pParams->Reserved8BitsB);
    logi(pParams->slice_group_change_rate_minus1);
    int i;
    for (i = 0; i < 16; i++)
    {
        logi(i);
        logi(pParams->RefFrameList[i].Index7Bits);
        logi(pParams->RefFrameList[i].AssociatedFlag);
        logi(pParams->FieldOrderCntList[i][0]);
        logi(pParams->FieldOrderCntList[i][1]);
        logi(pParams->FrameNumList[i]);
    }
    //logi(pParams->SliceGroupMap[810]; /* 4b/sgmu, Size BT.601 */
}

void dumpH264SliceParameter(void * sliceParam, bool isLong)
{
    if (isLong)
    {
        DXVA_Slice_H264_Long *pSlice = (DXVA_Slice_H264_Long*)sliceParam;
        logi(pSlice->BSNALunitDataLocation);
        logi(pSlice->SliceBytesInBuffer);
        logi(pSlice->wBadSliceChopping);
        logi(pSlice->first_mb_in_slice);
        logi(pSlice->NumMbsForSlice);
        logi(pSlice->BitOffsetToSliceData);
        logi(pSlice->slice_type);
        logi(pSlice->luma_log2_weight_denom);
        logi(pSlice->chroma_log2_weight_denom);
        logi(pSlice->num_ref_idx_l0_active_minus1);
        logi(pSlice->num_ref_idx_l1_active_minus1);
        logi(pSlice->slice_alpha_c0_offset_div2);
        logi(pSlice->slice_beta_offset_div2);
        logi(pSlice->Reserved8Bits);
        logi(pSlice->slice_qs_delta);
        logi(pSlice->slice_qp_delta);
        logi(pSlice->redundant_pic_cnt);
        logi(pSlice->direct_spatial_mv_pred_flag);
        logi(pSlice->cabac_init_idc);
        logi(pSlice->disable_deblocking_filter_idc);
        logi(pSlice->slice_id);
        for (int j = 0; j < 32; j++)
        {
            logi(j);
            logi(pSlice->RefPicList[0][j].bPicEntry);
            logi(pSlice->RefPicList[1][j].bPicEntry);
            logi(pSlice->Weights[0][j][0][0]);
            logi(pSlice->Weights[0][j][0][1]);
            logi(pSlice->Weights[0][j][1][0]);
            logi(pSlice->Weights[0][j][1][1]);
            logi(pSlice->Weights[0][j][2][0]);
            logi(pSlice->Weights[0][j][2][1]);
            logi(pSlice->Weights[1][j][0][0]);
            logi(pSlice->Weights[1][j][0][1]);
            logi(pSlice->Weights[1][j][1][0]);
            logi(pSlice->Weights[1][j][1][1]);
            logi(pSlice->Weights[1][j][2][0]);
            logi(pSlice->Weights[1][j][2][1]);
        }
    }
    else
    {
        DXVA_Slice_H264_Short *pSlice = (DXVA_Slice_H264_Short*)sliceParam;
        logi(pSlice->BSNALunitDataLocation);
        logi(pSlice->SliceBytesInBuffer);
        logi(pSlice->wBadSliceChopping);
    }
}

void dumpH264SliceParameters(void * sliceParam, size_t bufSize, bool isLong)
{
    if (isLong)
    {
        DXVA_Slice_H264_Long *pSlice = (DXVA_Slice_H264_Long*)sliceParam;
        for (int i = 0; i < bufSize/sizeof(DXVA_Slice_H264_Long); i++)
        {
            logi(i);
            dumpH264SliceParameter(&pSlice[i], isLong);
        }
    }
    else
    {
        DXVA_Slice_H264_Short *pSlice = (DXVA_Slice_H264_Short*)sliceParam;
        for (int i = 0; i < bufSize/sizeof(DXVA_Slice_H264_Short); i++)
        {
            logi(i);
            dumpH264SliceParameter(&pSlice[i], isLong);
        }
    }
}

void dumpMPEG2PictureParameters(void *pPictureParameters)
{
    DXVA_PictureParameters *pParams = (DXVA_PictureParameters*)pPictureParameters;
    logi(pParams->wDecodedPictureIndex);
    logi(pParams->wDeblockedPictureIndex);
    logi(pParams->wForwardRefPictureIndex);
    logi(pParams->wBackwardRefPictureIndex);
    logi(pParams->wPicWidthInMBminus1);
    logi(pParams->wPicHeightInMBminus1);
    logi(pParams->bMacroblockWidthMinus1);
    logi(pParams->bMacroblockHeightMinus1);
    logi(pParams->bBlockWidthMinus1);
    logi(pParams->bBlockHeightMinus1);
    logi(pParams->bBPPminus1);
    logi(pParams->bPicStructure);
    logi(pParams->bSecondField);
    logi(pParams->bPicIntra);
    logi(pParams->bPicBackwardPrediction);
    logi(pParams->bBidirectionalAveragingMode);
    logi(pParams->bMVprecisionAndChromaRelation);
    logi(pParams->bChromaFormat);
    logi(pParams->bPicScanFixed);
    logi(pParams->bPicScanMethod);
    logi(pParams->bPicReadbackRequests);
    logi(pParams->bRcontrol);
    logi(pParams->bPicSpatialResid8);
    logi(pParams->bPicOverflowBlocks);
    logi(pParams->bPicExtrapolation);
    logi(pParams->bPicDeblocked);
    logi(pParams->bPicDeblockConfined);
    logi(pParams->bPic4MVallowed);
    logi(pParams->bPicOBMC);
    logi(pParams->bPicBinPB);
    logi(pParams->bMV_RPS);
    logi(pParams->bReservedBits);
    logi(pParams->wBitstreamFcodes);
    logi(pParams->wBitstreamPCEelements);
    logi(pParams->bBitstreamConcealmentNeed);
    logi(pParams->bBitstreamConcealmentMethod);
}

void dumpMPEG4PictureParameters(void *pPictureParameters)
{
    DXVA_PicParams_MPEG4 *pParams = (DXVA_PicParams_MPEG4*)pPictureParameters;

    logi(pParams->wDecodedPictureIndex);
    logi(pParams->wForwardRefPictureIndex);
    logi(pParams->wBackwardRefPictureIndex);
    logi(pParams->wPicWidthInMBminus1);
    logi(pParams->wPicHeightInMBminus1);
    logi(pParams->wDisplayWidthMinus1);
    logi(pParams->wDisplayHeightMinus1);
    logi(pParams->bMacroblockWidthMinus1);
    logi(pParams->bMacroblockHeightMinus1);
    logi(pParams->bPicIntra);
    logi(pParams->bPicBackwardPrediction);
    logi(pParams->bPicSprite);
    logi(pParams->bChromaFormat);
    logi(pParams->wProfile);
    logi(pParams->bShortHeader);
    logi(pParams->bQuantPrecision);
    logi(pParams->bGMC);
    logi(pParams->bSpriteWarpingPoints);
    logi(pParams->bSpriteWarpingAccuracy);
    logi(pParams->bReversibleVLC);
    logi(pParams->bDataPartitioned);
    logi(pParams->bInterlaced);
    logi(pParams->bBwdRefVopType);
    logi(pParams->bIntraDcVlcThr);
    logi(pParams->bVopFcodeFwd);
    logi(pParams->bVopFcodeBwd);
    logi(pParams->wNumMBsInGob);
    logi(pParams->bNumGobsInVop);
    logi(pParams->bAlternateScan);
    logi(pParams->bTopFieldFirst);
    logi(pParams->bQuantType);
    logi(pParams->bObmcDisable);
    logi(pParams->bQuarterSample);
    logi(pParams->bVopRoundingType);
    logi(pParams->wSpriteTrajectoryDu);
    logi(pParams->wSpriteTrajectoryDv);
    logi(pParams->wTrb);
    logi(pParams->wTrd);
}

void dumpVC1PictureParametersExt(void *pPictureParameters)
{
    DXVA_Intel_VC1PictureParameters *pParams = (DXVA_Intel_VC1PictureParameters*)pPictureParameters;
    
    logi(pParams->bBScaleFactor);
    logi(pParams->bPQuant);
    logi(pParams->bAltPQuant);

    logi(pParams->bPictureFlags);
    logi(pParams->FrameCodingMode);
    logi(pParams->PictureType);
    logi(pParams->CondOverFlag);
    logi(pParams->Reserved1bit);
    logi(pParams->bPQuantFlags);
    logi(pParams->PQuantUniform);
    logi(pParams->HalfQP);
    logi(pParams->AltPQuantConfig);
    logi(pParams->AltPQuantEdgeMask);
    logi(pParams->bMvRange);;
    logi(pParams->ExtendedMVrange);
    logi(pParams->ExtendedDMVrange);
    logi(pParams->Reserved4bits);
    logi(pParams->wMvReference);
    logi(pParams->ReferenceDistance);
    logi(pParams->BwdReferenceDistance);
    logi(pParams->NumberOfReferencePictures);
    logi(pParams->ReferenceFieldPicIndicator);
    logi(pParams->FastUVMCflag);
    logi(pParams->FourMvSwitch);
    logi(pParams->UnifiedMvMode);
    logi(pParams->IntensityCompensationField);
    logi(pParams->wTransformFlags);;
    logi(pParams->CBPTable);
    logi(pParams->TransDCtable);
    logi(pParams->TransACtable);
    logi(pParams->TransACtable2);
    logi(pParams->MbModeTable);
    logi(pParams->TTMBF);
    logi(pParams->TTFRM);
    logi(pParams->Reserved2bits);
    logi(pParams->bMvTableFlags);
    logi(pParams->TwoMVBPtable);
    logi(pParams->FourMVBPtable);
    logi(pParams->MvTable);
    logi(pParams->reserved1bit);
    logi(pParams->bRawCodingFlag);
    logi(pParams->FieldTX);
    logi(pParams->ACpred);
    logi(pParams->Overflags);
    logi(pParams->DirectMB);
    logi(pParams->SkipMB);
    logi(pParams->MvTypeMB);
    logi(pParams->ForwardMB);
    logi(pParams->IsBitplanePresent);
}

void dumpD3DSurface(IDirect3DSurface9 *pD3D9Surface)
{
    if (!dumpDx9.GetDumpSurface())
        return;

    HRESULT hr;
    D3DSURFACE_DESC desc;
    D3DLOCKED_RECT lr;
    hr = pD3D9Surface->GetDesc(&desc);
    if (FAILED(hr)) return ;
    hr = pD3D9Surface->LockRect(&lr, NULL, D3DLOCK_NOSYSLOCK | D3DLOCK_READONLY);
    if (FAILED(hr)) return ;
    hr = pD3D9Surface->UnlockRect();

    FILE * f1;
    char fname[1024];
    sprintf(fname, "%s\\out.yuv", pDir );
    f1 = fopen(fname, "ab");
    
    for (int i = 0; i< desc.Height; i++)
    {
        fwrite(lr.pBits, 1, desc.Width, f1);
        lr.pBits = (char*)lr.pBits + lr.Pitch;
    }

    void * temp = lr.pBits;

    for (int i = 0; i < desc.Height/2; i++)
    {
        for (int j = 0; j < desc.Width; j += 2)
            fwrite((char*)lr.pBits + j, 1, 1, f1);

        lr.pBits = (char*)lr.pBits + lr.Pitch;
    }
    
    lr.pBits = temp;

    for (int i = 0; i< desc.Height/2; i++)
    {
        for (int j = 1; j < desc.Width; j += 2)
            fwrite((char*)lr.pBits + j, 1, 1, f1);

        lr.pBits = (char*)lr.pBits + lr.Pitch;
    }

    fclose(f1);
}

HRESULT STDMETHODCALLTYPE SpyD3D11VideoDevice::CreateVideoDecoder(
    const D3D11_VIDEO_DECODER_DESC *pVideoDesc,
    const D3D11_VIDEO_DECODER_CONFIG *pConfig,
    ID3D11VideoDecoder **ppDecoder)
{
    LLLVDECSRV;

    OPEN_SPEC_FILE;
    LOG_GUID(pVideoDesc->Guid);
    dumpVideoDesc(pVideoDesc);
    dumpConfig(pConfig);

    HRESULT hr  = m_pObject->CreateVideoDecoder(pVideoDesc, pConfig, ppDecoder);
    logi(hr);
    if (SUCCEEDED(hr))
    {
        dumpDx11.Init(pVideoDesc->Guid, pConfig);
    }

    CLOSE_SPEC_FILE;
    return hr;
}

HRESULT STDMETHODCALLTYPE SpyD3D11VideoContext::GetDecoderBuffer(
    /* [annotation] */
    _In_  ID3D11VideoDecoder *pDecoder,
    /* [annotation] */
    _In_  D3D11_VIDEO_DECODER_BUFFER_TYPE Type,
    /* [annotation] */
    _Out_  UINT *pBufferSize,
    /* [annotation] */
    _Out_writes_bytes_opt_(*pBufferSize)  void **ppBuffer)
{
    LLL;
    logi(Type);

    if (dumpDx11.GetSkipExecute())
    {
        dumpDx11.GetBuffer(Type, ppBuffer, pBufferSize);
        return S_OK;
    }

    HRESULT hr = m_pObject->GetDecoderBuffer(pDecoder, Type, pBufferSize, ppBuffer);
    logi(*pBufferSize);
    dumpDx11.GetBuffer(Type, ppBuffer, pBufferSize);
    logi_flush(hr);
    return hr;
}


HRESULT STDMETHODCALLTYPE SpyD3D11VideoContext::DecoderBeginFrame(
    /* [annotation] */
    _In_  ID3D11VideoDecoder *pDecoder,
    /* [annotation] */
    _In_  ID3D11VideoDecoderOutputView *pView,
    /* [annotation] */
    _In_  UINT ContentKeySize,
    /* [annotation] */
    _In_reads_bytes_opt_(ContentKeySize)  const void *pContentKey)
{
    LLL;
    return m_pObject->DecoderBeginFrame(pDecoder,pView,ContentKeySize,pContentKey);
}

HRESULT STDMETHODCALLTYPE SpyD3D11VideoContext::DecoderEndFrame(
    /* [annotation] */
    _In_  ID3D11VideoDecoder *pDecoder)
{
    LLL;
    return m_pObject->DecoderEndFrame(pDecoder);
}

HRESULT STDMETHODCALLTYPE SpyD3D11VideoContext::SubmitDecoderBuffers(
    /* [annotation] */
    _In_  ID3D11VideoDecoder *pDecoder,
    /* [annotation] */
    _In_  UINT NumBuffers,
    /* [annotation] */
    _In_reads_(NumBuffers)  const D3D11_VIDEO_DECODER_BUFFER_DESC *pBufferDesc)
{
    LLL;
    dumpDx11.Execute(NumBuffers, pBufferDesc);
    return m_pObject->SubmitDecoderBuffers(pDecoder, NumBuffers, pBufferDesc);
}
