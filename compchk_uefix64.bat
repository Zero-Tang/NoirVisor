@echo off
set edkpath=C:\UefiDKII
set binpath=.\bin\compchk_uefix64
set objpath=.\bin\compchk_uefix64\Intermediate

title Compiling NoirVisor, Checked Build, UEFI (AMD64 Architecture)
echo Project: NoirVisor
echo Platform: Unified Extensible Firmware Interface
echo Preset: Debug/Checked Build
echo Powered by zero.tangptr@gmail.com
echo Copyright (c) 2018-2020, zero.tangptr@gmail.com. All Rights Reserved.
pause

echo ============Start Compiling============
echo Compiling UEFI Application Framework
clang-cl .\src\booting\efiapp\efimain.c /I"%edkpath%\MdePkg\Include" /I"%edkpath%\MdePkg\Include\X64" /I"%edkpath%\MdePkg\Include\Protocol" /I".\src\include" /Zi /nologo /W3 /WX /Od /Oy- /D"_efi_boot" /Fa"%objpath%\efimain.cod" /Fo"%objpath%\efimain.obj" /Fd"%objpath%\vc140.pdb" /GS- /Gr /TC /c /errorReport:queue -Wno-incompatible-pointer-types

echo Compiling Core Engine of Intel VT-x...

echo Compiling Core Engine of AMD-V...

echo Compiling Core of Cross-Platform Framework (XPF)...
clang-cl .\src\xpf_core\uefi\format.c /I"%edkpath%\MdePkg\Include" /I"%edkpath%\MdePkg\Include\X64" /I"%edkpath%\MdePkg\Include\Protocol" /I".\src\include" /Zi /nologo /W3 /WX /Od /Oy- /D"_efi_boot" /Fa"%objpath%\format.cod" /Fo"%objpath%\format.obj" /Fd"%objpath%\vc140.pdb" /GS- /Gr /TC /c /errorReport:queue -Wno-incompatible-pointer-types -Wno-varargs

clang-cl .\src\xpf_core\uefi\string.c /I"%edkpath%\MdePkg\Include" /I"%edkpath%\MdePkg\Include\X64" /I"%edkpath%\MdePkg\Include\Protocol" /I".\src\include" /Zi /nologo /W3 /WX /Od /Oy- /D"_efi_boot" /Fa"%objpath%\string.cod" /Fo"%objpath%\string.obj" /Fd"%objpath%\vc140.pdb" /GS- /Gr /TC /c /errorReport:queue -Wno-incompatible-pointer-types

echo ============Start Linking============
lld-link "%objpath%\efimain.obj" "%objpath%\format.obj" "%objpath%\string.obj" /NODEFAULTLIB /NOLOGO /OUT:"%binpath%\bootx64.efi" /SUBSYSTEM:EFI_RUNTIME_DRIVER /ENTRY:"NoirEfiEntry" /Machine:X64 /ERRORREPORT:QUEUE

echo Completed!
pause.