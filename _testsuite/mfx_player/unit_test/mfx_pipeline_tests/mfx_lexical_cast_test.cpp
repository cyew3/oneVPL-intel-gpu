/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#include "utest_common.h"
#include "mfx_lexical_cast.h"

SUITE(LexicalCast)
{
    struct Init
    {
        int     i32;
        short   i16;
        char    i8;
        __int64 i64;

        float   f32;
        double  f64;

        unsigned int     u32;
        unsigned short   u16;
        unsigned char    u8;
        unsigned __int64 u64;

        tstring e;

    };
    TEST_FIXTURE(Init, signed_int_input)
    {
        try{
            lexical_cast(_T("qqq"), i32);}
        catch (tstring & error){
            e = error;}
        CHECK(!e.empty());
        e.clear();

        try{
            lexical_cast(_T("12qqq"), i32);}
        catch (tstring & error){
            e = error;}
        CHECK(!e.empty());
        e.clear();

        try{
            lexical_cast(_T("12e2"), i32);}
        catch (tstring & error){
            e = error;}
        CHECK(!e.empty());
        e.clear();

        try{
            lexical_cast(_T("-200"), i8);}
        catch (tstring & error){
            e = error;}
        CHECK(!e.empty());
        e.clear();

        try{
            lexical_cast(_T("200"), i8);}
        catch (tstring & error){
            e = error;}
        CHECK(!e.empty());
        e.clear();

        try{
            lexical_cast(_T("20"), i8);}
        catch (tstring & error){
            e = error;}
        CHECK(e.empty());
        e.clear();

        try{
            lexical_cast(_T("-20"), i8);}
        catch (tstring & error){
            e = error;}
        CHECK(e.empty());
        e.clear();

        try{
            lexical_cast(_T("-42000"), i16);}
        catch (tstring & error){
            e = error;}
        CHECK(!e.empty());
        e.clear();

        try{
            lexical_cast(_T("42000"), i16);}
        catch (tstring & error){
            e = error;}
        CHECK(!e.empty());
        e.clear();

        try{
            lexical_cast(_T("20000"), i16);}
        catch (tstring & error){
            e = error;}
        CHECK(e.empty());
        e.clear();

        try{
            lexical_cast(_T("-20000"), i16);}
        catch (tstring & error){
            e = error;}
        CHECK(e.empty());
        e.clear();
        
        try{
            lexical_cast(_T("-3000000000"), i32);}
        catch (tstring & error){
            e = error;}
        CHECK(!e.empty());
        e.clear();

        try{
            lexical_cast(_T("3000000000"), i32);}
        catch (tstring & error){
            e = error;}
        CHECK(!e.empty());
        e.clear();

        try{
            lexical_cast(_T("2000000000"), i32);}
        catch (tstring & error){
            e = error;}
        CHECK(e.empty());
        e.clear();

        try{
            lexical_cast(_T("-2000000000"), i32);}
        catch (tstring & error){
            e = error;}
        CHECK(e.empty());
        e.clear();

        try{
            lexical_cast(_T("-10223372036854775808"), i64);}
        catch (tstring & error){
            e = error;}
        CHECK(!e.empty());
        e.clear();

        try{
            lexical_cast(_T("10223372036854775808"), i64);}
        catch (tstring & error){
            e = error;}
        CHECK(!e.empty());
        e.clear();

        try{
            lexical_cast(_T("8223372036854775808"), i64);}
        catch (tstring & error){
            e = error;}
        CHECK(e.empty());
        e.clear();

        try{
            lexical_cast(_T("-8223372036854775808"), i64);}
        catch (tstring & error){
            e = error;}
        CHECK(e.empty());
        e.clear();
    }

    TEST_FIXTURE(Init, usigned_int_input)
    {
        try{
            lexical_cast(_T("qqq"), u32);}
        catch (tstring & error){
            e = error;}
        CHECK(!e.empty());
        e.clear();

        try{
            lexical_cast(_T("12qqq"), u32);}
        catch (tstring & error){
            e = error;}
        CHECK(!e.empty());
        e.clear();

        //not parsing double
        try{
            lexical_cast(_T("1e2"), u32);}
        catch (tstring & error){
            e = error;}
        CHECK(!e.empty());
        e.clear();

        //not parsing double
        try{
            lexical_cast(_T("12.4"), u32);}
        catch (tstring & error){
            e = error;}
        CHECK(!e.empty());
        e.clear();

        try{
            lexical_cast(_T("12e2"), u32);}
        catch (tstring & error){
            e = error;}
        CHECK(!e.empty());
        e.clear();

        try{
            lexical_cast(_T("-200"), u8);}
        catch (tstring & error){
            e = error;}
        CHECK(!e.empty());
        e.clear();

        try{
            lexical_cast(_T("200"), u8);}
        catch (tstring & error){
            e = error;}
        CHECK(e.empty());
        e.clear();

        try{
            lexical_cast(_T("300"), u8);}
        catch (tstring & error){
            e = error;}
        CHECK(!e.empty());
        e.clear();

        try{
            lexical_cast(_T("20"), u8);}
        catch (tstring & error){
            e = error;}
        CHECK(e.empty());
        e.clear();

        try{
            lexical_cast(_T("-20"), u8);}
        catch (tstring & error){
            e = error;}
        CHECK(!e.empty());
        e.clear();

        try{
            lexical_cast(_T("-42000"), u16);}
        catch (tstring & error){
            e = error;}
        CHECK(!e.empty());
        e.clear();

        try{
            lexical_cast(_T("42000"), u16);}
        catch (tstring & error){
            e = error;}
        CHECK(e.empty());
        e.clear();

        try{
            lexical_cast(_T("70000"), u16);}
        catch (tstring & error){
            e = error;}
        CHECK(!e.empty());
        e.clear();

        try{
            lexical_cast(_T("-20000"), u16);}
        catch (tstring & error){
            e = error;}
        CHECK(!e.empty());
        e.clear();

        try{
            lexical_cast(_T("-1000000000"), u32);}
        catch (tstring & error){
            e = error;}
        CHECK(!e.empty());
        e.clear();

        try{
            lexical_cast(_T("-3000000000"), u32);}
        catch (tstring & error){
            e = error;}
        CHECK(!e.empty());
        e.clear();

        try{
            lexical_cast(_T("3000000000"), u32);}
        catch (tstring & error){
            e = error;}
        CHECK(e.empty());
        e.clear();

        try{
            lexical_cast(_T("5000000000"), u32);}
        catch (tstring & error){
            e = error;}
        CHECK(!e.empty());
        e.clear();

        try{
            lexical_cast(_T("-5223372036854775808"), u64);}
        catch (tstring & error){
            e = error;}
        CHECK(!e.empty());
        e.clear();

        try{
            lexical_cast(_T("-10223372036854775808"), u64);}
        catch (tstring & error){
            e = error;}
        CHECK(!e.empty());
        e.clear();

        try{
            lexical_cast(_T("19223372036854775808"), u64);}
        catch (tstring & error){
            e = error;}
        CHECK(!e.empty());
        e.clear();

        try{
            lexical_cast(_T("18223372036854775808"), u64);}
        catch (tstring & error){
            e = error;}
        CHECK(e.empty());
        e.clear();
    }

    TEST_FIXTURE(Init, floating_point)
    {
        try{
            lexical_cast(_T("12.2"), f32);}
        catch (tstring & error){
            e = error;}
        CHECK(e.empty());
        e.clear();

        try{
            lexical_cast(_T("12.2e3"), f32);}
        catch (tstring & error){
            e = error;}
        CHECK(e.empty());
        e.clear();

        try{
            lexical_cast(_T("12.2a3"), f32);}
        catch (tstring & error){
            e = error;}
        CHECK(!e.empty());
        e.clear();

        try{
            lexical_cast(_T("12.2e305"), f32);}
        catch (tstring & error){
            e = error;}
        CHECK(!e.empty());
        e.clear();

        try{
            lexical_cast(_T("12.2e305"), f64);}
        catch (tstring & error){
            e = error;}
        CHECK(e.empty());
        e.clear();

        try{
            lexical_cast(_T("12.2e3105"), f64);}
        catch (tstring & error){
            e = error;}
        CHECK(!e.empty());
        e.clear();

    }

    TEST_FIXTURE(Init, string_parse)
    {
        tstring res;
        try{lexical_cast(_T("12.2e3105"), res);}
        catch (tstring & error){e = error;}
        CHECK(e.empty());
        e.clear();

        try{lexical_cast(_T("12.2e3105 222"), res);}
        catch (tstring & error){e = error;}
        CHECK(!e.empty());
        e.clear();
    }

}