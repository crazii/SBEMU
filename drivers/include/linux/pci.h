#ifndef SBEMU_LINUX_PCI_H
#define SBEMU_LINUX_PCI_H

#define CONFIG_PCI 1
#undef CONFIG_PCI_MSI
#include "linux/kernel.h"

#if defined(USE_LINUX_PCIBIOS) && USE_LINUX_PCIBIOS == 1 // Linux prototypes
#else
#include "au_cards/pcibios.h"
//#undef outb
//#undef outw
//#undef outl
#endif

#include "linux/err.h"
#include "linux/pci_regs.h"
#include "linux/pci_ids.h"

#include "linux/mutex.h"
#include "linux/types.h"
#include "linux/list.h"
#include "linux/compiler.h"
#include "linux/spinlock.h"
#include "linux/atomic.h"
#include "linux/slab.h"

#include "linux/pci_ids.h"

// linux/mod_devicetable.h:
#define PCI_ANY_ID (~0)
// asm-generic/io.h:
#define IO_SPACE_LIMIT 0xffff

#define FW_BUG "FW_BUG: "

#ifndef pcibios_assign_all_busses
/* For bootloaders that do not initialize the PCI bus */
#define pcibios_assign_all_busses() 1
#endif

#define CONFIG_NUMA 1

#ifndef pcibus_to_node
#define pcibus_to_node(bus)     ((void)(bus), -1)
#endif
extern unsigned int nr_node_ids;

// minimal device infrastructure
struct device_dma_parameters {
        /*
         * a low level driver may set these to teach IOMMU code about
         * sg limitations.
         */
        unsigned int max_segment_size;
        unsigned int min_align_mask;
        unsigned long segment_boundary_mask;
};

struct bus_type;

struct kobject {
        const char              *name;
        struct list_head        entry;
  struct kobject          *parent;
  //struct kset             *kset;
  //        const struct kobj_type  *ktype;
  //struct kernfs_node      *sd; /* sysfs directory entry */
  //struct kref             kref;

        unsigned int state_initialized:1;
        unsigned int state_in_sysfs:1;
        unsigned int state_add_uevent_sent:1;
        unsigned int state_remove_uevent_sent:1;
        unsigned int uevent_suppress:1;

#ifdef CONFIG_DEBUG_KOBJECT_RELEASE
        struct delayed_work     release;
#endif
};

typedef unsigned int refcount_t;
struct kref {
        refcount_t refcount;
};
static inline void kref_init(struct kref *kref)
{
  //refcount_set(&kref->refcount, 1);
  kref->refcount = 1;
}
static inline int kref_put(struct kref *kref, void (*release)(struct kref *kref))
{
  //if (refcount_dec_and_test(&kref->refcount)) {
  //  release(kref);
  //  return 1;
  //}
  kref->refcount--;
  return 1;
}
static inline int __must_check kref_get_unless_zero(struct kref *kref)
{
  //return refcount_inc_not_zero(&kref->refcount);
  if (kref->refcount == 0) {
    return 0;
  } else {
    kref->refcount++;
    return 1;
  }
}
static inline void kref_get(struct kref *kref)
{
  //refcount_inc(&kref->refcount);
  kref->refcount++;
}

#include "linux/klist.h"

/**
 * struct kset - a set of kobjects of a specific type, belonging to a specific subsystem.
 *
 * A kset defines a group of kobjects.  They can be individually
 * different "types" but overall these kobjects all want to be grouped
 * together and operated on in the same manner.  ksets are used to
 * define the attribute callbacks and other common events that happen to
 * a kobject.
 *
 * @list: the list of all kobjects for this kset
 * @list_lock: a lock for iterating over the kobjects
 * @kobj: the embedded kobject for this kset (recursion, isn't it fun...)
 * @uevent_ops: the set of uevent operations for this kset.  These are
 * called whenever a kobject has something happen to it so that the kset
 * can add new environment variables, or filter out the uevents if so
 * desired.
 */
struct kset {
        struct list_head list;
        spinlock_t list_lock;
        struct kobject kobj;
  //const struct kset_uevent_ops *uevent_ops;
} __randomize_layout;

#define kobj_to_dev(__kobj)     container_of_const(__kobj, struct device, kobj)
#define kobject_get(x) x
#define kobject_put(x)

struct subsys_private {
        struct kset subsys;
        struct kset *devices_kset;
        struct list_head interfaces;
        struct mutex mutex;

        struct kset *drivers_kset;
  struct klist klist_devices;
  //struct klist klist_drivers;
  //struct blocking_notifier_head bus_notifier;
        unsigned int drivers_autoprobe:1;
        const struct bus_type *bus;
        struct device *dev_root;

        struct kset glue_dirs;
        const struct class *class;

  //struct lock_class_key lock_key;
};
#define to_subsys_private(obj) container_of_const(obj, struct subsys_private, subsys.kobj)
#define subsys_get(x) x
#define subsys_put(x)

struct device_private {
        struct klist klist_children;
        struct klist_node knode_parent;
        struct klist_node knode_driver;
        struct klist_node knode_bus;
        struct klist_node knode_class;
        struct list_head deferred_probe;
        struct device_driver *async_driver;
        char *deferred_probe_reason;
        struct device *device;
        u8 dead:1;
};
#define to_device_private_parent(obj)   \
        container_of(obj, struct device_private, knode_parent)
#define to_device_private_driver(obj)   \
        container_of(obj, struct device_private, knode_driver)
#define to_device_private_bus(obj)      \
        container_of(obj, struct device_private, knode_bus)
#define to_device_private_class(obj)    \
        container_of(obj, struct device_private, knode_class)

struct class;
struct bus_type {
        const char              *name;
        const char              *dev_name;
        const struct attribute_group **bus_groups;
        const struct attribute_group **dev_groups;
        const struct attribute_group **drv_groups;

        int (*match)(struct device *dev, struct device_driver *drv);
  //int (*uevent)(const struct device *dev, struct kobj_uevent_env *env);
        int (*probe)(struct device *dev);
        void (*sync_state)(struct device *dev);
        void (*remove)(struct device *dev);
        void (*shutdown)(struct device *dev);

        int (*online)(struct device *dev);
        int (*offline)(struct device *dev);

        int (*suspend)(struct device *dev, pm_message_t state);
        int (*resume)(struct device *dev);

        int (*num_vf)(struct device *dev);

        int (*dma_configure)(struct device *dev);
        void (*dma_cleanup)(struct device *dev);

        const struct dev_pm_ops *pm;

        const struct iommu_ops *iommu_ops;

        bool need_parent_lock;
};

/*
 * The type of device, "struct device" is embedded in. A class
 * or bus can contain devices of different types
 * like "partitions" and "disks", "mouse" and "event".
 * This identifies the device type and carries type-specific
 * information, equivalent to the kobj_type of a kobject.
 * If "name" is specified, the uevent will contain it in
 * the DEVTYPE variable.
 */
struct device_type {
        const char *name;
        const struct attribute_group **groups;
  //int (*uevent)(const struct device *dev, struct kobj_uevent_env *env);
  //char *(*devnode)(const struct device *dev, umode_t *mode,
  //kuid_t *uid, kgid_t *gid);
        void (*release)(struct device *dev);

        const struct dev_pm_ops *pm;
};
struct device {
  struct kobject kobj;
  struct device *parent;
  struct device_private   *p;
  struct class *class;
  const struct device_type *type;
  const struct bus_type   *bus;   /* type of bus device is on */
#ifdef CONFIG_NUMA
        int             numa_node;      /* NUMA node this device is close to */
#endif
  struct device_dma_parameters *dma_parms;
  u64 coherent_dma_mask;
  u64 __dma_mask;
  u64 *dma_mask;
  void *platform_data;
  void *driver_data;
  char *init_name;
  struct device_node *of_node;
  void    (*release)(struct device *dev);
  int refcnt;
};

#define device_trylock(x) 1
#define device_unlock(x)

static inline int device_is_registered(struct device *dev)
{
        return dev->kobj.state_in_sysfs;
}

static inline char *dev_driver_string (struct device *dev) {
  return "dev_driver_string";
}

/* device resource management */
typedef void (*dr_release_t)(struct device *dev, void *res);
typedef int (*dr_match_t)(struct device *dev, void *res, void *match_data);

void *__devres_alloc_node(dr_release_t release, size_t size, gfp_t gfp,
                          int nid, const char *name) __malloc;
