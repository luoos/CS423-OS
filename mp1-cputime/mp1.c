#define LINUX

#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/list.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <linux/timer.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include "mp1_given.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("LUOJL");
MODULE_DESCRIPTION("CS-423 MP1");

#define DEBUG 1
#define FILENAME "status"
#define DIRECTORY "mp1"
#define BUFSIZE 512
#define WRITE_BUFSIZE 1024
#define UPDATE_INTERVAL 5000 // 5 seconds

static struct proc_dir_entry *proc_dir;
static struct proc_dir_entry *proc_entry;

typedef struct cpu_usage_list {
    int pid;
    unsigned long usage;
    struct list_head lis;
} cpu_usage;

LIST_HEAD(usage_head);
DEFINE_RWLOCK(list_lock);

static char write_buffer[WRITE_BUFSIZE];

static struct timer_list cpu_usage_timer;
void fire_timer(void);

void timer_callback(unsigned long data);

// for test
void create_list(void) {
    cpu_usage *p = NULL;
    int i = 0;
    for (; i < 3; i++) {
        p = (cpu_usage *) kmalloc(sizeof(cpu_usage), GFP_ATOMIC);

        if (!p) return;
        p->pid = i;
        p->usage = i * 10;
        list_add(&(p->lis), &usage_head);
    }
}

void free_list(void) {
    cpu_usage *cp;
    struct list_head *ptr;

    list_for_each(ptr, &usage_head) {
        cp = list_entry(ptr, cpu_usage, lis);
        printk(KERN_ALERT "kfree for pid: %d", cp->pid);
        kfree(cp);
    }
}

void add_pid(int pid) {
    cpu_usage *p = (cpu_usage *) kmalloc(sizeof(cpu_usage), GFP_ATOMIC);
    if (!p) {
        printk(KERN_ALERT "fail to malloc for cpu_usage");
        return;
    }
    write_lock(&list_lock);
    p->pid = pid;
    p->usage = 0;
    list_add(&(p->lis), &usage_head);
    write_unlock(&list_lock);
}

void update_cpu_usage(unsigned long unused) {
    cpu_usage *cp;
    struct list_head *ptr;
    struct list_head *tmp;
    int r;

    write_lock(&list_lock);
    list_for_each_safe(ptr, tmp, &usage_head) {
        cp = list_entry(ptr, cpu_usage, lis);
        r = get_cpu_use(cp->pid, &(cp->usage));
        if (r == -1) {
            // process disappear
            list_del(ptr);
            kfree(cp);
        }
    }
    write_unlock(&list_lock);
}

DECLARE_TASKLET (update_cpu_usage_tasklet, update_cpu_usage, 0);

void timer_callback(unsigned long data) {
    tasklet_schedule(&update_cpu_usage_tasklet);
    fire_timer();
}

void fire_timer(void) {
    mod_timer(&cpu_usage_timer, jiffies + msecs_to_jiffies(UPDATE_INTERVAL));
}

static ssize_t mp1_read (struct file *file, char __user *buffer, size_t count, loff_t *data) {
    char buf[BUFSIZE];
    int len = 0;
    cpu_usage *cp;
    struct list_head *ptr;

    printk(KERN_ALERT "mp1_read\n");
    if (*data > 0 || count < BUFSIZE) {
        return 0;
    }

    list_for_each(ptr, &usage_head) {
        cp = list_entry(ptr, cpu_usage, lis);
        len += sprintf(buf+len, "%d: %lu\n", cp->pid, cp->usage);
    }

    if (copy_to_user(buffer, buf, len)) {
       return -EFAULT;
    }
    *data = len;
    return len;
}

static ssize_t mp1_write (struct file *file, const char __user *buffer, size_t count, loff_t *data) {
    int buffer_size = count;
    int pid, n;
    if (count > WRITE_BUFSIZE) {
        buffer_size = WRITE_BUFSIZE;
    }
    if (copy_from_user(write_buffer, buffer, buffer_size)) {
        return -EFAULT;
    }
    n = sscanf(write_buffer, "%d", &pid);
    if (n == 1) {
        add_pid(pid);
    } else {
        printk(KERN_ALERT "fail, buf: %s", write_buffer);
    }
    return buffer_size;
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

    // create_list();
    setup_timer(&cpu_usage_timer, timer_callback, 0);
    fire_timer();

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
    del_timer_sync(&cpu_usage_timer);
    tasklet_disable(&update_cpu_usage_tasklet);

    proc_remove(proc_entry);
    proc_remove(proc_dir);

    free_list();

    printk(KERN_ALERT "MP1 MODULE UNLOADED\n");
}

// Register init and exit funtions
module_init(mp1_init);
module_exit(mp1_exit);
