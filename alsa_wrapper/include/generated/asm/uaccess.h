/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _HACK_ASM_X86_UACCESS_H
#define _HACK_ASM_X86_UACCESS_H
/*
 * Hack for User space memory access functions
 * pcm_native will access user space, but not compile for djgpp's bintuils (AS)
 * with error: Error: unknown pseudo-op: `.pushsection'/`.popsection'
 * this file will override arch/x86/include/asm/uaccess.h with include/asm-generic/uaccess.h
 */

#include <linux/compiler_attributes.h>

static __always_inline unsigned long __must_check
raw_copy_to_user(void __user *to, const void *from, unsigned long n);

static __always_inline unsigned long
raw_copy_from_user(void *to, const void __user *from, unsigned long n);

static inline __must_check unsigned long
copy_from_user_inatomic_nontemporal(void *to, const void __user *from,
				  unsigned long n);

#include <asm-generic/uaccess.h>

#include <asm/uaccess_32.h>


#endif /* _HACK_ASM_X86_UACCESS_H */

