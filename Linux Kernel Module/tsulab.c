#include <linux/kernel.h>
#include <linux/module.h> /* Needed by all modules */
#include <linux/printk.h> /* Needed for pr_info() */
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/version.h>
#include <linux/init.h> /* Needed for the macros */

#define procfs_name "tsulab"

static struct proc_dir_entry *some_proc_file = NULL;

static ssize_t procfile_read(struct file *file_pointer, char __user *buffer, size_t buffer_length, loff_t *offset) {
	char s[50] = "";
	s64 uptime;
	int len = sizeof(s);
	ssize_t ret = len;
	
	uptime = ktime_divns(ktime_get_coarse_boottime(), NSEC_PER_SEC);
	if (uptime % 2 == 0) {
		snprintf(s, sizeof(s), "True; Seconds since boot: %lld\n", uptime);
	}
	else {
		snprintf(s, sizeof(s), "False; Seconds since boot: %lld\n", uptime);
	}
	
	if (*offset >= len || copy_to_user(buffer, s, len)) {
		pr_info("copy_to_user failed\n");
		ret = 0;
	} else {
		pr_info("procfile read %s\n", file_pointer->f_path.dentry->d_name.name);
		*offset += len;
	}
	
	return ret;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0)
static const struct proc_ops proc_file_fops = {
    .proc_read = procfile_read,
};
#else
static const struct file_operations proc_file_fops = {
    .read = procfile_read,
};
#endif

static int __init procfs1_init(void) {
	some_proc_file = proc_create(procfs_name, 0644, NULL, &proc_file_fops);
	if (NULL == some_proc_file) {
		proc_remove(some_proc_file);
		pr_alert("Error: Could not initialize /proc/%s\n", procfs_name);
		return -ENOMEM;
	}
	
	pr_info("Welcome to the Tomsk State University\n");
	return 0;
}

static void __exit procfs1_exit(void) {
	proc_remove(some_proc_file);
	pr_info("Tomsk State University forever!\n");
}

module_init(procfs1_init);
module_exit(procfs1_exit);

MODULE_LICENSE("GPL");
