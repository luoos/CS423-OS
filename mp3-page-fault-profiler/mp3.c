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
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/mm.h>

#include "mp3_given.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("LUOJL");
MODULE_DESCRIPTION("CS-423 MP3");

#define PROC_FILE "status"
#define PROC_DIR  "mp3"
#define RW_BUFSIZE 512
#define PROFILE_PERIOD_MS 50  // millisecond
#define SAMPLE_BUFSIZE 128 * 4 * 1024
#define MAX_SAMPLE_CNT 48000
#define DEVICE_NAME "node"
#define CLASS_NAME "mp3_dev"

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
} mp3_task;

static LIST_HEAD(mp3_task_list);
static DEFINE_MUTEX(task_list_lock);
static int task_cnt = 0;

static struct workqueue_struct *wq;
static struct delayed_work *profiling_work;

static unsigned long *sample_buf;
static int sample_index = 0;

static int dev_major;
static struct class *dev_class = NULL;
static struct device *mp3_dev = NULL;

void sampling(void) {
    mp3_task *task;
    struct list_head *ptr;
    int r;
    unsigned long cur_jiff, min_flt, maj_flt, cpu_time;
    cur_jiff = jiffies;
    min_flt  = 0;
    maj_flt  = 0;
    cpu_time = 0;
    mutex_lock(&task_list_lock);
    list_for_each(ptr, &mp3_task_list) {
        task = list_entry(ptr, mp3_task, lis);
        cur_jiff = jiffies;
        r = get_cpu_use(task->pid, &(task->min_flt), &(task->maj_flt), &(task->utime), &(task->stime));
        if (r == 0) {
            min_flt  += task->min_flt;
            maj_flt  += task->maj_flt;
            cpu_time += (task->utime + task->stime);
        }
    }
    sample_buf[sample_index++] = cur_jiff;
    sample_buf[sample_index++] = min_flt;
    sample_buf[sample_index++] = maj_flt;
    sample_buf[sample_index++] = cpu_time;
    sample_index = sample_index % MAX_SAMPLE_CNT;
    mutex_unlock(&task_list_lock);
}

void work_callback(struct work_struct *work) {
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

static int device_open(struct inode *node, struct file *f) {
    return 0;
}
static int device_release(struct inode *node, struct file *f) {
    return 0;
}

static int device_mmap(struct file *filp, struct vm_area_struct *vma) {
    int index = 0;
    void *buf_pos = sample_buf;
    while (index < SAMPLE_BUFSIZE) {
        if (remap_pfn_range(vma, vma->vm_start + index,
                vmalloc_to_pfn(buf_pos+index), PAGE_SIZE, vma->vm_page_prot)) {
            printk(KERN_ALERT "fail to mmap\n");
            return -EAGAIN;
        }
        index += PAGE_SIZE;
    }
    return 0;
}

static const struct file_operations device_fops = {
    .open = device_open,
    .release = device_release,
    .mmap = device_mmap,
};

void reserve_pages(void *mem_start, int len) {
    int i;
    for(i = 0; i < len; i += PAGE_SIZE) {
        SetPageReserved(vmalloc_to_page(mem_start+i));
    }
}

void un_reserve_pages(void *mem_start, int len) {
    int i;
    for (i = 0; i < len; i += PAGE_SIZE) {
        ClearPageReserved(vmalloc_to_page(mem_start+i));
    }
}

int __init mp3_init(void) {
    // register character device
    dev_major = register_chrdev(0, DEVICE_NAME, &device_fops);
    dev_class = class_create(THIS_MODULE, CLASS_NAME);
    mp3_dev = device_create(dev_class, NULL, MKDEV(dev_major, 0), NULL, DEVICE_NAME);

    // create proc file
    printk(KERN_ALERT "MP3 MODULE INIT");

    sample_buf = vmalloc(SAMPLE_BUFSIZE);
    memset(sample_buf, -1, SAMPLE_BUFSIZE);
    reserve_pages(sample_buf, SAMPLE_BUFSIZE);

    wq = create_workqueue("mp3_wq");
    profiling_work = kmalloc(sizeof(struct delayed_work), GFP_KERNEL);
    INIT_DELAYED_WORK(profiling_work, work_callback);

    proc_dir = proc_mkdir(PROC_DIR, NULL);
    proc_file = proc_create(PROC_FILE, 0666, proc_dir, &file);
    return 0;
}

void __exit mp3_exit(void) {
    // remove character device
    device_destroy(dev_class, MKDEV(dev_major, 0));
    class_destroy(dev_class);
    class_unregister(dev_class);
    unregister_chrdev(dev_major, DEVICE_NAME);

    // remove proc file
    proc_remove(proc_file);
    proc_remove(proc_dir);

    cancel_delayed_work_sync(profiling_work);
    destroy_workqueue(wq);

    free_all_tasks();

    un_reserve_pages(sample_buf, SAMPLE_BUFSIZE);
    vfree(sample_buf);
    printk(KERN_ALERT "MP3 MODULE EXIT");
}

module_init(mp3_init);
module_exit(mp3_exit);