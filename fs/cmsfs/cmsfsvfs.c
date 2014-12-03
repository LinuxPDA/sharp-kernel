/*
 *
 *        Name: cmsfsvfs.c (C program source)
 *              CMS FS VFS interface routines (fs driver code)
 *        Date: 2000-Sep-14 (Thu)
 *
 *              This source contains the routines and structures
 *              required when CMS FS is built as a driver.
 *              It interfaces between CMS FS' own structures,
 *              which read the CMS EDF format,  and Linux VFS.
 *
 *	2001/03/09	Alan Cox <alan@redhat.com>
 *		Reformat into kernel style
 *		Remove static buffers
 *		Merge code together
 *		Throw out userspace tool support
 *		Switch to pure Linux errno codes
 *
 *		So if it stopped working - blame me first
 */

#include        <linux/module.h>
#include        <linux/version.h>
#include        <linux/fs.h>
#include        <linux/slab.h>
#include        <linux/mm.h>
#include        <linux/locks.h>
#include	<linux/init.h>
#include	<linux/blkdev.h>

#include        <asm/uaccess.h>
 
#include        "cmsfs.h"
#include        "cmsfsext.h"
#include 	"aecs.h"

#define CMSFS_HOST_ASCII

/* ----------------------------------------------------------------- ÆCS 
 * ASCII to EBCDIC and vice-versa code conversion routines. 
 * Tables included here are based on ASCII conforming to the ISO8859-1 
 * Latin 1 character set and EBCDIC conforming to the IBM Code Page 37 
 * Latin 1 character set (except for three pairs of characters in 037). 
 */

#ifdef CMSFS_HOST_ASCII

/* ------------------------------------------------------------- CHRATOE 
 * Translate an ASCII character into EBCDIC. 
 */

static inline int chratoe(unsigned int c)
{
	c &= 0xFF;
	return (ebc8859[c]);
}

/* ------------------------------------------------------------- STRATOE 
 * Translate an ASCII string into EBCDIC in place. Return length. 
 */

static void stratoe(unsigned char *string)
{
	int i;

	for (i = 0; string[i] != 0x00; i++)
		string[i] = ebc8859[(int) string[i]];
	string[i] = ebc8859[(int) string[i]];
}

/* ------------------------------------------------------------- CHRETOA 
 * Translate an EBCDIC character into ASCII. 
 */

static inline int chretoa(unsigned int c)
{
	c &= 0xFF;
	return asc8859[c];
}

/* ------------------------------------------------------------- STRETOA 
 * Translate an EBCDIC string into ASCII in place. Return length. 
 */

static void stretoa(unsigned char *s)
{
	int i;

	for (i = 0; s[i] != 0x00; i++)
		s[i] = asc8859[(int) s[i]];
	s[i] = asc8859[(int) s[i]];
}

#else

static inline void stretoa(unsigned char *s) {;}
static inline void stratoe(unsigned char *s) {;}
#define chretoa(x)	(x)
#define chratoe(x)	(x)

#endif
 
/* --------------------------------------------------------- CMSFS_BREAD
 *  The "b" here means "block", not "buffer".  Do not confuse this with
 *  Linux VFS bread() function.   This is CMS FS "block read" function.
 */

static int cmsfs_bread(struct cms_super *vol,void *buf,int block,int blocksize)
{
	struct buffer_head * bh;
	struct super_block * sb;
 
	/*  for the moment,  we only deal with physical blocks  */
	if (blocksize != vol->pbksz)
		BUG();

	/*  We could maybe handle that case by breaking-up this call into
	     multiple bread() calls, but that'll be a later driver rev.  */
 
	sb = (struct super_block *) vol->vfssuper;
 
	/*  Call the system-level bread()  */
	bh = bread(sb->s_dev,block,blocksize);
	if (bh == NULL)
	{
        	printk(KERN_WARNING "cmsfs_bread(): system bread() failed.\n");
        	return -1;
	}
 
	/*  copy the data part,  then release the VFS buffer  */
	(void) memmove(buf,bh->b_data,blocksize);
	(void) brelse(bh);
 
	return blocksize;
}
 

/* ------------------------------------------------------------ CMSFSBEX
 *  convert a big-endian integer into a local integer
 *  the slow (reliable) way
 */

static int cmsfsbex(char *ivalue, int l)
{
	int i, ovalue;
	ovalue = ivalue[0];
	for (i = 1; i < l; i++) {
		ovalue = ovalue << 8;
		ovalue += ivalue[i] & 0xFF;
	}
	return ovalue;
}

/* ------------------------------------------------------------ CMSFSX2I
 *  CMS FS heXadecimal-to-Integer function
 *  The EDF filesystem uses several "packed decimal" fields.
 *  (Actually a visual approximation of packed decimal format
 *  lacking the sign nibble.)   This is for converting them to int.
 */

static int cmsfsx2i(char *ivalue, int l)
{
	int i, ovalue;
	ovalue = (ivalue[0] & 0x0F) + ((ivalue[0] >> 4) & 0x0F) * 10;
	for (i = 1; i < l; i++) {
		ovalue = ovalue << 8;
		ovalue +=
		    (ivalue[i] & 0x0F) + ((ivalue[i] >> 4) & 0x0F) * 10;
	}
	return ovalue;
}

/* ---------------------------------------------------- CMSFS_FIND_LABEL
 *  Look for the CMS volume label structure (ADT).
 *  If we don't find it,  this ain't no CMS EDF filesystem.
 *  Returns the physical blocksize of the disk on success.
 */
static int cmsfs_find_label(struct cms_super *vol, struct CMSFSADT *adt)
{
	int i, rc;
	unsigned char *cmsfsflb;
	
	/*  (probably should compute)  */

	/*  char array (4 bytes) "CMS1" filesystem identifier  */
	char cmsfsvsk[8] = CMSFSVSK;
	/*  (a "magic number" by any other name)  */

	/*  array of possible CMS FS blocksizes  */
	int cmsfsbkt[8] = CMSFSBKT;

	cmsfsflb = (void *)get_free_page(GFP_KERNEL);
	if(cmsfsflb==NULL)
	{
		if(vol->error == 0)
			vol->error = EIO;
		return -1;
	}
	
	/*
	 *  FBA DASDs are special.  Physical blocsize is always 512
	 *  and the label is at physical offset 512 (second record),
	 *  though the logical blocksize may be 512, 1K, 2K, or 4K.
	 */
	if (vol->pbksz == 0 || vol->pbksz == 512) {
		/*  read FBA block #1 (second record)  */
		rc = cmsfs_bread(vol, cmsfsflb, 1, 512);
		if (rc != 512) {
			if (vol->error == 0)
				vol->error = EIO;	/* I/O error */
			free_page((unsigned long)cmsfsflb);
			return -1;
		}
		(void) memcpy(adt, cmsfsflb, sizeof(*adt));

		/*  check for the CMS1 magic at the FBA offset  */
		if (adt->ADTIDENT[0] == cmsfsvsk[0]
		    && adt->ADTIDENT[1] == cmsfsvsk[1]
		    && adt->ADTIDENT[2] == cmsfsvsk[2]
		    && adt->ADTIDENT[3] == cmsfsvsk[3]) 
		{
			int blksz;
			blksz = cmsfsbex(adt->ADTDBSIZ, 4);
			if (blksz != 512) {
				printk(KERN_WARNING  "cmsfs_find_label(): FS blocksize %d does not match device blocksize %d.\n", blksz, 512);
			}
			vol->flags = CMSFSFBA;
			vol->blksz = 512;
			free_page((unsigned long)cmsfsflb);
			return vol->blksz;
		}
	}

	/*  not an FBA volume; try CKD blocksizes  */
	/*
	 *  CKD DASDs are C/H/S in nature and can have any blocksize
	 *  that the utility or operating system decided to put there.
	 *  OS/390 uses no particular blocksize,  referring to tracks
	 *  directly.   For CMS CKD volumes,  the physical blocksize
	 *  should match logical,  unless obscured by the access method.
	 */
	for (i = 0; cmsfsbkt[i] != 0; i++) {
		if (vol->pbksz == 0 || vol->pbksz == cmsfsbkt[i]) {
			/*  read CKD block #2 (third record)  */
			rc = cmsfs_bread(vol, cmsfsflb, 2, cmsfsbkt[i]);
			if (rc != cmsfsbkt[i]) {
				if (vol->error == 0)
					vol->error = EIO;	/* I/O error */
				free_page((unsigned long)cmsfsflb);
				return -1;
			}
			(void) memcpy(adt, cmsfsflb, sizeof(*adt));

			/*  check for the CMS1 magic  */
			if (adt->ADTIDENT[0] == cmsfsvsk[0]
			    && adt->ADTIDENT[1] == cmsfsvsk[1]
			    && adt->ADTIDENT[2] == cmsfsvsk[2]
			    && adt->ADTIDENT[3] == cmsfsvsk[3]) {
				int blksz;
				blksz = cmsfsbex(adt->ADTDBSIZ, 4);
				if (blksz == cmsfsbkt[i]) {
					vol->flags = CMSFSCKD;
					vol->blksz = cmsfsbkt[i];
					free_page((unsigned long)cmsfsflb);
					return vol->blksz;

				}
				printk(KERN_ERR "cmsfs_find_label(): FS blocksize %d does not match device blocksize %d.\n", blksz, cmsfsbkt[i]);
			}
		}
	}

	vol->error = ENOMEDIUM;	/* "No medium found" */
	return -1;		/*  not a CMS volume  */
}

