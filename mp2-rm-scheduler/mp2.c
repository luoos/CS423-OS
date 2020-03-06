#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/timer.h>
#include <linux/kthread.h>

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

static DEFINE_MUTEX(RMS_tasks_lock);
static LIST_HEAD(tasks_list);

static struct task_struct *dispatcher;

// RMS: Rate-Monotonic CPU Scheduler
typedef struct RMS_task_struct {
    struct task_struct* linux_task;
    struct timer_list wakeup_timer;
    struct list_head lis;
    int state;
    pid_t pid;
    unsigned long period_ms;
    unsigned long compute_time_ms;
    unsigned long deadline_jiff;
} RMS_task;

RMS_task* running_task;

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

RMS_task* __get_task(pid_t pid) {
    RMS_task *task = NULL;
    RMS_task *tmp;
    mutex_lock(&RMS_tasks_lock);
    list_for_each_entry(tmp, &tasks_list, lis) {
        if (tmp->pid == pid) {
            task = tmp;
            break;
        }
    }
    mutex_unlock(&RMS_tasks_lock);
    return task;
}

RMS_task* __get_to_run_task(void) {
    RMS_task *task = NULL;
    RMS_task *tmp;
    unsigned long min_period = INT_MAX;
    mutex_lock(&RMS_tasks_lock);
    list_for_each_entry(tmp, &tasks_list, lis) {
        if (tmp->state == STATE_READY && tmp->period_ms < min_period) {
            task = tmp;
            min_period = tmp->period_ms;
        }
    }
    mutex_unlock(&RMS_tasks_lock);
    return task;
}

void free_all_tasks(void) {
    RMS_task *task;
    struct list_head *ptr, *tmp;
    mutex_lock(&RMS_tasks_lock);
    list_for_each_safe(ptr, tmp, &tasks_list) {
        task = list_entry(ptr, RMS_task, lis);
        del_timer(&(task->wakeup_timer));
        list_del(ptr);
        kfree(task);
    }
    mutex_unlock(&RMS_tasks_lock);
}

void __timer_callback(unsigned long data) {
    pid_t pid = (pid_t) data;
    RMS_task *task = __get_task(pid);
    if (task == NULL) {
        printk(KERN_ALERT "[WARN] timer callback NULL task, pid: %d", pid);
        return;
    }
    task->state = STATE_READY;
    wake_up_process(dispatcher);
    mod_timer(&(task->wakeup_timer), jiffies + msecs_to_jiffies(task->period_ms));
}

void run_task(struct task_struct *task) {
    struct sched_param sparam;
    wake_up_process(task);
    sparam.sched_priority = 99;
    sched_setscheduler(task, SCHED_FIFO, &sparam);
}

void pause_task(struct task_struct *task) {
    struct sched_param sparam;
    sparam.sched_priority = 0;
    sched_setscheduler(task, SCHED_NORMAL, &sparam);
}

int dispatching(void *data) {
    RMS_task *task_to_run;

    while (1) {
        set_current_state(TASK_INTERRUPTIBLE);
        schedule();

        if (kthread_should_stop()) return 0;

        task_to_run = __get_to_run_task();

        if (task_to_run == NULL) {
            if (running_task && running_task->state == STATE_RUNNING) {
                running_task->state = STATE_READY;
                pause_task(running_task->linux_task);
                running_task = NULL;
            }
        } else {
            if (running_task) {
                running_task->state = STATE_READY;
                pause_task(running_task->linux_task);
            }

            task_to_run->state = STATE_RUNNING;
            run_task(task_to_run->linux_task);
        }
        running_task = task_to_run;
    }
}

int admission_control(unsigned long period, unsigned computation) {
    unsigned long portion = (computation * 10000) / period;
    RMS_task *task = NULL;

    mutex_lock(&RMS_tasks_lock);
    list_for_each_entry(task, &tasks_list, lis) {
        portion += (task->compute_time_ms * 10000) / task->period_ms;
    }
    mutex_unlock(&RMS_tasks_lock);
    if (portion <= 6930) {
        return 1; // pass
    }
    return 0;  // fail
}

void action_register(pid_t pid, unsigned long period, unsigned long computation) {
    RMS_task *t;
    struct task_struct *ts;

    if (admission_control(period, computation) == 0) {
        printk(KERN_ALERT "process %d failed to pass admission_control", pid);
        return;
    }
    t = (RMS_task *) kmalloc(sizeof(RMS_task), GFP_KERNEL);
    ts = find_task_by_pid(pid);
    printk(KERN_ALERT "registration, pid: %d, period: %lu, computation: %lu", pid, period, computation);
    t->pid = pid;
    t->linux_task = ts;
    t->state = STATE_SLEEPING;
    t->period_ms = period;
    t->compute_time_ms = computation;
    t->deadline_jiff = 0;
    __add_task(t);
    setup_timer(&t->wakeup_timer, __timer_callback, ts->pid);
}

void action_yield(pid_t pid) {
    RMS_task *task;
    if (running_task && running_task->pid == pid) {
        task = running_task;
    } else {
        task = __get_task(pid);
    }
    if (task == NULL) {
        printk(KERN_ALERT "[Err] no such task to yield, pid: %d", pid);
        return;
    }

    if (task->deadline_jiff == 0) {
        // first time to yield
        unsigned long sleep_ms = task->period_ms - task->compute_time_ms;
        task->deadline_jiff = jiffies + msecs_to_jiffies(task->period_ms);
        mod_timer(&(task->wakeup_timer), jiffies + msecs_to_jiffies(sleep_ms));
    }

    if (running_task && running_task->pid == pid) {
        running_task = NULL;
    }
    task->state = STATE_SLEEPING;
    set_task_state(task->linux_task, TASK_INTERRUPTIBLE);
    schedule();
}

void action_deregister(pid_t pid) {
    RMS_task *task;
    if (running_task && running_task->pid == pid) {
        task = running_task;
    } else {
        task = __get_task(pid);
    }
    del_timer(&task->wakeup_timer);
    __del_task(pid);
    if (running_task && running_task->pid == pid) {
        running_task = NULL;
        wake_up_process(dispatcher);
    }
    printk(KERN_ALERT "deregistration: pid: %d", pid);
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
        len += sprintf(buf+len, "%d,%lu,%lu,%d\n", task->pid,
                        task->period_ms, task->compute_time_ms, task->state);
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
    char write_buffer[WRITE_BUFSIZE];
    int buffer_size = count;
    pid_t pid;
    unsigned long period, computation;
    int n;
    char action;
    if (count > WRITE_BUFSIZE) {
        buffer_size = WRITE_BUFSIZE;
    }
    if (copy_from_user(write_buffer, buffer, buffer_size)) {
        return -EFAULT;
    }
    n = sscanf(write_buffer, "%c,%d,%lu,%lu", &action, &pid, &period, &computation);

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

    dispatcher = kthread_run(dispatching, NULL, "dispatching");

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

    free_all_tasks();

    printk(KERN_ALERT "MP2 MODULE UNLOADED\n");
}

// Register init and exit funtions
module_init(sche_init);
module_exit(sche_exit);