@echo off
set path=T:\Program Files\Microsoft Visual Studio\2019\BuildTools\VC\Tools\MSVC\14.23.28105\bin\Hostx64\x64;T:\Program Files\Windows Kits\10\bin\10.0.19041.0\x64;%path%
set incpath=T:\Program Files\Windows Kits\10\Include\10.0.19041.0
set libpath=T:\Program Files\Windows Kits\10\Lib
set binpath=..\bin\compchk_win7x64
set objpath=..\bin\compchk_win7x64\Intermediate

title Compiling Zydis, Checked Build, 64-Bit Windows (AMD64 Architecture)
echo Project: Zydis Disassembler Library
echo Platform: 64-Bit Windows
echo Preset: Debug/Checked Build
echo Source Powered by zyantific. Script Powered by zero.tangptr@gmail.com.
echo Copyright (c) 2014-2020, zyantific. All Rights Reserved.
pause

echo ============Start Compiling============
for %%1 in (..\zydis\src\*.c) do (cl %%1 /I"..\zydis\include" /I"..\zycore-c\include" /I"..\zydis\msvc" /I"..\zydis\src" /nologo /Zi /W3 /WX /Od /Oi /D"ZYDIS_STATIC_DEFINE" /D"ZYAN_NO_LIBC" /FAcs /Fa"%objpath%\%%~n1.cod" /Fo"%objpath%\%%~n1.obj" /Fd"%objpath%\vc140.pdb" /GS- /Qspectre /Gr /TC /c /errorReport:queue)

cl ..\disasm.c /I"..\zydis\include" /I"..\zycore-c\include" /I"..\zydis\msvc" /nologo /Zi /W3 /WX /Od /Oi /D"ZYDIS_STATIC_DEFINE" /D"ZYAN_NO_LIBC" /FAcs /Fa"%objpath%\disasm.cod" /Fo"%objpath%\disasm.obj" /Fd"%objpath%\vc140.pdb" /GS- /Gr /Qspectre /TC /c /errorReport:queue

echo =============Start Linking=============
lib "%objpath%\*.obj" /NOLOGO /OUT:"%binpath%\zydis.lib" /MACHINE:X64 /ERRORREPORT:QUEUE

echo Completed!
pause.