@echo off
set ddkpath=C:\WinDDK\7600.16385.1\bin\x86
set incpath=C:\WinDDK\7600.16385.1\inc
set libpath=C:\WinDDK\7600.16385.1\lib
set binpath=.\bin\compchk_win7x64
set objpath=.\bin\compchk_win7x64\Intermediate

title Compiling NoirVisor
echo Project: NoirVisor
echo Platform: 64-Bit Windows
echo Powered by zero.tangptr@gmail.com
pause

echo ============Start Compiling============
echo Compiling Windows Driver Framework...
%ddkpath%\amd64\cl.exe .\src\booting\windrv\driver.c /I"%incpath%\crt" /I"%incpath%\api" /I"%incpath%\ddk" /Zi /nologo /W3 /WX /Od /D"_AMD64_" /D"_M_AMD64" /D"_WIN64" /D "_NDEBUG" /D"_UNICODE" /D "UNICODE" /Zc:wchar_t /Zc:forScope /FAcs /Fa"%objpath%\driver.cod" /Fo"%objpath%\driver.obj" /Fd"%objpath%\vc90.pdb" /GS- /Gr /TC /c /errorReport:queue

echo Compiling Core Engine of Intel VT-x...
%ddkpath%\amd64\cl.exe .\src\vt_core\vt_main.c /I".\src\include" /Zi /nologo /W3 /WX /Oi /Od /D"_msvc" /D"_amd64" /D"_vt_drv" /Zc:wchar_t /Zc:forScope /FAcs /Fa"%objpath%\vt_main.cod" /Fo"%objpath%\vt_main.obj" /Fd"%objpath%\vc90.pdb" /GS- /Gr /TC /c /errorReport:queue

%ddkpath%\amd64\cl.exe .\src\vt_core\vt_exit.c /I".\src\include" /Zi /nologo /W3 /WX /Oi /Od /D"_msvc" /D"_amd64" /D"_vt_exit" /Zc:wchar_t /Zc:forScope /FAcs /Fa"%objpath%\vt_exit.cod" /Fo"%objpath%\vt_exit.obj" /Fd"%objpath%\vc90.pdb" /GS- /Gr /TC /c /errorReport:queue

echo Compiling Core Engine of AMD-V...
%ddkpath%\amd64\cl.exe .\src\svm_core\svm_main.c /I".\src\include" /Zi /nologo /W3 /WX /Oi /Od /D"_msvc" /D"_amd64" /D"_svm_drv" /Zc:wchar_t /Zc:forScope /FAcs /Fa"%objpath%\svm_main.cod" /Fo"%objpath%\svm_main.obj" /Fd"%objpath%\vc90.pdb" /GS- /Gr /TC /c /errorReport:queue

%ddkpath%\amd64\cl.exe .\src\svm_core\svm_exit.c /I".\src\include" /Zi /nologo /W3 /WX /Oi /Od /D"_msvc" /D"_amd64" /D"_svm_drv" /D"_svm_exit" /Zc:wchar_t /Zc:forScope /FAcs /Fa"%objpath%\svm_exit.cod" /Fo"%objpath%\svm_exit.obj" /Fd"%objpath%\vc90.pdb" /GS- /Gr /TC /c /errorReport:queue

echo Compiling Core of Cross-Platform Framework (XPF)...
%ddkpath%\amd64\cl.exe .\src\xpf_core\windows\debug.c /I"%incpath%\crt" /I"%incpath%\api" /I"%incpath%\ddk" /Zi /nologo /W3 /WX /Od /D"_AMD64_" /D"_M_AMD64" /D"_WIN64" /D "_NDEBUG" /D"_UNICODE" /D "UNICODE" /Zc:wchar_t /Zc:forScope /FAcs /Fa"%objpath%\debug.cod" /Fo"%objpath%\debug.obj" /Fd"%objpath%\vc90.pdb" /GS- /Gr /TC /c /errorReport:queue

%ddkpath%\amd64\cl.exe .\src\xpf_core\windows\memory.c /I"%incpath%\crt" /I"%incpath%\api" /I"%incpath%\ddk" /Zi /nologo /W3 /WX /Od /D"_AMD64_" /D"_M_AMD64" /D"_WIN64" /D "_NDEBUG" /D"_UNICODE" /D "UNICODE" /Zc:wchar_t /Zc:forScope /FAcs /Fa"%objpath%\memory.cod" /Fo"%objpath%\memory.obj" /Fd"%objpath%\vc90.pdb" /GS- /Gr /TC /c /errorReport:queue

