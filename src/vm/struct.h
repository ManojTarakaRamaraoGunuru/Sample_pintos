#ifndef VM_STRUCT_H
#define VM_STRUCT_H

#include <stdio.h>
#include <string.h>
#include <bitmap.h>
#include <hash.h>
#include <list.h>
#include <stdbool.h>
#include <stddef.h>

// #include "lib/user/syscall.h"

#include "userprog/syscall.h"
#include "userprog/pagedir.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "devices/block.h"
#include "threads/pte.h"
#include "threads/palloc.h"
#include "filesys/off_t.h"

typedef int mapid_t;

//an array of locks for various purposes
#define NO_OF_LOCKS 7
struct lock l[NO_OF_LOCKS];
#define LOCK_LOAD 0
#define LOCK_UNLOAD 1
#define LOCK_FILE 2
#define LOCK_FRAME 3
#define LOCK_EVICT 4
#define LOCK_SWAP 5
#define LOCK_MMAP 6

//determines the type of the file
#define TYPE_ZERO 0
#define TYPE_FILE 1
#define TYPE_SWAP 2

//instructs the load function to perform required operation
#define OP_LOAD 0
#define OP_UNLOAD 1
#define OP_FIND 2
#define OP_FREE 3


/*For page*/
struct page_struct{
    int type; /*type of page*/
    void *virtual_address;
    void* physical_address;
    bool writable;  /*determines if page is writeable*/
    uint32_t *pagedir;  /*real pagediretory*/
    struct list_elem frame_elem; 
    size_t index;   /*index of swap_block*/
    bool loaded; /*determines if page is laoded*/

    struct file* file;  /*the file  struct of the page*/
    off_t offset;          /*offset in the file*/
    off_t bid;            /*inode block index*/
    size_t read_bytes;      /*read bytes*/
    size_t zero_bytes;      /*zero bytes*/
};

/* For Frame*/

struct hash hash_frame;
struct list hash_frame_list;

struct frame_struct{
    void *physical_address; //Physical address of the frame
	bool persistent; //determines if the frame is pinned or not
	struct list shared_pages; //list of all pages that share this frame
	struct list_elem frame_list_elem; //list element for the frames list
	struct lock page_list_lock; //page access is synchronized using
	struct hash_elem hash_elem; //for hash frame table
};

/*For Swap*/
struct block*swap_block;
struct bitmap *swap_bitmap;
size_t swap_size;

/*for Mmap*/
struct hash hash_mmap;
struct mmap_struct{
    mapid_t mapid;
    int fid; /*file desdriptor*/
    struct list_elem thread_mmap_list;  /*lsit of thread_mmaps*/
    struct hash_elem frame_hash_elem;   /* hash element from frame tables*/
    // Since file can't be stored in a single page.
    void*start_address;
    void*end_address;  
};
#endif
