/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#include <vector>

#define INIT1()         _storage.push_back(a1);
#define INIT2() INIT1() _storage.push_back(a2);
#define INIT3() INIT2() _storage.push_back(a3);
#define INIT4() INIT3() _storage.push_back(a4);
#define INIT5() INIT4() _storage.push_back(a5);
#define INIT6() INIT5() _storage.push_back(a6);
#define INIT7() INIT6() _storage.push_back(a7);
#define INIT8() INIT7() _storage.push_back(a8);
#define INIT9() INIT8() _storage.push_back(a9);
#define INIT10() INIT9() _storage.push_back(a10);
#define INIT11() INIT10() _storage.push_back(a11);
#define INIT12() INIT11() _storage.push_back(a12);
#define INIT13() INIT12() _storage.push_back(a13);
#define INIT14() INIT13() _storage.push_back(a14);
#define INIT15() INIT14() _storage.push_back(a15);
#define INIT16() INIT15() _storage.push_back(a16);
#define INIT17() INIT16() _storage.push_back(a17);
#define INIT18() INIT17() _storage.push_back(a18);
#define INIT19() INIT18() _storage.push_back(a19);
#define INIT20() INIT19() _storage.push_back(a20);
#define INIT21() INIT20() _storage.push_back(a21);
#define INIT22() INIT21() _storage.push_back(a22);
#define INIT23() INIT22() _storage.push_back(a23);
#define INIT24() INIT23() _storage.push_back(a24);
#define INIT25() INIT24() _storage.push_back(a25);
#define INIT26() INIT25() _storage.push_back(a26);
#define INIT27() INIT26() _storage.push_back(a27);
#define INIT28() INIT27() _storage.push_back(a28);
#define INIT29() INIT28() _storage.push_back(a29);
#define INIT30() INIT29() _storage.push_back(a30);


#define PARAM1() mfxU32 size, mfxU8 a1
#define PARAM2() PARAM1(), mfxU8 a2
#define PARAM3() PARAM2(), mfxU8 a3
#define PARAM4() PARAM3(), mfxU8 a4
#define PARAM5() PARAM4(), mfxU8 a5
#define PARAM6() PARAM5(), mfxU8 a6
#define PARAM7() PARAM6(), mfxU8 a7
#define PARAM8() PARAM7(), mfxU8 a8
#define PARAM9() PARAM8(), mfxU8 a9
#define PARAM10() PARAM9(), mfxU8 a10
#define PARAM11() PARAM10(), mfxU8 a11
#define PARAM12() PARAM11(), mfxU8 a12
#define PARAM13() PARAM12(), mfxU8 a13
#define PARAM14() PARAM13(), mfxU8 a14
#define PARAM15() PARAM14(), mfxU8 a15
#define PARAM16() PARAM15(), mfxU8 a16
#define PARAM17() PARAM16(), mfxU8 a17
#define PARAM18() PARAM17(), mfxU8 a18
#define PARAM19() PARAM18(), mfxU8 a19
#define PARAM20() PARAM19(), mfxU8 a20
#define PARAM21() PARAM20(), mfxU8 a21
#define PARAM22() PARAM21(), mfxU8 a22
#define PARAM23() PARAM22(), mfxU8 a23
#define PARAM24() PARAM23(), mfxU8 a24
#define PARAM25() PARAM24(), mfxU8 a25
#define PARAM26() PARAM25(), mfxU8 a26
#define PARAM27() PARAM26(), mfxU8 a27
#define PARAM28() PARAM27(), mfxU8 a28
#define PARAM29() PARAM28(), mfxU8 a29
#define PARAM30() PARAM29(), mfxU8 a30




class mfxTestBitstream : public mfxBitstream2
{
    std::vector<mfxU8> _storage;
    void set_bs_pointers() {
        Data = &_storage.front();
        MaxLength = (mfxU32) _storage.size();
    }

public:
    mfxTestBitstream(PARAM2())  : _storage(size) { INIT2();set_bs_pointers();}
    mfxTestBitstream(PARAM3())  : _storage(size) { INIT3();set_bs_pointers();}
    mfxTestBitstream(PARAM4())  : _storage(size) { INIT4();set_bs_pointers();}
    mfxTestBitstream(PARAM5())  : _storage(size) { INIT5();set_bs_pointers();}
    mfxTestBitstream(PARAM6())  : _storage(size) { INIT6();set_bs_pointers();}
    mfxTestBitstream(PARAM7())  : _storage(size) { INIT7();set_bs_pointers();}
    mfxTestBitstream(PARAM8())  : _storage(size) { INIT8();set_bs_pointers();}
    mfxTestBitstream(PARAM9())  : _storage(size) { INIT9();set_bs_pointers();}
    mfxTestBitstream(PARAM10()) : _storage(size) { INIT10();set_bs_pointers();}
    mfxTestBitstream(PARAM11()) : _storage(size) { INIT11();set_bs_pointers();}
    mfxTestBitstream(PARAM12()) : _storage(size) { INIT12();set_bs_pointers();}
    mfxTestBitstream(PARAM13()) : _storage(size) { INIT13();set_bs_pointers();}
    mfxTestBitstream(PARAM14()) : _storage(size) { INIT14();set_bs_pointers();}
    mfxTestBitstream(PARAM15()) : _storage(size) { INIT15();set_bs_pointers();}
    mfxTestBitstream(PARAM16()) : _storage(size) { INIT16();set_bs_pointers();}
    mfxTestBitstream(PARAM17()) : _storage(size) { INIT17();set_bs_pointers();}
    mfxTestBitstream(PARAM18()) : _storage(size) { INIT18();set_bs_pointers();}
    mfxTestBitstream(PARAM19()) : _storage(size) { INIT19();set_bs_pointers();}
    mfxTestBitstream(PARAM20()) : _storage(size) { INIT20();set_bs_pointers();}
    mfxTestBitstream(PARAM21()) : _storage(size) { INIT21();set_bs_pointers();}
    mfxTestBitstream(PARAM22()) : _storage(size) { INIT22();set_bs_pointers();}
    mfxTestBitstream(PARAM23()) : _storage(size) { INIT23();set_bs_pointers();}
    mfxTestBitstream(PARAM24()) : _storage(size) { INIT24();set_bs_pointers();}
    mfxTestBitstream(PARAM25()) : _storage(size) { INIT25();set_bs_pointers();}
    mfxTestBitstream(PARAM26()) : _storage(size) { INIT26();set_bs_pointers();}
    mfxTestBitstream(PARAM27()) : _storage(size) { INIT27();set_bs_pointers();}
    mfxTestBitstream(PARAM28()) : _storage(size) { INIT28();set_bs_pointers();}
    mfxTestBitstream(PARAM29()) : _storage(size) { INIT29();set_bs_pointers();}
    mfxTestBitstream(PARAM30()) : _storage(size) { INIT30();set_bs_pointers();}

    mfxTestBitstream(mfxU32 size) : _storage(size) {
        set_bs_pointers();
    }
    mfxU8* Buffer() {
        return &_storage.front();
    }

    
};