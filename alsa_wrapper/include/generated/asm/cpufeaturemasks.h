#ifndef _ASM_X86_CPUFEATUREMASKS_H
#define _ASM_X86_CPUFEATUREMASKS_H

/* REQUIRED_MASK definitions based on your .config */
#define REQUIRED_MASK0  0x01807bfb
#define REQUIRED_MASK1  0x00000000
#define REQUIRED_MASK2  0x00000800
/* ... [omitted for brevity] ... */
#define REQUIRED_MASK19 0x00000000
#define REQUIRED_MASK20 0x00000000

/* DISABLED_MASK definitions based on your .config */
#define DISABLED_MASK0  0x00000000
#define DISABLED_MASK1  0x01000000
#define DISABLED_MASK2  0x00004000
/* ... [omitted for brevity] ... */
#define DISABLED_MASK19 0x00002000
#define DISABLED_MASK20 0x00000000

/* Macro evaluating if a specific bit belongs to a REQUIRED mask */
#define REQUIRED_MASK_BIT_SET(bit) ( \
	(((bit) >> 5) == 0  && (1UL << ((bit) & 31)) & REQUIRED_MASK0)  || \
	(((bit) >> 5) == 1  && (1UL << ((bit) & 31)) & REQUIRED_MASK1)  || \
	(((bit) >> 5) == 2  && (1UL << ((bit) & 31)) & REQUIRED_MASK2)  || \
	/* ... [continues checking every capability word sequentially] ... */ \
	(((bit) >> 5) == 20 && (1UL << ((bit) & 31)) & REQUIRED_MASK20) || \
	0)

/* Macro evaluating if a specific bit belongs to a DISABLED mask */
#define DISABLED_MASK_BIT_SET(bit) ( \
	(((bit) >> 5) == 0  && (1UL << ((bit) & 31)) & DISABLED_MASK0)  || \
	(((bit) >> 5) == 1  && (1UL << ((bit) & 31)) & DISABLED_MASK1)  || \
	(((bit) >> 5) == 2  && (1UL << ((bit) & 31)) & DISABLED_MASK2)  || \
	/* ... [continues checking every capability word sequentially] ... */ \
	(((bit) >> 5) == 20 && (1UL << ((bit) & 31)) & DISABLED_MASK20) || \
	0)

#endif /* _ASM_X86_CPUFEATUREMASKS_H */
