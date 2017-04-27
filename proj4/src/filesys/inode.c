#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"

#include "stdio.h"

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44
#define BLOCK_DIRECT 10
#define BLOCK_INDIRECT 1
#define BLOCK_DOUBLE 1
#define BLOCK_NUMBER 128

/* On-disk inode.
   Must be exactly BLOCK_SECTOR_SIZE bytes long. */
struct inode_disk
  {
    // this has to add up to 512
    block_sector_t direct[BLOCK_DIRECT];          /* First data sector. */
    block_sector_t indirect[BLOCK_INDIRECT];           /* First data sector. */
    block_sector_t double_indirect[BLOCK_DOUBLE];
    off_t length;
    off_t EOF;
    bool type;
    block_sector_t parent;                     
    unsigned magic;                    /* Magic number. */
    uint32_t unused[(512-sizeof(unsigned)-sizeof(block_sector_t)-sizeof(bool)-sizeof(off_t)-sizeof(off_t)-BLOCK_DIRECT*sizeof(block_sector_t)-BLOCK_INDIRECT*sizeof(block_sector_t)-BLOCK_DOUBLE*sizeof(block_sector_t))/sizeof(uint32_t)];               /* Not used. */
  };

/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
static inline size_t
bytes_to_sectors (off_t size)
{
  return DIV_ROUND_UP (size, BLOCK_SECTOR_SIZE);
}

/* In-memory inode. */
struct inode 
  {
    struct list_elem elem;              /* Element in inode list. */
    block_sector_t sector;              /* Sector number of disk location. */
    int open_cnt;                       /* Number of openers. */
    bool removed;                       /* True if deleted, false otherwise. */
    int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */
    //enum inode_flags flag;
    struct inode_disk data;             /* Inode content. */
  };

  void sectors_to_index(size_t,size_t*,size_t*,size_t*);
  bool inode_expand(struct inode_disk*, off_t);


/* Returns the block device sector that contains byte offset POS
   within INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
/*static block_sector_t
byte_to_sector (const struct inode *inode, off_t pos) 
{
  ASSERT (inode != NULL);
  if (pos < inode->data.length)
    return inode->data.direct[0] + pos / BLOCK_SECTOR_SIZE;
  else
    return -1;
}*/
static block_sector_t
byte_to_sector (const struct inode *inode, off_t pos) 
{
  //printf("BYTE TO SECTOR: length %u pos %u\n", inode->data.length, pos);
  size_t Double_Index = 0;
  size_t Indirect_Index = 0;
  size_t Direct_Index = 0;

  ASSERT (inode != NULL);
//printf("pos: %jd inode->data.EOF: %jd\n",(intmax_t)pos,(intmax_t)inode->data.EOF);
  if(pos <= inode->data.EOF)
  {
    size_t sectors = pos/BLOCK_SECTOR_SIZE;
    sectors_to_index(sectors,&Double_Index,&Indirect_Index,&Direct_Index);
//printf("Double_Index: %zu Indirect_Index: %zu Direct_Index: %zu sectors: %zu \n",Double_Index,Indirect_Index,Direct_Index,sectors);


    if (sectors >= BLOCK_INDIRECT*BLOCK_NUMBER+ BLOCK_DIRECT)
    {
//printf("0\n");
      block_sector_t indirect_blocks[BLOCK_NUMBER];
      block_read(fs_device, inode->data.double_indirect[Double_Index], &indirect_blocks);
      
      block_sector_t direct_blocks[BLOCK_NUMBER];
      block_read(fs_device,indirect_blocks[Indirect_Index], &direct_blocks);
//printf("1\n");
      return direct_blocks[Direct_Index];  
    }
    else if( sectors >= BLOCK_DIRECT)
    {
//printf("2\n");

      block_sector_t direct_blocks[BLOCK_NUMBER];
      block_read(fs_device, inode->data.indirect[Indirect_Index], &direct_blocks);
//printf("3\n");     
      return direct_blocks[Direct_Index];
    }
    else
    {
//printf("4\n");
      return inode->data.direct[Direct_Index];
    }

  }
  else
//printf("failed\n");
    return -1; 
}
/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes;

/* Initializes the inode module. */
void
inode_init (void) 
{
  list_init (&open_inodes);
}

