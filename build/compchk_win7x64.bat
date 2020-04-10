@echo off
set ddkpath=C:\WinDDK\7600.16385.1\bin\x86
set incpath=C:\WinDDK\7600.16385.1\inc
set libpath=C:\WinDDK\7600.16385.1\lib
set binpath=..\bin\compchk_win7x64
set objpath=..\bin\compchk_win7x64\Intermediate

title Compiling NoirVisor, Checked Build, 64-Bit Windows (AMD64 Architecture)
echo Project: NoirVisor
echo Platform: 64-Bit Windows
echo Preset: Debug/Checked Build
echo Powered by zero.tangptr@gmail.com
echo Copyright (c) 2018-2020, zero.tangptr@gmail.com. All Rights Reserved.
pause

echo ============Start Compiling============
echo Compiling Windows Driver Framework...
%ddkpath%\amd64\cl.exe ..\src\booting\windrv\driver.c /I"%incpath%\crt" /I"%incpath%\api" /I"%incpath%\ddk" /Zi /nologo /W3 /WX /Od /Oy- /D"_AMD64_" /D"_M_AMD64" /D"_WIN64" /D "_NDEBUG" /D"_UNICODE" /D "UNICODE" /Zc:wchar_t /Zc:forScope /FAcs /Fa"%objpath%\driver.cod" /Fo"%objpath%\driver.obj" /Fd"%objpath%\vc90.pdb" /GS- /Gr /TC /c /errorReport:queue

echo Compiling Core Engine of Intel VT-x...
%ddkpath%\amd64\cl.exe ..\src\vt_core\vt_main.c /I"..\src\include" /Zi /nologo /W3 /WX /Oi /Od /D"_msvc" /D"_amd64" /D"_vt_core" /D"_vt_drv" /Zc:wchar_t /Zc:forScope /FAcs /Fa"%objpath%\vt_main.cod" /Fo"%objpath%\vt_main.obj" /Fd"%objpath%\vc90.pdb" /GS- /Gr /TC /c /errorReport:queue

%ddkpath%\amd64\cl.exe ..\src\vt_core\vt_exit.c /I"..\src\include" /Zi /nologo /W3 /WX /Oi /Od /D"_msvc" /D"_amd64" /D"_vt_core" /D"_vt_exit" /Zc:wchar_t /Zc:forScope /FAcs /Fa"%objpath%\vt_exit.cod" /Fo"%objpath%\vt_exit.obj" /Fd"%objpath%\vc90.pdb" /GS- /Gr /TC /c /errorReport:queue

%ddkpath%\amd64\cl.exe ..\src\vt_core\vt_ept.c /I"..\src\include" /Zi /nologo /W3 /WX /Oi /Od /D"_msvc" /D"_amd64" /D"_vt_core" /D"_vt_ept" /Zc:wchar_t /Zc:forScope /FAcs /Fa"%objpath%\vt_ept.cod" /Fo"%objpath%\vt_ept.obj" /Fd"%objpath%\vc90.pdb" /GS- /Gr /TC /c /errorReport:queue

%ddkpath%\amd64\cl.exe ..\src\vt_core\vt_cpuid.c /I"..\src\include" /Zi /nologo /W3 /WX /Oi /Od /D"_msvc" /D"_amd64" /D"_vt_core" /D"_vt_cpuid" /Zc:wchar_t /Zc:forScope /FAcs /Fa"%objpath%\vt_cpuid.cod" /Fo"%objpath%\vt_cpuid.obj" /Fd"%objpath%\vc90.pdb" /GS- /Gr /TC /c /errorReport:queue

%ddkpath%\amd64\cl.exe ..\src\vt_core\vt_nvcpu.c /I"..\src\include" /Zi /nologo /W3 /WX /Oi /Od /D"_msvc" /D"_amd64" /D"_vt_core" /D"_vt_nvcpu" /Zc:wchar_t /Zc:forScope /FAcs /Fa"%objpath%\vt_nvcpu.cod" /Fo"%objpath%\vt_nvcpu.obj" /Fd"%objpath%\vc90.pdb" /GS- /Gr /TC /c /errorReport:queue

