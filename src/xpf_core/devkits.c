/*
  NoirVisor - Hardware-Accelerated Hypervisor solution

  Copyright 2018-2024, Zero Tang. All rights reserved.

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

i64 static noir_get_avl_height(avl_node_p node)
{
	return node?node->height:0;
}

i64 static noir_get_avl_balance_factor(avl_node_p node)
{
	return node?noir_get_avl_height(node->right)-noir_get_avl_height(node->left):0;
}

void static noir_reset_avl_height(avl_node_p node)
{
	node->height=i64_max(2,noir_get_avl_height(node->right),noir_get_avl_height(node->left))+1;
}

avl_node_p static noir_rotl_avl_node(avl_node_p x)
{
	avl_node_p y=x->right,t2=y->left;
	y->left=x;
	x->right=t2;
	noir_reset_avl_height(x);
	noir_reset_avl_height(y);
	return y;
}

avl_node_p static noir_rotr_avl_node(avl_node_p y)
{
	avl_node_p x=y->left,t2=x->right;
	x->right=y;
	y->left=t2;
	noir_reset_avl_height(x);
	noir_reset_avl_height(y);
	return x;
}

// In AVL-Tree, return value of insertion is supposed to replace the parent.
avl_node_p noir_insert_avl_node(avl_node_p parent,avl_node_p node,noir_sorting_comparator compare_fn)
{
	// Insert to as-is place.
	if(parent==null)
		return node;
	// Insert according to the comparison result.
	const i32 compare_result=compare_fn(node,parent);
	if(compare_result<0)
		parent->left=noir_insert_avl_node(parent->left,node,compare_fn);
	else
		parent->right=noir_insert_avl_node(parent->right,node,compare_fn);
	noir_reset_avl_height(parent);
	// Retrieve balance factor to determine how to rotate.
	i64 bf=noir_get_avl_balance_factor(parent);
	if(bf>1 && compare_result<0)			// Left-Left Case.
		return noir_rotl_avl_node(parent);
	else if(bf<-1 && compare_result>0)		// Right-Right Case.
		return noir_rotr_avl_node(parent);
	else if(bf>1 && compare_result>0)		// Left-Right Case.
	{
		parent->left=noir_rotl_avl_node(parent->left);
		return noir_rotr_avl_node(parent);
	}
	else if(bf<-1 && compare_result<0)		// Right-Left Case.
	{
		parent->right=noir_rotr_avl_node(parent->right);
		return noir_rotl_avl_node(parent);
	}
	return parent;
}

avl_node_p noir_search_avl_node(avl_node_p root,const void* item,noir_bst_search_comparator compare_fn)
{
	avl_node_p cur=root;
	while(cur)
	{
		i32 result=compare_fn(cur,item);
		if(result==0)
			return cur;
		else if(result<0)
			cur=cur->left;
		else
			cur=cur->right;
	}
	return null;
}

// MMIO is basically memory operations wrapped with memory fences.
u8 noir_mmio_read8(u64 ptr)
{
	u8 val;
	noir_memory_fence();
	val=*(u8p)ptr;
	noir_memory_fence();
	return val;
}

u16 noir_mmio_read16(u64 ptr)
{
	u16 val;
	noir_memory_fence();
	val=*(u16p)ptr;
	noir_memory_fence();
	return val;
}

u32 noir_mmio_read32(u64 ptr)
{
	u32 val;
	noir_memory_fence();
	val=*(u32p)ptr;
	noir_memory_fence();
	return val;
}

u64 noir_mmio_read64(u64 ptr)
{
	u64 val;
	noir_memory_fence();
	val=*(u64p)ptr;
	noir_memory_fence();
	return val;
}

void noir_mmio_write8(u64 ptr,u8 val)
{
	noir_memory_fence();
	*(u8p)ptr=val;
	noir_memory_fence();
}

void noir_mmio_write16(u64 ptr,u16 val)
{
	noir_memory_fence();
	*(u16p)ptr=val;
	noir_memory_fence();
}

void noir_mmio_write32(u64 ptr,u32 val)
{
	noir_memory_fence();
	*(u32p)ptr=val;
	noir_memory_fence();
}

void noir_mmio_write64(u64 ptr,u64 val)
{
	noir_memory_fence();
	*(u64p)ptr=val;
	noir_memory_fence();
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

u64 u64_max(const u64 n,...)
{
	u64 m=0;
	va_list arg_list;
	va_start(arg_list,n);
	for(u64 i=0;i<n;i++)
	{
		u64 a=va_arg(arg_list,u64);
		if(a>m)m=a;
	}
	va_end(arg_list);
	return m;
}

i64 i64_max(const u64 n,...)
{
	i64 m=0x8000000000000000;
	va_list arg_list;
	va_start(arg_list,n);
	for(u64 i=0;i<n;i++)
	{
		i64 a=va_arg(arg_list,i64);
		if(a>m)m=a;
	}
	va_end(arg_list);
	return m;
}

// Use the third-party static library for internal debugger.
int rpl_vsnprintf(char *str,size_t size,const char *format,va_list args);

i32 cdecl nv_vsnprintf(char *buffer,size_t size,const char* format,va_list arg_list)
{
	return rpl_vsnprintf(buffer,size,format,arg_list);
}

i32 cdecl nv_snprintf(char* buffer,size_t size,const char* format,...)
{
	int retlen;
	va_list arg_list;
	va_start(arg_list,format);
	retlen=rpl_vsnprintf(buffer,size,format,arg_list);
	va_end(arg_list);
	return retlen;
}