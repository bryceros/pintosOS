#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "filesys/file.h"
#include "filesys/filesys.h"

#define USER_VADDR_BOTTOM ((void *) 0x08048000)
static void syscall_handler (struct intr_frame *);
static void is_valid_pointer (const void* pointer);
struct lock file_lock;

void
syscall_init (void) 
{
  lock_init(&file_lock);
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

void
is_valid_pointer(const void*  pointer)
{
	if(pointer == NULL || !is_user_vaddr(pointer) || pagedir_get_page(thread_current()->pagedir,pointer) == NULL || pointer < USER_VADDR_BOTTOM) exit(-1);
}

void 
halt()
{
	shutdown_power_off();
}
void 
exit(int status)
{
	printf("%s: exit(%d)\n",thread_current()->name,status);
	thread_current()->exit_status = status;
	thread_exit();
}
pid_t // need to do
exec(const char* cmd_line)
{
	char* cmd_line2 = malloc(strlen(cmd_line));
	memcpy(cmd_line2, cmd_line, strlen(cmd_line)+1);
	return process_execute(cmd_line2);
}
int // need to do
wait(pid_t pid)
{
	return process_wait(pid);
}
bool 
create(const char* file, unsigned initial_size)
{
	bool success = false;
	lock_acquire(&file_lock);
  	success = filesys_create(file, initial_size);
	lock_release(&file_lock);
  	return success;
}
bool 
remove(const char* file)
{
	bool success = false;
	lock_acquire(&file_lock);
  	success = filesys_remove(file);
	lock_release(&file_lock);
  	return success;

}
int // need to do std
open(const char* file)
{ 
	//printf("open: %s\n", file);
  int fd = -1;
  struct file* open_file = NULL; 
  lock_acquire(&file_lock);
  open_file = filesys_open(file);
  if(open_file != NULL){
	fd = thread_add_file(open_file);
  }
  lock_release(&file_lock);
  return fd;
}
size_t 
filesize(int fd)
{	
	size_t size = 0;
    lock_acquire(&file_lock);
    struct file* filesize_file = thread_get_file_by_id(fd);
	if(filesize_file != NULL)
	{
		size = file_length(filesize_file);
	}
	lock_release(&file_lock);
	return size;
}
int // need to do std
read(int fd, void* buffer, unsigned sized)
{
	int offset = -1;
	struct file* read_file;

	lock_acquire(&file_lock);
	if(fd == 0){
		input_getc(buffer,sized);
		lock_release(&file_lock);
		return sized;
	}
	else if((read_file = thread_get_file_by_id(fd)) != NULL)
	{
		offset = file_read(read_file,buffer,sized);
	}
	lock_release(&file_lock);
	return offset;
}
int 
write(int fd, const void* buffer, unsigned sized)
{
	//printf("file_id: %d\n",fd);
	int offset = 0;
	struct file* write_file;
	lock_acquire(&file_lock);

	if(fd == 1){

		putbuf(buffer,sized);
		lock_release(&file_lock);
		return sized;
	}

	write_file = thread_get_file_by_id(fd);
	//printf("bool: %d\n",write_file->deny_write);
	if (write_file != NULL)
	{
		offset = file_write(write_file,buffer,sized);
			  // printf("passed\n");
	}

	lock_release(&file_lock);
	return offset;
}
void 
seek(int fd, unsigned position)
{
	lock_acquire(&file_lock);
	struct file* seek_file = thread_get_file_by_id(fd);
	if (seek_file != NULL)
	{
		file_seek(seek_file,position);
	}
	lock_release(&file_lock);
}
unsigned 
tell(int fd)
{
	unsigned output = 0;
	lock_acquire(&file_lock);
	struct file* tell_file = thread_get_file_by_id(fd);
	if(tell_file != NULL)
	{
		output = file_tell(tell_file);
	}
	lock_release(&file_lock);
	return output;
}
void // need to do std
close(int fd)
{
	bool success = false;
	lock_acquire(&file_lock);
	struct thread_file* close_file = thread_get_thread_file_by_id(fd);
	if (close_file != NULL && close_file->this_file != NULL)
	{
		thread_remove_file(close_file);
		success = true;
	}
	lock_release(&file_lock);
	if (success == false) exit(-1);

}


static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  //first check if f->esp is a valid pointer)
  if (f->esp == NULL)
  {
  	exit(-1); 
  }
  is_valid_pointer(f->esp);

  //cast f->esp into an int*, then dereference it for the SYS_CODE
  switch(*(int*)f->esp)
  {
  	case SYS_HALT:{
  		halt();
  		break;
	}case SYS_EXIT:{
		  //Implement syscall EXIT
		is_valid_pointer((int*)f->esp + 1);

  	  	int status = *((int*)f->esp + 1);  	  	
  	  	exit(status);
  		break;
	}case SYS_EXEC:{
		is_valid_pointer((int*)f->esp + 1);

		const char* cmd_line = (const char*)(*((int*)f->esp+1));
		is_valid_pointer(cmd_line);

		f->eax = exec(cmd_line);
		break;
	}case SYS_WAIT:{
		is_valid_pointer((int*)f->esp + 1);

		pid_t pid = *((int*)f->esp + 1); 
		f->eax = wait(pid);
		break;
	}case SYS_CREATE:{
		is_valid_pointer((int*)f->esp + 1);
		is_valid_pointer((int*)f->esp + 2);

		const char* file = (const char*)(*((int*)f->esp+1));
		is_valid_pointer(file);

		unsigned initial_size = *((unsigned*)f->esp + 2);
		f->eax = create(file,initial_size);
		break;
	}case SYS_REMOVE:{
		is_valid_pointer((int*)f->esp + 1);

		const char* file = (const char*)(*((int*)f->esp+1));
		is_valid_pointer(file);

		f->eax = remove(file);
		break;
	}case SYS_OPEN:{
		is_valid_pointer((int*)f->esp + 1);

		const char* file = (const char*)(*((int*)f->esp+1));
		is_valid_pointer(file);

		f->eax = open(file);
		break;
	}case SYS_FILESIZE:{
		is_valid_pointer((int*)f->esp + 1);

		int fd = *((int*)f->esp + 1);  	  	
		f->eax = filesize(fd);
		break;
	}case SYS_READ:{ 
		is_valid_pointer((int*)f->esp + 1);
		is_valid_pointer((int*)f->esp + 2);
		is_valid_pointer((int*)f->esp + 3);

		int fd = *((int*)f->esp + 1);
        void* buffer = (void*)(*((int*)f->esp+2));
        is_valid_pointer(buffer);

        unsigned size = *((unsigned*)f->esp + 3);
        f->eax = read(fd, buffer, size);

		break;
	}case SYS_WRITE:{
		is_valid_pointer((int*)f->esp + 1);
		is_valid_pointer((int*)f->esp + 2);
		is_valid_pointer((int*)f->esp + 3);

		int fd = *((int*)f->esp + 1);

        void* buffer = (void*)(*((int*)f->esp+2));
        is_valid_pointer(buffer);

        unsigned size = *((unsigned*)f->esp + 3);
         f->eax = write(fd, buffer, size);
  		 break;
	}case SYS_SEEK:{
		is_valid_pointer((int*)f->esp + 1);

		int fd = *((int*)f->esp + 1);
		unsigned position = *((unsigned*)f->esp + 2);
		seek(fd,position);
		break;
	}case SYS_TELL:{
		is_valid_pointer((int*)f->esp + 1);

		int fd = *((int*)f->esp + 1);
		f->eax = tell(fd);
		break;
	}case SYS_CLOSE:{
		is_valid_pointer((int*)f->esp + 1);

		int fd = *((int*)f->esp + 1);
		close(fd);
		break;
	}
  }
}
