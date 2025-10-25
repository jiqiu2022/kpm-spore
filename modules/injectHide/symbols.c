#include "symbols.h"

void (*ori_show_map_vma)(struct seq_file *, struct vm_area_struct *);
int (*ori_show_smap)(struct seq_file *, void *);
struct file *(*ori_filp_open)(const char *filename, int flags, umode_t mode);
int (*ori_filp_close)(struct file *filp, void* id);
loff_t (*ori_vfs_llseek)(struct file *file, loff_t offset, int whence);
ssize_t (*ori_kernel_read)(struct file *file, void *buf, size_t count, loff_t *pos);
struct mm_struct *(*kf_get_task_mm)(struct task_struct *task);
void (*kf_mmput)(struct mm_struct *mm);
void *(*kf_vmalloc)(unsigned long size);
void (*kf_vfree)(const void *addr);

int init_symbols(void) {
    static struct symbol_entry symbols[] = {
            {"get_task_mm", (void **)&kf_get_task_mm, "get_task_mm"},
            {"mmput", (void **)&kf_mmput, "mmput"},
            {"show_map_vma", (void **)&ori_show_map_vma, "show_map_vma"},
            {"show_smap", (void **)&ori_show_smap, "show_smap"},
            {"filp_open", (void **)&ori_filp_open, "filp_open"},
            {"filp_close", (void **)&ori_filp_close, "filp_close"},
            {"vfs_llseek", (void **)&ori_vfs_llseek, "vfs_llseek"},
            {"kernel_read", (void **)&ori_kernel_read, "kernel_read"},
            {"vmalloc", (void **)&kf_vmalloc, "vmalloc"},
            {"vfree", (void **)&kf_vfree, "vfree"},
            {NULL, NULL, NULL} // 结束标记
    };

    struct symbol_entry *entry;

    for (entry = symbols; entry->name; entry++) {
        *entry->addr = (void *)kallsyms_lookup_name(entry->name);
        if (!*entry->addr) {
            pr_info("[yuuki] kernel func: '%s' does not exist!\n", entry->desc);
            return 1;
        }
    }

    return 0;
}