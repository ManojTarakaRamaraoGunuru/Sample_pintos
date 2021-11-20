#include "vm/struct.h"
#include "vm/page.h"
#include "filesys/file.h"


void vm_init(void){
    for(int i=0;i<NO_OF_LOCKS;i++)lock_init(&l[i]);

    hash_init(&hash_frame, frame_hash, frame_less_helper,NULL);
    hash_init(&hash_mmap, mmap_hash,mmap_less_helper,NULL);
    list_init(&hash_frame_list);
    swap_block=block_get_role(BLOCK_SWAP);
    swap_size=block_size(swap_block);
    swap_bitmap=bitmap_create(swap_size);
}

struct page_struct *vm_new_page(int type, void *virt_address, bool writable,
		struct file *file, off_t ofs, size_t read_bytes, size_t zero_bytes,
		off_t block_id)
{
    struct page_struct*p=(struct page_struct*)malloc(sizeof(struct page_struct));
    if(p!=NULL){
        p->physical_address=NULL;
        p->virtual_address=virt_address;
        p->index=0;
        p->pagedir=thread_current()->pagedir;
        p->loaded=false;
        p->writable=writable;
    }
    if(type==TYPE_ZERO){
        if(p!=NULL){
            p->type=TYPE_ZERO;
            pagedir_op_page(p->pagedir,p->virtual_address,(void*)p);
            return p;
        }
    }
    else if(type==TYPE_FILE){
        if(p!=NULL){
            p->type=TYPE_FILE;
            p->file=file;
            p->offset=ofs;
            p->read_bytes=read_bytes;
            p->zero_bytes=zero_bytes;
            p->bid=block_id;
            pagedir_op_page(p->pagedir,p->virtual_address,(void*)p);
            return p;
        }
    }
    return NULL;
}


bool vm_pin(bool operation, void*page_temp, bool direct_frame_access){
    struct page_struct*page=(struct page_struct*)page_temp;
    void*address=page->physical_address;
    if(direct_frame_access==true)address=page_temp;
    if(page->physical_address==NULL || direct_frame_access==true){
        struct frame_struct*f=address_to_frame(address);
        if(f!=NULL){
            if(operation==true){
                f->persistent=true;
                return true;
            }
            else{
                f->persistent=false;
                return true;
            }
        }
    }
    return false;
}

