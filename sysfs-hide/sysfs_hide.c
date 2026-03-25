#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kprobes.h>
#include <linux/atomic.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("kosemu");
MODULE_DESCRIPTION("Hide emulator sysfs entries from getdents64");

static atomic_t call_count = ATOMIC_INIT(0);

static int getdents64_entry_handler(struct kretprobe_instance *ri, struct pt_regs *regs)
{
    atomic_inc(&call_count);
    return 0;
}

static int getdents64_ret_handler(struct kretprobe_instance *ri, struct pt_regs *regs)
{
    /* No-op for now - just proving the kretprobe works */
    return 0;
}

static struct kretprobe getdents64_kretprobe = {
    .handler = getdents64_ret_handler,
    .entry_handler = getdents64_entry_handler,
    .data_size = 0,
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
    pr_info("sysfs_hide: loaded (test mode - counting only)\n");
    return 0;
}

static void __exit sysfs_hide_exit(void)
{
    unregister_kretprobe(&getdents64_kretprobe);
    pr_info("sysfs_hide: unloaded, saw %d getdents64 calls\n", atomic_read(&call_count));
}

module_init(sysfs_hide_init);
module_exit(sysfs_hide_exit);
