/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2020, Zero Tang. All rights reserved.

  This file is for C Runtime Library essentials.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /xpf_core/uefi/ncrt.h
*/

#include <Uefi.h>
#include <UnicodeCollation.h>

void NoirConsoleOutput(IN CHAR16* String);

// Basic Memory Operation Services
void NoirCopyMemory(OUT VOID* Dest,IN VOID* Src,IN UINTN Length);
void NoirFillMemory(OUT UINT8* Dest,IN UINT8 Src,IN UINTN Length);

// String Encoding Conversion Services
void NoirStringUnicodeToAnsi(CHAR8* as,CHAR16* ws,UINTN limit);
void NoirStringAnsiToUnicode(CHAR16* ws,CHAR8* as,UINTN limit);

// Ansi-String Encoding Essential Functions
UINTN NoirStringLengthA(CHAR8* string);

// Unicode-String Essential Functions with Limiter
CHAR16* NoirStringCopyNW(CHAR16* dest,CHAR16* src,UINTN limit);

// Unicode-String Essential Functions
UINTN NoirStringLengthW(CHAR16* string);
void NoirStringReverseW(CHAR16* string);
INT32 NoirStringCompareW(CHAR16* str1,CHAR16* str2);
void NoirIntegerToDecimalW(INT32 val,CHAR16* string);
void NoirIntegerToDecimalW64(INT64 val,CHAR16* string);
void NoirIntegerToHexW(int val,CHAR16* string);
void NoirIntegerToHexW64(INT64 val,CHAR16* string);
void NoirIntegerToHEXW(int val,CHAR16* string);
void NoirIntegerToHEXW64(INT64 val,CHAR16* string);

// Formatted Print
void __cdecl NoirConsolePrintfW(CHAR16* format,...);

// EFI Defined Services and Protocols
extern EFI_BOOT_SERVICES* EfiBoot;
extern EFI_RUNTIME_SERVICES* EfiRT;
extern EFI_SIMPLE_TEXT_INPUT_PROTOCOL* StdIn;
extern EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL* StdOut;
extern EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL* StdErr;
extern EFI_UNICODE_COLLATION_PROTOCOL* UnicodeCollation;