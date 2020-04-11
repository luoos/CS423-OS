#include <linux/sched.h>
#include <linux/proc_fs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
#include <linux/vmalloc.h>

#include "mp3_given.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("LUOJL");
MODULE_DESCRIPTION("CS-423 MP3");

#define PROC_FILE "status"
#define PROC_DIR  "mp3"
#define RW_BUFSIZE 512
#define PROFILE_PERIOD_MS 2000  // millisecond
#define SAMPLE_BUFSIZE 128 * 4 * 1024
#define MAX_SAMPLE_CNT 48000

static struct proc_dir_entry *proc_dir;
static struct proc_dir_entry *proc_file;

typedef struct mp3_task_struct {
    struct task_struct* linux_task;
    struct list_head lis;
    pid_t pid;
    unsigned long utime;
    unsigned long stime;
    unsigned long maj_flt;
    unsigned long min_flt;
    unsigned long last_jiff;
} mp3_task;

static LIST_HEAD(mp3_task_list);
static DEFINE_MUTEX(task_list_lock);
static int task_cnt = 0;

static struct workqueue_struct *wq;
static struct delayed_work *profiling_work;

static unsigned long *sample_buf;
static int sample_index = 0;

void sampling(void) {
    mp3_task *task;
    struct list_head *ptr;
    int r;
    unsigned long cur_jiff;

    mutex_lock(&task_list_lock);
    list_for_each(ptr, &mp3_task_list) {
        task = list_entry(ptr, mp3_task, lis);
        cur_jiff = jiffies;
        r = get_cpu_use(task->pid, &(task->min_flt), &(task->maj_flt), &(task->utime), &(task->stime));
        if (r == 0) {
            sample_buf[sample_index++] = cur_jiff;
            sample_buf[sample_index++] = task->min_flt;
            sample_buf[sample_index++] = task->maj_flt;
            sample_buf[sample_index++] = ((task->utime + task->stime) / (cur_jiff - task->last_jiff)) * 1000 ;
        }
        task->last_jiff = cur_jiff;
        sample_index = sample_index % MAX_SAMPLE_CNT;
    }
    mutex_unlock(&task_list_lock);
}

void work_callback(struct work_struct *work) {
    printk(KERN_ALERT "profling...\n");
    sampling();
    queue_delayed_work(wq, profiling_work, msecs_to_jiffies(PROFILE_PERIOD_MS));
}

int __add_task(mp3_task *task) {
    printk(KERN_ALERT "add task, pid: %d\n", task->pid);
    mutex_lock(&task_list_lock);
    list_add(&(task->lis), &mp3_task_list);
    task_cnt += 1;
    mutex_unlock(&task_list_lock);
    return task_cnt;
}

int __del_task(pid_t pid) {
    mp3_task *task;
    struct list_head *ptr;
    struct list_head *tmp;

    mutex_lock(&task_list_lock);
    list_for_each_safe(ptr, tmp, &mp3_task_list) {
        task = list_entry(ptr, mp3_task, lis);
        if (task->pid == pid) {
            list_del(ptr);
            kfree(task);
            printk(KERN_ALERT "deleted task, pid: %d\n", pid);
            task_cnt -= 1;
            break;
        }
    }
    mutex_unlock(&task_list_lock);
    return task_cnt;
}

void free_all_tasks(void) {
    mp3_task *task;
    struct list_head *ptr, *tmp;
    mutex_lock(&task_list_lock);
    list_for_each_safe(ptr, tmp, &mp3_task_list) {
        task = list_entry(ptr, mp3_task, lis);
        list_del(ptr);
        kfree(task);
    }
    mutex_unlock(&task_list_lock);
}

void start_profiling(void) {
    printk(KERN_ALERT "start profiling...\n");
    sample_index = 0;
    queue_delayed_work(wq, profiling_work, msecs_to_jiffies(PROFILE_PERIOD_MS));
}

void stop_profiling(void) {
    cancel_delayed_work_sync(profiling_work);
    printk(KERN_ALERT "stop profiling...\n");
}

void action_register(pid_t pid) {
    struct task_struct *linux_task;
    mp3_task *task;
    int exist_task_cnt;

    linux_task = find_task_by_pid(pid);
    if (linux_task != NULL) {
        task = (mp3_task *) kmalloc(sizeof(mp3_task), GFP_KERNEL);
        task->pid = pid;
        task->linux_task = linux_task;
        task->last_jiff = jiffies;
        exist_task_cnt = __add_task(task);

        if (exist_task_cnt == 1) {
            start_profiling();
        }
    }
}

void action_deregister(pid_t pid) {
    int exist_task_cnt = __del_task(pid);;
    if (exist_task_cnt == 0) {
        stop_profiling();
    }
}

static ssize_t file_read(struct file *file, char __user *buffer, size_t count, loff_t *data) {
    char buf[RW_BUFSIZE];
    int len = 0;
    mp3_task *task;
    struct list_head *ptr;

    if (*data > 0 || count < RW_BUFSIZE) {
        return 0;
    }

    mutex_lock(&task_list_lock);
    list_for_each(ptr, &mp3_task_list) {
        task = list_entry(ptr, mp3_task, lis);
        len += sprintf(buf+len, "%d\n", task->pid);
    }
    mutex_unlock(&task_list_lock);
    if (copy_to_user(buffer, buf, len)) {
        return -EFAULT;
    }
    *data += len;
    return len;
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

    sample_buf = vmalloc(SAMPLE_BUFSIZE);
    memset(sample_buf, -1, SAMPLE_BUFSIZE);

    wq = create_workqueue("mp3_wq");
    profiling_work = kmalloc(sizeof(struct delayed_work), GFP_KERNEL);
    INIT_DELAYED_WORK(profiling_work, work_callback);

    proc_dir = proc_mkdir(PROC_DIR, NULL);
    proc_file = proc_create(PROC_FILE, 0666, proc_dir, &file);
    return 0;
}

void __exit mp3_exit(void) {
    // remove character device

    // remove proc file
    proc_remove(proc_file);
    proc_remove(proc_dir);

    cancel_delayed_work_sync(profiling_work);
    destroy_workqueue(wq);

    free_all_tasks();

    vfree(sample_buf);
    printk(KERN_ALERT "MP3 MODULE EXIT");
}

module_init(mp3_init);
module_exit(mp3_exit);