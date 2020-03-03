#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>

#include "mp2_given.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("LUOJL");
MODULE_DESCRIPTION("CS-423 MP2");

#define FILENAME "status"
#define DIRECTORY "mp2"
#define READ_BUFSIZE  512
#define WRITE_BUFSIZE 512

static struct proc_dir_entry *proc_dir;
static struct proc_dir_entry *proc_entry;

static char write_buffer[WRITE_BUFSIZE];

void action_register(int pid, int period, int computation) {
    printk(KERN_ALERT "registration, pid: %d, period: %d, computation: %d", pid, period, computation);
}

void action_yield(int pid) {
    printk(KERN_ALERT "yield, pid: %d", pid);
}

void action_deregister(int pid) {
    printk(KERN_ALERT "deregistration: pid: %d", pid);
}

static ssize_t file_read (struct file *file, char __user *buffer, size_t count, loff_t *data) {
    char buf[READ_BUFSIZE];
    int len = 0;

    if (*data > 0 || count < READ_BUFSIZE) {
        return 0;
    }

    len += sprintf(buf+len, "file_read\n");

    if (copy_to_user(buffer, buf, len)) {
       return -EFAULT;
    }
    *data += len;
    return len;
}

/*
 * Registration: "R,PID,PERIOD,COMPUTATION"
 * YIELD: "Y,PID"
 * DE-REGISTRATION: "D,PID"
 */
static ssize_t file_write (struct file *file, const char __user *buffer, size_t count, loff_t *data) {
    int buffer_size = count;
    int pid, period, computation, n;
    char action;
    if (count > WRITE_BUFSIZE) {
        buffer_size = WRITE_BUFSIZE;
    }
    if (copy_from_user(write_buffer, buffer, buffer_size)) {
        return -EFAULT;
    }
    n = sscanf(write_buffer, "%c,%d,%d,%d", &action, &pid, &period, &computation);

    if (action == 'R' && n == 4) {
        action_register(pid, period, computation);
    } else if (action == 'Y' && n == 2) {
        action_yield(pid);
    } else if (action == 'D' && n == 2) {
        action_deregister(pid);
    } else {
        printk(KERN_ALERT "fail to interpret command: %s", write_buffer);
    }
    return buffer_size;
}

static const struct file_operations file = {
    .owner = THIS_MODULE,
    .read  = file_read,
    .write = file_write,
};

// mp2_init - Called when module is loaded
int __init sche_init(void)
{
    #ifdef DEBUG
    printk(KERN_ALERT "MP2 MODULE LOADING\n");
    #endif
    // Insert your code here ...
    proc_dir = proc_mkdir(DIRECTORY, NULL);
    proc_entry = proc_create(FILENAME, 0666, proc_dir, &file);

    printk(KERN_ALERT "MP2 MODULE LOADED\n");
    return 0;
}

// mp2_exit - Called when module is unloaded
void __exit sche_exit(void)
{
    #ifdef DEBUG
    printk(KERN_ALERT "MP2 MODULE UNLOADING\n");
    #endif

    proc_remove(proc_entry);
    proc_remove(proc_dir);

    printk(KERN_ALERT "MP2 MODULE UNLOADED\n");
}

// Register init and exit funtions
module_init(sche_init);
module_exit(sche_exit);