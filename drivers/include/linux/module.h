#ifndef SBEMU_LINUX_MODULE_H
#define SBEMU_LINUX_MODULE_H

#define MODULE_AUTHOR(x)
#define MODULE_LICENSE(x)
#define MODULE_DESCRIPTION(x)

#define module_param_array(...)
#define MODULE_PARM_DESC(...)

#define KBUILD_MODNAME __FILE__

#define MODULE_FIRMWARE(x) 0error = "MODULE_FIRMWARE is not supported"
#define MODULE_DEVICE_TABLE(x, y)

#endif

