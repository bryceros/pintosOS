#include "vm/frame.h"
#include "userprog/process.h"
#include "threads/vaddr.h"
#include "vm/swap.h"

void 
frametable_init()
{
	list_init(&frametable);
	lock_init(&frametable_lock);
	//swap_init();
}

struct frame_entry* 
frame_get_page(struct page_entry* page)
{

	if ((page->frame_flag & PAL_USER) != 0)
	{
	//printf("??????????????????? in frame_get_page\n");

		void* frame = (void*)palloc_get_page(page->frame_flag);

		if(frame == NULL)
		{
			//printf("???????????????????frame_evict\n");
			frame_evict();
			frame = (void*)palloc_get_page(page->frame_flag);
			if(frame == NULL)
			{
				//printf("???????????????????frame_evict failed\n");
			}
		}
		if(install_page (page->user_vaddr, frame, page->accessed))
		{
			//printf("??????????????????? frame_get_page succus\n");

			return frame_create(frame,page);
		}

	}
	//printf("??????????????????? frame_get_page failed\n");

	return NULL;
}

struct frame_entry* 
frame_create(void *frame, struct page_entry *page)
{
  struct frame_entry *f = malloc(sizeof(struct frame_entry));
  f->frame = frame;
  f->owner = thread_current();
  f->aux = page;
  frame_insert (f);
  return f;
}
void 
frame_insert (struct frame_entry* frameptr)
{
  lock_acquire(&frametable_lock);
  list_push_back(&frametable, &frameptr->elem);
  lock_release(&frametable_lock);
}
void 
frame_remove (struct frame_entry* frameptr)
{
	lock_acquire(&frametable_lock);
	palloc_free_page(frameptr->frame);
	list_remove (&frameptr->elem);
	pagedir_clear_page(thread_current()->pagedir, frameptr->aux->user_vaddr);

	free(frameptr);

	lock_release(&frametable_lock);
}

void frame_evict(void)
{
	lock_acquire(&frametable_lock);
	struct frame_entry* frameptr = list_entry(list_max (&frametable, frame_comp_less_time , NULL), struct frame_entry, elem);
	while(frameptr->aux->loaded) frameptr = list_entry(list_max (&frametable, frame_comp_less_time , NULL), struct frame_entry, elem);
	lock_release(&frametable_lock);

	struct page_entry* page = frameptr->aux;
	void * frame = frameptr->frame;

	page->swap_entry = swap_write(frame);
	page->flag = PAGE_SWAP;

	frame_remove (frameptr);
}
bool frame_reclaim(struct page_entry *page)
{
	page->flag = PAGE_FILE;
	struct frame_entry* frameptr = frame_get_page(page);
	if(frameptr == NULL) return false;
	swap_read (frameptr->aux->swap_entry, frameptr->frame);
	page->loaded = false;
	return true;
}


bool 
frame_comp_less_time(const struct list_elem *a,const struct list_elem *b, void *aux UNUSED)
{
   if(list_entry(a, struct frame_entry, elem)->aux->access_time < list_entry(b, struct frame_entry, elem)->aux->access_time) return true;
  else return false;
}

void frame_print(struct frame_entry* frameptr)
{
	printf("frameptr = %p\n", frameptr);
	printf("frameptr->frame = %p\n", frameptr->frame);
	printf("frameptr->owner = %p\n", frameptr->owner);
	printf("frameptr->aux = %p\n", frameptr->aux);
}