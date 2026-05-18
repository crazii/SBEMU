#ifndef SBEMU_LINUX_RWSEM_H
#define SBEMU_LINUX_RWSEM_H

#include "linux/atomic-long.h"
#include "linux/spinlock.h"

typedef unsigned long rwlock_t;

struct rw_semaphore;

/* All arch specific implementations share the same struct */
struct rw_semaphore {
	atomic_long_t count;
	struct list_head wait_list;
  /*raw_*/spinlock_t wait_lock;
#if 0
#ifdef CONFIG_RWSEM_SPIN_ON_OWNER
	struct optimistic_spin_queue osq; /* spinner MCS lock */
	/*
	 * Write owner. Used as a speculative check to see
	 * if the owner is running on the cpu.
	 */
	struct task_struct *owner;
#endif
#ifdef CONFIG_DEBUG_LOCK_ALLOC
	struct lockdep_map	dep_map;
#endif
#endif
};

#define DECLARE_RWSEM(x) struct rw_semaphore x

#if 1
static inline void down_write(struct rw_semaphore *sem) {
}
/*
 * lock for reading
 */
static inline void down_read(struct rw_semaphore *sem) {
}
static inline int down_write_trylock(struct rw_semaphore *sem) {
  return 1;
}
static inline void up_read(struct rw_semaphore *sem) {
}
static inline void up_write(struct rw_semaphore *sem) {
}
#else
extern void down_write(struct rw_semaphore *sem);
//extern int __must_check down_write_killable(struct rw_semaphore *sem);

/*
 * lock for writing
 */
extern void down_write(struct rw_semaphore *sem);
//extern int __must_check down_write_killable(struct rw_semaphore *sem);

/*
 * lock for reading
 */
extern void down_read(struct rw_semaphore *sem);

/*
 * trylock for writing -- returns 1 if successful, 0 if contention
 */
extern int down_write_trylock(struct rw_semaphore *sem);

/*
 * release a read lock
 */
extern void up_read(struct rw_semaphore *sem);

/*
 * release a write lock
 */
extern void up_write(struct rw_semaphore *sem);
#endif

#endif /* _LINUX_RWSEM_H */
