/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2008 Intel Corporation. All Rights Reserved.

File Name: test_h264_decode_utils.cpp

\* ****************************************************************************** */

#include "test_h264_bsd_dec_utils.h"
#include <memory>

MfxBitstreamHolder::MfxBitstreamHolder(const vm_char* name)
: m_file(0), m_dataLen(0), m_isEos(false)
{
    memset(&m_bs, 0, sizeof(mfxBitstream));
    m_bs.MaxLength = m_bufSize;
    Open(name);
}

MfxBitstreamHolder::~MfxBitstreamHolder()
{
    if (m_file)
        vm_file_close(m_file);
}

bool MfxBitstreamHolder::IsOpen() const
{
    return m_file != 0;
}

bool MfxBitstreamHolder::IsEos() const
{
    return m_isEos;
}

bool MfxBitstreamHolder::Open(const vm_char* name)
{
    if (m_file)
        vm_file_close(m_file);

    m_file = vm_file_open(name, VM_STRING("r"));
    if (!m_file)
        return false;

    Ipp32s toRead = (Ipp32s)min(m_file->fsize, m_bufSize);
    if (vm_file_fread(m_buf, 1, toRead, m_file) != toRead)
    {
        vm_file_close(m_file);
        return false;
    }

    m_dataLen = toRead;
    m_isEos = m_dataLen < m_bufSize;
    m_bs.Data = m_buf;
    m_bs.DataLength = m_dataLen;
    m_bs.DataOffset = 0;
    return true;
}

mfxBitstream& MfxBitstreamHolder::Extract()
{
    return m_bs;
}

const mfxBitstream& MfxBitstreamHolder::Extract() const
{
    return m_bs;
}

bool MfxBitstreamHolder::Update()
{
    if (m_bs.DataOffset == 0)
        return true;
    if (m_bs.DataOffset > m_dataLen)
        return false;

    Ipp64u pos = vm_file_ftell(m_file);
    Ipp32u tail = m_dataLen - m_bs.DataOffset;
    memcpy(m_buf, m_buf + m_bs.DataOffset, tail);
    Ipp32s toRead = (Ipp32s)min(m_file->fsize - pos, m_bufSize - tail);
    if (vm_file_fread(m_buf + tail, 1, toRead, m_file) != toRead)
    {
        m_dataLen = tail;
        vm_file_fseek(m_file, pos, VM_FILE_SEEK_SET);
        return false;
    }

    m_dataLen = tail + toRead;
    m_bs.Data = m_buf;
    m_bs.DataLength = m_dataLen;
    m_bs.DataOffset = 0;
    m_isEos = m_dataLen < m_bufSize;
    return true;
}

MfxFrameCucHolder::MfxFrameCucHolder()
{
    memset(&m_cuc, 0, sizeof(mfxFrameCUC));
    memset(&m_fs, 0, sizeof(mfxFrameSurface));
}

MfxFrameCucHolder::~MfxFrameCucHolder()
{
    Close();
}

mfxFrameCUC& MfxFrameCucHolder::Extract()
{
    return m_cuc;
}

const mfxFrameCUC& MfxFrameCucHolder::Extract() const
{
    return m_cuc;
}

template<class T>
struct SelfDestrArray
{
    explicit SelfDestrArray(T* ptr = 0) : m_ptr(ptr) {}
    ~SelfDestrArray() { delete[] m_ptr; }
    T* get() const { return ptr; }
    T* release() { T* ptr = m_ptr; m_ptr = 0; return ptr; }
private:
    SelfDestrArray(SelfDestrArray<T>&);
    SelfDestrArray<T>& operator=(SelfDestrArray<T>&);
    T* m_ptr;
};

void MfxFrameCucHolder::Init()
{
    std::auto_ptr<mfxFrameParam> ptrFrame(new mfxFrameParam);
    std::auto_ptr<mfxMbParam> ptrMbParam(new mfxMbParam);

    Close();
    m_cuc.FrameParam = ptrFrame.release();
    m_cuc.MbParam = ptrMbParam.release();
    m_cuc.MbParam->CucId = MFX_CUC_AVC_MBPARAM;
    m_cuc.MbParam->CucSz = sizeof(mfxMbParam);
    m_cuc.MbParam->NumMb = 0;
    m_cuc.MbParam->Mb = 0;
}

