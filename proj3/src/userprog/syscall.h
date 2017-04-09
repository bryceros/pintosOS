#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H
#include <stdbool.h>
#include <stddef.h>

typedef int pid_t;

struct lock file_lock;


void syscall_init (void);

void halt(void);

void exit(int status);

pid_t exec(const char* cmd_line);

int wait(pid_t pid);

bool create(const char* file, unsigned initial_size);

bool remove(const char* file);

int open(const char* file);

size_t filesize(int fd);

int read(int fd, void* buffer, unsigned sized);

int write(int fd, const void* buffer, unsigned sized);

void seek(int fd, unsigned position);

unsigned tell(int fd);

void close(int fd);


#endif /* userprog/syscall.h */
