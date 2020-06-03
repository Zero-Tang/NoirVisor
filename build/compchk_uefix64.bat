@echo off
set edkpath=C:\UefiDKII
set mdepath=C:\UefiDKII\MdePkg
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
clang-cl ..\src\booting\efiapp\efimain.c /I"%mdepath%\Include" /I"%mdepath%\Include\X64" /Zi /W3 /WX /Od /Oy- /D"_efi_boot" /Fa"%objpath%\efimain.cod" /Fo"%objpath%\efimain.obj" /GS- /Gr /TC /c -Wno-incompatible-pointer-types

clang-cl ..\src\booting\efiapp\driver.c /I"%mdepath%\Include" /I"%mdepath%\Include\X64" /Zi /W3 /WX /Od /Oy- /D"_efi_boot" /Fa"%objpath%\efimain.cod" /Fo"%objpath%\driver.obj" /GS- /Gr /TC /c -Wno-incompatible-pointer-types

clang-cl ..\src\booting\efiapp\init.c /I"%mdepath%\Include" /I"%mdepath%\Include\X64" /Zi /W3 /WX /Od /Oy- /D"_efi_init" /Fa"%objpath%\init.cod" /Fo"%objpath%\init.obj" /GS- /Gr /TC /c -Wno-incompatible-pointer-types

echo Compiling Core Engine of Intel VT-x...
for %%1 in (..\src\vt_core\*.c) do (clang-cl %%1 /I"..\src\include" /Zi /W3 /WX /Od /D"_llvm" /D"_amd64" /D"_vt_core" /D"_%%~n1" /Fa"%objpath%\%%~n1.cod" /Fo"%objpath%\%%~n1.obj" /GS- /Gr /TC /c -Wno-incompatible-pointer-types)

echo Compiling Core Engine of AMD-V...
for %%1 in (..\src\svm_core\*.c) do (clang-cl %%1 /I"..\src\include" /Zi  /W3 /WX /Od /D"_llvm" /D"_amd64" /D"_svm_core" /D"_%%~n1" /Fa"%objpath%\%%~n1.cod" /Fo"%objpath%\%%~n1.obj" /GS- /Gr /TC /c -Wno-incompatible-pointer-types)

echo Compiling Core of Cross-Platform Framework (XPF)...
clang-cl ..\src\xpf_core\uefi\format.c /I"%mdepath%\Include" /I"%mdepath%\Include\X64" /Zi  /W3 /WX /Od /Oy- /D"_efi_boot" /Fa"%objpath%\format.cod" /Fo"%objpath%\format.obj" /GS- /Gr /TC /c -Wno-incompatible-pointer-types -Wno-varargs

clang-cl ..\src\xpf_core\uefi\string.c /I"%mdepath%\Include" /I"%mdepath%\Include\X64" /Zi  /W3 /WX /Od /Oy- /D"_efi_boot" /Fa"%objpath%\string.cod" /Fo"%objpath%\string.obj" /GS- /Gr /TC /c -Wno-incompatible-pointer-types

echo ============Start Linking============
echo Linking NoirVisor EFI Loader Application...
lld-link "%objpath%\efimain.obj" "%objpath%\init.obj" "%objpath%\format.obj" "%objpath%\string.obj" /NODEFAULTLIB  /OUT:"%binpath%\bootx64.efi" /SUBSYSTEM:EFI_APPLICATION /ENTRY:"NoirEfiEntry" /DEBUG /PDB:"%objpath%\bootx64.pdb" /Machine:X64

echo Linking NoirVisor EFI Hypervisor Runtime Driver...
lld-link "%objpath%\driver.obj" "%objpath%\init.obj" "%objpath%\format.obj" "%objpath%\string.obj" /NODEFAULTLIB  /OUT:"%binpath%\NoirVisor.efi" /SUBSYSTEM:EFI_RUNTIME_DRIVER /ENTRY:"NoirDriverEntry" /DEBUG /PDB:"%objpath%\NoirVisor.pdb" /Machine:X64

echo Completed!
pause.