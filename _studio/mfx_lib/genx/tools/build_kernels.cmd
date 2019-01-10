SETLOCAL ENABLEDELAYEDEXPANSION

set ICLDIR=%MEDIASDK_ROOT%\cmrt
set RUNTIME_LIB=%ICLDIR%\runtime\lib
set COMPILER_LIB=%ICLDIR%\compiler\lib
set COMPILER_INC=%ICLDIR%\compiler\include
set RUNTIME_INC=%ICLDIR%\runtime\include

SET PATH=%ICLDIR%\compiler\bin;%PATH%
SET INCLUDE=%COMPILER_INC%;%COMPILER_INC%\cm;%COMPILER_INC%;%MSVSINCL%;%INCLUDE%
SET TMP=genx_temp

IF exist .\%TMP%\ ( rmdir .\%TMP% /S/Q )
mkdir %TMP%
cl tools\src\embed_isa.c
move embed_isa.exe %TMP%
cd %TMP%

echo ////////////////////////////////
echo //  ASC kernel compilation  //
echo ///////////////////////////////
for %%g in (8,9,11,11lp,12,12lp) do (
cmc.exe -mCM_init_global -c -Qxcm -Qxcm_jit_target=gen%%g ..\asc\src\genx_asc.cpp -Qxcm_print_asm_count -mCM_printregusage -menableiga -Dtarget_gen%%g -Qxcm_release -o asc_gen%%g.isa
embed_isa asc_gen%%g.isa
move asc_gen%%g_isa.h ..\asc\isa
move asc_gen%%g_isa.cpp ..\asc\isa
)

echo ////////////////////////////////
echo // MCTF kernel compilation //
echo ////////////////////////////////
for %%m in (me,mc,sd) do (
for %%g in (8,9,11,11lp,12,12lp) do (
cmc.exe -mCM_init_global -c -Qxcm -Qxcm_jit_target=gen%%g ..\mctf\src\genx_%%m.cpp -Qxcm_print_asm_count -mCM_printregusage -menableiga -Dtarget_gen%%g -Qxcm_release -o mctf_%%m_gen%%g.isa
embed_isa mctf_%%m_gen%%g.isa
move mctf_%%m_gen%%g_isa.h ..\mctf\isa
move mctf_%%m_gen%%g_isa.cpp ..\mctf\isa
)
)