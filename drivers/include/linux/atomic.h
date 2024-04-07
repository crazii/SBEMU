#ifndef _ASM_GENERIC_BITOPS_ATOMIC_H_
#define _ASM_GENERIC_BITOPS_ATOMIC_H_

#include "linux/types.h"
#include "linux/bits.h"
//#include <asm/types.h>
//#include <linux/irqflags.h>

#ifdef CONFIG_SMP
#include <asm/spinlock.h>
#include <asm/cache.h>		/* we use L1_CACHE_BYTES */

/* Use an array of spinlocks for our atomic_ts.
 * Hash function to index into a different SPINLOCK.
 * Since "a" is usually an address, use one spinlock per cacheline.
 */
#  define ATOMIC_HASH_SIZE 4
#  define ATOMIC_HASH(a) (&(__atomic_hash[ (((unsigned long) a)/L1_CACHE_BYTES) & (ATOMIC_HASH_SIZE-1) ]))

extern arch_spinlock_t __atomic_hash[ATOMIC_HASH_SIZE] __lock_aligned;

/* Can't use raw_spin_lock_irq because of #include problems, so
 * this is the substitute */
#define _atomic_spin_lock_irqsave(l,f) do {	\
	arch_spinlock_t *s = ATOMIC_HASH(l);	\
	local_irq_save(f);			\
	arch_spin_lock(s);			\
} while(0)

#define _atomic_spin_unlock_irqrestore(l,f) do {	\
	arch_spinlock_t *s = ATOMIC_HASH(l);		\
	arch_spin_unlock(s);				\
	local_irq_restore(f);				\
} while(0)


#else
#define local_irq_save(f) /*XXX*/
#define local_irq_restore(f) /*XXX*/
#  define _atomic_spin_lock_irqsave(l,f) do { local_irq_save(f); } while (0)
#  define _atomic_spin_unlock_irqrestore(l,f) do { local_irq_restore(f); } while (0)
#endif

/*
 * NMI events can occur at any time, including when interrupts have been
 * disabled by *_irqsave().  So you can get NMI events occurring while a
 * *_bit function is holding a spin lock.  If the NMI handler also wants
 * to do bit manipulation (and they do) then you can get a deadlock
 * between the original caller of *_bit() and the NMI handler.
 *
 * by Keith Owens
 */

/**
 * set_bit - Atomically set a bit in memory
 * @nr: the bit to set
 * @addr: the address to start counting from
 *
 * This function is atomic and may not be reordered.  See __set_bit()
 * if you do not require the atomic guarantees.
 *
 * Note: there are no guarantees that this function will not be reordered
 * on non x86 architectures, so if you are writing portable code,
 * make sure not to rely on its reordering guarantees.
 *
 * Note that @nr may be almost arbitrarily large; this function is not
 * restricted to acting on a single-word quantity.
 */
static inline void set_bit(int nr, volatile unsigned long *addr)
{
	unsigned long mask = BIT_MASK(nr);
	unsigned long *p = ((unsigned long *)addr) + BIT_WORD(nr);
	unsigned long flags;

	_atomic_spin_lock_irqsave(p, flags);
	*p  |= mask;
	_atomic_spin_unlock_irqrestore(p, flags);
}

/**
 * clear_bit - Clears a bit in memory
 * @nr: Bit to clear
 * @addr: Address to start counting from
 *
 * clear_bit() is atomic and may not be reordered.  However, it does
 * not contain a memory barrier, so if it is used for locking purposes,
 * you should call smp_mb__before_atomic() and/or smp_mb__after_atomic()
 * in order to ensure changes are visible on other processors.
 */
static inline void clear_bit(int nr, volatile unsigned long *addr)
{
	unsigned long mask = BIT_MASK(nr);
	unsigned long *p = ((unsigned long *)addr) + BIT_WORD(nr);
	unsigned long flags;

	_atomic_spin_lock_irqsave(p, flags);
	*p &= ~mask;
	_atomic_spin_unlock_irqrestore(p, flags);
}

/**
 * change_bit - Toggle a bit in memory
 * @nr: Bit to change
 * @addr: Address to start counting from
 *
 * change_bit() is atomic and may not be reordered. It may be
 * reordered on other architectures than x86.
 * Note that @nr may be almost arbitrarily large; this function is not
 * restricted to acting on a single-word quantity.
 */
static inline void change_bit(int nr, volatile unsigned long *addr)
{
	unsigned long mask = BIT_MASK(nr);
	unsigned long *p = ((unsigned long *)addr) + BIT_WORD(nr);
	unsigned long flags;

	_atomic_spin_lock_irqsave(p, flags);
	*p ^= mask;
	_atomic_spin_unlock_irqrestore(p, flags);
}