/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   device.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
/*bool
inode_create (block_sector_t sector, off_t length)
{
  struct inode_disk *disk_inode = NULL;
  bool success = false;

  ASSERT (length >= 0);

  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. *?/
  ASSERT (sizeof *disk_inode == BLOCK_SECTOR_SIZE);

  disk_inode = calloc (1, sizeof *disk_inode);
  if (disk_inode != NULL)
    {
      size_t sectors = bytes_to_sectors (length);
      disk_inode->length = length;
      disk_inode->magic = INODE_MAGIC;
      if (free_map_allocate (sectors, &disk_inode->direct[0])) 
        {
          block_write (fs_device, sector, disk_inode);
          if (sectors > 0) 
            {
              static char zeros[BLOCK_SECTOR_SIZE];
              size_t i;
              
              for (i = 0; i < sectors; i++) 
                block_write (fs_device, disk_inode->direct[0] + i, zeros);
            }
          success = true; 
        } 
      free (disk_inode);
    }
  return success;
}*/

bool
//inode_create (block_sector_t sector, off_t length, enum inode_flags type)
inode_create (block_sector_t sector, off_t length,bool type)
{
  struct inode_disk *disk_inode = NULL;
  bool success = false;

  ASSERT (length >= 0);

  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
  ASSERT (sizeof *disk_inode == BLOCK_SECTOR_SIZE);

  disk_inode = calloc (1, sizeof *disk_inode);
  if (disk_inode != NULL)
    {
      disk_inode->length = length;
      disk_inode->EOF = 0;
      disk_inode->magic = INODE_MAGIC;
      disk_inode->type = type;
      disk_inode->parent = ROOT_DIR_SECTOR;
      if (inode_expand (disk_inode, length)) 
        {
          block_write (fs_device, sector, disk_inode);
          success = true; 
        } 
      free (disk_inode);
    }
  return success;
}

void 
sectors_to_index(size_t cur_sectors,size_t* Double_Index,size_t* Indirect_Index,size_t* Direct_Index)
{
  if(cur_sectors >= BLOCK_INDIRECT*BLOCK_NUMBER + BLOCK_DIRECT)
    {
      *Double_Index = (cur_sectors - BLOCK_INDIRECT*BLOCK_NUMBER - BLOCK_DIRECT)/ (BLOCK_NUMBER*BLOCK_NUMBER);
      *Indirect_Index =(cur_sectors - ((*Double_Index)*BLOCK_INDIRECT*BLOCK_NUMBER + BLOCK_INDIRECT*BLOCK_NUMBER + BLOCK_DIRECT))/BLOCK_NUMBER;
      *Direct_Index = cur_sectors - ((*Double_Index)*BLOCK_INDIRECT*BLOCK_DIRECT + BLOCK_INDIRECT*BLOCK_NUMBER+ (*Indirect_Index)*BLOCK_NUMBER + BLOCK_DIRECT);
    }
   else if( cur_sectors >= BLOCK_DIRECT)
    {
      *Indirect_Index =(cur_sectors - BLOCK_DIRECT)/BLOCK_NUMBER;
      *Direct_Index = cur_sectors - ((*Indirect_Index)*BLOCK_NUMBER + BLOCK_DIRECT);
    }
  else
    {
      *Direct_Index = cur_sectors;
    }
}