echo Compiling Core Engine of AMD-V...
%ddkpath%\amd64\cl.exe ..\src\svm_core\svm_main.c /I"..\src\include" /Zi /nologo /W3 /WX /Oi /Od /D"_msvc" /D"_amd64" /D"_svm_core" /D"_svm_drv" /Zc:wchar_t /Zc:forScope /FAcs /Fa"%objpath%\svm_main.cod" /Fo"%objpath%\svm_main.obj" /Fd"%objpath%\vc90.pdb" /GS- /Gr /TC /c /errorReport:queue

%ddkpath%\amd64\cl.exe ..\src\svm_core\svm_exit.c /I"..\src\include" /Zi /nologo /W3 /WX /Oi /Od /D"_msvc" /D"_amd64" /D"_svm_core" /D"_svm_exit" /Zc:wchar_t /Zc:forScope /FAcs /Fa"%objpath%\svm_exit.cod" /Fo"%objpath%\svm_exit.obj" /Fd"%objpath%\vc90.pdb" /GS- /Gr /TC /c /errorReport:queue

%ddkpath%\amd64\cl.exe ..\src\svm_core\svm_cpuid.c /I"..\src\include" /Zi /nologo /W3 /WX /Oi /Od /D"_msvc" /D"_amd64" /D"_svm_core" /D"_svm_cpuid" /Zc:wchar_t /Zc:forScope /FAcs /Fa"%objpath%\svm_cpuid.cod" /Fo"%objpath%\svm_cpuid.obj" /Fd"%objpath%\vc90.pdb" /GS- /Gr /TC /c /errorReport:queue

%ddkpath%\amd64\cl.exe ..\src\svm_core\svm_npt.c /I"..\src\include" /Zi /nologo /W3 /WX /Oi /Od /D"_msvc" /D"_amd64" /D"_svm_core" /D"_svm_npt" /Zc:wchar_t /Zc:forScope /FAcs /Fa"%objpath%\svm_npt.cod" /Fo"%objpath%\svm_npt.obj" /Fd"%objpath%\vc90.pdb" /GS- /Gr /TC /c /errorReport:queue

echo Compiling Core of Cross-Platform Framework (XPF)...
%ddkpath%\amd64\cl.exe ..\src\xpf_core\windows\nvsys.c /I"%incpath%\crt" /I"%incpath%\api" /I"%incpath%\ddk" /Zi /nologo /W3 /WX /Od /D"_AMD64_" /D"_M_AMD64" /D"_WIN64" /D "_NDEBUG" /D"_UNICODE" /D "UNICODE" /Zc:wchar_t /Zc:forScope /FAcs /Fa"%objpath%\nvsys.cod" /Fo"%objpath%\nvsys.obj" /Fd"%objpath%\vc90.pdb" /GS- /Gr /TC /c /errorReport:queue

%ddkpath%\amd64\cl.exe ..\src\xpf_core\windows\hooks.c /I"%incpath%\crt" /I"%incpath%\api" /I"%incpath%\ddk" /Zi /nologo /W3 /WX /Od /D"_AMD64_" /D"_M_AMD64" /D"_WIN64" /D "_NDEBUG" /D"_UNICODE" /D "UNICODE" /Zc:wchar_t /Zc:forScope /FAcs /Fa"%objpath%\hooks.cod" /Fo"%objpath%\hooks.obj" /Fd"%objpath%\vc90.pdb" /GS- /Gr /TC /c /errorReport:queue

%ddkpath%\amd64\cl.exe ..\src\xpf_core\windows\detour.c /I"%incpath%\crt" /I"%incpath%\api" /I"%incpath%\ddk" /Zi /nologo /W3 /WX /Od /D"_AMD64_" /D"_M_AMD64" /D"_WIN64" /D "_NDEBUG" /D"_UNICODE" /D "UNICODE" /Zc:wchar_t /Zc:forScope /FAcs /Fa"%objpath%\detour.cod" /Fo"%objpath%\detour.obj" /Fd"%objpath%\vc90.pdb" /GS- /Gr /TC /c /errorReport:queue

