/****************************************************************************
* Generic File I/O for VisualBoyAdvance
*
* Currently only supports SD
****************************************************************************/
#ifndef __SDFILEIO__
#define __SDFILEIO__
#include "../tffs/tff.h"

#define MAXDIRENTRIES 1000

extern "C"
  {
    /* Required Functions */
    FIL *gen_fopen( const char *filename, const char *mode );
    int gen_fwrite( const void *buffer, int len, int block, FIL *f );
    int gen_fread( void *buffer, int len, int block, FIL *f );
    void gen_fclose( FIL *f );
    int gen_fseek(FIL *f, int where, int whence);
    int gen_fgetc( FIL *f );
    int SDInit( void );
    int gen_getdir( char *thisdir );
    extern char *direntries[MAXDIRENTRIES];
  }

#endif

