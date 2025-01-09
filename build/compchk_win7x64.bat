@echo off
set ddkpath=T:\Program Files\Microsoft Visual Studio\2019\BuildTools\VC\Tools\MSVC\14.29.30133
set oldpath=%path%
set path=%ddkpath%\bin\Hostx64\x64;T:\Program Files\Windows Kits\10\bin\10.0.22000.0\x64;%path%
set crtpath=%ddkpath%\crt\src\x64
set incpath=T:\Program Files\Windows Kits\10\Include\10.0.22000.0
set libpath=T:\Program Files\Windows Kits\10\Lib
set binpath=..\bin\compchk_win7x64
set objpath=..\bin\compchk_win7x64\Intermediate

title Compiling NoirVisor, Checked Build, 64-Bit Windows (AMD64 Architecture)
echo Project: NoirVisor
echo Platform: 64-Bit Windows
echo Preset: Debug/Checked Build
echo Powered by zero.tangptr@gmail.com
echo Copyright (c) 2018-2024, zero.tangptr@gmail.com. All Rights Reserved.
if "%~1"=="/s" (echo DO-NOT-PAUSE is activated!) else (pause)

echo ============Start Compiling============
echo Compiling Windows Driver Framework...
cl ..\src\booting\windrv\driver.c /I"%incpath%\km\crt" /I"%incpath%\shared" /I"%incpath%\km" /Zi /nologo /W3 /WX /wd4311 /Od /D"_AMD64_" /D"_M_AMD64" /D"_WIN64" /D "_NDEBUG" /D"_UNICODE" /D "UNICODE" /Zc:wchar_t /std:c17 /FAcs /Fa"%objpath%\driver.cod" /Fo"%objpath%\driver.obj" /Fd"%objpath%\vc140.pdb" /GS- /Qspectre /TC /c /errorReport:queue

rc /nologo /i"%incpath%\shared" /i"%incpath%\um" /i"%incpath%\km\crt" /d"_AMD64_" /fo"%objpath%\version.res" /n ..\src\booting\windrv\version.rc

echo Compiling Core of Cross-Platform Framework (XPF)...
for %%1 in (..\src\xpf_core\windows\*.c) do (cl %%1 /I"%incpath%\km\crt" /I"%incpath%\shared" /I"%incpath%\km" /Zi /nologo /W3 /WX /wd4311 /Oi /Od /D"_KERNEL_MODE" /D"_AMD64_" /D"_M_AMD64" /D"_WIN64" /D "_NDEBUG" /D"_UNICODE" /D "UNICODE" /D"_%%~n1" /Zc:wchar_t /std:c17 /FAcs /Fa"%objpath%\%%~n1.cod" /Fo"%objpath%\%%~n1.obj" /Fd"%objpath%\vc140.pdb" /GS- /Qspectre /TC /c /errorReport:queue)

ml64 /W3 /WX /D"_amd64" /Zf /Zd /Fo"%objpath%\msrhook.obj" /c /nologo ..\src\xpf_core\windows\msrhook.asm

ml64 /X /D"_amd64" /D"_msvc" /nologo /I"..\src\xpf_core\msvc" /Fo"%objpath%\svm_hv.obj" /c ..\src\xpf_core\msvc\svm_hv.asm

ml64 /X /D"_amd64" /D"_msvc" /nologo /I"..\src\xpf_core\msvc" /Fo"%objpath%\interrupt.obj" /c ..\src\xpf_core\msvc\interrupt.asm

ml64 /X /D"_amd64" /D"_msvc" /nologo /I"..\src\xpf_core\msvc" /Fo"%objpath%\kpcr.obj" /c ..\src\xpf_core\msvc\kpcr.asm

echo Compiling and Extracting Microsoft-Optimized CRT...
ml64 /I"%incpath%\shared" /W3 /WX /Zf /Zd /Zi /Fo"%objpath%\memcpy.obj" /c /nologo "%crtpath%\memcpy.asm"
ml64 /I"%incpath%\shared" /W3 /WX /Zf /Zd /Zi /Fo"%objpath%\memcmp.obj" /c /nologo "%crtpath%\memcmp.asm"
ml64 /I"%incpath%\shared" /W3 /WX /Zf /Zd /Zi /Fo"%objpath%\memset.obj" /c /nologo "%crtpath%\memset.asm"

lib "%libpath%\10.0.22000.0\ucrt\x64\libucrt.lib" /EXTRACT:"d:\os\obj\amd64fre\minkernel\crts\ucrt\src\appcrt\dll\mt\..\..\string\mt\objfre\amd64\strlen.obj" /NOLOGO /OUT:"%objpath%\strlen.obj"
lib "%ddkpath%\lib\x64\libcmt.lib" /EXTRACT:"d:\a01\_work\12\s\Intermediate\vctools\libcmt.nativeproj__851063217\objr\amd64\cpu_disp.obj" /NOLOGO /OUT:"%objpath%\cpu_disp.obj"
copy /Y "%ddkpath%\lib\x64\libcmt.amd64.pdb" "%binpath%\libcmt.amd64.pdb"

echo Compiling NoirVisor in Rust...
set cflags=/GS-
cargo build --target x86_64-pc-windows-msvc

echo ============Start Linking============
link "%objpath%\*.obj" "%objpath%\version.res" "..\target\x86_64-pc-windows-msvc\debug\nvcore.lib" /LIBPATH:"%libpath%\win7\km\x64" /NODEFAULTLIB "ntoskrnl.lib" "hal.lib" /NOLOGO /DEBUG /PDB:"%binpath%\NoirVisor.pdb" /OUT:"%binpath%\NoirVisor.sys" /OPT:REF /SUBSYSTEM:NATIVE /Driver /ENTRY:"NoirDriverEntry" /Machine:X64 /ERRORREPORT:QUEUE

echo ============Start Signing============
signtool sign /v /fd SHA1 /f .\ztnxtest.pfx  %binpath%\NoirVisor.sys

set path=%oldpath%
if "%~1"=="/s" (echo Completed!) else (pause)