void MfxFrameCucHolder::Close()
{
    FreeFrameSurface();
    FreeSlicesAndMbs();
    FreeExtBuffers();

    delete m_cuc.FrameParam;
    m_cuc.FrameParam = 0;
    delete m_cuc.MbParam;
    m_cuc.MbParam = 0;
    memset(&m_cuc, 0, sizeof(mfxFrameCUC));
}

void MfxFrameCucHolder::AllocateFrameSurface(const mfxVideoParam &videoPar)
{
    mfxU32 width = videoPar.mfx.FrameInfo.Width;
    mfxU32 height = videoPar.mfx.FrameInfo.Height;

    m_fs.Info.FourCC;
    m_fs.Info.PicStruct;
    m_fs.Info.Height = (mfxU16)videoPar.mfx.FrameInfo.Height;
    m_fs.Info.Width = (mfxU16) videoPar.mfx.FrameInfo.Width;

    m_fs.Info.CropX = 0;
    m_fs.Info.CropY = 0;
    m_fs.Info.CropW = (mfxU16) videoPar.mfx.FrameInfo.Width;
    m_fs.Info.CropH = (mfxU16) videoPar.mfx.FrameInfo.Height;

    m_fs.Info.ScaleCHeight = (videoPar.mfx.FrameInfo.ChromaFormat - 1) == MFX_CHROMAFORMAT_YUV420 ? 1 : 0;
    m_fs.Info.ScaleCPitch = (videoPar.mfx.FrameInfo.ChromaFormat - 1) == MFX_CHROMAFORMAT_YUV444 ? 0 : 1;
    m_fs.Info.ScaleCWidth = (videoPar.mfx.FrameInfo.ChromaFormat - 1) == MFX_CHROMAFORMAT_YUV444 ? 0 : 1;

    m_fs.Info.ChromaFormat = videoPar.mfx.FrameInfo.ChromaFormat;

    m_fs.Info.Step = 1;
    m_fs.Info.DeltaCStep = 0;
    m_fs.Info.BitDepthLuma = 8;
    m_fs.Info.BitDepthChroma = 8;
    m_fs.Info.DeltaCBitDepth = 0;
    m_fs.Info.ByteOrderFlag = 0;

    m_fs.Info.NumFrameData = (mfxU8)17;
    m_fs.Data = new mfxFrameData * [m_fs.Info.NumFrameData];

    mfxU32 i = 0;
    //for (mfxU32 i = 0; i < m_fs.Info.NumFrameData; i++)
    {
        m_fs.Data[i] = new mfxFrameData;
        m_fs.Data[i]->Pitch = (mfxU16)width;
        m_fs.Data[i]->FrameOrder = 0;
        m_fs.Data[i]->Locked = 0;
        m_fs.Data[i]->DataLength = (width * height * 3) / 2;
        m_fs.Data[i]->TimeStamp = 0;
        m_fs.Data[i]->Y = new mfxU8[width * height];
        m_fs.Data[i]->U = new mfxU8[width * height / 4];
        m_fs.Data[i]->V = new mfxU8[width * height / 4];
        m_fs.Data[i]->A = 0;
    }

    m_cuc.FrameSurface = &m_fs;
}

void MfxFrameCucHolder::AllocSlicesAndMbs(mfxU32 numMbs)
{
    SelfDestrArray<mfxMbCode> arrMb(new mfxMbCode[numMbs]);
    SelfDestrArray<mfxSliceParam> arrSlice(new mfxSliceParam[numMbs]);

    FreeSlicesAndMbs();
    m_cuc.MbParam->Mb = arrMb.release();
    m_cuc.MbParam->NumMb = numMbs;
    m_cuc.SliceParam = arrSlice.release();
    m_cuc.NumSlice = 0;
}

void MfxFrameCucHolder::FreeSlicesAndMbs()
{
    delete[] m_cuc.SliceParam;
    m_cuc.SliceParam = 0;
    m_cuc.NumSlice = 0;

    if (m_cuc.MbParam)
    {
        delete[] m_cuc.MbParam->Mb;
        m_cuc.MbParam->Mb = 0;
        m_cuc.MbParam->NumMb = 0;
    }
}

