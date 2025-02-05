@echo off
set ddkpath=V:\Program Files\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.38.33130
set oldpath=%path%
set path=%ddkpath%\bin\Hostx64\x64;V:\Program Files\Windows Kits\10\bin\10.0.26100.0\x64;%path%
set crtpath=%ddkpath%\crt\src\x64
set incpath=V:\Program Files\Windows Kits\10\Include\10.0.26100.0
set libpath=V:\Program Files\Windows Kits\10\Lib
set binpath=..\bin\compfre_win11x64
set objpath=..\bin\compfre_win11x64\Intermediate

title Compiling NoirVisor, Free Build, 64-Bit Windows (AMD64 Architecture)
echo Project: NoirVisor
echo Platform: 64-Bit Windows
echo Preset: Release/Free Build
echo Powered by zero.tangptr@gmail.com
echo Copyright (c) 2018-2025, zero.tangptr@gmail.com. All Rights Reserved.
if "%~1"=="/s" (echo DO-NOT-PAUSE is activated!) else (pause)

if not exist %objpath% mkdir %objpath%

echo ============Start Compiling============
echo Compiling Windows Driver Framework...
cl ..\src\booting\windrv\driver.c /I"%incpath%\km\crt" /I"%incpath%\shared" /I"%incpath%\km" /Zi /nologo /W3 /WX /wd4311 /O2 /D"_AMD64_" /D"_M_AMD64" /D"_WIN64" /D "_NDEBUG" /D"_UNICODE" /D "UNICODE" /Zc:wchar_t /std:c17 /FAcs /Fa"%objpath%\driver.cod" /Fo"%objpath%\driver.obj" /Fd"%objpath%\vc140.pdb" /GS- /GF /Gy /Qspectre /TC /c /errorReport:queue

rc /nologo /i"%incpath%\shared" /i"%incpath%\um" /i"%incpath%\km\crt" /d"_AMD64_" /fo"%objpath%\version.res" /n ..\src\booting\windrv\version.rc

echo Compiling Core of Cross-Platform Framework (XPF)...
for %%1 in (..\src\xpf_core\windows\*.c) do (cl %%1 /I"%incpath%\km\crt" /I"%incpath%\shared" /I"%incpath%\km" /Zi /nologo /W3 /WX /wd4311 /wd4996 /O2 /Oi /D"_KERNEL_MODE" /D"_AMD64_" /D"_M_AMD64" /D"_WIN64" /D "_NDEBUG" /D"_UNICODE" /D "UNICODE" /D"_%%~n1" /Zc:wchar_t /std:c17 /FAcs /Fa"%objpath%\%%~n1.cod" /Fo"%objpath%\%%~n1.obj" /Fd"%objpath%\vc140.pdb" /GS- /GF /Gy /Qspectre /TC /c /errorReport:queue)

ml64 /W3 /WX /D"_amd64" /Zf /Zd /Fo"%objpath%\msrhook.obj" /c /nologo ..\src\xpf_core\windows\msrhook.asm

ml64 /X /Zi /D"_amd64" /D"_msvc" /nologo /I"..\src\xpf_core\msvc" /Fo"%objpath%\svm_hv.obj" /c ..\src\xpf_core\msvc\svm_hv.asm

ml64 /X /Zi /D"_amd64" /D"_msvc" /nologo /I"..\src\xpf_core\msvc" /Fo"%objpath%\vt_hv.obj" /c ..\src\xpf_core\msvc\vt_hv.asm

ml64 /X /Zi /D"_amd64" /D"_msvc" /nologo /I"..\src\xpf_core\msvc" /Fo"%objpath%\interrupt.obj" /c ..\src\xpf_core\msvc\interrupt.asm

ml64 /X /Zi /D"_amd64" /D"_msvc" /nologo /I"..\src\xpf_core\msvc" /Fo"%objpath%\kpcr.obj" /c ..\src\xpf_core\msvc\kpcr.asm

echo Compiling and Extracting Microsoft-Optimized CRT...
rem The source code of Microsoft CRT is read-only, so build them once only.
if not exist "%objpath%\memcpy.obj" ml64 /I"%incpath%\shared" /W3 /WX /Zf /Zd /Zi /Fo"%objpath%\memcpy.obj" /c /nologo "%crtpath%\memcpy.asm"
if not exist "%objpath%\memcmp.obj" ml64 /I"%incpath%\shared" /W3 /WX /Zf /Zd /Zi /Fo"%objpath%\memcmp.obj" /c /nologo "%crtpath%\memcmp.asm"
if not exist "%objpath%\memset.obj" ml64 /I"%incpath%\shared" /W3 /WX /Zf /Zd /Zi /Fo"%objpath%\memset.obj" /c /nologo "%crtpath%\memset.asm"

if not exist "%objpath%\strlen.obj" lib "%libpath%\10.0.26100.0\ucrt\x64\libucrt.lib" /EXTRACT:"d:\os\obj\amd64fre\minkernel\crts\ucrt\src\appcrt\dll\mt\..\..\string\mt\objfre\amd64\strlen.obj" /NOLOGO /OUT:"%objpath%\strlen.obj"
if not exist "%objpath%\cpu_disp.obj" lib "%ddkpath%\lib\x64\libcmt.lib" /EXTRACT:"D:\a\_work\1\s\Intermediate\crt\vcstartup\build\mt\libcmt_kernel32\libcmt_kernel32.nativeproj\objr\amd64\cpu_disp.obj" /NOLOGO /OUT:"%objpath%\cpu_disp.obj"
if not exist "%binpath%\libcmt.amd64.pdb" copy /Y "%ddkpath%\lib\x64\libcmt.amd64.pdb" "%binpath%\libcmt.amd64.pdb"

echo Compiling NoirVisor in Rust...
set cflags=/GS-
cargo build --target x86_64-pc-windows-msvc --release

echo ============Start Linking============
link "%objpath%\*.obj" "%objpath%\version.res" "..\target\x86_64-pc-windows-msvc\release\nvcore.lib" /LIBPATH:"%libpath%\10.0.26100.0\km\x64" /NODEFAULTLIB "ntoskrnl.lib" "hal.lib" /NOLOGO /DEBUG /PDB:"%binpath%\NoirVisor.pdb" /OUT:"%binpath%\NoirVisor.sys" /OPT:REF /OPT:ICF /SUBSYSTEM:NATIVE /Driver /ENTRY:"NoirDriverEntry" /Machine:X64 /ERRORREPORT:QUEUE

echo This is Release/Free Build. Compiling Script will not perform signing!
echo You are supposed to sign this binary file with a formal signature!

set path=%oldpath%
if "%~1"=="/s" (echo Completed!) else (pause)