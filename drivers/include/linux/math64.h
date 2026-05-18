#ifndef _LINUX_MATH64_H
#define _LINUX_MATH64_H

#include "linux/types.h"
#include "dpmi/dbgutil.h"
//#include <asm/div64.h>

#ifndef BITS_PER_LONG
#define BITS_PER_LONG 32
#endif

#if BITS_PER_LONG == 64

#define div64_long(x, y) div64_s64((x), (y))
#define div64_ul(x, y)   div64_u64((x), (y))

/**
 * div_u64_rem - unsigned 64bit divide with 32bit divisor with remainder
 *
 * This is commonly provided by 32bit archs to provide an optimized 64bit
 * divide.
 */
static inline u64 div_u64_rem(u64 dividend, u32 divisor, u32 *remainder)
{
	*remainder = dividend % divisor;
	return dividend / divisor;
}

/**
 * div_s64_rem - signed 64bit divide with 32bit divisor with remainder
 */
static inline s64 div_s64_rem(s64 dividend, s32 divisor, s32 *remainder)
{
	*remainder = dividend % divisor;
	return dividend / divisor;
}

/**
 * div64_u64_rem - unsigned 64bit divide with 64bit divisor and remainder
 */
static inline u64 div64_u64_rem(u64 dividend, u64 divisor, u64 *remainder)
{
	*remainder = dividend % divisor;
	return dividend / divisor;
}

/**
 * div64_u64 - unsigned 64bit divide with 64bit divisor
 */
static inline u64 div64_u64(u64 dividend, u64 divisor)
{
	return dividend / divisor;
}

/**
 * div64_s64 - signed 64bit divide with 64bit divisor
 */
static inline s64 div64_s64(s64 dividend, s64 divisor)
{
	return dividend / divisor;
}

#elif BITS_PER_LONG == 32

//#include <linux/log2.h>

/*
 * If the divisor happens to be constant, we determine the appropriate
 * inverse at compile time to turn the division into a few inline
 * multiplications which ought to be much faster.
 *
 * (It is unfortunate that gcc doesn't perform all this internally.)
 */

# define do_div(n,base) ({					\
	uint32_t __base = (base);				\
	uint32_t __rem;						\
	__rem = ((uint64_t)(n)) % __base;			\
	(n) = ((uint64_t)(n)) / __base;				\
	__rem;							\
 })