void MfxFrameCucHolder::FreeFrameSurface()
{
    Ipp32s i = 0;

    if (m_fs.Data)
    {
        delete [] m_fs.Data[i]->Y;
        delete [] m_fs.Data[i]->U;
        delete [] m_fs.Data[i]->V;
        delete m_fs.Data[i];
    }

    delete [] m_fs.Data;// = new mfxFrameData * [m_fs.Info.NumFrameData];

    memset(&m_fs, 0, sizeof(mfxFrameSurface));
}

mfxU32 AlignToMfxU32(mfxU32 value)
{
    return ((value + sizeof(mfxU32) - 1) / sizeof(mfxU32)) * sizeof(mfxU32);
}

mfxHDL AllocExtBuffer(mfxU32 structSize, mfxU32 heapSize)
{
    return new mfxU32[(4 + AlignToMfxU32(structSize) + AlignToMfxU32(heapSize)) / sizeof(mfxU32)];
}

void SetExtBufferIdAndTag(mfxHDL ExtBuffer, mfxU32& tag, mfxU32 id)
{
    ((mfxU32 *)ExtBuffer)[0] = id;
    tag |= (1 << id);
}

mfxI16  GetExBufferIndex (mfxFrameCUC *cuc, mfxU32 buf_label, mfxU32 cucID)
{
    mfxI16 index = -1;
    if (((cuc->TagExtBuffer & (1<<buf_label)) || buf_label>32) && cuc->ExtBuffer)
    {
        for (mfxI16 i = 0; i < cuc->NumExtBuffer; i ++)
        {
            if (((mfxU32*)(cuc->ExtBuffer[i]))[0] == cucID)
            {
                index = i;
                break;
            }
        }
    }
    return index;
}

template<class T>
T& GetExtBufStruct(mfxHDL extBuf)
{
    return *((T *)((mfxU32 *)extBuf /*+ 1*/));
}

template<class PtrType, class StructType>
PtrType* GetExtBufStructPtr(StructType& extBufStruct)
{
    return (PtrType *)((mfxU8 *)&extBufStruct + AlignToMfxU32(sizeof(StructType)));
}

template <class T>
inline T * GetExtBuffer(mfxFrameCUC * cuc, mfxU32 label, mfxU32 cucValue)
{
    mfxI16  index  =  GetExBufferIndex(cuc, label, cucValue);

    if (index == -1)
        return 0;

    return (T*)cuc->ExtBuffer[index];
}

void MfxFrameCucHolder::Reset()
{
    ExtH264ResidCoeffs * buf = GetExtBuffer<ExtH264ResidCoeffs>(&m_cuc, MFX_LABEL_RESCOEFF, MFX_CUC_AVC_MV_RES);
    if (!buf)
        return;

    buf->numIndexes = 0;
}

#define MAX_SLICE_COUNT 256

