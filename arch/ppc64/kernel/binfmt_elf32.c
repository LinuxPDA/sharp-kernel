/*
 * binfmt_elf32.c: Support 32-bit PPC ELF binaries on Power3 and followons.
 * based on the SPARC64 version.
 * Copyright (C) 1995, 1996, 1997, 1998 David S. Miller	(davem@redhat.com)
 * Copyright (C) 1995, 1996, 1997, 1998 Jakub Jelinek	(jj@ultra.linux.cz)
 *
 * Copyright (C) 2000,2001 Ken Aaker (kdaaker@rchland.vnet.ibm.com), IBM Corp
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#define ELF_ARCH		EM_PPC
#define ELF_CLASS		ELFCLASS32
#define ELF_DATA		ELFDATA2MSB;

#include <asm/processor.h>
#include <linux/module.h>
#include <linux/config.h>
#include <linux/elfcore.h>

struct timeval32
{
	int tv_sec, tv_usec;
};

#define elf_prstatus elf_prstatus32
struct elf_prstatus32
{
	struct elf_siginfo pr_info;	/* Info associated with signal */
	short	pr_cursig;		/* Current signal */
	unsigned int pr_sigpend;	/* Set of pending signals */
	unsigned int pr_sighold;	/* Set of held signals */
	pid_t	pr_pid;
	pid_t	pr_ppid;
	pid_t	pr_pgrp;
	pid_t	pr_sid;
	struct timeval32 pr_utime;	/* User time */
	struct timeval32 pr_stime;	/* System time */
	struct timeval32 pr_cutime;	/* Cumulative user time */
	struct timeval32 pr_cstime;	/* Cumulative system time */
	elf_gregset_t pr_reg;		/* General purpose registers. */
	int pr_fpvalid;		/* True if math co-processor being used. */
};


#define elf_prpsinfo elf_prpsinfo32
struct elf_prpsinfo32
{
	char	pr_state;	/* numeric process state */
	char	pr_sname;	/* char for pr_state */
	char	pr_zomb;	/* zombie */
	char	pr_nice;	/* nice val */
    char    pr_pad[4];  /* pad to put pr_flag on 8 byte boundary */
	unsigned int pr_flag;	/* flags */
	u16	pr_uid;
	u16	pr_gid;
	pid_t	pr_pid, pr_ppid, pr_pgrp, pr_sid;
	/* Lots missing */
	char	pr_fname[16];	/* filename of executable */
	char	pr_psargs[ELF_PRARGSZ];	/* initial part of arg list */
};

#include <linux/highuid.h>

#undef NEW_TO_OLD_UID
#undef NEW_TO_OLD_GID
#define NEW_TO_OLD_UID(uid) ((uid) > 65535) ? (u16)overflowuid : (u16)(uid)
#define NEW_TO_OLD_GID(gid) ((gid) > 65535) ? (u16)overflowgid : (u16)(gid)

#undef start_thread
#define start_thread start_thread32

#define elf_debug elf_debug_elf32
#define set_brk set_brk_elf32
#define padzero padzero_elf32
#define create_elf_tables create_elf_tables_elf32
// #define elf_map elf_map_elf32
#define load_elf_interp load_elf_interp_elf32
#define load_aout_interp load_aout_interp_elf32
#define elf_format elf_format_elf32
#define load_elf_binary load_elf_binary_elf32
#define load_elf_library load_elf_library_elf32
#define dump_write dump_write_elf32
#define dump_seek dump_seek_elf32
#define maydump maydump_elf32
#define notesize notesize_elf32
#define dump_regs dump_regs_elf32
#define writenote writenote_elf32
#define elf_core_dump elf_core_dump_elf32
#define init_elf_binfmt init_elf_binfmt_elf32
#define exit_elf_binfmt exit_elf_binfmt_elf32


#undef CONFIG_BINFMT_ELF
#ifdef CONFIG_BINFMT_ELF32
#define CONFIG_BINFMT_ELF CONFIG_BINFMT_ELF32
#endif
#undef CONFIG_BINFMT_ELF_MODULE
#ifdef CONFIG_BINFMT_ELF32_MODULE
#define CONFIG_BINFMT_ELF_MODULE CONFIG_BINFMT_ELF32_MODULE
#endif

MODULE_DESCRIPTION("Binary format loader for compatibility with 32bit Linux PPC binaries on the Power3+");
MODULE_AUTHOR("Eric Youngdale, David S. Miller, Jakub Jelinek, Ken Aaker");

#undef MODULE_DESCRIPTION
#undef MODULE_AUTHOR

#include "../../../fs/binfmt_elf.c"