/*
 *  The above routine presumes that even an FBA volume will be
 *  at least 12K.   This is technically incorrect:  FBA CMS formatting
 *  can use as little as 8K.   But since such a volume would have
 *  only 1024 bytes for storage,  I think this is a small exposure.
 */

/* ------------------------------------------------------- CMSFS_MAP_ADT
 *  Map an ADT struct into our own cms_super superblock struct.
 *  CMS flavor superblock struct must be supplied.
 *  CMS flavor inode struct will be allocated,  if this succeeeds.
 */

static int cmsfs_map_ADT(struct cms_super *vol)
{
	int i,			/*  a counter  */
	 rc;			/*  a return code  */

	/*  partial "ADT" structure (per IBM)  */
	static struct CMSFSADT cmsvol;
	unsigned char *cmsfsflb;
	/*  (probably should compute instead of always using max)  */

	/*  should check that cms_super passed is non-NULL  */
	/*  should perhaps check that vol->inuse == 0  */

	/*  set this to zero now because it serves as a check later  */
	vol->blksz = 0;

	/*  look for the CMS volume label (aka VOLSER)  */
	rc = cmsfs_find_label(vol, &cmsvol);
	/*  this also effects a load of the ADT struct to &cmsvol  */
	/*  and sets the blksz member to match the volume found  */

	/*  did we find a CMS1 volume label?  */
	if (rc <= 0) {
		printk(KERN_WARNING "cmsfs_map_ADT(): cannot find a CMS1 label");
		return -1;
	}

	/*  extract volume label and translate  */
	for (i = 0; i < 6; i++)
		vol->volid[i] = cmsvol.ADTID[i];
	vol->volid[6] = 0x00;
	
	stretoa(vol->volid);

	/*  extract "directory origin pointer"  */
	vol->origin = cmsfsbex(cmsvol.ADTDOP, 4);

	/*  extract number of cylinders used  */
	vol->ncyls = cmsfsbex(cmsvol.ADTCYL, 4);

	/*  extract max number of cylinders  */
	vol->mcyls = cmsfsbex(cmsvol.ADTMCYL, 4);

	/*  extract "total blocks on disk" count  */
	vol->blocks = cmsfsbex(cmsvol.ADTNUM, 4);

	/*  compute "blocks used" count  */
	vol->bkused = cmsfsbex(cmsvol.ADTUSED, 4);
	vol->bkused += 1;	/*  why???  */

	/*  compute "blocks free" count  */
	vol->bkfree = vol->blocks - vol->bkused;

	/*  compute time (as ctime) when this volume was created  */
	{
		struct cmsfs_tm temptime;	/*  temporary  */
		temptime.tm_sec = cmsfsx2i(&cmsvol.ADTDCRED[5], 1);
		temptime.tm_min = cmsfsx2i(&cmsvol.ADTDCRED[4], 1);
		temptime.tm_hour = cmsfsx2i(&cmsvol.ADTDCRED[3], 1);
		temptime.tm_mday = cmsfsx2i(&cmsvol.ADTDCRED[2], 1);
		temptime.tm_mon = cmsfsx2i(&cmsvol.ADTDCRED[1], 1) - 1;
		temptime.tm_year = cmsfsx2i(&cmsvol.ADTDCRED[0], 1);
		if (cmsvol.ADTFLGL[0] & ADTCNTRY)
			temptime.tm_year += 100;
		vol->ctime = mktime(temptime.tm_year, temptime.tm_mon,
					temptime.tm_mday, temptime.tm_hour,
					temptime.tm_min, temptime.tm_sec);
	}

	/*  extract offset to reserved file,  if any  */
	vol->resoff = cmsfsbex(cmsvol.ADTOFFST, 4);

	/*  extract size and number of FSTs  */
	vol->fstsz = cmsfsbex(cmsvol.ADTFSTSZ, 4);
	vol->fstct = cmsfsbex(cmsvol.ADTNFST, 4);

	/*  initial filetype mapping table is NULL  */
	vol->cmsfsext = NULL;

	cmsfsflb = (void *)get_free_page(GFP_KERNEL);
	if(cmsfsflb==NULL)
	{
		printk(KERN_WARNING "cmfsfs_map_ADT(): out of memory for buffer.\n");
		return -1;
	}

	/*  allocate and map the directory inode from its FST  */

	vol->cmsrooti = kmalloc(sizeof(struct cms_inode), GFP_KERNEL);
	if (vol->cmsrooti == NULL) {
		printk(KERN_WARNING "cmsfs_map_ADT(): cannot allocate CMS directory inode.\n");
		free_page((unsigned long)cmsfsflb);
		return -1;
	}

	/*  read in the first FST  */
	rc = cmsfs_bread(vol, cmsfsflb, vol->origin - 1, vol->blksz);
	if (rc != vol->blksz) {
		kfree(vol->cmsrooti);
		free_page((unsigned long)cmsfsflb);
		vol->inuse = 0;	/*  we just deallocated that inode  */
		return -1;
	}

	vol->inuse = 1;		/*  we just allocated one inode  */

	/*  point dir inode back to the superblock  */
	vol->cmsrooti->cmssuper = vol;
	/*  must be set BEFORE calling cmsfs_map_FST()  */

	/*  map the directory ADT into a the root directory inode  */
	rc = cmsfs_map_FST(vol->cmsrooti, (struct CMSFSFST *) cmsfsflb);
	if (rc != 0) {
		printk(KERN_WARNING "cmsfs_map_ADT(): cmsfs_map_FST() of the directory returned %d.\n", rc);
		kfree(vol->cmsrooti);
		free_page((unsigned long)cmsfsflb);
		vol->inuse = 0;	/*  we just deallocated that inode  */
		return rc;
	}

	vol->cmsrooti->flags = CMSFSBIN;	/*  directory is F 64 binary  */

	/*  (announce if debugging) dir inode is now allocated and mapped  */

	/*  sanity check:  FOP in dir entry must match DOP in vol  */

	/*  sanity check:  RECFM of a directory must be "F"  */
	if (vol->cmsrooti->recfm[0] != 'F') {
		printk("cmsfs_map_ADT(): directory RECFM '%s' not 'F'.\n", vol->cmsrooti->recfm);
		kfree(vol->cmsrooti);
		free_page((unsigned long)cmsfsflb);
		vol->inuse = 0;	/*  we just deallocated that inode  */
		return -1;
	}

	/*  sanity check:  LRECL of a directory must be 64  */
	if (vol->cmsrooti->lrecl != 64) {
		printk(KERN_WARNING "cmsfs_map_ADT(): directory LRECL %d not 64.\n", vol->cmsrooti->lrecl);
		kfree(vol->cmsrooti);
		free_page((unsigned long)cmsfsflb);
		vol->inuse = 0;	/*  we just deallocated that inode  */
		return -1;
	}

	vol->files = vol->cmsrooti->items;

	/*  report  */
	vol->error = 0;		/*  no error yet;  this is a new superblock  */

	free_page((unsigned long)cmsfsflb);
	return 0;
}

