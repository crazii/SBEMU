#ifndef SBEMU_LINUX_KERNEL_H
#define SBEMU_LINUX_KERNEL_H

#ifndef outb
//#define outb(x,y) outportb(y,x)
//#define outw(x,y) outportw(y,x)
//#define outl(x,y) outportl(y,x)
#define outb(x,y) outp(y,x)
#define outw(x,y) outpw(y,x)
#define outl(x,y) outpd(y,x)
#endif

#ifndef inb
#define inb(reg) inp(reg)
#define inw(reg) inpw(reg)
#define inl(reg) inpd(reg)
#endif

#define CONFIG_X86 1

#include <stddef.h>
#include "linux/types.h"

#ifndef NR_CPUS
#define NR_CPUS 1
#endif
typedef unsigned long cpumask_var_t;

#define USHRT_MAX       ((u16)(~0U))
#define SHRT_MAX        ((s16)(USHRT_MAX>>1))
#define SHRT_MIN        ((s16)(-SHRT_MAX - 1))
#define INT_MAX         ((int)(~0U>>1))
#define INT_MIN         (-INT_MAX - 1)
#define UINT_MAX        (~0U)
#define LONG_MAX        ((long)(~0UL>>1))
#define LONG_MIN        (-LONG_MAX - 1)
#define ULONG_MAX       (~0UL)
#define LLONG_MAX       ((long long)(~0ULL>>1))
#define LLONG_MIN       (-LLONG_MAX - 1)
#define ULLONG_MAX      (~0ULL)
//#define SIZE_MAX        (~(size_t)0)

#define U8_MAX          ((u8)~0U)
#define S8_MAX          ((s8)(U8_MAX>>1))
#define S8_MIN          ((s8)(-S8_MAX - 1))
#define U16_MAX         ((u16)~0U)
#define S16_MAX         ((s16)(U16_MAX>>1))
#define S16_MIN         ((s16)(-S16_MAX - 1))
#define U32_MAX         ((u32)~0U)
#define S32_MAX         ((s32)(U32_MAX>>1))
#define S32_MIN         ((s32)(-S32_MAX - 1))
#define U64_MAX         ((u64)~0ULL)
#define S64_MAX         ((s64)(U64_MAX>>1))
#define S64_MIN         ((s64)(-S64_MAX - 1))

//#define unlikely(x) x

#define container_of(ptr, type, member) ({			\
	const typeof( ((type *)0)->member ) *__mptr = (ptr);	\
	(type *)( (char *)__mptr - offsetof(type,member) );})

#define container_of_const(ptr, type, member) ({			\
	const typeof( ((type *)0)->member ) *__mptr = (ptr);	\
	(type *)( (char *)__mptr - offsetof(type,member) );})

#define ALIGN(x, a) __ALIGN_KERNEL((x), (a))
#define __ALIGN_KERNEL(x, a) __ALIGN_KERNEL_MASK(x, (__typeof__(x))(a) - 1)
#define __ALIGN_KERNEL_MASK(x, mask) (((x) + (mask)) & ~(mask))

/* @a is a power of 2 value */
#define ALIGN_DOWN(x, a)        __ALIGN_KERNEL((x) - ((a) - 1), (a))
#define __ALIGN_MASK(x, mask)   __ALIGN_KERNEL_MASK((x), (mask))
#define PTR_ALIGN(p, a)         ((typeof(p))ALIGN((unsigned long)(p), (a)))
#define PTR_ALIGN_DOWN(p, a)    ((typeof(p))ALIGN_DOWN((unsigned long)(p), (a)))
#define IS_ALIGNED(x, a)                (((x) & ((typeof(x))(a) - 1)) == 0)

/* to align the pointer to the (next) page boundary */
#define PAGE_ALIGN(addr) ALIGN(addr, PAGE_SIZE)

/* to align the pointer to the (prev) page boundary */
#define PAGE_ALIGN_DOWN(addr) ALIGN_DOWN(addr, PAGE_SIZE)

/* test whether an address (unsigned long or pointer) is aligned to PAGE_SIZE */
#define PAGE_ALIGNED(addr)      IS_ALIGNED((unsigned long)(addr), PAGE_SIZE)

#define cpu_to_le16(x) x // assumes Little Endian CPU
#define le16_to_cpu(x) x // assumes Little Endian CPU
#define cpu_to_le32(x) x // assumes Little Endian CPU
#define le32_to_cpu(x) x // assumes Little Endian CPU

