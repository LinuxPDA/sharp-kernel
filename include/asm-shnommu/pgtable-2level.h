#ifndef __ASM_SH_PGTABLE_2LEVEL_H
#define __ASM_SH_PGTABLE_2LEVEL_H

/*
 * traditional two-level paging structure:
 */

#define PGDIR_SHIFT	22
#define PTRS_PER_PGD	1024

/*
 * this is two-level, so we don't really have any
 * PMD directory physically.
 */
#define PMD_SHIFT	22
#define PTRS_PER_PMD	1

#define PTRS_PER_PTE	1024

#define pte_ERROR(e) \
	printk("%s:%d: bad pte %08lx.\n", __FILE__, __LINE__, pte_val(e))
#define pmd_ERROR(e) \
	printk("%s:%d: bad pmd %08lx.\n", __FILE__, __LINE__, pmd_val(e))
#define pgd_ERROR(e) \
	printk("%s:%d: bad pgd %08lx.\n", __FILE__, __LINE__, pgd_val(e))

#define pgd_page(pgd) \
((unsigned long) __va(pgd_val(pgd) & PAGE_MASK))

#endif /* __ASM_SH_PGTABLE_2LEVEL_H */
