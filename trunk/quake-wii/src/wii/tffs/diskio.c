/****************************************************************************
* diskio
*
* Disk I/O Layer between elm-chan.org excellent TinyFATFS and libOGC
* card_io layer.
*
* softdev 2007
****************************************************************************/
#include "diskio.h"
#include <gccore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sdcard/wiisd_io.h>

/* Not so public exports from card_io.c */
#define MAX_DRIVE 2
#define SECTOR_SIZE 512
extern u8 g_CID[MAX_DRIVE][16];
extern u8 g_CSD[MAX_DRIVE][16];
extern u8 g_CardStatus[MAX_DRIVE][64];
extern s32 card_initIO(s32 drv_no);
extern s32 card_readSector(s32 drv_no,u32 sector_no,u8 *buf,u32 len);
extern s32 card_writeSector(s32 drv_no,u32 sector_no,const void *buf,u32 len);
extern s32 card_readStatus(s32 drv_no);

/* End of not so public exports */

int sd_atexited = 0;

/****************************************************************************
* disk_initialize
*
* TinyFATFS only supports 2 drives. Ideal for GameCube!
****************************************************************************/
void disk_deinitialize (void)
{
    if (sd_atexited)
    {
	  sdio_Shutdown();
      sd_atexited = 0;
    }
}

/****************************************************************************
* disk_initialize
*
* TinyFATFS only supports 2 drives. Ideal for GameCube!
****************************************************************************/
DSTATUS disk_initialize (BYTE drv)
{
  if ( drv != 0 && drv != 1 )
    return RES_PARERR;		/* Must be 0 or 1 */

  if (sdio_Startup())
  {
    if (!sd_atexited)
    {
      atexit(disk_deinitialize);
      sd_atexited = 1;
    }
    return RES_OK;			/* Card is ready to rumble! */
  }

  return RES_NOTRDY;			/* Something is amiss Watson! */
}

/****************************************************************************
* disk_status
****************************************************************************/
DSTATUS disk_status ( BYTE drv )
{
  if ( drv != 0 && drv != 1 )
    return RES_PARERR;		/* Must be 0 or 1 */

/* TODO: why does it fail? do some research
  if ( sdio_IsInserted() )
    return RES_OK;

  return RES_NOTRDY;*/
return RES_OK;
}

/****************************************************************************
* disk_read
****************************************************************************/
DRESULT disk_read (
  BYTE drv,		/* Physical drive nmuber (0) */
  BYTE *buff,		/* Data buffer to store read data */
  DWORD sector,		/* Sector number (LBA) */
  BYTE count		/* Sector count (1..255) */
)
{
  if ( drv != 0 && drv != 1 )
    return RES_PARERR;		/* Must be 0 or 1 */

  if ( sdio_ReadSectors(sector, count, buff) )
    return RES_OK;

  return RES_NOTRDY;
}

/****************************************************************************
* disk_write
****************************************************************************/
DRESULT disk_write (
  BYTE drv,			/* Physical drive nmuber (0) */
  const BYTE *buff,	/* Data to be written */
  DWORD sector,		/* Sector number (LBA) */
  BYTE count			/* Sector count (1..255) */
)
{
  if ( drv != 0 && drv != 1 )
    return RES_PARERR;		/* Must be 0 or 1 */

  if ( sdio_WriteSectors(sector, count, buff) )
    return RES_OK;

  return RES_NOTRDY;
}

/****************************************************************************
* disk_ioctl
****************************************************************************/
DRESULT disk_ioctl (
  BYTE drv,		/* Physical drive nmuber */
  BYTE ctrl,		/* Control code */
  void *buff		/* Buffer to send/receive data block */
)
{

  if ( drv != 0 && drv != 1 )
    return RES_PARERR;		/* Must be 0 or 1 */

  /* TinyFAT FS Only requires CTRL_SYNC */
  if ( ctrl != CTRL_SYNC )
    return RES_PARERR;

  /* Ensure the card is OK */
  return disk_status(drv);
}

/****************************************************************************
* get_fattime
*
* bit31:25
*    Year from 1980 (0..127)
* bit24:21
*    Month (1..12)
* bit20:16
*    Date (1..31)
* bit15:11
*    Hour (0..23)
* bit10:5
*    Minute (0..59)
* bit4:0
*    Second/2 (0..29) 
*
* I confess this is a mess - but it works!
****************************************************************************/
u32 get_fattime( void )
{
  time_t rtc;
  char realtime[128];
  int year, month, day, hour, minute, second;
  char mth[4];
  char *p;
  char *months = "JanFebMarAprMayJunJulAugSepOctNovDec";
  u32 ret = 0;

  rtc = time(NULL);
  strcpy(realtime, ctime(&rtc));

  /* ctime returns dates formatted to ASCII thus,
     DDD MMM 000 HH:MM:SS CCYY
   */
  year = atoi( realtime + 20 );
  day = atoi( realtime + 7 );
  hour = atoi( realtime + 11 );
  minute = atoi( realtime + 14 );
  second = atoi( realtime + 17 );

  memcpy(mth, realtime + 4, 3);
  mth[3] = 0;
  p = strstr(months, mth);
  if ( !p )
    month = 1;
  else
    month = (( p - months ) / 3 ) + 1;

  /* Convert to DOS time */
  /* YYYYYYY MMMM DDDDD HHHHH MMMMMM SSSSS */
  /* 1098765 4321 09876 54321 098765 43210 */

  year = year - 1980;
  ret = ( year & 0x7f ) << 25;
  ret |= ( month << 21 );
  ret |= ( day << 16 );
  ret |= ( hour << 11 );
  ret |= ( minute << 5 );
  ret |= ( second & 0x1f );

  return ret;

}

