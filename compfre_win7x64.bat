@echo off
set ddkpath=C:\WinDDK\7600.16385.1\bin\x86
set incpath=C:\WinDDK\7600.16385.1\inc
set libpath=C:\WinDDK\7600.16385.1\lib
set binpath=.\bin\compfre_win7x64
set objpath=.\bin\compfre_win7x64\Intermediate

title Compiling NoirVisor, Free Build, 64-Bit Windows (AMD64 Architecture)
echo Project: NoirVisor
echo Platform: 64-Bit Windows
echo Preset: Release/Free Build
echo Powered by zero.tangptr@gmail.com
echo Copyright 2018-2020, Zero Tang. All rights reserved.
pause

echo ============Start Compiling============
echo Compiling Windows Driver Framework...
%ddkpath%\amd64\cl.exe .\src\booting\windrv\driver.c /I"%incpath%\crt" /I"%incpath%\api" /I"%incpath%\ddk" /Zi /nologo /W3 /WX /O2 /Oy- /D"_AMD64_" /D"_M_AMD64" /D"_WIN64" /D "_NDEBUG" /D"_UNICODE" /D "UNICODE" /Zc:wchar_t /Zc:forScope /FAcs /Fa"%objpath%\driver.cod" /Fo"%objpath%\driver.obj" /Fd"%objpath%\vc90.pdb" /GS- /Gy /Gr /TC /c /errorReport:queue

echo Compiling Core Engine of Intel VT-x...
%ddkpath%\amd64\cl.exe .\src\vt_core\vt_main.c /I".\src\include" /Zi /nologo /W3 /WX /Oi /O2 /favor:INTEL64 /D"_msvc" /D"_amd64" /D"_vt_drv" /Zc:wchar_t /Zc:forScope /FAcs /Fa"%objpath%\vt_main.cod" /Fo"%objpath%\vt_main.obj" /Fd"%objpath%\vc90.pdb" /GS- /Gy /Gr /TC /c /errorReport:queue

%ddkpath%\amd64\cl.exe .\src\vt_core\vt_exit.c /I".\src\include" /Zi /nologo /W3 /WX /Oi /O2 /favor:INTEL64 /D"_msvc" /D"_amd64" /D"_vt_exit" /Zc:wchar_t /Zc:forScope /FAcs /Fa"%objpath%\vt_exit.cod" /Fo"%objpath%\vt_exit.obj" /Fd"%objpath%\vc90.pdb" /GS- /Gy /Gr /TC /c /errorReport:queue

%ddkpath%\amd64\cl.exe .\src\vt_core\vt_ept.c /I".\src\include" /Zi /nologo /W3 /WX /Oi /O2 /favor:INTEL64 /D"_msvc" /D"_amd64" /D"_vt_ept" /Zc:wchar_t /Zc:forScope /FAcs /Fa"%objpath%\vt_ept.cod" /Fo"%objpath%\vt_ept.obj" /Fd"%objpath%\vc90.pdb" /GS- /Gy /Gr /TC /c /errorReport:queue

%ddkpath%\amd64\cl.exe .\src\vt_core\vt_nvcpu.c /I".\src\include" /Zi /nologo /W3 /WX /Oi /O2 /favor:INTEL64 /D"_msvc" /D"_amd64" /D"_vt_nvcpu" /Zc:wchar_t /Zc:forScope /FAcs /Fa"%objpath%\vt_nvcpu.cod" /Fo"%objpath%\vt_nvcpu.obj" /Fd"%objpath%\vc90.pdb" /GS- /Gy /Gr /TC /c /errorReport:queue

echo Compiling Core Engine of AMD-V...
%ddkpath%\amd64\cl.exe .\src\svm_core\svm_main.c /I".\src\include" /Zi /nologo /W3 /WX /Oi /O2 /favor:AMD64 /D"_msvc" /D"_amd64" /D"_svm_drv" /Zc:wchar_t /Zc:forScope /FAcs /Fa"%objpath%\svm_main.cod" /Fo"%objpath%\svm_main.obj" /Fd"%objpath%\vc90.pdb" /GS- /Gy /Gr /TC /c /errorReport:queue

