/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2020, Zero Tang. All rights reserved.

  This file defines basic types, constants, etc.

  This program is distributed in the hope that it will be useful, but 
  without any warranty (no matter implied warranty or merchantability
  or fitness for a particular purpose, etc.).

  File Location: /include/nvdef.h
*/

#if defined(_msvc) || defined(_llvm)
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

typedef u8*		u8p;
typedef u16*	u16p;
typedef u32*	u32p;
typedef u64*	u64p;

typedef i8*		i8p;
typedef i16*	i16p;
typedef i32*	i32p;
typedef i64*	i64p;

#if defined(_amd64)
typedef u64 ulong_ptr;
typedef i64 long_ptr;
#else
typedef u32 ulong_ptr;
typedef i32 long_ptr;
typedef i32 size_t;
#endif

typedef ulong_ptr*	ulong_ptr_p;
typedef long_ptr*	long_ptr_p;
typedef size_t*		size_p;

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

typedef u8v*	u8vp;
typedef u16v*	u16vp;
typedef u32v*	u32vp;
typedef u64v*	u64vp;

typedef i8v*	i8vp;
typedef i16v*	i16vp;
typedef i32v*	i32vp;
typedef i64v*	i64vp;

typedef vulong_ptr*	vulong_ptr_p;
typedef vlong_ptr*	vlong_ptr_p;

typedef union r128
{
	float s[4];		// Single-Precision
	double d[2];	// Double-Precision
}*r128;

#if defined(_msvc) || defined(_llvm)
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