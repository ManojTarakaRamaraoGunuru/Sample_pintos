#ifndef VM_PAGE_H
#define VM_PAGE_H

#include "vm/struct.h"
#include <list.h>
#include <stdbool.h>
#include <stddef.h>
#include "filesys/file.h"

struct page_struct *vm_new_page(int type, void *virt_address, bool writable,
		struct file *file, off_t ofs, size_t read_bytes, size_t zero_bytes,
		off_t block_id);

bool vm_pin(bool operation, void*pagetemp, bool directFrameAccess);

void vm_init(void);

bool vm_operation_page(int type, void*address, void* kpage, bool pinned);

struct page_struct* vm_find_page(void*address);

struct page_struct* vm_stack_grow(void* address, bool pin);

unsigned frame_hash(const struct hash_elem*f_, void*aux);

bool frame_less_helper(const struct hash_elem*a_, const struct hash_elem* b_, void*aux);

unsigned mmap_hash(const struct hash_elem*mf_, void*aux);

bool mmap_less_helper(const struct hash_elem* a_, const struct hash_elem*b_, void*aux);

#endif