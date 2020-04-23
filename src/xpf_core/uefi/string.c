/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2020, Zero Tang. All rights reserved.

  This file is the string operation facility.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /xpf_core/uefi/string.c
*/

#include <Uefi.h>
#include <Protocol/UnicodeCollation.h>
#include "ncrt.h"

// Simple Console Output
void NoirConsoleOutput(IN CHAR16* String)
{
	StdOut->OutputString(StdOut,String);
}

// Memory Operation
void NoirCopyMemory(OUT VOID* Dest,IN VOID* Src,IN UINTN Length)
{
	EfiBoot->CopyMem(Dest,Src,Length);
}

void NoirFillMemory(OUT UINT8* Dest,IN UINT8 Src,IN UINTN Length)
{
	EfiBoot->SetMem(Dest,Length,Src);
}

// Encoding Conversion Functions
void NoirStringUnicodeToAnsi(CHAR8* as,CHAR16* ws,UINTN limit)
{
	UnicodeCollation->StrToFat(UnicodeCollation,ws,limit,as);
}

void NoirStringAnsiToUnicode(CHAR16* ws,CHAR8* as,UINTN limit)
{
	UnicodeCollation->FatToStr(UnicodeCollation,limit,as,ws);
}

// Ansi-String Essential Functions
UINTN NoirStringLengthA(CHAR8* string)
{
	UINTN i=0;
	while(string[i++]);
	return i-1;
}

// Unicode-String Essential Functions with Limiter
CHAR16* NoirStringCopyNW(CHAR16* dest,CHAR16* src,UINTN limit)
{
	for(UINTN i=0;i<limit;i++)
	{
		dest[i]=src[i];
		if(src[i]==0)break;
	}
	return dest;
}

// Unicode-String Essential Functions
UINTN NoirStringLengthW(CHAR16* string)
{
	UINTN i=0;
	while(string[i++]);
	return i-1;
}

void NoirStringReverseW(CHAR16* string)
{
	CHAR16* left=string;
	CHAR16 c;
	while(*string++);
	string-=2;
	while(left<string)
	{
		c=*left;
		*left++=*string;
		*string--=c;
	}
}

INT32 NoirStringCompareW(CHAR16* str1,CHAR16* str2)
{
	UINTN i=0;
	while(str1[i] || str2[i])
	{
		NoirConsolePrintfW(L"Comparing %d and %d\r\n",str1[i],str2[i]);
		if(str1[i]>str2[i])
			return 1;
		else if(str1[i]<str2[i])
			return -1;
		else
			i++;
	}
	return 0;
}

// Integer to String Functions
void NoirIntegerToDecimalW(INT32 val,CHAR16* string)
{
	int q=val;
	int i=0;
	if(q==0)string[i++]=L'0';
	for(;q;q/=10)
	{
		int r=q%10;
		string[i++]=r+L'0';
	}
	string[i]=0;
	NoirStringReverseW(string);
}

void NoirIntegerToDecimalW64(INT64 val,CHAR16* string)
{
	int q=val;
	int i=0;
	if(q==0)string[i++]=L'0';
	for(;q;q/=10)
	{
		int r=q%10;
		string[i++]=r+L'0';
	}
	string[i]=0;
	NoirStringReverseW(string);
}

void NoirIntegerToHexW(int val,CHAR16* string)
{
	CHAR16 hp[16]={L'0',L'1',L'2',L'3',L'4',L'5',L'6',L'7',L'8',L'9',L'a',L'b',L'c',L'd',L'e',L'f'};
	int q=val;
	int i=0;
	if(q==0)string[i++]=hp[0];
	for(;q;q>>=4)
	{
		int r=q&15;
		string[i++]=hp[r];
	}
	string[i]=0;
	NoirStringReverseW(string);
}

void NoirIntegerToHexW64(INT64 val,CHAR16* string)
{
	CHAR16 hp[16]={L'0',L'1',L'2',L'3',L'4',L'5',L'6',L'7',L'8',L'9',L'a',L'b',L'c',L'd',L'e',L'f'};
	int q=val;
	int i=0;
	if(q==0)string[i++]=hp[0];
	for(;q;q>>=4)
	{
		int r=q&15;
		string[i++]=hp[r];
	}
	string[i]=0;
	NoirStringReverseW(string);
}

void NoirIntegerToHEXW(int val,CHAR16* string)
{
	CHAR16 hp[16]={L'0',L'1',L'2',L'3',L'4',L'5',L'6',L'7',L'8',L'9',L'A',L'B',L'C',L'D',L'E',L'F'};
	int q=val;
	int i=0;
	if(q==0)string[i++]=hp[0];
	for(;q;q>>=4)
	{
		int r=q&15;
		string[i++]=hp[r];
	}
	string[i]=0;
	NoirStringReverseW(string);
}

void NoirIntegerToHEXW64(INT64 val,CHAR16* string)
{
	CHAR16 hp[16]={L'0',L'1',L'2',L'3',L'4',L'5',L'6',L'7',L'8',L'9',L'A',L'B',L'C',L'D',L'E',L'F'};
	int q=val;
	int i=0;
	if(q==0)string[i++]=hp[0];
	for(;q;q>>=4)
	{
		int r=q&15;
		string[i++]=hp[r];
	}
	string[i]=0;
	NoirStringReverseW(string);
}