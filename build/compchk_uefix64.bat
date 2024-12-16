@echo off
set ddkpath=V:\Program Files\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.38.33130
set oldpath=%path%
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
echo Copyright (c) 2018-2024, zero.tangptr@gmail.com. All Rights Reserved.
if "%~1"=="/s" (echo DO-NOT-PAUSE is activated!) else (pause)

echo ============Start Compiling============
echo Compiling UEFI Booting Facility...
cl ..\src\booting\efiapp\efimain.c /I"%mdepath%\Include" /I"%mdepath%\Include\X64" /nologo /Zi /W3 /WX /Od /Oi /D"_efi_boot" /FAcs /Fa"%objpath%\efiapp\efimain.cod" /Fo"%objpath%\efiapp\efimain.obj" /Fd"%objpath%\vc140.pdb" /GS- /Qspectre /Gr /TC /c

cl ..\src\booting\efiapp\driver.c /I"%mdepath%\Include" /I"%mdepath%\Include\X64" /nologo /Zi /W3 /WX /Od /Oi /D"_efi_boot" /FAcs /Fa"%objpath%\driver\driver.cod" /Fo"%objpath%\driver\driver.obj" /Fd"%objpath%\vc140.pdb" /GS- /Qspectre /Gr /TC /c

echo Compiling Core of Cross-Platform Framework (XPF)...
for %%1 in (..\src\xpf_core\uefi\*.c) do (cl %%1 /I"%mdepath%\Include" /I"%mdepath%\Include\X64" /I"%ddkpath%\include" /I"..\src\disasm\zydis\include" /I"..\src\disasm\zydis\dependencies\zycore\include" /nologo /Zi /W3 /WX /Od /D"_efi_boot" /FAcs /Fa"%objpath%\driver\%%~n1.cod" /Fo"%objpath%\driver\%%~n1.obj" /Fd"%objpath%\vc140.pdb" /GS- /Qspectre /Gr /TC /utf-8 /c)

ml64 /X /D"_amd64" /D"_msvc" /D"_efi" /nologo /I"..\src\xpf_core\msvc" /Fo"%objpath%\driver\svm_hv.obj" /c ..\src\xpf_core\msvc\svm_hv.asm

ml64 /X /D"_amd64" /D"_msvc" /D"_efi" /nologo /I"..\src\xpf_core\msvc" /Fo"%objpath%\driver\interrupt.obj" /c ..\src\xpf_core\msvc\interrupt.asm

ml64 /X /D"_amd64" /D"_msvc" /D"_efi" /nologo /I"..\src\xpf_core\msvc" /Fo"%objpath%\driver\kpcr.obj" /c ..\src\xpf_core\msvc\kpcr.asm

echo Compiling NoirVisor Core in Rust...
rem Wrapping lib into ar is required since Cargo cc does not know UEFI uses MSVC!
set ar=python %cd%\ar-lib.py
set cc=cl
set cflags=/GS- /Zi
cargo build --target x86_64-unknown-uefi

echo ============Start Linking============
echo Linking NoirVisor EFI Loader Application...
link "%objpath%\efiapp\*.obj" /NODEFAULTLIB /LIBPATH:"%libpath%\compchk_uefix64" "MdePkgGuids.lib" "BaseLib.lib" "BaseDebugPrintErrorLevelLib.lib" "BaseMemoryLib.lib" "BasePrintLib.lib" "UefiLib.lib" "UefiDebugLibConOut.lib" "UefiMemoryAllocationLib.lib" "UefiDevicePathLibDevicePathProtocol.Lib" "UefiBootServicesTableLib.Lib" "UefiRuntimeServicesTableLib.Lib" /NOLOGO /OUT:"%binpath%\bootx64.efi" /SUBSYSTEM:EFI_APPLICATION /ENTRY:"NoirEfiEntry" /DEBUG /PDB:"%binpath%\bootx64.pdb" /Machine:X64

echo Linking NoirVisor EFI Hypervisor Runtime Driver...
link "%objpath%\driver\*.obj" "..\target\x86_64-unknown-uefi\debug\libnvcore.a" /NODEFAULTLIB /LIBPATH:"%libpath%\compchk_uefix64" "MdePkgGuids.lib" "BaseLib.lib" "BaseDebugPrintErrorLevelLib.lib" "BaseIoLibIntrinsic.Lib" "BaseMemoryLib.lib" "BasePrintLib.lib" "UefiLib.lib" "UefiDebugLibConOut.lib" "UefiMemoryAllocationLib.lib" "UefiDevicePathLibDevicePathProtocol.Lib" "UefiBootServicesTableLib.Lib" "UefiRuntimeServicesTableLib.Lib" /NOLOGO /OUT:"%binpath%\NoirVisor.efi" /SUBSYSTEM:EFI_RUNTIME_DRIVER /ENTRY:"NoirDriverEntry" /DEBUG /PDB:"%binpath%\NoirVisor.pdb" /Machine:X64

echo ============Start Imaging============
echo Creating Disk Image...
set /A imagesize_kb=2880
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

set path=%oldpath%
if "%~1"=="/s" (echo Completed!) else (pause)