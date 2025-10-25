#include "fopt.h"
#include "accctl.h"
#include "linux/printk.h"
#include <linux/fs.h>

void *kernel_read_file(const char *path, loff_t *len) { // 设置/data/adb 权限 701
    set_priv_sel_allow(current, true);
    void *data = 0;

    struct file *filp = ori_filp_open(path, O_RDONLY, 0);
    if (!filp || IS_ERR(filp)) {
        pr_info("[yuuki] open file: %s error: %d\n", path, PTR_ERR(filp));
        goto out;
    }
    *len = ori_vfs_llseek(filp, 0, SEEK_END);

    ori_vfs_llseek(filp, 0, SEEK_SET);

    data = kf_vmalloc(*len);
    loff_t pos = 0;
    ssize_t ret = ori_kernel_read(filp, data, *len, &pos);
    if(ret < 0){
        pr_info("[yuuki] ori_kernel_read: %zd\n", ret);
    }
    ori_filp_close(filp, 0);

    out:
    set_priv_sel_allow(current, false);
    return data;
}
