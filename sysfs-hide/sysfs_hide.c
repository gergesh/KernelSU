/*
 * sysfs_hide.ko - Hide emulator hardware from /sys/devices/platform/
 *
 * Uses kretprobe on __arm64_sys_getdents64 to filter directory entries
 * matching goldfish/virtio/ranchu/qemu patterns.
 *
 * For Android 14 emulator (kernel 6.1.23-android14-4)
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kprobes.h>
#include <linux/dirent.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/dcache.h>
#include <linux/namei.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("kosemu");
MODULE_DESCRIPTION("Hide emulator sysfs entries from getdents64");

static const char *filter_patterns[] = {
    "goldfish", "virtio", "ranchu", "qemu", NULL
};

static int matches_filter(const char *name) {
    const char **p;
    for (p = filter_patterns; *p; p++)
        if (strstr(name, *p))
            return 1;
    return 0;
}

// linux_dirent64 is defined in <linux/dirent.h>
static int is_sysfs_platform_fd(int fd) {
    struct fd f = fdget(fd);
    int result = 0;
    if (f.file) {
        char buf[256];
        char *path = d_path(&f.file->f_path, buf, sizeof(buf));
        if (!IS_ERR(path)) {
            result = (strstr(path, "/sys/devices/platform") != NULL ||
                      strstr(path, "/sys/bus") != NULL ||
                      strstr(path, "/sys/class") != NULL);
        }
        fdput(f);
    }
    return result;
}

// Kretprobe handler: runs AFTER getdents64 returns
static int getdents64_ret_handler(struct kretprobe_instance *ri, struct pt_regs *regs) {
    long ret = regs_return_value(regs);
    
    if (ret <= 0)
        return 0;

    // Get the syscall arguments from the pt_regs
    // On ARM64: x0=fd, x1=dirent_buf, x2=count (but these are the ORIGINAL args)
    // For kretprobe, we saved the args in the entry handler
    struct {
        int fd;
        void __user *buf;
    } *data = (void *)ri->data;

    if (!is_sysfs_platform_fd(data->fd))
        return 0;

    // Read the dirent buffer from userspace
    char *kbuf = kmalloc(ret, GFP_KERNEL);
    if (!kbuf) return 0;

    if (copy_from_user(kbuf, data->buf, ret)) {
        kfree(kbuf);
        return 0;
    }

    // Filter entries
    int src = 0, dst = 0;
    while (src < ret) {
        struct linux_dirent64 *d = (struct linux_dirent64 *)(kbuf + src);
        if (d->d_reclen == 0) break;
        
        if (!matches_filter(d->d_name)) {
            if (dst != src)
                memmove(kbuf + dst, kbuf + src, d->d_reclen);
            dst += d->d_reclen;
        } else {
            pr_info("sysfs_hide: filtered %s\n", d->d_name);
        }
        src += d->d_reclen;
    }

    if (dst != ret) {
        // Write filtered buffer back
        if (!copy_to_user(data->buf, kbuf, dst)) {
            // Update return value
            regs->regs[0] = dst;  // ARM64: x0 = return value
        }
    }

    kfree(kbuf);
    return 0;
}

// Entry handler: save syscall arguments
static int getdents64_entry_handler(struct kretprobe_instance *ri, struct pt_regs *regs) {
    struct {
        int fd;
        void __user *buf;
    } *data = (void *)ri->data;

    // ARM64 syscall args: x0=fd, x1=buf, x2=count
    data->fd = (int)regs->regs[0];
    data->buf = (void __user *)regs->regs[1];
    return 0;
}

static struct kretprobe getdents64_kretprobe = {
    .handler = getdents64_ret_handler,
    .entry_handler = getdents64_entry_handler,
    .data_size = sizeof(struct { int fd; void *buf; }),
    .maxactive = 20,
    .kp.symbol_name = "__arm64_sys_getdents64",
};

static int __init sysfs_hide_init(void) {
    int ret = register_kretprobe(&getdents64_kretprobe);
    if (ret < 0) {
        pr_err("sysfs_hide: kretprobe registration failed: %d\n", ret);
        return ret;
    }
    pr_info("sysfs_hide: loaded, filtering sysfs emulator entries\n");
    return 0;
}

static void __exit sysfs_hide_exit(void) {
    unregister_kretprobe(&getdents64_kretprobe);
    pr_info("sysfs_hide: unloaded\n");
}

module_init(sysfs_hide_init);
module_exit(sysfs_hide_exit);