%ddkpath%\amd64\cl.exe .\src\xpf_core\windows\processor.c /I"%incpath%\crt" /I"%incpath%\api" /I"%incpath%\ddk" /Zi /nologo /W3 /WX /Od /D"_AMD64_" /D"_M_AMD64" /D"_WIN64" /D "_NDEBUG" /D"_UNICODE" /D "UNICODE" /Zc:wchar_t /Zc:forScope /FAcs /Fa"%objpath%\processor.cod" /Fo"%objpath%\processor.obj" /Fd"%objpath%\vc90.pdb" /GS- /Gr /TC /c /errorReport:queue

%ddkpath%\amd64\cl.exe .\src\xpf_core\windows\hookmsr.c /I"%incpath%\crt" /I"%incpath%\api" /I"%incpath%\ddk" /Zi /nologo /W3 /WX /Od /D"_AMD64_" /D"_M_AMD64" /D"_WIN64" /D "_NDEBUG" /D"_UNICODE" /D "UNICODE" /Zc:wchar_t /Zc:forScope /FAcs /Fa"%objpath%\hookmsr.cod" /Fo"%objpath%\hookmsr.obj" /Fd"%objpath%\vc90.pdb" /GS- /Gr /TC /c /errorReport:queue

%ddkpath%\amd64\cl.exe .\src\xpf_core\windows\winhvm.c /I"%incpath%\crt" /I"%incpath%\api" /I"%incpath%\ddk" /Zi /nologo /W3 /WX /Od /D"_AMD64_" /D"_M_AMD64" /D"_WIN64" /D "_NDEBUG" /D"_UNICODE" /D "UNICODE" /Zc:wchar_t /Zc:forScope /FAcs /Fa"%objpath%\winhvm.cod" /Fo"%objpath%\winhvm.obj" /Fd"%objpath%\vc90.pdb" /GS- /Gr /TC /c /errorReport:queue

%ddkpath%\amd64\cl.exe .\src\xpf_core\noirhvm.c /I".\src\include" /Zi /nologo /W3 /WX /Oi /Od /D"_msvc" /D"_amd64" /D"_central_hvm" /Zc:wchar_t /Zc:forScope /FAcs /Fa"%objpath%\noirhvm.cod" /Fo"%objpath%\noirhvm.obj" /Fd"%objpath%\vc90.pdb" /GS- /Gr /TC /c /errorReport:queue

%ddkpath%\amd64\ml64.exe /W3 /WX /Zf /Zd /Fo"%objpath%\svm_hv64.obj" /c /nologo .\src\xpf_core\windows\svm_hv64.asm

%ddkpath%\amd64\ml64.exe /W3 /WX /Zf /Zd /Fo"%objpath%\vt_hv64.obj" /c /nologo .\src\xpf_core\windows\vt_hv64.asm

%ddkpath%\amd64\ml64.exe /W3 /WX /Zf /Zd /Fo"%objpath%\msrhook64.obj" /c /nologo .\src\xpf_core\windows\msrhook64.asm

echo ============Start Linking============
%ddkpath%\amd64\link.exe "%objpath%\driver.obj" "%objpath%\vt_main.obj" "%objpath%\vt_exit.obj" "%objpath%\svm_main.obj" "%objpath%\svm_exit.obj" "%objpath%\memory.obj" "%objpath%\debug.obj" "%objpath%\processor.obj" "%objpath%\hookmsr.obj" "%objpath%\winhvm.obj" "%objpath%\noirhvm.obj" "%objpath%\svm_hv64.obj" "%objpath%\vt_hv64.obj" "%objpath%\msrhook64.obj" /LIBPATH:"%libpath%\win7\amd64" /NODEFAULTLIB "ntoskrnl.lib" /NOLOGO /DEBUG /PDB:"%objpath%\NoirVisor.pdb" /OUT:"%binpath%\NoirVisor.sys" /SUBSYSTEM:NATIVE /Driver /ENTRY:"NoirDriverEntry" /Machine:X64 /ERRORREPORT:QUEUE

echo ============Start Signing============
%ddkpath%\signtool.exe sign /v /f .\ztnxtest.pfx /t http://timestamp.globalsign.com/scripts/timestamp.dll %binpath%\NoirVisor.sys

pause