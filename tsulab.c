/* Сколько дней мы наслаждаемся тем фактом, что судный день из Т2 еще не настал. */
#include <linux/init.h>    // Для макросов
#include <linux/kernel.h>  
#include <linux/module.h>
#include <linux/proc_fs.h> // Для работы с файловой системой /proc
#include <linux/uaccess.h> // Для копирования
#include <linux/version.h> // Для проверки версии ядра
#include <linux/time.h>    // Для работы со временем

#define procfs_name "tsu"

static struct proc_dir_entry *our_proc_file = NULL;

static ssize_t procfile_read(struct file *filePointer, char __user *buffer,
                             size_t buffer_length, loff_t *offset)
{
    time64_t nowTime = ktime_get_real_seconds();

    // 29 августа 1997 года. 07:14:00 GMT \\ 02:14:00 EST
    time64_t judgementDay = 872838840;
    
    uint64_t days = (nowTime - judgementDay) / (60 * 60 * 24);

    char msg[50];
    ssize_t strLen = snprintf(msg, sizeof(msg), "How many days have passed: %llu", days);

    if (*offset >= strLen || copy_to_user(buffer, msg, strLen)) {
        pr_info("copy_to_user failed\n");
        return 0;
    }
    
    pr_info("procfile read %s\n", filePointer->f_path.dentry->d_name.name);
    
    *offset += strLen;
        
    return strLen;
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


static int __init procFsInit(void)
{
    our_proc_file = proc_create (
        procfs_name,
        0644, // права на чтение и запись
        NULL,
        &proc_file_fops
    );

    if (NULL == our_proc_file) {
        proc_remove(our_proc_file);
        pr_alert("Error: could not initialize /proc/%s\n", procfs_name);
        return -ENOMEM; // не смогли выделить память
    }

    pr_info("Welcome to Tomsk State University\n");
    pr_info("/proc/%s was created\n", procfs_name);

    return 0;
}


static void __exit procFsExit(void)
{
    proc_remove(our_proc_file); // Удаляем наш файл tsu
    pr_info("/proc/%s was removed\n", procfs_name);
    pr_info("Tomsk State University forever!\n");
}


module_init(procFsInit);
module_exit(procFsExit);


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Petinov Ilya");
MODULE_DESCRIPTION("Laboratory work number 4. Operating systems.");
