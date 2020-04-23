/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2020, Zero Tang. All rights reserved.

  This file is the Formatted Printing facility.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /xpf_core/uefi/format.c
*/

#include <Uefi.h>
#include <stdarg.h>
#include "ncrt.h"

int vswnprintf(CHAR16* output,UINTN limit,CHAR16* format,va_list args)
{
	CHAR16* buf=output;
	UINTN i=0,j=0;
	NoirFillMemory(buf,0,limit<<1);
	for(;format[i];i++)
	{
		if(format[i]==L'%')
		{
			UINTN inc=1;
			switch(format[i+1])
			{
				case L'c':
				{
					CHAR8 c=va_arg(args,CHAR8);
					buf[j]=(CHAR16)c;
					inc=1;
					break;
				}
				case L'd':
				{
					int n=va_arg(args,int);
					CHAR16 s[16];
					NoirIntegerToDecimalW(n,s);
					inc=NoirStringLengthW(s);
					NoirStringCopyNW(&buf[j],s,limit-j);
					i++;
					break;
				}
				case L'l':
				{
					break;
				}
				case L'p':
				{
					int n=va_arg(args,int);
					CHAR16 s[16];
					NoirIntegerToHEXW64(n,s);
					inc=NoirStringLengthW(s);
					NoirStringCopyNW(&buf[j],s,limit-j);
					i++;
					break;
				}
				case L's':
				{
					CHAR8* s=va_arg(args,CHAR8*);
					NoirStringAnsiToUnicode(&buf[j],s,limit-j);
					inc=NoirStringLengthW(&buf[j]);
					i++;
					break;
				}
				case L'w':
				{
					if(format[i+2]==L'c')
					{
						CHAR16 c=va_arg(args,CHAR16);
						inc=1;
						buf[j]=c;
						i+=2;
					}
					else if(format[i+2]==L's')
					{
						CHAR16* s=va_arg(args,CHAR16*);
						inc=NoirStringLengthW(s);
						NoirStringCopyNW(&buf[j],s,limit-j);
						i+=2;
					}
					else
					{
						inc=2;
						buf[j]=L'%';
						buf[j+1]=L'w';
						i+=2;
					}
					break;
				}
				case L'x':
				{
					int n=va_arg(args,int);
					CHAR16 s[16];
					NoirIntegerToHexW(n,s);
					inc=NoirStringLengthW(s);
					NoirStringCopyNW(&buf[j],s,limit-j);
					i++;
					break;
				}
				case L'X':
				{
					int n=va_arg(args,int);
					CHAR16 s[16];
					NoirIntegerToHEXW(n,s);
					inc=NoirStringLengthW(s);
					NoirStringCopyNW(&buf[j],s,limit-j);
					i++;
					break;
				}
				default:
				{
					buf[j]=L'%';
					break;
				}
			}
			j+=inc;
		}
		else
			buf[j++]=format[i];
		if(j>=limit)
		{
			j=limit;
			break;
		}
	}
	return (int)j;
}

void __cdecl NoirConsolePrintfW(CHAR16* format,...)
{
	CHAR16 buf[512];
	int len=0;
	va_list args;
	va_start(args,format);
	len=vswnprintf(buf,512,format,args);
	va_end(args);
	StdOut->OutputString(StdOut,buf);
}