#define __KERNEL_DIV_ROUND_UP(n, d) (((n) + (d) - 1) / (d))
#define DIV_ROUND_UP __KERNEL_DIV_ROUND_UP

/*
 * Divide positive or negative dividend by positive or negative divisor
 * and round to closest integer. Result is undefined for negative
 * divisors if the dividend variable type is unsigned and for negative
 * dividends if the divisor variable type is unsigned.
 */
#define DIV_ROUND_CLOSEST(x, divisor)(                  \
{                                                       \
        typeof(x) __x = x;                              \
        typeof(divisor) __d = divisor;                  \
        (((typeof(x))-1) > 0 ||                         \
         ((typeof(divisor))-1) > 0 ||                   \
         (((__x) > 0) == ((__d) > 0))) ?                \
                (((__x) + ((__d) / 2)) / (__d)) :       \
                (((__x) - ((__d) / 2)) / (__d));        \
}                                                       \
)

/*
 * This looks more complex than it should be. But we need to
 * get the type for the ~ right in round_down (it needs to be
 * as wide as the result!), and we want to evaluate the macro
 * arguments just once each.
 */
#define __round_mask(x, y) ((__typeof__(x))((y)-1))

/**
 * round_up - round up to next specified power of 2
 * @x: the value to round
 * @y: multiple to round up to (must be a power of 2)
 *
 * Rounds @x up to next multiple of @y (which must be a power of 2).
 * To perform arbitrary rounding up, use roundup() below.
 */
#define round_up(x, y) ((((x)-1) | __round_mask(x, y))+1)

/**
 * round_down - round down to next specified power of 2
 * @x: the value to round
 * @y: multiple to round down to (must be a power of 2)
 *
 * Rounds @x down to next multiple of @y (which must be a power of 2).
 * To perform arbitrary rounding down, use rounddown() below.
 */
#define round_down(x, y) ((x) & ~__round_mask(x, y))

/*
 * Check at compile time that something is of a particular type.
 * Always evaluates to 1 so you may use it easily in comparisons.
 */
#define typecheck(type,x) \
({      type __dummy; \
        typeof(x) __dummy2; \
        (void)(&__dummy == &__dummy2); \
        1; \
})

/*
 * Check at compile time that 'function' is a certain type, or is a pointer
 * to that type (needs to use typedef for the function type.)
 */
#define typecheck_fn(type,function) \
({      typeof(type) __tmp = function; \
        (void)__tmp; \
})

#define EXPORT_SYMBOL(x)
#define EXPORT_SYMBOL_NS_GPL(x, y)
#define EXPORT_SYMBOL_GPL(x)

#define BUG_ON(x) x
#define WARN_ON(x) x

#ifndef BUILD_BUG_ON
/* Force a compilation error if condition is true */
#define BUILD_BUG_ON(condition) ((void)BUILD_BUG_ON_ZERO(condition))
/* Force a compilation error if condition is true, but also produce a
   result (of value 0 and type size_t), so the expression can be used
   e.g. in a structure initializer (or where-ever else comma expressions
   aren't permitted). */
#define BUILD_BUG_ON_ZERO(e) (sizeof(struct { int:-!!(e); }))
#define BUILD_BUG_ON_NULL(e) ((void *)sizeof(struct { int:-!!(e); }))
#endif

#include "dpmi/dbgutil.h"

#define PAGE_SIZE 4096
#define PAGE_MASK	(~(PAGE_SIZE-1))

#include "au_cards/au_base.h"
//#define mdelay(m) pds_mdelay((m)*100)
//#define msleep(m) pds_mdelay((m)*100)
#define mdelay(m) pds_delay_10us((m)*100)
#define msleep(m) pds_delay_10us((m)*100)
#define udelay(u) pds_delay_1695ns(u)

#define usleep_range(x,y) pds_delay_10us((x)/10)

#define schedule_timeout_uninterruptible(ticks) 0brokenDONOTUSE//pds_delay_1695ns(ticks)


#ifndef BITS_PER_LONG
#define BITS_PER_LONG 32
#endif
#ifndef BITS_PER_LONG_LONG
#define BITS_PER_LONG_LONG 64
#endif
#ifndef PAGE_SHIFT
#define PAGE_SHIFT 12
#endif