bool 
inode_expand(struct inode_disk* node, off_t length)
{  

  size_t cur_sectors = bytes_to_sectors (node->EOF);
  size_t Double_Index = 0;
  size_t Indirect_Index = 0;
  size_t Direct_Index = 0;

  size_t sectors = bytes_to_sectors (length) - bytes_to_sectors (node->EOF);
  static char zeros[BLOCK_NUMBER];
    //printf("sectors: %d\n", sectors);

  while(sectors > 0)
  {
    sectors_to_index(cur_sectors,&Double_Index,&Indirect_Index,&Direct_Index);

//printf("cur_sectors: %zu Double_Index: %zu Indirect_Index: %zu Direct_Index: %zu sectors: %zu node->EOF: %jd \n",cur_sectors,Double_Index,Indirect_Index,Direct_Index,sectors,(intmax_t)node->EOF);
    if (cur_sectors >= BLOCK_INDIRECT*BLOCK_NUMBER + BLOCK_DIRECT)
    {
     //printf("In DOUBLE\n");
      block_sector_t indirect_blocks[BLOCK_NUMBER];
      if(Indirect_Index == 0 && Direct_Index == 0)
          free_map_allocate(1, &node->double_indirect[Double_Index]);
      else
          block_read(fs_device, node->double_indirect[Double_Index], &indirect_blocks);
      if (Indirect_Index < BLOCK_NUMBER)
      {
        block_sector_t direct_blocks[BLOCK_NUMBER];
        if(Direct_Index == 0)
            free_map_allocate(1, &indirect_blocks[Indirect_Index]);
        else
          block_read(fs_device,indirect_blocks[Indirect_Index], &direct_blocks);
        if (Direct_Index < BLOCK_NUMBER)
        {
          free_map_allocate(1, &direct_blocks[Direct_Index]);
          block_write (fs_device, direct_blocks[Direct_Index], zeros);
          cur_sectors++;
          node->EOF += BLOCK_SECTOR_SIZE;
          sectors--;
        }
        block_write(fs_device, indirect_blocks[Indirect_Index], &direct_blocks);
      }
      block_write(fs_device, node->double_indirect[Double_Index], &indirect_blocks);
    }
    else if( cur_sectors >= BLOCK_DIRECT)
    {
      //printf("In INDIRECT\n");
      block_sector_t direct_blocks[BLOCK_NUMBER];
      if(Direct_Index == 0)
          free_map_allocate(1, &node->indirect[Indirect_Index]);
      else
        block_read(fs_device, node->indirect[Indirect_Index], &direct_blocks);
      if (Direct_Index < BLOCK_NUMBER)
      {
        free_map_allocate(1, &direct_blocks[Direct_Index]);
        block_write (fs_device, direct_blocks[Direct_Index], zeros);
        cur_sectors++;
        node->EOF += BLOCK_SECTOR_SIZE;
        sectors--;
      }
      block_write(fs_device, node->indirect[Indirect_Index], &direct_blocks);
    }
    else
    {
      //printf("In DIRECT\n");
      free_map_allocate(1, &node->direct[Direct_Index]);
      block_write (fs_device, node->direct[Direct_Index], zeros);
      cur_sectors++;
      node->EOF += BLOCK_SECTOR_SIZE;
      sectors--;

    }
  }
  return true;
}

/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
struct inode *
inode_open (block_sector_t sector)
{
  struct list_elem *e;
  struct inode *inode;

  /* Check whether this inode is already open. */
  for (e = list_begin (&open_inodes); e != list_end (&open_inodes);
       e = list_next (e)) 
    {
      inode = list_entry (e, struct inode, elem);
      if (inode->sector == sector) 
        {
          inode_reopen (inode);
          return inode; 
        }
    }

  /* Allocate memory. */
  inode = malloc (sizeof *inode);
  if (inode == NULL)
    return NULL;

  /* Initialize. */
  list_push_front (&open_inodes, &inode->elem);
  inode->sector = sector;
  inode->open_cnt = 1;
  inode->deny_write_cnt = 0;
  inode->removed = false;
  block_read (fs_device, inode->sector, &inode->data);
  return inode;
}

/* Reopens and returns INODE. */
struct inode *
inode_reopen (struct inode *inode)
{
  if (inode != NULL)
    inode->open_cnt++;
  return inode;
}

/* Returns INODE's inode number. */
block_sector_t
inode_get_inumber (const struct inode *inode)
{
  return inode->sector;
}

/* Closes INODE and writes it to disk.
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks. */
void
inode_close (struct inode *inode) 
{
  /* Ignore null pointer. */
  if (inode == NULL)
    return;

  /* Release resources if this was the last opener. */
  if (--inode->open_cnt == 0)
    {
      /* Remove from inode list and release lock. */
      list_remove (&inode->elem);
 
      /* Deallocate blocks if removed. */
      if (inode->removed) 
        {
          free_map_release (inode->sector, 1);
          free_map_release (inode->data.direct[0],
                            bytes_to_sectors (inode->data.length)); 
        }

      free (inode); 
    }
}

/* Marks INODE to be deleted when it is closed by the last caller who
   has it open. */
void
inode_remove (struct inode *inode) 
{
  ASSERT (inode != NULL);
  inode->removed = true;
}

