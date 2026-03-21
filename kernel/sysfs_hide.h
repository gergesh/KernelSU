#ifndef __KSU_SYSFS_HIDE_H
#define __KSU_SYSFS_HIDE_H

#include <linux/types.h>

/* Called at sys_enter for getdents64 on sysfs directories */
void ksu_sysfs_hide_getdents64_enter(unsigned int fd, void __user *dirent,
				     unsigned int count);

/* Called at sys_exit for getdents64 — filters emulator entries from results */
long ksu_sysfs_hide_getdents64_exit(long ret);

/* Check if a path should be hidden (emulator sysfs path) */
bool ksu_sysfs_hide_should_block_path(const char __user *pathname);

#endif /* __KSU_SYSFS_HIDE_H */
