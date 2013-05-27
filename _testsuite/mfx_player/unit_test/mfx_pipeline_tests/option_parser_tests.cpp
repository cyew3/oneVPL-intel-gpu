/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2010-2011 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#include "utest_common.h"
#include "mfx_cmd_option_processor.h"
#include "option_parser_tests.h"

SUITE(ComdOptionProcessor)
{
    struct
    { 
        vm_char str[128];
        vm_char str_no_special[128];
        bool result;
    } patterns_1[] = 
    {
        VM_STRING(")"), VM_STRING(""), false,
        VM_STRING("sss)"),VM_STRING("sss"), false,
        VM_STRING("("), VM_STRING(""), false,
        VM_STRING("()"), VM_STRING(""), true,
        VM_STRING(")("), VM_STRING(""), false,
        VM_STRING("((((("), VM_STRING(""), false,
        VM_STRING("(((((a"), VM_STRING("a"), false,
        VM_STRING("( ( ) ( ) () (ss) )"), VM_STRING("      ss "), true,
        VM_STRING("( ( (((s)()((s)) ( ) () () ) )as)ass1sss)"), VM_STRING("  ss      asass1sss"), true,
        VM_STRING("a((((b(((((((((c(((((((de))))fg)))hi)))jk(((((l(m(n(()|op))))))))))))))))))r"), VM_STRING("abcdefghijklmnopr"), true
    };

    TEST(OptionCallbacVerifier)
    {
        int i;
        for ( i = 0; i < sizeof(patterns_1)/sizeof(patterns_1[0]); i++)
        {
            //printf("OptionVerifier run=%d\n", i);
            OptionCallbacVerifier verifier;
            CHECK(patterns_1[i].result == OptionPatternParser::Parse(patterns_1[i].str, &verifier));
        }
    }

    TEST(SpecialItemSkipper)
    {
        int i;
        for ( i = 0; i < sizeof(patterns_1)/sizeof(patterns_1[0]); i++)
        {
            //printf("OptionVerifier run=%d\n", i);
            vm_char buf[128];
            SpecialItemSkipper skipper(buf);
            OptionPatternParser::Parse(patterns_1[i].str, &skipper);

            CHECK(!_tcscmp(patterns_1[i].str_no_special, buf));
        }
    }

    TEST(OptionsContainer)
    {
        vm_char pattern[][1024] = 
        {
            VM_STRING("abc"),
            VM_STRING("abc|efd"),
            VM_STRING("()(a)"),
            VM_STRING("(((a|(b)x|u((e)s|d))|c"),
            VM_STRING("(((a)|b))|c"),
            VM_STRING("(((a)|b))c"),
            VM_STRING("((a))c"),
            VM_STRING("ax|(b|c|d|)er|(g)"),
            VM_STRING("(a)((b))((((c)))"),
            VM_STRING("a(b|c|d)u(z|m)x"),
            VM_STRING("ax(b|c|d|)er|(s|p)g"),
            VM_STRING("ax(b|c|d)|er|(g)"),
        };

        struct {
            int   number;//number of valid cases
            vm_char str[10][12];
            
        }pattern_result[] = {
            {1,VM_STRING("abc")},
            {2,VM_STRING("abc"), VM_STRING("efd")},
            {1,VM_STRING("a")},
            {5,VM_STRING("a"),VM_STRING("bx"),VM_STRING("c"),VM_STRING("ud"),VM_STRING("ues")},
            {3,VM_STRING("a"),VM_STRING("b"),VM_STRING("c"),VM_STRING("er"),VM_STRING("g")},
            {2,VM_STRING("ac"),VM_STRING("bc"),VM_STRING("axd"),VM_STRING("er"),VM_STRING("g")},
            {1,VM_STRING("ac"),VM_STRING("bc"),VM_STRING("axd"),VM_STRING("er"),VM_STRING("g")},
            {5, VM_STRING("ax"),VM_STRING("g"),VM_STRING("ber"),VM_STRING("cer"),VM_STRING("der")},
            {1,VM_STRING("abc"),VM_STRING("axc"),VM_STRING("axd"),VM_STRING("er"),VM_STRING("g")},
            {6,VM_STRING("abuzx"),VM_STRING("abumx"),VM_STRING("acuzx"),VM_STRING("acumx"),VM_STRING("aduzx"),VM_STRING("adumx")},
            {5,VM_STRING("axber"),VM_STRING("axcer"),VM_STRING("axder"),VM_STRING("sg"),VM_STRING("pg")},
            {5,VM_STRING("axb"),VM_STRING("axc"),VM_STRING("axd"),VM_STRING("er"),VM_STRING("g")},
        };

        /*proc.Check(opt, pattern, NULL, "description");*/

        int i;
        int j;
        for (j = 0; j <sizeof(pattern_result)/sizeof(pattern_result[0]); j++)
        {
            OptionsContainer container;
            OptionPatternParser::Parse(pattern[j], &container);
            std::list<tstring> & all_opt = container.get_all_options();
            std::list<tstring>::iterator it;
            
            //printf("OptionsContainer::LenghtMatch=%d\n",j);

            CHECK(pattern_result[j].number == (int)all_opt.size());
         
            //all required case matches
            for ( i = 0; i < pattern_result[j].number; i++)
            {
                //printf("OptionsContainer::Run=%d,%d\n",j,i);
                bool bMatched = false;
                for (it = all_opt.begin(); it != all_opt.end(); it++)
                {
                    if (!_tcscmp(it->c_str(), pattern_result[j].str[i]))
                    {
                        bMatched = true;
                        break;
                    }
                }
                CHECK(bMatched);
            }
        }
    }
    TEST(CmdOptionProcessor)
    {
        vm_char pattern[][1024] = 
        {
            VM_STRING("-i:(yv12|nv12)"),
            VM_STRING("-hw"),
            VM_STRING("-hw:(1|2|3|4|any|emulate_sw)"),
            VM_STRING("(-| )(m2|mpeg2|avc)"),
        };

        struct {
            int   number;//number of valid cases
            struct
            {
                vm_char str[20];
                bool isValid;
            }cases[10];
        }pattern_result[] = {
             2,{{VM_STRING("-i:YV12"), true},
                {VM_STRING("-i:NV12"), true}},
             1,{VM_STRING("-hw"), true},
             7,{{VM_STRING("-hw:1"),true},
                {VM_STRING("-hw:2"),true},
                {VM_STRING("-hw:3"), true},
                {VM_STRING("-hw:4"),true},
                {VM_STRING("-hw:any"),true},
                {VM_STRING("-hw:emulate_sw"),true},
                {VM_STRING("-hw:emuate_sw"),false}},
             6,{{VM_STRING("-m2"),true},
                {VM_STRING("-mpeg2"),true},
                {VM_STRING("-avc"), true},
                {VM_STRING("m2"),true},
                {VM_STRING("mpeg2"),true},
                {VM_STRING("avc"),true}},
        };

        CmdOptionProcessor proc(false);
        
        
        for (int i = 0 ; i< sizeof(pattern)/sizeof(pattern[0]);i++)
        {
            for (int j = 0; j < pattern_result[i].number; j++)
            {
                //_tprintf(VM_STRING("CmdOptionProcessor:check :%s\n"), pattern_result[i].cases[j].str);
                int nOrder = pattern_result[i].cases[j].isValid ? j+1 : 0;
                CHECK(nOrder == (int)proc.Check(pattern_result[i].cases[j].str, pattern[i], VM_STRING("dsc")));
            }
        }
    }
#if 0
    TEST(CmdOptionProcessor_QUANTIFICATOR)
    {
        vm_char pattern[][1024] = 
        {
            VM_STRING("-o(:a){,1}(:b){,1}")
        };

        struct {
            int   number;//number of valid cases
            struct
            {
                vm_char str[20];
                bool isValid;
            }cases[10];
        }pattern_result[] = {
            4,{{VM_STRING("-o"),true},
            {VM_STRING("-o:a"),true},
            {VM_STRING("-o:b"), true},
            {VM_STRING("-o:a:b"),true}},
        };

        CmdOptionProcessor proc(false);


        for (int i = 0 ; i< sizeof(pattern)/sizeof(pattern[0]);i++)
        {
            for (int j = 0; j < pattern_result[i].number; j++)
            {
                _tprintf(VM_STRING("CmdOptionProcessor:check :%s\n"), pattern_result[i].cases[j].str);
                int nOrder = pattern_result[i].cases[j].isValid ? j+1 : 0;
                CHECK(nOrder == (int)proc.Check(pattern_result[i].cases[j].str, pattern[i], VM_STRING("dsc")));
            }
        }
    }

    TEST(CmdOptionProcessor_Printer)
    {
        //uses redirection of printf to temporary file approach to catch print help
        vm_char pattern[][1024] = 
        {
            VM_STRING("-i:(yv12|nv12)"),
            VM_STRING("-hw"), 
            VM_STRING("-hw:(1|2|3|4|any|emulate_sw)"),
        };
        
        vm_char *pFilePath;
        pFilePath = _ttempnam(VM_STRING(".\\"), VM_STRING(""));

        FILE * ftmp;
        _tfopen_s(&ftmp, pFilePath, VM_STRING("w+"));

        int saved_stdout = _dup(_fileno(stdout));
        CHECK(-1 != _dup2(_fileno(ftmp), _fileno(stdout)));

        vm_char printed[][256] = 
        {
            VM_STRING("    -i:yv12  (-i:nv12)      (no arg  ) TODO: add option description\n"),
            VM_STRING("    -hw                     (no arg  ) TODO: add option description\n"),
            VM_STRING("    -hw:1  ( -hw:2, -hw:3, -hw:4, -hw:any, -hw:emulate_sw) (no arg  ) TODO: add option description\n")
        };

        CmdOptionProcessor printer(true, false);
        
        for (int i = 0 ; i< sizeof(pattern)/sizeof(pattern[0]);i++)
        {
            fpos_t pos = 0;
            vm_char line[256]={0};

            fflush(stdout);
            fsetpos(ftmp, &pos);

            CHECK((int)printer.Check(VM_STRING(""), pattern[i], VM_STRING("TODO: add option description")));

            fflush(stdout);
            fsetpos(ftmp, &pos);
            
            _fgetts(line, sizeof(line)/sizeof(line[0]), ftmp);

            CHECK_EQUAL(convert_w2s(printed[i]).c_str(), convert_w2s(line).c_str());
        }

        _dup2(saved_stdout, _fileno(stdout));
        fclose(ftmp);
    }
    #endif
}