/*
 * small_const_nbits(n) is true precisely when it is known at compile-time
 * that BITMAP_SIZE(n) is 1, i.e. 1 <= n <= BITS_PER_LONG. This allows
 * various bit/bitmap APIs to provide a fast inline implementation. Bitmaps
 * of size 0 are very rare, and a compile-time-known-size 0 is most likely
 * a sign of error. They will be handled correctly by the bit/bitmap APIs,
 * but using the out-of-line functions, so that the inline implementations
 * can unconditionally dereference the pointer(s).
 */
#define small_const_nbits(nbits) \
        (__builtin_constant_p(nbits) && (nbits) <= BITS_PER_LONG && (nbits) > 0)

// from linux/cpumask.h
/* Don't assign or return these: may not be this big! */
typedef struct cpumask { DECLARE_BITMAP(bits, NR_CPUS); } cpumask_t;
extern struct cpumask __cpu_possible_mask;
#define cpu_possible_mask ((const struct cpumask *)&__cpu_possible_mask)


// IRQ stuff from linux/interrupt.h
#define IRQ_NONE 0
#define IRQ_HANDLED 1
#define IRQ_RETVAL(x) ((x) ? IRQ_HANDLED : IRQ_NONE)

typedef int irqreturn_t;
#define IRQF_SHARED 0

/**
 * struct irq_affinity_desc - Interrupt affinity descriptor
 * @mask:       cpumask to hold the affinity assignment
 * @is_managed: 1 if the interrupt is managed internally
 */
struct irq_affinity_desc {
        struct cpumask  mask;
        unsigned int    is_managed : 1;
};

#define DMA_BIT_MASK(n)	(((n) == 64) ? ~0ULL : ((1ULL<<(n))-1))

#define DMA_MASK_NONE	0x0ULL

#define free_irq(x,y)

#define linux_writel(addr,value) PDS_PUTB_LE32((volatile char *)(addr),value)
#define linux_readl(addr) PDS_GETB_LE32((volatile char *)(addr))
#define linux_writew(addr,value) PDS_PUTB_LE16((volatile char *)(addr), value)
#define linux_readw(addr) PDS_GETB_LE16((volatile char *)(addr))
#define linux_writeb(addr,value) *((volatile unsigned char *)(addr))=value
#define linux_readb(addr) PDS_GETB_8U((volatile char *)(addr))

#define writel(value,addr) linux_writel(addr,value)
#define readl(addr) linux_readl(addr)
#define writew(value,addr) linux_writew(addr,value)
#define readw(addr) linux_readw(addr)
#define writeb(value,addr) linux_writeb(addr,value)
#define readb(addr) linux_readb(addr)

// from linux/args.h:
/* This counts to 12. Any more, it will return 13th argument. */
#define __COUNT_ARGS(_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _n, X...) _n
#define COUNT_ARGS(X...) __COUNT_ARGS(, ##X, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0)

/* Concatenate two parameters, but allow them to be expanded beforehand. */
#define __CONCAT(a, b) a ## b
#define CONCATENATE(a, b) __CONCAT(a, b)

// linux/kernel.h:
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

#ifndef PCI_DEBUG
#define PCI_DEBUG 0
#endif

// linux/printk.h:
#define KERN_INFO "INFO: "
#define KERN_DEBUG "DEBUG: "
#define KERN_ERR "ERROR: "

#define PRINTK_USES_PRINTF 0
#if PRINTK_USES_PRINTF
#define printk(...) printf(__VA_ARGS__)
#else
#define printk(...) DBG_Logi(__VA_ARGS__)
#endif
#define snd_printk(...) printk(__VA_ARGS__)
#define panic(...) printk("PANIC " __VA_ARGS__)

#if PCI_DEBUG
#define pr_info_once(...) printk(__VA_ARGS__)
#define pr_info(...) printk(__VA_ARGS__)
#define snd_printd(...) printk(__VA_ARGS__)
#define snd_printdd(...) printk(__VA_ARGS__)
#else
#define pr_info_once(...) //printk(__VA_ARGS__)
#define pr_info(...) //printk(__VA_ARGS__)
#define snd_printd(...) //printk(__VA_ARGS__)
#define snd_printdd(...) //printk(__VA_ARGS__)
#endif
#define pr_warn(...) printk(__VA_ARGS__)
#define pr_err(...) printk(__VA_ARGS__)
#define WARN_ON(x) x