/**
 * test_and_set_bit - Set a bit and return its old value
 * @nr: Bit to set
 * @addr: Address to count from
 *
 * This operation is atomic and cannot be reordered.
 * It may be reordered on other architectures than x86.
 * It also implies a memory barrier.
 */
static inline int test_and_set_bit(int nr, volatile unsigned long *addr)
{
	unsigned long mask = BIT_MASK(nr);
	unsigned long *p = ((unsigned long *)addr) + BIT_WORD(nr);
	unsigned long old;
	unsigned long flags;

	_atomic_spin_lock_irqsave(p, flags);
	old = *p;
	*p = old | mask;
	_atomic_spin_unlock_irqrestore(p, flags);

	return (old & mask) != 0;
}

/**
 * test_and_clear_bit - Clear a bit and return its old value
 * @nr: Bit to clear
 * @addr: Address to count from
 *
 * This operation is atomic and cannot be reordered.
 * It can be reorderdered on other architectures other than x86.
 * It also implies a memory barrier.
 */
static inline int test_and_clear_bit(int nr, volatile unsigned long *addr)
{
	unsigned long mask = BIT_MASK(nr);
	unsigned long *p = ((unsigned long *)addr) + BIT_WORD(nr);
	unsigned long old;
	unsigned long flags;

	_atomic_spin_lock_irqsave(p, flags);
	old = *p;
	*p = old & ~mask;
	_atomic_spin_unlock_irqrestore(p, flags);

	return (old & mask) != 0;
}

/**
 * test_and_change_bit - Change a bit and return its old value
 * @nr: Bit to change
 * @addr: Address to count from
 *
 * This operation is atomic and cannot be reordered.
 * It also implies a memory barrier.
 */
static inline int test_and_change_bit(int nr, volatile unsigned long *addr)
{
	unsigned long mask = BIT_MASK(nr);
	unsigned long *p = ((unsigned long *)addr) + BIT_WORD(nr);
	unsigned long old;
	unsigned long flags;

	_atomic_spin_lock_irqsave(p, flags);
	old = *p;
	*p = old ^ mask;
	_atomic_spin_unlock_irqrestore(p, flags);

	return (old & mask) != 0;
}

#endif /* _ASM_GENERIC_BITOPS_ATOMIC_H */

/*
 * Generic C implementation of atomic counter operations. Usable on
 * UP systems only. Do not include in machine independent code.
 *
 * Originally implemented for MN10300.
 *
 * Copyright (C) 2007 Red Hat, Inc. All Rights Reserved.
 * Written by David Howells (dhowells@redhat.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public Licence
 * as published by the Free Software Foundation; either version
 * 2 of the Licence, or (at your option) any later version.
 */
#ifndef __ASM_GENERIC_ATOMIC_H
#define __ASM_GENERIC_ATOMIC_H

//#include <asm/cmpxchg.h>
//#include <asm/barrier.h>

/*
 * atomic_$op() - $op integer to atomic variable
 * @i: integer value to $op
 * @v: pointer to the atomic variable
 *
 * Atomically $ops @i to @v. Does not strictly guarantee a memory-barrier, use
 * smp_mb__{before,after}_atomic().
 */

/*
 * atomic_$op_return() - $op interer to atomic variable and returns the result
 * @i: integer value to $op
 * @v: pointer to the atomic variable
 *
 * Atomically $ops @i to @v. Does imply a full memory barrier.
 */

#ifdef CONFIG_SMP

/* we can build all atomic primitives from cmpxchg */

#define ATOMIC_OP(op, c_op)						\
static inline void atomic_##op(int i, atomic_t *v)			\
{									\
	int c, old;							\
									\
	c = v->counter;							\
	while ((old = cmpxchg(&v->counter, c, c c_op i)) != c)		\
		c = old;						\
}

#define ATOMIC_OP_RETURN(op, c_op)					\
static inline int atomic_##op##_return(int i, atomic_t *v)		\
{									\
	int c, old;							\
									\
	c = v->counter;							\
	while ((old = cmpxchg(&v->counter, c, c c_op i)) != c)		\
		c = old;						\
									\
	return c c_op i;						\
}

#define ATOMIC_FETCH_OP(op, c_op)					\
static inline int atomic_fetch_##op(int i, atomic_t *v)			\
{									\
	int c, old;							\
									\
	c = v->counter;							\
	while ((old = cmpxchg(&v->counter, c, c c_op i)) != c)		\
		c = old;						\
									\
	return c;							\
}

