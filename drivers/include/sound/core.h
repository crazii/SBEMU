#ifndef SBEMU_SOUND_CORE_H
#define SBEMU_SOUND_CORE_H

#include "linux/spinlock.h"
#include "linux/time.h"
#include "linux/rwsem.h"
#include "linux/mutex.h"
#include "linux/pci.h"

#include "sound/asoundef.h"

static inline int snd_BUG_ON (int _cond) { return _cond; }
#define snd_BUG() do { printk("snd_BUG at %s:%d\n", __FILE__, __LINE__); } while (0)

/* PCI quirk list helper */
struct snd_pci_quirk {
	unsigned short subvendor;	/* PCI subvendor ID */
	unsigned short subdevice;	/* PCI subdevice ID */
	unsigned short subdevice_mask;	/* bitmask to match */
	int value;			/* value */
#ifdef CONFIG_SND_DEBUG_VERBOSE
	const char *name;		/* name of the device (optional) */
#endif
};

#define _SND_PCI_QUIRK_ID_MASK(vend, mask, dev)	\
	.subvendor = (vend), .subdevice = (dev), .subdevice_mask = (mask)
#define _SND_PCI_QUIRK_ID(vend, dev) \
	_SND_PCI_QUIRK_ID_MASK(vend, 0xffff, dev)
#define SND_PCI_QUIRK_ID(vend,dev) {_SND_PCI_QUIRK_ID(vend, dev)}
#ifdef CONFIG_SND_DEBUG_VERBOSE
#define SND_PCI_QUIRK(vend,dev,xname,val) \
	{_SND_PCI_QUIRK_ID(vend, dev), .value = (val), .name = (xname)}
#define SND_PCI_QUIRK_VENDOR(vend, xname, val)			\
	{_SND_PCI_QUIRK_ID_MASK(vend, 0, 0), .value = (val), .name = (xname)}
#define SND_PCI_QUIRK_MASK(vend, mask, dev, xname, val)			\
	{_SND_PCI_QUIRK_ID_MASK(vend, mask, dev),			\
			.value = (val), .name = (xname)}
#define snd_pci_quirk_name(q)	((q)->name)
#else
#define SND_PCI_QUIRK(vend,dev,xname,val) \
	{_SND_PCI_QUIRK_ID(vend, dev), .value = (val)}
#define SND_PCI_QUIRK_MASK(vend, mask, dev, xname, val)			\
	{_SND_PCI_QUIRK_ID_MASK(vend, mask, dev), .value = (val)}
#define SND_PCI_QUIRK_VENDOR(vend, xname, val)			\
	{_SND_PCI_QUIRK_ID_MASK(vend, 0, 0), .value = (val)}
#define snd_pci_quirk_name(q)	""
#endif

/**
 * snd_pci_quirk_lookup_id - look up a PCI SSID quirk list
 * @vendor: PCI SSV id
 * @device: PCI SSD id
 * @list: quirk list, terminated by a null entry
 *
 * Look through the given quirk list and finds a matching entry
 * with the same PCI SSID.  When subdevice is 0, all subdevice
 * values may match.
 *
 * Returns the matched entry pointer, or NULL if nothing matched.
 */
static inline const struct snd_pci_quirk *
snd_pci_quirk_lookup_id (u16 vendor, u16 device, const struct snd_pci_quirk *list)
{
	const struct snd_pci_quirk *q;

	for (q = list; q->subvendor; q++) {
		if (q->subvendor != vendor)
			continue;
		if (!q->subdevice ||
		    (device & q->subdevice_mask) == q->subdevice)
			return q;
	}
	return NULL;
}

/**
 * snd_pci_quirk_lookup - look up a PCI SSID quirk list
 * @pci: pci_dev handle
 * @list: quirk list, terminated by a null entry
 *
 * Look through the given quirk list and finds a matching entry
 * with the same PCI SSID.  When subdevice is 0, all subdevice
 * values may match.
 *
 * Returns the matched entry pointer, or NULL if nothing matched.
 */
static inline const struct snd_pci_quirk *
snd_pci_quirk_lookup(struct pci_dev *pci, const struct snd_pci_quirk *list)
{
        if (!pci)
                return NULL;
        return snd_pci_quirk_lookup_id(pci->subsystem_vendor,
                                       pci->subsystem_device,
                                       list);
}

