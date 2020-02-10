#define LINUX

#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include "mp1_given.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("LUOJL");
MODULE_DESCRIPTION("CS-423 MP1");

#define DEBUG 1
#define FILENAME "status"
#define DIRECTORY "mp1"
#define BUFSIZE 512

static struct proc_dir_entry *proc_dir;
static struct proc_dir_entry *proc_entry;

static ssize_t mp1_read (struct file *file, char __user *buffer, size_t count, loff_t *data) {
    char buf[BUFSIZE];
    int len = 0;

    printk(KERN_ALERT "mp1_read\n");
    if (*data > 0 || count < BUFSIZE) {
        return 0;
    }
    len += sprintf(buf, "Hey there\n");

    if (copy_to_user(buffer, buf, len)) {
       return -EFAULT;
    }
    *data = len;
    return len;
}

static ssize_t mp1_write (struct file *file, const char __user *buffer, size_t count, loff_t *data) {
    printk(KERN_ALERT "mp1_write\n");
    return 0;
}

static const struct file_operations mp1_file = {
    .owner = THIS_MODULE,
    .read  = mp1_read,
    .write = mp1_write,
};

// mp1_init - Called when module is loaded
int __init mp1_init(void)
{
    #ifdef DEBUG
    printk(KERN_ALERT "MP1 MODULE LOADING\n");
    #endif
    // Insert your code here ...
    proc_dir = proc_mkdir(DIRECTORY, NULL);
    proc_entry = proc_create(FILENAME, 0666, proc_dir, & mp1_file);


    printk(KERN_ALERT "MP1 MODULE LOADED\n");
    return 0;
}

// mp1_exit - Called when module is unloaded
void __exit mp1_exit(void)
{
    #ifdef DEBUG
    printk(KERN_ALERT "MP1 MODULE UNLOADING\n");
    #endif
    // Insert your code here ...
    proc_remove(proc_entry);
    proc_remove(proc_dir);

    printk(KERN_ALERT "MP1 MODULE UNLOADED\n");
}

// Register init and exit funtions
module_init(mp1_init);
module_exit(mp1_exit);
