@echo off
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
echo Copyright (c) 2018-2020, zero.tangptr@gmail.com. All Rights Reserved.
clang-cl --version
lld-link --version
pause

echo ============Start Compiling============
echo Compiling UEFI Booting Facility...
clang-cl ..\src\booting\efiapp\efimain.c /I"%mdepath%\Include" /I"%mdepath%\Include\X64" /Zi /W3 /WX /Od /Oi /D"_efi_boot" /Fa"%objpath%\efimain.cod" /Fo"%objpath%\efimain.obj" /GS- /Gr /TC /c -Wno-incompatible-pointer-types -Wno-int-to-pointer-cast

clang-cl ..\src\booting\efiapp\driver.c /I"%mdepath%\Include" /I"%mdepath%\Include\X64" /Zi /W3 /WX /Od /Oi /D"_efi_boot" /Fa"%objpath%\driver.cod" /Fo"%objpath%\driver.obj" /GS- /Gr /TC /c -Wno-incompatible-pointer-types

echo Compiling Core Engine of Intel VT-x...
for %%1 in (..\src\vt_core\*.c) do (clang-cl %%1 /I"..\src\include" /Zi /W3 /WX /Od /Oi /D"_llvm" /D"_amd64" /D"_hv_type1" /D"_vt_core" /D"_%%~n1" /Fa"%objpath%\%%~n1.cod" /Fo"%objpath%\%%~n1.obj" /GS- /Gr /TC /c -Wno-incompatible-pointer-types -Wno-pointer-sign)

echo Compiling Core Engine of AMD-V...
for %%1 in (..\src\svm_core\*.c) do (clang-cl %%1 /I"..\src\include" /Zi /W3 /WX /Od /Oi /D"_llvm" /D"_amd64" /D"_hv_type1" /D"_svm_core" /D"_%%~n1" /Fa"%objpath%\%%~n1.cod" /Fo"%objpath%\%%~n1.obj" /GS- /Gr /TC /c -Wno-incompatible-pointer-types -Wno-pointer-sign)

echo Compiling Core Engine of Microsoft Hypervisor (MSHV)...
for %%1 in (..\src\mshv_core\*.c) do (clang-cl %%1 /I"..\src\include" /Zi /W3 /WX /Od /Oi /D"_llvm" /D"_amd64" /D"_hv_type1" /D"_mshv_core" /D"_%%~n1" /Fa"%objpath%\%%~n1.cod" /Fo"%objpath%\%%~n1.obj" /GS- /Gr /TC /c -Wno-incompatible-pointer-types -Wno-pointer-sign)

echo Compiling Core of Cross-Platform Framework (XPF)...
for %%1 in (..\src\xpf_core\uefi\*.c) do (clang-cl %%1 /I"%mdepath%\Include" /I"%mdepath%\Include\X64" /Zi /W3 /WX /Od /Oi /D"_efi_boot" /Fa"%objpath%\%%~n1.cod" /Fo"%objpath%\%%~n1.obj" /GS- /Gr /TC /c)

for %%1 in (..\src\xpf_core\uefi\*.asm) do (nasm -o "%objpath%\%%~n1.obj" -i "..\src\xpf_core\uefi" -l "%objpath%\%%~n1.cod" -fwin64 -g -d"_amd64" %%1)

clang-cl ..\src\xpf_core\noirhvm.c /I"..\src\include" /Zi /W3 /WX /Od /Oi /D"_llvm" /D"_amd64" /D"_central_hvm" /Fa"%objpath%\noirhvm.cod" /Fo"%objpath%\noirhvm.obj" /GS- /Gr /TC /c -Wno-incompatible-pointer-types -Wno-pointer-sign

clang-cl ..\src\xpf_core\ci.c /I"..\src\include" /Zi /W3 /WX /Od /Oi /D"_llvm" /D"_amd64" /D"_code_integrity" /Fa"%objpath%\ci.cod" /Fo"%objpath%\ci.obj" /GS- /Gr /TC /c -Wno-incompatible-pointer-types -Wno-pointer-sign

echo ============Start Linking============
echo Linking NoirVisor EFI Loader Application...
lld-link "%objpath%\efimain.obj" /NODEFAULTLIB /LIBPATH:"%libpath%\compchk_uefix64" "BaseLib.lib" "BaseDebugPrintErrorLevelLib.lib" "BaseMemoryLib.lib" "BasePrintLib.lib" "UefiLib.lib" "UefiDebugLibConOut.lib" "UefiMemoryAllocationLib.lib" "UefiDevicePathLib.Lib" "UefiBootServicesTableLib.Lib" "UefiRuntimeServicesTableLib.Lib" /OUT:"%binpath%\bootx64.efi" /SUBSYSTEM:EFI_APPLICATION /ENTRY:"NoirEfiEntry" /DEBUG /PDB:"%objpath%\bootx64.pdb" /Machine:X64

echo Linking NoirVisor EFI Hypervisor Runtime Driver...
lld-link "%objpath%\driver.obj" "%objpath%\host.obj" "%objpath%\kpcr.obj" /NODEFAULTLIB /LIBPATH:"%libpath%\compchk_uefix64" "BaseLib.lib" "BaseDebugPrintErrorLevelLib.lib" "BaseMemoryLib.lib" "BasePrintLib.lib" "UefiLib.lib" "UefiDebugLibConOut.lib" "UefiMemoryAllocationLib.lib" "UefiDevicePathLib.Lib" "UefiBootServicesTableLib.Lib" "UefiRuntimeServicesTableLib.Lib" /OUT:"%binpath%\NoirVisor.efi" /SUBSYSTEM:EFI_RUNTIME_DRIVER /ENTRY:"NoirDriverEntry" /DEBUG /PDB:"%objpath%\NoirVisor.pdb" /Machine:X64

echo Completed!
pause.