/* ------------------------------------------------------- CMSFS_MAP_FST
 *  Map a CMS FS FST structure to our own cms_inode "inode" structure.
 *  Operation is function(target,source).
 */

static int cmsfs_map_FST(struct cms_inode *finode, struct CMSFSFST *cmsfst)
{
	int i;
	char *p, *q, *qq;

	/*  extract and translate filename  */
	p = cmsfst->FSTFNAME;
	q = finode->name;
	qq = finode->fname;
	i = 0;
	while (*p != 0x40 && *p != 0x00 && i < 8) {
		*qq++ = *q++ = chretoa(*p++);
		i++;
	}
	*q++ = '.';
	*qq++ = 0x00;
	p = cmsfst->FSTFTYPE;
	qq = finode->ftype;
	i = 0;
	while (*p != 0x40 && *p != 0x00 && i < 8) {
		*qq++ = *q++ = chretoa(*p++);
		i++;
	}
	*q++ = 0x00;
	*qq++ = 0x00;

	/*  extract and translate FMODE  */
	p = cmsfst->FSTFMODE;
	q = finode->fmode;
	q[0] = chretoa(p[0]);
	q[1] = chretoa(p[1]);
	q[2] = 0x00;

	/*  extract and translate RECFM  */
	p = cmsfst->FSTRECFM;
	q = finode->recfm;
	q[0] = chretoa(p[0]);
	q[1] = 0x00;

	/*  extract LRECL  */
	finode->lrecl = cmsfsbex(cmsfst->FSTLRECL, 4);

	/*  extract "directory origin pointer"  */
	finode->origin = cmsfsbex(cmsfst->FSTFOP, 4);

	/*  extract block count  */
	finode->bloks = cmsfsbex(cmsfst->FSTADBC, 4);

	/*  extract item count  */
	finode->items = cmsfsbex(cmsfst->FSTAIC, 4);

	/*  conditionally report these CMS file attributes  */

	/*  extract date and compute time stamp  */
	{
		struct cmsfs_tm temptime;	/*  temporary  */
		temptime.tm_year = cmsfsx2i(&cmsfst->FSTADATI[0], 1);
		if (cmsfst->FSTFLAGS[0] & FSTCNTRY)
			temptime.tm_year += 100;
		temptime.tm_mon = cmsfsx2i(&cmsfst->FSTADATI[1], 1) - 1;
		temptime.tm_mday = cmsfsx2i(&cmsfst->FSTADATI[2], 1);
		temptime.tm_hour = cmsfsx2i(&cmsfst->FSTADATI[3], 1);
		temptime.tm_min = cmsfsx2i(&cmsfst->FSTADATI[4], 1);
		temptime.tm_sec = cmsfsx2i(&cmsfst->FSTADATI[5], 1);
		finode->ctime = mktime(temptime.tm_year, temptime.tm_mon,
					temptime.tm_mday, temptime.tm_hour,
					temptime.tm_min, temptime.tm_sec);
	}

	/*  levels of indirection and pointer size  */
	finode->level = cmsfst->FSTNLVL[0] & 0xFF;
	finode->psize = cmsfst->FSTPTRSZ[0] & 0xFF;

	/*  conditionally report these CMS file attributes  */

	finode->rdpnt = finode->rdblk = finode->rdoff = 0;	/*  set read pointers to zero  */
	finode->rdoff2 = 0;	/*  set read pointers to zero  */

	finode->rdbuf = NULL;	/*  no work buffer unless alloc later  */
	finode->rdbuf2 = NULL;	/*  no span buffer unless alloc later  */

	finode->error = 0;	/*  no error yet;  this is a new inode  */
	finode->bytes = 0;	/*  don't bother about the size yet  */
	finode->flags = 0x0000;

	return 0;
}

/* ------------------------------------------------------- CMSFS_MAP_EXT
 *  Map a CMS filetype (sometimes called an "extension").
 *  The terms "filetype" and "extension" are used interchangably here.
 */

static void cmsfs_map_EXT(struct cms_inode *fi)
{
	int i;
	unsigned char *p;

	/*  sanity check:  be sure supplied "inode" pointer is non-NULL  */
	if (fi == NULL)
		BUG();

	/*  sanity check:  be sure inode referenced has a superblock  */
	if (fi->cmssuper == NULL)
		BUG();

	/*  silently return if this file has any flags already set  */
	if (fi->flags != 0x0000)
		return;

	/*  possibly initialize the superblock's filetype mapping table  */
	if (fi->cmssuper->cmsfsext == NULL) {
		fi->cmssuper->cmsfsext = cmsfsext;
		for (i = 0; fi->cmssuper->cmsfsext[i].ftype[0] != ' '
		     && fi->cmssuper->cmsfsext[i].ftype[0] != ' '; i++) {
			p = fi->cmssuper->cmsfsext[i].ftype;
			while (*p != ' ' && *p != 0x00)
				p++;
			if (*p == ' ')
				*p = 0x00;
		}
	}

	/*  step through the known types list,  looking for a match  */
	for (i = 0; fi->cmssuper->cmsfsext[i].ftype[0] != ' '; i++) {
		if (strncmp(fi->ftype,
			    fi->cmssuper->cmsfsext[i].ftype, 8) == 0x0000)
		{
			fi->flags = fi->cmssuper->cmsfsext[i].flags;
			return;
		}
	}

	/*  file is of a type we don't know how to canonicalize  */
	return;
}

/* ------------------------------------------------------------ CMSFSRD2
 *  "Read Function number Two" -- read a block from a CMS file.
 *  This is a second attempt.   THIS FUNCTION NEEDS A BETTER NAME.
 */

static int cmsfsrd2(struct cms_inode *inode, void *buffer, int block)
{
	int ppb, i, j, b1, b2, rc;
	void *bp;

	/*  when no indirection,  do a simple read  */
	if (inode->level == 0)
		return cmsfs_bread(inode->cmssuper, buffer,
				inode->origin - 1 + block,
				inode->cmssuper->blksz);

	/* pointers per block */
	if (inode->psize > 0)
		ppb = inode->cmssuper->blksz / inode->psize;
	else
		ppb = inode->cmssuper->blksz / 4;

	/*  read first block for indirection  */
	b1 = block;
	for (i = 0; i < inode->level; i++)
		b1 = b1 / ppb;
	rc = cmsfs_bread(inode->cmssuper, buffer,
			 inode->origin - 1 + b1, inode->cmssuper->blksz);
	if (rc != inode->cmssuper->blksz) {
		printk(KERN_WARNING
			       "cmsfsrd2(): cmsfs_bread(,,%d,%ld) returned %d.\n",
			       inode->origin - 1 + b1,
			       inode->cmssuper->blksz, rc);
		return rc;
	}

	for (j = 1; j <= inode->level; j++) {
		/*  read next block for indirection  */
		b1 = block;
		for (i = j; i < inode->level; i++)
			b1 = b1 / ppb;
		b1 = b1 % ppb;
		bp = buffer;
		bp += b1 * inode->psize;
		b2 = cmsfsbex(bp, 4);

		/*  read next block for indirection  */
		rc = cmsfs_bread(inode->cmssuper, buffer,
				 b2 - 1, inode->cmssuper->blksz);
		if (rc != inode->cmssuper->blksz) {
			printk(KERN_WARNING "cmsfsrd2(): cmsfs_bread(,,%d,%ld) returned %d.\n",
				       inode->origin - 1 + b1,
				       inode->cmssuper->blksz, rc);
			return rc;
		}
	}
	return inode->cmssuper->blksz;
}

