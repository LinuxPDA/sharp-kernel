/* collie_deviceinfo.c:	device specific informations
 * Copryright(R) SHARP 2001, All rights reserved.
 */
#include	"deviceinfo.h"
#include <asm/hardware.h>
#include <linux/string.h>

/*
 * Flash Memory mappings
 *
 */

#define FLASH_MEM_BASE_PARA4 0xe8fec000
#define FLASH_MEM_BASE_PARA5 0xe8ff0000
#define FLASH_MEM_BASE_PARA6 0xe8ff4000
#define FLASH_MEM_BASE_PARA7 0xe8ff8000
#define FLASH_MEM_BASE_PARA8 0xe8ffc000


#define FLASH_DATA_SHORT(adr)	(*(volatile unsigned short*)(adr))
#define FLASH_DATA_INT(adr)	(*(volatile unsigned int*)(adr))

// UUID
#define FLASH_UUID_MAJIC	FLASH_MAGIC_CHG('U','U','I','D')
#define	FLASH_UUID_MAGIC_ADR	0x08
#define	FLASH_UUID_DATA_ADR	0x0C

#define STRING_ID	0
#define COUNTRY		16
#define VERSION		18
#define CHECKSUM	20
#define MODEL		22


// COUNTRY
#define USA		0x0000
#define GER		0x0001
#define JPN		0x0002
#define ITA		0x0003
#define CHI		0x0004

// MODEL Data
#define SL5000D		0x0001
#define SL5500		0x0002

static char temp[40];
static int check = 0;
static unsigned short checksum = 0;


int check_flash(void)
{

  if ( !strncmp((char *)(FLASH_MEM_BASE_PARA6),"SHARP SL-series ",16) )
     return 1;
  else
     return 0;
}

int check_sum(void)
{
  unsigned short *rom_adr16;
  unsigned short csum = 0;
  const int count = 127*128*1024/2;
  int j;

  if ( !check ) {
    rom_adr16 = ((unsigned short *)0xe8000000);
    for(j=0;j<count;j++) {
      csum += *rom_adr16++;
    }
    checksum = csum;
    check = 1;
  }
  return check;
}


/* mandatory entries */
char *di_vender(void)		/* device vendor name */
{
  if ( check_flash )
	return "SHARP";
  else
        return "unknown";
}

char *di_product(void)		/* product name */
{
  if ( check_flash ) {
    switch (FLASH_DATA_SHORT(FLASH_MEM_BASE_PARA6+MODEL)) {
      case SL5000D:
	return "SL-5000D";
      case SL5500:
	return "SL-5500";
    }
  }
  return "unknown";
}

char *di_revision(void)		/* ROM version */
{
  unsigned int romver = FLASH_DATA_INT(FLASH_MEM_BASE_PARA6+VERSION);
  unsigned short cksum = FLASH_DATA_SHORT(FLASH_MEM_BASE_PARA6+CHECKSUM);

  memset(temp,0x00,20);
  if ( check_flash && check_sum() && ( checksum == cksum )) {
    sprintf(temp,"%x.%02x",(romver >> 8) & 0x00ff,(romver) & 0x00ff);
    return temp;
  }
  return "unknown";
}

/* optional entries */
char *di_locale(void)		/* locale */
{
  if ( check_flash ) {
    switch (FLASH_DATA_SHORT(FLASH_MEM_BASE_PARA6+COUNTRY)) {
      case USA:
	return "US";
      case GER:
	return "DE";
      case JPN:
	return "JP";
      case ITA:
	return "IT";
      case CHI:
	return "CN";
    }
  }
  return "unknown";
}

char *di_serial(void)		/* device individual id */
{
  memset(temp,0x00,20);

  if ( FLASH_DATA(FLASH_UUID_MAGIC_ADR) == FLASH_UUID_MAJIC ) {
    strncpy(temp,(char *)(FLASH_MEM_BASE + FLASH_UUID_DATA_ADR),16);
    return temp;
  }
  return "unknown";
}

char *di_checksum(void)		/* ROM checksum */
{
  unsigned short cksum = FLASH_DATA_SHORT(FLASH_MEM_BASE_PARA6+CHECKSUM);

  memset(temp,0x00,20);
  
  if ( check_flash && check_sum() && ( checksum == cksum )) {
    sprintf(temp,"%04x",cksum);
    return temp;
  } else
    return "Err.";
}

char *di_bootstr(void)		/* boot string */
{
  int i;

  memset(temp,0x00,40);
  if ( check_flash ) {
    strncpy(temp,(char *)(FLASH_MEM_BASE_PARA4),32);
    for(i=0;i<32;i++)
      if ( *(temp+i) == 0xff )  {  *(temp + i) = 0x00; break; }
    return temp;
  }
  return "unknown";
}
