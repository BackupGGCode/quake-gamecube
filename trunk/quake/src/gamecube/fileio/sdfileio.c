/****************************************************************************
* Generic File I/O for VisualBoyAdvance
*
* Currently only supports SD
****************************************************************************/
#include "../tffs/tff.h"
#include <gccore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../tffs/diskio.h"

static FATFS fatfs;

#define MAXDIRENTRIES 1000
char *direntries[MAXDIRENTRIES];

/**
 * SDInit
 */
void SDInit( void )
{
  SDCARD_Init();
  memset(&direntries, 0, 4 * MAXDIRENTRIES);
  f_mount(0, &fatfs);
}

/**
 * SD Card f_open
 */
FIL *gen_fopen( const char *filename, const char *mode )
{
  int flags = 0;
  int res;
  FIL *f;

  if ( stricmp(mode, "rb") == 0 )
    flags |= FA_READ;

  if ( stricmp(mode, "wb") == 0 )
    flags |= (FA_WRITE | FA_CREATE_ALWAYS);

  if ( flags == 0 )
    return NULL;

  f = malloc( sizeof(FIL) );
  res = f_open(f, filename, flags);
  if ( res != FR_OK )
    {
      free(f);
      return NULL;
    }

  return f;
}

/**
 * SD Card f_write
 */
int gen_fwrite( const void *buffer, int len, int block, FIL *f )
{
  int actuallen = len * block;
  int res;
  int blocks;
  unsigned short wb;
  int i;
  int bytesdone = 0;

  if ( !actuallen )
    return 0;

  blocks = actuallen >> 15;
  for( i = 0; i < blocks; i++ )
    {
      res = f_write(f, buffer + bytesdone, 32768, &wb);
      if ( res || ( wb != 32768 ) )
        return bytesdone;

      bytesdone += 32768;
    }

  blocks = actuallen & 0x7FFF;
  if ( blocks )
    {
      res = f_write(f, buffer + bytesdone, blocks, &wb);
      if ( res || ( wb != blocks ) )
        return bytesdone;

      bytesdone += blocks;
    }

  return bytesdone;
}

/**
 * SD Card f_read
 */
int gen_fread( void *buffer, int len, int block, FIL *f )
{
  int actuallen = len * block;
  int bytesdone = 0;
  unsigned short rb;
  int i, blocks;
  int res;

  if ( !actuallen )
    return 0;

  blocks = actuallen >> 15;
  for( i = 0; i < blocks; i++ )
    {
      res = f_read(f, buffer + bytesdone, 32768, &rb);
      if ( res || ( rb != 32768 ) )
        return bytesdone;

      bytesdone += 32768;
    }

  blocks = actuallen & 0x7FFF;
  if ( blocks )
    {
      res = f_read(f, buffer + bytesdone, blocks, &rb);
      if ( res || ( rb != blocks ) )
        return bytesdone;

      bytesdone += blocks;
    }

  return bytesdone;
}

/**
 * SD Card fclose
 */
void gen_fclose( FIL *f )
{
  int res;

  res = f_close(f);
  free(f);
}

/**
 * SD Card fseek
 *
 * NB: Only supports SEEK_SET
 */
int gen_fseek(FIL *f, int where, int whence)
{
  int res = 0;
  char b;
  unsigned short rb;
  int i;

  if ( whence == SEEK_SET )
    res = f_lseek(f, where);
  else
    {
      if ( whence == SEEK_CUR )
        {
          for( i = 0; i < where; i++ )
            res = f_read(f, &b, 1, &rb);
        }
    }

  return ( res == FR_OK );
}

/**
 * Simple fgetc
 */
int gen_fgetc( FIL *f )
{
  int res;
  unsigned char c;
  unsigned short rb;

  res = f_read(f, &c, 1, &rb);
  if ( res || ( rb != 1 ) )
    return EOF;

  return c;
}

/**
 * SortListing
 */
void SortListing( int max )
{
  int top,seek;
  char *t;

  for( top = 0; top < max - 1; top++ )
    {
      for( seek = top + 1; seek < max; seek++ )
        {
          if ( stricmp(direntries[top], direntries[seek]) > 0 )
            {
              t = direntries[top];
              direntries[top] = direntries[seek];
              direntries[seek] = t;
            }
        }
    }
}

/**
 * Get directory listing
 */
int gen_getdir( char *thisdir )
{
  int res;
  DIR dirs;
  FILINFO finfo;
  int count = 0;
  int i;

  /** Dispose of any existing entries **/
  i = 0;
  while ( direntries[i] != NULL )
    free(direntries[i]);

  res = f_opendir(&dirs, thisdir);
  if ( res == FR_OK )
    {
      while ( ( f_readdir(&dirs, &finfo) == FR_OK ) && ( finfo.fname[0] ) )
        {
          /* Skip any sub directories */
          if ( !(finfo.fattrib & AM_DIR) )
            {
              direntries[count++] = strdup(finfo.fname);
              if ( count == MAXDIRENTRIES ) break;
            }
        }
    }

  if ( count > 1 )
    SortListing(count);

  return count;
}

