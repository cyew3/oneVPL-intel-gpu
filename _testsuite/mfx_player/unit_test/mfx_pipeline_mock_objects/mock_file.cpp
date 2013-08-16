/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#include "mfx_pipeline_defs.h"
#include "mock_file.h"

mfxStatus MockFile::Open(const tstring & path, const tstring & access)
{
    //todo simplify this
    TEST_METHOD_TYPE(Open) params_holder(MFX_ERR_NONE
        , mfxTypeTrait<const tstring&>::assign(path)
        , mfxTypeTrait<const tstring&>::assign(access));

    _Open.RegisterEvent(params_holder);

    return MFX_ERR_NONE;
}

mfxStatus MockFile::Close()
{
    //todo simplify this
    TEST_METHOD_TYPE(Close) params_holder(MFX_ERR_NONE);

    _Close.RegisterEvent(params_holder);

    return MFX_ERR_NONE;
}

mfxStatus MockFile::Reopen()
{
    //todo simplify this
    TEST_METHOD_TYPE(Reopen) params_holder(MFX_ERR_NONE);

    _Close.RegisterEvent(params_holder);

    return MFX_ERR_NONE;
}

mfxStatus MockFile::Read(mfxU8* pBuffer, mfxU32 &nSize)
{
    TEST_METHOD_TYPE(Read) params_holder(MFX_ERR_NONE, pBuffer
        , mfxTypeTrait<mfxU32&>::assign(nSize));
    _Read.RegisterEvent(params_holder);

    return MFX_ERR_NONE;
}

mfxStatus MockFile::Write(mfxU8* pBuffer, mfxU32 nSize)
{
    TEST_METHOD_TYPE(Write) params_holder(MFX_ERR_NONE, pBuffer, nSize);
    _Write.RegisterEvent(params_holder);

    return MFX_ERR_NONE;
}

mfxStatus MockFile::Write(mfxBitstream *pbs)
{
    if (!pbs)
        return MFX_ERR_NONE;

    TEST_METHOD_TYPE(Write) params_holder(MFX_ERR_NONE, pbs->Data + pbs->DataOffset, pbs->DataLength);
    _Write.RegisterEvent(params_holder);

    return MFX_ERR_NONE;
}

mfxStatus MockFile::Seek(Ipp64s position, VM_FILE_SEEK_MODE mode)
{
    TEST_METHOD_TYPE(Seek) params_holder(MFX_ERR_NONE, position, mode);
    _Seek.RegisterEvent(params_holder);

    return MFX_ERR_NONE;
}

bool MockFile::isOpen()
{
    TEST_METHOD_TYPE(isOpen) params_holder;
    if (!_isOpen.GetReturn(params_holder))
    {
        //no strategy
        return false;
    }
    return params_holder.ret_val;
}