#define __div64_const32(n, ___b)					\
({									\
	/*								\
	 * Multiplication by reciprocal of b: n / b = n * (p / b) / p	\
	 *								\
	 * We rely on the fact that most of this code gets optimized	\
	 * away at compile time due to constant propagation and only	\
	 * a few multiplication instructions should remain.		\
	 * Hence this monstrous macro (static inline doesn't always	\
	 * do the trick here).						\
	 */								\
	uint64_t ___res, ___x, ___t, ___m, ___n = (n);			\
	uint32_t ___p, ___bias;						\
									\
	/* determine MSB of b */					\
	___p = 1 << ilog2(___b);					\
									\
	/* compute m = ((p << 64) + b - 1) / b */			\
	___m = (~0ULL / ___b) * ___p;					\
	___m += (((~0ULL % ___b + 1) * ___p) + ___b - 1) / ___b;	\
									\
	/* one less than the dividend with highest result */		\
	___x = ~0ULL / ___b * ___b - 1;					\
									\
	/* test our ___m with res = m * x / (p << 64) */		\
	___res = ((___m & 0xffffffff) * (___x & 0xffffffff)) >> 32;	\
	___t = ___res += (___m & 0xffffffff) * (___x >> 32);		\
	___res += (___x & 0xffffffff) * (___m >> 32);			\
	___t = (___res < ___t) ? (1ULL << 32) : 0;			\
	___res = (___res >> 32) + ___t;					\
	___res += (___m >> 32) * (___x >> 32);				\
	___res /= ___p;							\
									\
	/* Now sanitize and optimize what we've got. */			\
	if (~0ULL % (___b / (___b & -___b)) == 0) {			\
		/* special case, can be simplified to ... */		\
		___n /= (___b & -___b);					\
		___m = ~0ULL / (___b / (___b & -___b));			\
		___p = 1;						\
		___bias = 1;						\
	} else if (___res != ___x / ___b) {				\
		/*							\
		 * We can't get away without a bias to compensate	\
		 * for bit truncation errors.  To avoid it we'd need an	\
		 * additional bit to represent m which would overflow	\
		 * a 64-bit variable.					\
		 *							\
		 * Instead we do m = p / b and n / b = (n * m + m) / p.	\
		 */							\
		___bias = 1;						\
		/* Compute m = (p << 64) / b */				\
		___m = (~0ULL / ___b) * ___p;				\
		___m += ((~0ULL % ___b + 1) * ___p) / ___b;		\
	} else {							\
		/*							\
		 * Reduce m / p, and try to clear bit 31 of m when	\
		 * possible, otherwise that'll need extra overflow	\
		 * handling later.					\
		 */							\
		uint32_t ___bits = -(___m & -___m);			\
		___bits |= ___m >> 32;					\
		___bits = (~___bits) << 1;				\
		/*							\
		 * If ___bits == 0 then setting bit 31 is  unavoidable.	\
		 * Simply apply the maximum possible reduction in that	\
		 * case. Otherwise the MSB of ___bits indicates the	\
		 * best reduction we should apply.			\
		 */							\
		if (!___bits) {						\
			___p /= (___m & -___m);				\
			___m /= (___m & -___m);				\
		} else {						\
			___p >>= ilog2(___bits);			\
			___m >>= ilog2(___bits);			\
		}							\
		/* No bias needed. */					\
		___bias = 0;						\
	}								\
									\
	/*								\
	 * Now we have a combination of 2 conditions:			\
	 *								\
	 * 1) whether or not we need to apply a bias, and		\
	 *								\
	 * 2) whether or not there might be an overflow in the cross	\
	 *    product determined by (___m & ((1 << 63) | (1 << 31))).	\
	 *								\
	 * Select the best way to do (m_bias + m * n) / (1 << 64).	\
	 * From now on there will be actual runtime code generated.	\
	 */								\
	___res = __arch_xprod_64(___m, ___n, ___bias);			\
									\
	___res /= ___p;							\
})

#define div64_long(x, y) div_s64((x), (y))
#define div64_ul(x, y)   div_u64((x), (y))

#ifndef div_u64_rem
static inline u64 div_u64_rem(u64 dividend, u32 divisor, u32 *remainder)
{
	*remainder = do_div(dividend, divisor);
	return dividend;
}
#endif

#ifndef div_s64_rem
extern s64 div_s64_rem(s64 dividend, s32 divisor, s32 *remainder);
#endif

#ifndef div64_u64_rem
extern u64 div64_u64_rem(u64 dividend, u64 divisor, u64 *remainder);
#endif

#ifndef div64_u64
extern u64 div64_u64(u64 dividend, u64 divisor);
#endif

#ifndef div64_s64
extern s64 div64_s64(s64 dividend, s64 divisor);
#endif

#endif /* BITS_PER_LONG */

/**
 * div_u64 - unsigned 64bit divide with 32bit divisor
 *
 * This is the most common 64bit divide and should be used if possible,
 * as many 32bit archs can optimize this variant better than a full 64bit
 * divide.
 */
#ifndef div_u64
static inline u64 div_u64(u64 dividend, u32 divisor)
{
	u32 remainder;
	return div_u64_rem(dividend, divisor, &remainder);
}
#endif

/**
 * div_s64 - signed 64bit divide with 32bit divisor
 */
#ifndef div_s64
static inline s64 div_s64(s64 dividend, s32 divisor)
{
	s32 remainder;
	return div_s64_rem(dividend, divisor, &remainder);
}
#endif

