/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2022, Zero Tang. All rights reserved.

  This file is the NoirVisor's Basic Development Kits
  function assets based on XPF-Core.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: ./xpf_core/devkits.c
*/

#include <nvdef.h>
#include <nvbdk.h>
#include <nvstatus.h>
#include <noirhvm.h>
#include <nv_intrin.h>
#include <stdarg.h>

void noir_initialize_list_entry(list_entry_p entry)
{
	entry->next=entry;
	entry->prev=entry;
}

void noir_insert_to_prev(list_entry_p inserter,list_entry_p insertee)
{
	// Initialize the insertee.
	insertee->prev=inserter->prev;
	insertee->next=inserter;
	// Set the insertee surroundings.
	inserter->prev->next=insertee;
	inserter->prev=insertee;
}

void noir_insert_to_next(list_entry_p inserter,list_entry_p insertee)
{
	// Initialize the insertee.
	insertee->next=inserter->next;
	insertee->prev=inserter;
	// Set the insertee surroundings.
	inserter->next->prev=insertee;
	inserter->next=insertee;
}

void noir_remove_list_entry(list_entry_p entry)
{
	// Remove the entry from sides.
	entry->prev->next=entry->next;
	entry->next->prev=entry->prev;
	// Reset this entry.
	entry->prev=entry->next=entry;
}

