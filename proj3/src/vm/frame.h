#ifndef VM_FRAME_H
#define VM_FRAME_H
#include <stdint.h>
#include <stdbool.h>
#include <list.h>
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/synch.h"
#include "vm/page.h"



//a list of frame_table_entry as the page table

struct list frametable;
struct lock frametable_lock;
struct frame_entry {
    uint32_t* frame;
	struct thread* owner;
    struct page_entry* aux;
    // Maybe store information for memory mapped files here too?
    struct list_elem elem;              /* List element. */

};

void frametable_init(void);

struct frame_entry* frame_get_page(struct page_entry* page);

struct frame_entry* frame_create (void *frame, struct page_entry *page);

void frame_insert (struct frame_entry* );
void frame_remove (struct frame_entry* );

void frame_evict(void);
bool frame_reclaim(struct page_entry *page);

bool frame_comp_less_time(const struct list_elem *a,const struct list_elem *b, void *aux);


#endif