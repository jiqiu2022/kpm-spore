/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * ASLR diagnostic module.
 * Source note: https://mp.weixin.qq.com/s/N7oaErkBhYaQADxv-DAiNg
 *
 * This module intentionally reports ASLR-related kernel state without
 * modifying randomization behavior.
 */

#include <compiler.h>
#include <kpmodule.h>
#include <kallsyms.h>
#include <linux/kernel.h>
#include <linux/printk.h>
#include <linux/string.h>
#include <common.h>
#include <kputils.h>

KPM_NAME("kpm-aslr-report");
KPM_VERSION("1.0.0");
KPM_LICENSE("GPL v2");
KPM_AUTHOR("OpenAI");
KPM_DESCRIPTION("Inspect ASLR-related kernel symbols without changing policy");

#define ASLR_REPORT_BUF_LEN 256

static const char k_source_url[] = "https://mp.weixin.qq.com/s/N7oaErkBhYaQADxv-DAiNg";
static int *randomize_va_space_addr;
static unsigned long load_elf_binary_addr;
static unsigned int refresh_count;

static void refresh_symbol_state(void)
{
    refresh_count++;

    if (!kallsyms_lookup_name) {
        return;
    }

    if (!randomize_va_space_addr) {
        randomize_va_space_addr = (int *)kallsyms_lookup_name("randomize_va_space");
    }

    if (!load_elf_binary_addr) {
        load_elf_binary_addr = kallsyms_lookup_name("load_elf_binary");
    }

    if (!load_elf_binary_addr) {
        load_elf_binary_addr = kallsyms_lookup_name("load_elf_binary.cfi_jt");
    }
}

static void write_status(char *buf, int buf_len)
{
    int randomize_va_space = -1;

    if (!buf || buf_len <= 0) {
        return;
    }

    if (randomize_va_space_addr) {
        randomize_va_space = *randomize_va_space_addr;
    }

    snprintf(buf, buf_len,
             "randomize_va_space=%d load_elf_binary=0x%lx refresh_count=%u",
             randomize_va_space, load_elf_binary_addr, refresh_count);
}

static void copy_response(char *__user out_msg, int outlen, const char *response)
{
    int response_len;
    int copy_len;

    if (!out_msg || outlen <= 0 || !response) {
        return;
    }

    response_len = strlen(response) + 1;
    copy_len = response_len < outlen ? response_len : outlen;
    compat_copy_to_user(out_msg, response, copy_len);
}

static long aslr_report_init(const char *args, const char *event, void *__user reserved)
{
    char status[ASLR_REPORT_BUF_LEN];

    refresh_symbol_state();
    write_status(status, sizeof(status));

    pr_info("aslr_report: init event=%s args=%s\n",
            event ? event : "(null)",
            args ? args : "(null)");
    pr_info("aslr_report: %s\n", status);

    return 0;
}

static long aslr_report_control0(const char *args, char *__user out_msg, int outlen)
{
    char response[ASLR_REPORT_BUF_LEN];

    if (!args || args[0] == '\0' || strncmp(args, "status", 6) == 0) {
        write_status(response, sizeof(response));
    } else if (strncmp(args, "refresh", 7) == 0) {
        refresh_symbol_state();
        write_status(response, sizeof(response));
    } else if (strncmp(args, "source", 6) == 0) {
        snprintf(response, sizeof(response), "%s", k_source_url);
    } else {
        snprintf(response, sizeof(response),
                 "usage: status | refresh | source");
    }

    copy_response(out_msg, outlen, response);
    return 0;
}

static long aslr_report_exit(void *__user reserved)
{
    pr_info("aslr_report: exit\n");
    return 0;
}

KPM_INIT(aslr_report_init);
KPM_CTL0(aslr_report_control0);
KPM_EXIT(aslr_report_exit);
