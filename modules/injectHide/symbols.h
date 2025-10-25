#ifndef KPM_BUILD_ANYWHERE_SYMBOLS_H
#define KPM_BUILD_ANYWHERE_SYMBOLS_H

#include "linux/printk.h"

#define HOOK_NOT_HOOK (-1)
#define FAILED (-1)
#define SUCCESS 0

#define SEEK_SET 0
#define SEEK_END 2

#define CONFIG_PATH "/data/local/tmp/kpm_hide_config.txt"

struct config {
    char *keyword;
    // ...
};

struct symbol_entry {
    const char *name;
    void **addr;
    const char *desc;
};

struct seq_file;
struct vm_area_struct;
struct file;
struct task_struct;
struct mm_struct;

extern void (*ori_show_map_vma)(struct seq_file *, struct vm_area_struct *);
extern int (*ori_show_smap)(struct seq_file *, void *);
extern struct file *(*ori_filp_open)(const char *filename, int flags, umode_t mode);
extern int (*ori_filp_close)(struct file *filp, void* id);
extern loff_t (*ori_vfs_llseek)(struct file *file, loff_t offset, int whence);
extern ssize_t (*ori_kernel_read)(struct file *file, void *buf, size_t count, loff_t *pos);
extern struct mm_struct *(*kf_get_task_mm)(struct task_struct *task);
extern void (*kf_mmput)(struct mm_struct *mm);
extern void *(*kf_vmalloc)(unsigned long size);
extern void (*kf_vfree)(const void *addr);

int init_symbols(void);

#endif