#define devres_alloc(release, size, gfp) \
        __devres_alloc_node(release, size, gfp, NUMA_NO_NODE, #release)
#define devres_alloc_node(release, size, gfp, nid) \
        __devres_alloc_node(release, size, gfp, nid, #release)

void devres_free(void *res);
void devres_add(struct device *dev, void *res);
void *devres_find(struct device *dev, dr_release_t release,
                  dr_match_t match, void *match_data);
void *devres_get(struct device *dev, void *new_res,
                 dr_match_t match, void *match_data);
void *devres_remove(struct device *dev, dr_release_t release,
                    dr_match_t match, void *match_data);
int devres_destroy(struct device *dev, dr_release_t release,
                   dr_match_t match, void *match_data);
int devres_release(struct device *dev, dr_release_t release,
                   dr_match_t match, void *match_data);

extern void bus_sort_breadthfirst(struct bus_type *bus,
                                  int (*compare)(const struct device *a,
                                                 const struct device *b));

#define device_enable_async_suspend(x)
#define dev_is_removable(x) 0
#define dev_set_removable(x,y)

struct pci_sysdata {
        int             domain;         /* PCI domain */
        int             node;           /* NUMA node */
#ifdef CONFIG_ACPI
        struct acpi_device *companion;  /* ACPI companion device */
#endif
#ifdef CONFIG_X86_64
        void            *iommu;         /* IOMMU private data */
#endif
#ifdef CONFIG_PCI_MSI
        void            *fwnode;        /* IRQ domain for MSI assignment */
#endif
  //#if IS_ENABLED(CONFIG_VMD)
  //        struct pci_dev  *vmd_dev;       /* VMD Device if in Intel VMD domain */
  //#endif
};

// linux/sizes.h:
#ifndef SZ_64K
#define SZ_64K				0x00010000
#endif

static inline unsigned int dma_get_max_seg_size(struct device *dev)
{
        if (dev->dma_parms && dev->dma_parms->max_segment_size)
                return dev->dma_parms->max_segment_size;
        return SZ_64K;
}

static inline int dma_set_max_seg_size(struct device *dev, unsigned int size)
{
        if (dev->dma_parms) {
                dev->dma_parms->max_segment_size = size;
                return 0;
        }
        return -EIO;
}

static inline unsigned long dma_get_seg_boundary(struct device *dev)
{
        if (dev->dma_parms && dev->dma_parms->segment_boundary_mask)
                return dev->dma_parms->segment_boundary_mask;
        return ULONG_MAX;
}

static inline int dma_set_seg_boundary(struct device *dev, unsigned long mask)
{
        if (dev->dma_parms) {
                dev->dma_parms->segment_boundary_mask = mask;
                return 0;
        }
        return -EIO;
}

#define NUMA_NO_NODE    (-1)
#define NUMA_NO_MEMBLK  (-1)

#ifdef CONFIG_NUMA
static inline int dev_to_node(struct device *dev)
{
        return dev->numa_node;
}
static inline void set_dev_node(struct device *dev, int node)
{
        dev->numa_node = node;
}
#else
static inline int dev_to_node(struct device *dev)
{
        return NUMA_NO_NODE;
}
static inline void set_dev_node(struct device *dev, int node)
{
}
#endif

/*
 * High level routines for use by the bus drivers
 */
//extern int __must_check device_register(struct device *dev);
//extern void device_unregister(struct device *dev);
//extern void device_initialize(struct device *dev);
//extern int __must_check device_add(struct device *dev);
//extern void device_del(struct device *dev);

static inline int __must_check device_register(struct device *dev) {
  return 0;
}
static inline void device_unregister(struct device *dev) {
}
static inline void device_initialize(struct device *dev)  {
  memset(dev, 0, sizeof(*dev));
}
static inline int __must_check device_add(struct device *dev) {
  return 0;
}
static inline void device_del(struct device *dev) {
}

//extern __printf(2, 3) int dev_set_name(struct device *dev, const char *name, ...);
static inline int dev_set_name(struct device *dev, const char *name, ...) {
  return 0;
}

static inline struct irq_domain *dev_get_msi_domain(const struct device *dev)
{
#ifdef CONFIG_GENERIC_MSI_IRQ
        return dev->msi.domain;
#else
        return NULL;
#endif
}

static inline void dev_set_msi_domain(struct device *dev, struct irq_domain *d)
{
#ifdef CONFIG_GENERIC_MSI_IRQ
        dev->msi.domain = d;
#endif
}

/*
 * get_device - atomically increment the reference count for the device.
 *
 */
struct device *get_device(struct device *dev);
void put_device(struct device *dev);

struct device_node {
};

struct device_driver {
};

struct class {
  const char              *name;
        const struct attribute_group    **class_groups;
        const struct attribute_group    **dev_groups;

#if 0
        int (*dev_uevent)(const struct device *dev, struct kobj_uevent_env *env);
#endif
        char *(*devnode)(const struct device *dev, umode_t *mode);

        void (*class_release)(const struct class *class);
        void (*dev_release)(struct device *dev);

        int (*shutdown_pre)(struct device *dev);

#if 0
        const struct kobj_ns_type_operations *ns_type;
        const void *(*namespace)(const struct device *dev);

        void (*get_ownership)(const struct device *dev, kuid_t *uid, kgid_t *gid);

        const struct dev_pm_ops *pm;
#endif
};

/**
 * dev_name - Return a device's name.
 * @dev: Device with name to get.
 * Return: The kobject name of the device, or its initial name if unavailable.
 */
static inline const char *dev_name(const struct device *dev)
{
  return dev->init_name;
#if 0
        /* Use the init name until the kobject becomes available */
        if (dev->init_name)
                return dev->init_name;

        return kobject_name(&dev->kobj);
#endif
}

static inline void *dev_get_drvdata(const struct device *dev)
{
        return dev->driver_data;
}

static inline void dev_set_drvdata(struct device *dev, void *data)
{
        dev->driver_data = data;
}

#define PCI_STATUS_ERROR_BITS (PCI_STATUS_DETECTED_PARITY  | \
			       PCI_STATUS_SIG_SYSTEM_ERROR | \
			       PCI_STATUS_REC_MASTER_ABORT | \
			       PCI_STATUS_REC_TARGET_ABORT | \
			       PCI_STATUS_SIG_TARGET_ABORT | \
			       PCI_STATUS_PARITY)

/* Number of reset methods used in pci_reset_fn_methods array in pci.c */
#define PCI_NUM_RESET_METHODS 7

#define PCI_RESET_PROBE		true
#define PCI_RESET_DO_RESET	false

// struct msi_map from linux/msi_api.h
/**
 * msi_map - Mapping between MSI index and Linux interrupt number
 * @index:      The MSI index, e.g. slot in the MSI-X table or
 *              a software managed index if >= 0. If negative
 *              the allocation function failed and it contains
 *              the error code.
 * @virq:       The associated Linux interrupt number
 */
struct msi_map {
        int     index;
        int     virq;
};

// struct resource etc. from linux/ioport.h
/*
 * IO resources have these defined flags.
 *
 * PCI devices expose these flags to userspace in the "resource" sysfs file,
 * so don't move them.
 */
#define IORESOURCE_BITS		0x000000ff	/* Bus-specific bits */

#define IORESOURCE_TYPE_BITS	0x00001f00	/* Resource type */
#define IORESOURCE_IO		0x00000100	/* PCI/ISA I/O ports */
#define IORESOURCE_MEM		0x00000200
#define IORESOURCE_REG		0x00000300	/* Register offsets */
#define IORESOURCE_IRQ		0x00000400
#define IORESOURCE_DMA		0x00000800
#define IORESOURCE_BUS		0x00001000

#define IORESOURCE_PREFETCH     0x00002000      /* No side effects */
#define IORESOURCE_READONLY     0x00004000
#define IORESOURCE_CACHEABLE    0x00008000
#define IORESOURCE_RANGELENGTH  0x00010000
#define IORESOURCE_SHADOWABLE   0x00020000

#define IORESOURCE_SIZEALIGN    0x00040000      /* size indicates alignment */
#define IORESOURCE_STARTALIGN   0x00080000      /* start field is alignment */

#define IORESOURCE_MEM_64       0x00100000
#define IORESOURCE_WINDOW       0x00200000      /* forwarded by bridge */
#define IORESOURCE_MUXED        0x00400000      /* Resource is software muxed */

#define IORESOURCE_EXT_TYPE_BITS 0x01000000     /* Resource extended types */
#define IORESOURCE_SYSRAM       0x01000000      /* System RAM (modifier) */

/* IORESOURCE_SYSRAM specific bits. */
#define IORESOURCE_SYSRAM_DRIVER_MANAGED        0x02000000 /* Always detected via a driver. */
#define IORESOURCE_SYSRAM_MERGEABLE             0x04000000 /* Resource can be merged. */

#define IORESOURCE_EXCLUSIVE    0x08000000      /* Userland may not map this resource */

#define IORESOURCE_DISABLED     0x10000000
#define IORESOURCE_UNSET        0x20000000      /* No address assigned yet */
#define IORESOURCE_AUTO         0x40000000
#define IORESOURCE_BUSY         0x80000000      /* Driver has marked this resource busy */

/* I/O resource extended types */
#define IORESOURCE_SYSTEM_RAM           (IORESOURCE_MEM|IORESOURCE_SYSRAM)

/* PCI ROM control bits (IORESOURCE_BITS) */
#define IORESOURCE_ROM_ENABLE           (1<<0)  /* ROM is enabled, same as PCI_ROM_ADDRESS_ENABLE */
#define IORESOURCE_ROM_SHADOW           (1<<1)  /* Use RAM image, not ROM BAR */

/* PCI control bits.  Shares IORESOURCE_BITS with above PCI ROM.  */
#define IORESOURCE_PCI_FIXED            (1<<4)  /* Do not move resource */
#define IORESOURCE_PCI_EA_BEI           (1<<5)  /* BAR Equivalent Indicator */

/*
 * I/O Resource Descriptors
 *
 * Descriptors are used by walk_iomem_res_desc() and region_intersects()
 * for searching a specific resource range in the iomem table.  Assign
 * a new descriptor when a resource range supports the search interfaces.
 * Otherwise, resource.desc must be set to IORES_DESC_NONE (0).
 */
enum {
        IORES_DESC_NONE                         = 0,
        IORES_DESC_CRASH_KERNEL                 = 1,
        IORES_DESC_ACPI_TABLES                  = 2,
        IORES_DESC_ACPI_NV_STORAGE              = 3,
        IORES_DESC_PERSISTENT_MEMORY            = 4,
        IORES_DESC_PERSISTENT_MEMORY_LEGACY     = 5,
        IORES_DESC_DEVICE_PRIVATE_MEMORY        = 6,
        IORES_DESC_RESERVED                     = 7,
        IORES_DESC_SOFT_RESERVED                = 8,
        IORES_DESC_CXL                          = 9,
};

/*
 * Flags controlling ioremap() behavior.
 */
enum {
        IORES_MAP_SYSTEM_RAM            = BIT(0),
        IORES_MAP_ENCRYPTED             = BIT(1),
};


/*
 * Resources are tree-like, allowing
 * nesting etc..
 */
struct resource {
        resource_size_t start;
        resource_size_t end;
        const char *name;
        unsigned long flags;
        unsigned long desc;
        struct resource *parent, *sibling, *child;
};

static inline unsigned long resource_type(const struct resource *res)
{
        return res->flags & IORESOURCE_TYPE_BITS;
}
static inline unsigned long resource_ext_type(const struct resource *res)
{
        return res->flags & IORESOURCE_EXT_TYPE_BITS;
}

extern int release_resource(struct resource *new);

extern struct resource ioport_resource;
extern struct resource iomem_resource;

extern resource_size_t resource_alignment(struct resource *res);
static inline resource_size_t resource_size(const struct resource *res)
{
        return res->end - res->start + 1;
}

extern struct resource * __request_region(struct resource *,
                                        resource_size_t start,
                                        resource_size_t n,
                                        const char *name, int flags);
/* Convenience shorthand with allocation */
#define request_region(start,n,name)            __request_region(&ioport_resource, (start), (n), (name), 0)
#define request_muxed_region(start,n,name)      __request_region(&ioport_resource, (start), (n), (name), IORESOURCE_MUXED)
#define __request_mem_region(start,n,name, excl) __request_region(&iomem_resource, (start), (n), (name), excl)
#define request_mem_region(start,n,name) __request_region(&iomem_resource, (start), (n), (name), 0)
#define request_mem_region_muxed(start, n, name) \
        __request_region(&iomem_resource, (start), (n), (name), IORESOURCE_MUXED)
#define request_mem_region_exclusive(start,n,name) \
        __request_region(&iomem_resource, (start), (n), (name), IORESOURCE_EXCLUSIVE)
#define rename_region(region, newname) do { (region)->name = (newname); } while (0)

#define devm_request_region(dev,start,n,name) request_region(start,n,name)

extern int request_resource(struct resource *root, struct resource *new);
extern struct resource *request_resource_conflict(struct resource *root, struct resource *new);

extern struct resource *lookup_resource(struct resource *root, resource_size_t start);
extern int adjust_resource(struct resource *res, resource_size_t start,
                           resource_size_t size);

/* True iff r1 completely contains r2 */
static inline bool resource_contains(struct resource *r1, struct resource *r2)
{
        if (resource_type(r1) != resource_type(r2))
                return false;
        if (r1->flags & IORESOURCE_UNSET || r2->flags & IORESOURCE_UNSET)
                return false;
        return r1->start <= r2->start && r1->end >= r2->end;
}

/* True if any part of r1 overlaps r2 */
static inline bool resource_overlaps(struct resource *r1, struct resource *r2)
{
       return r1->start <= r2->end && r1->end >= r2->start;
}

// linux/resource_ext.h:
/*
 * Common resource list management data structure and interfaces to support
 * ACPI, PNP and PCI host bridge etc.
 */
struct resource_entry {
        struct list_head        node;
        struct resource         *res;   /* In master (CPU) address space */
        resource_size_t         offset; /* Translation offset for bridge */
        struct resource         __res;  /* Default storage for res */
};

static inline void resource_list_add(struct resource_entry *entry,
                                     struct list_head *head)
{
        list_add(&entry->node, head);
}

static inline void resource_list_add_tail(struct resource_entry *entry,
                                          struct list_head *head)
{
        list_add_tail(&entry->node, head);
}

static inline void resource_list_del(struct resource_entry *entry)
{
        list_del(&entry->node);
}

static inline void resource_list_free_entry(struct resource_entry *entry)
{
        kfree(entry);
}

static inline void
resource_list_destroy_entry(struct resource_entry *entry)
{
        resource_list_del(entry);
        resource_list_free_entry(entry);
}

extern struct resource_entry *
resource_list_create_entry(struct resource *res, size_t extra_size);
extern void resource_list_free(struct list_head *head);

#define resource_list_for_each_entry(entry, list)       \
        list_for_each_entry((entry), (list), node)

#define resource_list_for_each_entry_safe(entry, tmp, list)     \
        list_for_each_entry_safe((entry), (tmp), (list), node)

/* Compatibility cruft */
#define release_region(start,n) __release_region(&ioport_resource, (start), (n))
#define release_mem_region(start,n)     __release_region(&iomem_resource, (start), (n))

extern void __release_region(struct resource *, resource_size_t,
                                resource_size_t);

#define IORESOURCE_EXCLUSIVE	0x08000000	/* Userland may not map this resource */

#define EXPORT_SYMBOL_GPL(x)

#define dma_supported(x,y) 1

static inline int dma_set_coherent_mask(struct device *dev, u64 mask)
{
  if (!dma_supported(dev, mask))
    return -EIO;
  dev->coherent_dma_mask = mask;
  return 0;
}

static inline int dma_set_mask(struct device *dev, u64 mask)
{
  //struct dma_map_ops *ops = get_dma_ops(dev);

  //	if (ops->set_dma_mask)
  //		return ops->set_dma_mask(dev, mask);

  //  	if (!dev->dma_mask || !dma_supported(dev, mask))
  //  		return -EIO;
  if (!dev->dma_mask)
    dev->dma_mask = &dev->__dma_mask;
  *dev->dma_mask = mask;
  return 0;
}

/*
 * Set both the DMA mask and the coherent DMA mask to the same thing.
 * Note that we don't check the return value from dma_set_coherent_mask()
 * as the DMA API guarantees that the coherent DMA mask can be set to
 * the same or smaller than the streaming DMA mask.
 */
static inline int dma_set_mask_and_coherent(struct device *dev, u64 mask)
{
        int rc = dma_set_mask(dev, mask);
        if (rc == 0)
                dma_set_coherent_mask(dev, mask);
        return rc;
}

/**
 * struct pci_device_id - PCI device ID structure
 * @vendor:             Vendor ID to match (or PCI_ANY_ID)
 * @device:             Device ID to match (or PCI_ANY_ID)
 * @subvendor:          Subsystem vendor ID to match (or PCI_ANY_ID)
 * @subdevice:          Subsystem device ID to match (or PCI_ANY_ID)
 * @class:              Device class, subclass, and "interface" to match.
 *                      See Appendix D of the PCI Local Bus Spec or
 *                      include/linux/pci_ids.h for a full list of classes.
 *                      Most drivers do not need to specify class/class_mask
 *                      as vendor/device is normally sufficient.
 * @class_mask:         Limit which sub-fields of the class field are compared.
 *                      See drivers/scsi/sym53c8xx_2/ for example of usage.
 * @driver_data:        Data private to the driver.
 *                      Most drivers don't need to use driver_data field.
 *                      Best practice is to use driver_data as an index
 *                      into a static list of equivalent device types,
 *                      instead of using it as a pointer.
 * @override_only:      Match only when dev->driver_override is this driver.
 */
struct pci_device_id {
        __u32 vendor, device;           /* Vendor and device ID or PCI_ANY_ID*/
        __u32 subvendor, subdevice;     /* Subsystem ID's or PCI_ANY_ID */
        __u32 class, class_mask;        /* (class,subclass,prog-if) triplet */
        kernel_ulong_t driver_data;     /* Data private to the driver */
        __u32 override_only;
};

struct old_pci_dev {
  struct device dev;
  struct pci_config_s *pcibios_dev;
  u8 revision;
  u16 vendor;
  u16 device;
  u16 irq;
  u16 subsystem_vendor;
  u16 subsystem_device;
  u32 barflags;
  u32 barlen[6];
};

/*
 * pci_power_t values must match the bits in the Capabilities PME_Support
 * and Control/Status PowerState fields in the Power Management capability.
 */
typedef int __bitwise pci_power_t;

/* File state for mmap()s on /proc/bus/pci/X/Y */
enum pci_mmap_state {
	pci_mmap_io,
	pci_mmap_mem
};

/* For PCI devices, the region numbers are assigned this way: */
enum {
	/* #0-5: standard PCI resources */
	PCI_STD_RESOURCES,
	PCI_STD_RESOURCE_END = PCI_STD_RESOURCES + PCI_STD_NUM_BARS - 1,

	/* #6: expansion ROM resource */
	PCI_ROM_RESOURCE,

	/* Device-specific resources */
#ifdef CONFIG_PCI_IOV
	PCI_IOV_RESOURCES,
	PCI_IOV_RESOURCE_END = PCI_IOV_RESOURCES + PCI_SRIOV_NUM_BARS - 1,
#endif

/* PCI-to-PCI (P2P) bridge windows */
#define PCI_BRIDGE_IO_WINDOW		(PCI_BRIDGE_RESOURCES + 0)
#define PCI_BRIDGE_MEM_WINDOW		(PCI_BRIDGE_RESOURCES + 1)
#define PCI_BRIDGE_PREF_MEM_WINDOW	(PCI_BRIDGE_RESOURCES + 2)

/* CardBus bridge windows */
#define PCI_CB_BRIDGE_IO_0_WINDOW	(PCI_BRIDGE_RESOURCES + 0)
#define PCI_CB_BRIDGE_IO_1_WINDOW	(PCI_BRIDGE_RESOURCES + 1)
#define PCI_CB_BRIDGE_MEM_0_WINDOW	(PCI_BRIDGE_RESOURCES + 2)
#define PCI_CB_BRIDGE_MEM_1_WINDOW	(PCI_BRIDGE_RESOURCES + 3)

/* Total number of bridge resources for P2P and CardBus */
#define PCI_BRIDGE_RESOURCE_NUM 4

	/* Resources assigned to buses behind the bridge */
	PCI_BRIDGE_RESOURCES,
	PCI_BRIDGE_RESOURCE_END = PCI_BRIDGE_RESOURCES +
				  PCI_BRIDGE_RESOURCE_NUM - 1,

	/* Total resources associated with a PCI device */
	PCI_NUM_RESOURCES,

	/* Preserve this for compatibility */
	DEVICE_COUNT_RESOURCE = PCI_NUM_RESOURCES,
};

typedef unsigned short __bitwise pci_dev_flags_t;
enum pci_dev_flags {
	/* INTX_DISABLE in PCI_COMMAND register disables MSI too */
	PCI_DEV_FLAGS_MSI_INTX_DISABLE_BUG = (__force pci_dev_flags_t) (1 << 0),
	/* Device configuration is irrevocably lost if disabled into D3 */
	PCI_DEV_FLAGS_NO_D3 = (__force pci_dev_flags_t) (1 << 1),
	/* Provide indication device is assigned by a Virtual Machine Manager */
	PCI_DEV_FLAGS_ASSIGNED = (__force pci_dev_flags_t) (1 << 2),
	/* Flag for quirk use to store if quirk-specific ACS is enabled */
	PCI_DEV_FLAGS_ACS_ENABLED_QUIRK = (__force pci_dev_flags_t) (1 << 3),
	/* Use a PCIe-to-PCI bridge alias even if !pci_is_pcie */
	PCI_DEV_FLAG_PCIE_BRIDGE_ALIAS = (__force pci_dev_flags_t) (1 << 5),
	/* Do not use bus resets for device */
	PCI_DEV_FLAGS_NO_BUS_RESET = (__force pci_dev_flags_t) (1 << 6),
	/* Do not use PM reset even if device advertises NoSoftRst- */
	PCI_DEV_FLAGS_NO_PM_RESET = (__force pci_dev_flags_t) (1 << 7),
	/* Get VPD from function 0 VPD */
	PCI_DEV_FLAGS_VPD_REF_F0 = (__force pci_dev_flags_t) (1 << 8),
	/* A non-root bridge where translation occurs, stop alias search here */
	PCI_DEV_FLAGS_BRIDGE_XLATE_ROOT = (__force pci_dev_flags_t) (1 << 9),
	/* Do not use FLR even if device advertises PCI_AF_CAP */
	PCI_DEV_FLAGS_NO_FLR_RESET = (__force pci_dev_flags_t) (1 << 10),
	/* Don't use Relaxed Ordering for TLPs directed at this device */
	PCI_DEV_FLAGS_NO_RELAXED_ORDERING = (__force pci_dev_flags_t) (1 << 11),
	/* Device does honor MSI masking despite saying otherwise */
	PCI_DEV_FLAGS_HAS_MSI_MASKING = (__force pci_dev_flags_t) (1 << 12),
};

struct pci_vpd {
	struct mutex	lock;
	unsigned int	len;
	u8		cap;
};

/**
 * typedef pci_channel_state_t
 *
 * The pci_channel state describes connectivity between the CPU and
 * the PCI device.  If some PCI bus between here and the PCI device
 * has crashed or locked up, this info is reflected here.
 */
typedef unsigned int __bitwise pci_channel_state_t;

enum {
	/* I/O channel is in normal state */
	pci_channel_io_normal = (__force pci_channel_state_t) 1,

	/* I/O to channel is blocked */
	pci_channel_io_frozen = (__force pci_channel_state_t) 2,

	/* PCI card is dead */
	pci_channel_io_perm_failure = (__force pci_channel_state_t) 3,
};

typedef unsigned int __bitwise pcie_reset_state_t;

enum pcie_reset_state {
	/* Reset is NOT asserted (Use to deassert reset) */
	pcie_deassert_reset = (__force pcie_reset_state_t) 1,

	/* Use #PERST to reset PCIe device */
	pcie_warm_reset = (__force pcie_reset_state_t) 2,

	/* Use PCIe Hot Reset to reset device */
	pcie_hot_reset = (__force pcie_reset_state_t) 3
};

/* The pci_dev structure describes PCI devices */
struct pci_dev {
  struct pci_config_s *pcibios_dev;
  u32 barflags;
  u32 barlen[6];
	struct list_head bus_list;	/* Node in per-bus list */
	struct pci_bus	*bus;		/* Bus this device is on */
	struct pci_bus	*subordinate;	/* Bus this device bridges to */

	void		*sysdata;	/* Hook for sys-specific extension */
	struct proc_dir_entry *procent;	/* Device entry in /proc/bus/pci */
	struct pci_slot	*slot;		/* Physical slot this device is in */

	unsigned int	devfn;		/* Encoded device & function index */
	unsigned short	vendor;
	unsigned short	device;
	unsigned short	subsystem_vendor;
	unsigned short	subsystem_device;
	unsigned int	class;		/* 3 bytes: (base,sub,prog-if) */
	u8		revision;	/* PCI revision, low byte of class word */
	u8		hdr_type;	/* PCI header type (`multi' flag masked out) */
#ifdef CONFIG_PCIEAER
	u16		aer_cap;	/* AER capability offset */
	struct aer_stats *aer_stats;	/* AER stats for this device */
#endif
#ifdef CONFIG_PCIEPORTBUS
	struct rcec_ea	*rcec_ea;	/* RCEC cached endpoint association */
	struct pci_dev  *rcec;          /* Associated RCEC device */
#endif
	u32		devcap;		/* PCIe Device Capabilities */
	u8		pcie_cap;	/* PCIe capability offset */
	u8		msi_cap;	/* MSI capability offset */
	u8		msix_cap;	/* MSI-X capability offset */
	u8		pcie_mpss:3;	/* PCIe Max Payload Size Supported */
	u8		rom_base_reg;	/* Config register controlling ROM */
	u8		pin;		/* Interrupt pin this device uses */
	u16		pcie_flags_reg;	/* Cached PCIe Capabilities Register */
	unsigned long	*dma_alias_mask;/* Mask of enabled devfn aliases */

	struct pci_driver *driver;	/* Driver bound to this device */
	u64		dma_mask;	/* Mask of the bits of bus address this
					   device implements.  Normally this is
					   0xffffffff.  You only need to change
					   this if your device has broken DMA
					   or supports 64-bit transfers.  */

	struct device_dma_parameters dma_parms;

	pci_power_t	current_state;	/* Current operating state. In ACPI,
					   this is D0-D3, D0 being fully
					   functional, and D3 being off. */
	u8		pm_cap;		/* PM capability offset */
	unsigned int	imm_ready:1;	/* Supports Immediate Readiness */
	unsigned int	pme_support:5;	/* Bitmask of states from which PME#
					   can be generated */
	unsigned int	pme_poll:1;	/* Poll device's PME status bit */
	unsigned int	d1_support:1;	/* Low power state D1 is supported */
	unsigned int	d2_support:1;	/* Low power state D2 is supported */
	unsigned int	no_d1d2:1;	/* D1 and D2 are forbidden */
	unsigned int	no_d3cold:1;	/* D3cold is forbidden */
	unsigned int	bridge_d3:1;	/* Allow D3 for bridge */
	unsigned int	d3cold_allowed:1;	/* D3cold is allowed by user */
	unsigned int	mmio_always_on:1;	/* Disallow turning off io/mem
						   decoding during BAR sizing */
	unsigned int	wakeup_prepared:1;
	unsigned int	skip_bus_pm:1;	/* Internal: Skip bus-level PM */
	unsigned int	ignore_hotplug:1;	/* Ignore hotplug events */
	unsigned int	hotplug_user_indicators:1; /* SlotCtl indicators
						      controlled exclusively by
						      user sysfs */
	unsigned int	clear_retrain_link:1;	/* Need to clear Retrain Link
						   bit manually */
	unsigned int	d3hot_delay;	/* D3hot->D0 transition time in ms */
	unsigned int	d3cold_delay;	/* D3cold->D0 transition time in ms */

#ifdef CONFIG_PCIEASPM
	struct pcie_link_state	*link_state;	/* ASPM link state */
	u16		l1ss;		/* L1SS Capability pointer */
	unsigned int	ltr_path:1;	/* Latency Tolerance Reporting
					   supported from root to here */
#endif
	unsigned int	pasid_no_tlp:1;		/* PASID works without TLP Prefix */
	unsigned int	eetlp_prefix_path:1;	/* End-to-End TLP Prefix */

	pci_channel_state_t error_state;	/* Current connectivity state */
	struct device	dev;			/* Generic device interface */

	int		cfg_size;		/* Size of config space */

	/*
	 * Instead of touching interrupt line and base address registers
	 * directly, use the values stored here. They might be different!
	 */
	unsigned int	irq;
	struct resource resource[DEVICE_COUNT_RESOURCE]; /* I/O and memory regions + expansion ROMs */
	struct resource driver_exclusive_resource;	 /* driver exclusive resource ranges */

	bool		match_driver;		/* Skip attaching driver */

	unsigned int	transparent:1;		/* Subtractive decode bridge */
	unsigned int	io_window:1;		/* Bridge has I/O window */
	unsigned int	pref_window:1;		/* Bridge has pref mem window */
	unsigned int	pref_64_window:1;	/* Pref mem window is 64-bit */
	unsigned int	multifunction:1;	/* Multi-function device */

	unsigned int	is_busmaster:1;		/* Is busmaster */
	unsigned int	no_msi:1;		/* May not use MSI */
	unsigned int	no_64bit_msi:1;		/* May only use 32-bit MSIs */
	unsigned int	block_cfg_access:1;	/* Config space access blocked */
	unsigned int	broken_parity_status:1;	/* Generates false positive parity */
	unsigned int	irq_reroute_variant:2;	/* Needs IRQ rerouting variant */
	unsigned int	msi_enabled:1;
	unsigned int	msix_enabled:1;
	unsigned int	ari_enabled:1;		/* ARI forwarding */
	unsigned int	ats_enabled:1;		/* Address Translation Svc */
	unsigned int	pasid_enabled:1;	/* Process Address Space ID */
	unsigned int	pri_enabled:1;		/* Page Request Interface */
	unsigned int	is_managed:1;		/* Managed via devres */
	unsigned int	is_msi_managed:1;	/* MSI release via devres installed */
	unsigned int	needs_freset:1;		/* Requires fundamental reset */
	unsigned int	state_saved:1;
	unsigned int	is_physfn:1;
	unsigned int	is_virtfn:1;
	unsigned int	is_hotplug_bridge:1;
	unsigned int	shpc_managed:1;		/* SHPC owned by shpchp */
	unsigned int	is_thunderbolt:1;	/* Thunderbolt controller */
	/*
	 * Devices marked being untrusted are the ones that can potentially
	 * execute DMA attacks and similar. They are typically connected
	 * through external ports such as Thunderbolt but not limited to
	 * that. When an IOMMU is enabled they should be getting full
	 * mappings to make sure they cannot access arbitrary memory.
	 */
	unsigned int	untrusted:1;
	/*
	 * Info from the platform, e.g., ACPI or device tree, may mark a
	 * device as "external-facing".  An external-facing device is
	 * itself internal but devices downstream from it are external.
	 */
	unsigned int	external_facing:1;
	unsigned int	broken_intx_masking:1;	/* INTx masking can't be used */
	unsigned int	io_window_1k:1;		/* Intel bridge 1K I/O windows */
	unsigned int	irq_managed:1;
	unsigned int	non_compliant_bars:1;	/* Broken BARs; ignore them */
	unsigned int	is_probed:1;		/* Device probing in progress */
	unsigned int	link_active_reporting:1;/* Device capable of reporting link active */
	unsigned int	no_vf_scan:1;		/* Don't scan for VFs after IOV enablement */
	unsigned int	no_command_memory:1;	/* No PCI_COMMAND_MEMORY */
	unsigned int	rom_bar_overlap:1;	/* ROM BAR disable broken */
	unsigned int	rom_attr_enabled:1;	/* Display of ROM attribute enabled? */
	pci_dev_flags_t dev_flags;
	atomic_t	enable_cnt;	/* pci_enable_device has been called */

	spinlock_t	pcie_cap_lock;		/* Protects RMW ops in capability accessors */
	u32		saved_config_space[16]; /* Config space saved at suspend time */
	struct hlist_head saved_cap_space;
	struct bin_attribute *res_attr[DEVICE_COUNT_RESOURCE]; /* sysfs file for resources */
	struct bin_attribute *res_attr_wc[DEVICE_COUNT_RESOURCE]; /* sysfs file for WC mapping of resources */

#ifdef CONFIG_HOTPLUG_PCI_PCIE
	unsigned int	broken_cmd_compl:1;	/* No compl for some cmds */
#endif
#ifdef CONFIG_PCIE_PTM
	u16		ptm_cap;		/* PTM Capability */
	unsigned int	ptm_root:1;
	unsigned int	ptm_enabled:1;
	u8		ptm_granularity;
#endif
#ifdef CONFIG_PCI_MSI
	void __iomem	*msix_base;
	raw_spinlock_t	msi_lock;
#endif
	struct pci_vpd	vpd;
#ifdef CONFIG_PCIE_DPC
	u16		dpc_cap;
	unsigned int	dpc_rp_extensions:1;
	u8		dpc_rp_log_size;
#endif
#ifdef CONFIG_PCI_ATS
	union {
		struct pci_sriov	*sriov;		/* PF: SR-IOV info */
		struct pci_dev		*physfn;	/* VF: related PF */
	};
	u16		ats_cap;	/* ATS Capability offset */
	u8		ats_stu;	/* ATS Smallest Translation Unit */
#endif
#ifdef CONFIG_PCI_PRI
	u16		pri_cap;	/* PRI Capability offset */
	u32		pri_reqs_alloc; /* Number of PRI requests allocated */
	unsigned int	pasid_required:1; /* PRG Response PASID Required */
#endif
#ifdef CONFIG_PCI_PASID
	u16		pasid_cap;	/* PASID Capability offset */
	u16		pasid_features;
#endif
#ifdef CONFIG_PCI_P2PDMA
	struct pci_p2pdma __rcu *p2pdma;
#endif
#ifdef CONFIG_PCI_DOE
	struct xarray	doe_mbs;	/* Data Object Exchange mailboxes */
#endif
	u16		acs_cap;	/* ACS Capability offset */
	phys_addr_t	rom;		/* Physical address if not from BAR */
	size_t		romlen;		/* Length if not from BAR */
	/*
	 * Driver name to force a match.  Do not set directly, because core
	 * frees it.  Use driver_set_override() to set or clear it.
	 */
	const char	*driver_override;

	unsigned long	priv_flags;	/* Private flags for the PCI driver */

	/* These methods index pci_reset_fn_methods[] */
	u8 reset_methods[PCI_NUM_RESET_METHODS]; /* In priority order */
};

#define dev_notice(dev,...) printk(__VA_ARGS__)
#define dev_warn(dev,...) printk(__VA_ARGS__)
#define dev_alert(dev,...) printk(__VA_ARGS__)
#define dev_err(dev,...) printk(__VA_ARGS__)
#define dev_crit(dev,...) printk(__VA_ARGS__)

#if PCI_DEBUG
#define dev_dbg(dev,...) printk(__VA_ARGS__)
#define dev_info(dev,...) printk(__VA_ARGS__)
#define dev_info_ratelimited(dev, fmt, ...)                             \
        printk(fmt, ##__VA_ARGS__)
#else
#define dev_dbg(dev,...) //printk(__VA_ARGS__)
#define dev_info(dev,...) //printk(__VA_ARGS__)
#define dev_info_ratelimited(dev, fmt, ...) //printk(fmt, ##__VA_ARGS__)
#endif

#define dev_WARN_ONCE(dev, condition, format, arg...) \
        WARN_ONCE(condition, "%s %s: " format, \
                        dev_driver_string(dev), dev_name(dev), ## arg)

// linux/pci.h:
#define PCIBIOS_SUCCESSFUL		0x00

#if defined(USE_LINUX_PCIBIOS) && USE_LINUX_PCIBIOS == 1 // Linux prototypes
int pci_read_config_byte(const struct pci_dev *dev, int where, u8 *val);
int pci_read_config_word(const struct pci_dev *dev, int where, u16 *val);
int pci_read_config_dword(const struct pci_dev *dev, int where, u32 *val);
int pci_write_config_byte(const struct pci_dev *dev, int where, u8 val);
int pci_write_config_word(const struct pci_dev *dev, int where, u16 val);
int pci_write_config_dword(const struct pci_dev *dev, int where, u32 val);

extern int pci_enable_device (struct pci_dev *pcidev);
extern int pcim_enable_device (struct pci_dev *pcidev);
extern void pci_disable_device (struct pci_dev *pcidev);
extern void pcim_disable_device (struct pci_dev *pcidev);
extern void pci_set_master (struct pci_dev *pcidev);

int __must_check pci_request_regions(struct pci_dev *, const char *);
void pci_release_regions(struct pci_dev *);
#else
static inline int pci_read_config_dword (struct pci_dev *pcidev, u16 addr, u32 *wordp) {
  *wordp = pcibios_ReadConfig_Dword(pcidev->pcibios_dev, addr);
  return PCIBIOS_SUCCESSFUL;
}

static inline int pci_write_config_dword (struct pci_dev *pcidev, u16 addr, u32 word) {
  pcibios_WriteConfig_Dword(pcidev->pcibios_dev, addr, word);
  return PCIBIOS_SUCCESSFUL;
}

static inline int pci_read_config_word (struct pci_dev *pcidev, u16 addr, u16 *wordp) {
  *wordp = pcibios_ReadConfig_Word(pcidev->pcibios_dev, addr);
  return PCIBIOS_SUCCESSFUL;
}

static inline int pci_write_config_word (struct pci_dev *pcidev, u16 addr, u16 word) {
  pcibios_WriteConfig_Word(pcidev->pcibios_dev, addr, word);
  return PCIBIOS_SUCCESSFUL;
}

static inline int pci_read_config_byte (struct pci_dev *pcidev, u16 addr, u8 *wordp) {
  *wordp = pcibios_ReadConfig_Byte(pcidev->pcibios_dev, addr);
  return PCIBIOS_SUCCESSFUL;
}

static inline int pci_write_config_byte (struct pci_dev *pcidev, u16 addr, u8 word) {
  pcibios_WriteConfig_Byte(pcidev->pcibios_dev, addr, word);
  return PCIBIOS_SUCCESSFUL;
}


static inline int pci_enable_device (struct pci_dev *pcidev) {
 return 0; // XXX
}
static inline int pcim_enable_device (struct pci_dev *pcidev) {
 return 0; // XXX
}
static inline int pci_disable_device (struct pci_dev *pcidev) {
 return 0; // XXX
}
static inline void pci_set_master (struct pci_dev *pcidev) {
  pcibios_set_master(pcidev->pcibios_dev);
}

static inline unsigned int pci_resource_start (struct pci_dev *pcidev, int bar_index) {
  uint32_t addr = pcibios_ReadConfig_Dword(pcidev->pcibios_dev, 0x10 + bar_index * 4);
  addr &= 0xfffffff8;
  return addr;
}

#if 1
// We should probably get the length only when creating the pci_dev structure
static inline unsigned int pci_resource_len (struct pci_dev *pcidev, int bar_index) {
  if (pcidev->barflags & (1<<bar_index)) {
    return pcidev->barlen[bar_index];
  } else {
    uint32_t addr, len;
    addr = pcibios_ReadConfig_Dword(pcidev->pcibios_dev, 0x10 + bar_index * 4);
    pcibios_WriteConfig_Dword(pcidev->pcibios_dev, 0x10 + bar_index * 4, 0xffffffff);
    len = pcibios_ReadConfig_Dword(pcidev->pcibios_dev, 0x10 + bar_index * 4);
    len = ~(len & 0xfffffff0) + 1;
    //printk("bar %u len %u (0x%X)\n", bar_index, len, len);
    pcibios_WriteConfig_Dword(pcidev->pcibios_dev, 0x10 + bar_index * 4, addr);
    pcidev->barlen[bar_index] = len;
    pcidev->barflags |= (1<<bar_index);
    return len;
  }
}
#define ioremap(x,y) (unsigned long)(pds_dpmi_map_physical_memory(x, y))
#else
#define ioremap(x,y) (unsigned long)(pds_dpmi_map_physical_memory(x, 0x200000))
#endif

#define iounmap(x) pds_dpmi_unmap_physycal_memory(x)

#define pci_request_regions(x,nm) 0
#define pci_release_regions(x)

static inline int
pci_resource_flags (struct pci_dev *dev, int bar)
{
  //return (IORESOURCE_IO|IORESOURCE_MEM); // fake both
  if (pci_resource_start(dev, bar) < 0x10000) {
    return IORESOURCE_IO;
  } else {
    return IORESOURCE_MEM;
  }
}

#endif // XXX

extern unsigned long pci_mem_start;
#define PCIBIOS_MIN_IO          0x1000
#define PCIBIOS_MIN_MEM         (pci_mem_start)

/* SPDX-License-Identifier: GPL-2.0 */
/*
 *	pci.h
 *
 *	PCI defines and function prototypes
 *	Copyright 1994, Drew Eckhardt
 *	Copyright 1997--1999 Martin Mares <mj@ucw.cz>
 *
 *	PCI Express ASPM defines and function prototypes
 *	Copyright (c) 2007 Intel Corp.
 *		Zhang Yanmin (yanmin.zhang@intel.com)
 *		Shaohua Li (shaohua.li@intel.com)
 *
 *	For more information, please consult the following manuals (look at
 *	http://www.pcisig.com/ for how to get them):
 *
 *	PCI BIOS Specification
 *	PCI Local Bus Specification
 *	PCI to PCI Bridge Specification
 *	PCI Express Specification
 *	PCI System Design Guide
 */
#ifndef LINUX_PCI_H
#define LINUX_PCI_H

//#include <linux/args.h>
//#include <linux/mod_devicetable.h>
#include "linux/mutex.h"

#include "linux/types.h"
//#include <linux/init.h>
//#include <linux/ioport.h>
#include "linux/list.h"
#include "linux/compiler.h"
//#include <linux/errno.h>
//#include <linux/kobject.h>

static inline const char *kobject_name(const struct kobject *kobj)
{
	return kobj->name;
}

//#include <linux/atomic.h>
///#include <linux/device.h>
//#include <linux/interrupt.h>
//#include <linux/io.h>
//#include <linux/resource_ext.h>
//#include <linux/msi_api.h>
//#include <uapi/linux/pci.h>
/*
 * The PCI interface treats multi-function devices as independent
 * devices.  The slot/function address of each device is encoded
 * in a single byte as follows:
 *
 *      7:3 = slot
 *      2:0 = function
 */
#define PCI_DEVFN(slot, func)   ((((slot) & 0x1f) << 3) | ((func) & 0x07))
#define PCI_SLOT(devfn)         (((devfn) >> 3) & 0x1f)
#define PCI_FUNC(devfn)         ((devfn) & 0x07)

/* Ioctls for /proc/bus/pci/X/Y nodes. */
#define PCIIOC_BASE             ('P' << 24 | 'C' << 16 | 'I' << 8)
#define PCIIOC_CONTROLLER       (PCIIOC_BASE | 0x00)    /* Get controller for PCI device. */
#define PCIIOC_MMAP_IS_IO       (PCIIOC_BASE | 0x01)    /* Set mmap state to I/O space. */
#define PCIIOC_MMAP_IS_MEM      (PCIIOC_BASE | 0x02)    /* Set mmap state to MEM space. */
#define PCIIOC_WRITE_COMBINE    (PCIIOC_BASE | 0x03)    /* Enable/disable write-combining. */

/*
 * The PCI interface treats multi-function devices as independent
 * devices.  The slot/function address of each device is encoded
 * in a single byte as follows:
 *
 *	7:3 = slot
 *	2:0 = function
 *
 * PCI_DEVFN(), PCI_SLOT(), and PCI_FUNC() are defined in uapi/linux/pci.h.
 * In the interest of not exposing interfaces to user-space unnecessarily,
 * the following kernel-only defines are being added here.
 */
#define PCI_DEVID(bus, devfn)	((((u16)(bus)) << 8) | (devfn))
/* return bus from PCI devid = ((u16)bus_number) << 8) | devfn */
#define PCI_BUS_NUM(x) (((x) >> 8) & 0xff)

/* pci_slot represents a physical slot */
struct pci_slot {
	struct pci_bus		*bus;		/* Bus this slot is on */
	struct list_head	list;		/* Node in list of slots */
	struct hotplug_slot	*hotplug;	/* Hotplug info (move here) */
	unsigned char		number;		/* PCI_SLOT(pci_dev->devfn) */
	struct kobject		kobj;
};

static inline const char *pci_slot_name(const struct pci_slot *slot)
{
	return kobject_name(&slot->kobj);
}

/**
 * enum pci_interrupt_pin - PCI INTx interrupt values
 * @PCI_INTERRUPT_UNKNOWN: Unknown or unassigned interrupt
 * @PCI_INTERRUPT_INTA: PCI INTA pin
 * @PCI_INTERRUPT_INTB: PCI INTB pin
 * @PCI_INTERRUPT_INTC: PCI INTC pin
 * @PCI_INTERRUPT_INTD: PCI INTD pin
 *
 * Corresponds to values for legacy PCI INTx interrupts, as can be found in the
 * PCI_INTERRUPT_PIN register.
 */
enum pci_interrupt_pin {
	PCI_INTERRUPT_UNKNOWN,
	PCI_INTERRUPT_INTA,
	PCI_INTERRUPT_INTB,
	PCI_INTERRUPT_INTC,
	PCI_INTERRUPT_INTD,
};

/* The number of legacy PCI INTx interrupts */
#define PCI_NUM_INTX	4

/*
 * Reading from a device that doesn't respond typically returns ~0.  A
 * successful read from a device may also return ~0, so you need additional
 * information to reliably identify errors.
 */
#define PCI_ERROR_RESPONSE		(~0ULL)
#define PCI_SET_ERROR_RESPONSE(val)	(*(val) = ((typeof(*(val))) PCI_ERROR_RESPONSE))
#define PCI_POSSIBLE_ERROR(val)		((val) == ((typeof(val)) PCI_ERROR_RESPONSE))

#define PCI_D0		((pci_power_t __force) 0)
#define PCI_D1		((pci_power_t __force) 1)
#define PCI_D2		((pci_power_t __force) 2)
#define PCI_D3hot	((pci_power_t __force) 3)
#define PCI_D3cold	((pci_power_t __force) 4)
#define PCI_UNKNOWN	((pci_power_t __force) 5)
#define PCI_POWER_ERROR	((pci_power_t __force) -1)

/* Remember to update this when the list above changes! */
extern const char *pci_power_names[];

static inline const char *pci_power_name(pci_power_t state)
{
	return pci_power_names[1 + (__force int) state];
}
enum pci_irq_reroute_variant {
	INTEL_IRQ_REROUTE_VARIANT = 1,
	MAX_IRQ_REROUTE_VARIANTS = 3
};

typedef unsigned short __bitwise pci_bus_flags_t;
enum pci_bus_flags {
	PCI_BUS_FLAGS_NO_MSI	= (__force pci_bus_flags_t) 1,
	PCI_BUS_FLAGS_NO_MMRBC	= (__force pci_bus_flags_t) 2,
	PCI_BUS_FLAGS_NO_AERSID	= (__force pci_bus_flags_t) 4,
	PCI_BUS_FLAGS_NO_EXTCFG	= (__force pci_bus_flags_t) 8,
};

/* Values from Link Status register, PCIe r3.1, sec 7.8.8 */
enum pcie_link_width {
	PCIE_LNK_WIDTH_RESRV	= 0x00,
	PCIE_LNK_X1		= 0x01,
	PCIE_LNK_X2		= 0x02,
	PCIE_LNK_X4		= 0x04,
	PCIE_LNK_X8		= 0x08,
	PCIE_LNK_X12		= 0x0c,
	PCIE_LNK_X16		= 0x10,
	PCIE_LNK_X32		= 0x20,
	PCIE_LNK_WIDTH_UNKNOWN	= 0xff,
};

/* See matching string table in pci_speed_string() */
enum pci_bus_speed {
	PCI_SPEED_33MHz			= 0x00,
	PCI_SPEED_66MHz			= 0x01,
	PCI_SPEED_66MHz_PCIX		= 0x02,
	PCI_SPEED_100MHz_PCIX		= 0x03,
	PCI_SPEED_133MHz_PCIX		= 0x04,
	PCI_SPEED_66MHz_PCIX_ECC	= 0x05,
	PCI_SPEED_100MHz_PCIX_ECC	= 0x06,
	PCI_SPEED_133MHz_PCIX_ECC	= 0x07,
	PCI_SPEED_66MHz_PCIX_266	= 0x09,
	PCI_SPEED_100MHz_PCIX_266	= 0x0a,
	PCI_SPEED_133MHz_PCIX_266	= 0x0b,
	AGP_UNKNOWN			= 0x0c,
	AGP_1X				= 0x0d,
	AGP_2X				= 0x0e,
	AGP_4X				= 0x0f,
	AGP_8X				= 0x10,
	PCI_SPEED_66MHz_PCIX_533	= 0x11,
	PCI_SPEED_100MHz_PCIX_533	= 0x12,
	PCI_SPEED_133MHz_PCIX_533	= 0x13,
	PCIE_SPEED_2_5GT		= 0x14,
	PCIE_SPEED_5_0GT		= 0x15,
	PCIE_SPEED_8_0GT		= 0x16,
	PCIE_SPEED_16_0GT		= 0x17,
	PCIE_SPEED_32_0GT		= 0x18,
	PCIE_SPEED_64_0GT		= 0x19,
	PCI_SPEED_UNKNOWN		= 0xff,
};

enum pci_bus_speed pcie_get_speed_cap(struct pci_dev *dev);
enum pcie_link_width pcie_get_width_cap(struct pci_dev *dev);

struct irq_affinity;
struct pcie_link_state;
struct pci_sriov;
struct pci_p2pdma;
struct rcec_ea;

#if 0
/* The pci_dev structure describes PCI devices */
struct pci_dev {
	struct list_head bus_list;	/* Node in per-bus list */
	struct pci_bus	*bus;		/* Bus this device is on */
	struct pci_bus	*subordinate;	/* Bus this device bridges to */

	void		*sysdata;	/* Hook for sys-specific extension */
	struct proc_dir_entry *procent;	/* Device entry in /proc/bus/pci */
	struct pci_slot	*slot;		/* Physical slot this device is in */

	unsigned int	devfn;		/* Encoded device & function index */
	unsigned short	vendor;
	unsigned short	device;
	unsigned short	subsystem_vendor;
	unsigned short	subsystem_device;
	unsigned int	class;		/* 3 bytes: (base,sub,prog-if) */
	u8		revision;	/* PCI revision, low byte of class word */
	u8		hdr_type;	/* PCI header type (`multi' flag masked out) */
#ifdef CONFIG_PCIEAER
	u16		aer_cap;	/* AER capability offset */
	struct aer_stats *aer_stats;	/* AER stats for this device */
#endif
#ifdef CONFIG_PCIEPORTBUS
	struct rcec_ea	*rcec_ea;	/* RCEC cached endpoint association */
	struct pci_dev  *rcec;          /* Associated RCEC device */
#endif
	u32		devcap;		/* PCIe Device Capabilities */
	u8		pcie_cap;	/* PCIe capability offset */
	u8		msi_cap;	/* MSI capability offset */
	u8		msix_cap;	/* MSI-X capability offset */
	u8		pcie_mpss:3;	/* PCIe Max Payload Size Supported */
	u8		rom_base_reg;	/* Config register controlling ROM */
	u8		pin;		/* Interrupt pin this device uses */
	u16		pcie_flags_reg;	/* Cached PCIe Capabilities Register */
	unsigned long	*dma_alias_mask;/* Mask of enabled devfn aliases */

	struct pci_driver *driver;	/* Driver bound to this device */
	u64		dma_mask;	/* Mask of the bits of bus address this
					   device implements.  Normally this is
					   0xffffffff.  You only need to change
					   this if your device has broken DMA
					   or supports 64-bit transfers.  */

	struct device_dma_parameters dma_parms;

	pci_power_t	current_state;	/* Current operating state. In ACPI,
					   this is D0-D3, D0 being fully
					   functional, and D3 being off. */
	u8		pm_cap;		/* PM capability offset */
	unsigned int	imm_ready:1;	/* Supports Immediate Readiness */
	unsigned int	pme_support:5;	/* Bitmask of states from which PME#
					   can be generated */
	unsigned int	pme_poll:1;	/* Poll device's PME status bit */
	unsigned int	d1_support:1;	/* Low power state D1 is supported */
	unsigned int	d2_support:1;	/* Low power state D2 is supported */
	unsigned int	no_d1d2:1;	/* D1 and D2 are forbidden */
	unsigned int	no_d3cold:1;	/* D3cold is forbidden */
	unsigned int	bridge_d3:1;	/* Allow D3 for bridge */
	unsigned int	d3cold_allowed:1;	/* D3cold is allowed by user */
	unsigned int	mmio_always_on:1;	/* Disallow turning off io/mem
						   decoding during BAR sizing */
	unsigned int	wakeup_prepared:1;
	unsigned int	skip_bus_pm:1;	/* Internal: Skip bus-level PM */
	unsigned int	ignore_hotplug:1;	/* Ignore hotplug events */
	unsigned int	hotplug_user_indicators:1; /* SlotCtl indicators
						      controlled exclusively by
						      user sysfs */
	unsigned int	clear_retrain_link:1;	/* Need to clear Retrain Link
						   bit manually */
	unsigned int	d3hot_delay;	/* D3hot->D0 transition time in ms */
	unsigned int	d3cold_delay;	/* D3cold->D0 transition time in ms */

#ifdef CONFIG_PCIEASPM
	struct pcie_link_state	*link_state;	/* ASPM link state */
	u16		l1ss;		/* L1SS Capability pointer */
	unsigned int	ltr_path:1;	/* Latency Tolerance Reporting
					   supported from root to here */
#endif
	unsigned int	pasid_no_tlp:1;		/* PASID works without TLP Prefix */
	unsigned int	eetlp_prefix_path:1;	/* End-to-End TLP Prefix */

	pci_channel_state_t error_state;	/* Current connectivity state */
	struct device	dev;			/* Generic device interface */

	int		cfg_size;		/* Size of config space */

	/*
	 * Instead of touching interrupt line and base address registers
	 * directly, use the values stored here. They might be different!
	 */
	unsigned int	irq;
	struct resource resource[DEVICE_COUNT_RESOURCE]; /* I/O and memory regions + expansion ROMs */
	struct resource driver_exclusive_resource;	 /* driver exclusive resource ranges */

	bool		match_driver;		/* Skip attaching driver */

	unsigned int	transparent:1;		/* Subtractive decode bridge */
	unsigned int	io_window:1;		/* Bridge has I/O window */
	unsigned int	pref_window:1;		/* Bridge has pref mem window */
	unsigned int	pref_64_window:1;	/* Pref mem window is 64-bit */
	unsigned int	multifunction:1;	/* Multi-function device */

	unsigned int	is_busmaster:1;		/* Is busmaster */
	unsigned int	no_msi:1;		/* May not use MSI */
	unsigned int	no_64bit_msi:1;		/* May only use 32-bit MSIs */
	unsigned int	block_cfg_access:1;	/* Config space access blocked */
	unsigned int	broken_parity_status:1;	/* Generates false positive parity */
	unsigned int	irq_reroute_variant:2;	/* Needs IRQ rerouting variant */
	unsigned int	msi_enabled:1;
	unsigned int	msix_enabled:1;
	unsigned int	ari_enabled:1;		/* ARI forwarding */
	unsigned int	ats_enabled:1;		/* Address Translation Svc */
	unsigned int	pasid_enabled:1;	/* Process Address Space ID */
	unsigned int	pri_enabled:1;		/* Page Request Interface */
	unsigned int	is_managed:1;		/* Managed via devres */
	unsigned int	is_msi_managed:1;	/* MSI release via devres installed */
	unsigned int	needs_freset:1;		/* Requires fundamental reset */
	unsigned int	state_saved:1;
	unsigned int	is_physfn:1;
	unsigned int	is_virtfn:1;
	unsigned int	is_hotplug_bridge:1;
	unsigned int	shpc_managed:1;		/* SHPC owned by shpchp */
	unsigned int	is_thunderbolt:1;	/* Thunderbolt controller */
	/*
	 * Devices marked being untrusted are the ones that can potentially
	 * execute DMA attacks and similar. They are typically connected
	 * through external ports such as Thunderbolt but not limited to
	 * that. When an IOMMU is enabled they should be getting full
	 * mappings to make sure they cannot access arbitrary memory.
	 */
	unsigned int	untrusted:1;
	/*
	 * Info from the platform, e.g., ACPI or device tree, may mark a
	 * device as "external-facing".  An external-facing device is
	 * itself internal but devices downstream from it are external.
	 */
	unsigned int	external_facing:1;
	unsigned int	broken_intx_masking:1;	/* INTx masking can't be used */
	unsigned int	io_window_1k:1;		/* Intel bridge 1K I/O windows */
	unsigned int	irq_managed:1;
	unsigned int	non_compliant_bars:1;	/* Broken BARs; ignore them */
	unsigned int	is_probed:1;		/* Device probing in progress */
	unsigned int	link_active_reporting:1;/* Device capable of reporting link active */
	unsigned int	no_vf_scan:1;		/* Don't scan for VFs after IOV enablement */
	unsigned int	no_command_memory:1;	/* No PCI_COMMAND_MEMORY */
	unsigned int	rom_bar_overlap:1;	/* ROM BAR disable broken */
	unsigned int	rom_attr_enabled:1;	/* Display of ROM attribute enabled? */
	pci_dev_flags_t dev_flags;
	atomic_t	enable_cnt;	/* pci_enable_device has been called */

	spinlock_t	pcie_cap_lock;		/* Protects RMW ops in capability accessors */
	u32		saved_config_space[16]; /* Config space saved at suspend time */
	struct hlist_head saved_cap_space;
	struct bin_attribute *res_attr[DEVICE_COUNT_RESOURCE]; /* sysfs file for resources */
	struct bin_attribute *res_attr_wc[DEVICE_COUNT_RESOURCE]; /* sysfs file for WC mapping of resources */

#ifdef CONFIG_HOTPLUG_PCI_PCIE
	unsigned int	broken_cmd_compl:1;	/* No compl for some cmds */
#endif
#ifdef CONFIG_PCIE_PTM
	u16		ptm_cap;		/* PTM Capability */
	unsigned int	ptm_root:1;
	unsigned int	ptm_enabled:1;
	u8		ptm_granularity;
#endif
#ifdef CONFIG_PCI_MSI
	void __iomem	*msix_base;
	raw_spinlock_t	msi_lock;
#endif
	struct pci_vpd	vpd;
#ifdef CONFIG_PCIE_DPC
	u16		dpc_cap;
	unsigned int	dpc_rp_extensions:1;
	u8		dpc_rp_log_size;
#endif
#ifdef CONFIG_PCI_ATS
	union {
		struct pci_sriov	*sriov;		/* PF: SR-IOV info */
		struct pci_dev		*physfn;	/* VF: related PF */
	};
	u16		ats_cap;	/* ATS Capability offset */
	u8		ats_stu;	/* ATS Smallest Translation Unit */
#endif
#ifdef CONFIG_PCI_PRI
	u16		pri_cap;	/* PRI Capability offset */
	u32		pri_reqs_alloc; /* Number of PRI requests allocated */
	unsigned int	pasid_required:1; /* PRG Response PASID Required */
#endif
#ifdef CONFIG_PCI_PASID
	u16		pasid_cap;	/* PASID Capability offset */
	u16		pasid_features;
#endif
#ifdef CONFIG_PCI_P2PDMA
	struct pci_p2pdma __rcu *p2pdma;
#endif
#ifdef CONFIG_PCI_DOE
	struct xarray	doe_mbs;	/* Data Object Exchange mailboxes */
#endif
	u16		acs_cap;	/* ACS Capability offset */
	phys_addr_t	rom;		/* Physical address if not from BAR */
	size_t		romlen;		/* Length if not from BAR */
	/*
	 * Driver name to force a match.  Do not set directly, because core
	 * frees it.  Use driver_set_override() to set or clear it.
	 */
	const char	*driver_override;

	unsigned long	priv_flags;	/* Private flags for the PCI driver */

	/* These methods index pci_reset_fn_methods[] */
	u8 reset_methods[PCI_NUM_RESET_METHODS]; /* In priority order */
};
#endif

static inline struct pci_dev *pci_physfn(struct pci_dev *dev)
{
#ifdef CONFIG_PCI_IOV
	if (dev->is_virtfn)
		dev = dev->physfn;
#endif
	return dev;
}

struct pci_dev *pci_alloc_dev(struct pci_bus *bus);

#define	to_pci_dev(n) container_of(n, struct pci_dev, dev)
#define for_each_pci_dev(d) while ((d = pci_get_device(PCI_ANY_ID, PCI_ANY_ID, d)) != NULL)

static inline int pci_channel_offline(struct pci_dev *pdev)
{
	return (pdev->error_state != pci_channel_io_normal);
}

/*
 * Currently in ACPI spec, for each PCI host bridge, PCI Segment
 * Group number is limited to a 16-bit value, therefore (int)-1 is
 * not a valid PCI domain number, and can be used as a sentinel
 * value indicating ->domain_nr is not set by the driver (and
 * CONFIG_PCI_DOMAINS_GENERIC=y archs will set it with
 * pci_bus_find_domain_nr()).
 */
#define PCI_DOMAIN_NR_NOT_SET (-1)

struct pci_host_bridge {
	struct device	dev;
	struct pci_bus	*bus;		/* Root bus */
	struct pci_ops	*ops;
	struct pci_ops	*child_ops;
	void		*sysdata;
	int		busnr;
	int		domain_nr;
	struct list_head windows;	/* resource_entry */
	struct list_head dma_ranges;	/* dma ranges resource list */
	u8 (*swizzle_irq)(struct pci_dev *, u8 *); /* Platform IRQ swizzler */
	int (*map_irq)(const struct pci_dev *, u8, u8);
	void (*release_fn)(struct pci_host_bridge *);
	void		*release_data;
	unsigned int	ignore_reset_delay:1;	/* For entire hierarchy */
	unsigned int	no_ext_tags:1;		/* No Extended Tags */
	unsigned int	no_inc_mrrs:1;		/* No Increase MRRS */
	unsigned int	native_aer:1;		/* OS may use PCIe AER */
	unsigned int	native_pcie_hotplug:1;	/* OS may use PCIe hotplug */
	unsigned int	native_shpc_hotplug:1;	/* OS may use SHPC hotplug */
	unsigned int	native_pme:1;		/* OS may use PCIe PME */
	unsigned int	native_ltr:1;		/* OS may use PCIe LTR */
	unsigned int	native_dpc:1;		/* OS may use PCIe DPC */
	unsigned int	native_cxl_error:1;	/* OS may use CXL RAS/Events */
	unsigned int	preserve_config:1;	/* Preserve FW resource setup */
	unsigned int	size_windows:1;		/* Enable root bus sizing */
	unsigned int	msi_domain:1;		/* Bridge wants MSI domain */

	/* Resource alignment requirements */
	resource_size_t (*align_resource)(struct pci_dev *dev,
			const struct resource *res,
			resource_size_t start,
			resource_size_t size,
			resource_size_t align);
	unsigned long	private[] /*____cacheline_aligned*/;
};

#define	to_pci_host_bridge(n) container_of(n, struct pci_host_bridge, dev)

static inline void *pci_host_bridge_priv(struct pci_host_bridge *bridge)
{
	return (void *)bridge->private;
}

static inline struct pci_host_bridge *pci_host_bridge_from_priv(void *priv)
{
	return container_of(priv, struct pci_host_bridge, private);
}

struct pci_host_bridge *pci_alloc_host_bridge(size_t priv);
struct pci_host_bridge *devm_pci_alloc_host_bridge(struct device *dev,
						   size_t priv);
void pci_free_host_bridge(struct pci_host_bridge *bridge);
struct pci_host_bridge *pci_find_host_bridge(struct pci_bus *bus);

void pci_set_host_bridge_release(struct pci_host_bridge *bridge,
				 void (*release_fn)(struct pci_host_bridge *),
				 void *release_data);

int pcibios_root_bridge_prepare(struct pci_host_bridge *bridge);

/*
 * The first PCI_BRIDGE_RESOURCE_NUM PCI bus resources (those that correspond
 * to P2P or CardBus bridge windows) go in a table.  Additional ones (for
 * buses below host bridges or subtractive decode bridges) go in the list.
 * Use pci_bus_for_each_resource() to iterate through all the resources.
 */

/*
 * PCI_SUBTRACTIVE_DECODE means the bridge forwards the window implicitly
 * and there's no way to program the bridge with the details of the window.
 * This does not apply to ACPI _CRS windows, even with the _DEC subtractive-
 * decode bit set, because they are explicit and can be programmed with _SRS.
 */
#define PCI_SUBTRACTIVE_DECODE	0x1

struct pci_bus_resource {
	struct list_head	list;
	struct resource		*res;
	unsigned int		flags;
};

#define PCI_REGION_FLAG_MASK	0x0fU	/* These bits of resource flags tell us the PCI region flags */

struct pci_bus {
	struct list_head node;		/* Node in list of buses */
	struct pci_bus	*parent;	/* Parent bus this bridge is on */
	struct list_head children;	/* List of child buses */
	struct list_head devices;	/* List of devices on this bus */
	struct pci_dev	*self;		/* Bridge device as seen by parent */
	struct list_head slots;		/* List of slots on this bus;
					   protected by pci_slot_mutex */
	struct resource *resource[PCI_BRIDGE_RESOURCE_NUM];
	struct list_head resources;	/* Address space routed to this bus */
	struct resource busn_res;	/* Bus numbers routed to this bus */

	struct pci_ops	*ops;		/* Configuration access functions */
	void		*sysdata;	/* Hook for sys-specific extension */
	struct proc_dir_entry *procdir;	/* Directory entry in /proc/bus/pci */

	unsigned char	number;		/* Bus number */
	unsigned char	primary;	/* Number of primary bridge */
	unsigned char	max_bus_speed;	/* enum pci_bus_speed */
	unsigned char	cur_bus_speed;	/* enum pci_bus_speed */
#ifdef CONFIG_PCI_DOMAINS_GENERIC
	int		domain_nr;
#endif

	char		name[48];

	unsigned short	bridge_ctl;	/* Manage NO_ISA/FBB/et al behaviors */
	pci_bus_flags_t bus_flags;	/* Inherited by child buses */
	struct device		*bridge;
	struct device		dev;
	struct bin_attribute	*legacy_io;	/* Legacy I/O for this bus */
	struct bin_attribute	*legacy_mem;	/* Legacy mem */
	unsigned int		is_added:1;
	unsigned int		unsafe_warn:1;	/* warned about RW1C config write */
};

#define to_pci_bus(n)	container_of(n, struct pci_bus, dev)

static inline u16 pci_dev_id(struct pci_dev *dev)
{
	return PCI_DEVID(dev->bus->number, dev->devfn);
}

/*
 * Returns true if the PCI bus is root (behind host-PCI bridge),
 * false otherwise
 *
 * Some code assumes that "bus->self == NULL" means that bus is a root bus.
 * This is incorrect because "virtual" buses added for SR-IOV (via
 * virtfn_add_bus()) have "bus->self == NULL" but are not root buses.
 */
static inline bool pci_is_root_bus(struct pci_bus *pbus)
{
	return !(pbus->parent);
}

/**
 * pci_is_bridge - check if the PCI device is a bridge
 * @dev: PCI device
 *
 * Return true if the PCI device is bridge whether it has subordinate
 * or not.
 */
static inline bool pci_is_bridge(struct pci_dev *dev)
{
	return dev->hdr_type == PCI_HEADER_TYPE_BRIDGE ||
		dev->hdr_type == PCI_HEADER_TYPE_CARDBUS;
}

#define for_each_pci_bridge(dev, bus)				\
	list_for_each_entry(dev, &bus->devices, bus_list)	\
		if (!pci_is_bridge(dev)) {} else

static inline struct pci_dev *pci_upstream_bridge(struct pci_dev *dev)
{
	dev = pci_physfn(dev);
	if (pci_is_root_bus(dev->bus))
		return NULL;

	return dev->bus->self;
}

#ifdef CONFIG_PCI_MSI
static inline bool pci_dev_msi_enabled(struct pci_dev *pci_dev)
{
	return pci_dev->msi_enabled || pci_dev->msix_enabled;
}
#else
static inline bool pci_dev_msi_enabled(struct pci_dev *pci_dev) { return false; }
#endif

/* Error values that may be returned by PCI functions */
#define PCIBIOS_SUCCESSFUL		0x00
#define PCIBIOS_FUNC_NOT_SUPPORTED	0x81
#define PCIBIOS_BAD_VENDOR_ID		0x83
#define PCIBIOS_DEVICE_NOT_FOUND	0x86
#define PCIBIOS_BAD_REGISTER_NUMBER	0x87
#define PCIBIOS_SET_FAILED		0x88
#define PCIBIOS_BUFFER_TOO_SMALL	0x89

/* Translate above to generic errno for passing back through non-PCI code */
static inline int pcibios_err_to_errno(int err)
{
	if (err <= PCIBIOS_SUCCESSFUL)
		return err; /* Assume already errno */

	switch (err) {
	case PCIBIOS_FUNC_NOT_SUPPORTED:
		return -ENOENT;
	case PCIBIOS_BAD_VENDOR_ID:
		return -ENOTTY;
	case PCIBIOS_DEVICE_NOT_FOUND:
		return -ENODEV;
	case PCIBIOS_BAD_REGISTER_NUMBER:
		return -EFAULT;
	case PCIBIOS_SET_FAILED:
		return -EIO;
	case PCIBIOS_BUFFER_TOO_SMALL:
		return -ENOSPC;
	}

	return -ERANGE;
}

/* Low-level architecture-dependent routines */

struct pci_ops {
	int (*add_bus)(struct pci_bus *bus);
	void (*remove_bus)(struct pci_bus *bus);
	void __iomem *(*map_bus)(struct pci_bus *bus, unsigned int devfn, int where);
	int (*read)(struct pci_bus *bus, unsigned int devfn, int where, int size, u32 *val);
	int (*write)(struct pci_bus *bus, unsigned int devfn, int where, int size, u32 val);
};

/*
 * ACPI needs to be able to access PCI config space before we've done a
 * PCI bus scan and created pci_bus structures.
 */
int raw_pci_read(unsigned int domain, unsigned int bus, unsigned int devfn,
		 int reg, int len, u32 *val);
int raw_pci_write(unsigned int domain, unsigned int bus, unsigned int devfn,
		  int reg, int len, u32 val);

#ifdef CONFIG_ARCH_DMA_ADDR_T_64BIT
typedef u64 pci_bus_addr_t;
#else
typedef u32 pci_bus_addr_t;
#endif

struct pci_bus_region {
	pci_bus_addr_t	start;
	pci_bus_addr_t	end;
};

struct pci_dynids {
	spinlock_t		lock;	/* Protects list, index */
	struct list_head	list;	/* For IDs added at runtime */
};


/*
 * PCI Error Recovery System (PCI-ERS).  If a PCI device driver provides
 * a set of callbacks in struct pci_error_handlers, that device driver
 * will be notified of PCI bus errors, and will be driven to recovery
 * when an error occurs.
 */

typedef unsigned int __bitwise pci_ers_result_t;

enum pci_ers_result {
	/* No result/none/not supported in device driver */
	PCI_ERS_RESULT_NONE = (__force pci_ers_result_t) 1,

	/* Device driver can recover without slot reset */
	PCI_ERS_RESULT_CAN_RECOVER = (__force pci_ers_result_t) 2,

	/* Device driver wants slot to be reset */
	PCI_ERS_RESULT_NEED_RESET = (__force pci_ers_result_t) 3,

	/* Device has completely failed, is unrecoverable */
	PCI_ERS_RESULT_DISCONNECT = (__force pci_ers_result_t) 4,

	/* Device driver is fully recovered and operational */
	PCI_ERS_RESULT_RECOVERED = (__force pci_ers_result_t) 5,

	/* No AER capabilities registered for the driver */
	PCI_ERS_RESULT_NO_AER_DRIVER = (__force pci_ers_result_t) 6,
};

/* PCI bus error event callbacks */
struct pci_error_handlers {
	/* PCI bus error detected on this device */
	pci_ers_result_t (*error_detected)(struct pci_dev *dev,
					   pci_channel_state_t error);

	/* MMIO has been re-enabled, but not DMA */
	pci_ers_result_t (*mmio_enabled)(struct pci_dev *dev);

	/* PCI slot has been reset */
	pci_ers_result_t (*slot_reset)(struct pci_dev *dev);

	/* PCI function reset prepare or completed */
	void (*reset_prepare)(struct pci_dev *dev);
	void (*reset_done)(struct pci_dev *dev);

	/* Device driver may resume normal operations */
	void (*resume)(struct pci_dev *dev);

	/* Allow device driver to record more details of a correctable error */
	void (*cor_error_detected)(struct pci_dev *dev);
};


struct module;

/**
 * struct pci_driver - PCI driver structure
 * @node:	List of driver structures.
 * @name:	Driver name.
 * @id_table:	Pointer to table of device IDs the driver is
 *		interested in.  Most drivers should export this
 *		table using MODULE_DEVICE_TABLE(pci,...).
 * @probe:	This probing function gets called (during execution
 *		of pci_register_driver() for already existing
 *		devices or later if a new device gets inserted) for
 *		all PCI devices which match the ID table and are not
 *		"owned" by the other drivers yet. This function gets
 *		passed a "struct pci_dev \*" for each device whose
 *		entry in the ID table matches the device. The probe
 *		function returns zero when the driver chooses to
 *		take "ownership" of the device or an error code
 *		(negative number) otherwise.
 *		The probe function always gets called from process
 *		context, so it can sleep.
 * @remove:	The remove() function gets called whenever a device
 *		being handled by this driver is removed (either during
 *		deregistration of the driver or when it's manually
 *		pulled out of a hot-pluggable slot).
 *		The remove function always gets called from process
 *		context, so it can sleep.
 * @suspend:	Put device into low power state.
 * @resume:	Wake device from low power state.
 *		(Please see Documentation/power/pci.rst for descriptions
 *		of PCI Power Management and the related functions.)
 * @shutdown:	Hook into reboot_notifier_list (kernel/sys.c).
 *		Intended to stop any idling DMA operations.
 *		Useful for enabling wake-on-lan (NIC) or changing
 *		the power state of a device before reboot.
 *		e.g. drivers/net/e100.c.
 * @sriov_configure: Optional driver callback to allow configuration of
 *		number of VFs to enable via sysfs "sriov_numvfs" file.
 * @sriov_set_msix_vec_count: PF Driver callback to change number of MSI-X
 *              vectors on a VF. Triggered via sysfs "sriov_vf_msix_count".
 *              This will change MSI-X Table Size in the VF Message Control
 *              registers.
 * @sriov_get_vf_total_msix: PF driver callback to get the total number of
 *              MSI-X vectors available for distribution to the VFs.
 * @err_handler: See Documentation/PCI/pci-error-recovery.rst
 * @groups:	Sysfs attribute groups.
 * @dev_groups: Attributes attached to the device that will be
 *              created once it is bound to the driver.
 * @driver:	Driver model structure.
 * @dynids:	List of dynamically added device IDs.
 * @driver_managed_dma: Device driver doesn't use kernel DMA API for DMA.
 *		For most device drivers, no need to care about this flag
 *		as long as all DMAs are handled through the kernel DMA API.
 *		For some special ones, for example VFIO drivers, they know
 *		how to manage the DMA themselves and set this flag so that
 *		the IOMMU layer will allow them to setup and manage their
 *		own I/O address space.
 */
struct pci_driver {
	struct list_head	node;
	const char		*name;
	const struct pci_device_id *id_table;	/* Must be non-NULL for probe to be called */
	int  (*probe)(struct pci_dev *dev, const struct pci_device_id *id);	/* New device inserted */
	void (*remove)(struct pci_dev *dev);	/* Device removed (NULL if not a hot-plug capable driver) */
	int  (*suspend)(struct pci_dev *dev, pm_message_t state);	/* Device suspended */
	int  (*resume)(struct pci_dev *dev);	/* Device woken up */
	void (*shutdown)(struct pci_dev *dev);
	int  (*sriov_configure)(struct pci_dev *dev, int num_vfs); /* On PF */
	int  (*sriov_set_msix_vec_count)(struct pci_dev *vf, int msix_vec_count); /* On PF */
	u32  (*sriov_get_vf_total_msix)(struct pci_dev *pf);
	const struct pci_error_handlers *err_handler;
	const struct attribute_group **groups;
	const struct attribute_group **dev_groups;
	struct device_driver	driver;
	struct pci_dynids	dynids;
	bool driver_managed_dma;
};

static inline struct pci_driver *to_pci_driver(struct device_driver *drv)
{
    return drv ? container_of(drv, struct pci_driver, driver) : NULL;
}

/**
 * PCI_DEVICE - macro used to describe a specific PCI device
 * @vend: the 16 bit PCI Vendor ID
 * @dev: the 16 bit PCI Device ID
 *
 * This macro is used to create a struct pci_device_id that matches a
 * specific device.  The subvendor and subdevice fields will be set to
 * PCI_ANY_ID.
 */
#define PCI_DEVICE(vend,dev) \
	.vendor = (vend), .device = (dev), \
	.subvendor = PCI_ANY_ID, .subdevice = PCI_ANY_ID

/**
 * PCI_DEVICE_DRIVER_OVERRIDE - macro used to describe a PCI device with
 *                              override_only flags.
 * @vend: the 16 bit PCI Vendor ID
 * @dev: the 16 bit PCI Device ID
 * @driver_override: the 32 bit PCI Device override_only
 *
 * This macro is used to create a struct pci_device_id that matches only a
 * driver_override device. The subvendor and subdevice fields will be set to
 * PCI_ANY_ID.
 */
#define PCI_DEVICE_DRIVER_OVERRIDE(vend, dev, driver_override) \
	.vendor = (vend), .device = (dev), .subvendor = PCI_ANY_ID, \
	.subdevice = PCI_ANY_ID, .override_only = (driver_override)

/**
 * PCI_DRIVER_OVERRIDE_DEVICE_VFIO - macro used to describe a VFIO
 *                                   "driver_override" PCI device.
 * @vend: the 16 bit PCI Vendor ID
 * @dev: the 16 bit PCI Device ID
 *
 * This macro is used to create a struct pci_device_id that matches a
 * specific device. The subvendor and subdevice fields will be set to
 * PCI_ANY_ID and the driver_override will be set to
 * PCI_ID_F_VFIO_DRIVER_OVERRIDE.
 */
#define PCI_DRIVER_OVERRIDE_DEVICE_VFIO(vend, dev) \
	PCI_DEVICE_DRIVER_OVERRIDE(vend, dev, PCI_ID_F_VFIO_DRIVER_OVERRIDE)

/**
 * PCI_DEVICE_SUB - macro used to describe a specific PCI device with subsystem
 * @vend: the 16 bit PCI Vendor ID
 * @dev: the 16 bit PCI Device ID
 * @subvend: the 16 bit PCI Subvendor ID
 * @subdev: the 16 bit PCI Subdevice ID
 *
 * This macro is used to create a struct pci_device_id that matches a
 * specific device with subsystem information.
 */
#define PCI_DEVICE_SUB(vend, dev, subvend, subdev) \
	.vendor = (vend), .device = (dev), \
	.subvendor = (subvend), .subdevice = (subdev)

/**
 * PCI_DEVICE_CLASS - macro used to describe a specific PCI device class
 * @dev_class: the class, subclass, prog-if triple for this device
 * @dev_class_mask: the class mask for this device
 *
 * This macro is used to create a struct pci_device_id that matches a
 * specific PCI class.  The vendor, device, subvendor, and subdevice
 * fields will be set to PCI_ANY_ID.
 */
#define PCI_DEVICE_CLASS(dev_class,dev_class_mask) \
	.class = (dev_class), .class_mask = (dev_class_mask), \
	.vendor = PCI_ANY_ID, .device = PCI_ANY_ID, \
	.subvendor = PCI_ANY_ID, .subdevice = PCI_ANY_ID

/**
 * PCI_VDEVICE - macro used to describe a specific PCI device in short form
 * @vend: the vendor name
 * @dev: the 16 bit PCI Device ID
 *
 * This macro is used to create a struct pci_device_id that matches a
 * specific PCI device.  The subvendor, and subdevice fields will be set
 * to PCI_ANY_ID. The macro allows the next field to follow as the device
 * private data.
 */
#define PCI_VDEVICE(vend, dev) \
	.vendor = PCI_VENDOR_ID_##vend, .device = (dev), \
	.subvendor = PCI_ANY_ID, .subdevice = PCI_ANY_ID, 0, 0

/**
 * PCI_DEVICE_DATA - macro used to describe a specific PCI device in very short form
 * @vend: the vendor name (without PCI_VENDOR_ID_ prefix)
 * @dev: the device name (without PCI_DEVICE_ID_<vend>_ prefix)
 * @data: the driver data to be filled
 *
 * This macro is used to create a struct pci_device_id that matches a
 * specific PCI device.  The subvendor, and subdevice fields will be set
 * to PCI_ANY_ID.
 */
#define PCI_DEVICE_DATA(vend, dev, data) \
	.vendor = PCI_VENDOR_ID_##vend, .device = PCI_DEVICE_ID_##vend##_##dev, \
	.subvendor = PCI_ANY_ID, .subdevice = PCI_ANY_ID, 0, 0, \
	.driver_data = (kernel_ulong_t)(data)

enum {
	PCI_REASSIGN_ALL_RSRC	= 0x00000001,	/* Ignore firmware setup */
	PCI_REASSIGN_ALL_BUS	= 0x00000002,	/* Reassign all bus numbers */
	PCI_PROBE_ONLY		= 0x00000004,	/* Use existing setup */
	PCI_CAN_SKIP_ISA_ALIGN	= 0x00000008,	/* Don't do ISA alignment */
	PCI_ENABLE_PROC_DOMAINS	= 0x00000010,	/* Enable domains in /proc */
	PCI_COMPAT_DOMAIN_0	= 0x00000020,	/* ... except domain 0 */
	PCI_SCAN_ALL_PCIE_DEVS	= 0x00000040,	/* Scan all, not just dev 0 */
};

#define PCI_IRQ_LEGACY		(1 << 0) /* Allow legacy interrupts */
#define PCI_IRQ_MSI		(1 << 1) /* Allow MSI interrupts */
#define PCI_IRQ_MSIX		(1 << 2) /* Allow MSI-X interrupts */
#define PCI_IRQ_AFFINITY	(1 << 3) /* Auto-assign affinity */

/* These external functions are only available when PCI support is enabled */
#ifdef CONFIG_PCI

extern unsigned int pci_flags;

static inline void pci_set_flags(int flags) { pci_flags = flags; }
static inline void pci_add_flags(int flags) { pci_flags |= flags; }
static inline void pci_clear_flags(int flags) { pci_flags &= ~flags; }
static inline int pci_has_flag(int flag) { return pci_flags & flag; }

void pcie_bus_configure_settings(struct pci_bus *bus);

enum pcie_bus_config_types {
	PCIE_BUS_TUNE_OFF,	/* Don't touch MPS at all */
	PCIE_BUS_DEFAULT,	/* Ensure MPS matches upstream bridge */
	PCIE_BUS_SAFE,		/* Use largest MPS boot-time devices support */
	PCIE_BUS_PERFORMANCE,	/* Use MPS and MRRS for best performance */
	PCIE_BUS_PEER2PEER,	/* Set MPS = 128 for all devices */
};

extern enum pcie_bus_config_types pcie_bus_config;

extern /*const*/ struct bus_type pci_bus_type;

/* Do NOT directly access these two variables, unless you are arch-specific PCI
 * code, or PCI core code. */
extern struct list_head pci_root_buses;	/* List of all known PCI buses */
/* Some device drivers need know if PCI is initiated */
int no_pci_devices(void);

void pcibios_resource_survey_bus(struct pci_bus *bus);
void pcibios_bus_add_device(struct pci_dev *pdev);
void pcibios_add_bus(struct pci_bus *bus);
void pcibios_remove_bus(struct pci_bus *bus);
void pcibios_fixup_bus(struct pci_bus *);
int __must_check pcibios_enable_device(struct pci_dev *, int mask);
/* Architecture-specific versions may override this (weak) */
char *pcibios_setup(char *str);

/* Used only when drivers/pci/setup.c is used */
resource_size_t pcibios_align_resource(void *, const struct resource *,
				resource_size_t,
				resource_size_t);

/* Weak but can be overridden by arch */
void pci_fixup_cardbus(struct pci_bus *);

/* Generic PCI functions used internally */

void pcibios_resource_to_bus(struct pci_bus *bus, struct pci_bus_region *region,
			     struct resource *res);
void pcibios_bus_to_resource(struct pci_bus *bus, struct resource *res,
			     struct pci_bus_region *region);
void pcibios_scan_specific_bus(int busn);
struct pci_bus *pci_find_bus(int domain, int busnr);
void pci_bus_add_devices(const struct pci_bus *bus);
struct pci_bus *pci_scan_bus(int bus, struct pci_ops *ops, void *sysdata);
struct pci_bus *pci_create_root_bus(struct device *parent, int bus,
				    struct pci_ops *ops, void *sysdata,
				    struct list_head *resources);
int pci_host_probe(struct pci_host_bridge *bridge);
int pci_bus_insert_busn_res(struct pci_bus *b, int bus, int busmax);
int pci_bus_update_busn_res_end(struct pci_bus *b, int busmax);
void pci_bus_release_busn_res(struct pci_bus *b);
struct pci_bus *pci_scan_root_bus(struct device *parent, int bus,
				  struct pci_ops *ops, void *sysdata,
				  struct list_head *resources);
int pci_scan_root_bus_bridge(struct pci_host_bridge *bridge);
struct pci_bus *pci_add_new_bus(struct pci_bus *parent, struct pci_dev *dev,
				int busnr);
struct pci_slot *pci_create_slot(struct pci_bus *parent, int slot_nr,
				 const char *name,
				 struct hotplug_slot *hotplug);
void pci_destroy_slot(struct pci_slot *slot);
#ifdef CONFIG_SYSFS
void pci_dev_assign_slot(struct pci_dev *dev);
#else
static inline void pci_dev_assign_slot(struct pci_dev *dev) { }
#endif
int pci_scan_slot(struct pci_bus *bus, int devfn);
struct pci_dev *pci_scan_single_device(struct pci_bus *bus, int devfn);
void pci_device_add(struct pci_dev *dev, struct pci_bus *bus);
unsigned int pci_scan_child_bus(struct pci_bus *bus);
void pci_bus_add_device(struct pci_dev *dev);
void pci_read_bridge_bases(struct pci_bus *child);
struct resource *pci_find_parent_resource(const struct pci_dev *dev,
					  struct resource *res);
u8 pci_swizzle_interrupt_pin(const struct pci_dev *dev, u8 pin);
int pci_get_interrupt_pin(struct pci_dev *dev, struct pci_dev **bridge);
u8 pci_common_swizzle(struct pci_dev *dev, u8 *pinp);
//struct pci_dev *pci_dev_get(struct pci_dev *dev);
//void pci_dev_put(struct pci_dev *dev);
static inline struct pci_dev *pci_dev_get(struct pci_dev *dev) {
  return dev;
}
static inline void pci_dev_put(struct pci_dev *dev) {
}
void pci_remove_bus(struct pci_bus *b);
void pci_stop_and_remove_bus_device(struct pci_dev *dev);
void pci_stop_and_remove_bus_device_locked(struct pci_dev *dev);
void pci_stop_root_bus(struct pci_bus *bus);
void pci_remove_root_bus(struct pci_bus *bus);
void pci_setup_cardbus(struct pci_bus *bus);
void pcibios_setup_bridge(struct pci_bus *bus, unsigned long type);
void pci_sort_breadthfirst(void);
#define dev_is_pci(d) ((d)->bus == &pci_bus_type)
#define dev_is_pf(d) ((dev_is_pci(d) ? to_pci_dev(d)->is_physfn : false))

/* Generic PCI functions exported to card drivers */

u8 pci_bus_find_capability(struct pci_bus *bus, unsigned int devfn, int cap);
u8 pci_find_capability(struct pci_dev *dev, int cap);
u8 pci_find_next_capability(struct pci_dev *dev, u8 pos, int cap);
u8 pci_find_ht_capability(struct pci_dev *dev, int ht_cap);
u8 pci_find_next_ht_capability(struct pci_dev *dev, u8 pos, int ht_cap);
u16 pci_find_ext_capability(struct pci_dev *dev, int cap);
u16 pci_find_next_ext_capability(struct pci_dev *dev, u16 pos, int cap);
struct pci_bus *pci_find_next_bus(const struct pci_bus *from);
u16 pci_find_vsec_capability(struct pci_dev *dev, u16 vendor, int cap);
u16 pci_find_dvsec_capability(struct pci_dev *dev, u16 vendor, u16 dvsec);

u64 pci_get_dsn(struct pci_dev *dev);

struct pci_dev *pci_get_device(unsigned int vendor, unsigned int device,
			       struct pci_dev *from);
struct pci_dev *pci_get_subsys(unsigned int vendor, unsigned int device,
			       unsigned int ss_vendor, unsigned int ss_device,
			       struct pci_dev *from);
struct pci_dev *pci_get_slot(struct pci_bus *bus, unsigned int devfn);
struct pci_dev *pci_get_domain_bus_and_slot(int domain, unsigned int bus,
					    unsigned int devfn);
struct pci_dev *pci_get_class(unsigned int class, struct pci_dev *from);
int pci_dev_present(const struct pci_device_id *ids);

#if 0
int pci_bus_read_config_byte(struct pci_bus *bus, unsigned int devfn,
			     int where, u8 *val);
int pci_bus_read_config_word(struct pci_bus *bus, unsigned int devfn,
			     int where, u16 *val);
int pci_bus_read_config_dword(struct pci_bus *bus, unsigned int devfn,
			      int where, u32 *val);
int pci_bus_write_config_byte(struct pci_bus *bus, unsigned int devfn,
			      int where, u8 val);
int pci_bus_write_config_word(struct pci_bus *bus, unsigned int devfn,
			      int where, u16 val);
int pci_bus_write_config_dword(struct pci_bus *bus, unsigned int devfn,
			       int where, u32 val);
#endif

int pci_generic_config_read(struct pci_bus *bus, unsigned int devfn,
			    int where, int size, u32 *val);
int pci_generic_config_write(struct pci_bus *bus, unsigned int devfn,
			    int where, int size, u32 val);
int pci_generic_config_read32(struct pci_bus *bus, unsigned int devfn,
			      int where, int size, u32 *val);
int pci_generic_config_write32(struct pci_bus *bus, unsigned int devfn,
			       int where, int size, u32 val);

struct pci_ops *pci_bus_set_ops(struct pci_bus *bus, struct pci_ops *ops);

int pcie_capability_read_word(struct pci_dev *dev, int pos, u16 *val);
int pcie_capability_read_dword(struct pci_dev *dev, int pos, u32 *val);
int pcie_capability_write_word(struct pci_dev *dev, int pos, u16 val);
int pcie_capability_write_dword(struct pci_dev *dev, int pos, u32 val);
int pcie_capability_clear_and_set_word_unlocked(struct pci_dev *dev, int pos,
						u16 clear, u16 set);
int pcie_capability_clear_and_set_word_locked(struct pci_dev *dev, int pos,
					      u16 clear, u16 set);
int pcie_capability_clear_and_set_dword(struct pci_dev *dev, int pos,
					u32 clear, u32 set);

/**
 * pcie_capability_clear_and_set_word - RMW accessor for PCI Express Capability Registers
 * @dev:	PCI device structure of the PCI Express device
 * @pos:	PCI Express Capability Register
 * @clear:	Clear bitmask
 * @set:	Set bitmask
 *
 * Perform a Read-Modify-Write (RMW) operation using @clear and @set
 * bitmasks on PCI Express Capability Register at @pos. Certain PCI Express
 * Capability Registers are accessed concurrently in RMW fashion, hence
 * require locking which is handled transparently to the caller.
 */
static inline int pcie_capability_clear_and_set_word(struct pci_dev *dev,
						     int pos,
						     u16 clear, u16 set)
{
	switch (pos) {
	case PCI_EXP_LNKCTL:
	case PCI_EXP_RTCTL:
		return pcie_capability_clear_and_set_word_locked(dev, pos,
								 clear, set);
	default:
		return pcie_capability_clear_and_set_word_unlocked(dev, pos,
								   clear, set);
	}
}

static inline int pcie_capability_set_word(struct pci_dev *dev, int pos,
					   u16 set)
{
	return pcie_capability_clear_and_set_word(dev, pos, 0, set);
}

static inline int pcie_capability_set_dword(struct pci_dev *dev, int pos,
					    u32 set)
{
	return pcie_capability_clear_and_set_dword(dev, pos, 0, set);
}

static inline int pcie_capability_clear_word(struct pci_dev *dev, int pos,
					     u16 clear)
{
	return pcie_capability_clear_and_set_word(dev, pos, clear, 0);
}

static inline int pcie_capability_clear_dword(struct pci_dev *dev, int pos,
					      u32 clear)
{
	return pcie_capability_clear_and_set_dword(dev, pos, clear, 0);
}

/* User-space driven config access */
int pci_user_read_config_byte(struct pci_dev *dev, int where, u8 *val);
int pci_user_read_config_word(struct pci_dev *dev, int where, u16 *val);
int pci_user_read_config_dword(struct pci_dev *dev, int where, u32 *val);
int pci_user_write_config_byte(struct pci_dev *dev, int where, u8 val);
int pci_user_write_config_word(struct pci_dev *dev, int where, u16 val);
int pci_user_write_config_dword(struct pci_dev *dev, int where, u32 val);

int __must_check pci_enable_device(struct pci_dev *dev);
int __must_check pci_enable_device_io(struct pci_dev *dev);
int __must_check pci_enable_device_mem(struct pci_dev *dev);
int __must_check pci_reenable_device(struct pci_dev *);
int __must_check pcim_enable_device(struct pci_dev *pdev);
void pcim_pin_device(struct pci_dev *pdev);

static inline bool pci_intx_mask_supported(struct pci_dev *pdev)
{
	/*
	 * INTx masking is supported if PCI_COMMAND_INTX_DISABLE is
	 * writable and no quirk has marked the feature broken.
	 */
	return !pdev->broken_intx_masking;
}

static inline int pci_is_enabled(struct pci_dev *pdev)
{
	return (atomic_read(&pdev->enable_cnt) > 0);
}

static inline int pci_is_managed(struct pci_dev *pdev)
{
	return pdev->is_managed;
}

//void pci_disable_device(struct pci_dev *dev);

extern unsigned int pcibios_max_latency;
//void pci_set_master(struct pci_dev *dev);
//void pci_clear_master(struct pci_dev *dev);

int pci_set_pcie_reset_state(struct pci_dev *dev, enum pcie_reset_state state);
int pci_set_cacheline_size(struct pci_dev *dev);
int __must_check pci_set_mwi(struct pci_dev *dev);
int __must_check pcim_set_mwi(struct pci_dev *dev);
int pci_try_set_mwi(struct pci_dev *dev);
void pci_clear_mwi(struct pci_dev *dev);
void pci_disable_parity(struct pci_dev *dev);
void pci_intx(struct pci_dev *dev, int enable);
bool pci_check_and_mask_intx(struct pci_dev *dev);
bool pci_check_and_unmask_intx(struct pci_dev *dev);
int pci_wait_for_pending(struct pci_dev *dev, int pos, u16 mask);
int pci_wait_for_pending_transaction(struct pci_dev *dev);
int pcix_get_max_mmrbc(struct pci_dev *dev);
int pcix_get_mmrbc(struct pci_dev *dev);
int pcix_set_mmrbc(struct pci_dev *dev, int mmrbc);
int pcie_get_readrq(struct pci_dev *dev);
int pcie_set_readrq(struct pci_dev *dev, int rq);
int pcie_get_mps(struct pci_dev *dev);
int pcie_set_mps(struct pci_dev *dev, int mps);
u32 pcie_bandwidth_available(struct pci_dev *dev, struct pci_dev **limiting_dev,
			     enum pci_bus_speed *speed,
			     enum pcie_link_width *width);
void pcie_print_link_status(struct pci_dev *dev);
int pcie_reset_flr(struct pci_dev *dev, bool probe);
int pcie_flr(struct pci_dev *dev);
int __pci_reset_function_locked(struct pci_dev *dev);
int pci_reset_function(struct pci_dev *dev);
int pci_reset_function_locked(struct pci_dev *dev);
int pci_try_reset_function(struct pci_dev *dev);
int pci_probe_reset_slot(struct pci_slot *slot);
int pci_probe_reset_bus(struct pci_bus *bus);
int pci_reset_bus(struct pci_dev *dev);
void pci_reset_secondary_bus(struct pci_dev *dev);
void pcibios_reset_secondary_bus(struct pci_dev *dev);
void pci_update_resource(struct pci_dev *dev, int resno);
int __must_check pci_assign_resource(struct pci_dev *dev, int i);
int __must_check pci_reassign_resource(struct pci_dev *dev, int i, resource_size_t add_size, resource_size_t align);
void pci_release_resource(struct pci_dev *dev, int resno);
#include "linux/bitops.h"
#include "linux/log2.h"
#if 0
/**
 * __roundup_pow_of_two() - round up to nearest power of two
 * @n: value to round up
 */
static inline __attribute__((const))
unsigned long roundup_pow_of_two(unsigned long n)
{
        return 1UL << fls_long(n - 1);
}
#endif
static inline int pci_rebar_bytes_to_size(u64 bytes)
{
	bytes = roundup_pow_of_two(bytes);

	/* Return BAR size as defined in the resizable BAR specification */
	return max(ilog2(bytes), 20) - 20;
}

u32 pci_rebar_get_possible_sizes(struct pci_dev *pdev, int bar);
int __must_check pci_resize_resource(struct pci_dev *dev, int i, int size);
int pci_select_bars(struct pci_dev *dev, unsigned long flags);
bool pci_device_is_present(struct pci_dev *pdev);
void pci_ignore_hotplug(struct pci_dev *dev);
struct pci_dev *pci_real_dma_dev(struct pci_dev *dev);
int pci_status_get_and_clear_errors(struct pci_dev *pdev);

int __printf(6, 7) pci_request_irq(struct pci_dev *dev, unsigned int nr,
		irq_handler_t handler, irq_handler_t thread_fn, void *dev_id,
		const char *fmt, ...);
void pci_free_irq(struct pci_dev *dev, unsigned int nr, void *dev_id);

/* ROM control related routines */
int pci_enable_rom(struct pci_dev *pdev);
void pci_disable_rom(struct pci_dev *pdev);
void __iomem __must_check *pci_map_rom(struct pci_dev *pdev, size_t *size);
void pci_unmap_rom(struct pci_dev *pdev, void __iomem *rom);

/* Power management related routines */
int pci_save_state(struct pci_dev *dev);
void pci_restore_state(struct pci_dev *dev);
struct pci_saved_state *pci_store_saved_state(struct pci_dev *dev);
int pci_load_saved_state(struct pci_dev *dev,
			 struct pci_saved_state *state);
int pci_load_and_free_saved_state(struct pci_dev *dev,
				  struct pci_saved_state **state);
int pci_platform_power_transition(struct pci_dev *dev, pci_power_t state);
int pci_set_power_state(struct pci_dev *dev, pci_power_t state);
pci_power_t pci_choose_state(struct pci_dev *dev, pm_message_t state);
bool pci_pme_capable(struct pci_dev *dev, pci_power_t state);
void pci_pme_active(struct pci_dev *dev, bool enable);
int pci_enable_wake(struct pci_dev *dev, pci_power_t state, bool enable);
int pci_wake_from_d3(struct pci_dev *dev, bool enable);
int pci_prepare_to_sleep(struct pci_dev *dev);
int pci_back_from_sleep(struct pci_dev *dev);
bool pci_dev_run_wake(struct pci_dev *dev);
void pci_d3cold_enable(struct pci_dev *dev);
void pci_d3cold_disable(struct pci_dev *dev);
bool pcie_relaxed_ordering_enabled(struct pci_dev *dev);
void pci_resume_bus(struct pci_bus *bus);
void pci_bus_set_current_state(struct pci_bus *bus, pci_power_t state);

/* For use by arch with custom probe code */
void set_pcie_port_type(struct pci_dev *pdev);
void set_pcie_hotplug_bridge(struct pci_dev *pdev);

/* Functions for PCI Hotplug drivers to use */
unsigned int pci_rescan_bus_bridge_resize(struct pci_dev *bridge);
unsigned int pci_rescan_bus(struct pci_bus *bus);
void pci_lock_rescan_remove(void);
void pci_unlock_rescan_remove(void);

/* Vital Product Data routines */
ssize_t pci_read_vpd(struct pci_dev *dev, loff_t pos, size_t count, void *buf);
ssize_t pci_write_vpd(struct pci_dev *dev, loff_t pos, size_t count, const void *buf);
ssize_t pci_read_vpd_any(struct pci_dev *dev, loff_t pos, size_t count, void *buf);
ssize_t pci_write_vpd_any(struct pci_dev *dev, loff_t pos, size_t count, const void *buf);

/* Helper functions for low-level code (drivers/pci/setup-[bus,res].c) */
resource_size_t pcibios_retrieve_fw_addr(struct pci_dev *dev, int idx);
void pci_bus_assign_resources(const struct pci_bus *bus);
void pci_bus_claim_resources(struct pci_bus *bus);
void pci_bus_size_bridges(struct pci_bus *bus);
int pci_claim_resource(struct pci_dev *, int);
int pci_claim_bridge_resource(struct pci_dev *bridge, int i);
void pci_assign_unassigned_resources(void);
void pci_assign_unassigned_bridge_resources(struct pci_dev *bridge);
void pci_assign_unassigned_bus_resources(struct pci_bus *bus);
void pci_assign_unassigned_root_bus_resources(struct pci_bus *bus);
int pci_reassign_bridge_resources(struct pci_dev *bridge, unsigned long type);
int pci_enable_resources(struct pci_dev *, int mask);
void pci_assign_irq(struct pci_dev *dev);
struct resource *pci_find_resource(struct pci_dev *dev, struct resource *res);
#define HAVE_PCI_REQ_REGIONS	2
int __must_check pci_request_regions_exclusive(struct pci_dev *, const char *);
int __must_check pci_request_region(struct pci_dev *, int, const char *);
void pci_release_region(struct pci_dev *, int);
int pci_request_selected_regions(struct pci_dev *, int, const char *);
int pci_request_selected_regions_exclusive(struct pci_dev *, int, const char *);
void pci_release_selected_regions(struct pci_dev *, int);

static inline __must_check struct resource *
pci_request_config_region_exclusive(struct pci_dev *pdev, unsigned int offset,
				    unsigned int len, const char *name)
{
	return __request_region(&pdev->driver_exclusive_resource, offset, len,
				name, IORESOURCE_EXCLUSIVE);
}

static inline void pci_release_config_region(struct pci_dev *pdev,
					     unsigned int offset,
					     unsigned int len)
{
	__release_region(&pdev->driver_exclusive_resource, offset, len);
}

/* drivers/pci/bus.c */
void pci_add_resource(struct list_head *resources, struct resource *res);
void pci_add_resource_offset(struct list_head *resources, struct resource *res,
			     resource_size_t offset);
void pci_free_resource_list(struct list_head *resources);
void pci_bus_add_resource(struct pci_bus *bus, struct resource *res,
			  unsigned int flags);
struct resource *pci_bus_resource_n(const struct pci_bus *bus, int n);
void pci_bus_remove_resources(struct pci_bus *bus);
void pci_bus_remove_resource(struct pci_bus *bus, struct resource *res);
int devm_request_pci_bus_resources(struct device *dev,
				   struct list_head *resources);

/* Temporary until new and working PCI SBR API in place */
int pci_bridge_secondary_bus_reset(struct pci_dev *dev);

#define __pci_bus_for_each_res0(bus, res, ...)				\
	for (unsigned int __b = 0;					\
	     (res = pci_bus_resource_n(bus, __b)) || __b < PCI_BRIDGE_RESOURCE_NUM; \
	     __b++)

#define __pci_bus_for_each_res1(bus, res, __b)				\
	for (__b = 0;							\
	     (res = pci_bus_resource_n(bus, __b)) || __b < PCI_BRIDGE_RESOURCE_NUM; \
	     __b++)

/**
 * pci_bus_for_each_resource - iterate over PCI bus resources
 * @bus: the PCI bus
 * @res: pointer to the current resource
 * @...: optional index of the current resource
 *
 * Iterate over PCI bus resources. The first part is to go over PCI bus
 * resource array, which has at most the %PCI_BRIDGE_RESOURCE_NUM entries.
 * After that continue with the separate list of the additional resources,
 * if not empty. That's why the Logical OR is being used.
 *
 * Possible usage:
 *
 *	struct pci_bus *bus = ...;
 *	struct resource *res;
 *	unsigned int i;
 *
 * 	// With optional index
 * 	pci_bus_for_each_resource(bus, res, i)
 * 		pr_info("PCI bus resource[%u]: %pR\n", i, res);
 *
 * 	// Without index
 * 	pci_bus_for_each_resource(bus, res)
 * 		_do_something_(res);
 */
#define pci_bus_for_each_resource(bus, res, ...)			\
	CONCATENATE(__pci_bus_for_each_res, COUNT_ARGS(__VA_ARGS__))	\
		    (bus, res, __VA_ARGS__)

int __must_check pci_bus_alloc_resource(struct pci_bus *bus,
			struct resource *res, resource_size_t size,
			resource_size_t align, resource_size_t min,
			unsigned long type_mask,
			resource_size_t (*alignf)(void *,
						  const struct resource *,
						  resource_size_t,
						  resource_size_t),
			void *alignf_data);

struct fwnode_handle;

int pci_register_io_range(struct fwnode_handle *fwnode, phys_addr_t addr,
			resource_size_t size);
unsigned long pci_address_to_pio(phys_addr_t addr);
phys_addr_t pci_pio_to_address(unsigned long pio);
int pci_remap_iospace(const struct resource *res, phys_addr_t phys_addr);
int devm_pci_remap_iospace(struct device *dev, const struct resource *res,
			   phys_addr_t phys_addr);
void pci_unmap_iospace(struct resource *res);
void __iomem *devm_pci_remap_cfgspace(struct device *dev,
				      resource_size_t offset,
				      resource_size_t size);
void __iomem *devm_pci_remap_cfg_resource(struct device *dev,
					  struct resource *res);

static inline pci_bus_addr_t pci_bus_address(struct pci_dev *pdev, int bar)
{
	struct pci_bus_region region;

	pcibios_resource_to_bus(pdev->bus, &region, &pdev->resource[bar]);
	return region.start;
}

/* Proper probing supporting hot-pluggable devices */
int __must_check __pci_register_driver(struct pci_driver *, struct module *,
				       const char *mod_name);

/* pci_register_driver() must be a macro so KBUILD_MODNAME can be expanded */
#define pci_register_driver(driver)		\
	__pci_register_driver(driver, THIS_MODULE, KBUILD_MODNAME)

void pci_unregister_driver(struct pci_driver *dev);

/**
 * module_pci_driver() - Helper macro for registering a PCI driver
 * @__pci_driver: pci_driver struct
 *
 * Helper macro for PCI drivers which do not do anything special in module
 * init/exit. This eliminates a lot of boilerplate. Each module may only
 * use this macro once, and calling it replaces module_init() and module_exit()
 */
#define module_pci_driver(__pci_driver) \
	module_driver(__pci_driver, pci_register_driver, pci_unregister_driver)

/**
 * builtin_pci_driver() - Helper macro for registering a PCI driver
 * @__pci_driver: pci_driver struct
 *
 * Helper macro for PCI drivers which do not do anything special in their
 * init code. This eliminates a lot of boilerplate. Each driver may only
 * use this macro once, and calling it replaces device_initcall(...)
 */
#define builtin_pci_driver(__pci_driver) \
	builtin_driver(__pci_driver, pci_register_driver)

struct pci_driver *pci_dev_driver(const struct pci_dev *dev);
int pci_add_dynid(struct pci_driver *drv,
		  unsigned int vendor, unsigned int device,
		  unsigned int subvendor, unsigned int subdevice,
		  unsigned int class, unsigned int class_mask,
		  unsigned long driver_data);
//const struct pci_device_id *pci_match_id(const struct pci_device_id *ids,
//					 struct pci_dev *dev);
// pci_match_one_device from pci/pci.h
/**
 * pci_match_one_device - Tell if a PCI device structure has a matching
 *                        PCI device id structure
 * @id: single PCI device id structure to match
 * @dev: the PCI device structure to match against
 *
 * Returns the matching pci_device_id structure or %NULL if there is no match.
 */
static inline const struct pci_device_id *
pci_match_one_device(const struct pci_device_id *id, const struct pci_dev *dev)
{
        if ((id->vendor == PCI_ANY_ID || id->vendor == dev->vendor) &&
            (id->device == PCI_ANY_ID || id->device == dev->device) &&
            (id->subvendor == PCI_ANY_ID || id->subvendor == dev->subsystem_vendor) &&
            (id->subdevice == PCI_ANY_ID || id->subdevice == dev->subsystem_device) &&
            !((id->class ^ dev->class) & id->class_mask))
                return id;
        return NULL;
}
/**
 * pci_match_id - See if a PCI device matches a given pci_id table
 * @ids: array of PCI device ID structures to search in
 * @dev: the PCI device structure to match against.
 *
 * Used by a driver to check whether a PCI device is in its list of
 * supported devices.  Returns the matching pci_device_id structure or
 * %NULL if there is no match.
 *
 * Deprecated; don't use this as it will not catch any dynamic IDs
 * that a driver might want to check for.
 */
static inline
const struct pci_device_id *pci_match_id(const struct pci_device_id *ids,
                                         struct pci_dev *dev)
{
        if (ids) {
                while (ids->vendor || ids->subvendor || ids->class_mask) {
                        if (pci_match_one_device(ids, dev))
                                return ids;
                        ids++;
                }
        }
        return NULL;
}
EXPORT_SYMBOL(pci_match_id);
int pci_scan_bridge(struct pci_bus *bus, struct pci_dev *dev, int max,
		    int pass);

void pci_walk_bus(struct pci_bus *top, int (*cb)(struct pci_dev *, void *),
		  void *userdata);
int pci_cfg_space_size(struct pci_dev *dev);
unsigned char pci_bus_max_busnr(struct pci_bus *bus);
void pci_setup_bridge(struct pci_bus *bus);
resource_size_t pcibios_window_alignment(struct pci_bus *bus,
					 unsigned long type);

#define PCI_VGA_STATE_CHANGE_BRIDGE (1 << 0)
#define PCI_VGA_STATE_CHANGE_DECODES (1 << 1)

int pci_set_vga_state(struct pci_dev *pdev, bool decode,
		      unsigned int command_bits, u32 flags);

/*
 * Virtual interrupts allow for more interrupts to be allocated
 * than the device has interrupts for. These are not programmed
 * into the device's MSI-X table and must be handled by some
 * other driver means.
 */
#define PCI_IRQ_VIRTUAL		(1 << 4)

#define PCI_IRQ_ALL_TYPES \
	(PCI_IRQ_LEGACY | PCI_IRQ_MSI | PCI_IRQ_MSIX)

//#include <linux/dmapool.h>

struct msix_entry {
	u32	vector;	/* Kernel uses to write allocated vector */
	u16	entry;	/* Driver uses to specify entry, OS writes */
};

struct msi_domain_template;

#ifdef CONFIG_PCI_MSI
int pci_msi_vec_count(struct pci_dev *dev);
void pci_disable_msi(struct pci_dev *dev);
int pci_msix_vec_count(struct pci_dev *dev);
void pci_disable_msix(struct pci_dev *dev);
void pci_restore_msi_state(struct pci_dev *dev);
int pci_msi_enabled(void);
int pci_enable_msi(struct pci_dev *dev);
int pci_enable_msix_range(struct pci_dev *dev, struct msix_entry *entries,
			  int minvec, int maxvec);
static inline int pci_enable_msix_exact(struct pci_dev *dev,
					struct msix_entry *entries, int nvec)
{
	int rc = pci_enable_msix_range(dev, entries, nvec, nvec);
	if (rc < 0)
		return rc;
	return 0;
}
int pci_alloc_irq_vectors(struct pci_dev *dev, unsigned int min_vecs,
			  unsigned int max_vecs, unsigned int flags);
int pci_alloc_irq_vectors_affinity(struct pci_dev *dev, unsigned int min_vecs,
				   unsigned int max_vecs, unsigned int flags,
				   struct irq_affinity *affd);

bool pci_msix_can_alloc_dyn(struct pci_dev *dev);
struct msi_map pci_msix_alloc_irq_at(struct pci_dev *dev, unsigned int index,
				     const struct irq_affinity_desc *affdesc);
void pci_msix_free_irq(struct pci_dev *pdev, struct msi_map map);

void pci_free_irq_vectors(struct pci_dev *dev);
int pci_irq_vector(struct pci_dev *dev, unsigned int nr);
const struct cpumask *pci_irq_get_affinity(struct pci_dev *pdev, int vec);
bool pci_create_ims_domain(struct pci_dev *pdev, const struct msi_domain_template *template,
			   unsigned int hwsize, void *data);
struct msi_map pci_ims_alloc_irq(struct pci_dev *pdev, union msi_instance_cookie *icookie,
				 const struct irq_affinity_desc *affdesc);
void pci_ims_free_irq(struct pci_dev *pdev, struct msi_map map);

#else
static inline int pci_msi_vec_count(struct pci_dev *dev) { return -ENOSYS; }
static inline void pci_disable_msi(struct pci_dev *dev) { }
static inline int pci_msix_vec_count(struct pci_dev *dev) { return -ENOSYS; }
static inline void pci_disable_msix(struct pci_dev *dev) { }
static inline void pci_restore_msi_state(struct pci_dev *dev) { }
static inline int pci_msi_enabled(void) { return 0; }
static inline int pci_enable_msi(struct pci_dev *dev)
{ return -ENOSYS; }
static inline int pci_enable_msix_range(struct pci_dev *dev,
			struct msix_entry *entries, int minvec, int maxvec)
{ return -ENOSYS; }
static inline int pci_enable_msix_exact(struct pci_dev *dev,
			struct msix_entry *entries, int nvec)
{ return -ENOSYS; }

static inline int
pci_alloc_irq_vectors_affinity(struct pci_dev *dev, unsigned int min_vecs,
			       unsigned int max_vecs, unsigned int flags,
			       struct irq_affinity *aff_desc)
{
	if ((flags & PCI_IRQ_LEGACY) && min_vecs == 1 && dev->irq)
		return 1;
	return -ENOSPC;
}
static inline int
pci_alloc_irq_vectors(struct pci_dev *dev, unsigned int min_vecs,
		      unsigned int max_vecs, unsigned int flags)
{
	return pci_alloc_irq_vectors_affinity(dev, min_vecs, max_vecs,
					      flags, NULL);
}

static inline bool pci_msix_can_alloc_dyn(struct pci_dev *dev)
{ return false; }
static inline struct msi_map pci_msix_alloc_irq_at(struct pci_dev *dev, unsigned int index,
						   const struct irq_affinity_desc *affdesc)
{
	struct msi_map map = { .index = -ENOSYS, };

	return map;
}

static inline void pci_msix_free_irq(struct pci_dev *pdev, struct msi_map map)
{
}

static inline void pci_free_irq_vectors(struct pci_dev *dev)
{
}

#ifndef WARN_ON_ONCE
#define WARN_ON_ONCE(x) x
#endif

#ifndef WARN_ONCE
#define WARN_ONCE(...) __VA_ARGS__
#endif

static inline int pci_irq_vector(struct pci_dev *dev, unsigned int nr)
{
	if (WARN_ON_ONCE(nr > 0))
		return -EINVAL;
	return dev->irq;
}
static inline const struct cpumask *pci_irq_get_affinity(struct pci_dev *pdev,
		int vec)
{
	return cpu_possible_mask;
}

static inline bool pci_create_ims_domain(struct pci_dev *pdev,
					 const struct msi_domain_template *template,
					 unsigned int hwsize, void *data)
{ return false; }

union msi_instance_cookie;

static inline struct msi_map pci_ims_alloc_irq(struct pci_dev *pdev,
					       union msi_instance_cookie *icookie,
					       const struct irq_affinity_desc *affdesc)
{
	struct msi_map map = { .index = -ENOSYS, };

	return map;
}

static inline void pci_ims_free_irq(struct pci_dev *pdev, struct msi_map map)
{
}

#endif

/**
 * pci_irqd_intx_xlate() - Translate PCI INTx value to an IRQ domain hwirq
 * @d: the INTx IRQ domain
 * @node: the DT node for the device whose interrupt we're translating
 * @intspec: the interrupt specifier data from the DT
 * @intsize: the number of entries in @intspec
 * @out_hwirq: pointer at which to write the hwirq number
 * @out_type: pointer at which to write the interrupt type
 *
 * Translate a PCI INTx interrupt number from device tree in the range 1-4, as
 * stored in the standard PCI_INTERRUPT_PIN register, to a value in the range
 * 0-3 suitable for use in a 4 entry IRQ domain. That is, subtract one from the
 * INTx value to obtain the hwirq number.
 *
 * Returns 0 on success, or -EINVAL if the interrupt specifier is out of range.
 */
static inline int pci_irqd_intx_xlate(struct irq_domain *d,
				      struct device_node *node,
				      const u32 *intspec,
				      unsigned int intsize,
				      unsigned long *out_hwirq,
				      unsigned int *out_type)
{
	const u32 intx = intspec[0];

	if (intx < PCI_INTERRUPT_INTA || intx > PCI_INTERRUPT_INTD)
		return -EINVAL;

	*out_hwirq = intx - PCI_INTERRUPT_INTA;
	return 0;
}

#ifdef CONFIG_PCIEPORTBUS
extern bool pcie_ports_disabled;
extern bool pcie_ports_native;
#else
#define pcie_ports_disabled	true
#define pcie_ports_native	false
#endif

#define PCIE_LINK_STATE_L0S		BIT(0)
#define PCIE_LINK_STATE_L1		BIT(1)
#define PCIE_LINK_STATE_CLKPM		BIT(2)
#define PCIE_LINK_STATE_L1_1		BIT(3)
#define PCIE_LINK_STATE_L1_2		BIT(4)
#define PCIE_LINK_STATE_L1_1_PCIPM	BIT(5)
#define PCIE_LINK_STATE_L1_2_PCIPM	BIT(6)
#define PCIE_LINK_STATE_ALL		(PCIE_LINK_STATE_L0S | PCIE_LINK_STATE_L1 |\
					 PCIE_LINK_STATE_CLKPM | PCIE_LINK_STATE_L1_1 |\
					 PCIE_LINK_STATE_L1_2 | PCIE_LINK_STATE_L1_1_PCIPM |\
					 PCIE_LINK_STATE_L1_2_PCIPM)

#ifdef CONFIG_PCIEASPM
int pci_disable_link_state(struct pci_dev *pdev, int state);
int pci_disable_link_state_locked(struct pci_dev *pdev, int state);
int pci_enable_link_state(struct pci_dev *pdev, int state);
int pci_enable_link_state_locked(struct pci_dev *pdev, int state);
void pcie_no_aspm(void);
bool pcie_aspm_support_enabled(void);
bool pcie_aspm_enabled(struct pci_dev *pdev);
#else
static inline int pci_disable_link_state(struct pci_dev *pdev, int state)
{ return 0; }
static inline int pci_disable_link_state_locked(struct pci_dev *pdev, int state)
{ return 0; }
static inline int pci_enable_link_state(struct pci_dev *pdev, int state)
{ return 0; }
static inline int pci_enable_link_state_locked(struct pci_dev *pdev, int state)
{ return 0; }
static inline void pcie_no_aspm(void) { }
static inline bool pcie_aspm_support_enabled(void) { return false; }
static inline bool pcie_aspm_enabled(struct pci_dev *pdev) { return false; }
#endif

#ifdef CONFIG_PCIEAER
bool pci_aer_available(void);
#else
static inline bool pci_aer_available(void) { return false; }
#endif

bool pci_ats_disabled(void);

#ifdef CONFIG_PCIE_PTM
int pci_enable_ptm(struct pci_dev *dev, u8 *granularity);
void pci_disable_ptm(struct pci_dev *dev);
bool pcie_ptm_enabled(struct pci_dev *dev);
#else
static inline int pci_enable_ptm(struct pci_dev *dev, u8 *granularity)
{ return -EINVAL; }
static inline void pci_disable_ptm(struct pci_dev *dev) { }
static inline bool pcie_ptm_enabled(struct pci_dev *dev)
{ return false; }
#endif

void pci_cfg_access_lock(struct pci_dev *dev);
bool pci_cfg_access_trylock(struct pci_dev *dev);
void pci_cfg_access_unlock(struct pci_dev *dev);

void pci_dev_lock(struct pci_dev *dev);
int pci_dev_trylock(struct pci_dev *dev);
void pci_dev_unlock(struct pci_dev *dev);

/*
 * PCI domain support.  Sometimes called PCI segment (eg by ACPI),
 * a PCI domain is defined to be a set of PCI buses which share
 * configuration space.
 */
#ifdef CONFIG_PCI_DOMAINS
extern int pci_domains_supported;
#else
enum { pci_domains_supported = 0 };
static inline int pci_domain_nr(struct pci_bus *bus) { return 0; }
static inline int pci_proc_domain(struct pci_bus *bus) { return 0; }
#endif /* CONFIG_PCI_DOMAINS */

/*
 * Generic implementation for PCI domain support. If your
 * architecture does not need custom management of PCI
 * domains then this implementation will be used
 */
#ifdef CONFIG_PCI_DOMAINS_GENERIC
static inline int pci_domain_nr(struct pci_bus *bus)
{
	return bus->domain_nr;
}
#ifdef CONFIG_ACPI
int acpi_pci_bus_find_domain_nr(struct pci_bus *bus);
#else
static inline int acpi_pci_bus_find_domain_nr(struct pci_bus *bus)
{ return 0; }
#endif
int pci_bus_find_domain_nr(struct pci_bus *bus, struct device *parent);
void pci_bus_release_domain_nr(struct pci_bus *bus, struct device *parent);
#endif

/* Some architectures require additional setup to direct VGA traffic */
typedef int (*arch_set_vga_state_t)(struct pci_dev *pdev, bool decode,
				    unsigned int command_bits, u32 flags);
void pci_register_set_vga_state(arch_set_vga_state_t func);

static inline int
pci_request_io_regions(struct pci_dev *pdev, const char *name)
{
	return pci_request_selected_regions(pdev,
			    pci_select_bars(pdev, IORESOURCE_IO), name);
}

static inline void
pci_release_io_regions(struct pci_dev *pdev)
{
	return pci_release_selected_regions(pdev,
			    pci_select_bars(pdev, IORESOURCE_IO));
}

static inline int
pci_request_mem_regions(struct pci_dev *pdev, const char *name)
{
	return pci_request_selected_regions(pdev,
			    pci_select_bars(pdev, IORESOURCE_MEM), name);
}

static inline void
pci_release_mem_regions(struct pci_dev *pdev)
{
	return pci_release_selected_regions(pdev,
			    pci_select_bars(pdev, IORESOURCE_MEM));
}
#endif /* CONFIG_PCI */

/* Include architecture-dependent settings and functions */

//#include <asm/pci.h>

/*
 * pci_mmap_resource_range() maps a specific BAR, and vm->vm_pgoff
 * is expected to be an offset within that region.
 *
 */
int pci_mmap_resource_range(struct pci_dev *dev, int bar,
			    struct vm_area_struct *vma,
			    enum pci_mmap_state mmap_state, int write_combine);

#ifndef arch_can_pci_mmap_wc
#define arch_can_pci_mmap_wc()		0
#endif

#ifndef arch_can_pci_mmap_io
#define arch_can_pci_mmap_io()		0
#define pci_iobar_pfn(pdev, bar, vma) (-EINVAL)
#else
int pci_iobar_pfn(struct pci_dev *pdev, int bar, struct vm_area_struct *vma);
#endif

#ifndef pci_root_bus_fwnode
#define pci_root_bus_fwnode(bus)	NULL
#endif

#if defined(USE_LINUX_PCIBIOS) && USE_LINUX_PCIBIOS == 1
/*
 * These helpers provide future and backwards compatibility
 * for accessing popular PCI BAR info
 */
#define pci_resource_n(dev, bar)	(&(dev)->resource[(bar)])
#define pci_resource_start(dev, bar)	(pci_resource_n(dev, bar)->start)
#define pci_resource_end(dev, bar)	(pci_resource_n(dev, bar)->end)
#define pci_resource_flags(dev, bar)	(pci_resource_n(dev, bar)->flags)
#define pci_resource_len(dev,bar)					\
	(pci_resource_end((dev), (bar)) ? 				\
	 resource_size(pci_resource_n((dev), (bar))) : 0)
#endif

#define __pci_dev_for_each_res0(dev, res, ...)				\
	for (unsigned int __b = 0;					\
	     res = pci_resource_n(dev, __b), __b < PCI_NUM_RESOURCES;	\
	     __b++)

#define __pci_dev_for_each_res1(dev, res, __b)				\
	for (__b = 0;							\
	     res = pci_resource_n(dev, __b), __b < PCI_NUM_RESOURCES;	\
	     __b++)

#define pci_dev_for_each_resource(dev, res, ...)			\
	CONCATENATE(__pci_dev_for_each_res, COUNT_ARGS(__VA_ARGS__)) 	\
		    (dev, res, __VA_ARGS__)

/*
 * Similar to the helpers above, these manipulate per-pci_dev
 * driver-specific data.  They are really just a wrapper around
 * the generic device structure functions of these calls.
 */
static inline void *pci_get_drvdata(struct pci_dev *pdev)
{
	return dev_get_drvdata(&pdev->dev);
}

static inline void pci_set_drvdata(struct pci_dev *pdev, void *data)
{
	dev_set_drvdata(&pdev->dev, data);
}

static inline const char *pci_name(const struct pci_dev *pdev)
{
	return dev_name(&pdev->dev);
}

void pci_resource_to_user(const struct pci_dev *dev, int bar,
			  const struct resource *rsrc,
			  resource_size_t *start, resource_size_t *end);

/*
 * The world is not perfect and supplies us with broken PCI devices.
 * For at least a part of these bugs we need a work-around, so both
 * generic (drivers/pci/quirks.c) and per-architecture code can define
 * fixup hooks to be called for particular buggy devices.
 */

struct pci_fixup {
	u16 vendor;			/* Or PCI_ANY_ID */
	u16 device;			/* Or PCI_ANY_ID */
	u32 class;			/* Or PCI_ANY_ID */
	unsigned int class_shift;	/* should be 0, 8, 16 */
#ifdef CONFIG_HAVE_ARCH_PREL32_RELOCATIONS
	int hook_offset;
#else
	void (*hook)(struct pci_dev *dev);
#endif
};

enum pci_fixup_pass {
	pci_fixup_early,	/* Before probing BARs */
	pci_fixup_header,	/* After reading configuration header */
	pci_fixup_final,	/* Final phase of device fixups */
	pci_fixup_enable,	/* pci_enable_device() time */
	pci_fixup_resume,	/* pci_device_resume() */
	pci_fixup_suspend,	/* pci_device_suspend() */
	pci_fixup_resume_early, /* pci_device_resume_early() */
	pci_fixup_suspend_late,	/* pci_device_suspend_late() */
};

#ifdef CONFIG_HAVE_ARCH_PREL32_RELOCATIONS
#define ___DECLARE_PCI_FIXUP_SECTION(sec, name, vendor, device, class,	\
				    class_shift, hook)			\
	__ADDRESSABLE(hook)						\
	asm(".section "	#sec ", \"a\"				\n"	\
	    ".balign	16					\n"	\
	    ".short "	#vendor ", " #device "			\n"	\
	    ".long "	#class ", " #class_shift "		\n"	\
	    ".long "	#hook " - .				\n"	\
	    ".previous						\n");

/*
 * Clang's LTO may rename static functions in C, but has no way to
 * handle such renamings when referenced from inline asm. To work
 * around this, create global C stubs for these cases.
 */
#ifdef CONFIG_LTO_CLANG
#define __DECLARE_PCI_FIXUP_SECTION(sec, name, vendor, device, class,	\
				  class_shift, hook, stub)		\
	void stub(struct pci_dev *dev);					\
	void stub(struct pci_dev *dev)					\
	{ 								\
		hook(dev); 						\
	}								\
	___DECLARE_PCI_FIXUP_SECTION(sec, name, vendor, device, class,	\
				  class_shift, stub)
