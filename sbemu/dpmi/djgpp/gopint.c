/* Copyright (C) 2015 DJ Delorie, see COPYING.DJ for details */
/* Copyright (C) 2007 DJ Delorie, see COPYING.DJ for details */
/* Copyright (C) 1996 DJ Delorie, see COPYING.DJ for details */
/* Copyright (C) 1995 DJ Delorie, see COPYING.DJ for details */
#include <dpmi.h>
#include <go32.h>
#include <stdlib.h>
#include <string.h>
#include <sys/exceptn.h>

/* We enter with only CS known, and SS on a locked 4K stack which
   is *NOT* our SS.  We must set up everthing, including a stack swap,
   then restore it the way we found it.   C. Sandmann 4-93 */

/*
 * NOTE: we now store some information in the stack interrupt stack
 *	long 0 = flags	(currently bit 0 = this stack was malloc()ed)
 *	long 1 = block count (if non-zero, users's function won't be called)
 *	long 2 = count of how many times this wrapper has been hit
 *			(note: count includes times when we're recursively
 *                             called and don't call the user's function)
 */

#define	STACK_WAS_MALLOCED	(1 << 0)

#define	FILL	0x00

static unsigned char wrapper_intcommon[] = {
/* 00 */ 0x1e,				/*     push ds              	*/
/* 01 */ 0x06,				/*     push es			*/
/* 02 */ 0x0f, 0xa0,			/*     push fs              	*/
/* 04 */ 0x0f, 0xa8,			/*     push gs              	*/
/* 06 */ 0x60,				/*     pusha                	*/
/* 07 */ 0x66, 0xb8,			/*     mov ax,			*/
/* 09 */ FILL, FILL,			/*         _our_selector	*/
/* 0B */ 0x8e, 0xd8,			/*     mov ds, ax           	*/
/* 0D */ 0xff, 0x05, 			/*     incl			*/
/* 0F */ FILL, FILL, FILL, FILL,	/*	    _call_count		*/
/* 13 */ 0x83, 0x3d,			/*     cmpl			*/
/* 15 */ FILL, FILL, FILL, FILL,	/*         _in_this_handler	*/
/* 19 */ 0x00,				/*         $0			*/
/* 1A */ 0x75,				/*     jne			*/
/* 1B */ 0x2F,				/*         bypass		*/
/* 1C */ 0xc6, 0x05,			/*     movb			*/
/* 1E */ FILL, FILL, FILL, FILL,	/*         _in_this_handler 	*/
/* 22 */ 0x01,				/*         $1			*/
/* 23 */ 0x8e, 0xc0,			/*     mov es, ax           	*/
/* 25 */ 0x8e, 0xe0,			/*     mov fs, ax           	*/
/* 27 */ 0x8e, 0xe8,			/*     mov gs, ax           	*/
/* 29 */ 0xbb,				/*     mov ebx,			*/
/* 2A */ FILL, FILL, FILL, FILL,	/*         _local_stack		*/
/* 2E */ 0xfc,				/*     cld                  	*/
/* 2F */ 0x89, 0xe1,			/*     mov ecx, esp         	*/
/* 31 */ 0x8c, 0xd2,			/*     mov dx, ss           	*/
/* 33 */ 0x8e, 0xd0,			/*     mov ss, ax           	*/
/* 35 */ 0x89, 0xdc,			/*     mov esp, ebx         	*/
/* 37 */ 0x52,				/*     push edx             	*/
/* 38 */ 0x51,				/*     push ecx             	*/
/* 39 */ 0xe8,				/*     call			*/
/* 3A */ FILL, FILL, FILL, FILL,	/*         _rmih		*/
/* 3E */ 0x58,				/*     pop eax                 	*/
/* 3F */ 0x5b,				/*     pop ebx                 	*/
/* 40 */ 0x8e, 0xd3,			/*     mov ss, bx               */
/* 42 */ 0x89, 0xc4,			/*     mov esp, eax             */
/* 44 */ 0xc6, 0x05,			/*     movb			*/
/* 46 */ FILL, FILL, FILL, FILL,	/*         _in_this_handler	*/
/* 4A */ 0x00,				/*         $0			*/
/* 4B */ 0x61,				/* bypass:  popa		*/
/* 4C */ 0x90,				/*     nop			*/
/* 4D */ 0x0f, 0xa9,			/*     pop gs                   */
/* 4F */ 0x0f, 0xa1,			/*     pop fs                   */
/* 51 */ 0x07,				/*     pop es                   */
/* 52 */ 0x1f				/*     pop ds                   */
};

static unsigned char wrapper_intiret[] = {
/* 53 */ 0xcf,				/*     iret                     */
};

static unsigned char wrapper_intchain[] = {
/* 53 */ 0x2e, 0xff, 0x2d,		/*     jmp     cs: 		*/
/* 56 */ FILL, FILL, FILL, FILL,	/*     [_old_int+39] 		*/
/* 5A */ 0xcf,				/*     iret                     */
/* 5B */ FILL, FILL, FILL, FILL,	/*     old_address 		*/
/* 5F */ FILL, FILL,			/*     old_segment 		*/
};

unsigned long _go32_interrupt_stack_size = 32256;

int _go32_dpmi_lock_data( void *lockaddr, unsigned long locksize )
    {
    unsigned long baseaddr;
    __dpmi_meminfo memregion;

    if( __dpmi_get_segment_base_address( _go32_my_ds(), &baseaddr) == -1 ) return( -1 );

    memset( &memregion, 0, sizeof(memregion) );

    memregion.address = baseaddr + (unsigned long) lockaddr;
    memregion.size    = locksize;

    if( __dpmi_lock_linear_region( &memregion ) == -1 ) return( -1 );

    return( 0 );
    }