/* -------------------------------------------------------- CMSFS_LOOKUP
 *  Lookup a file in the CMS directory.  Allocates a cms_inode struct.
 *  After successful call to this function,  the caller might
 *  possibly set the ->flags member.  In kernel mode (VFS),
 *  the caller should also set the ->vfsinode member.
 */

static struct cms_inode *cmsfs_lookup(struct cms_inode *di, unsigned char *fn)
{
	struct cms_inode *cmsinode;
	int i, j, k, rc;
	unsigned char *p, xmatch[18], umatch[18];
	unsigned char *buff;

	/*  first things first:  confirm superblock and root inode  */
	if (di->cmssuper == NULL)
		BUG();
	if (di->cmssuper->cmsrooti == NULL)
		BUG();

	/*  gimme another CMS inode structure  */
	cmsinode = kmalloc(sizeof(struct cms_inode), GFP_KERNEL);
	if (cmsinode == NULL) {
		printk(KERN_WARNING "cmsfs_lookup(): cannot allocate CMS inode.\n");
		return NULL;
	}

	/*  if we're opening the directory as a file, then ...  */
	if (strncmp(fn, ".dir", 4) == 0) {
		(void) memcpy(cmsinode,
			      di->cmssuper->cmsrooti, sizeof(*cmsinode));
		cmsinode->index = 0;	/*  the number of this inode  */
		di->cmssuper->inuse += 1;	/*  another inode for this super  */
		cmsinode->flags = CMSFSBIN;	/*  F 64 so, yes, binary  */
		return cmsinode;	/*  return "success"  */
	}

	/*  set stuff up:  fill-in the structure for this file  */
	cmsinode->cmssuper = di->cmssuper;

	/*  prepare to search:  establish upper-case and exact matches  */
	p = fn;
	i = 0;

	/*  first eight bytes (CMS filename)  */
	while (i < 8 && *p != '.' && *p != 0x00) {
		umatch[i] = xmatch[i] = chratoe(*p);
		if (0x80 < umatch[i] && umatch[i] < 0xC0)
			umatch[i] += 0x40;
		i++;
		p++;
	}

	/*  conditionally adjust pointer  */
	if (*p != 0x00)
		p++;

	/*  possibly pad with blanks  */
	while (i < 8) {
		umatch[i] = xmatch[i] = 0x40;
		i++;
	}

	/*  second eight bytes (CMS filetype)  */
	while (i < 16 && *p != '.' && *p != 0x00) {
		umatch[i] = xmatch[i] = chratoe(*p);
		if (0x80 < umatch[i] && umatch[i] < 0xC0)
			umatch[i] += 0x40;
		i++;
		p++;
	}

	/*  possibly pad with blanks  */
	while (i < 16) {
		umatch[i] = xmatch[i] = 0x40;
		i++;
	}

	/*  terminate with NULL character  */
	umatch[i] = xmatch[i] = 0x00;

	buff = (void *)get_free_page(GFP_KERNEL);
	if(buff == NULL)
	{
		printk(KERN_WARNING "cmsfs_lookup: out of memory.\n");
		kfree(cmsinode);
		return NULL;
	}
			
	/*  find the file  */
	j = di->cmssuper->fstct;
	/*  list the files  */
	for (i = k = 0; i < di->cmssuper->files; i++) {
		char *pp;
		if (j >= di->cmssuper->fstct) {
			(void) cmsfsrd2(di->cmssuper->cmsrooti, buff, k);
			k += 1;	/*  increment block counter  */
			j = 0;	/*  reset intermediate item counter  */
		}
		pp = &buff[j * di->cmssuper->fstsz];
		/*  compare  */
		rc = strncmp(pp, umatch, 16);
		if (rc == 0) {	/*  found it??  */
			/*  map the inode for the file  */
			rc = cmsfs_map_FST(cmsinode,
					   (struct CMSFSFST *) pp);
			if (rc != 0) {
				printk(KERN_WARNING "cmsfs_lookup(): cannot map CMS inode (FST).\n");
				goto out;
			}
			cmsinode->index = i;	/*  the number of this inode  */
			di->cmssuper->inuse += 1;	/*  another inode for super  */
			cmsinode->flags = 0x0000;	/*  CALLER MUST APPLY  */
			free_page((unsigned long)buff);
			return cmsinode;	/*  return "success"  */
		}
		j += 1;		/*  increment intermediate item counter  */
	}
	di->cmssuper->error = di->cmssuper->cmsrooti->error = ENOENT;
out:	
	kfree(cmsinode);
	di->cmssuper->inuse -= 1;
	free_page((unsigned long)buff);
	return NULL;
}

/* ---------------------------------------------------------- CMSFS_READ
 *  Read a record at the current pointer in the CMS file.
 *  Simple:  read the record,  increment the record counter.
 */