%ddkpath%\amd64\cl.exe .\src\svm_core\svm_exit.c /I".\src\include" /Zi /nologo /W3 /WX /Oi /O2 /favor:AMD64 /D"_msvc" /D"_amd64" /D"_svm_exit" /Zc:wchar_t /Zc:forScope /FAcs /Fa"%objpath%\svm_exit.cod" /Fo"%objpath%\svm_exit.obj" /Fd"%objpath%\vc90.pdb" /GS- /Gy /Gr /TC /c /errorReport:queue

%ddkpath%\amd64\cl.exe .\src\svm_core\svm_cpuid.c /I".\src\include" /Zi /nologo /W3 /WX /Oi /O2 /favor:AMD64 /D"_msvc" /D"_amd64" /D"_svm_exit" /Zc:wchar_t /Zc:forScope /FAcs /Fa"%objpath%\svm_cpuid.cod" /Fo"%objpath%\svm_cpuid.obj" /Fd"%objpath%\vc90.pdb" /GS- /Gy /Gr /TC /c /errorReport:queue

%ddkpath%\amd64\cl.exe .\src\svm_core\svm_npt.c /I".\src\include" /Zi /nologo /W3 /WX /Oi /O2 /favor:AMD64 /D"_msvc" /D"_amd64" /D"_svm_exit" /Zc:wchar_t /Zc:forScope /FAcs /Fa"%objpath%\svm_npt.cod" /Fo"%objpath%\svm_npt.obj" /Fd"%objpath%\vc90.pdb" /GS- /Gy /Gr /TC /c /errorReport:queue

echo Compiling Core of Cross-Platform Framework (XPF)...
%ddkpath%\amd64\cl.exe .\src\xpf_core\windows\nvsys.c /I"%incpath%\crt" /I"%incpath%\api" /I"%incpath%\ddk" /Zi /nologo /W3 /WX /O2 /D"_AMD64_" /D"_M_AMD64" /D"_WIN64" /D "_NDEBUG" /D"_UNICODE" /D "UNICODE" /Zc:wchar_t /Zc:forScope /FAcs /Fa"%objpath%\nvsys.cod" /Fo"%objpath%\nvsys.obj" /Fd"%objpath%\vc90.pdb" /GS- /Gy /Gr /TC /c /errorReport:queue

%ddkpath%\amd64\cl.exe .\src\xpf_core\windows\hooks.c /I"%incpath%\crt" /I"%incpath%\api" /I"%incpath%\ddk" /Zi /nologo /W3 /WX /O2 /D"_AMD64_" /D"_M_AMD64" /D"_WIN64" /D "_NDEBUG" /D"_UNICODE" /D "UNICODE" /Zc:wchar_t /Zc:forScope /FAcs /Fa"%objpath%\hooks.cod" /Fo"%objpath%\hooks.obj" /Fd"%objpath%\vc90.pdb" /GS- /Gy /Gr /TC /c /errorReport:queue

%ddkpath%\amd64\cl.exe .\src\xpf_core\windows\detour.c /I"%incpath%\crt" /I"%incpath%\api" /I"%incpath%\ddk" /Zi /nologo /W3 /WX /O2 /D"_AMD64_" /D"_M_AMD64" /D"_WIN64" /D "_NDEBUG" /D"_UNICODE" /D "UNICODE" /Zc:wchar_t /Zc:forScope /FAcs /Fa"%objpath%\detour.cod" /Fo"%objpath%\detour.obj" /Fd"%objpath%\vc90.pdb" /GS- /Gy /Gr /TC /c /errorReport:queue