int _go32_dpmi_lock_code( void *lockaddr, unsigned long locksize )
    {
    unsigned long baseaddr;
    __dpmi_meminfo memregion;

    if( __dpmi_get_segment_base_address( _go32_my_cs(), &baseaddr) == -1 ) return( -1 );

    memset( &memregion, 0, sizeof(memregion) );

    memregion.address = baseaddr + (unsigned long) lockaddr;
    memregion.size    = locksize;

    if( __dpmi_lock_linear_region( &memregion ) == -1 ) return( -1 );

    return( 0 );
    }

static int _go32_dpmi_chain_protected_mode_interrupt_vector_with_stack(int vector,
    _go32_dpmi_seginfo *info, unsigned char *stack, unsigned long stack_length)
{
  unsigned char *wrapper;
  __dpmi_paddr pm_int;

#define	CHECK_STACK()							\
  if ((stack_length && stack_length < 512) ||				\
		(!stack_length && _go32_interrupt_stack_size < 512))	\
    return 0x8015

  CHECK_STACK();

  wrapper = (unsigned char *)malloc(sizeof(wrapper_intcommon) +
						     sizeof(wrapper_intchain));
  if (wrapper == 0)
    return 0x8015;

  if( _go32_dpmi_lock_data( wrapper,
    sizeof(wrapper_intcommon) + sizeof(wrapper_intchain)) ) return 0x8015;

#define	MALLOC_STACK()					\
  do {							\
      if (!stack_length) {				\
	  stack_length = _go32_interrupt_stack_size;	\
	  stack = (unsigned char *)malloc(stack_length);\
	  if (stack == 0) {				\
	    free(wrapper);				\
	    return 0x8015;				\
	  }						\
          if( _go32_dpmi_lock_data( stack,              \
            stack_length) ) return 0x8015;              \
	  ((long *)stack)[0] = STACK_WAS_MALLOCED;	\
      } else						\
	  ((long *)stack)[0] = 0;			\
      ((long *)stack)[1] = 0;				\
      ((long *)stack)[2] = 0;				\
  } while (0)

  MALLOC_STACK();

  __dpmi_get_protected_mode_interrupt_vector(vector, &pm_int);

  memcpy(wrapper, wrapper_intcommon, sizeof(wrapper_intcommon));
  memcpy(wrapper+sizeof(wrapper_intcommon), wrapper_intchain,
						     sizeof(wrapper_intchain));

#define	FILL_INT_WRAPPER()						\
  *(short *)(wrapper+0x09) = __djgpp_ds_alias;				\
  *(long  *)(wrapper+0x0F) = (long) stack + 8;				\
  *(long  *)(wrapper+0x15) = (long) stack + 4;				\
  *(long  *)(wrapper+0x1E) = (long) stack + 4;				\
  *(long  *)(wrapper+0x2A) = (long) stack + stack_length;		\
  *(long  *)(wrapper+0x3A) = info->pm_offset - ((long)wrapper + 0x3E);	\
  *(long  *)(wrapper+0x46) = (long) stack + 4

  FILL_INT_WRAPPER();

  *(long  *)(wrapper+0x56) = (long) wrapper + 0x5B;
  *(long  *)(wrapper+0x5B) = pm_int.offset32;
  *(short *)(wrapper+0x5F) = pm_int.selector;

  pm_int.offset32 = (int)wrapper;
  pm_int.selector = _my_cs();
  __dpmi_set_protected_mode_interrupt_vector(vector, &pm_int);
  return 0;
}

int _go32_dpmi_chain_protected_mode_interrupt_vector(int vector,
						      _go32_dpmi_seginfo *info)
{
    return _go32_dpmi_chain_protected_mode_interrupt_vector_with_stack(vector,
						 info, (unsigned char *) 0, 0);
}

static int _go32_dpmi_allocate_iret_wrapper_with_stack(_go32_dpmi_seginfo *info,
			      unsigned char *stack, unsigned long stack_length)
{
  unsigned char *wrapper;

  CHECK_STACK();

  wrapper = (unsigned char *)malloc(sizeof(wrapper_intcommon) +
						     sizeof(wrapper_intiret));

  if (wrapper == 0)
    return 0x8015;

  if( _go32_dpmi_lock_data( wrapper,
    sizeof(wrapper_intcommon) + sizeof(wrapper_intiret)) ) return 0x8015;

  MALLOC_STACK();

  memcpy(wrapper, wrapper_intcommon, sizeof(wrapper_intcommon));
  memcpy(wrapper+sizeof(wrapper_intcommon), wrapper_intiret,
	 sizeof(wrapper_intiret));

  FILL_INT_WRAPPER();

  info->pm_offset = (int)wrapper;
  info->pm_selector = _my_cs();
  return 0;
}

int _go32_dpmi_allocate_iret_wrapper(_go32_dpmi_seginfo *info)
{
    return _go32_dpmi_allocate_iret_wrapper_with_stack(info,
							(unsigned char *)0, 0);
}

int _go32_dpmi_free_iret_wrapper(_go32_dpmi_seginfo *info)
{
  char *stack;
  char *wrapper = (char *)info->pm_offset;

  stack = (char *)(*(long *)(wrapper+0x0F) - 8);
  if (*(long *) stack & STACK_WAS_MALLOCED)
      free(stack);
  free(wrapper);
  return 0;
}
