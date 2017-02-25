#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}


int write(int id, const void* buffer, unsigned sized)
{
	if(id == 1){
		putbuf(buffer,sized);
		return sized;
	}
	else return 0;
}
void exit(int status)
{
	printf("%s: exit(%d)\n",thread_current()->name,status);
	thread_exit();
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  //first check if f->esp is a valid pointer)
  if (f->esp == NULL)
  {
  	exit(-1); 
  }
  //cast f->esp into an int*, then dereference it for the SYS_CODE
  switch(*(int*)f->esp)
  {
      case SYS_WRITE:
      {
         int fd = *((int*)f->esp + 1);
         void* buffer = (void*)(*((int*)f->esp+2));
         unsigned size = *((unsigned*)f->esp + 3);
         f->eax = write(fd, buffer, size);
  		  break;
  	  }
  	  case SYS_EXIT:
  	  {
      //Implement syscall EXIT
  	  	exit(0);
  		break; 
  	  }
  }
}