%ddkpath%\amd64\cl.exe ..\src\xpf_core\windows\winhvm.c /I"%incpath%\crt" /I"%incpath%\api" /I"%incpath%\ddk" /Zi /nologo /W3 /WX /Od /D"_AMD64_" /D"_M_AMD64" /D"_WIN64" /D "_NDEBUG" /D"_UNICODE" /D "UNICODE" /Zc:wchar_t /Zc:forScope /FAcs /Fa"%objpath%\winhvm.cod" /Fo"%objpath%\winhvm.obj" /Fd"%objpath%\vc90.pdb" /GS- /Gr /TC /c /errorReport:queue

%ddkpath%\amd64\cl.exe ..\src\xpf_core\noirhvm.c /I"..\src\include" /Zi /nologo /W3 /WX /Oi /Od /D"_msvc" /D"_amd64" /D"_central_hvm" /Zc:wchar_t /Zc:forScope /FAcs /Fa"%objpath%\noirhvm.cod" /Fo"%objpath%\noirhvm.obj" /Fd"%objpath%\vc90.pdb" /GS- /Gr /TC /c /errorReport:queue

%ddkpath%\amd64\cl.exe ..\src\xpf_core\ci.c /I"..\src\include" /Zi /nologo /W3 /WX /Oi /Od /D"_msvc" /D"_amd64" /D"_code_integrity" /Zc:wchar_t /Zc:forScope /FAcs /Fa"%objpath%\ci.cod" /Fo"%objpath%\ci.obj" /Fd"%objpath%\vc90.pdb" /GS- /Gr /TC /c /errorReport:queue

%ddkpath%\amd64\ml64.exe /W3 /WX /D"_amd64" /Zf /Zd /Fo"%objpath%\svm_hv.obj" /c /nologo ..\src\xpf_core\windows\svm_hv.asm

%ddkpath%\amd64\ml64.exe /W3 /WX /D"_amd64" /Zf /Zd /Fo"%objpath%\vt_hv.obj" /c /nologo ..\src\xpf_core\windows\vt_hv.asm

%ddkpath%\amd64\ml64.exe /W3 /WX /D"_amd64" /Zf /Zd /Fo"%objpath%\msrhook.obj" /c /nologo ..\src\xpf_core\windows\msrhook.asm

%ddkpath%\amd64\ml64.exe /W3 /WX /D"_amd64" /Zf /Zd /Fo"%objpath%\kpcr.obj" /c /nologo ..\src\xpf_core\windows\kpcr.asm

%ddkpath%\amd64\ml64.exe /W3 /WX /D"_amd64" /Zf /Zd /Fo"%objpath%\crc32.obj" /c /nologo ..\src\xpf_core\windows\crc32.asm

echo ============Start Linking============
%ddkpath%\amd64\link.exe "%objpath%\driver.obj" "%objpath%\vt_main.obj" "%objpath%\vt_exit.obj" "%objpath%\vt_ept.obj" "%objpath%\vt_cpuid.obj" "%objpath%\vt_nvcpu.obj" "%objpath%\svm_main.obj" "%objpath%\svm_exit.obj" "%objpath%\svm_cpuid.obj" "%objpath%\svm_npt.obj" "%objpath%\nvsys.obj" "%objpath%\hooks.obj" "%objpath%\detour.obj" "%objpath%\winhvm.obj" "%objpath%\noirhvm.obj" "%objpath%\ci.obj" "%objpath%\svm_hv.obj" "%objpath%\vt_hv.obj" "%objpath%\msrhook.obj" "%objpath%\kpcr.obj" "%objpath%\crc32.obj" /LIBPATH:"%libpath%\win7\amd64" /NODEFAULTLIB "ntoskrnl.lib" "..\src\disasm\LDE64.lib" /NOLOGO /DEBUG /PDB:"%objpath%\NoirVisor.pdb" /OUT:"%binpath%\NoirVisor.sys" /SUBSYSTEM:NATIVE /Driver /ENTRY:"NoirDriverEntry" /Machine:X64 /ERRORREPORT:QUEUE

echo ============Start Signing============
%ddkpath%\signtool.exe sign /v /f ..\ztnxtest.pfx /t http://timestamp.globalsign.com/scripts/timestamp.dll %binpath%\NoirVisor.sys

pause