static ssize_t cmsfs_read(struct cms_inode * cmsfil, void *recbuf, size_t reclen)
{
	int i, l, rc;
	char *bufi, *bufo;

	/*  sanity check:  file pointer must non-NULL  */
	if (cmsfil == NULL)
		BUG();
	/*  sanity check:  superblock pointer must non-NULL  */
	if (cmsfil->cmssuper == NULL)
		BUG();

	/*  can't read past end of file  */
	if (cmsfil->rdpnt >= cmsfil->items)
		return 0;	/*  zero means EOF */

	/*  possibly allocate a work buffer  */
	if (cmsfil->rdbuf == NULL) 
	{
		cmsfil->rdbuf = kmalloc(cmsfil->cmssuper->blksz, GFP_KERNEL);
		if (cmsfil->rdbuf == NULL) {
			printk(KERN_WARNING "cmsfs_read(): unable to allocate a work buffer.\n");
			return -ENOMEM;
		}
#ifdef  CMSFS_DEBUG
		(void) sprintf(cmsfs_ermsg,
			       "cmsfs_read(): '%s' blk %d rec %d (#1)",
			       cmsfil->name, cmsfil->rdblk, cmsfil->rdpnt);
		cmsfs_error(cmsfs_ermsg);
#endif

		/*  now load-up a block into this new buffer  */
		rc = cmsfsrd2(cmsfil, cmsfil->rdbuf, cmsfil->rdblk);
		if (rc != cmsfil->cmssuper->blksz) {
			printk(KERN_WARNING "cmsfs_read(): could not read block.\n");
			return -EIO;
		}
		cmsfil->rdblk += 1;
		cmsfil->rdoff = 0;
	}

	/*  dereference VOID buffers to CHAR buffers  */
	bufi = cmsfil->rdbuf;
	bufo = recbuf;

	/*  how big is this record?  */
	switch (cmsfil->recfm[0]) {
	case 'F':
		l = cmsfil->lrecl;
		break;
	case 'V':
		if (cmsfil->rdoff >= cmsfil->cmssuper->blksz) {
			rc = cmsfsrd2(cmsfil, cmsfil->rdbuf,
				      cmsfil->rdblk);
			if (rc != cmsfil->cmssuper->blksz) {
				printk(KERN_ERR "cmsfs_read(): could not read block.\n");
				return -1;
			}
			cmsfil->rdblk += 1;
			cmsfil->rdoff = 0;
		}
		l = bufi[cmsfil->rdoff++] & 0xFF;
		if (cmsfil->rdoff >= cmsfil->cmssuper->blksz) {
			rc = cmsfsrd2(cmsfil, cmsfil->rdbuf,
				      cmsfil->rdblk);
			if (rc != cmsfil->cmssuper->blksz) {
				printk(KERN_WARNING "cmsfs_read(): could not read block.\n");
				return -EIO;
			}
			cmsfil->rdblk += 1;
			cmsfil->rdoff = 0;
		}
		l = (l * 256) + (bufi[cmsfil->rdoff++] & 0xFF);
		break;
	default:		/*  Bzzzttt!!!  */
		printk(KERN_WARNING "cmsfs_read(): RECFM not 'F' or 'V'.\n");
		return -EOPNOTSUPP;
	}

	if (l > reclen) {
		printk(KERN_WARNING "cmsfs_read(): record (%d) is longer than buffer (%d).\n", l, reclen);
		return -EMSGSIZE;
	}

	/*  copy bytes from work buffer to caller's buffer  */
	for (i = 0; i < l; i++) {
		if (cmsfil->rdoff >= cmsfil->cmssuper->blksz) {
			rc = cmsfsrd2(cmsfil, cmsfil->rdbuf,
				      cmsfil->rdblk);
			if (rc != cmsfil->cmssuper->blksz) {
				printk(KERN_WARNING "cmsfs_read(): could not read block.\n");
				return -EIO;
			}
			cmsfil->rdblk += 1;
			cmsfil->rdoff = 0;
		}
		bufo[i] = bufi[cmsfil->rdoff++];
	}

	/*  terminate the record (with NULL and possibly also NL)  */
	bufo[i] = 0x00;
	if (cmsfil->flags & CMSFSTXT) {
		stretoa(bufo);
		bufo[i++] = '\n';
		bufo[i] = 0x00;
	}

	cmsfil->rdpnt += 1;	/*  increment record pointer  */

	return i;		/*  return length of string copied  */
}

/* --------------------------------------------------------- CMSFS_BYTES
 *  Return the size-in-bytes of a CMS file and stamp it in the struct.
 */
static long cmsfs_bytes(struct cms_inode *fi)
{
	int i, j, k, l;

	/*  quick return if this function has been called before  */
	if (fi->bytes != 0)
		return fi->bytes;

	/*  if there are no records,  then there ain't no bytes  */
	if (fi->items == 0)
		return fi->bytes = 0;

	/*  in case conversion is not already known,  do this  */
	if (fi->flags == 0x0000)
		cmsfs_map_EXT(fi);

	/*  how big is this file?  "it depends"  */
	switch (fi->recfm[0]) {
	case 'F':		/*  for fixed-length,  total size is easy  */
		fi->bytes = fi->lrecl * fi->items;
		/*  for text files, add NL to each record  */
		if (fi->flags & CMSFSTXT)
			fi->bytes += fi->items;
		break;

	case 'V':		/*  for variable-length,  we must read content  */
		l = fi->lrecl + 2;
		/*  allocated a work buffer of LRECL size  (plus two)  */
		if (fi->rdbuf2 == NULL)
			fi->rdbuf2 = kmalloc(l, GFP_KERNEL);
		if (fi->rdbuf2 == NULL) {
			printk(KERN_WARNING "cmsfs_lookup(): cannot allocate CMS inode.\n");
			return -1;
		}
		k = 0;		/*  byte total starts at zero  */
		fi->rdpnt = 0;	/*  record counter starts at zero  */
		/*  read every record, totalling up the bytes  */
		for (i = 0; i < fi->items; i++) {
			j = cmsfs_read(fi, fi->rdbuf2, l);
			if (j < 0)
				return j;
			k = k + j;
		}
		fi->bytes = k;
		break;

	default:
		fi->error = EIO;	/*  internal error  */
		return 0;
	}

	return fi->bytes;
}

 
/* ----------------------------------------------------- CMSFS_FILE_READ
 *  CMS FS "read" function for VFS file.
 *  We prefer to work with inodes,  so extract the inode
 *  from the file pointer,  formally  file->f_dentry->d_inode.
 */

static ssize_t cmsfs_file_read(struct file * file,
                char * buffer, size_t length, loff_t * offset)
{
	struct cms_inode * ci;
	int     rc;
 
	/*  dereference CMS inode pointer  */
	ci = file->f_dentry->d_inode->u.generic_ip;
	if (ci->vfsinode != file->f_dentry->d_inode)
		BUG();
 
	/*  if translation hasn't been determined,  figure it out  */
	cmsfs_map_EXT(ci);
	/*  default to text files for the time being  */
	if (ci->flags == 0x0000)
		ci->flags = CMSFSTXT;
 
	/*  allocate a record buffer,  if needed  */
	if (ci->rdbuf2 == NULL)
        	ci->rdbuf2 = kmalloc(ci->lrecl+2, GFP_KERNEL);
	if (ci->rdbuf2 == NULL)
	{
		printk(KERN_WARNING "cmsfs_file_read(): cannot allocate record buffer.\n");
		return -1;
	}
 
	/*  try reading the file  */
	rc = cmsfs_read(ci,ci->rdbuf2,length);
#ifdef  CMSFS_DEBUG
	printk(KERN_DEBUG "cmsfs_file_read(): cmsfs_read() returned %d %d.\n",rc,length);
#endif
	if (rc < 0)
		return rc;
 
	/*  copy from kernel storage to user storage  */
	if (rc > 0) 
		(void) copy_to_user(buffer,ci->rdbuf2,rc);
 
	/*  update file pointers  */
	*offset = *offset + rc;
	return rc;      /*  report how many bytes were read  */
}
 
/* -------------------------------------------------- CMSFS_FILE_READDIR
 *  CMS FS "readdir" function for VFS directory.
 *  It took me a long time to figure out to code the right datatype
 *  for the fourth argument to filldir().   [sigh]
 */

static int cmsfs_file_readdir(struct file * file,
                                void * dirent, filldir_t myfd)
{
	struct  cms_inode  *dirinode, *tmpinode;
	int rc, i, j, k;
	unsigned char *cmsfsflb; 
	if (file->f_pos != 0) 
		return 0;
 
	tmpinode = kmalloc(sizeof(struct cms_inode), GFP_KERNEL);
	/* Not a good way to gracefully handle failure here, just give up */
	if(tmpinode==NULL)
		return 0;
		
	cmsfsflb = (void *)get_free_page(GFP_KERNEL);
 
	dirinode = file->f_dentry->d_inode->u.generic_ip;
	dirinode->rdpnt = 0;
	file->f_pos = dirinode->rdpnt * dirinode->lrecl;
 
	rc = myfd(dirent,".",1,file->f_pos,0, DT_DIR);
 
	dirinode->rdpnt += 1;
	file->f_pos = dirinode->rdpnt * dirinode->lrecl;
 
	rc = myfd(dirent,".",1,file->f_pos,0, DT_DIR);
	
	dirinode->rdpnt += 1;
	file->f_pos = dirinode->rdpnt * dirinode->lrecl;
 
	/*  prime the loop with the first block of the directory  */
	j = 128;  k = 0;
	rc = cmsfsrd2(dirinode,cmsfsflb,k);
 
	/*  skipping .DIRECTOR and .ALLOCMAP,  fill VFS dir with files  */
	for (i = 2 ; i < dirinode->items ; i++)
	{
        	(void) cmsfs_map_FST(tmpinode, (struct CMSFSFST *)&cmsfsflb[j]);
        	/* No subdirs so all files are regular */
		rc = myfd(dirent,tmpinode->name, strlen(tmpinode->name),file->f_pos,i, DT_REG);
 
		/*  if the filldir() call failed,  get outta here  */
		if (rc != 0) break;
 
		j += 64;

		if (j >= dirinode->cmssuper->blksz)
		{
			k++;
			rc = cmsfsrd2(dirinode,cmsfsflb,k);
			j = 0;
		}
 
		dirinode->rdpnt += 1;
		file->f_pos = dirinode->rdpnt * dirinode->lrecl;
	}
 
	kfree(tmpinode);
	free_page((unsigned long)cmsfsflb);
	return 0;
}
 
