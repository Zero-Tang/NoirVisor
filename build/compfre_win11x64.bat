@echo off
set ddkpath=V:\Program Files\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.31.31103
set path=%ddkpath%\bin\Hostx64\x64;V:\Program Files\Windows Kits\10\bin\10.0.22621.0\x64;%path%
set incpath=V:\Program Files\Windows Kits\10\Include\10.0.22621.0
set libpath=V:\Program Files\Windows Kits\10\Lib
set binpath=..\bin\compfre_win11x64
set objpath=..\bin\compfre_win11x64\Intermediate

title Compiling NoirVisor, Free Build, 64-Bit Windows (AMD64 Architecture)
echo Project: NoirVisor
echo Platform: 64-Bit Windows
echo Preset: Release/Free Build
echo Powered by zero.tangptr@gmail.com
echo Copyright (c) 2018-2023, zero.tangptr@gmail.com. All Rights Reserved.
if "%~1"=="/s" (echo DO-NOT-PAUSE is activated!) else (pause)

echo ============Start Compiling============
echo Compiling Windows Driver Framework...
cl ..\src\booting\windrv\driver.c /I"%incpath%\km\crt" /I"%incpath%\shared" /I"%incpath%\km" /Zi /nologo /W3 /WX /wd4311 /O2 /D"_AMD64_" /D"_M_AMD64" /D"_WIN64" /D "_NDEBUG" /D"_UNICODE" /D "UNICODE" /Zc:wchar_t /std:c17 /FAcs /Fa"%objpath%\driver.cod" /Fo"%objpath%\driver.obj" /Fd"%objpath%\vc140.pdb" /GS- /GF /Gy /Qspectre /TC /c /errorReport:queue

rc /nologo /i"%incpath%\shared" /i"%incpath%\um" /i"%incpath%\km\crt" /d"_AMD64_" /fo"%objpath%\version.res" /n ..\src\booting\windrv\version.rc

echo Compiling NoirVisor CVM Emulator...
cl ..\src\disasm\emulator.c /I"..\src\include" /I"..\src\disasm\zydis\include" /I"..\src\disasm\zydis\dependencies\zycore\include" /I"..\src\disasm\zydis\msvc" /nologo /Zi /W3 /WX /Od /Oi /D"ZYDIS_STATIC_BUILD" /D"ZYAN_NO_LIBC" /D"_msvc" /D"_amd64" /D"_emulator" /FAcs /Fa"%objpath%\emulator.cod" /Fo"%objpath%\emulator.obj" /Fd"%objpath%\vc140.pdb" /GS- /Gr /Qspectre /TC /c /errorReport:queue

echo Compiling Core Engine of Intel VT-x...
for %%1 in (..\src\vt_core\*.c) do (cl %%1 /I"..\src\include" /Zi /nologo /W3 /WX /Oi /O2 /favor:INTEL64 /D"_msvc" /D"_amd64" /D"_vt_core" /D"_%%~n1" /Zc:wchar_t /std:c17 /FAcs /Fa"%objpath%\%%~n1.cod" /Fo"%objpath%\%%~n1.obj" /Fd"%objpath%\vc140.pdb" /GS- /GF /Gy /Qspectre /TC /c /errorReport:queue)

echo Compiling Core Engine of AMD-V...
for %%1 in (..\src\svm_core\*.c) do (cl %%1 /I"..\src\include" /Zi /nologo /W3 /WX /Oi /O2 /favor:AMD64 /D"_msvc" /D"_amd64" /D"_svm_core" /D"_%%~n1" /Zc:wchar_t /std:c17 /FAcs /Fa"%objpath%\%%~n1.cod" /Fo"%objpath%\%%~n1.obj" /Fd"%objpath%\vc140.pdb" /GS- /GF /Gy /Qspectre /TC /c /errorReport:queue)

echo Compiling Core Engine of Microsoft Hypervisor (MSHV)...
for %%1 in (..\src\mshv_core\*.c) do (cl %%1 /I"..\src\include" /Zi /nologo /W3 /WX /Oi /O2 /D"_msvc" /D"_amd64" /D"_mshv_core" /D"_%%~n1" /std:c17 /Zc:wchar_t /FAcs /Fa"%objpath%\%%~n1.cod" /Fo"%objpath%\%%~n1.obj" /Fd"%objpath%\vc140.pdb" /GS- /GF /Gy /Qspectre /TC /c /errorReport:queue)

echo Compiling Core of Cross-Platform Framework (XPF)...
for %%1 in (..\src\xpf_core\windows\*.c) do (cl %%1 /I"%incpath%\km\crt" /I"%incpath%\shared" /I"%incpath%\km" /Zi /nologo /W3 /WX /wd4311 /wd4996 /O2 /Oi /D"_KERNEL_MODE" /D"_AMD64_" /D"_M_AMD64" /D"_WIN64" /D "_NDEBUG" /D"_UNICODE" /D "UNICODE" /D"_%%~n1" /Zc:wchar_t /std:c17 /FAcs /Fa"%objpath%\%%~n1.cod" /Fo"%objpath%\%%~n1.obj" /Fd"%objpath%\vc140.pdb" /GS- /GF /Gy /Qspectre /TC /c /errorReport:queue)