#define HERE() printk("%s:%d\n", __FILE__, __LINE__)
#define LHERE(_label)                                                   \
  do {                                                                  \
    printk("%s:%d %4.4X:%8.8X\n", __FILE__, __LINE__,                   \
           _my_cs(), (uintptr_t)&&_label);                              \
  } while (0)
#define HEREL(_label)                                                   \
  do {                                                                  \
    printk("%s:%d %4.4X:%8.8X\n", __FILE__, __LINE__,                   \
           _my_cs(), (uintptr_t)&&_label);                              \
  _label:                                                               \
  } while (0)

enum {
        DUMP_PREFIX_NONE,
        DUMP_PREFIX_ADDRESS,
        DUMP_PREFIX_OFFSET
};
extern void print_hex_dump(const char *level, const char *prefix_str,
                           int prefix_type, int rowsize, int groupsize,
                           const void *buf, size_t len, bool ascii);

#define dump_stack(x) printk("dump_stack\n")

#define kstrdup(x,y) strdup(x)
#define kstrndup(x,n,z) strndup(x,n)
#define simple_strtoul(x,y,b) strtoul(x,y,b)
#define strchrnul(x,y) strchr(x,y)

//#ifndef USEC_PER_MSEC
//#define USEC_PER_MSEC 1000L
//#endif

// linux/bitmap.h:
/*
 * Allocation and deallocation of bitmap.
 * Provided in lib/bitmap.c to avoid circular dependency.
 */
unsigned long *bitmap_alloc(unsigned int nbits, gfp_t flags);
unsigned long *bitmap_zalloc(unsigned int nbits, gfp_t flags);
unsigned long *bitmap_alloc_node(unsigned int nbits, gfp_t flags, int node);
unsigned long *bitmap_zalloc_node(unsigned int nbits, gfp_t flags, int node);
void bitmap_free(const unsigned long *bitmap);

// linux/minmax.h:
//#define __safe_cmp(x, y) (__typecheck(x, y) && __no_side_effects(x, y))
#define __cmp(x, y, op) ((x) op (y) ? (x) : (y))
#define __safe_cmp __cmp
#define __careful_cmp __cmp

/**
 * min_t - return minimum of two values, using the specified type
 * @type: data type to use
 * @x: first value
 * @y: second value
 */
#define min_t(type, x, y)       __careful_cmp((type)(x), (type)(y), <)

/**
 * max_t - return maximum of two values, using the specified type
 * @type: data type to use
 * @x: first value
 * @y: second value
 */
#define max_t(type, x, y)       __careful_cmp((type)(x), (type)(y), >)

#define DEFINE_RWLOCK(x) int x
#define read_lock(x)
#define read_unlock(x)
#define write_lock(x)
#define write_unlock(x)

// linux/seq_file.h:

#include "linux/mutex.h"

struct seq_operations;

struct seq_file {
        char *buf;
        size_t size;
        size_t from;
        size_t count;
        size_t pad_until;
        loff_t index;
        loff_t read_pos;
        struct mutex lock;
        const struct seq_operations *op;
        int poll_event;
        const struct file *file;
        void *private;
};

struct seq_operations {
        void * (*start) (struct seq_file *m, loff_t *pos);
        void (*stop) (struct seq_file *m, void *v);
        void * (*next) (struct seq_file *m, void *v, loff_t *pos);
        int (*show) (struct seq_file *m, void *v);
};

// include/linux/pfn.h:
/*
 * pfn_t: encapsulates a page-frame number that is optionally backed
 * by memmap (struct page).  Whether a pfn_t has a 'struct page'
 * backing is indicated by flags in the high bits of the value.
 */
typedef struct {
        u64 val;
} pfn_t;

#define PFN_ALIGN(x)    (((unsigned long)(x) + (PAGE_SIZE - 1)) & PAGE_MASK)
#define PFN_UP(x)       (((x) + PAGE_SIZE-1) >> PAGE_SHIFT)
#define PFN_DOWN(x)     ((x) >> PAGE_SHIFT)
#define PFN_PHYS(x)     ((phys_addr_t)(x) << PAGE_SHIFT)
#define PHYS_PFN(x)     ((unsigned long)((x) >> PAGE_SHIFT))

#include "linux/gfp_types.h"

struct task_struct {
};