/* ----------------------------------------------------- CMSFS_FILE_OPEN
 *  CMS FS "open" function for VFS file.
 *  This is just a check in this implementation.
 */

static ssize_t cmsfs_file_open(struct inode * in, struct file * file)
{
	struct cms_inode * ci;
 
	/*  dereference CMS inode pointer and check cross-link  */
	ci = in->u.generic_ip;
	if (ci == NULL)
		BUG();
	if (ci->vfsinode != in)
		BUG();
		 
	/*  reset CMS and VFS pointers  */
	ci->rdpnt = ci->rdblk = ci->rdoff = 0;
	file->f_pos = 0;
 
	return 0;
}
 
/* ------------------------------------------------------------------ */
static struct file_operations cmsfs_file_operations = {
	read: cmsfs_file_read,
	readdir: cmsfs_file_readdir,
	open: cmsfs_file_open,
};
 
/* -------------------------------------------------- CMSFS_INODE_LOOKUP
 *  Search for the file in the CMS directory.
 *  Calls:  cmsfs_lookup() to find the file,
 *          iget() to get a VFS inode for the file, if found,
 *          d_add() to add that inode to the VFS directory.
 */
static struct dentry *cmsfs_inode_lookup(struct inode * in, struct dentry *de)
{
	struct cms_inode * cmsinode;
	struct inode * inode;
	int         n;
 
	/*
	 *  On CMS minidisks,  the directory entries (FST) serve as inodes.
	 *  Here we allocate one in doing the search,  and then free it.
	 *  Not elegant,  but works,  and interfaces properly with VFS.
	 */

	cmsinode = cmsfs_lookup((struct cms_inode *) in->u.generic_ip, 
		(unsigned char *) de->d_name.name);
	if (cmsinode == NULL)
		return NULL;
 
	/*  it would be nice to be able to hand-off the CMS inode struct  *
	 *  rather than going through another allocation from iget().     */

	n = cmsinode->index;
	cmsinode->cmssuper->inuse -= 1;
	kfree(cmsinode);
 
	/*  iget() will allocate a VFS inode.         *
	 *  it will also call cmsfs_read_inode(),     *
	 *  thus re-allocating the CMS inode struct.  */
	inode = iget(in->i_sb,n);
	if (inode == NULL)
		return NULL;
 
	d_add(de,inode); 
	return NULL;
}
 
/* ------------------------------------------------------------------ */

static struct inode_operations cmsfs_inode_operations = {
        lookup:	cmsfs_inode_lookup,
};
 
/* ---------------------------------------------------- CMSFS_READ_INODE
 *  CMS FS inode numbers are base-zero index into the CMS directory.
 *  CMS FS (EDF) is flat, no sub-directories, so there is only
 *  one such directory into which we index, always the "root".
 *  If the inode we are reading is zero,  we mark it as the directory.
 *  The CMS superblock struct must have already been allocated,
 *  although CMS inode structs will usually be allocated here.
 */

static void cmsfs_read_inode(struct inode * in)
{
	struct  cms_inode  *cmsinode = NULL;
	struct  cms_super  *cmssuper;
	int rc, ct = 0;
 
	/*  dummy values in case any of this fails  */
	in->i_uid = 0;  in->i_gid = 0;
	in->i_mode = 0;  in->i_nlink = 0;  in->i_size = 0;
	in->i_atime = in->i_mtime = in->i_ctime = 0;
	in->i_blksize = 0;
 
	/*  dereference pointer to CMS superblock from VFS superblock  */
	cmssuper = (struct cms_super *) in->i_sb->u.generic_sbp;
	/*  and check it ...  */
	if (cmssuper == NULL)
		BUG();
	if (cmssuper->vfssuper != in->i_sb)
		BUG();
 
	/*  allocate a CMS inode struct  */
	if (in->u.generic_ip == NULL)
	{
        	if (in->i_ino == 0 && cmssuper->cmsrooti != NULL)
		{
			cmsinode = cmssuper->cmsrooti;
			/*  and check it  */
			if (cmsinode->cmssuper != cmssuper)
				BUG();
			ct = 0;
		}
		else
		{
			cmsinode = kmalloc(sizeof(struct cms_inode), GFP_KERNEL);
        		if (cmsinode != NULL)
        			cmsinode->cmssuper = NULL;
			ct = 1;
		}
	}
	if (cmsinode == NULL)
	{
        	printk(KERN_WARNING "cmsfs_read_inode(): error allocating CMS inode structure.\n");
		return;
	}
 
	/*  cross-link CMS and VFS inode structures  */
	cmsinode->vfsinode = in;
	in->u.generic_ip = cmsinode;
 
	/*  Non-NULL CMS superblock pointer implies that we have          *
	 *  already mapped the CMS inode.   A CMS superblock is           *
	 *  required for the cmsfs_map_FST() call to work.                */
	
	if (cmsinode->cmssuper == NULL)
	{
		unsigned char *cmsfsflb;
		cmsfsflb = (void *)get_free_page(GFP_KERNEL);
        	cmsinode->cmssuper = cmssuper;
		/*  read the CMS "inode" (FST entry) indexed by in->i_ino ,       *
		 *  map it to the newly allocated CMS inode struct,               *
		 *  and copy relevent content to the supplied VFS inode struct    */
		/*  RECHECK THIS LOGIC for block calc against different blocksizes  */

		rc = cmsfsrd2(cmssuper->cmsrooti,cmsfsflb,
        	        ((in->i_ino)*64)/(in->i_sb->s_blocksize));
		(void) cmsfs_map_FST(cmsinode,(struct CMSFSFST *)
		        &cmsfsflb[(in->i_ino%(in->i_sb->s_blocksize/64))*64]);
		        
		free_page((unsigned long)cmsfsflb);
	}
 
	/*  copy from CMS to VFS  */
	in->i_uid = 0;  in->i_gid = 0;
	in->i_mode = S_IRUGO;               /* set "r--r--r--" perms */
	in->i_nlink = 1;
	in->i_op = &cmsfs_inode_operations;
	in->i_fop = &cmsfs_file_operations;
 
	/*  how big is this file?  */
	in->i_size = cmsfs_bytes(cmsinode);
 
	/*  CMS files have only one time stamp  */
	in->i_atime = in->i_mtime = in->i_ctime = cmsinode->ctime;
 
	/*  CMS file blocksize is by definition filesystem blocksize  */
	in->i_blksize = cmsinode->cmssuper->blksz;
 
	/*  may not be accurate for total  */
	in->i_blocks = cmsinode->bloks;
 
	in->i_attr_flags = 0;
	in->i_state = 0;
	in->i_flags = 0;
	in->i_generation = 0;
	atomic_set(&in->i_writecount, 0);
 
	/*  now point to our custom inode_operations struct  */
	in->i_op = &cmsfs_inode_operations;
	/*  CMS FS uses the same i_op struct for files and the directory  */
 
	/*  is this a directory??  */
	if (in->i_ino == 0)
	{
        	in->i_mode |= S_IFDIR;          /* mark it as a directory */
        	/*  CMS directory was created when the filesystem was made  */
        	in->i_ctime = cmssuper->ctime;
		in->i_nlink += 1;
		in->i_mode |= S_IXUGO;          /* add "--x--x--x" perms */
	}
	/*  we know that inode zero is the CMS directory  */
	else
		in->i_mode |= S_IFREG;         /* mark it as a regular file */
 
	/*  increment the "in use" counter  */
	cmssuper->inuse += ct;
 	return;
}
 
