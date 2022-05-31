@echo off
set ddkpath=V:\Program Files\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.31.31103
set path=%ddkpath%\bin\Hostx64\x64;%path%
set edkpath=C:\UefiDKII
set mdepath=C:\UefiDKII\MdePkg
set libpath=C:\UefiDKII\Bin\MdePkg
set binpath=..\bin\compchk_uefix64
set objpath=..\bin\compchk_uefix64\Intermediate

title Compiling NoirVisor, Checked Build, UEFI (AMD64 Architecture)
echo Project: NoirVisor
echo Platform: Unified Extensible Firmware Interface
echo Preset: Debug/Checked Build
echo Powered by zero.tangptr@gmail.com
echo Copyright (c) 2018-2022, zero.tangptr@gmail.com. All Rights Reserved.
if "%~1"=="/s" (echo DO-NOT-PAUSE is activated!) else (pause)

echo ============Start Compiling============
echo Compiling UEFI Booting Facility...
cl ..\src\booting\efiapp\efimain.c /I"%mdepath%\Include" /I"%mdepath%\Include\X64" /nologo /Zi /W3 /WX /Od /Oi /D"_efi_boot" /FAcs /Fa"%objpath%\efiapp\efimain.cod" /Fo"%objpath%\efiapp\efimain.obj" /Fd"%objpath%\vc140.pdb" /GS- /Gr /TC /c

cl ..\src\booting\efiapp\driver.c /I"%mdepath%\Include" /I"%mdepath%\Include\X64" /nologo /Zi /W3 /WX /Od /Oi /D"_efi_boot" /FAcs /Fa"%objpath%\driver\driver.cod" /Fo"%objpath%\driver\driver.obj" /Fd"%objpath%\vc140.pdb" /GS- /Gr /TC /c

echo Compiling Core Engine of Intel VT-x...
for %%1 in (..\src\vt_core\*.c) do (cl %%1 /I"..\src\include" /nologo /Zi /W3 /WX /Od /Oi /D"_msvc" /D"_amd64" /D"_hv_type1" /D"_vt_core" /D"_%%~n1" /FAcs /Fa"%objpath%\driver\%%~n1.cod" /Fo"%objpath%\driver\%%~n1.obj" /Fd"%objpath%\vc140.pdb" /GS- /Gr /TC /c)

echo Compiling Core Engine of AMD-V...
for %%1 in (..\src\svm_core\*.c) do (cl %%1 /I"..\src\include" /nologo /Zi /W3 /WX /Od /Oi /D"_msvc" /D"_amd64" /D"_hv_type1" /D"_svm_core" /D"_%%~n1" /FAcs /Fa"%objpath%\driver\%%~n1.cod" /Fo"%objpath%\driver\%%~n1.obj" /Fd"%objpath%\vc140.pdb" /GS- /Gr /TC /c)

echo Compiling Core Engine of Microsoft Hypervisor (MSHV)...
for %%1 in (..\src\mshv_core\*.c) do (cl %%1 /I"..\src\include" /nologo /Zi /W3 /WX /Od /Oi /D"_msvc" /D"_amd64" /D"_hv_type1" /D"_mshv_core" /D"_%%~n1" /FAcs /Fa"%objpath%\driver\%%~n1.cod" /Fo"%objpath%\driver\%%~n1.obj" /Fd"%objpath%\vc140.pdb" /GS- /Gr /TC /c)

echo Compiling Core of Cross-Platform Framework (XPF)...
for %%1 in (..\src\xpf_core\uefi\*.c) do (cl %%1 /I"%mdepath%\Include" /I"%mdepath%\Include\X64" /I"%ddkpath%\include" /I"..\src\disasm\zydis\include" /I"..\src\disasm\zydis\dependencies\zycore\include" /nologo /Zi /W3 /WX /Od /Oi /D"_efi_boot" /FAcs /Fa"%objpath%\driver\%%~n1.cod" /Fo"%objpath%\driver\%%~n1.obj" /Fd"%objpath%\vc140.pdb" /GS- /Gr /TC /c)

ml64 /X /D"_amd64" /D"_msvc" /nologo /I"..\src\xpf_core\msvc" /Fo"%objpath%\driver\exception.obj" /c ..\src\xpf_core\uefi\exception.asm

for %%1 in (..\src\xpf_core\msvc\*.asm) do (ml64 /X /D"_amd64" /D"_msvc" /nologo /I"..\src\xpf_core\msvc" /Fo"%objpath%\driver\%%~n1.obj" /c %%1)

cl ..\src\xpf_core\noirhvm.c /I"..\src\include" /nologo /Zi /W3 /WX /Od /Oi /D"_msvc" /D"_amd64" /D"_hv_type1" /D"_central_hvm" /FAcs /Fa"%objpath%\driver\noirhvm.cod" /Fo"%objpath%\driver\noirhvm.obj" /Fd"%objpath%\vc140.pdb" /GS- /Gr /TC /c

cl ..\src\xpf_core\ci.c /I"..\src\include" /nologo /Zi /W3 /WX /Od /Oi /D"_msvc" /D"_amd64" /D"_hv_type1" /D"_code_integrity" /FAcs /Fa"%objpath%\driver\ci.cod" /Fo"%objpath%\driver\ci.obj" /Fd"%objpath%\vc140.pdb" /GS- /Gr /TC /c

cl ..\src\xpf_core\devkits.c /I"..\src\include" /nologo /Zi /W3 /WX /Od /Oi /D"_msvc" /D"_amd64" /D"_hv_type1" /D"_devkits" /FAcs /Fa"%objpath%\driver\devkits.cod" /Fo"%objpath%\driver\devkits.obj" /Fd"%objpath%\vc140.pdb" /GS- /Gr /TC /c

echo ============Start Linking============
echo Linking NoirVisor EFI Loader Application...
link "%objpath%\efiapp\*.obj" /NODEFAULTLIB /LIBPATH:"%libpath%\compchk_uefix64" "BaseLib.lib" "BaseDebugPrintErrorLevelLib.lib" "BaseMemoryLib.lib" "BasePrintLib.lib" "UefiLib.lib" "UefiDebugLibConOut.lib" "UefiMemoryAllocationLib.lib" "UefiDevicePathLib.Lib" "UefiBootServicesTableLib.Lib" "UefiRuntimeServicesTableLib.Lib" /NOLOGO /OUT:"%binpath%\bootx64.efi" /SUBSYSTEM:EFI_APPLICATION /ENTRY:"NoirEfiEntry" /DEBUG /PDB:"%binpath%\bootx64.pdb" /Machine:X64

echo Linking NoirVisor EFI Hypervisor Runtime Driver...
link "%objpath%\driver\*.obj"  /NODEFAULTLIB /LIBPATH:"%libpath%\compchk_uefix64" "BaseLib.lib" "BaseDebugPrintErrorLevelLib.lib" "BaseIoLibIntrinsic.Lib" "BaseMemoryLib.lib" "BasePrintLib.lib" "RegisterFilterLibNull.Lib" "UefiLib.lib" "UefiDebugLibConOut.lib" "UefiMemoryAllocationLib.lib" "UefiDevicePathLib.Lib" "UefiBootServicesTableLib.Lib" "UefiRuntimeServicesTableLib.Lib" "..\src\disasm\bin\compchk_win11x64\zydis.lib" /NOLOGO /OUT:"%binpath%\NoirVisor.efi" /SUBSYSTEM:EFI_RUNTIME_DRIVER /ENTRY:"NoirDriverEntry" /DEBUG /PDB:"%binpath%\NoirVisor.pdb" /Machine:X64

if "%~1"=="/s" (echo Completed!) else (pause)