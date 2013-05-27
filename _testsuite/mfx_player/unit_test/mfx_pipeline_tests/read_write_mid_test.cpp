/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2010 - 2011 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#include "utest_common.h"
#include "mfx_d3d11_allocator.h"

SUITE(READ_WRIE_MID)
{
    TEST(Generic_write)
    {
        mfxMemId id = (mfxMemId)666;

        MFXReadWriteMid md(id, MFXReadWriteMid::write);
        CHECK(md.isWrite());
        CHECK(!md.isRead());

        MFXReadWriteMid md2(id, MFXReadWriteMid::read | MFXReadWriteMid::write);
        CHECK(md2.isWrite());
        CHECK(md2.isRead());

        MFXReadWriteMid md3(id, MFXReadWriteMid::read);
        CHECK(md3.isRead());
        CHECK(!md3.isWrite());

        MFXReadWriteMid md4(id);
        CHECK(md4.isWrite());
        CHECK(md4.isRead());
    }

    TEST(read_to_any)
    {
        mfxMemId id = (mfxMemId)666;

        MFXReadWriteMid md(id, MFXReadWriteMid::read);
        MFXReadWriteMid md2(md, MFXReadWriteMid::read | MFXReadWriteMid::write);
        CHECK(md2.isRead());
        CHECK(md2.isWrite());

        MFXReadWriteMid md3(md, MFXReadWriteMid::write);
        CHECK(!md3.isRead());
        CHECK(md3.isWrite());

        MFXReadWriteMid md4(md, MFXReadWriteMid::read);
        CHECK(md4.isRead());
        CHECK(!md4.isWrite());
    }

    TEST(write_to_any)
    {
        mfxMemId id = (mfxMemId)666;

        MFXReadWriteMid md(id, MFXReadWriteMid::write);
        MFXReadWriteMid md2(md, MFXReadWriteMid::read | MFXReadWriteMid::write);
        CHECK(md2.isRead());
        CHECK(md2.isWrite());

        MFXReadWriteMid md3(md, MFXReadWriteMid::write);
        CHECK(!md3.isRead());
        CHECK(md3.isWrite());

        MFXReadWriteMid md4(md, MFXReadWriteMid::read);
        CHECK(md4.isRead());
        CHECK(!md4.isWrite());
    }

    TEST(read_and_write_to_any)
    {
        mfxMemId id = (mfxMemId)666;

        MFXReadWriteMid md(id, MFXReadWriteMid::read | MFXReadWriteMid::write);
        MFXReadWriteMid md2(md, MFXReadWriteMid::read | MFXReadWriteMid::write);
        CHECK(md2.isRead());
        CHECK(md2.isWrite());

        MFXReadWriteMid md3(md, MFXReadWriteMid::write);
        CHECK(!md3.isRead());
        CHECK(md3.isWrite());

        MFXReadWriteMid md4(md, MFXReadWriteMid::read);
        CHECK(md4.isRead());
        CHECK(!md4.isWrite());
    }

    TEST(pack_unpack)
    {
        mfxMemId id = (mfxMemId)666;

        MFXReadWriteMid md(id, MFXReadWriteMid::write);
        CHECK_EQUAL(id, md.raw());

        MFXReadWriteMid md2(id, MFXReadWriteMid::read | MFXReadWriteMid::write);
        CHECK_EQUAL(id, md2.raw());

        MFXReadWriteMid md3(id, MFXReadWriteMid::read);
        CHECK_EQUAL(id, md3.raw());

        CHECK_EQUAL(id, MFXReadWriteMid(id).raw());
    }

}