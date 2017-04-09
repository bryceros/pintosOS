#include "vm/page.h"
#include <stdlib.h>
#include "threads/thread.h"
#include "threads/loader.h"
#include "threads/vaddr.h"
#include "vm/frame.h"
#include "devices/timer.h"
//#include "userprog/process.h"


#define  PAGE_SIZE  (4096)

//lock_init(&pagetable_lock);
extern struct lock file_lock;

bool 
page_check(void *fault_addr, void* esp)
{      
	//printf("////////////////////////////////////////////in page check\n");
	struct page_entry *page = page_get(fault_addr);

	if(page != NULL)
	{
		//page_print(page);
		page->access_time = timer_ticks();
		page->loaded = true;

		switch(page->flag)
		{
		    case PAGE_FILE: 
		    	return frame_reclaim(page);
		    case PAGE_SWAP: 
		    	return frame_reclaim(page);
		    case PAGE_MMAP:  return false;
		    default: return false;
		}

	}
      else if( fault_addr >= esp -32)
	{
		return page_grow_stack(fault_addr);

	}

	return false;
}

unsigned
page_hash (const struct hash_elem *elem, void *aux UNUSED)
{
    const struct page_entry *p = hash_entry (elem, struct page_entry, elem);
    return hash_bytes (&p->user_vaddr, sizeof p->user_vaddr);
}
bool
page_comp_less (const struct hash_elem *a, const struct hash_elem *b, void *aux
UNUSED)
{
    const struct page_entry *pa = hash_entry (a, struct page_entry, elem);
    const struct page_entry *pb = hash_entry (b, struct page_entry, elem);
    return pa->user_vaddr < pb->user_vaddr;
}


struct page_entry*
page_create_file(struct file *file, off_t ofs, uint8_t *upage,uint32_t read_bytes, uint32_t zero_bytes, bool writable,enum palloc_flags flag)
{
	struct page_entry* page = malloc(sizeof(struct page_entry));
	page->user_vaddr = pg_round_down(upage);
	page->access_time = timer_ticks();
	page->loaded = false;
	page->accessed = writable;
	page->flag = PAGE_FILE;
	page->doc = file;
	page->read_bytes = read_bytes;
	page->zero_bytes = zero_bytes;
	page->offset = ofs;
	page->swap_entry = 0;
	page->frame_flag = flag;

  hash_insert(&thread_current()->spt_hash, &page->elem);

  return page;

}

bool 
page_load_file(struct file *file, off_t ofs, uint8_t *upage,uint32_t page_read_bytes, uint32_t page_zero_bytes, bool writable)
{
	struct page_entry *spte = page_create_file(file, ofs, upage,page_read_bytes, page_zero_bytes, writable, PAL_USER);
      //printf(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> 1\n");
      //set_supp_pte(&thread_current()->supp_table, spte);
      if (spte == NULL)
        return false;

      //printf(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> 2\n");
      struct frame_entry *fptr = frame_get_page(spte);
      
      if (fptr->frame == NULL)
        return false;

      /* Load this page. */
      //printf(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> 3\n");

      if (file_read (file, fptr->frame, page_read_bytes) != (int) page_read_bytes)
        {
          //printf(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> failed to read\n");
          frame_remove(fptr);
          return false; 
        }
         //printf(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> 4\n");

      memset (fptr->frame + page_read_bytes, 0, page_zero_bytes);
      return true;

}
struct page_entry* page_get (void *upage)
{
  struct page_entry page;
  page.user_vaddr = pg_round_down(upage);

  struct hash_elem *hold = hash_find(&thread_current()->spt_hash, &page.elem);
  if (!hold) { return NULL;}
  else return hash_entry (hold, struct page_entry, elem);
}

bool page_grow_stack(void * uva){

	struct page_entry* page = NULL;
    struct frame_entry* frame = NULL;
        page = page_create_file(NULL, 0, uva, 0, 0, true, PAL_USER);

        if (page != NULL)
        {
            frame_get_page(page);
            page->loaded = false;
            return true;
        }

    return false;
}

void page_print(struct page_entry* page)
{
	printf("page = %p\n", page);
	printf("page->user_vaddr = %p\n",page->user_vaddr);
	printf("page->access_time = %d\n", page->access_time);
	printf("page->accessed = %d\n", page->accessed);
	printf("age->flag = %d\n", page->flag);
	printf("page->doc = %p\n", page->doc);
	printf("page->read_bytes = %d\n", page->read_bytes);
	printf("page->zero_bytes = %d\n", page->zero_bytes);
	printf("page->offset = %jd\n", (intmax_t)page->offset);
	printf("page->swap_entry = %d\n", page->swap_entry);
	printf("page->frame_flag = %d\n", page->frame_flag);
}