extern struct task_struct __g_current;

//DECLARE_PER_CPU(struct task_struct *, current_task);

static inline struct task_struct *get_current(void)
{
  return &__g_current;
}

#define current get_current()
#define TASK_UNINTERRUPTIBLE 0
#define TASK_NORMAL 1
#define TASK_INTERRUPTIBLE 2
#define TASK_WAKEKILL 4
#define set_current_state(x)
#define schedule()
#define might_sleep()

// Linux kernel parameters
#define __setup(x,y)
#define early_param(x,y)
#define late_initcall(x)
#define pure_initcall(x)

struct work_struct;

#ifndef __bf_shf
#define __bf_shf(x) (__builtin_ffsll(x) - 1)
#endif
#ifndef FIELD_GET
/**
 * FIELD_GET() - extract a bitfield element
 * @_mask: shifted mask defining the field's length and position
 * @_reg:  value of entire bitfield
 *
 * FIELD_GET() extracts the field specified by @_mask from the
 * bitfield passed in as @_reg by masking and shifting it down.
 */
#define FIELD_GET(_mask, _reg)                                          \
        ({                                                              \
                (typeof(_mask))(((_reg) & (_mask)) >> __bf_shf(_mask)); \
        })
#endif

// Power management stuff
typedef struct pm_message {
        int event;
} pm_message_t;

#define pm_runtime_forbid(x)
#define pm_runtime_set_active(x)
#define pm_runtime_enable(x)
#define pm_runtime_get(x)
#define pm_runtime_put(x)
#define pm_runtime_put_sync(x)
#define pm_runtime_get_sync(x)
#define pm_runtime_get_noresume(x)
#define pm_runtime_resume(x)
#define pm_runtime_suspended(x) 0
#define pm_runtime_barrier(x)

#define PM_EVENT_INVALID        (-1)
#define PM_EVENT_ON             0x0000
#define PM_EVENT_FREEZE         0x0001
#define PM_EVENT_SUSPEND        0x0002
#define PM_EVENT_HIBERNATE      0x0004
#define PM_EVENT_QUIESCE        0x0008
#define PM_EVENT_RESUME         0x0010
#define PM_EVENT_THAW           0x0020
#define PM_EVENT_RESTORE        0x0040
#define PM_EVENT_RECOVER        0x0080
#define PM_EVENT_USER           0x0100
#define PM_EVENT_REMOTE         0x0200
#define PM_EVENT_AUTO           0x0400

// include/linux/mod_devicetable.h:
#define DMI_MATCH(a, b)	{ .slot = a, .substr = b }
#define device_may_wakeup(x) 0 // ???
#define device_can_wakeup(x) 0 // ???
#define device_set_wakeup_capable(x,y) // ???
#define device_lock(x)

#define OF_BAD_ADDR     ((u64)-1)

#define IOMEM_ERR_PTR(err) (__force void __iomem *)ERR_PTR(err)

// sysfs
struct kobject;
struct bin_attribute;
struct attribute {
        const char              *name;
        umode_t                 mode;
#ifdef CONFIG_DEBUG_LOCK_ALLOC
        bool                    ignore_lockdep:1;
        struct lock_class_key   *key;
        struct lock_class_key   skey;
#endif
};
struct attribute_group {
        const char              *name;
        umode_t                 (*is_visible)(struct kobject *,
                                              struct attribute *, int);
        umode_t                 (*is_bin_visible)(struct kobject *,
                                                  struct bin_attribute *, int);
        struct attribute        **attrs;
        struct bin_attribute    **bin_attrs;
};

#define upper_32_bits(n) ((u32)(((n) >> 16) >> 16))

#define RESOURCE_SIZE_MAX	((resource_size_t)~0)

#define IS_REACHABLE(x) x
#define fallthrough

// cpu_relax from arch/x86/boot/boot.h
#define cpu_relax()     asm volatile("rep; nop")

#endif

/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __ASM_GENERIC_CACHE_H
#define __ASM_GENERIC_CACHE_H
/*
 * 32 bytes appears to be the most common cache line size,
 * so make that the default here. Architectures with larger
 * cache lines need to provide their own cache.h.
 */

#define L1_CACHE_SHIFT		5
#define L1_CACHE_BYTES		(1 << L1_CACHE_SHIFT)
#endif /* __ASM_GENERIC_CACHE_H */
