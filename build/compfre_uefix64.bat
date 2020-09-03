@echo off
set edkpath=C:\UefiDKII
set mdepath=C:\UefiDKII\MdePkg
set libpath=C:\UefiDKII\Bin\MdePkg
set binpath=..\bin\compfre_uefix64
set objpath=..\bin\compfre_uefix64\Intermediate

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
clang-cl ..\src\booting\efiapp\efimain.c /I"%mdepath%\Include" /I"%mdepath%\Include\X64" /Zi /W3 /WX /O2 /Oy- /D"_efi_boot" /Fa"%objpath%\efimain.cod" /Fo"%objpath%\efimain.obj" /GS- /Gy /Gr /TC /c -Wno-incompatible-pointer-types

clang-cl ..\src\booting\efiapp\driver.c /I"%mdepath%\Include" /I"%mdepath%\Include\X64" /Zi /W3 /WX /O2 /Oy- /D"_efi_boot" /Fa"%objpath%\efimain.cod" /Fo"%objpath%\driver.obj" /GS- /Gy /Gr /TC /c -Wno-incompatible-pointer-types

echo Compiling Core Engine of Intel VT-x...
for %%1 in (..\src\vt_core\*.c) do (clang-cl %%1 /I"..\src\include" /Zi /W3 /WX /O2 /D"_llvm" /D"_amd64" /D"_vt_core" /D"_%%~n1" /Fa"%objpath%\%%~n1.cod" /Fo"%objpath%\%%~n1.obj" /GS- /Gy /Gr /TC /c -Wno-incompatible-pointer-types -Wno-pointer-sign)

echo Compiling Core Engine of AMD-V...
for %%1 in (..\src\svm_core\*.c) do (clang-cl %%1 /I"..\src\include" /Zi  /W3 /WX /O2 /D"_llvm" /D"_amd64" /D"_svm_core" /D"_%%~n1" /Fa"%objpath%\%%~n1.cod" /Fo"%objpath%\%%~n1.obj" /GS- /Gy /Gr /TC /c -Wno-incompatible-pointer-types -Wno-pointer-sign)

echo Compiling Core of Cross-Platform Framework (XPF)...

echo ============Start Linking============
echo Linking NoirVisor EFI Loader Application...
lld-link "%objpath%\efimain.obj" /NODEFAULTLIB /LIBPATH:"%libpath%\compfre_uefix64" "BaseLib.lib" "BaseDebugPrintErrorLevelLib.lib" "BaseMemoryLib.lib" "BasePrintLib.lib" "UefiLib.lib" "UefiDebugLibConOut.lib" "UefiMemoryAllocationLib.lib" "UefiDevicePathLib.Lib" "UefiBootServicesTableLib.Lib" "UefiRuntimeServicesTableLib.Lib" /OUT:"%binpath%\bootx64.efi" /OPT:REF /SUBSYSTEM:EFI_APPLICATION /ENTRY:"NoirEfiEntry" /DEBUG /PDB:"%objpath%\bootx64.pdb" /Machine:X64

echo Linking NoirVisor EFI Hypervisor Runtime Driver...
lld-link "%objpath%\driver.obj" /NODEFAULTLIB /LIBPATH:"%libpath%\compfre_uefix64" "BaseLib.lib" "BaseDebugPrintErrorLevelLib.lib" "BaseMemoryLib.lib" "BasePrintLib.lib" "UefiLib.lib" "UefiDebugLibConOut.lib" "UefiMemoryAllocationLib.lib" "UefiDevicePathLib.Lib" "UefiBootServicesTableLib.Lib" "UefiRuntimeServicesTableLib.Lib" /OUT:"%binpath%\NoirVisor.efi" /OPT:REF /SUBSYSTEM:EFI_RUNTIME_DRIVER /ENTRY:"NoirDriverEntry" /DEBUG /PDB:"%objpath%\NoirVisor.pdb" /Machine:X64

echo Completed!
pause.