/* --------------------------------------------------- CMSFS_CLEAR_SUPER
 *  Clears the cross-links and frees the CMS superblock struct.
 *  Cannot lock/unlock the superblock at this stage of the game.
 */

static void cmsfs_clear_super(struct super_block * sb)
{
	struct cms_super  *cmssuper;
 
	/*  dereference the CMS superblock pointer and check  */
	cmssuper = sb->u.generic_sbp;
	/*  if we've already been here,  then silently return  */
	if (cmssuper == NULL)
		return;
 
	/*  sanity check,  cross-reference CMS and VFS superblocks  */
	if (cmssuper->vfssuper != sb)
		BUG();
 
	/*  if superblock is still busy,  then bail  */
	if (cmssuper->inuse != 0)
	{
        	printk(KERN_WARNING "cmsfs_clear_super(): CMS superblock at %p is busy.\n", cmssuper);
		return;
	}
 
	/*  free the cms_super struct and mark it freed  */
	cmssuper->vfssuper = NULL;
	kfree(cmssuper);       /*  deref'd from sb->u.generic_sbp  */
	sb->u.generic_sbp = NULL;
 
	/*  one less instance of this driver  */
	MOD_DEC_USE_COUNT;
	/*  don't do this until all storage is freed  */
 
	return;
}
 
/* --------------------------------------------------- CMSFS_CLEAR_INODE
 *  Cleanly deallocate the CMS inode linked to this VFS inode.
 *  Because of the order in which VFS objects are released
 *  at umount() time,  this function must clear the CMS superblock
 *  if the inode being cleared is the directory.
 */
static void cmsfs_clear_inode(struct inode * in)
{
	struct cms_inode * ck;
	struct cms_super * cs;
	struct super_block * sb;
 
	/*  dereference the generic pointer to a CMS struct  */
	ck = in->u.generic_ip;
 
	/*  if no CMS inode to free,  then silently return  */
	if (ck == NULL) 
		return;
 
	/*  if ->vfsinode doesn't point back to VFS inode,  then error  */
	if (ck->vfsinode != in)
		BUG();
 
	/*  dereference VFS and CMS superblock pointers for shorthand  */
	sb = in->i_sb;      cs = ck->cmssuper;
 
	/*  sanity check those,  expecting them to be cross-linked  */
	if (sb->u.generic_sbp != cs)
		BUG();
	if (cs->vfssuper != sb)
		BUG();
 
	/*  decrement the "in use" counter  */
	cs->inuse -= 1;
 
	if (ck->rdbuf != NULL)
	{
	        kfree(ck->rdbuf);
	        ck->rdbuf = NULL;
	        ck->rdoff = 0;
	}
 
	if (ck->rdbuf2 != NULL)
	{
		kfree(ck->rdbuf2);
		ck->rdbuf2 = NULL;
		ck->rdoff2 = 0;
	}
 
	/*  reset some values to clean-up the storage  */
	ck->vfsinode = NULL;
	ck->cmssuper = NULL;
	/*  now free the storage  */
	kfree(ck);
	/*  and clear the VFS ref to CMS  */
	in->u.generic_ip = NULL;
 
	/*  was that the root (CMS dir) inode?  */
	if (ck != cs->cmsrooti) 
		return;
	/*  if it wasn't root,  then we're done  */
 
	/*  should be safe to free the CMS superblock  (ref'd by VFS)  */
	cmsfs_clear_super(sb);
 
	return;
}
 
/* -------------------------------------------------------- CMSFS_UMOUNT
 *  This is called technically to "begin umount".
 *  Ordinarily a no-op because CMS FS is read-only.
 *  Cannot be used as the code path to free the superblock
 *  because of the order in which VFS comes down when unmounting.
 
                CLEAN THIS UP!!!      Remove lock() unlock()
 
 */

static void cmsfs_umount(struct super_block * sb)
{
	struct cms_super  *cmssuper;
 
	/*  dereference the CMS superblock pointer and check  */
	cmssuper = sb->u.generic_sbp;
	/*  if we've already been here,  then silently return  */
	if (cmssuper == NULL) 
		return;
 
	/*  sanity check,  cross-reference CMS and VFS superblocks  */
	if (cmssuper->vfssuper != sb)
		BUG();
	/*  if superblock is still busy,  then bail  */
	if (cmssuper->inuse != 0)
	{
        	printk(KERN_WARNING "cmsfs_umount(): CMS superblock at %p is busy.\n", cmssuper);
        	return;
	}
 
	/*
	 *  VFS doesn't free inodes and superblock in the order I expected.
	 *  It calls  cmsfs_put_super()  first,  and then clears inodes.
	 *  The last inode cleared is root,  which is as expected.
	 *  But when should the superblock be cleared and freed?
	 *
	 *  Since VFS will do a  cmsfs_clear_inode()  for the directory,
	 *  DO NOT call it here,  and DO NOT clear the CMS dir inode here.
	 */
 
	/*  if we still have a CMS dir inode,  go ahead and sanity check  */
	if (cmssuper->cmsrooti != NULL)
	{
        	struct inode * in;
        	in = cmssuper->cmsrooti->vfsinode;
        	if (in->u.generic_ip != cmssuper->cmsrooti)
        		BUG();
	}
 	
	/*  free the cms_super struct and mark it freed  */
	lock_super(sb);
	cmssuper->vfssuper = NULL;
	kfree(cmssuper);       /*  deref'd from sb->u.generic_sbp  */
	sb->u.generic_sbp = NULL;
	unlock_super(sb);
 
	/*  one less instance of this driver  */
	MOD_DEC_USE_COUNT;
	/*  don't do this until all storage is freed  */
 
	return;
}
 
/* ----------------------------------------------------- CMSFS_PUT_SUPER
 *  Ordinarily a no-op because CMS FS is read-only.
 *  Cannot be used as the code path to free the superblock
 *  because of the order in which VFS comes down when unmounting.
 */

static void cmsfs_put_super(struct super_block * sb)
{
	return;
}
 
/* -------------------------------------------------------- CMSFS_STATFS
 *  Arguments: superblock, user-space statfs, length
 *  Returns zero for success, non-zero otherwise (typically -EFAULT).
 */

static int cmsfs_statfs(struct super_block * sb,struct statfs *st)
{
	struct cms_super  *cmssuper;
 
	/*  what if superblock pointer is NULL?  */
	if (sb == NULL)
		BUG();

	/*  dereference CMS superblock for short-hand below  */
	cmssuper = (struct cms_super *) sb->u.generic_sbp;
 
	/*  what if CMS superblock pointer is NULL?  */
	if (cmssuper == NULL)
		BUG();
 
	/*  as always,  sanity check the cross-link  */
	if (cmssuper->vfssuper != sb)
		BUG();
 
	/*  take data from CMS superblock  */
	st->f_bsize = cmssuper->blksz;
	st->f_blocks = cmssuper->blocks;
	st->f_bfree = cmssuper->bkfree;
	st->f_bavail = cmssuper->bkfree;
	st->f_files = cmssuper->files;
	/*  st=?f_ffree = cmssuper->ffree;  */
	st->f_ffree = 0;            /*  no files available on R/O media  */
 	return 0;
}
 
