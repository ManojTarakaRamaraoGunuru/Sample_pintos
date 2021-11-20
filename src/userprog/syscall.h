#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include <stdbool.h>
#include "threads/thread.h"
#include <list.h>
#include "threads/synch.h"
#include "vm/struct.h"


#define pid_t tid_t
typedef int mapid_t ;

struct fd_entry{
    int fd;
    struct file*act_file;
    struct list_elem fd_elem;
};

void syscall_init (void);
struct lock lock_file;

struct fd_entry* get_fd(int x);


void is_valid_ptr (const void *ptr);
struct child_element* get_child(tid_t tid,struct list *mylist);

void halt (void) NO_RETURN;
void exit (int status) NO_RETURN;
tid_t exec (const char *file);
int wait (pid_t pid);
bool create (const char *file, unsigned initial_size);
bool remove (const char *file);
int open (const char *file);
int filesize (int fd);
int read (int fd, void *buffer, unsigned length);
int write (int fd, void *buffer, unsigned length);
void seek (int fd, unsigned position);
unsigned tell (int fd);
void close (int fd);

//added for project 3s
mapid_t mmap(int fd, void *addr);					
void munmap(mapid_t mapid);


#endif /* userprog/syscall.h */
