#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kprobes.h>
#include <linux/dirent.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/file.h>
#include <linux/dcache.h>
#include <linux/fs.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("kosemu");
MODULE_DESCRIPTION("Hide emulator sysfs entries from getdents64");

static const char *filter_patterns[] = {
    "goldfish", "virtio", "ranchu", "qemu", NULL
};

static int matches_filter(const char *name)
{
    const char **p;
    for (p = filter_patterns; *p; p++)
        if (strstr(name, *p))
            return 1;
    return 0;
}

struct saved_args {
    int fd;
    void __user *buf;
    int is_sysfs;
};

static int getdents64_entry_handler(struct kretprobe_instance *ri, struct pt_regs *regs)
{
    struct saved_args *data = (struct saved_args *)ri->data;
    struct fd f;
    char pathbuf[256];
    char *path;

    data->fd = (int)regs->regs[0];
    data->buf = (void __user *)regs->regs[1];
    data->is_sysfs = 0;

    f = fdget(data->fd);
    if (f.file) {
        path = d_path(&f.file->f_path, pathbuf, sizeof(pathbuf));
        if (!IS_ERR(path)) {
            if (strstr(path, "/sys/devices/platform") ||
                strstr(path, "/sys/bus") ||
                strstr(path, "/sys/class"))
                data->is_sysfs = 1;
        }
        fdput(f);
    }
    return 0;
}

static int getdents64_ret_handler(struct kretprobe_instance *ri, struct pt_regs *regs)
{
    struct saved_args *data = (struct saved_args *)ri->data;
    long ret;
    char *kbuf;
    int src, dst;
    struct linux_dirent64 *d;

    if (!data->is_sysfs)
        return 0;

    ret = regs_return_value(regs);
    if (ret <= 0)
        return 0;

    kbuf = kmalloc(ret, GFP_ATOMIC);
    if (!kbuf)
        return 0;

    if (copy_from_user(kbuf, data->buf, ret)) {
        kfree(kbuf);
        return 0;
    }

    src = 0;
    dst = 0;
    while (src < ret) {
        d = (struct linux_dirent64 *)(kbuf + src);
        if (d->d_reclen == 0 || d->d_reclen > ret - src)
            break;
        if (!matches_filter(d->d_name)) {
            if (dst != src)
                memmove(kbuf + dst, kbuf + src, d->d_reclen);
            dst += d->d_reclen;
        }
        src += d->d_reclen;
    }

    if (dst != ret) {
        if (!copy_to_user(data->buf, kbuf, dst))
            regs->regs[0] = dst;
    }

    kfree(kbuf);
    return 0;
}

static struct kretprobe getdents64_kretprobe = {
    .handler = getdents64_ret_handler,
    .entry_handler = getdents64_entry_handler,
    .data_size = sizeof(struct saved_args),
    .maxactive = 20,
    .kp.symbol_name = "__arm64_sys_getdents64",
};

static int __init sysfs_hide_init(void)
{
    int ret = register_kretprobe(&getdents64_kretprobe);
    if (ret < 0) {
        pr_err("sysfs_hide: kretprobe failed: %d\n", ret);
        return ret;
    }
    pr_info("sysfs_hide: loaded\n");
    return 0;
}

static void __exit sysfs_hide_exit(void)
{
    unregister_kretprobe(&getdents64_kretprobe);
    pr_info("sysfs_hide: unloaded\n");
}

module_init(sysfs_hide_init);
module_exit(sysfs_hide_exit);
