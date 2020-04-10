#include <linux/sched.h>
#include <linux/proc_fs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/uaccess.h>

#include "mp3_given.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("LUOJL");
MODULE_DESCRIPTION("CS-423 MP3");

#define PROC_FILE "status"
#define PROC_DIR  "mp3"
#define RW_BUFSIZE 512

static struct proc_dir_entry *proc_dir;
static struct proc_dir_entry *proc_file;

void action_register(pid_t pid) {
    printk("register pid: %d\n", pid);
}

void action_deregister(pid_t pid) {
    printk("deregister pid: %d\n", pid);
}

static ssize_t file_read(struct file *file, char __user *buffer, size_t count, loff_t *data) {
    return 0;
}

static ssize_t file_write(struct file *file, const char __user *user_buffer, size_t count, loff_t *data) {
    char buffer[RW_BUFSIZE];
    int buffer_size = count;
    char action;
    pid_t pid;
    int n;

    if (count > RW_BUFSIZE) {
        buffer_size = RW_BUFSIZE;
    }
    if (copy_from_user(buffer, user_buffer, buffer_size)) {
        return -EFAULT;
    }
    n = sscanf(buffer, "%c %d", &action, &pid);

    if (action == 'R' && n == 2) {
        action_register(pid);
    } else if (action == 'U' && n == 2) {
        action_deregister(pid);
    } else {
        printk(KERN_ALERT "fail to interpret command: %s\n", buffer);
        return -EINVAL;
    }
    return buffer_size;
}

static const struct file_operations file = {
    .owner = THIS_MODULE,
    .read  = file_read,
    .write = file_write,
};

int __init mp3_init(void) {
    // register character device

    // create proc file
    printk(KERN_ALERT "MP3 MODULE INIT");

    proc_dir = proc_mkdir(PROC_DIR, NULL);
    proc_file = proc_create(PROC_FILE, 0666, proc_dir, &file);
    return 0;
}

void __exit mp3_exit(void) {
    // remove character device

    // remove proc file
    proc_remove(proc_file);
    proc_remove(proc_dir);
    printk(KERN_ALERT "MP3 MODULE EXIT");
}

module_init(mp3_init);
module_exit(mp3_exit);