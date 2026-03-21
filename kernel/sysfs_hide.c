/*
 * sysfs_hide.c - Hide emulator sysfs entries from userspace
 *
 * Hooks getdents64 syscall via sys_exit tracepoint to filter out
 * goldfish/virtio/ranchu/qemu entries from sysfs directory listings.
 *
 * Also hooks openat/newfstatat/faccessat at sys_enter to block direct
 * access to emulator sysfs paths.
 *
 * This prevents DroidGuard from detecting the emulator via /sys/ reads.
 */

#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/version.h>
#include <linux/sched.h>
#include <linux/file.h>
#include <asm/syscall.h>

#include "klog.h"
#include "sysfs_hide.h"

/* Patterns that indicate emulator hardware in sysfs entry names */
static inline bool name_matches_emulator(const char *name, unsigned int len)
{
	/* Short names can't match our patterns */
	if (len < 4)
		return false;

	/* Check for substrings: goldfish, virtio, ranchu, qemu */
	{
		unsigned int i;
		for (i = 0; i + 7 <= len; i++) {
			if (memcmp(name + i, "goldfish", 8) == 0 && i + 8 <= len)
				return true;
			if (memcmp(name + i, "virtio", 6) == 0 && i + 6 <= len)
				return true;
			if (memcmp(name + i, "ranchu", 6) == 0 && i + 6 <= len)
				return true;
		}
		for (i = 0; i + 3 <= len; i++) {
			if (memcmp(name + i, "qemu", 4) == 0 && i + 4 <= len)
				return true;
		}
	}
	return false;
}

/* linux_dirent64 layout for getdents64 buffer parsing */
struct ksu_dirent64 {
	u64		d_ino;
	s64		d_off;
	unsigned short	d_reclen;
	unsigned char	d_type;
	char		d_name[];
};

/*
 * Check if fd points to a sysfs directory.
 */
static bool is_sysfs_fd(unsigned int fd)
{
	struct file *f;
	bool result = false;

	f = fget(fd);
	if (!f)
		return false;

	if (f->f_path.mnt->mnt_sb->s_type &&
	    f->f_path.mnt->mnt_sb->s_type->name &&
	    strcmp(f->f_path.mnt->mnt_sb->s_type->name, "sysfs") == 0)
		result = true;

	fput(f);
	return result;
}

/*
 * Per-task storage for getdents64 args between sys_enter and sys_exit.
 * We use a simple percpu approach since tracepoints run with preemption
 * disabled on the same CPU.
 */
struct getdents64_args {
	pid_t		pid;		/* task pid to verify */
	unsigned int	fd;
	void __user	*dirent;
	unsigned int	count;
	bool		active;		/* set at enter, cleared at exit */
};

static DEFINE_PER_CPU(struct getdents64_args, gd64_args);

/*
 * Called at sys_enter for getdents64.
 * Saves args for the sys_exit handler to use.
 */
void ksu_sysfs_hide_getdents64_enter(unsigned int fd, void __user *dirent,
				     unsigned int count)
{
	struct getdents64_args *args = this_cpu_ptr(&gd64_args);

	/* Only intercept sysfs directories */
	if (!is_sysfs_fd(fd))
		return;

	args->pid = current->pid;
	args->fd = fd;
	args->dirent = dirent;
	args->count = count;
	args->active = true;
}

/*
 * Called at sys_exit for getdents64.
 * Filters the result buffer to remove emulator entries.
 * Returns the (possibly modified) return value.
 */
long ksu_sysfs_hide_getdents64_exit(long ret)
{
	struct getdents64_args *args = this_cpu_ptr(&gd64_args);
	struct ksu_dirent64 *kbuf, *src, *dst;
	int remaining, new_len;

	if (!args->active || args->pid != current->pid) {
		args->active = false;
		return ret;
	}
	args->active = false;

	if (ret <= 0)
		return ret;

	kbuf = kmalloc(ret, GFP_ATOMIC);
	if (!kbuf)
		return ret;

	if (copy_from_user(kbuf, args->dirent, ret)) {
		kfree(kbuf);
		return ret;
	}

	/* Walk entries, skip emulator ones */
	src = kbuf;
	dst = kbuf;
	remaining = ret;
	new_len = 0;

	while (remaining > 0) {
		unsigned short reclen = src->d_reclen;
		unsigned int namelen;

		if (reclen == 0 || reclen > remaining)
			break;

		namelen = strnlen(src->d_name,
				  reclen - offsetof(struct ksu_dirent64, d_name));

		if (!name_matches_emulator(src->d_name, namelen)) {
			if (dst != src)
				memmove(dst, src, reclen);
			dst = (void *)dst + reclen;
			new_len += reclen;
		}

		src = (void *)src + reclen;
		remaining -= reclen;
	}

	if (new_len < ret) {
		if (copy_to_user(args->dirent, kbuf, new_len)) {
			kfree(kbuf);
			return ret;
		}
		kfree(kbuf);
		return new_len;
	}

	kfree(kbuf);
	return ret;
}

/*
 * Check if a user-provided path is a sysfs emulator path that should be hidden.
 * Used to make openat/stat/access return ENOENT for these paths.
 */
bool ksu_sysfs_hide_should_block_path(const char __user *pathname)
{
	char kpath[256];
	int len;

	if (!pathname)
		return false;

	len = strncpy_from_user(kpath, pathname, sizeof(kpath) - 1);
	if (len <= 0)
		return false;
	kpath[len] = '\0';

	/* Only filter /sys/ paths */
	if (strncmp(kpath, "/sys/", 5) != 0)
		return false;

	return name_matches_emulator(kpath, len);
}
