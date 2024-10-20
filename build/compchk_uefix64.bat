@echo off
set ddkpath=V:\Program Files\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.38.33130
set path=%ddkpath%\bin\Hostx64\x64;%path%
set incpath=V:\Program Files\Windows Kits\10\Include\10.0.26100.0
set mdepath=%EDK2_PATH%\edk2\MdePkg
set libpath=%EDK2_PATH%\bin\MdePkg
set binpath=..\bin\compchk_uefix64
set objpath=..\bin\compchk_uefix64\Intermediate

title Compiling NoirVisor, Checked Build, UEFI (AMD64 Architecture)
echo Project: NoirVisor
echo Platform: Unified Extensible Firmware Interface
echo Preset: Debug/Checked Build
echo Powered by zero.tangptr@gmail.com
echo Copyright (c) 2018-2023, zero.tangptr@gmail.com. All Rights Reserved.
if "%~1"=="/s" (echo DO-NOT-PAUSE is activated!) else (pause)

echo ============Start Compiling============
echo Compiling UEFI Booting Facility...
cl ..\src\booting\efiapp\efimain.c /I"%mdepath%\Include" /I"%mdepath%\Include\X64" /nologo /Zi /W3 /WX /Od /Oi /D"_efi_boot" /FAcs /Fa"%objpath%\efiapp\efimain.cod" /Fo"%objpath%\efiapp\efimain.obj" /Fd"%objpath%\vc140.pdb" /GS- /Qspectre /Gr /TC /c

cl ..\src\booting\efiapp\driver.c /I"%mdepath%\Include" /I"%mdepath%\Include\X64" /nologo /Zi /W3 /WX /Od /Oi /D"_efi_boot" /FAcs /Fa"%objpath%\driver\driver.cod" /Fo"%objpath%\driver\driver.obj" /Fd"%objpath%\vc140.pdb" /GS- /Qspectre /Gr /TC /c

echo Compiling NoirVisor CVM Emulator...
cl ..\src\disasm\emulator.c /I"..\src\include" /I"..\src\disasm\zydis\include" /I"..\src\disasm\zydis\dependencies\zycore\include" /I"..\src\disasm\zydis\msvc" /nologo /Zi /W3 /WX /Od /Oi /D"ZYDIS_STATIC_BUILD" /D"ZYAN_NO_LIBC" /D"_msvc" /D"_amd64" /D"_emulator" /FAcs /Fa"%objpath%\driver\emulator.cod" /Fo"%objpath%\driver\emulator.obj" /Fd"%objpath%\vc140.pdb" /GS- /Gr /Qspectre /TC /c /errorReport:queue

echo Compiling Core of Cross-Platform Framework (XPF)...
for %%1 in (..\src\xpf_core\uefi\*.c) do (cl %%1 /I"%mdepath%\Include" /I"%mdepath%\Include\X64" /I"%ddkpath%\include" /I"..\src\disasm\zydis\include" /I"..\src\disasm\zydis\dependencies\zycore\include" /nologo /Zi /W3 /WX /Od /D"_efi_boot" /FAcs /Fa"%objpath%\driver\%%~n1.cod" /Fo"%objpath%\driver\%%~n1.obj" /Fd"%objpath%\vc140.pdb" /GS- /Qspectre /Gr /TC /utf-8 /c)

cl ..\src\xpf_core\devkits.c /I"..\src\include" /I"%ddkpath%\include" /nologo /Zi /W3 /WX /Od /Oi /D"_msvc" /D"_amd64" /D"_hv_type1" /D"_devkits" /FAcs /Fa"%objpath%\driver\devkits.cod" /Fo"%objpath%\driver\devkits.obj" /Fd"%objpath%\vc140.pdb" /GS- /Qspectre /Gr /TC /c

cl ..\src\xpf_core\c99-snprintf\snprintf.c /I"%incpath%\shared" /I"%incpath%\um" /I"%incpath%\ucrt" /I"%ddkpath%\include" /Zi /nologo /W3 /WX /wd4267 /wd4244 /Od /D"HAVE_STDARG_H" /D"HAVE_LOCALE_H" /D"HAVE_STDDEF_H" /D"HAVE_FLOAT_H" /D"HAVE_STDINT_H" /D"HAVE_INTTYPES_H" /D"HAVE_LONG_LONG_INT" /D"HAVE_UNSIGNED_LONG_LONG_INT" /D"HAVE_ASPRINTF" /D"HAVE_VASPRINTF" /D"HAVE_SNPRINTF" /Zc:wchar_t /std:c17 /FAcs /Fa"%objpath%\driver\snprintf.cod" /Fo"%objpath%\driver\snprintf.obj" /Fd"vc140.pdb" /GS- /Qspectre /TC /c /errorReport:queue

