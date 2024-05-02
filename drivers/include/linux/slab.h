#ifndef SBEMU_LINUX_SLAB_H
#define SBEMU_LINUX_SLAB_H

extern void pds_memxch(char *,char *,unsigned int);
extern void *pds_malloc(unsigned int bufsize);
extern void *pds_zalloc(unsigned int bufsize);
extern void *pds_calloc(unsigned int nitems,unsigned int itemsize);
extern void *pds_realloc(void *bufptr,unsigned int bufsize);
extern void pds_free(void *bufptr);

#define kmalloc(size,flags) pds_malloc(size)
#define kcalloc(n,size,flags) pds_calloc(n,size)
#define kzalloc(size,flags) pds_zalloc(size) /* zero */
#define kfree(p) pds_free((void *)p)
#define vmalloc(size) pds_malloc(size)
#define vfree(p) pds_free(p)
#define devm_kzalloc(dev,size,flags) pds_zalloc(size) /* zero */
#define devm_kcalloc(dev,n,size,flags) pds_calloc(n,size)
#define devm_kfree(dev,p) pds_free((void *)p)

#include "linux/types.h"

/**
 * kmalloc_array - allocate memory for an array.
 * @n: number of elements.
 * @size: element size.
 * @flags: the type of memory to allocate (see kmalloc).
 */
static inline void *kmalloc_array(size_t n, size_t size, gfp_t flags)
{
  size_t bytes = n * size; // XXX no overflow checking
  return kmalloc(bytes, flags);
}
static inline void *kmalloc_array_node(size_t n, size_t size, gfp_t flags, int node)
{
  size_t bytes = n * size; // XXX no overflow checking
  return kmalloc(bytes, flags);
}

/*
 * This struct defines a memory VMM memory area. There is one of these
 * per VM-area/task.  A VM area is any part of the process virtual memory
 * space that has a special rule for the page-fault handlers (ie a shared
 * library, the executable area etc).
 */
struct vm_area_struct {
	/* The first cache line has the info for VMA tree walking. */

	unsigned long vm_start;		/* Our start address within vm_mm. */
	unsigned long vm_end;		/* The first byte after our end address
					   within vm_mm. */

	/* linked list of VM areas per task, sorted by address */
	struct vm_area_struct *vm_next, *vm_prev;

  #if 0
	struct rb_node vm_rb;

	/*
	 * Largest free memory gap in bytes to the left of this VMA.
	 * Either between this VMA and vma->vm_prev, or between one of the
	 * VMAs below us in the VMA rbtree and its ->vm_prev. This helps
	 * get_unmapped_area find a free area of the right size.
	 */
	unsigned long rb_subtree_gap;

	/* Second cache line starts here. */

	struct mm_struct *vm_mm;	/* The address space we belong to. */
	pgprot_t vm_page_prot;		/* Access permissions of this VMA. */
	unsigned long vm_flags;		/* Flags, see mm.h. */

	/*
	 * For areas with an address space and backing store,
	 * linkage into the address_space->i_mmap interval tree.
	 */
	struct {
		struct rb_node rb;
		unsigned long rb_subtree_last;
	} shared;

	/*
	 * A file's MAP_PRIVATE vma can be in both i_mmap tree and anon_vma
	 * list, after a COW of one of the file pages.	A MAP_SHARED vma
	 * can only be in the i_mmap tree.  An anonymous MAP_PRIVATE, stack
	 * or brk vma (with NULL file) can only be in an anon_vma list.
	 */
	struct list_head anon_vma_chain; /* Serialized by mmap_sem &
					  * page_table_lock */
	struct anon_vma *anon_vma;	/* Serialized by page_table_lock */

	/* Function pointers to deal with this struct. */
	const struct vm_operations_struct *vm_ops;

	/* Information about our backing store: */
	unsigned long vm_pgoff;		/* Offset (within vm_file) in PAGE_SIZE
					   units */
	struct file * vm_file;		/* File we map to (can be NULL). */
	struct file *vm_prfile;		/* shadow of vm_file */
#endif
	void * vm_private_data;		/* was vm_pte (shared mem) */
#if 0
#ifndef CONFIG_MMU
	struct vm_region *vm_region;	/* NOMMU mapping region */
#endif
#ifdef CONFIG_NUMA
	struct mempolicy *vm_policy;	/* NUMA policy for the VMA */
#endif
	struct vm_userfaultfd_ctx vm_userfaultfd_ctx;
#endif
};

enum {
        REGION_INTERSECTS,
        REGION_DISJOINT,
        REGION_MIXED,
};

#endif