#else
#define __DECLARE_PCI_FIXUP_SECTION(sec, name, vendor, device, class,	\
				  class_shift, hook, stub)		\
	___DECLARE_PCI_FIXUP_SECTION(sec, name, vendor, device, class,	\
				  class_shift, hook)
#endif

#define DECLARE_PCI_FIXUP_SECTION(sec, name, vendor, device, class,	\
				  class_shift, hook)			\
	__DECLARE_PCI_FIXUP_SECTION(sec, name, vendor, device, class,	\
				  class_shift, hook, __UNIQUE_ID(hook))
#else
/* Anonymous variables would be nice... */
#define DECLARE_PCI_FIXUP_SECTION(section, name, vendor, device, class,	\
				  class_shift, hook)			\
	static const struct pci_fixup __PASTE(__pci_fixup_##name,__LINE__) __used	\
	__attribute__((__section__(#section), aligned((sizeof(void *)))))    \
		= { vendor, device, class, class_shift, hook };
#endif

#define DECLARE_PCI_FIXUP_CLASS_EARLY(vendor, device, class,		\
					 class_shift, hook)		\
	DECLARE_PCI_FIXUP_SECTION(.pci_fixup_early,			\
		hook, vendor, device, class, class_shift, hook)
#define DECLARE_PCI_FIXUP_CLASS_HEADER(vendor, device, class,		\
					 class_shift, hook)		\
	DECLARE_PCI_FIXUP_SECTION(.pci_fixup_header,			\
		hook, vendor, device, class, class_shift, hook)
#define DECLARE_PCI_FIXUP_CLASS_FINAL(vendor, device, class,		\
					 class_shift, hook)		\
	DECLARE_PCI_FIXUP_SECTION(.pci_fixup_final,			\
		hook, vendor, device, class, class_shift, hook)
#define DECLARE_PCI_FIXUP_CLASS_ENABLE(vendor, device, class,		\
					 class_shift, hook)		\
	DECLARE_PCI_FIXUP_SECTION(.pci_fixup_enable,			\
		hook, vendor, device, class, class_shift, hook)