void MfxFrameCucHolder::AllocExtBuffers(const mfxFrameInfo& info, mfxU32 /*pitch*/)
{
    mfxU32 pixSize = info.Width * info.Height + 2 * (info.Width >> info.ScaleCWidth) * (info.Height >> info.ScaleCHeight);
    //mfxU32 byteSize = pitch * info.Height + 2 * (pitch >> info.ScaleCPitch) * (info.Height >> info.ScaleCHeight);
    // allocate all memory needed
    SelfDestrArray<void*> arrExtBuf(new mfxHDL[NumExtBuffer]);
    SelfDestrArray<void> arrExtBufQuantMatrix(AllocExtBuffer(sizeof(mfxExtAvcQMatrix), 0));
    //SelfDestrArray<void> arrExtBufResPixel(AllocExtBuffer(sizeof(mfxExtMpeg2Residual), 2 * byteSize));
    SelfDestrArray<void> arrExtBufResCoeff(AllocExtBuffer(sizeof(ExtH264ResidCoeffs), 5 * pixSize));
    //SelfDestrArray<void> arrExtBufResPred(AllocExtBuffer(sizeof(mfxExtMpeg2Prediction), 1 * byteSize));

    SelfDestrArray<void> arrExtBufDxvaPicParam(AllocExtBuffer(sizeof(mfxExtAvcDxvaPicParam), 0));
    SelfDestrArray<void> arrExtBufAVCRefParam(new AvcReferenceParamSlice [MAX_SLICE_COUNT]);

    SelfDestrArray<void> arrExtBufCUCRefList(AllocExtBuffer(sizeof(mfxExtCUCRefList), 0));

    SelfDestrArray<void> arrExtBufMVs(AllocExtBuffer(sizeof(ExtH264MVValues), 3 * pixSize));

    //SelfDestrArray<void> arrExtBufSliceRefs(AllocExtBuffer(sizeof(mfxExtSliceRefList), 10 * pixSize));

    FreeExtBuffers();
    m_cuc.ExtBuffer = arrExtBuf.release();

    m_cuc.ExtBuffer[0] = arrExtBufResCoeff.release();
    SetExtBufferIdAndTag(m_cuc.ExtBuffer[0], m_cuc.TagExtBuffer, MFX_LABEL_RESCOEFF);
    ExtH264ResidCoeffs& sCoeffs = GetExtBufStruct<ExtH264ResidCoeffs>(m_cuc.ExtBuffer[0]);
    sCoeffs.CucId = MFX_CUC_AVC_MV_RES;
    sCoeffs.CucSz = AlignToMfxU32(sizeof(MFX_H264_Coefficient)) + AlignToMfxU32(5 * pixSize); //  816
    sCoeffs.Coeffs = GetExtBufStructPtr<MFX_H264_Coefficient>(sCoeffs);
    sCoeffs.numIndexes = 0;

    m_cuc.ExtBuffer[1] = arrExtBufQuantMatrix.release();
    SetExtBufferIdAndTag(m_cuc.ExtBuffer[1], m_cuc.TagExtBuffer, MFX_LABEL_QMATRIX);
    mfxExtAvcQMatrix& sQMatrix = GetExtBufStruct<mfxExtAvcQMatrix>(m_cuc.ExtBuffer[1]);
    sQMatrix.CucId = MFX_CUC_AVC_QMATRIX;
    sQMatrix.CucSz = AlignToMfxU32(sizeof(mfxExtAvcQMatrix));

    m_cuc.ExtBuffer[2] = arrExtBufDxvaPicParam.release();
    SetExtBufferIdAndTag(m_cuc.ExtBuffer[2], m_cuc.TagExtBuffer, MFX_LABEL_EXTHEADER);
    mfxExtAvcDxvaPicParam& dxvaPicParam = GetExtBufStruct<mfxExtAvcDxvaPicParam>(m_cuc.ExtBuffer[2]);
    dxvaPicParam.CucId = MFX_CUC_AVC_EXTHEADER;
    dxvaPicParam.CucSz = AlignToMfxU32(sizeof(mfxExtAvcDxvaPicParam));

    m_cuc.ExtBuffer[3] = arrExtBufAVCRefParam.release();
    SetExtBufferIdAndTag(m_cuc.ExtBuffer[3], m_cuc.TagExtBuffer, MFX_LABEL_REFINDEX);
    AvcReferenceParamSlice& avcRefParam = GetExtBufStruct<AvcReferenceParamSlice>(m_cuc.ExtBuffer[3]);

    for (Ipp32s i = 0; i < MAX_SLICE_COUNT; i++)
    {
        AvcReferenceParamSlice* sl = (&avcRefParam) + i;
        sl->CucId = MFX_CUC_AVC_REFLABEL;
        sl->CucSz = AlignToMfxU32(sizeof(AvcReferenceParamSlice));
    }

    m_cuc.ExtBuffer[4] = arrExtBufCUCRefList.release();
    SetExtBufferIdAndTag(m_cuc.ExtBuffer[4], m_cuc.TagExtBuffer, MFX_LABEL_CUCREFLIST);
    mfxExtCUCRefList& avcCUCRef = GetExtBufStruct<mfxExtCUCRefList>(m_cuc.ExtBuffer[4]);
    avcCUCRef.CucId = MFX_CUC_AVC_CUC_REFLIST;
    avcCUCRef.CucSz = AlignToMfxU32(sizeof(mfxExtCUCRefList));

    m_cuc.ExtBuffer[5] = arrExtBufMVs.release();
    SetExtBufferIdAndTag(m_cuc.ExtBuffer[5], m_cuc.TagExtBuffer, MFX_LABEL_MVDATA);
    ExtH264MVValues& avcMVs = GetExtBufStruct<ExtH264MVValues>(m_cuc.ExtBuffer[5]);
    avcMVs.CucId = MFX_CUC_AVC_MV;
    avcMVs.CucSz = AlignToMfxU32(sizeof(ExtH264MVValues));
    avcMVs.mvs = GetExtBufStructPtr<MFX_MV>(avcMVs);

    /*m_cuc.ExtBuffer[6] = arrExtBufMVs.release();
    SetExtBufferIdAndTag(m_cuc.ExtBuffer[6], m_cuc.TagExtBuffer, MFX_LABEL_MVDATA);
    ExtH264MVValues& avcMVs = GetExtBufStruct<ExtH264MVValues>(m_cuc.ExtBuffer[6]);
    avcMVs.CucId = MFX_CUC_AVC_MV;
    avcMVs.CucSz = AlignToMfxU32(sizeof(ExtH264MVValues));
    avcMVs.mvs = GetExtBufStructPtr<MFX_MV>(avcMVs);*/

    m_cuc.NumExtBuffer = NumExtBuffer;
}

