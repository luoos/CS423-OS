#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/slab.h>

#include "mp2_given.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("LUOJL");
MODULE_DESCRIPTION("CS-423 MP2");

#define FILENAME "status"
#define DIRECTORY "mp2"
#define READ_BUFSIZE  512
#define WRITE_BUFSIZE 512
#define STATE_SLEEPING 0
#define STATE_READY 1
#define STATE_RUNNING 2

static struct proc_dir_entry *proc_dir;
static struct proc_dir_entry *proc_entry;

static char write_buffer[WRITE_BUFSIZE];

static DEFINE_MUTEX(RMS_tasks_lock);
static LIST_HEAD(tasks_list);

// RMS: Rate-Monotonic CPU Scheduler
typedef struct RMS_task_struct {
    struct task_struct* linux_task;
    struct timer_list wakeup_timer;
    struct list_head lis;
    int state;
    uint period;
    uint computation;
} RMS_task;

void __add_task(RMS_task *task) {
    mutex_lock(&RMS_tasks_lock);
    list_add(&(task->lis), &tasks_list);
    mutex_unlock(&RMS_tasks_lock);
    printk(KERN_ALERT "added task, pid: %d", task->linux_task->pid);
}

void __del_task(pid_t pid) {
    RMS_task *task;
    struct list_head *ptr;
    struct list_head *tmp;

    mutex_lock(&RMS_tasks_lock);
    list_for_each_safe(ptr, tmp, &tasks_list) {
        task = list_entry(ptr, RMS_task, lis);
        if (task->linux_task->pid == pid) {
            list_del(ptr);
            kfree(task);
            printk(KERN_ALERT "deleted task, pid: %d", pid);
            break;
        }
    }
    mutex_unlock(&RMS_tasks_lock);
}

void action_register(uint pid, uint period, uint computation) {
    RMS_task *t = (RMS_task *) kmalloc(sizeof(RMS_task), GFP_KERNEL);
    struct task_struct *ts = find_task_by_pid(pid);
    printk(KERN_ALERT "registration, pid: %d, period: %d, computation: %d", pid, period, computation);
    t->linux_task = ts;
    t->state = STATE_SLEEPING;
    t->period = period;
    t->computation = computation;
    __add_task(t);
}

void action_yield(uint pid) {
    printk(KERN_ALERT "yield, pid: %d", pid);
}

void action_deregister(uint pid) {
    printk(KERN_ALERT "deregistration: pid: %d", pid);
    __del_task(pid);
}

static ssize_t file_read (struct file *file, char __user *buffer, size_t count, loff_t *data) {
    char buf[READ_BUFSIZE];
    int len = 0;
    RMS_task *task;
    struct list_head *ptr;

    if (*data > 0 || count < READ_BUFSIZE) {
        return 0;
    }

    mutex_lock(&RMS_tasks_lock);
    list_for_each(ptr, &tasks_list) {
        task = list_entry(ptr, RMS_task, lis);
        len += sprintf(buf+len, "%d,%d,%d,%d\n", task->linux_task->pid,
                        task->period, task->computation, task->state);
    }
    mutex_unlock(&RMS_tasks_lock);

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
    uint pid, period, computation;
    int n;
    char action;
    if (count > WRITE_BUFSIZE) {
        buffer_size = WRITE_BUFSIZE;
    }
    if (copy_from_user(write_buffer, buffer, buffer_size)) {
        return -EFAULT;
    }
    n = sscanf(write_buffer, "%c,%d,%d,%d", &action, &pid, &period, &computation);

    if (action == 'Y' && n == 2) {
        action_yield(pid);
    } else if (action == 'R' && n == 4) {
        action_register(pid, period, computation);
    } else if (action == 'D' && n == 2) {
        action_deregister(pid);
    } else {
        printk(KERN_ALERT "fail to interpret command: %s", write_buffer);
        return -EINVAL;
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