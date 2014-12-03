/*************************************************************************

Title:   	
Filename:   $Header:$
Hardware:   MX21
Summay:

License:    The contents of this file are subject to the Mozilla Public
            License Version 1.1 (the "License"); you may not use this file
            except in compliance with the License. You may obtain a copy of
            the License at http://www.mozilla.org/MPL/

Author:   
Company:    Motorola Suzhou Design Center

====================Change Log========================
$Log:$

********************************************************************************/

///@file 	mx2_nand.h
///@brief	head file of MX21 NAND Flash Control low level.
///
///@author	Frank Li(zhili@motorola.com)
///@version	$Version$

#ifndef MX_mx2_nand_INCLUDE_01072003
#define MX_mx2_nand_INCLUDE_01072003

//<<<<<Include

//>>>>>Include


//<<<<<Macro
#define ECC_EN 0x8
#define SP_EN  0x4

#define NAND_FLASH_CONFIG2_INT 0x8000
#define NAND_FLASH_CONFIG2_FCMD 0x1
#define NAND_FLASH_CONFIG2_FADD 0x2
#define NAND_FLASH_CONFIG2_FDI  0x4


#define ERR_TIME_OUT -1
#define ERR_VERIFY   -2

#define CONFIG2_SET_FDO_PAGE_OUT() _reg_NFC_NF_CONFIG2=0x8; 
#define CONFIG2_SET_FDO_ID() 	  _reg_NFC_NF_CONFIG2=0x10
#define CONFIG2_SET_FDO_STATUS()  _reg_NFC_NF_CONFIG2=0x20;

#define CONFIG1_SET_READ() do{ _reg_NFC_NF_CONFIG1|=0x2; _reg_NFC_NF_CONFIG1&=~0x1;}while(0)
#define CONFIG1_SET_WRITE() do{ _reg_NFC_NF_CONFIG1&=~0x2; _reg_NFC_NF_CONFIG1|=0x1;}while(0)


#ifdef _STANDALONE_
//#define WAIT(condition, timeout_usec)  do{ volatile int i=0; while(condition){ i++; if(i>timeout_usec*10) break;} }while(0)
//Frank Li for reduce space

#define WAIT(condition, timeout_usec) while(condition);

#else

#define WAIT(condition, timeout_usec) 					\
    do {								\
	unsigned long j;						\
	j = jiffies + (timeout_usec * HZ + 999999) / 1000000 + 1;	\
	while((condition) && jiffies < j)				\
	    ;								\
    } while(0)

#endif

#define NAND_FLASH_BLOCK_NUMBER             4096*64
#define NAND_FLASH_PAGE_PER_BLOCK  			32
#define NAND_FLASH_PAGE_MAIN_SIZE           512
#define	NAND_FLASH_PAGE_SPARE_SIZE			16			


//<<<<<Public Structure
#ifdef _STANDALONE_
typedef unsigned int u32;
typedef unsigned short u16;
typedef unsigned char u8;
#endif
//>>>>>Public Structure

//<<<<<Public Function Declearation

void mx2_nand_preset(void);
int mx2_nand_command(int command);
int mx2_nand_address(int addr);
void mx2_nand_read(unsigned char *buf, int start, int end, unsigned io);
void mx2_nand_write(char *databuf, char *oobbuf, struct mtd_info *mtd);
void mx2_nand_write_oob(char *oobbuf, struct mtd_info *mtd);

//>>>>>Public Function Declearation

static inline void mx2_nand_read_data(unsigned char *buf, int start, int end)
{
    mx2_nand_read(buf, start, end, NFC_MAB3_BASE);
}

static inline void mx2_nand_read_oob(unsigned char *buf, int start, int end)
{
    mx2_nand_read(buf, start, end, NFC_SAB3_BASE);
}

//<<<<<Public Gobal Variable Declearation

//>>>>>Public Gobal Variable Declearation

#endif
