/* SPDX-License-Identifier: GPL-2.0-or-later */
/* 
 * HideMap - Hide specific .so files from /proc/pid/maps
 * Based on Cartographer's approach but adapted for KernelPatch
 */

#include <compiler.h>
#include <kpmodule.h>
#include <linux/printk.h>
#include <common.h>
#include <kputils.h>
#include <hook.h>
#include <kallsyms.h>
#include <linux/string.h>
#include <ktypes.h>

// Helper macros
#define min(a, b) ((a) < (b) ? (a) : (b))

// External kernel functions we'll use
extern int sprintf(char *buf, const char *fmt, ...);
extern int snprintf(char *buf, size_t size, const char *fmt, ...);

KPM_NAME("kpm-hidemap");
KPM_VERSION("1.0.0");
KPM_LICENSE("GPL v2");
KPM_AUTHOR("HideMap");
KPM_DESCRIPTION("Hide specific libraries from /proc/pid/maps");

// Configuration
#define MAX_HIDE_PATTERN_LEN 256
static char hide_pattern[MAX_HIDE_PATTERN_LEN] = {0};
static int pattern_enabled = 0;

// Minimal structure definitions (opaque pointers)
// We don't need full definitions, just enough to work with pointers
struct seq_file;
struct vm_area_struct;
struct file;
struct path;
struct dentry;
struct qstr;

// Backup for original function
static void *original_show_map_vma = NULL;

/**
 * @brief Check if a path contains the hide pattern
 */
static int should_hide_path(const char *path)
{
    if (!pattern_enabled || !path || !hide_pattern[0])
        return 0;
    
    return strstr(path, hide_pattern) != NULL;
}

// Structure offset definitions for vm_area_struct
// These offsets may vary by kernel version - this is a common layout
#define VMA_VM_FILE_OFFSET 0xC8  // Typical offset for vm_file in vm_area_struct

// Structure offset for struct file
#define FILE_F_PATH_OFFSET 0x10  // Typical offset for f_path in struct file

// Structure offset for struct path  
#define PATH_DENTRY_OFFSET 0x8   // Typical offset for dentry in struct path

// Structure offset for struct dentry
#define DENTRY_D_NAME_OFFSET 0x20  // Typical offset for d_name in struct dentry

// Structure offset for struct qstr (d_name)
#define QSTR_NAME_OFFSET 0x8     // Typical offset for name pointer in qstr

/**
 * @brief Hook callback for show_map_vma - before function
 * Note: This uses pointer arithmetic to access struct members
 * Offsets may need adjustment for different kernel versions
 */
static void before_show_map_vma(hook_fargs2_t *args, void *udata)
{
    void *vma_ptr = (void *)args->arg1;
    
    if (!pattern_enabled || !hide_pattern[0] || !vma_ptr)
        return;
    
    // Try to get vm_file from vma using pointer arithmetic
    // vm_file is typically at offset 0xC8 in vm_area_struct
    void **file_ptr_location = (void **)((char *)vma_ptr + VMA_VM_FILE_OFFSET);
    void *file = *file_ptr_location;
    
    if (!file)
        return;  // No file associated with this mapping
    
    // Get f_path from file (typically at offset 0x10)
    void *path_ptr = (char *)file + FILE_F_PATH_OFFSET;
    
    // Get dentry from path (typically at offset 0x8)
    void **dentry_ptr_location = (void **)((char *)path_ptr + PATH_DENTRY_OFFSET);
    void *dentry = *dentry_ptr_location;
    
    if (!dentry)
        return;
    
    // Get d_name (qstr) from dentry (typically at offset 0x20)
    void *qstr_ptr = (char *)dentry + DENTRY_D_NAME_OFFSET;
    
    // Get name pointer from qstr (typically at offset 0x8)
    const char **name_ptr_location = (const char **)((char *)qstr_ptr + QSTR_NAME_OFFSET);
    const char *name = *name_ptr_location;
    
    if (!name)
        return;
    
    // Check if this file should be hidden
    if (should_hide_path(name)) {
        pr_info("hidemap: hiding map entry for %s\n", name);
        // Skip the original function by setting skip_origin flag
        args->skip_origin = 1;
        args->ret = 0;  // Return 0 (success) without showing anything
    }
}

/**
 * @brief Initialize the hidemap module
 */