void MfxFrameCucHolder::FreeExtBuffers()
{
    if (m_cuc.ExtBuffer)
    {
        for (mfxU8 i = 0; i < m_cuc.NumExtBuffer; i++)
            delete[] m_cuc.ExtBuffer[i];
        delete[] m_cuc.ExtBuffer;
        m_cuc.ExtBuffer = 0;
        m_cuc.NumExtBuffer = 0;
        m_cuc.TagExtBuffer = 0;
    }
}

Mfxh264SurfaceHolder::Mfxh264SurfaceHolder()
{
    memset(&m_fs, 0, sizeof(mfxFrameSurface));
}

Mfxh264SurfaceHolder::~Mfxh264SurfaceHolder()
{
    Close();
}

mfxFrameSurface& Mfxh264SurfaceHolder::Extract()
{
    return m_fs;
}

const mfxFrameSurface& Mfxh264SurfaceHolder::Extract() const
{
    return m_fs;
}

mfxFrameData& Mfxh264SurfaceHolder::Data(mfxU32 frameLabel)
{
    return *(m_fs.Data[frameLabel]);
}

const mfxFrameData& Mfxh264SurfaceHolder::Data(mfxU32 frameLabel) const
{
    return *(m_fs.Data[frameLabel]);
}

mfxStatus Mfxh264SurfaceHolder::Init(mfxU32 numFrameData, mfxU32 chromaFormat, mfxU32 width, mfxU32 height)
{
    if (numFrameData >= 0xff)
        return MFX_ERR_UNSUPPORTED;

    Close();
    m_fs.Info.FourCC;
    m_fs.Info.PicStruct;
    m_fs.Info.Height = (mfxU16)height;
    m_fs.Info.Width = (mfxU16) width;
    m_fs.Info.ScaleCHeight = (chromaFormat == MFX_CHROMAFORMAT_YUV420) ? 1 : 0;
    m_fs.Info.ScaleCPitch = (chromaFormat == MFX_CHROMAFORMAT_YUV444) ? 0 : 1;
    m_fs.Info.ScaleCWidth = (chromaFormat == MFX_CHROMAFORMAT_YUV444) ? 0 : 1;

    m_fs.Info.Step = 1;
    m_fs.Info.DeltaCStep = 0;
    m_fs.Info.BitDepthLuma = 8;
    m_fs.Info.BitDepthChroma = 8;
    m_fs.Info.DeltaCBitDepth = 0;
    m_fs.Info.ByteOrderFlag = 0;

    m_fs.Info.NumFrameData = (mfxU8)numFrameData;
    m_fs.Data = new mfxFrameData* [numFrameData];

    mfxU32 i = 0;
    //for (mfxU32 i = 0; i < m_fs.Info.NumFrameData; i++)
    {
        m_fs.Data[i] = new mfxFrameData;
        m_fs.Data[i]->Pitch = (mfxU16)width;
        m_fs.Data[i]->FrameOrder = 0;
        m_fs.Data[i]->Locked = 0;
        m_fs.Data[i]->DataLength = (width * height * 3) / 2;
        m_fs.Data[i]->TimeStamp = 0;
        m_fs.Data[i]->Y = new mfxU8[width * height];
        m_fs.Data[i]->U = new mfxU8[width * height / 4];
        m_fs.Data[i]->V = new mfxU8[width * height / 4];
        m_fs.Data[i]->A = 0;
    }

    m_bEos = false;
    m_type = PicN;
    m_fullness = PicNone;
    m_labelCur = MfxNoLabel;
    m_labelPrev = MfxNoLabel;
    m_labelPrevPrev = MfxNoLabel;
    m_nextFrameOrder = 0;
    m_firstDispFrameOrder = 0;
    m_firstDispLabel = MfxNoLabel;
    return MFX_ERR_NONE;
}

