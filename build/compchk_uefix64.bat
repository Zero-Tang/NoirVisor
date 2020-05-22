@echo off
set edkpath=C:\UefiDKII
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
clang-cl ..\src\booting\efiapp\efimain.c /I"%edkpath%\MdePkg\Include" /I"%edkpath%\MdePkg\Include\X64" /Zi /W3 /WX /Od /Oy- /D"_efi_boot" /Fa"%objpath%\efimain.cod" /Fo"%objpath%\efimain.obj" /GS- /Gr /TC /c -Wno-incompatible-pointer-types

clang-cl ..\src\booting\efiapp\driver.c /I"%edkpath%\MdePkg\Include" /I"%edkpath%\MdePkg\Include\X64" /Zi /W3 /WX /Od /Oy- /D"_efi_boot" /Fa"%objpath%\efimain.cod" /Fo"%objpath%\driver.obj" /GS- /Gr /TC /c -Wno-incompatible-pointer-types

clang-cl ..\src\booting\efiapp\init.c /I"%edkpath%\MdePkg\Include" /I"%edkpath%\MdePkg\Include\X64" /Zi /W3 /WX /Od /Oy- /D"_efi_init" /Fa"%objpath%\init.cod" /Fo"%objpath%\init.obj" /GS- /Gr /TC /c -Wno-incompatible-pointer-types

echo Compiling Core Engine of Intel VT-x...
clang-cl ..\src\vt_core\vt_main.c /I"..\src\include" /Zi /W3 /WX /Od /D"_llvm" /D"_amd64" /D"_vt_core" /D"_vt_drv" /Fa"%objpath%\vt_main.cod" /Fo"%objpath%\vt_main.obj" /GS- /Gr /TC /c -Wno-incompatible-pointer-types

clang-cl ..\src\vt_core\vt_exit.c /I"..\src\include" /Zi /W3 /WX /Od /D"_llvm" /D"_amd64" /D"_vt_core" /D"_vt_exit" /Fa"%objpath%\vt_exit.cod" /Fo"%objpath%\vt_exit.obj" /GS- /Gr /TC /c -Wno-incompatible-pointer-types

clang-cl ..\src\vt_core\vt_ept.c /I"..\src\include" /Zi  /W3 /WX /Od /D"_llvm" /D"_amd64" /D"_vt_core" /D"_vt_ept" /Fa"%objpath%\vt_ept.cod" /Fo"%objpath%\vt_ept.obj" /GS- /Gr /TC /c -Wno-incompatible-pointer-types

clang-cl ..\src\vt_core\vt_nvcpu.c /I"..\src\include" /Zi  /W3 /WX /Od /D"_llvm" /D"_amd64" /D"_vt_core" /D"_vt_nvcpu" /Fa"%objpath%\vt_nvcpu.cod" /Fo"%objpath%\vt_nvcpu.obj" /GS- /Gr /TC /c -Wno-incompatible-pointer-types

echo Compiling Core Engine of AMD-V...
clang-cl ..\src\svm_core\svm_main.c /I"..\src\include" /Zi  /W3 /WX /Od /D"_llvm" /D"_amd64" /D"_svm_core" /D"_svm_drv" /Fa"%objpath%\svm_main.cod" /Fo"%objpath%\svm_main.obj" /GS- /Gr /TC /c -Wno-incompatible-pointer-types

clang-cl ..\src\svm_core\svm_exit.c /I"..\src\include" /Zi  /W3 /WX /Od /D"_llvm" /D"_amd64" /D"_svm_core" /D"_svm_exit" /Fa"%objpath%\svm_exit.cod" /Fo"%objpath%\svm_exit.obj" /GS- /Gr /TC /c -Wno-incompatible-pointer-types

clang-cl ..\src\svm_core\svm_npt.c /I"..\src\include" /Zi  /W3 /WX /Od /D"_llvm" /D"_amd64" /D"_svm_core" /D"_svm_npt" /Fa"%objpath%\svm_npt.cod" /Fo"%objpath%\svm_npt.obj" /GS- /Gr /TC /c -Wno-incompatible-pointer-types

clang-cl ..\src\svm_core\svm_cpuid.c /I"..\src\include" /Zi  /W3 /WX /Od /D"_llvm" /D"_amd64" /D"_svm_core" /D"_svm_cpuid" /Fa"%objpath%\svm_cpuid.cod" /Fo"%objpath%\svm_cpuid.obj" /GS- /Gr /TC /c -Wno-incompatible-pointer-types

echo Compiling Core of Cross-Platform Framework (XPF)...
clang-cl ..\src\xpf_core\uefi\format.c /I"%edkpath%\MdePkg\Include" /I"%edkpath%\MdePkg\Include\X64" /Zi  /W3 /WX /Od /Oy- /D"_efi_boot" /Fa"%objpath%\format.cod" /Fo"%objpath%\format.obj" /GS- /Gr /TC /c -Wno-incompatible-pointer-types -Wno-varargs

clang-cl ..\src\xpf_core\uefi\string.c /I"%edkpath%\MdePkg\Include" /I"%edkpath%\MdePkg\Include\X64" /Zi  /W3 /WX /Od /Oy- /D"_efi_boot" /Fa"%objpath%\string.cod" /Fo"%objpath%\string.obj" /GS- /Gr /TC /c -Wno-incompatible-pointer-types

echo ============Start Linking============
echo Linking NoirVisor EFI Loader Application...
lld-link "%objpath%\efimain.obj" "%objpath%\init.obj" "%objpath%\format.obj" "%objpath%\string.obj" /NODEFAULTLIB  /OUT:"%binpath%\bootx64.efi" /SUBSYSTEM:EFI_APPLICATION /ENTRY:"NoirEfiEntry" /DEBUG /PDB:"%objpath%\bootx64.pdb" /Machine:X64

echo Linking NoirVisor EFI Hypervisor Runtime Driver...
lld-link "%objpath%\driver.obj" "%objpath%\init.obj" "%objpath%\format.obj" "%objpath%\string.obj" /NODEFAULTLIB  /OUT:"%binpath%\NoirVisor.efi" /SUBSYSTEM:EFI_RUNTIME_DRIVER /ENTRY:"NoirDriverEntry" /DEBUG /PDB:"%objpath%\NoirVisor.pdb" /Machine:X64

echo Completed!
pause.