static long hidemap_init(const char *args, const char *event, void *__user reserved)
{
    hook_err_t err;
    
    pr_info("hidemap: initializing module\n");
    pr_info("hidemap: event: %s, args: %s\n", event, args);
    pr_info("hidemap: kernelpatch version: 0x%x\n", kpver);
    
    // Look up the show_map_vma symbol
    unsigned long show_map_vma_addr = 0;
    
    if (kallsyms_lookup_name) {
        show_map_vma_addr = kallsyms_lookup_name("show_map_vma");
    }
    
    if (!show_map_vma_addr) {
        pr_err("hidemap: failed to find show_map_vma symbol\n");
        pr_info("hidemap: trying alternative symbol names...\n");
        
        // Try alternative names for different kernel versions
        if (kallsyms_lookup_name) {
            show_map_vma_addr = kallsyms_lookup_name("show_vma_header_prefix");
        }
    }
    
    if (!show_map_vma_addr) {
        pr_err("hidemap: failed to find any suitable symbol to hook\n");
        pr_err("hidemap: module will be loaded but inactive\n");
        return 0;  // Don't fail the module load
    }
    
    pr_info("hidemap: found show_map_vma at 0x%lx\n", show_map_vma_addr);
    
    // Hook the function using hook_wrap2 (2 arguments: seq_file, vma)
    err = hook_wrap2((void *)show_map_vma_addr, before_show_map_vma, NULL, NULL);
    
    if (err != HOOK_NO_ERR) {
        pr_err("hidemap: failed to hook show_map_vma, error: %d\n", err);
        return -1;
    }
    
    original_show_map_vma = (void *)show_map_vma_addr;
    pr_info("hidemap: successfully hooked show_map_vma\n");
    
    // Parse initial args if provided
    if (args && strlen(args) > 0) {
        strncpy(hide_pattern, args, MAX_HIDE_PATTERN_LEN - 1);
        hide_pattern[MAX_HIDE_PATTERN_LEN - 1] = '\0';
        pattern_enabled = 1;
        pr_info("hidemap: hiding pattern set to: %s\n", hide_pattern);
    }
    
    pr_info("hidemap: initialization complete\n");
    return 0;
}

/**
 * @brief Control function - set hide pattern
 * Usage: 
 *   - "enable <pattern>" - enable hiding with pattern
 *   - "disable" - disable hiding
 *   - "status" - show current status
 */
static long hidemap_control0(const char *args, char *__user out_msg, int outlen)
{
    char response[256] = {0};
    
    pr_info("hidemap: control called with args: %s\n", args);
    
    if (!args) {
        snprintf(response, sizeof(response), "error: no arguments provided");
        goto out;
    }
    
    if (strncmp(args, "enable ", 7) == 0) {
        const char *pattern = args + 7;
        if (strlen(pattern) == 0) {
            snprintf(response, sizeof(response), "error: pattern is empty");
        } else {
            strncpy(hide_pattern, pattern, MAX_HIDE_PATTERN_LEN - 1);
            hide_pattern[MAX_HIDE_PATTERN_LEN - 1] = '\0';
            pattern_enabled = 1;
            snprintf(response, sizeof(response), "ok: hiding enabled for pattern: %s", hide_pattern);
            pr_info("hidemap: enabled hiding for pattern: %s\n", hide_pattern);
        }
    } else if (strncmp(args, "disable", 7) == 0) {
        pattern_enabled = 0;
        snprintf(response, sizeof(response), "ok: hiding disabled");
        pr_info("hidemap: hiding disabled\n");
    } else if (strncmp(args, "status", 6) == 0) {
        if (pattern_enabled && hide_pattern[0]) {
            snprintf(response, sizeof(response), "status: enabled, pattern: %s", hide_pattern);
        } else {
            snprintf(response, sizeof(response), "status: disabled");
        }
    } else if (strncmp(args, "pattern ", 8) == 0) {
        const char *pattern = args + 8;
        strncpy(hide_pattern, pattern, MAX_HIDE_PATTERN_LEN - 1);
        hide_pattern[MAX_HIDE_PATTERN_LEN - 1] = '\0';
        snprintf(response, sizeof(response), "ok: pattern set to: %s (use 'enable' to activate)", hide_pattern);
        pr_info("hidemap: pattern set to: %s\n", hide_pattern);
    } else {
        snprintf(response, sizeof(response), "error: unknown command. Use: enable <pattern>, disable, status, or pattern <str>");
    }
    
out:
    if (out_msg && outlen > 0) {
        compat_copy_to_user(out_msg, response, min((int)sizeof(response), outlen));
    }
    
    return 0;
}

/**
 * @brief Extended control function
 */
static long hidemap_control1(void *a1, void *a2, void *a3)
{
    pr_info("hidemap: control1 called - a1: 0x%llx, a2: 0x%llx, a3: 0x%llx\n", 
            (uint64_t)a1, (uint64_t)a2, (uint64_t)a3);
    return 0;
}

/**
 * @brief Module exit/cleanup
 */
static long hidemap_exit(void *__user reserved)
{
    pr_info("hidemap: exiting module\n");
    
    // Unhook if we hooked successfully
    if (original_show_map_vma) {
        unhook(original_show_map_vma);
        pr_info("hidemap: unhooked show_map_vma\n");
    }
    
    pattern_enabled = 0;
    pr_info("hidemap: module unloaded\n");
    return 0;
}

// Register module callbacks
KPM_INIT(hidemap_init);
KPM_CTL0(hidemap_control0);
KPM_CTL1(hidemap_control1);
KPM_EXIT(hidemap_exit);

