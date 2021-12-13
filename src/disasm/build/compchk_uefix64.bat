@echo off
set binpath=..\bin\compchk_uefix64
set objpath=..\bin\compchk_uefix64\Intermediate

title Compiling Zydis, Checked Build, 64-Bit UEFI (AMD64 Architecture)
echo Project: Zydis Disassembler Library
echo Platform: 64-Bit Unified Extensible Firmware Interface
echo Preset: Debug/Checked Build
echo Source Powered by zyantific. Script Powered by zero.tangptr@gmail.com.
echo Copyright (c) 2014-2021, zyantific. All Rights Reserved.
pause

echo ============Start Compiling============
for %%1 in (..\zydis\src\*.c) do (clang-cl %%1 /I"..\zydis\include" /I"..\zydis\dependencies\zycore\include" /I"..\zydis\msvc" /I"..\zydis\src" /Zi /W3 /WX /Od /Oi /D"ZYDIS_STATIC_DEFINE" /D"ZYAN_NO_LIBC" /Fa"%objpath%\%%~n1.cod" /Fo"%objpath%\%%~n1.obj" /GS- /Gr /TC /c)

clang-cl ..\disasm.c /I"..\zydis\include" /I"..\zydis\dependencies\zycore\include" /I"..\zydis\msvc" /Zi /W3 /WX /Od /Oi /D"ZYDIS_STATIC_DEFINE" /D"ZYAN_NO_LIBC" /Fa"%objpath%\disasm.cod" /Fo"%objpath%\disasm.obj" /GS- /Gr /TC /c

echo =============Start Linking=============
llvm-lib /NOLOGO /OUT:"%binpath%\zydis.lib" /MACHINE:X64 "%objpath%\*.obj"

if "%~1"=="/s" (echo Completed!) else (pause)