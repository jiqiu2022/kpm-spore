#include <hook.h>
#include <linux/printk.h>
#include <kputils.h>
#include <linux/string.h>
#include "maps.h"
#include "../utils/config.h"

static void (*backup_show_map_vma)(struct seq_file *, struct vm_area_struct *);
static int (*backup_show_smap)(struct seq_file *, void *);

static hook_err_t hook_err = HOOK_NOT_HOOK;

static inline bool filter_output(struct seq_file *m, size_t old_count) {
    char *buf_start;
    size_t len;
    int i;
    int config_count;

    if (!m || !m->buf) {
        return false;
    }

    if (m->count <= old_count) {
        return false;
    }

    buf_start = m->buf + old_count;
    len = m->count - old_count;

    config_count = get_config_count();
    if (config_count <= 0) {
        return false;
    }

    for (i = 0; i < config_count; i++) {
        const char *keyword = get_config_content(i);
        if (keyword && strnstr(buf_start, keyword, len)) {
            m->count = old_count;
            return true;
        }
    }

    return false;
}

static void rep_show_map_vma(struct seq_file *m, struct vm_area_struct *vma){
    size_t old_count;

    old_count = m->count;
    backup_show_map_vma(m, vma);

    if (is_proc_eff()) {
        filter_output(m, old_count);
    }
}

static int rep_show_smap(struct seq_file *m, struct vm_area_struct *vma){
    size_t old_count;
    int ret;

    old_count = m->count;
    ret = backup_show_smap(m, vma);

    if (ret == 0 && is_proc_eff()) {
        filter_output(m, old_count);
    }

    return ret;
}


static inline bool hook_all() {
    if (ori_show_map_vma && ori_show_smap) {
        hook_err = hook((void *)ori_show_map_vma, (void *)rep_show_map_vma, (void **)&backup_show_map_vma);
        hook_err = hook((void *)ori_show_smap, (void *)rep_show_smap, (void **)&backup_show_smap);

        if (hook_err != HOOK_NO_ERR) {
            pr_err("[yuuki] hook some fuc error\n");
        } else {
            return true;
        }
    } else {
        hook_err = HOOK_BAD_ADDRESS;
        pr_err("[yuuki] no symbols\n");
    }
    return false;
}

static inline bool install_hook(void) {
    if (hook_err == HOOK_NO_ERR) {
        pr_info("[yuuki] hook already installed, skipping...\n");
        return true;
    }

    if (hook_all()) {
        pr_info("[yuuki] feature hook installed successfully\n");
        return true;
    }

    pr_err("[yuuki] hook installation failed\n");
    return false;
}

static inline bool uninstall_hook(void) {
    if (hook_err != HOOK_NO_ERR) {
        pr_info("[yuuki] not hooked, skipping uninstall...\n");
        return true;
    }

    if (ori_show_map_vma) unhook(ori_show_map_vma);
    if (ori_show_smap) unhook(ori_show_smap);

    hook_err = -1;
    pr_info("[yuuki] feature hooks uninstalled successfully\n");
    return true;
}

static inline bool control_hook(bool enable) {
return enable ? install_hook() : uninstall_hook();
}

long add_maps_hooks() {
    return control_hook(true) ? SUCCESS : FAILED;
}

long remove_maps_hooks() {
    return control_hook(false) ? SUCCESS : FAILED;
}