u32 noir_find_clear_bit(void* bitmap,u32 limit)
{
	// Confirm the index border for bit scanning.
	u32 result=0xffffffff;
#if defined(_amd64)
	u64 *bmp=(u64*)bitmap;
	u32 limit_index=limit>>6;
	if(limit & 0x3F)limit_index++;
#else
	u32 *bmp=(u64*)bitmap;
	u32 limit_index=limit>>5;
	if(limit & 0x1F)limit_index++;
#endif
	// Search the bitmap.
	for(u32 i=0;i<limit_index;i++)
	{
		// Complement the mask before scanning in
		// that we are looking for a cleared bit.
#if defined(_amd64)
		if(noir_bsf64(&result,~bmp[i]))
		{
			result+=(i<<6);
#else
		if(noir_bsf(&result,~bmp[i]))
		{
			result+=(i<<5);
#endif
			break;
		}
	}
	return result;
}

u32 noir_find_set_bit(void* bitmap,u32 limit)
{
	// Confirm the index border for bit scanning.
	u32 result=0xffffffff;
#if defined(_amd64)
	u64 *bmp=(u64*)bitmap;
	u32 limit_index=limit>>6;
	if(limit & 0x3F)limit_index++;
#else
	u32 *bmp=(u64*)bitmap;
	u32 limit_index=limit>>5;
	if(limit & 0x1F)limit_index++;
#endif
	// Search the bitmap.
	for(u32 i=0;i<limit_index;i++)
	{
		// Use bsf instruction to search.
#if defined(_amd64)
		if(noir_bsf64(&result,bmp[i]))
		{
			result+=(i<<6);
#else
		if(noir_bsf(&result,bmp[i]))
		{
			result+=(i<<5);
#endif
			break;
		}
	}
	return result;
}

size_t nv_strfromd32(char* string,i32 value)
{
	u32 len=0;
	bool neg=false;
	if(value<0)
	{
		value=-value;
		neg=true;
	}
	while(value>0)
	{
		i32 q=value/10,r=value%10;
		string[len++]=(char)r+'0';
		value=q;
	}
	if(neg)
	{
		for(u32 i=len;i;i--)
			string[i]=string[i-1];
		*string='-';
	}
	return len;
}

size_t nv_strfromd64(char* string,i64 value)
{
	u32 len=0;
	bool neg=false;
	if(value<0)
	{
		value=-value;
		neg=true;
	}
	while(value>0)
	{
		i64 q=value/10,r=value%10;
		string[len++]=(char)r+'0';
		value=q;
	}
	if(neg)
	{
		for(u32 i=len;i;i--)
			string[i]=string[i-1];
		*string='-';
	}
	return len;
}

size_t nv_strfromu32(char* string,u32 value)
{
	u32 len=0;
	while(value)
	{
		u32 q=value/10,r=value%10;
		string[len++]=(char)r+'0';
		value=q;
	}
	return len;
}

size_t nv_strfromu64(char* string,u64 value)
{
	u32 len=0;
	while(value)
	{
		u64 q=value/10,r=value%10;
		string[len++]=(char)r+'0';
		value=q;
	}
	return len;
}

void nv_strfromx8(char* string,u8 value)
{
	u8 q=value>>4,r=value&0xF;
	if(r<10)
		string[0]=r+'0';
	else
		string[0]=r+'a';
	if(q<10)
		string[1]=q+'0';
	else
		string[1]=q+'a';
}

void nv_strfromx32(char* string,u32 value)
{
	for(u32 i=0;i<8;i++)
	{
		u32 q=value>>4,r=value&0xF;
		if(r<10)
			string[7-i]=(char)r+'0';
		else
			string[7-i]=(char)r+'a';
		value=q;
	}
}

void nv_strfromx64(char* string,u64 value)
{
	for(u32 i=0;i<15;i++)
	{
		u64 q=value>>4,r=value&0xF;
		if(r<10)
			string[15-i]=(char)r+'0';
		else
			string[15-i]=(char)r+'a';
		value=q;
	}
}

size_t cdecl nv_vsnprintf(char* buffer,size_t limit,const char* format,va_list arg_list)
{
	size_t acclen=0;
	for(size_t i=0;format[i];i++)
	{
		if(format[i]=='%')
		{
			size_t inter_len=0;
			// Format identifier is found!
			switch(format[i+1])
			{
				case 'b':
				{
					char buff_b[2];
					nv_strfromx8(buff_b,va_arg(arg_list,u8));
					inter_len=sizeof(buff_b);
					if(acclen+inter_len>=limit)inter_len=limit-acclen;
					noir_movsb(&buffer[acclen],&buff_b[2-inter_len],inter_len);
					inter_len=2;
				}
				case 'c':
				{
					buffer[i]=va_arg(arg_list,char);
					inter_len=1;
					break;
				}
				case 'd':
				{
					char buff_d[12];
					noir_stosb(buff_d,0,sizeof(buff_d));
					inter_len=nv_strfromd32(buff_d,va_arg(arg_list,i32));
					if(acclen+inter_len>=limit)inter_len=limit-acclen;
					noir_movsb(&buffer[acclen],buff_d,inter_len);
					break;
				}
				case 'l':
				{
					// Long integer...
					if(format[i+2]=='l')
					{
						switch(format[i+3])
						{
							case 'd':
							{
								char buff_d[24];
								noir_stosb(buff_d,0,sizeof(buff_d));
								inter_len=nv_strfromd64(buff_d,va_arg(arg_list,i64));
								if(acclen+inter_len>=limit)inter_len=limit-acclen;
								noir_movsb(&buffer[acclen],buff_d,inter_len);
								break;
							}
							case 'x':
							{
								char buff_x[16];
								nv_strfromx64(buff_x,va_arg(arg_list,u64));
								inter_len=sizeof(buff_x);
								if(acclen+inter_len>=limit)inter_len=limit-acclen;
								noir_movsb(&buffer[acclen],&buff_x[8-inter_len],inter_len);
								break;
							}
							case 'u':
							{
								char buff_u[24];
								noir_stosb(buff_u,0,sizeof(buff_u));
								inter_len=nv_strfromu64(buff_u,va_arg(arg_list,u64));
								if(acclen+inter_len>=limit)inter_len=limit-acclen;
								noir_movsb(&buffer[acclen],buff_u,inter_len);
								break;
							}
						}
					}
					break;
				}
				case 'p':
				{
#if defined(_amd64)
					char buff_p[16];
					nv_strfromx64(buff_p,va_arg(arg_list,u64));
#else
					char buff_p[8];
					nv_strfromx32(buff_p,va_arg(arg_list,u32));
#endif
					inter_len=sizeof(buff_p);
					if(acclen+inter_len>=limit)inter_len=limit-acclen;
					noir_movsb(&buffer[acclen],&buff_p[sizeof(buff_p)-inter_len],inter_len);
					break;
				}
				case 's':
				{
					char* str=va_arg(arg_list,char*);
					for(size_t j=0;j<limit-acclen;j++)
					{
						if(acclen+j>=limit)break;
						buffer[acclen+j]=str[j];
						inter_len++;
						if(str[j]=='\0')break;
					}
					break;
				}
				case 'x':
				{
					char buff_x[8];
					nv_strfromx32(buff_x,va_arg(arg_list,u32));
					inter_len=sizeof(buff_x);
					if(acclen+inter_len>=limit)inter_len=limit-acclen;
					noir_movsb(&buffer[acclen],&buff_x[8-inter_len],inter_len);
					break;
				}
				case 'u':
				{
					char buff_u[12];
					noir_stosb(buff_u,0,sizeof(buff_u));
					inter_len=nv_strfromu32(buff_u,va_arg(arg_list,u32));
					if(acclen+inter_len>=limit)inter_len=limit-acclen;
					noir_movsb(&buffer[acclen],buff_u,inter_len);
					break;
				}
			}
			acclen+=inter_len;
		}
		else if(format[i]=='\n')
		{
			buffer[acclen++]='\r';
			buffer[acclen++]='\n';
		}
		else
			buffer[acclen++]=format[i];
	}
	return acclen;
}

size_t cdecl nv_snprintf(char* buffer,size_t limit,const char* format,...)
{
	size_t retlen;
	va_list arg_list;
	va_start(arg_list,format);
	retlen=nv_vsnprintf(buffer,limit,format,arg_list);
	va_end(arg_list);
	return retlen;
}