%ddkpath%\amd64\cl.exe .\src\xpf_core\windows\winhvm.c /I"%incpath%\crt" /I"%incpath%\api" /I"%incpath%\ddk" /Zi /nologo /W3 /WX /O2 /D"_AMD64_" /D"_M_AMD64" /D"_WIN64" /D "_NDEBUG" /D"_UNICODE" /D "UNICODE" /Zc:wchar_t /Zc:forScope /FAcs /Fa"%objpath%\winhvm.cod" /Fo"%objpath%\winhvm.obj" /Fd"%objpath%\vc90.pdb" /GS- /Gy /Gr /TC /c /errorReport:queue

%ddkpath%\amd64\cl.exe .\src\xpf_core\noirhvm.c /I".\src\include" /Zi /nologo /W3 /WX /Oi /O2 /D"_msvc" /D"_amd64" /D"_central_hvm" /Zc:wchar_t /Zc:forScope /FAcs /Fa"%objpath%\noirhvm.cod" /Fo"%objpath%\noirhvm.obj" /Fd"%objpath%\vc90.pdb" /GS- /Gy /Gr /TC /c /errorReport:queue

%ddkpath%\amd64\cl.exe .\src\xpf_core\ci.c /I".\src\include" /Zi /nologo /W3 /WX /Oi /O2 /D"_msvc" /D"_amd64" /Zc:wchar_t /Zc:forScope /FAcs /Fa"%objpath%\ci.cod" /Fo"%objpath%\ci.obj" /Fd"%objpath%\vc90.pdb" /GS- /Gy /Gr /TC /c /errorReport:queue

%ddkpath%\amd64\ml64.exe /W3 /WX /Zf /Zd /Fo"%objpath%\svm_hv64.obj" /c /nologo .\src\xpf_core\windows\svm_hv64.asm

%ddkpath%\amd64\ml64.exe /W3 /WX /Zf /Zd /Fo"%objpath%\vt_hv64.obj" /c /nologo .\src\xpf_core\windows\vt_hv64.asm

%ddkpath%\amd64\ml64.exe /W3 /WX /Zf /Zd /Fo"%objpath%\msrhook64.obj" /c /nologo .\src\xpf_core\windows\msrhook64.asm

%ddkpath%\amd64\ml64.exe /W3 /WX /Zf /Zd /Fo"%objpath%\kpcr64.obj" /c /nologo .\src\xpf_core\windows\kpcr64.asm

%ddkpath%\amd64\ml64.exe /W3 /WX /Zf /Zd /Fo"%objpath%\crc32_x64.obj" /c /nologo .\src\xpf_core\windows\crc32_x64.asm

echo ============Start Linking============
%ddkpath%\amd64\link.exe "%objpath%\driver.obj" "%objpath%\vt_main.obj" "%objpath%\vt_exit.obj" "%objpath%\vt_ept.obj" "%objpath%\vt_nvcpu.obj" "%objpath%\svm_main.obj" "%objpath%\svm_exit.obj" "%objpath%\svm_cpuid.obj" "%objpath%\svm_npt.obj" "%objpath%\nvsys.obj" "%objpath%\hooks.obj" "%objpath%\detour.obj" "%objpath%\winhvm.obj" "%objpath%\noirhvm.obj" "%objpath%\ci.obj" "%objpath%\svm_hv64.obj" "%objpath%\vt_hv64.obj" "%objpath%\msrhook64.obj" "%objpath%\kpcr64.obj" "%objpath%\crc32_x64.obj" /LIBPATH:"%libpath%\win7\amd64" /NODEFAULTLIB "ntoskrnl.lib" ".\src\disasm\LDE64.lib" /NOLOGO /OPT:REF /OPT:ICF /DEBUG /PDB:"%objpath%\NoirVisor.pdb" /OUT:"%binpath%\NoirVisor.sys" /SUBSYSTEM:NATIVE /Driver /ENTRY:"NoirDriverEntry" /Machine:X64 /ERRORREPORT:QUEUE
echo Completed!
echo.

echo This is Release/Free Build. Compiling Script will not perform signing!
echo You should sign this binary file with formal signature!

pause