/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
off_t
inode_read_at (struct inode *inode, void *buffer_, off_t size, off_t offset) 
{
  uint8_t *buffer = buffer_;
  off_t bytes_read = 0;
  uint8_t *bounce = NULL;

    if (size > inode->data.length)
      return 0;

  while (size > 0) 
    {
      /* Disk sector to read, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually copy out of this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
          /* Read full sector directly into caller's buffer. */
          block_read (fs_device, sector_idx, buffer + bytes_read);
        }
      else 
        {
          /* Read sector into bounce buffer, then partially copy
             into caller's buffer. */
          if (bounce == NULL) 
            {
              bounce = malloc (BLOCK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }
          block_read (fs_device, sector_idx, bounce);
          memcpy (buffer + bytes_read, bounce + sector_ofs, chunk_size);
        }
      
      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_read += chunk_size;
    }
  free (bounce);

  return bytes_read;
}

/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
   Returns the number of bytes actually written, which may be
   less than SIZE if end of file is reached or an error occurs.
   (Normally a write at end of file would extend the inode, but
   growth is not yet implemented.) */
off_t
inode_write_at (struct inode *inode, const void *buffer_, off_t size,
                off_t offset) 
{
  const uint8_t *buffer = buffer_;
  off_t bytes_written = 0;
  //printf("BEFORE WRITE: %u\n", inode_length(inode));
  uint8_t *bounce = NULL;
  if (inode->deny_write_cnt)
    return 0;

  if(size+offset > inode->data.EOF)
    inode_expand(&inode->data, size+offset);

  if (size+offset > inode->data.length)
    inode->data.length = size+offset;



  while (size > 0) 
    {
      /* Sector to write, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      //printf("INODE_WRITE_AT: bytes_to_sector returned: %u\n", sector_idx);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually write into this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
          /* Write full sector directly to disk. */
          block_write (fs_device, sector_idx, buffer + bytes_written);
        }
      else 
        {
          /* We need a bounce buffer. */
          if (bounce == NULL) 
            {
              bounce = malloc (BLOCK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }

          /* If the sector contains data before or after the chunk
             we're writing, then we need to read in the sector
             first.  Otherwise we start with a sector of all zeros. */
          if (sector_ofs > 0 || chunk_size < sector_left) 
            block_read (fs_device, sector_idx, bounce);
          else
            memset (bounce, 0, BLOCK_SECTOR_SIZE);
          memcpy (bounce + sector_ofs, buffer + bytes_written, chunk_size);
          block_write (fs_device, sector_idx, bounce);
        }

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_written += chunk_size;
    }
  free (bounce);

  //printf("AFTER WRITE: %u\n", inode_length(inode));
  return bytes_written;
}
/*off_t
inode_write_at (struct inode *inode, const void *buffer_, off_t size,
                off_t offset) 
{
  const uint8_t *buffer = buffer_;
  off_t bytes_written = 0;
  uint8_t *bounce = NULL;

  size_t Double_Index = 0;
  size_t Indirect_Index = 0;
  size_t Direct_Index = 0;

  if (inode->deny_write_cnt)
    return 0;

  if(inode->data.length <= offset+size)
    inode_expand(&inode->data,inode->data.length + offset + size);

  while (size > 0) 
    {
      size_t sector_idx = bytes_to_sectors (offset);
      size_t min_lenght = (size < BLOCK_SECTOR_SIZE)? size : BLOCK_SECTOR_SIZE;
      sectors_to_index(sector_idx,&Double_Index,&Indirect_Index,&Direct_Index);

      if(sector_idx >= BLOCK_INDIRECT*BLOCK_DIRECT + BLOCK_DIRECT)
      {

      }
      else if( sector_idx >= BLOCK_DIRECT)
      {
        
      }
      else
      {
        block_write (fs_device, inode->data.direct[Direct_Index], buffer + offset);
        size -= min_lenght;
        offset += min_lenght;
        bytes_written += min_lenght;

      }
    }
  return bytes_written;
}*/
/* Disables writes to INODE.
   May be called at most once per inode opener. */
void
inode_deny_write (struct inode *inode) 
{
  inode->deny_write_cnt++;
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
}

/* Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
void
inode_allow_write (struct inode *inode) 
{
  ASSERT (inode->deny_write_cnt > 0);
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
  inode->deny_write_cnt--;
}

/* Returns the length, in bytes, of INODE's data. */
off_t
inode_length (const struct inode *inode)
{
  return inode->data.length;
}
void
inode_set_parent(struct inode * child,struct inode * parent)
{
   child->data.parent = parent->sector;
}

struct inode *
inode_get_parent(struct inode * inode)
{
  return inode_open(inode->data.parent);
}

bool 
inode_get_type(struct inode *inode)
{
  return inode->data.type;
}
