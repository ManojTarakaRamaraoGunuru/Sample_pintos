#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "userprog/process.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "devices/input.h"
#include "devices/shutdown.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "threads/malloc.h"

#include "vm/page.h"
#include "vm/frame.h"



static int new_mapid =2;



#define STDIN_FILENO 0
#define STDOUT_FILENO 1

void is_valid_ptr(const void*ptr){
  // If ptr is invalid exit immediately by rising -1
  // Invalid_ptr in the sense example: below initially *esp pointing below the PHYS_BASE
  if (!is_user_vaddr(ptr))exit(-1);
  void *temp = pagedir_get_page(thread_current()->pagedir, ptr);
  // If user virtual address is unmapped to physical address returns NULL
  if (temp == NULL){
    // printf("fail\n");
    exit(-1);
  }
}

struct fd_entry*get_fd(int x){
  struct thread*curr=thread_current();
  for(struct list_elem* i=list_begin(&curr->fd_table);i!=list_end(&curr->fd_table); i=list_next(i)){
    struct fd_entry*temp=list_entry(i,struct fd_entry, fd_elem);
    if(temp->fd==x){
      return temp;
    }
  }
  return NULL;
}

static void syscall_handler(struct intr_frame *);

void syscall_init(void)
{
  intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall");
  lock_init(&lock_file);
}

void halt(){
  shutdown_power_off();
}


void exit(int status){
  struct thread*curr=thread_current();
  printf ("%s: exit(%d)\n", curr->name, status);

  struct child_element*child=get_child(curr->tid,&curr->parent->child_list);

  child->exit_status=status;
  if(status==-1){
    child->cur_status=WAS_KILLED;
  }
  else{
    child->cur_status=HAD_EXITED;
  }
  thread_exit();
}

pid_t exec(const char*file){
  if(file==NULL)return -1;
  struct thread* cur=thread_current();
  //printf("Exec\n");
  tid_t pid=process_execute(file);
  struct child_element* child=get_child(pid,&cur->child_list);
  sema_down(&child-> act_child -> sema_exec);
  if(!child -> load)
    return -1;
  return pid;
}

int wait(pid_t pid){
  return process_wait(pid);
}
bool create(const char*file, unsigned initial_size){
  lock_acquire(&lock_file);
  bool ans=filesys_create(file, initial_size);
  lock_release(&lock_file);
  return ans;
}

bool remove (const char *file){
  lock_acquire(&lock_file);
   bool ans= filesys_remove(file);
   lock_release(&lock_file);
   return ans;
 }

 int open (const char *file){
 int ans=-1;
 lock_acquire(&lock_file);
 struct thread*curr=thread_current();
 struct file*new_file=filesys_open(file);
 lock_release(&lock_file);
 if(new_file){
   curr->fd_size++;
   ans=curr->fd_size;
   struct fd_entry* temp=(struct fd_entry*)malloc(sizeof(struct fd_entry));
   temp->fd=ans;
   temp->act_file=new_file;
   list_push_back(&curr->fd_table, &temp->fd_elem);
 }
 return ans;
}

void close(int fd){
  struct fd_entry* temp=get_fd(fd);
  if(temp!=NULL){
    struct file* act_file=temp->act_file;
    lock_acquire(&lock_file);
    file_close(act_file);
    lock_release(&lock_file);
    list_remove(&temp->fd_elem);
    free(temp);
  }
}

int read (int fd, void *buffer, unsigned length){
  void*buffer_tmp=buffer;
  is_valid_ptr((int*)buffer_tmp);
  int status=0;
  if(fd==1){
    return -1;
  }
  else if(fd==0){
    status=input_getc();
    return status;
  }
  else{
    struct fd_entry* temp=get_fd(fd);
    if(temp==NULL||buffer==NULL)return -1;
    struct file* act_file=temp->act_file;
    lock_acquire(&lock_file);
    status=file_read(act_file, buffer, length);
    lock_release(&lock_file);
    if(status<(int)length && status!=0)status=-1;
  }
  return status;
}

int filesize (int fd){
  struct fd_entry* temp=get_fd(fd);
  struct file*act_file=temp->act_file;
  lock_acquire(&lock_file);
  int len=file_length(act_file);
  lock_release(&lock_file);
  return len;
}

