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
#include "filesys/inode.h"
#include "filesys/directory.h"


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
	char* cmd_line2 = malloc(strlen(cmd_line)+1);
	memcpy(cmd_line2, cmd_line, strlen(cmd_line)+1);
	pid_t temp = process_execute (cmd_line2);
	free (cmd_line2);
	return temp;
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
  	success = filesys_create(file, initial_size,false);
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

	
	if(fd == 0){
		input_getc(buffer,sized);
		//lock_release(&file_lock);
		return sized;
	}
	lock_acquire(&file_lock);
	read_file = thread_get_file_by_id(fd);
	if(read_file != NULL)
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
	int offset = -1;
	struct file* write_file;

	if(fd == 1){

		putbuf(buffer,sized);
		//lock_release(&file_lock);
		return sized;
	}
	lock_acquire(&file_lock);
	write_file = thread_get_file_by_id(fd);
	//printf("bool: %d\n",write_file->deny_write);
	if (write_file != NULL && inode_get_type(file_get_inode(write_file)) == false)
	{
			//	printf("in\n");
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
bool 
chdir (const char *dir)
{
  bool full;
	struct dir* new_dir = dir_getdir(dir,&full);
	//printf("1\n");
	dir_swap_parent(&new_dir);
	//printf("4\n");
	char *filename = filesys_get_filename(dir);
	struct inode* node;
	if (new_dir != NULL && dir_lookup(new_dir,filename,&node))
	{
		//printf("5\n");
		thread_current()->curr_dir = dir_open(node);
		dir_close(new_dir);
		return true;
	}
	//printf("6\n");
	dir_close(new_dir);
	return false;
}

bool 
mkdir (const char *dir)
{
	return filesys_create (dir, 0, true);
}
bool 
readdir (int fd, char *name)
{
	if(isdir(fd))
	{	
		struct file* readdir = thread_get_file_by_id(fd);
		return dir_readdir(dir_open(file_get_inode(readdir)),name);
	}

	return false;
}
bool 
isdir (int fd)
{
	struct file* isdir = thread_get_file_by_id(fd);
	return inode_get_type(file_get_inode(isdir));
}
int 
inumber (int fd)
{
	struct file* inumber_file = thread_get_file_by_id(fd);
		return inode_get_inumber(file_get_inode(inumber_file));
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
	}case SYS_CHDIR:{
		is_valid_pointer((int*)f->esp + 1);

		const char* file = (const char*)(*((int*)f->esp+1));
		is_valid_pointer(file);

		f->eax = chdir(file);
		break;
	}case SYS_MKDIR:{
		is_valid_pointer((int*)f->esp + 1);

		const char* file = (const char*)(*((int*)f->esp+1));
		is_valid_pointer(file);

		f->eax = mkdir(file);
		break;
	}case SYS_READDIR:{
		is_valid_pointer((int*)f->esp + 1);
		is_valid_pointer((int*)f->esp + 2);


		int fd = *((int*)f->esp + 1);
		const char* file = (const char*)(*((int*)f->esp+2));
		is_valid_pointer(file);

		f->eax = readdir(fd,file);
		break;
	}case SYS_ISDIR:{
		is_valid_pointer((int*)f->esp + 1);

		int fd = *((int*)f->esp + 1);

		f->eax = isdir(fd);
		break;
	}case SYS_INUMBER:{
		is_valid_pointer((int*)f->esp + 1);

		int fd = *((int*)f->esp + 1);
		
		f->eax = inumber(fd);
		break;
	}
  }
}
