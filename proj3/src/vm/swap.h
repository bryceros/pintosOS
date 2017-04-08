#ifndef SWAP_H
#define SWAP_H

#include "devices/block.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "threads/synch.h"
#include <bitmap.h>

void swap_init (void);
size_t swap_write (void *frame);
void swap_read (size_t used_index, void* frame);

#endif