int write(int fd, void *buffer, unsigned size)
{
  // struct file_descriptor *fd_struct;
  int status = 0;
  unsigned buffer_size = size;
  void *buffer_tmp = buffer;
  /*   is the user memory pointing by buffer are valid */
  while (buffer_tmp != NULL)
  {
    is_valid_ptr((int*)buffer_tmp);
    if (buffer_size > PGSIZE)
    {
      buffer_tmp += PGSIZE;
      buffer_size -= PGSIZE;
    }
    else if (buffer_size == 0)
    {
      /* terminate the   ising loop */
      buffer_tmp = NULL;
    }
    else
    {
      /* last loop */
      buffer_tmp = buffer + size - 1;
      buffer_size = 0;
    }
  }
  if (fd == STDIN_FILENO)
  {
    status = -1;
  }
  else if (fd == STDOUT_FILENO)
  {
    putbuf(buffer, size);
    status = size;
  }
  else
  {
    struct fd_entry* temp=get_fd(fd);
    if(temp==NULL||buffer==NULL)return -1;
    struct file* act_file=temp->act_file;
    lock_acquire(&lock_file);
    status=file_write(act_file, buffer, size);
    lock_release(&lock_file);
  }
  return status;
}

void seek(int fd, unsigned position){
  struct fd_entry*temp=get_fd(fd);
  if(temp!=NULL){
    struct file*act_file=temp->act_file;
    lock_acquire(&lock_file);
    file_seek(act_file, position);
    lock_release(&lock_file);
  }
}

unsigned tell(int fd){
  struct fd_entry* temp=get_fd(fd);
  if(temp==NULL)return 0;          
  struct file*act_file=temp->act_file;
  lock_acquire(&lock_file);
  unsigned ans=file_tell(act_file);
  lock_release(&lock_file);
  return ans;
}

mapid_t mmap(int fd, void *address){

	ASSERT(fd != STDIN_FILENO || fd != STDOUT_FILENO);

	size_t size;
	struct file *f = NULL;
	size_t start_bytes;
	size_t first_bytes;

	size = filesize(fd);
	lock_acquire(&lock_file);
	struct fd_entry *fs = NULL;
	fs = get_fd(fd);

	if (fs != NULL)
	f = file_reopen(fs->act_file);
	else
	{
		lock_release(&lock_file);
		return -1;
	}
	lock_release(&lock_file);

	if (f == NULL || size <= 0 || address == NULL || address == 0x0
			|| pg_ofs(address) != 0)
	return -1;

	size_t offset = 0;
	void *tmp_addr = address;

	while (size > 0)
	{
		if (size < PGSIZE)
		{
			start_bytes = size;
			first_bytes = PGSIZE - size;
		}
		else
		{
			start_bytes = PGSIZE;
			first_bytes = 0;
		}

		//If a page is already mapped, fail
		struct page_struct * temp = vm_find_page(tmp_addr);
		if (temp == NULL)
		{

			temp = vm_new_page(TYPE_FILE, tmp_addr, true, f, offset,
					start_bytes, first_bytes, -1);

			if (temp != NULL)
			{
				offset += PGSIZE;
				size -= start_bytes;
				tmp_addr += PGSIZE;
			}
			else
			return -1;
		}
		else
		return -1;
	}

	mapid_t mapid = new_mapid++;

	struct mmap_struct *mf = (struct mmap_struct *) malloc(
			sizeof(struct mmap_struct));
	if (mf != NULL)
	{
		lock_acquire(&l[LOCK_MMAP]);
		mf->fid = fd;
		mf->mapid = mapid;
		mf->start_address = address;
		mf->end_address = tmp_addr;

		//Insert file to hashmap
		list_push_front(&thread_current()->mmap_files, &mf->thread_mmap_list);
		hash_insert(&hash_mmap, &mf->frame_hash_elem);

		lock_release(&l[LOCK_MMAP]);
		return mapid;
	}
	else
	return -1;
}
void munmap(mapid_t mapid)
{
	struct mmap_struct mm_temp;
	struct mmap_struct *mf = NULL;
	struct hash_elem *e = NULL;
	struct page_struct *page = NULL;

	mm_temp.mapid = mapid;
	e = hash_find(&hash_mmap, &mm_temp.frame_hash_elem);
	if (e != NULL)
	{
		mf = hash_entry(e, struct mmap_struct, frame_hash_elem);
	}
	if (mf != NULL)
	{
		void *end_addr = mf->end_address;
		void *cur_addr = mf->start_address;
		// Given a file, free each page mapped in memory
		while (cur_addr < end_addr)
		{
			page = vm_find_page(cur_addr);
			if (page != NULL)
			{
				if (page->loaded)
				{
					vm_pin(true, page, false);

					if (page->loaded && page->physical_address != NULL)
					vm_free_frame(page->physical_address, page->pagedir);
					else
					PANIC("Problem in system unmap");
				}
				vm_operation_page(OP_FREE, page, NULL, NULL);
				cur_addr += PGSIZE;
				page = NULL;
			}
		}
	}
	else
	exit(-1);

	//remove the file from hash table
	lock_acquire(&l[LOCK_MMAP]);
	hash_delete(&hash_mmap, &mf->frame_hash_elem);
	list_remove(&mf->thread_mmap_list);
	free(mf);
	lock_release(&l[LOCK_MMAP]);
}



