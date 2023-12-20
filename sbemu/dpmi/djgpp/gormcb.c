/* Copyright (C) 2015 DJ Delorie, see COPYING.DJ for details */
/* Copyright (C) 2007 DJ Delorie, see COPYING.DJ for details */
/* Copyright (C) 1999 DJ Delorie, see COPYING.DJ for details */
/* Copyright (C) 1998 DJ Delorie, see COPYING.DJ for details */
/* Copyright (C) 1996 DJ Delorie, see COPYING.DJ for details */
/* Copyright (C) 1995 DJ Delorie, see COPYING.DJ for details */
#include <dpmi.h>
#include <go32.h>
#include <stdlib.h>
#include <string.h>
#include <sys/exceptn.h>

/* This code really can't be nested since the RMCB structure isn't copied,
   so the stack check isn't really useful.  But someone may fix it someday.
   On entry CS is known to be ours, ES is probably ours (since we passed it),
   SS:ESP is locked 4K stack.  ES:EDI is regs structure, DS:ESI is RM SS:SP.
   Do NOT enable interrupts in the user routine.  Thanks to ctm@ardi.com for
   the improvements.  C. Sandmann 3/95 */

/* Keeps FLAGS untouched on IRET (new line 83), 
   useful for HW interrupt handlers. Crazii 12/20/2023 */

#define	STACK_WAS_MALLOCED	(1 << 0)

#define	FILL	0x00

static unsigned char wrapper_common[] = {
/* 00 */ 0x06,				/*     push    es               */
/* 01 */ 0x1e,				/*     push    ds               */
/* 02 */ 0x06,				/*     push    es               */
/* 03 */ 0x1f,				/*     pop     ds               */
/* 04 */ 0x66, 0xb8,			/*     mov ax,			*/
/* 06 */ FILL, FILL,			/*         _our_selector	*/
/* 08 */ 0x8e, 0xd8,			/*     mov ds, ax           	*/
/* 0a */ 0xff, 0x05, 			/*     incl			*/
/* 0c */ FILL, FILL, FILL, FILL,	/*	    _call_count		*/
/* 10 */ 0x83, 0x3d,			/*     cmpl			*/
/* 12 */ FILL, FILL, FILL, FILL,	/*         _in_this_handler	*/
/* 16 */ 0x00,				/*         $0			*/
/* 17 */ 0x75,				/*     jne			*/
/* 18 */ 0x33,				/*         bypass		*/
/* 19 */ 0xc6, 0x05,			/*     movb			*/
/* 1b */ FILL, FILL, FILL, FILL,	/*         _in_this_handler 	*/
/* 1f */ 0x01,				/*         $1			*/
/* 20 */ 0x8e, 0xc0,			/*     mov es, ax           	*/
/* 22 */ 0x8e, 0xe0,			/*     mov fs, ax           	*/
/* 24 */ 0x8e, 0xe8,			/*     mov gs, ax           	*/
/* 26 */ 0xbb,				/*     mov ebx,			*/
/* 27 */ FILL, FILL, FILL, FILL,	/*         _local_stack		*/
/* 2b */ 0xfc,				/*     cld                  	*/
/* 2c */ 0x89, 0xe1,			/*     mov ecx, esp         	*/
/* 2e */ 0x8c, 0xd2,			/*     mov dx, ss           	*/
/* 30 */ 0x8e, 0xd0,			/*     mov ss, ax           	*/
/* 32 */ 0x89, 0xdc,			/*     mov esp, ebx         	*/
/* 34 */ 0x52,				/*     push edx             	*/
/* 35 */ 0x51,				/*     push ecx             	*/
/* 36 */ 0x56,				/*     push esi                 */
/* 37 */ 0x57,				/*     push edi                 */
/* 38 */ 0xe8,				/*     call		        */
/* 39 */ FILL, FILL, FILL, FILL,	/*         _rmcb                */
/* 3d */ 0x5f,				/*     pop edi                  */
/* 3e */ 0x5e,				/*     pop esi                  */
/* 3f */ 0x58,				/*     pop eax                 	*/
/* 40 */ 0x5b,				/*     pop ebx                 	*/
/* 41 */ 0x8e, 0xd3,			/*     mov ss, bx               */
/* 43 */ 0x89, 0xc4,			/*     mov esp, eax             */
/* 45 */ 0xc6, 0x05,			/*     movb			*/
/* 47 */ FILL, FILL, FILL, FILL,	/*         _in_this_handler	*/
/* 4b */ 0x00,				/*         $0			*/
/* 4c */ 0x1f,			        /* bypass:  pop ds              */
/* 4d */ 0x07,				/*     pop es                   */

/* 4e */ 0x8b, 0x06,			/*     mov eax,[esi]            */
/* 50 */ 0x26, 0x89, 0x47, 0x2a,	/*     mov es:[edi+42],eax      */
};