/* forward declarations */
struct pci_dev;
struct module;
struct completion;

/* device allocation stuff */

/* type of the object used in snd_device_*()
 * this also defines the calling order
 */
enum snd_device_type {
	SNDRV_DEV_LOWLEVEL,
	SNDRV_DEV_CONTROL,
	SNDRV_DEV_INFO,
	SNDRV_DEV_BUS,
	SNDRV_DEV_CODEC,
	SNDRV_DEV_PCM,
	SNDRV_DEV_COMPRESS,
	SNDRV_DEV_RAWMIDI,
	SNDRV_DEV_TIMER,
	SNDRV_DEV_SEQUENCER,
	SNDRV_DEV_HWDEP,
	SNDRV_DEV_JACK,
};

enum snd_device_state {
	SNDRV_DEV_BUILD,
	SNDRV_DEV_REGISTERED,
	SNDRV_DEV_DISCONNECTED,
};

struct snd_device;

struct snd_device_ops {
	int (*dev_free)(struct snd_device *dev);
	int (*dev_register)(struct snd_device *dev);
	int (*dev_disconnect)(struct snd_device *dev);
};

struct snd_device {
	struct list_head list;		/* list of registered devices */
	struct snd_card *card;		/* card which holds this device */
	enum snd_device_state state;	/* state of the device */
	enum snd_device_type type;	/* device type */
	void *device_data;		/* device structure */
	struct snd_device_ops *ops;	/* operations */
};

#define snd_device(n) list_entry(n, struct snd_device, list)

/* main structure for soundcard */

struct snd_card {
	int number;			/* number of soundcard (index to
								snd_cards) */

	char id[16];			/* id string of this card */
	char driver[16];		/* driver name */
	char shortname[32];		/* short name of this soundcard */
	char longname[80];		/* name of this soundcard */
	char irq_descr[32];		/* Interrupt description */
	char mixername[80];		/* mixer name */
	char components[128];		/* card components delimited with
								space */
	struct module *module;		/* top-level module */

	void *private_data;		/* private data for soundcard */
	void (*private_free) (struct snd_card *card); /* callback for freeing of
								private data */
	struct list_head devices;	/* devices */

	struct device ctl_dev;		/* control device */
	unsigned int last_numid;	/* last used numeric ID */
	struct rw_semaphore controls_rwsem;	/* controls list lock */
	rwlock_t ctl_files_rwlock;	/* ctl_files list lock */
	int controls_count;		/* count of all controls */
	int user_ctl_count;		/* count of all user controls */
	struct list_head controls;	/* all controls for this card */
	struct list_head ctl_files;	/* active control files */
	struct mutex user_ctl_lock;	/* protects user controls against
					   concurrent access */

	struct snd_info_entry *proc_root;	/* root for soundcard specific files */
	struct snd_info_entry *proc_id;	/* the card id */
	struct proc_dir_entry *proc_root_link;	/* number link to real id */

	struct list_head files_list;	/* all files associated to this card */
	struct snd_shutdown_f_ops *s_f_ops; /* file operations in the shutdown
								state */
	spinlock_t files_lock;		/* lock the files for this card */
	int shutdown;			/* this card is going down */
	struct completion *release_completion;
	struct device *dev;		/* device assigned to this card */
	struct device card_dev;		/* cardX object for sysfs */
	const struct attribute_group *dev_groups[4]; /* assigned sysfs attr */
	bool registered;		/* card_dev is registered? */

#ifdef CONFIG_PM
	unsigned int power_state;	/* power state */
	struct mutex power_lock;	/* power lock */
	wait_queue_head_t power_sleep;
#endif

#if defined(CONFIG_SND_MIXER_OSS) || defined(CONFIG_SND_MIXER_OSS_MODULE)
	struct snd_mixer_oss *mixer_oss;
	int mixer_oss_change_count;
#endif
    u16 sync_irq;
};

#define dev_to_snd_card(p)	container_of(p, struct snd_card, card_dev)

#define snd_i2c_lock(x)
#define snd_i2c_unlock(x)

#endif