static void
syscall_handler(struct intr_frame *f UNUSED)
{
  is_valid_ptr(f->esp);
  void* args = f->esp;
  args += 4;
  is_valid_ptr(args);
  int no = *(int*)f->esp;
  if(no==SYS_HALT){
    halt();
  }
  else if(no==SYS_EXIT){
    int argv = *(int*)args;
    exit(argv);
  }
  else if(no==SYS_EXEC){
    int argv = *(int*)args;
    is_valid_ptr((void*)argv);
    f->eax=exec(((char*)argv));
  }
  else if(no==SYS_WAIT){
    int argv = *(int*)args;
    f->eax=wait(argv);
  }
   else if(no==SYS_CREATE){
    int argv = *(int*)args;
    args+=4;
    int argv1=*(int*)args;
    is_valid_ptr((void*)argv);
    f->eax=create(((char*)argv),(unsigned)argv1);
  }
  else if(no==SYS_REMOVE){
    int argv = *(int*)args;
    is_valid_ptr((void*)argv);
    f->eax=remove((char*)argv);
  }
   else if(no==SYS_OPEN){
     int argv = *(int*)args;
      is_valid_ptr((void*)argv);
      f->eax=open((char*)argv);
  }
  else if(no==SYS_FILESIZE){
      int argv = *(int*)args;
      f->eax=filesize(argv);
  }
  else if (no == SYS_WRITE)
  {
    int argv = *(int*)args;
    args+=4;
    int argv1 = *(int*)args;
    args+=4;
    int argv2 = *(int*)args;
    is_valid_ptr((const void*)argv1);
    void *temp = ((void*)argv1)+argv2;
    is_valid_ptr((const void*)temp);
    f->eax = write(argv, (void *)argv1, (unsigned)argv2);
  }
  else if(no==SYS_READ){
    int argv = *(int*)args;
    args+=4;
    int argv1 = *(int*)args;
    args+=4;
    int argv2 = *(int*)args;
    is_valid_ptr((const void*)argv1);
    void *temp = ((void*)argv1)+argv2;
    is_valid_ptr((void*)temp);
    f->eax = read(argv, (void *)argv1, (unsigned)argv2);
  }
  else if(no==SYS_SEEK){
    int argv = *(int*)args;
    args+=4;
    int argv1 = *(int*)args;
    seek(argv,(unsigned)argv1);
  }
  else if(no==SYS_TELL){
    int argv = *(int*)args;
    f->eax=tell(argv);
  }
  else if(no==SYS_CLOSE){
    int argv = *(int*)args;
    close(argv);
  }
  else if(no==SYS_MMAP){
    // int argv=*(int*)args;
    // args+=4;
    int argv = *(int*)args;
    args+=4;
    int argv1 = *(int*)args;
    args+=4;
    int argv2 = *(int*)args;
    is_valid_ptr((const void*)argv1);
    is_valid_ptr((const void*)argv2);
    f->eax=mmap(argv1,(void*)argv2);
  }
  else if(no==SYS_MUNMAP){
    int argv = *(int*)args;
    args+=4;
    int argv1 = *(int*)args;
    is_valid_ptr((const void*)argv1);
    munmap((mapid_t)argv1);
  }
}
