/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2020, Zero Tang. All rights reserved.

  This file defines basic types, constants, etc.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /include/nvdef.h
*/

#if defined(_msvc)
#pragma once

typedef unsigned __int8		u8;
typedef unsigned __int16	u16;
typedef unsigned __int32	u32;
typedef unsigned __int64	u64;

typedef signed __int8		i8;
typedef signed __int16		i16;
typedef signed __int32		i32;
typedef signed __int64		i64;
#endif

#if defined(_amd64)
typedef u64 ulong_ptr;
typedef i64 long_ptr;
#else
typedef u32 ulong_ptr;
typedef i32 long_ptr;
#endif

typedef volatile u8		u8v;
typedef volatile u16	u16v;
typedef volatile u32	u32v;
typedef volatile u64	u64v;

typedef volatile i8		i8v;
typedef volatile i16	i16v;
typedef volatile i32	i32v;
typedef volatile i64	i64v;

typedef volatile ulong_ptr	vulong_ptr;
typedef volatile long_ptr	vlong_ptr;

#if defined(_msvc)
typedef enum
{
	false=0,
	true=1
}bool;

#define cdecl		__cdecl
#define stdcall		__stdcall
#define fastcall	__fastcall

#define inline			__inline
#define always_inline	__forceinline
#endif

#define null	(void*)0
#define maxu8	0xff
#define maxu16	0xffff
#define maxu32	0xffffffff
#define maxu64	0xffffffffffffffff