cl ..\src\xpf_core\noirhvm.c /I"..\src\include" /Zi /nologo /W3 /WX /Oi /O2 /D"_msvc" /D"_amd64" /D"_central_hvm" /Zc:wchar_t /std:c17 /FAcs /Fa"%objpath%\noirhvm.cod" /Fo"%objpath%\noirhvm.obj" /Fd"%objpath%\vc140.pdb" /GS- /GF /Gy /Qspectre /TC /c /errorReport:queue

cl ..\src\xpf_core\ci.c /I"..\src\include" /Zi /nologo /W3 /WX /Oi /O2 /D"_msvc" /D"_amd64" /D"_code_integrity" /Zc:wchar_t /std:c17 /FAcs /Fa"%objpath%\ci.cod" /Fo"%objpath%\ci.obj" /Fd"%objpath%\vc140.pdb" /GS- /GF /Gy /Qspectre /TC /c /errorReport:queue

cl ..\src\xpf_core\devkits.c /I"..\src\include" /I"%ddkpath%\include" /Zi /nologo /W3 /WX /Oi /O2 /D"_msvc" /D"_amd64" /D"_dev_kits" /Zc:wchar_t /std:c17 /FAcs /Fa"%objpath%\devkits.cod" /Fo"%objpath%\devkits.obj" /Fd"%objpath%\vc140.pdb" /GS- /GF /Gy /Qspectre /TC /c /errorReport:queue

cl ..\src\xpf_core\nvdbg.c /I"..\src\include" /I"%ddkpath%\include" /Zi /nologo /W3 /WX /Oi /O2 /D"_msvc" /D"_amd64" /D"_nvdbg" /Zc:wchar_t /std:c17 /FAcs /Fa"%objpath%\nvdbg.cod" /Fo"%objpath%\nvdbg.obj" /Fd"%objpath%\vc140.pdb" /GS- /GF /Gy /Qspectre /TC /c /errorReport:queue

cl ..\src\xpf_core\cvhax.c /I"..\src\include" /Zi /nologo /W3 /WX /Oi /O2 /D"_msvc" /D"_amd64" /D"_cvhax" /Zc:wchar_t /std:c17 /FAcs /Fa"%objpath%\cvhax.cod" /Fo"%objpath%\cvhax.obj" /Fd"%objpath%\vc140.pdb" /GS- /GF /Gy /Qspectre /TC /c /errorReport:queue

ml64 /W3 /WX /D"_amd64" /Zf /Zd /Fo"%objpath%\msrhook.obj" /c /nologo ..\src\xpf_core\windows\msrhook.asm

for %%1 in (..\src\xpf_core\msvc\*.asm) do (ml64 /W3 /WX /D"_amd64" /Zf /Zd /Fo"%objpath%\%%~n1.obj" /c /nologo %%1)

for %%1 in (..\src\drv_core\acpi\*.c) do (cl %%1 /I"..\src\include" /Zi /nologo /W3 /WX /Oi /O2 /D"_msvc" /D"_amd64" /D"_%%~n1" /Zc:wchar_t /std:c17 /FAcs /Fa"%objpath%\%%~n1.cod" /Fo"%objpath%\%%~n1.obj" /Fd"%objpath%\vc140.pdb" /GS- /GF /Gy /Qspectre /TC /c /errorReport:queue)

echo Compiling Core of Drivers...
cl ..\src\drv_core\serial\serial.c /I"..\src\include" /Zi /nologo /W3 /WX /Oi /O2 /D"_msvc" /D"_amd64" /D"_drv_serial" /Zc:wchar_t /std:c17 /FAcs /Fa"%objpath%\serial.cod" /Fo"%objpath%\serial.obj" /Fd"%objpath%\vc140.pdb" /GS- /GF /Gy /Qspectre /TC /c /errorReport:queue

echo ============Start Linking============
link "%objpath%\*.obj" "%objpath%\version.res" /LIBPATH:"%libpath%\10.0.22621.0\km\x64" /NODEFAULTLIB "ntoskrnl.lib" "hal.lib" "..\src\disasm\bin\compfre_win11x64\zydis.lib" /NOLOGO /DEBUG /PDB:"%binpath%\NoirVisor.pdb" /OUT:"%binpath%\NoirVisor.sys" /OPT:REF /OPT:ICF /SUBSYSTEM:NATIVE /Driver /ENTRY:"NoirDriverEntry" /Machine:X64 /ERRORREPORT:QUEUE

echo This is Release/Free Build. Compiling Script will not perform signing!
echo You are supposed to sign this binary file with a formal signature!

if "%~1"=="/s" (echo Completed!) else (pause)