#define DECLARE_PCI_FIXUP_CLASS_RESUME(vendor, device, class,		\
					 class_shift, hook)		\
	DECLARE_PCI_FIXUP_SECTION(.pci_fixup_resume,			\
		resume##hook, vendor, device, class, class_shift, hook)
#define DECLARE_PCI_FIXUP_CLASS_RESUME_EARLY(vendor, device, class,	\
					 class_shift, hook)		\
	DECLARE_PCI_FIXUP_SECTION(.pci_fixup_resume_early,		\
		resume_early##hook, vendor, device, class, class_shift, hook)
#define DECLARE_PCI_FIXUP_CLASS_SUSPEND(vendor, device, class,		\
					 class_shift, hook)		\
	DECLARE_PCI_FIXUP_SECTION(.pci_fixup_suspend,			\
		suspend##hook, vendor, device, class, class_shift, hook)
#define DECLARE_PCI_FIXUP_CLASS_SUSPEND_LATE(vendor, device, class,	\
					 class_shift, hook)		\
	DECLARE_PCI_FIXUP_SECTION(.pci_fixup_suspend_late,		\
		suspend_late##hook, vendor, device, class, class_shift, hook)

#define DECLARE_PCI_FIXUP_EARLY(vendor, device, hook)			\
	DECLARE_PCI_FIXUP_SECTION(.pci_fixup_early,			\
		hook, vendor, device, PCI_ANY_ID, 0, hook)
#define DECLARE_PCI_FIXUP_HEADER(vendor, device, hook)			\
	DECLARE_PCI_FIXUP_SECTION(.pci_fixup_header,			\
		hook, vendor, device, PCI_ANY_ID, 0, hook)
#define DECLARE_PCI_FIXUP_FINAL(vendor, device, hook)			\
	DECLARE_PCI_FIXUP_SECTION(.pci_fixup_final,			\
		hook, vendor, device, PCI_ANY_ID, 0, hook)
#define DECLARE_PCI_FIXUP_ENABLE(vendor, device, hook)			\
	DECLARE_PCI_FIXUP_SECTION(.pci_fixup_enable,			\
		hook, vendor, device, PCI_ANY_ID, 0, hook)
#define DECLARE_PCI_FIXUP_RESUME(vendor, device, hook)			\
	DECLARE_PCI_FIXUP_SECTION(.pci_fixup_resume,			\
		resume##hook, vendor, device, PCI_ANY_ID, 0, hook)
#define DECLARE_PCI_FIXUP_RESUME_EARLY(vendor, device, hook)		\
	DECLARE_PCI_FIXUP_SECTION(.pci_fixup_resume_early,		\
		resume_early##hook, vendor, device, PCI_ANY_ID, 0, hook)
#define DECLARE_PCI_FIXUP_SUSPEND(vendor, device, hook)			\
	DECLARE_PCI_FIXUP_SECTION(.pci_fixup_suspend,			\
		suspend##hook, vendor, device, PCI_ANY_ID, 0, hook)
#define DECLARE_PCI_FIXUP_SUSPEND_LATE(vendor, device, hook)		\
	DECLARE_PCI_FIXUP_SECTION(.pci_fixup_suspend_late,		\
		suspend_late##hook, vendor, device, PCI_ANY_ID, 0, hook)

#ifdef CONFIG_PCI_QUIRKS
void pci_fixup_device(enum pci_fixup_pass pass, struct pci_dev *dev);
#else
static inline void pci_fixup_device(enum pci_fixup_pass pass,
				    struct pci_dev *dev) { }
#endif

void __iomem *pcim_iomap(struct pci_dev *pdev, int bar, unsigned long maxlen);
void pcim_iounmap(struct pci_dev *pdev, void __iomem *addr);
void __iomem * const *pcim_iomap_table(struct pci_dev *pdev);
int pcim_iomap_regions(struct pci_dev *pdev, int mask, const char *name);
int pcim_iomap_regions_request_all(struct pci_dev *pdev, int mask,
				   const char *name);
void pcim_iounmap_regions(struct pci_dev *pdev, int mask);

extern int pci_pci_problems;
#define PCIPCI_FAIL		1	/* No PCI PCI DMA */
#define PCIPCI_TRITON		2
#define PCIPCI_NATOMA		4
#define PCIPCI_VIAETBF		8
#define PCIPCI_VSFX		16
#define PCIPCI_ALIMAGIK		32	/* Need low latency setting */
#define PCIAGP_FAIL		64	/* No PCI to AGP DMA */

extern unsigned long pci_cardbus_io_size;
extern unsigned long pci_cardbus_mem_size;
extern u8 pci_dfl_cache_line_size;
extern u8 pci_cache_line_size;

/* Architecture-specific versions may override these (weak) */
void pcibios_disable_device(struct pci_dev *dev);
void linux_pcibios_set_master(struct pci_dev *dev);
int pcibios_set_pcie_reset_state(struct pci_dev *dev,
				 enum pcie_reset_state state);
int pcibios_device_add(struct pci_dev *dev);
void pcibios_release_device(struct pci_dev *dev);
#ifdef CONFIG_PCI
void pcibios_penalize_isa_irq(int irq, int active);
#else
static inline void pcibios_penalize_isa_irq(int irq, int active) {}
#endif
int pcibios_alloc_irq(struct pci_dev *dev);
void pcibios_free_irq(struct pci_dev *dev);
resource_size_t pcibios_default_alignment(void);

#if !defined(HAVE_PCI_MMAP) && !defined(ARCH_GENERIC_PCI_MMAP_RESOURCE)
extern int pci_create_resource_files(struct pci_dev *dev);
extern void pci_remove_resource_files(struct pci_dev *dev);
#endif

#if defined(CONFIG_PCI_MMCONFIG) || defined(CONFIG_ACPI_MCFG)
void __init pci_mmcfg_early_init(void);
void __init pci_mmcfg_late_init(void);
#else
static inline void pci_mmcfg_early_init(void) { }
static inline void pci_mmcfg_late_init(void) { }
#endif

int pci_ext_cfg_avail(void);

void __iomem *pci_ioremap_bar(struct pci_dev *pdev, int bar);
void __iomem *pci_ioremap_wc_bar(struct pci_dev *pdev, int bar);

#ifdef CONFIG_PCI_IOV
int pci_iov_virtfn_bus(struct pci_dev *dev, int id);
int pci_iov_virtfn_devfn(struct pci_dev *dev, int id);
int pci_iov_vf_id(struct pci_dev *dev);
void *pci_iov_get_pf_drvdata(struct pci_dev *dev, struct pci_driver *pf_driver);
int pci_enable_sriov(struct pci_dev *dev, int nr_virtfn);
void pci_disable_sriov(struct pci_dev *dev);

int pci_iov_sysfs_link(struct pci_dev *dev, struct pci_dev *virtfn, int id);
int pci_iov_add_virtfn(struct pci_dev *dev, int id);
void pci_iov_remove_virtfn(struct pci_dev *dev, int id);
int pci_num_vf(struct pci_dev *dev);
int pci_vfs_assigned(struct pci_dev *dev);
int pci_sriov_set_totalvfs(struct pci_dev *dev, u16 numvfs);
int pci_sriov_get_totalvfs(struct pci_dev *dev);
int pci_sriov_configure_simple(struct pci_dev *dev, int nr_virtfn);
resource_size_t pci_iov_resource_size(struct pci_dev *dev, int resno);
void pci_vf_drivers_autoprobe(struct pci_dev *dev, bool probe);

/* Arch may override these (weak) */
int pcibios_sriov_enable(struct pci_dev *pdev, u16 num_vfs);
int pcibios_sriov_disable(struct pci_dev *pdev);
resource_size_t pcibios_iov_resource_alignment(struct pci_dev *dev, int resno);
#else
static inline int pci_iov_virtfn_bus(struct pci_dev *dev, int id)
{
	return -ENOSYS;
}
static inline int pci_iov_virtfn_devfn(struct pci_dev *dev, int id)
{
	return -ENOSYS;
}

static inline int pci_iov_vf_id(struct pci_dev *dev)
{
	return -ENOSYS;
}

static inline void *pci_iov_get_pf_drvdata(struct pci_dev *dev,
					   struct pci_driver *pf_driver)
{
	return ERR_PTR(-EINVAL);
}

static inline int pci_enable_sriov(struct pci_dev *dev, int nr_virtfn)
{ return -ENODEV; }

static inline int pci_iov_sysfs_link(struct pci_dev *dev,
				     struct pci_dev *virtfn, int id)
{
	return -ENODEV;
}
static inline int pci_iov_add_virtfn(struct pci_dev *dev, int id)
{
	return -ENOSYS;
}
static inline void pci_iov_remove_virtfn(struct pci_dev *dev,
					 int id) { }
static inline void pci_disable_sriov(struct pci_dev *dev) { }
static inline int pci_num_vf(struct pci_dev *dev) { return 0; }
static inline int pci_vfs_assigned(struct pci_dev *dev)
{ return 0; }
static inline int pci_sriov_set_totalvfs(struct pci_dev *dev, u16 numvfs)
{ return 0; }
static inline int pci_sriov_get_totalvfs(struct pci_dev *dev)
{ return 0; }
#define pci_sriov_configure_simple	NULL
static inline resource_size_t pci_iov_resource_size(struct pci_dev *dev, int resno)
{ return 0; }
static inline void pci_vf_drivers_autoprobe(struct pci_dev *dev, bool probe) { }
#endif

#if defined(CONFIG_HOTPLUG_PCI) || defined(CONFIG_HOTPLUG_PCI_MODULE)
void pci_hp_create_module_link(struct pci_slot *pci_slot);
void pci_hp_remove_module_link(struct pci_slot *pci_slot);
#endif

/**
 * pci_pcie_cap - get the saved PCIe capability offset
 * @dev: PCI device
 *
 * PCIe capability offset is calculated at PCI device initialization
 * time and saved in the data structure. This function returns saved
 * PCIe capability offset. Using this instead of pci_find_capability()
 * reduces unnecessary search in the PCI configuration space. If you
 * need to calculate PCIe capability offset from raw device for some
 * reasons, please use pci_find_capability() instead.
 */
static inline int pci_pcie_cap(struct pci_dev *dev)
{
	return dev->pcie_cap;
}

/**
 * pci_is_pcie - check if the PCI device is PCI Express capable
 * @dev: PCI device
 *
 * Returns: true if the PCI device is PCI Express capable, false otherwise.
 */
static inline bool pci_is_pcie(struct pci_dev *dev)
{
	return pci_pcie_cap(dev);
}

/**
 * pcie_caps_reg - get the PCIe Capabilities Register
 * @dev: PCI device
 */
static inline u16 pcie_caps_reg(const struct pci_dev *dev)
{
	return dev->pcie_flags_reg;
}

/**
 * pci_pcie_type - get the PCIe device/port type
 * @dev: PCI device
 */
static inline int pci_pcie_type(const struct pci_dev *dev)
{
	return (pcie_caps_reg(dev) & PCI_EXP_FLAGS_TYPE) >> 4;
}

/**
 * pcie_find_root_port - Get the PCIe root port device
 * @dev: PCI device
 *
 * Traverse up the parent chain and return the PCIe Root Port PCI Device
 * for a given PCI/PCIe Device.
 */
static inline struct pci_dev *pcie_find_root_port(struct pci_dev *dev)
{
	while (dev) {
		if (pci_is_pcie(dev) &&
		    pci_pcie_type(dev) == PCI_EXP_TYPE_ROOT_PORT)
			return dev;
		dev = pci_upstream_bridge(dev);
	}

	return NULL;
}

void pci_request_acs(void);
bool pci_acs_enabled(struct pci_dev *pdev, u16 acs_flags);
bool pci_acs_path_enabled(struct pci_dev *start,
			  struct pci_dev *end, u16 acs_flags);
int pci_enable_atomic_ops_to_root(struct pci_dev *dev, u32 cap_mask);

#define PCI_VPD_LRDT			0x80	/* Large Resource Data Type */
#define PCI_VPD_LRDT_ID(x)		((x) | PCI_VPD_LRDT)

/* Large Resource Data Type Tag Item Names */
#define PCI_VPD_LTIN_ID_STRING		0x02	/* Identifier String */
#define PCI_VPD_LTIN_RO_DATA		0x10	/* Read-Only Data */
#define PCI_VPD_LTIN_RW_DATA		0x11	/* Read-Write Data */

#define PCI_VPD_LRDT_ID_STRING		PCI_VPD_LRDT_ID(PCI_VPD_LTIN_ID_STRING)
#define PCI_VPD_LRDT_RO_DATA		PCI_VPD_LRDT_ID(PCI_VPD_LTIN_RO_DATA)
#define PCI_VPD_LRDT_RW_DATA		PCI_VPD_LRDT_ID(PCI_VPD_LTIN_RW_DATA)

#define PCI_VPD_RO_KEYWORD_PARTNO	"PN"
#define PCI_VPD_RO_KEYWORD_SERIALNO	"SN"
#define PCI_VPD_RO_KEYWORD_MFR_ID	"MN"
#define PCI_VPD_RO_KEYWORD_VENDOR0	"V0"
#define PCI_VPD_RO_KEYWORD_CHKSUM	"RV"

/**
 * pci_vpd_alloc - Allocate buffer and read VPD into it
 * @dev: PCI device
 * @size: pointer to field where VPD length is returned
 *
 * Returns pointer to allocated buffer or an ERR_PTR in case of failure
 */
void *pci_vpd_alloc(struct pci_dev *dev, unsigned int *size);

/**
 * pci_vpd_find_id_string - Locate id string in VPD
 * @buf: Pointer to buffered VPD data
 * @len: The length of the buffer area in which to search
 * @size: Pointer to field where length of id string is returned
 *
 * Returns the index of the id string or -ENOENT if not found.
 */
int pci_vpd_find_id_string(const u8 *buf, unsigned int len, unsigned int *size);

/**
 * pci_vpd_find_ro_info_keyword - Locate info field keyword in VPD RO section
 * @buf: Pointer to buffered VPD data
 * @len: The length of the buffer area in which to search
 * @kw: The keyword to search for
 * @size: Pointer to field where length of found keyword data is returned
 *
 * Returns the index of the information field keyword data or -ENOENT if
 * not found.
 */
int pci_vpd_find_ro_info_keyword(const void *buf, unsigned int len,
				 const char *kw, unsigned int *size);

/**
 * pci_vpd_check_csum - Check VPD checksum
 * @buf: Pointer to buffered VPD data
 * @len: VPD size
 *
 * Returns 1 if VPD has no checksum, otherwise 0 or an errno
 */
int pci_vpd_check_csum(const void *buf, unsigned int len);

/* PCI <-> OF binding helpers */
#ifdef CONFIG_OF
struct device_node;
struct irq_domain;
struct irq_domain *pci_host_bridge_of_msi_domain(struct pci_bus *bus);
bool pci_host_of_has_msi_map(struct device *dev);

/* Arch may override this (weak) */
struct device_node *pcibios_get_phb_of_node(struct pci_bus *bus);

#else	/* CONFIG_OF */
static inline struct irq_domain *
pci_host_bridge_of_msi_domain(struct pci_bus *bus) { return NULL; }
static inline bool pci_host_of_has_msi_map(struct device *dev) { return false; }
#endif  /* CONFIG_OF */

static inline struct device_node *
pci_device_to_OF_node(const struct pci_dev *pdev)
{
	return pdev ? pdev->dev.of_node : NULL;
}

static inline struct device_node *pci_bus_to_OF_node(struct pci_bus *bus)
{
	return bus ? bus->dev.of_node : NULL;
}

#ifdef CONFIG_ACPI
struct irq_domain *pci_host_bridge_acpi_msi_domain(struct pci_bus *bus);

void
pci_msi_register_fwnode_provider(struct fwnode_handle *(*fn)(struct device *));
bool pci_pr3_present(struct pci_dev *pdev);
#else
static inline struct irq_domain *
pci_host_bridge_acpi_msi_domain(struct pci_bus *bus) { return NULL; }
static inline bool pci_pr3_present(struct pci_dev *pdev) { return false; }
#endif

#ifdef CONFIG_EEH
static inline struct eeh_dev *pci_dev_to_eeh_dev(struct pci_dev *pdev)
{
	return pdev->dev.archdata.edev;
}
#endif

void pci_add_dma_alias(struct pci_dev *dev, u8 devfn_from, unsigned nr_devfns);
bool pci_devs_are_dma_aliases(struct pci_dev *dev1, struct pci_dev *dev2);
int pci_for_each_dma_alias(struct pci_dev *pdev,
			   int (*fn)(struct pci_dev *pdev,
				     u16 alias, void *data), void *data);

/* Helper functions for operation of device flag */
static inline void pci_set_dev_assigned(struct pci_dev *pdev)
{
	pdev->dev_flags |= PCI_DEV_FLAGS_ASSIGNED;
}
static inline void pci_clear_dev_assigned(struct pci_dev *pdev)
{
	pdev->dev_flags &= ~PCI_DEV_FLAGS_ASSIGNED;
}
static inline bool pci_is_dev_assigned(struct pci_dev *pdev)
{
	return (pdev->dev_flags & PCI_DEV_FLAGS_ASSIGNED) == PCI_DEV_FLAGS_ASSIGNED;
}

/**
 * pci_ari_enabled - query ARI forwarding status
 * @bus: the PCI bus
 *
 * Returns true if ARI forwarding is enabled.
 */
static inline bool pci_ari_enabled(struct pci_bus *bus)
{
	return bus->self && bus->self->ari_enabled;
}

/**
 * pci_is_thunderbolt_attached - whether device is on a Thunderbolt daisy chain
 * @pdev: PCI device to check
 *
 * Walk upwards from @pdev and check for each encountered bridge if it's part
 * of a Thunderbolt controller.  Reaching the host bridge means @pdev is not
 * Thunderbolt-attached.  (But rather soldered to the mainboard usually.)
 */
static inline bool pci_is_thunderbolt_attached(struct pci_dev *pdev)
{
	struct pci_dev *parent = pdev;

	if (pdev->is_thunderbolt)
		return true;

	while ((parent = pci_upstream_bridge(parent)))
		if (parent->is_thunderbolt)
			return true;

	return false;
}

#if defined(CONFIG_PCIEPORTBUS) || defined(CONFIG_EEH)
void pci_uevent_ers(struct pci_dev *pdev, enum  pci_ers_result err_type);
#endif

//#include <linux/dma-mapping.h>

#define pci_printk(level, pdev, fmt, arg...) \
	dev_printk(level, &(pdev)->dev, fmt, ##arg)

#define pci_emerg(pdev, fmt, arg...)	dev_emerg(&(pdev)->dev, fmt, ##arg)
#define pci_alert(pdev, fmt, arg...)	dev_alert(&(pdev)->dev, fmt, ##arg)
#define pci_crit(pdev, fmt, arg...)	dev_crit(&(pdev)->dev, fmt, ##arg)
#define pci_err(pdev, fmt, arg...)	dev_err(&(pdev)->dev, fmt, ##arg)
#define pci_warn(pdev, fmt, arg...)	dev_warn(&(pdev)->dev, fmt, ##arg)
#define pci_warn_once(pdev, fmt, arg...) dev_warn_once(&(pdev)->dev, fmt, ##arg)
#define pci_notice(pdev, fmt, arg...)	dev_notice(&(pdev)->dev, fmt, ##arg)
#define pci_info(pdev, fmt, arg...)	dev_info(&(pdev)->dev, fmt, ##arg)
#define pci_dbg(pdev, fmt, arg...)	dev_dbg(&(pdev)->dev, fmt, ##arg)

#define pci_notice_ratelimited(pdev, fmt, arg...) \
	dev_notice_ratelimited(&(pdev)->dev, fmt, ##arg)

#define pci_info_ratelimited(pdev, fmt, arg...) \
	dev_info_ratelimited(&(pdev)->dev, fmt, ##arg)

#define pci_WARN(pdev, condition, fmt, arg...) \
	WARN(condition, "%s %s: " fmt, \
	     dev_driver_string(&(pdev)->dev), pci_name(pdev), ##arg)

#define pci_WARN_ONCE(pdev, condition, fmt, arg...) \
	WARN_ONCE(condition, "%s %s: " fmt, \
		  dev_driver_string(&(pdev)->dev), pci_name(pdev), ##arg)

#endif /* LINUX_PCI_H */

#define pci_dev_lock(x) 1
#define pci_dev_trylock(x) 1
#define pci_dev_unlock(x)

#if 0
/***
typedef struct pci_config_s{
 uint8_t bBus;
 uint8_t bDev;
 uint8_t bFunc;
 uint16_t vendor_id;
 uint16_t device_id;
 char *device_name;     // from pci_device_s
 uint16_t device_type;  // from pci_device_s
}pci_config_s;
***/

static inline int pci_bus_read_config_dword (struct pci_bus *bus, unsigned int devfn, int addr, u32 *wordp) {
  pci_config_s pcicfg;
  pcicfg.bBus = bus->number;
  pcicfg.bDev = PCI_SLOT(devfn);
  pcicfg.bFunc = PCI_FUNC(devfn);
  *wordp = pcibios_ReadConfig_Dword(&pcicfg, addr);
  return PCIBIOS_SUCCESSFUL;
}

static inline int pci_bus_write_config_dword (struct pci_bus *bus, unsigned int devfn, int addr, unsigned int word) {
  pci_config_s pcicfg;
  pcicfg.bBus = bus->number;
  pcicfg.bDev = PCI_SLOT(devfn);
  pcicfg.bFunc = PCI_FUNC(devfn);
  pcibios_WriteConfig_Dword(&pcicfg, addr, word);
  return PCIBIOS_SUCCESSFUL;
}


static inline int pci_bus_read_config_word (struct pci_bus *bus, unsigned int devfn, int addr, u32 *wordp) {
  pci_config_s pcicfg;
  pcicfg.bBus = bus->number;
  pcicfg.bDev = PCI_SLOT(devfn);
  pcicfg.bFunc = PCI_FUNC(devfn);
  *wordp = pcibios_ReadConfig_Word(&pcicfg, addr);
  return PCIBIOS_SUCCESSFUL;
}

static inline int pci_bus_write_config_word (struct pci_bus *bus, unsigned int devfn, int addr, unsigned int word) {
  pci_config_s pcicfg;
  pcicfg.bBus = bus->number;
  pcicfg.bDev = PCI_SLOT(devfn);
  pcicfg.bFunc = PCI_FUNC(devfn);
  pcibios_WriteConfig_Word(&pcicfg, addr, word);
  return PCIBIOS_SUCCESSFUL;
}

static inline int pci_bus_read_config_byte (struct pci_bus *bus, unsigned int devfn, int addr, u32 *bytep) {
  pci_config_s pcicfg;
  pcicfg.bBus = bus->number;
  pcicfg.bDev = PCI_SLOT(devfn);
  pcicfg.bFunc = PCI_FUNC(devfn);
  *bytep = pcibios_ReadConfig_Byte(&pcicfg, addr);
  return PCIBIOS_SUCCESSFUL;
}

static inline int pci_bus_write_config_byte (struct pci_bus *bus, unsigned int devfn, int addr, unsigned int byte) {
  pci_config_s pcicfg;
  pcicfg.bBus = bus->number;
  pcicfg.bDev = PCI_SLOT(devfn);
  pcicfg.bFunc = PCI_FUNC(devfn);
  pcibios_WriteConfig_Byte(&pcicfg, addr, byte);
  return PCIBIOS_SUCCESSFUL;
}
#endif

// in access.c PCI_OP_READ/PCI_OP_WRITE
int pci_bus_read_config_byte(struct pci_bus *bus, unsigned int devfn,
			     int where, u8 *val);
int pci_bus_read_config_word(struct pci_bus *bus, unsigned int devfn,
			     int where, u16 *val);
int pci_bus_read_config_dword(struct pci_bus *bus, unsigned int devfn,
			      int where, u32 *val);
int pci_bus_write_config_byte(struct pci_bus *bus, unsigned int devfn,
			      int where, u8 val);
int pci_bus_write_config_word(struct pci_bus *bus, unsigned int devfn,
			      int where, u16 val);
int pci_bus_write_config_dword(struct pci_bus *bus, unsigned int devfn,
			       int where, u32 val);

bool pci_enable_writes ();
void pci_disable_writes ();

#endif