/* ------------------------------------------------------- CMSFS_REMOUNT
 * Remount a cms minidisk. If the flush seems odd remember you have multiple
 * OS's running here and IBM people use minidisks rather like floppies
 */

static int cmsfs_remount(struct super_block * sb,int *flags,char *data)
{
	/* Might be a convenient way to report FYI data on a CMS volume  */
	/* XXX when we are writable we may need to sync data blocks too */
	invalidate_buffers(sb->s_dev);
	return 0;
}
 
/* ------------------------------------------------------------------ */

static struct super_operations cmsfs_super_operations = {
	read_inode: cmsfs_read_inode,
        put_super:cmsfs_put_super,
        statfs: cmsfs_statfs,
        remount_fs: cmsfs_remount,
        clear_inode: cmsfs_clear_inode,
        umount_begin: cmsfs_umount
};
 
/* --------------------------------------------------------- CMSFS_MOUNT
 */

static struct super_block *cmsfs_mount(struct super_block * sb, void * data, int silent)
{
	struct  cms_super  *cmssuper;
	int         rc;
	struct  inode  *vfsrooti;
 
	/*  let the kernel know we're busy ... for the moment  */
	MOD_INC_USE_COUNT;
	lock_super(sb);
 
	/* -------------------------------------------------------------- *
	 *  Fill-out the VFS superblock and allocate a CMS superblock.    *
	 * -------------------------------------------------------------- */

	cmssuper = kmalloc(sizeof(struct cms_super), GFP_KERNEL);
	if (cmssuper == NULL)
	{
        	printk(KERN_WARNING "cannot allocate CMS superblock.\n");
        	sb->s_dev = 0;  /*  why?  */
        	unlock_super(sb);
        	MOD_DEC_USE_COUNT;
        	return NULL;
	}
 
	/*  cross-reference CMS superblock and VFS superblock  */
	sb->u.generic_sbp = cmssuper;       /*  VFS to ref CMS  */
	cmssuper->vfssuper = sb;            /*  CMS to ref VFS  */
 
	/*  root inode not there yet  */
	cmssuper->cmsrooti = NULL;
	/*  cmsfs_map_ADT()  will see the NULL and give us one  */
 
	/*  determine device blocksize  */
	cmssuper->pbksz = get_hardsect_size(sb->s_dev);
	/*  This may be redundant.  */
	set_blocksize(sb->s_dev,cmssuper->pbksz);
	/*  Got the following suggestion from Alan Cox.  */
	sb->s_blocksize_bits = 12;  /*  probably should switch()  */
 
	/*
	 *  Is there a generic VFS super_operations struct?
	 *  Because this is not cleared before we get called.
	 *  So we're overwriting something ... what?
	 */

	sb->s_op = &cmsfs_super_operations;
 
	/*  possibly correct the VFS superblock blocksize value  */
	if (sb->s_blocksize != cmssuper->pbksz)
	{
        	/*  set filesystem blocksize to device blocksize  */
        	sb->s_blocksize = get_hardsect_size(sb->s_dev);
	}
 
	/* -------------------------------------------------------------- *
	 *  Map the CMS volume (ADT) to CMS superblock.                   *
	 * -------------------------------------------------------------- */

	rc = cmsfs_map_ADT(cmssuper);
	if (rc != 0)
	{
		printk(KERN_WARNING "cmsfs_mount(): cmsfs_map_ADT() returned %d.\n",rc);
        	kfree(cmssuper);
        	sb->s_dev = 0;  /*  why?  */
        	unlock_super(sb);
        	MOD_DEC_USE_COUNT;
        	return NULL;
	}
 
	/* -------------------------------------------------------------- *
	 *  Check CMS directory inode allocated in the previous section.  *
	 * -------------------------------------------------------------- */
	if (cmssuper->cmsrooti == NULL)
	{
        	printk(KERN_ERR "cmsfs_mount(): did not allocate CMS dir inode.\n");
        	kfree(cmssuper);   /*  cmssuper == sb->u.generic_sbp  */
        	sb->u.generic_sbp = NULL;
        	sb->s_dev = 0;  /*  why?  */
        	unlock_super(sb);
        	MOD_DEC_USE_COUNT;
        	return NULL;
	}
	
	if (cmssuper->cmsrooti->cmssuper != cmssuper)
		BUG();
 
	/* -------------------------------------------------------------- *
	 *  Allocate and flesh-out a VFS inode for the directory.         *
	 *  This will be called "root" for this filesystem,               *
	 *  even though there will be no other directories.               *
	 *  This will be inode zero,  and is always the first FST.        *
	 * -------------------------------------------------------------- */

	vfsrooti = iget(sb,0);
	if (vfsrooti == NULL)
	{
        	printk(KERN_ERR "cmsfs_mount(): cannot get directory (root) inode.\n");
        	kfree(cmssuper);   /*  cmssuper == sb->u.generic_sbp  */
        	sb->u.generic_sbp = NULL;
        	sb->s_dev = 0;  /*  why?  */
        	unlock_super(sb);
        	MOD_DEC_USE_COUNT;
        	return NULL;
	}
 
	/*
	 *  Sanity check against iget()'s work:  it should have called
	 *  cmsfs_read_inode(),  which in turn should have cross-linked
	 *  the CMS and VFS inode structs and marked "inode zero" as root.
	 */

	if (cmssuper->cmsrooti->vfsinode != vfsrooti)
		BUG();
	 if (vfsrooti->u.generic_ip != cmssuper->cmsrooti)
	 	BUG();
 
	/* -------------------------------------------------------------- *
	 *  Allocate and flesh-out a directory entry (VFS dnode).         *
	 * -------------------------------------------------------------- */

	sb->s_root = d_alloc_root(cmssuper->cmsrooti->vfsinode);
	if (sb->s_root == NULL)
	{
        	printk(KERN_ERR "cmsfs_mount(): cannot allocate VFS directory structure.\n");
		kfree(cmssuper->cmsrooti->vfsinode);
		kfree(cmssuper->cmsrooti);
		cmssuper->cmsrooti = NULL;
		kfree(cmssuper);   /*  cmssuper == sb->u.generic_sbp  */
		sb->u.generic_sbp = NULL;
		sb->s_dev = 0;  /*  why?  */
		unlock_super(sb);
		MOD_DEC_USE_COUNT;
		return NULL;
	}
 
	/*  CMS FS mounts are always read-only to Linux  */
	sb->s_flags |= MS_RDONLY;
	sb->s_maxbytes = MAX_NON_LFS;
 
	unlock_super(sb);
	return sb;
}
 
/* ------------------------------------------------------------------ */

static struct file_system_type cms_fs_type = {
        "cms" ,
        FS_REQUIRES_DEV ,
        cmsfs_mount ,
	NULL
} ;
 
/* ---------------------------------------------------------- CMSFS_INIT
 *  filesystem init, especially useful for module init
 */

static int cmsfs_init(void)
{
	printk(KERN_INFO "%s %s\n",CMSFS_DESCRIPTION,CMSFS_VERSION);
	return register_filesystem(&cms_fs_type);
}
 
/* ------------------------------------------------------- CMSFS_CLEANUP
 *  filesystem cleanup & shut down for module extraction
 */

static void cmsfs_cleanup(void)
{
	unregister_filesystem(&cms_fs_type);
	return;
}
 
 
/* -------------------------------------------------------- CMSFS_MODULE
 *  additional routines required when CMS FS is built as a module
 */
 
MODULE_AUTHOR(CMSFS_AUTHOR);
MODULE_DESCRIPTION(CMSFS_DESCRIPTION);
EXPORT_NO_SYMBOLS;

module_init(cmsfs_init);
module_exit(cmsfs_cleanup); 
