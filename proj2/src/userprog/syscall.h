#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

void syscall_init (void);
int write(int id, const void* buffer, unsigned sized);
void exit(int status);

#endif /* userprog/syscall.h */
