#include "vm/swap.h"


static struct block* global_swap_block;
struct lock swap_lock;
struct bitmap *swap_bitmap;

//Get the block device when we initialize our swap code
void swap_init()
{
    global_swap_block = block_get_role(BLOCK_SWAP);
    swap_bitmap = bitmap_create(block_size(global_swap_block));
	bitmap_set_all (swap_bitmap, true);
    lock_init(&swap_lock);
}

size_t swap_write (void *frame)
{
	//printf("//////////////in swap_write\n");
	lock_acquire(&swap_lock);
	size_t index = bitmap_scan_and_flip (swap_bitmap, 0, 1, true);

	if(index == BITMAP_ERROR)
		    PANIC ("Swap disk full");

	size_t i;
	for(i = 0; i < 8; ++i)
	{
	    //each read/write will rea/write 512 bytes, therefore we need to read/
	    //write 8 times, each at 512 increments of the frame
	    block_write(global_swap_block, index + i, frame + (i * BLOCK_SECTOR_SIZE));
	}
	lock_release(&swap_lock);
	//printf("//////////////exit swap_write\n");
	return index;
}

void swap_read(size_t index, void* frame)
{
	//printf("//////////////in swap_read\n");
	lock_acquire(&swap_lock);
	bitmap_set(swap_bitmap,index,0);
	size_t i;
	for(i = 0; i < 8; ++i)
	{
	    //each read/write will rea/write 512 bytes, therefore we need to read/
	    //write 8 times, each at 512 increments of the frame
	    block_read(global_swap_block, index + i, frame + (i * BLOCK_SECTOR_SIZE));
	}
	lock_release(&swap_lock);
	//printf("//////////////exit swap_read\n");

}