mfxStatus Mfxh264SurfaceHolder::Close()
{
    if (m_fs.Data)
    {
        mfxU32 i = 0;
        //for (mfxU32 i = 0; i < m_fs.Info.NumFrameData; i++)
        {
            delete[] m_fs.Data[i]->Y;
            delete[] m_fs.Data[i]->U;
            delete[] m_fs.Data[i]->V;
            delete[] m_fs.Data[i]->A;
        }
        delete[] m_fs.Data;
        m_fs.Data = 0;
    }
    return MFX_ERR_NONE;
}

inline bool CheckSurfaceFlag(const mfxFrameData& frame, mfxU32 flag) { return frame.Locked & flag ? true : false; }
inline void SetSurfaceFlag(mfxFrameData& frame, mfxU32 flag) { frame.Locked |= flag; }
inline void UnSetSurfaceFlag(mfxFrameData& frame, mfxU32 flag) { frame.Locked &= ~flag; }

mfxU8 Mfxh264SurfaceHolder::FindAndLockFreeSurface() const
{
    for (mfxU8 i = 0; i < m_fs.Info.NumFrameData; i++)
    {
        if (!m_fs.Data[i]->Locked)
        {
            m_fs.Data[i]->Locked = 1;
            return i;
        }
    }

    return MfxNoLabel;
}

mfxU8 Mfxh264SurfaceHolder::FindSurface(mfxU16 frameOrder) const
{
    for (mfxU8 i = 0; i < m_fs.Info.NumFrameData; i++)
        if (m_fs.Data[i]->FrameOrder == frameOrder)
            return i;
    return MfxNoLabel;
}

mfxU8 GetPictureType(mfxU32 FrameType, mfxU32 BottomFieldFlag)
{
    mfxU8 fieldType = (mfxU8) (BottomFieldFlag ? (FrameType >> 4) : (FrameType & 0xf));
    return (mfxU8) (fieldType == MFX_FRAMETYPE_I ? Mfxh264SurfaceHolder::PicI :
                    fieldType == MFX_FRAMETYPE_P ? Mfxh264SurfaceHolder::PicP :
                    Mfxh264SurfaceHolder::PicB);
}

mfxU8 GetFrameFullness(mfxU32 FieldPicFlag, mfxU32 BottomFieldFlag)
{
    return (mfxU8) (!FieldPicFlag ? Mfxh264SurfaceHolder::PicFrame :
                    BottomFieldFlag ? Mfxh264SurfaceHolder::PicBot :
                    Mfxh264SurfaceHolder::PicTop);
}

bool Mfxh264SurfaceHolder::IsNewPic(mfxU32 FrameType, mfxU32 FieldPicFlag, mfxU32 BottomFieldFlag) const
{
    if (m_fullness == Mfxh264SurfaceHolder::PicNone || m_fullness & GetFrameFullness(FieldPicFlag, BottomFieldFlag))
        return true;
    if (m_type != Mfxh264SurfaceHolder::PicI && m_type != GetPictureType(FrameType, BottomFieldFlag))
        return true;
    return false;
}

void Mfxh264SurfaceHolder::SetFrameOrder(mfxU8 label)
{
    if (m_firstDispLabel == MfxNoLabel)
    {
        m_firstDispLabel = label;
        m_firstDispFrameOrder = m_nextFrameOrder;
    }
    m_fs.Data[label]->FrameOrder = m_nextFrameOrder;
    m_nextFrameOrder++;
}