static unsigned char wrapper_retf[] = {
         0x66, 0x26, 0x83, 0x47, 0x2e, 0x04,	/* add     es:[edi+46],0x4 */
         0xcf				    	/* iret                    */
};

static unsigned char wrapper_iret[] = {
#if 1
  /* This overwrote the FLAGS in the real-mode call structure with
     their original value, thus making it impossible for the user's
     RMCB to change FLAGS (e.g., to set/reset the carry bit).  */
         0x66, 0x8b, 0x46, 0x04,		/* mov     ax,[esi+4]      */
         0x66, 0x26, 0x89, 0x47, 0x20,		/* mov     es:[edi+32],ax  */
#endif
         0x66, 0x26, 0x83, 0x47, 0x2e, 0x06,	/* add     es:[edi+46],0x6 */
         0xcf					/* iret                    */
};

unsigned long _go32_rmcb_stack_size = 32256;

static int setup_rmcb(unsigned char *wrapper, _go32_dpmi_seginfo *info,
  __dpmi_regs *regs, unsigned char *stack, unsigned long stack_length)
{
#define	MALLOC_STACK()					\
  do {							\
      if (!stack_length) {				\
	  stack_length = _go32_rmcb_stack_size;		\
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
  if( _go32_dpmi_lock_data(regs, sizeof(__dpmi_regs)))
    return 0x8015;

  *(short *)(wrapper+0x06) = __djgpp_ds_alias;
  *(long  *)(wrapper+0x0c) = (long) stack + 8;
  *(long  *)(wrapper+0x12) = (long) stack + 4;
  *(long  *)(wrapper+0x1b) = (long) stack + 4;
  *(long  *)(wrapper+0x27) = (long) stack + stack_length;
  *(long  *)(wrapper+0x39) = info->pm_offset - ((long)wrapper + 0x3d);
  *(long  *)(wrapper+0x47) = (long) stack + 4;

  info->size = (int)wrapper;

  return __dpmi_allocate_real_mode_callback((void *)wrapper, regs,
                                            (__dpmi_raddr *)&info->rm_offset);
}


static int _go32_dpmi_allocate_real_mode_callback_retf_with_stack(
    _go32_dpmi_seginfo *info, __dpmi_regs *regs, unsigned char *stack,
						    unsigned long stack_length)
{
  unsigned char *wrapper;

#define	CHECK_STACK()							\
  if ((stack_length && stack_length < 512) ||				\
		(!stack_length && _go32_rmcb_stack_size < 512))	\
    return 0x8015

  CHECK_STACK();

  wrapper = (unsigned char *)malloc(sizeof(wrapper_common) +
						         sizeof(wrapper_retf));
  if (wrapper == 0)
    return 0x8015;

  if( _go32_dpmi_lock_data( wrapper,
    sizeof(wrapper_common) + sizeof(wrapper_retf)) ) return 0x8015;

  memcpy(wrapper, wrapper_common, sizeof(wrapper_common));
  memcpy(wrapper+sizeof(wrapper_common), wrapper_retf, sizeof(wrapper_retf));

  return setup_rmcb(wrapper, info, regs, stack, stack_length);
}

int _go32_dpmi_allocate_real_mode_callback_retf(_go32_dpmi_seginfo *info,
						__dpmi_regs *regs)
{
    return _go32_dpmi_allocate_real_mode_callback_retf_with_stack
      (info, regs, (unsigned char *) 0, 0);
}

static int _go32_dpmi_allocate_real_mode_callback_iret_with_stack(
                              _go32_dpmi_seginfo *info, __dpmi_regs *regs,
                              unsigned char *stack, unsigned long stack_length)
{
  unsigned char *wrapper;

  CHECK_STACK();

  wrapper = (unsigned char *)malloc(sizeof(wrapper_common) +
						         sizeof(wrapper_iret));
  if (wrapper == 0)
    return 0x8015;

  if( _go32_dpmi_lock_data( wrapper,
    sizeof(wrapper_common) + sizeof(wrapper_iret)) ) return 0x8015;

  memcpy(wrapper, wrapper_common, sizeof(wrapper_common));
  memcpy(wrapper+sizeof(wrapper_common), wrapper_iret, sizeof(wrapper_iret));

  return setup_rmcb(wrapper, info, regs, stack, stack_length);
}

int _go32_dpmi_allocate_real_mode_callback_iret(_go32_dpmi_seginfo *info,
						    __dpmi_regs *regs)
{
    return _go32_dpmi_allocate_real_mode_callback_iret_with_stack(info, regs,
						       (unsigned char *) 0, 0);
}

int _go32_dpmi_free_real_mode_callback(_go32_dpmi_seginfo *info)
{
  unsigned char *stack;

  stack = (unsigned char *)(*(long *)((long) info->size+0x12) - 4);
  if (*(long *) stack & STACK_WAS_MALLOCED)
      free(stack);

  free((char *)info->size);
  return __dpmi_free_real_mode_callback((__dpmi_raddr *)&info->rm_offset);
}