bool vm_operation_page(int operation, void*address, void*kpage, bool pinned){
    if(operation==OP_LOAD){
        lock_acquire(&l[LOCK_LOAD]);
        struct page_struct*page=(struct page_struct*)address;
        if(page->physical_address==NULL){
            page->physical_address=vm_get_frame(NULL, NULL,PAL_USER);
        }
        lock_release(&l[LOCK_LOAD]);

        struct frame_struct* vf=address_to_frame(page->physical_address);
        if(vf==NULL){
            return false;
        }
        lock_acquire(&vf->page_list_lock);

        list_push_back(&vf->shared_pages, &page->frame_elem);

        lock_release(&vf->page_list_lock);

        bool success=true;

        if(page->type==TYPE_FILE){
            lock_acquire(&lock_file);
            file_seek(page->file,page->offset);

            size_t ret=file_read(page->file, page->physical_address, page->read_bytes);
            lock_release(&lock_file);

            if(ret!=page->read_bytes){
                vm_free_frame(page->physical_address, page->pagedir);
                success=false;
            }
            else{
                void*block=page->physical_address+page->read_bytes;
                memset(block,0,page->zero_bytes);
                success=true;
            }
        }
        else if(page->type==TYPE_ZERO){
            memset(page->physical_address,0,PGSIZE);
        }
        else{
            size_t i=page->index;
            void*address=page->physical_address;
            size_t offset=0;
            int blocks_in_one_page=PGSIZE/BLOCK_SECTOR_SIZE;

            lock_acquire(&l[LOCK_SWAP]);
            while(offset<(unsigned long)blocks_in_one_page){
                if(bitmap_test(swap_bitmap, i) && i<swap_size){
                    void*buffer=address+offset*BLOCK_SECTOR_SIZE;
                    block_read(swap_block, i, buffer);
                }
                else{
                    PANIC("Probelm when loading a page from swap to main memory");
                }
                i++;
                offset++;
            }
            offset=0;
            i=page->index;
            while(offset<(unsigned long)blocks_in_one_page){
                if(bitmap_test(swap_bitmap, i)&&i<swap_size){
                    bitmap_reset(swap_bitmap,i);
                }
                else{
                    PANIC("Problems when freeong the swap");
                }
                i++;
                offset++;
            }
            lock_release(&l[LOCK_SWAP]);
        }
        if(success==false){
            vm_pin(false,page->physical_address, true);
            return false;
        }
        pagedir_clear_page(page->pagedir, page->virtual_address);
        bool s=pagedir_set_page(page->pagedir, page->virtual_address, page->physical_address, page->writable);
        if(s==false){
            ASSERT(false);                              /*check here*/
            vm_pin(false,page->physical_address,true);
            return false;
        }
        pagedir_set_dirty(page->pagedir,page->virtual_address, false);
        pagedir_set_accessed(page->pagedir, page->virtual_address,true);
        page->loaded=true;
        if(pinned==false){
            vm_pin(false,page->physical_address,true);
        }
        return true;
    }
    else if(operation==OP_UNLOAD){
        struct page_struct*page=(struct page_struct*)address;
        lock_acquire(&l[LOCK_UNLOAD]);
        if(page->type==TYPE_FILE && pagedir_is_dirty(page->pagedir,page->virtual_address) 
        && file_check_write(page->file)==false){
            vm_pin(true,kpage, true);
            lock_acquire(&lock_file);
            file_seek(page->file, page->offset);
            file_write(page->file, kpage, page->read_bytes);
            lock_release(&lock_file);
            vm_pin(false, kpage, true);
        }
        else if(page->type==TYPE_SWAP||pagedir_is_dirty(page->pagedir, page->virtual_address)){
            page->type=TYPE_SWAP;
            void*addr=kpage;
            int blocks_in_one_page=PGSIZE/BLOCK_SECTOR_SIZE;
            lock_acquire(&l[LOCK_SWAP]);
            size_t index=bitmap_scan_and_flip(swap_bitmap, 0, blocks_in_one_page, false);
            if(index!=BITMAP_ERROR){
                size_t offset=0, i=index;
                while(offset<(unsigned long)blocks_in_one_page){
                    if(bitmap_test(swap_bitmap,i)&& index<swap_size){               // verify i and index
                        block_write(swap_block, i, addr+offset*BLOCK_SECTOR_SIZE);
                    }
                    else{
                        PANIC("Problem when moving a page from memory to swap");
                    }
                    i++;
                    offset++;
                }
            }
            else{
                PANIC("Problem when moving a page from memory to swap -- index");
            }
            lock_release(&l[LOCK_SWAP]);
            page->index=index;
        }
        lock_release(&l[LOCK_UNLOAD]);
        pagedir_clear_page(page->pagedir,page->virtual_address);
        pagedir_op_page(page->pagedir, page->virtual_address,(void*)page);
        page->loaded=false;
        page->physical_address=NULL;
    }
    else if(operation==OP_FIND){
        /*check here*/
    }
    else if(operation==OP_FREE){
        struct page_struct*page=(struct page_struct*)address;
        if(page==NULL)return false;

        if(page->type==TYPE_SWAP && !page->loaded){
            size_t offset=0, i=page->index;
            int blocks_in_one_page=PGSIZE/BLOCK_SECTOR_SIZE;
            lock_acquire(&l[LOCK_SWAP]);
            while(offset<(unsigned long)blocks_in_one_page){
                    if(bitmap_test(swap_bitmap,i)&& i<swap_size){                   //verify i and index
                        bitmap_reset(swap_bitmap, i);
                    }
                    else{
                        PANIC("Problem when freeing");
                    }
                    i++;
                    offset++;
                }
            lock_release(&l[LOCK_SWAP]);
        }
        pagedir_clear_page(page->pagedir,page->virtual_address);
        free(page);
        return true;
    }
    else{
        return false;
    }
    return false;
}

struct page_struct* vm_stack_grow(void*address, bool pin){
    struct page_struct* page=vm_new_page(TYPE_ZERO,address,true,NULL,0,0,0,0);
    if(page!=NULL){
        bool success=vm_operation_page(OP_LOAD,page, page->physical_address, pin);
        if(success==true)return page;
        else return NULL;
    }
    return NULL;
}

struct page_struct*vm_find_page(void*address){
    uint32_t*pagedir=thread_current()->pagedir;
    if(pagedir!=NULL){
        struct page_struct*page=(struct page_struct*)pagedir_op_page(pagedir,(void*)address,NULL);
        return page;
    }
    return NULL;
}

unsigned frame_hash(const struct hash_elem* f_,void*aux UNUSED){
    
    const struct frame_struct* f=hash_entry(f_,struct frame_struct, hash_elem);
    return hash_int((unsigned)f->physical_address);
}

bool frame_less_helper(const struct hash_elem*a_, const struct hash_elem*b_, void*aux UNUSED){
    const struct frame_struct* a=hash_entry(a_,struct frame_struct, hash_elem);
    const struct frame_struct* b=hash_entry(b_,struct frame_struct, hash_elem);
    return a->physical_address< b->physical_address;
}

unsigned mmap_hash(const struct hash_elem*mf_, void*aux UNUSED){
    const struct mmap_struct*mf=hash_entry(mf_, struct mmap_struct, frame_hash_elem);
    return hash_int((unsigned)mf->mapid);
}

bool mmap_less_helper(const struct hash_elem*a_, const struct hash_elem* b_, void*aux UNUSED){
    const struct mmap_struct* a=hash_entry(a_,struct mmap_struct, frame_hash_elem);
    const struct mmap_struct* b=hash_entry(b_,struct mmap_struct, frame_hash_elem);
    return a->mapid<b->mapid;
}