cl ..\src\xpf_core\portable-dlmalloc\malloc.c /I"%incpath%\ucrt" /I"%ddkpath%\include" /D"PORTABLE" /D"USE_DL_PREFIX" /D"NO_MALLOC_STATS=1" /D"USE_LOCKS=2" /D"DEFAULT_GRANULARITY=0x200000" /Zi /nologo /W3 /WX /O2 /Zc:wchar_t /FAcs /Fa"%objpath%\driver\malloc.cod" /Fo"%objpath%\driver\malloc.obj" /Fd"%objpath%\vc140.pdb" /GS- /std:c17 /Qspectre /TC /c /errorReport:queue

echo Compiling NoirVisor Core in Rust...
cargo build -Z build-std=core,panic_abort,alloc --target x86_64-pc-windows-msvc

echo ============Start Linking============
echo Linking NoirVisor EFI Loader Application...
link "%objpath%\efiapp\*.obj" /NODEFAULTLIB /LIBPATH:"%libpath%\compchk_uefix64" "MdePkgGuids.lib" "BaseLib.lib" "BaseDebugPrintErrorLevelLib.lib" "BaseMemoryLib.lib" "BasePrintLib.lib" "UefiLib.lib" "UefiDebugLibConOut.lib" "UefiMemoryAllocationLib.lib" "UefiDevicePathLibDevicePathProtocol.Lib" "UefiBootServicesTableLib.Lib" "UefiRuntimeServicesTableLib.Lib" /NOLOGO /OUT:"%binpath%\bootx64.efi" /SUBSYSTEM:EFI_APPLICATION /ENTRY:"NoirEfiEntry" /DEBUG /PDB:"%binpath%\bootx64.pdb" /Machine:X64

echo Linking NoirVisor EFI Hypervisor Runtime Driver...
link "%objpath%\driver\*.obj" "..\target\x86_64-pc-windows-msvc\debug\nvcore.lib" /NODEFAULTLIB /LIBPATH:"%libpath%\compchk_uefix64" "MdePkgGuids.lib" "BaseLib.lib" "BaseDebugPrintErrorLevelLib.lib" "BaseIoLibIntrinsic.Lib" "BaseMemoryLib.lib" "BasePrintLib.lib" "UefiLib.lib" "UefiDebugLibConOut.lib" "UefiMemoryAllocationLib.lib" "UefiDevicePathLibDevicePathProtocol.Lib" "UefiBootServicesTableLib.Lib" "UefiRuntimeServicesTableLib.Lib" "..\src\disasm\bin\compchk_win11x64\zydis.lib" /NOLOGO /OUT:"%binpath%\NoirVisor.efi" /SUBSYSTEM:EFI_RUNTIME_DRIVER /ENTRY:"NoirDriverEntry" /OPT:REF /DEBUG /PDB:"%binpath%\NoirVisor.pdb" /Machine:X64

echo ============Start Imaging============
echo Creating Disk Image...
set /A imagesize_kb=1440
set /A imagesize_b=%imagesize_kb*1024
if exist %binpath%\NoirVisor-Uefi.img (fsutil file setzerodata offset=0 length=%imagesize_b% %binpath%\NoirVisor-Uefi.img) else (fsutil file createnew %binpath%\NoirVisor-Uefi.img %imagesize_b%)
echo Formatting Disk Image...
mformat -i %binpath%\NoirVisor-Uefi.img -f %imagesize_kb% ::
mmd -i %binpath%\NoirVisor-Uefi.img ::/EFI
mmd -i %binpath%\NoirVisor-Uefi.img ::/EFI/BOOT
echo Making Config...
python makeueficonfig.py DefaultUefiConfig.json %binpath%\NoirVisorConfig.bin
echo Copying into Disk Image...
mcopy -i %binpath%\NoirVisor-Uefi.img %binpath%\NoirVisor.efi ::/
mcopy -i %binpath%\NoirVisor-Uefi.img %binpath%\NoirVisorConfig.bin ::/
mcopy -i %binpath%\NoirVisor-Uefi.img %binpath%\bootx64.efi ::/EFI/BOOT

if "%~1"=="/s" (echo Completed!) else (pause)