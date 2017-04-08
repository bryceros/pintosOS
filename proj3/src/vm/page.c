#include "vm/page.h"
#include <stdlib.h>
#include "threads/thread.h"
#include "threads/loader.h"
#include "threads/vaddr.h"
#include "vm/frame.h"
#include "devices/timer.h"
//#include "userprog/process.h"


#define  PAGE_SIZE  (4096)

bool 
page_check(void *fault_addr, void* esp)
{
	//printf("////////////////////////////////////////////in page check\n");
	struct page_entry *page = page_get(fault_addr);
	if(page != NULL)
	{
		page->access_time = timer_ticks();

		switch(page->flag)
		{
		    case PAGE_FILE: 
		    //	printf("/////////////////////////////////FILE\n");
		    	return page_load_file(page);
		    case PAGE_SWAP: 
		    //	printf("////////////////////////////////SWAP\n");

		    	frame_reclaim(page);
		    	return true;
		    case PAGE_MMAP:  return false;
		    default: return false;
		}

	}
	else if(esp-32 <= fault_addr)
	{
		//printf("///////////////////GROW STACK\n");
		return page_growth_stack(fault_addr);

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
	page->dirty = false;
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
page_load_file(struct page_entry* page)
{
	//page_print(page);
	struct frame_entry* file_frame = frame_get_page(page);
	//frame_print(file_frame);
	//printf("////////////////////////////////////////////// 1\n");
	if (file_frame == NULL)
        return false;
	//printf("////////////////////////////////////////////// 2\n");

      /* Load this page. */ 
      if (file_read_at(page->doc, file_frame->frame, page->read_bytes, page->offset) != page->read_bytes)
      {
    //  	printf("////////////////////////////////////////////// failed to read\n");
         //return false; 
      }
    //   printf("////////////////////////////////////////////// 3\n");

      memset (file_frame->frame + page->read_bytes, 0, page->zero_bytes);
    //  printf("////////////////////////////////////////////// 4\n");
      return true;
      /* Add the page to the process's address space. */

}
struct page_entry* page_get (void *upage)
{
  struct page_entry page;
  page.user_vaddr = pg_round_down(upage);

  struct hash_elem *hold = hash_find(&thread_current()->spt_hash, &page.elem);

  if (!hold) return NULL;
  else return hash_entry (hold, struct page_entry, elem);
}

bool page_growth_stack(void * uva){

	struct page_entry* page = NULL;
    struct frame_entry* frame = NULL;

        page = page_create_file(NULL, 0, uva, 0, 0, true, PAL_USER);

        if (page != NULL)
        {
            frame_get_page(page);
            return true;
        }
    return false;
}

void page_print(struct page_entry* page)
{
	printf("page = %p\n", page);
	printf("page->user_vaddr = %p\n",page->user_vaddr);
	printf("page->access_time = %d\n", page->access_time);
	printf("page->dirty = %d\n", page->dirty);
	printf("page->accessed = %d\n", page->accessed);
	printf("age->flag = %d\n", page->flag);
	printf("page->doc = %p\n", page->doc);
	printf("page->read_bytes = %d\n", page->read_bytes);
	printf("page->zero_bytes = %d\n", page->zero_bytes);
	printf("page->offset = %jd\n", (intmax_t)page->offset);
	printf("page->swap_entry = %d\n", page->swap_entry);
	printf("page->frame_flag = %d\n", page->frame_flag);
}
