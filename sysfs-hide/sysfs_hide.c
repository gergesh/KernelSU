#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kprobes.h>
#include <linux/fs.h>
#include <linux/dcache.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("kosemu");
MODULE_DESCRIPTION("Hide emulator sysfs entries via filldir64 kprobe");

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

/* Track whether the current iterate_dir is on a sysfs path */
static DEFINE_PER_CPU(int, in_sysfs_dir);

/* kprobe on iterate_dir entry: check if this is a sysfs directory */
static int iterate_dir_entry(struct kprobe *p, struct pt_regs *regs)
{
    /* x0 = struct file *file */
    struct file *file = (struct file *)regs->regs[0];
    char buf[256];
    char *path;

    this_cpu_write(in_sysfs_dir, 0);

    if (file) {
        path = d_path(&file->f_path, buf, sizeof(buf));
        if (!IS_ERR(path)) {
            if (strstr(path, "/sys/devices/platform") ||
                strstr(path, "/sys/bus") ||
                strstr(path, "/sys/class"))
                this_cpu_write(in_sysfs_dir, 1);
        }
    }
    return 0;
}

static struct kprobe iterate_dir_kp = {
    .symbol_name = "iterate_dir",
    .pre_handler = iterate_dir_entry,
};

/* kprobe on filldir64: skip entries matching filter if we're in sysfs */
static int filldir64_entry(struct kprobe *p, struct pt_regs *regs)
{
    /* x1 = const char *name */
    const char *name = (const char *)regs->regs[1];

    if (!this_cpu_read(in_sysfs_dir))
        return 0;

    if (matches_filter(name)) {
        /* Skip this entry by making filldir64 return immediately.
         * Set PC to the return address (LR) and set x0=true (continue iterating) */
        regs->pc = regs->regs[30]; /* LR = return address */
        regs->regs[0] = 1;         /* return true = continue */
        return 1; /* skip the probed instruction (don't execute filldir64) */
    }
    return 0;
}

static struct kprobe filldir64_kp = {
    .symbol_name = "filldir64",
    .pre_handler = filldir64_entry,
};

static int __init sysfs_hide_init(void)
{
    int ret;

    ret = register_kprobe(&iterate_dir_kp);
    if (ret < 0) {
        pr_err("sysfs_hide: iterate_dir kprobe failed: %d\n", ret);
        return ret;
    }

    ret = register_kprobe(&filldir64_kp);
    if (ret < 0) {
        pr_err("sysfs_hide: filldir64 kprobe failed: %d\n", ret);
        unregister_kprobe(&iterate_dir_kp);
        return ret;
    }

    pr_info("sysfs_hide: loaded, hooking iterate_dir + filldir64\n");
    return 0;
}

static void __exit sysfs_hide_exit(void)
{
    unregister_kprobe(&filldir64_kp);
    unregister_kprobe(&iterate_dir_kp);
    pr_info("sysfs_hide: unloaded\n");
}

module_init(sysfs_hide_init);
module_exit(sysfs_hide_exit);
