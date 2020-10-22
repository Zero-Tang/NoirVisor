@echo off

title NoirVisor Compilation Preparation
echo Project: NoirVisor, Zydis
echo Platform: Universal (Non-Binary Build)
echo Preset: Directory Build
echo Powered by zero.tangptr@gmail.com
echo Warning: You are supposed to execute this batch file to build directories for first-time compilation.
echo Besides first-time compilation, you are not expected to execute this batch file at anytime.
pause.

echo Starting Compilation Preparations
mkdir ..\bin

echo Making Directories for NoirVisor Checked Build, 64-Bit Windows
mkdir ..\bin\compchk_win7x64
mkdir ..\bin\compchk_win7x64\Intermediate

echo Making Directories for NoirVisor Free Build, 64-Bit Windows
mkdir ..\bin\compfre_win7x64
mkdir ..\bin\compfre_win7x64\Intermediate

echo Making Directories for NoirVisor Checked Build, 32-Bit Windows
mkdir ..\bin\compchk_win7x86
mkdir ..\bin\compchk_win7x86\Intermediate

echo Making Directories for NoirVisor Free Build, 32-Bit Windows
mkdir ..\bin\compfre_win7x86
mkdir ..\bin\compfre_win7x86\Intermediate

echo Making Directories for NoirVisor Checked Build, 64-Bit UEFI
mkdir ..\bin\compchk_uefix64
mkdir ..\bin\compchk_uefix64\Intermediate

echo Making Directories for NoirVisor Free Build, 64-Bit UEFI
mkdir ..\bin\compfre_uefix64
mkdir ..\bin\compfre_uefix64\Intermediate

echo Preparation Completed!
pause.