#else

#define raw_local_irq_save(f) /*XXX*/
#define raw_local_irq_restore(f) /*XXX*/
//#include <linux/irqflags.h>

#define ATOMIC_OP(op, c_op)						\
static inline void atomic_##op(int i, atomic_t *v)			\
{									\
	unsigned long flags;						\
									\
	raw_local_irq_save(flags);					\
	v->counter = v->counter c_op i;					\
	raw_local_irq_restore(flags);					\
}

#define ATOMIC_OP_RETURN(op, c_op)					\
static inline int atomic_##op##_return(int i, atomic_t *v)		\
{									\
	unsigned long flags;						\
	int ret;							\
									\
	raw_local_irq_save(flags);					\
	ret = (v->counter = v->counter c_op i);				\
	raw_local_irq_restore(flags);					\
									\
	return ret;							\
}

#define ATOMIC_FETCH_OP(op, c_op)					\
static inline int atomic_fetch_##op(int i, atomic_t *v)			\
{									\
	unsigned long flags;						\
	int ret;							\
									\
	raw_local_irq_save(flags);					\
	ret = v->counter;						\
	v->counter = v->counter c_op i;					\
	raw_local_irq_restore(flags);					\
									\
	return ret;							\
}

#endif /* CONFIG_SMP */

#ifndef atomic_add_return
ATOMIC_OP_RETURN(add, +)
#endif

#ifndef atomic_sub_return
ATOMIC_OP_RETURN(sub, -)
#endif

#ifndef atomic_fetch_add
ATOMIC_FETCH_OP(add, +)
#endif

#ifndef atomic_fetch_sub
ATOMIC_FETCH_OP(sub, -)
#endif

#ifndef atomic_fetch_and
ATOMIC_FETCH_OP(and, &)
#endif

#ifndef atomic_fetch_or
ATOMIC_FETCH_OP(or, |)
#endif

#ifndef atomic_fetch_xor
ATOMIC_FETCH_OP(xor, ^)
#endif

#ifndef atomic_and
ATOMIC_OP(and, &)
#endif

#ifndef atomic_or
ATOMIC_OP(or, |)
#endif

#ifndef atomic_xor
ATOMIC_OP(xor, ^)
#endif

#undef ATOMIC_FETCH_OP
#undef ATOMIC_OP_RETURN
#undef ATOMIC_OP

/*
 * Atomic operations that C can't guarantee us.  Useful for
 * resource counting etc..
 */

#define ATOMIC_INIT(i)	{ (i) }

/**
 * atomic_read - read atomic variable
 * @v: pointer of type atomic_t
 *
 * Atomically reads the value of @v.
 */
#ifndef atomic_read
#define atomic_read(v)	READ_ONCE((v)->counter)
#endif

/**
 * atomic_set - set atomic variable
 * @v: pointer of type atomic_t
 * @i: required value
 *
 * Atomically sets the value of @v to @i.
 */
#define atomic_set(v, i) WRITE_ONCE(((v)->counter), (i))

//#include <linux/irqflags.h>

static inline int atomic_add_negative(int i, atomic_t *v)
{
	return atomic_add_return(i, v) < 0;
}

static inline void atomic_add(int i, atomic_t *v)
{
	atomic_add_return(i, v);
}

static inline void atomic_sub(int i, atomic_t *v)
{
	atomic_sub_return(i, v);
}

static inline void atomic_inc(atomic_t *v)
{
	atomic_add_return(1, v);
}

static inline void atomic_dec(atomic_t *v)
{
	atomic_sub_return(1, v);
}

#define atomic_dec_return(v)		atomic_sub_return(1, (v))
#define atomic_inc_return(v)		atomic_add_return(1, (v))

#define atomic_sub_and_test(i, v)	(atomic_sub_return((i), (v)) == 0)
#define atomic_dec_and_test(v)		(atomic_dec_return(v) == 0)
#define atomic_inc_and_test(v)		(atomic_inc_return(v) == 0)

#if 0
#define atomic_xchg(ptr, v)		(xchg(&(ptr)->counter, (v)))
#define atomic_cmpxchg(v, old, new)	(cmpxchg(&((v)->counter), (old), (new)))

static inline int __atomic_add_unless(atomic_t *v, int a, int u)
{
	int c, old;
	c = atomic_read(v);
	while (c != u && (old = atomic_cmpxchg(v, c, c + a)) != c)
		c = old;
	return c;
}
#endif

#endif /* __ASM_GENERIC_ATOMIC_H */
