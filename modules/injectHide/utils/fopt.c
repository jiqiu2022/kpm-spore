#include "fopt.h"
#include "accctl.h"
#include "linux/printk.h"
#include <linux/fs.h>
#include <linux/cred.h>

void *kernel_read_file(const char *path, loff_t *len) {
    const struct cred *old_cred = NULL;
    struct cred *new_cred = NULL;
    struct file *filp = NULL;
    void *data = NULL;
    
    // 绕过 SELinux
    set_priv_sel_allow(current, true);
    
    // 提升为 root 权限（绕过 DAC 权限检查）
    new_cred = prepare_kernel_cred(NULL);
    if (!new_cred) {
        pr_err("[yuuki] failed to prepare kernel cred\n");
        goto out_selinux;
    }
    old_cred = override_creds(new_cred);

    // 打开文件（现在可以访问 700 权限的目录了）
    filp = ori_filp_open(path, O_RDONLY, 0);
    if (!filp || IS_ERR(filp)) {
        pr_info("[yuuki] open file: %s error: %ld\n", path, PTR_ERR(filp));
        goto out_cred;
    }
    
    // 获取文件大小
    *len = ori_vfs_llseek(filp, 0, SEEK_END);
    if (*len <= 0) {
        pr_info("[yuuki] file is empty or seek failed: %lld\n", *len);
        goto out_close;
    }
    
    ori_vfs_llseek(filp, 0, SEEK_SET);

    // 分配内存并读取
    data = kf_vmalloc(*len);
    if (!data) {
        pr_err("[yuuki] failed to allocate memory for file\n");
        goto out_close;
    }
    
    loff_t pos = 0;
    ssize_t ret = ori_kernel_read(filp, data, *len, &pos);
    if (ret < 0) {
        pr_err("[yuuki] ori_kernel_read failed: %zd\n", ret);
        kf_vfree(data);
        data = NULL;
    } else if (ret != *len) {
        pr_warn("[yuuki] partial read: %zd/%lld\n", ret, *len);
        *len = ret;  // 更新实际读取的长度
    }

out_close:
    ori_filp_close(filp, 0);
    
out_cred:
    // 恢复原来的 credentials
    if (old_cred) {
        revert_creds(old_cred);
    }
    if (new_cred) {
        __put_cred(new_cred);
    }
    
out_selinux:
    // 恢复 SELinux 设置
    set_priv_sel_allow(current, false);
    
    return data;
}
