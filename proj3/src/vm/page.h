#ifndef PAGE_H
#define PAGE_H
#include <hash.h>
#include <stdint.h>
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "filesys/off_t.h"


//a list or hashtable of sup_page_table_entry as your supplementary page table
//remember, each thread should have its own sup_page_table, so create a new
struct lock pagetable_lock;

enum page_flags
  {
    PAGE_FILE = 002,                   
    PAGE_SWAP = 003,           
    PAGE_MMAP = 004           
  };

struct page_entry {
    uint32_t* user_vaddr;
    /*
    Consider storing the time at which this page was accessed if you want to
       implement LRU!
Use the timer_ticks () function to get this value!
56
*/
	uint64_t access_time;
// You can use the provided PTE functions instead. I’ve posted links to
	bool loaded;
	bool accessed;

	struct hash_elem elem;              /* List element. */

	enum page_flags flag;

	struct file* doc;

	uint32_t read_bytes;

	uint32_t zero_bytes;

	off_t offset;

	size_t swap_entry;

	enum palloc_flags frame_flag;


};

bool page_check(void *fault_addr, void *esp);

unsigned page_hash (const struct hash_elem *elem, void *aux);

bool page_comp_less(const struct hash_elem *a, const struct hash_elem *b, void *aux);

struct page_entry* page_create_file(struct file*, off_t, uint8_t*, uint32_t, uint32_t, bool, enum palloc_flags);

struct page_entry* page_get (void *);

bool page_load_file(struct file*, off_t, uint8_t*, uint32_t, uint32_t, bool);

bool page_grow_stack(void *);

void page_print(struct page_entry*);

bool page_destroy_table(struct hash *pagetable_hash);

#endif
