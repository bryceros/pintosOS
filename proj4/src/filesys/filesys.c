#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "threads/thread.h"

#define ASCII_SLASH 47

/* Partition that contains the file system. */
struct block *fs_device;

static void do_format (void);

/* Initializes the file system module.
   If FORMAT is true, reformats the file system. */
void
filesys_init (bool format) 
{
  fs_device = block_get_role (BLOCK_FILESYS);
  if (fs_device == NULL)
    PANIC ("No file system device found, can't initialize file system.");

  inode_init ();
  free_map_init ();

  if (format) 
    do_format ();

  free_map_open ();
}

/* Shuts down the file system module, writing any unwritten data
   to disk. */
void
filesys_done (void) 
{
  free_map_close ();
}

/* Creates a file named NAME with the given INITIAL_SIZE.
   Returns true if successful, false otherwise.
   Fails if a file named NAME already exists,
   or if internal memory allocation fails. */
bool
//filesys_create (const char *name, off_t initial_size, enum inode_flags type)
filesys_create (const char *name, off_t initial_size, bool type)

{

  block_sector_t inode_sector = 0;
  //struct dir *dir = dir_open_root ();
  bool full;
  struct dir *dir = dir_getdir(name,&full);
  if (full == true && dir != dir_open_root())
    return false;
//printf("0/ %d\n",inode_get_cnt(dir_get_inode(dir)));
  char *filename = filesys_get_filename(name);

  bool success = (dir != NULL
                  && free_map_allocate (1, &inode_sector)
                  && inode_create (inode_sector, initial_size,type)
                  && dir_add (dir, filename, inode_sector));
  if (!success && inode_sector != 0)
    free_map_release (inode_sector, 1);

//printf("1/ %d\n",inode_get_cnt(dir_get_inode(dir)));

//printf("filesys_create %d\n",inode_get_inumber(dir_get_inode(dir)));

dir_close (dir);
free(filename);
  return success;
}

/* Opens the file with the given NAME.
   Returns the new file if successful or a null pointer
   otherwise.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
struct file *
filesys_open (const char *name)
{
  //struct dir *dir = dir_open_root ();
  //printf("filesys_open\n");
bool full;
  struct dir *dir = dir_getdir(name,&full);  //printf("0* %d\n",inode_get_cnt(dir_get_inode(dir)));

  if (full == false)
    return NULL;
  
//printf("///////////////////////////////////////open file\n");
  /*char *filename = filesys_get_filename(name);

if(strcmp(filename,".") != 0){
  //printf("1*  %d\n",inode_get_cnt(dir_get_inode(dir)));
  dir_swap_parent(&dir);
  //printf("2*  %d\n",inode_get_cnt(dir_get_inode(dir)));

}

  struct inode *inode = NULL;


  if (dir != NULL)
    dir_lookup (dir, filename, &inode);
  if(strcmp(filename,".") == 0)
    inode = dir_get_inode(dir);

  //printf("3* %d\n",inode_get_cnt(dir_get_inode(dir)));
  dir_close (dir);

  //printf("3* node %d\n",inode_get_cnt(inode));

  //printf("filesys_open %d\n",inode_get_inumber(dir_get_inode(dir)));*/

  return file_open (dir_get_inode(dir));
}

/* Deletes the file named NAME.
   Returns true if successful, false on failure.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
bool
filesys_remove (const char *name) 
{
  //struct dir *dir = dir_open_root ();
bool full;
  struct dir *dir = dir_getdir(name,&full);  //printf("0? %d\n",inode_get_cnt(dir_get_inode(dir)));

if (full == false)
    return false;

  dir_swap_parent(&dir);
  //printf("1? %d\n",inode_get_cnt(dir_get_inode(dir)));
  char *filename = filesys_get_filename(name);

  bool success = dir != NULL && dir_remove (dir, filename);
  //printf("2? %d\n",inode_get_cnt(dir_get_inode(dir)));
  dir_close (dir); 

  return success;
}

/* Formats the file system. */
static void
do_format (void)
{
  printf ("Formatting file system...");
  free_map_create ();
  if (!dir_create (ROOT_DIR_SECTOR, 16))
    PANIC ("root directory creation failed");
  free_map_close ();
  printf ("done.\n");
}

char* 
filesys_get_filename(const char* dir_path)
{
  char file_name[strlen(dir_path) + 1];
  memcpy(file_name, dir_path, strlen(dir_path) + 1);
 char* token;
 char* save_ptr;
 char* filename = "";
 for (token = strtok_r (file_name, "/", &save_ptr); token != NULL; token = strtok_r (NULL, "/", &save_ptr))
  {
    filename = token;
  }
  char *file = malloc(strlen(filename) + 1);
  memcpy(file, filename, strlen(filename) + 1);
  return file;
}

