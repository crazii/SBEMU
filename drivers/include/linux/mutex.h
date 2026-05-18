#ifndef SBEMU_LINUX_MUTEX_H
#define SBEMU_LINUX_MUTEX_H

struct mutex {
};

#define mutex_init(x)
#define mutex_lock(x)
#define mutex_unlock(x)
#define mutex_destroy(x)

#define DEFINE_MUTEX(x) struct mutex x

#endif
