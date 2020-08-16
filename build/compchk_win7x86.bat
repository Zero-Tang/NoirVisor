@echo off
set ddkpath=C:\WinDDK\7600.16385.1\bin\x86
set incpath=C:\WinDDK\7600.16385.1\inc
set libpath=C:\WinDDK\7600.16385.1\lib
set binpath=..\bin\compchk_win7x86
set objpath=..\bin\compchk_win7x86\Intermediate

title Compiling NoirVisor, Checked Build, 32-Bit Windows (IA-32 Architecture)
echo Project: NoirVisor
echo Platform: 32-Bit Windows
echo Preset: Debug/Checked Build
echo Powered by zero.tangptr@gmail.com
echo Copyright (c) 2018-2020, zero.tangptr@gmail.com. All Rights Reserved.
pause

echo ============Start Compiling============
echo Compiling Windows Driver Framework...
%ddkpath%\x86\cl.exe ..\src\booting\windrv\driver.c /I"%incpath%\crt" /I"%incpath%\api" /I"%incpath%\ddk" /Zi /nologo /W3 /WX /Od /Oy- /D"_X86_" /D"_X86" /D"_WIN32" /D "_NDEBUG" /D"_UNICODE" /D "UNICODE" /Zc:wchar_t /FAcs /Fa"%objpath%\driver.cod" /Fo"%objpath%\driver.obj" /Fd"%objpath%\vc90.pdb" /GS- /Gz /TC /c /errorReport:queue

echo Compiling Core Engine of Intel VT-x...
for %%1 in (..\src\vt_core\*.c) do (%ddkpath%\x86\cl.exe %%1 /I"..\src\include" /Zi /nologo /W3 /WX /Oi /Od /D"_msvc" /D"_ia32" /D"_vt_core" /D"_%%~n1" /Zc:wchar_t /FAcs /Fa"%objpath%\%%~n1.cod" /Fo"%objpath%\%%~n1.obj" /Fd"%objpath%\vc90.pdb" /GS- /Gz /TC /c /errorReport:queue)

echo Compiling Core Engine of AMD-V...
for %%1 in (..\src\svm_core\*.c) do (%ddkpath%\x86\cl.exe %%1 /I"..\src\include" /Zi /nologo /W3 /WX /Oi /Od /D"_msvc" /D"_ia32" /D"_svm_core" /D"_%%~n1" /Zc:wchar_t /FAcs /Fa"%objpath%\%%~n1.cod" /Fo"%objpath%\%%~n1.obj" /Fd"%objpath%\vc90.pdb" /GS- /Gz /TC /c /errorReport:queue)

echo Compiling Core of Cross-Platform Framework (XPF)...
for %%1 in (..\src\xpf_core\windows\*.c) do (%ddkpath%\x86\cl.exe %%1 /I"%incpath%\crt" /I"%incpath%\api" /I"%incpath%\ddk" /Zi /nologo /W3 /WX /Od /D"_X86_" /D"_X86" /D"_WIN32" /D "_NDEBUG" /D"_UNICODE" /D "UNICODE" /Zc:wchar_t /FAcs /Fa"%objpath%\%%~n1.cod" /Fo"%objpath%\%%~n1.obj" /Fd"%objpath%\vc90.pdb" /GS- /Gz /TC /c /errorReport:queue)

%ddkpath%\x86\cl.exe ..\src\xpf_core\noirhvm.c /I"..\src\include" /Zi /nologo /W3 /WX /Oi /Od /D"_msvc" /D"_ia32" /D"_central_hvm" /Zc:wchar_t /FAcs /Fa"%objpath%\noirhvm.cod" /Fo"%objpath%\noirhvm.obj" /Fd"%objpath%\vc90.pdb" /GS- /Gz /TC /c /errorReport:queue

%ddkpath%\x86\cl.exe ..\src\xpf_core\ci.c /I"..\src\include" /Zi /nologo /W3 /WX /Oi /Od /D"_msvc" /D"_ia32" /D"_code_integrity" /Zc:wchar_t /FAcs /Fa"%objpath%\ci.cod" /Fo"%objpath%\ci.obj" /Fd"%objpath%\vc90.pdb" /GS- /Gz /TC /c /errorReport:queue

for %%1 in (..\src\xpf_core\windows\*.asm) do (%ddkpath%\ml.exe /W3 /WX /D"_ia32" /Zf /Zd /Fo"%objpath%\%%~n1.obj" /c /nologo %%1)

echo ============Start Linking============
%ddkpath%\x86\link.exe "%objpath%\driver.obj" "%objpath%\vt_main.obj" "%objpath%\vt_exit.obj" "%objpath%\vt_ept.obj" "%objpath%\vt_cpuid.obj" "%objpath%\vt_nvcpu.obj" "%objpath%\svm_main.obj" "%objpath%\svm_exit.obj" "%objpath%\svm_cpuid.obj" "%objpath%\svm_npt.obj" "%objpath%\nvsys.obj" "%objpath%\hooks.obj" "%objpath%\detour.obj" "%objpath%\winhvm.obj" "%objpath%\noirhvm.obj" "%objpath%\ci.obj" "%objpath%\svm_hv.obj" "%objpath%\vt_hv.obj" "%objpath%\msrhook.obj" "%objpath%\kpcr.obj" "%objpath%\crc32.obj" /LIBPATH:"%libpath%\win7\i386" /NODEFAULTLIB "ntoskrnl.lib" "..\src\disasm\LDE32.lib" /NOLOGO /DEBUG /PDB:"%objpath%\NoirVisor.pdb" /OUT:"%binpath%\NoirVisor.sys" /SUBSYSTEM:NATIVE /Driver /ENTRY:"NoirDriverEntry" /Machine:X86 /ERRORREPORT:QUEUE

echo ============Start Signing============
%ddkpath%\signtool.exe sign /v /f .\ztnxtest.pfx /t http://timestamp.globalsign.com/scripts/timestamp.dll %binpath%\NoirVisor.sys

pause