void Mfxh264SurfaceHolder::PushEos()
{
    if (!m_bEos && m_labelPrev != MfxNoLabel)
    {
        m_bEos = true;
        if (m_type == PicB)
            SetFrameOrder(m_labelPrev);
        else
            SetFrameOrder(m_labelCur);
    }
}

mfxU8 Mfxh264SurfaceHolder::PushPic(mfxU32 FrameType, mfxU32 FieldPicFlag, mfxU32 BottomFieldFlag)
{
    if (m_bEos)
        return MfxNoLabel;

    mfxU8 label;
    if (IsNewPic(FrameType, FieldPicFlag, BottomFieldFlag))
    {
        mfxU8 newPicType = GetPictureType(FrameType, BottomFieldFlag);
        if (newPicType == PicB)
        {
            if (m_type == PicB)
            {
                if (m_labelCur == MfxNoLabel)
                    m_labelCur = FindAndLockFreeSurface();
                if (m_labelCur == MfxNoLabel)
                    return MfxNoLabel;
                label = m_labelCur;

                m_type = newPicType;
                m_fullness = GetFrameFullness(FieldPicFlag, BottomFieldFlag);
                SetFrameOrder(m_labelCur);
            }
            else
            {
                label = FindAndLockFreeSurface();
                if (label == MfxNoLabel)
                    return MfxNoLabel;

                if (m_labelPrevPrev != MfxNoLabel)
                    m_fs.Data[m_labelPrevPrev]->Locked = 0;

                m_labelPrevPrev = m_labelPrev;
                m_labelPrev = m_labelCur;
                m_labelCur = label;
                m_type = newPicType;
                m_fullness = GetFrameFullness(FieldPicFlag, BottomFieldFlag);

                SetFrameOrder(m_labelCur);
            }
        }
        else
        {
            if (m_type == PicB)
            {
                if (m_labelCur == MfxNoLabel)
                    m_labelCur = FindAndLockFreeSurface();
                if (m_labelCur == MfxNoLabel)
                    return MfxNoLabel;
                label = m_labelCur;

                m_type = newPicType;
                m_fullness = GetFrameFullness(FieldPicFlag, BottomFieldFlag);
                if (m_labelPrev != MfxNoLabel)
                    SetFrameOrder(m_labelPrev);
            }
            else
            {
                label = FindAndLockFreeSurface();
                if (label == MfxNoLabel)
                    return MfxNoLabel;

                if (m_labelPrevPrev != MfxNoLabel)
                    m_fs.Data[m_labelPrevPrev]->Locked = 0;

                m_labelPrevPrev = m_labelPrev;
                m_labelPrev = m_labelCur;
                m_labelCur = label;
                m_type = newPicType;
                m_fullness = GetFrameFullness(FieldPicFlag, BottomFieldFlag);

                if (m_labelPrev != MfxNoLabel)
                    SetFrameOrder(m_labelPrev);
            }
        }
    }
    else
    {
        label = m_labelCur;
        m_fullness |= GetFrameFullness(FieldPicFlag, BottomFieldFlag);
    }

    return label;
}

mfxU8 Mfxh264SurfaceHolder::PushPic(const mfxFrameParam& fPar)
{
    return PushPic(fPar.FrameType, fPar.FieldPicFlag, fPar.BottomFieldFlag);
}

void Mfxh264SurfaceHolder::PopDisplayable()
{
    if (m_firstDispLabel != MfxNoLabel)
    {
        m_fs.Data[m_firstDispLabel]->Locked = 0;
        //SetSurfaceFlag(m_fs.Data[m_firstDispLabel], MFX_LM_PRESENTED);
        m_firstDispFrameOrder++;
        m_firstDispLabel = (mfxU8) ((m_firstDispFrameOrder != m_nextFrameOrder) ? FindSurface(m_nextFrameOrder) : MfxNoLabel);
    }
}

mfxU8 Mfxh264SurfaceHolder::Displayable() const
{
    return m_firstDispLabel;
}
mfxU8 Mfxh264SurfaceHolder::FwdRef() const
{
    return (mfxU8) ((m_type == PicB) ? m_labelPrevPrev : (m_type == PicP ? m_labelPrev : MfxNoLabel));
}

mfxU8 Mfxh264SurfaceHolder::BwdRef() const
{
    return (mfxU8) ((m_type == PicB) ? m_labelPrev : MfxNoLabel);
}