u32 iter_div_u64_rem(u64 dividend, u32 divisor, u64 *remainder);

static inline u32
__iter_div_u64_rem(u64 dividend, u32 divisor, u64 *remainder)
{
	u32 ret = 0;

	while (dividend >= divisor) {
		/* The following asm() prevents the compiler from
		   optimising this loop into a modulo operation.  */
		asm("" : "+rm"(dividend));

		dividend -= divisor;
		ret++;
	}

	*remainder = dividend;

	return ret;
}

#if defined(CONFIG_ARCH_SUPPORTS_INT128) && defined(__SIZEOF_INT128__)

#ifndef mul_u64_u32_shr
static inline u64 mul_u64_u32_shr(u64 a, u32 mul, unsigned int shift)
{
	return (u64)(((unsigned __int128)a * mul) >> shift);
}
#endif /* mul_u64_u32_shr */

#ifndef mul_u64_u64_shr
static inline u64 mul_u64_u64_shr(u64 a, u64 mul, unsigned int shift)
{
	return (u64)(((unsigned __int128)a * mul) >> shift);
}
#endif /* mul_u64_u64_shr */

#else

#ifndef mul_u64_u32_shr
static inline u64 mul_u64_u32_shr(u64 a, u32 mul, unsigned int shift)
{
	u32 ah, al;
	u64 ret;

	al = a;
	ah = a >> 32;

	ret = ((u64)al * mul) >> shift;
	if (ah)
		ret += ((u64)ah * mul) << (32 - shift);

	return ret;
}
#endif /* mul_u64_u32_shr */

#ifndef mul_u64_u64_shr
static inline u64 mul_u64_u64_shr(u64 a, u64 b, unsigned int shift)
{
	union {
		u64 ll;
		struct {
#ifdef __BIG_ENDIAN
			u32 high, low;
#else
			u32 low, high;
#endif
		} l;
	} rl, rm, rn, rh, a0, b0;
	u64 c;

	a0.ll = a;
	b0.ll = b;

	rl.ll = (u64)a0.l.low * b0.l.low;
	rm.ll = (u64)a0.l.low * b0.l.high;
	rn.ll = (u64)a0.l.high * b0.l.low;
	rh.ll = (u64)a0.l.high * b0.l.high;

	/*
	 * Each of these lines computes a 64-bit intermediate result into "c",
	 * starting at bits 32-95.  The low 32-bits go into the result of the
	 * multiplication, the high 32-bits are carried into the next step.
	 */
	rl.l.high = c = (u64)rl.l.high + rm.l.low + rn.l.low;
	rh.l.low = c = (c >> 32) + rm.l.high + rn.l.high + rh.l.low;
	rh.l.high = (c >> 32) + rh.l.high;

	/*
	 * The 128-bit result of the multiplication is in rl.ll and rh.ll,
	 * shift it right and throw away the high part of the result.
	 */
	if (shift == 0)
		return rl.ll;
	if (shift < 64)
		return (rl.ll >> shift) | (rh.ll << (64 - shift));
	return rh.ll >> (shift & 63);
}
#endif /* mul_u64_u64_shr */

#endif

#ifndef mul_u64_u32_div
static inline u64 mul_u64_u32_div(u64 a, u32 mul, u32 divisor)
{
	union {
		u64 ll;
		struct {
#ifdef __BIG_ENDIAN
			u32 high, low;
#else
			u32 low, high;
#endif
		} l;
	} u, rl, rh;

	u.ll = a;
	rl.ll = (u64)u.l.low * mul;
	rh.ll = (u64)u.l.high * mul + rl.l.high;

	/* Bits 32-63 of the result will be in rh.l.low. */
	rl.l.high = do_div(rh.ll, divisor);

	/* Bits 0-31 of the result will be in rl.l.low.	*/
	do_div(rl.ll, divisor);

	rl.l.high = rh.l.low;
	return rl.ll;
}
#endif /* mul_u64_u32_div */

#endif /* _LINUX_MATH64_H */
