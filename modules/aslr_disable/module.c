/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * aslr_disable - KPM module that disables ASLR by hooking load_elf_binary.
 *
 * Implementation based on:
 *   https://mp.weixin.qq.com/s/N7oaErkBhYaQADxv-DAiNg
 *
 * Strategy:
 *   Primary:   Hook load_elf_binary before-handler and zero randomize_va_space,
 *              so PF_RANDOMIZE is never set for any new process.
 *
 *   Fallback:  Hook arch_mmap_rnd (returns 0) so even if PF_RANDOMIZE is set,
 *              the mmap base gets no random factor.  Combined with hooking
 *              arch_randomize_brk the heap base is also fixed.
 *
 * Finding randomize_va_space when kallsyms omits it:
 *   Scan load_elf_binary's AArch64 instructions for the ADRP+LDR Wt pattern
 *   (READ_ONCE of a 32-bit global) followed by a CBZ/CBNZ conditional branch.
 *   The scan covers up to SCAN_INSNS instructions (load_elf_binary is large on
 *   Android).
 */

#include <compiler.h>
#include <kpmodule.h>
#include <kallsyms.h>
#include <linux/kernel.h>
#include <linux/printk.h>
#include <linux/string.h>
#include <common.h>
#include <kputils.h>
#include <hook.h>
#include <ktypes.h>

KPM_NAME("kpm-aslr-disable");
KPM_VERSION("1.0.2");
KPM_LICENSE("GPL v2");
KPM_AUTHOR("kpm-spore");
KPM_DESCRIPTION("Disable ASLR by hooking load_elf_binary + arch_mmap_rnd");

/* ------------------------------------------------------------------ */
/* State                                                               */
/* ------------------------------------------------------------------ */

static int *randomize_va_space_addr;
static unsigned long load_elf_binary_addr;
static unsigned long arch_mmap_rnd_addr;
static unsigned long arch_randomize_brk_addr;

static int hooked_leb;       /* load_elf_binary */
static int hooked_amr;       /* arch_mmap_rnd   */
static int hooked_arb;       /* arch_randomize_brk */
static int enabled = 1;

/* ------------------------------------------------------------------ */
/* AArch64 instruction scanner for randomize_va_space                  */
/* ------------------------------------------------------------------ */

#define SCAN_INSNS 3000

/*
 * Return the page-aligned ADRP target for instruction `insn` at `pc`.
 * insn must already be verified as an ADRP.
 */
static unsigned long adrp_target(unsigned long pc, unsigned int insn)
{
    long immhi = (long)((insn >> 5) & 0x7ffff);
    long immlo = (long)((insn >> 29) & 0x3);
    long imm21 = (immhi << 2) | immlo;
    imm21 = (imm21 << 43) >> 43; /* sign-extend 21-bit */
    return (pc & ~(unsigned long)0xfff) + (unsigned long)(imm21 << 12);
}

/* Byte offset encoded in LDR Wt, [Xn, #imm12<<2] */
static unsigned long ldr_w_offset(unsigned int insn)
{
    return (unsigned long)((insn >> 10) & 0xfff) << 2;
}

static int *scan_for_randomize_va_space(unsigned long func)
{
    const unsigned int *code = (const unsigned int *)func;
    unsigned long adrp_page[32];
    unsigned char adrp_valid[32];
    int i, rd, rn;

    for (i = 0; i < 32; i++) {
        adrp_page[i] = 0;
        adrp_valid[i] = 0;
    }

    for (i = 0; i < SCAN_INSNS; i++) {
        unsigned int insn = code[i];
        unsigned long pc  = func + (unsigned long)i * 4;

        /* ADRP: bit31=1, bits[28:24]=10000 */
        if ((insn & 0x9f000000) == 0x90000000) {
            rd = (int)(insn & 0x1f);
            adrp_page[rd]  = adrp_target(pc, insn);
            adrp_valid[rd] = 1;
            continue;
        }

        /* LDR Wt, [Xn, #imm12<<2]: bits[31:22] = 1011100101 */
        if ((insn & 0xffc00000) == 0xb9400000) {
            rn = (int)((insn >> 5) & 0x1f);
            if (adrp_valid[rn]) {
                unsigned long addr = adrp_page[rn] + ldr_w_offset(insn);
                int val = *(volatile int *)addr;
                if (val >= 0 && val <= 2) {
                    /* Check next instruction is a conditional branch on same Wt */
                    unsigned int next = code[i + 1];
                    unsigned int rt   = insn & 0x1f;
                    int branched = (((next & 0xff00001f) == (0x34000000u | rt)) ||
                                    ((next & 0xff00001f) == (0x35000000u | rt)));
                    if (branched) {
                        pr_info("aslr_disable: scan found @ 0x%lx = %d (insn[%d], CBZ/CBNZ)\n",
                                addr, val, i);
                        return (int *)addr;
                    }
                    /* Also accept without branch – first match wins */
                    pr_info("aslr_disable: scan candidate @ 0x%lx = %d (insn[%d])\n",
                            addr, val, i);
                    return (int *)addr;
                }
            }
            /* Destination register now unknown */
            adrp_valid[insn & 0x1f] = 0;
            continue;
        }

        /* Any instruction that writes a register (broad conservative check):
         * invalidate if Rd matches an ADRP result we're tracking, EXCEPT
         * for LDP/STP patterns that use X29 frame pointer (Rd != ADRP result). */
        rd = (int)(insn & 0x1f);
        if (rd < 31 && adrp_valid[rd]) {
            /* Not ADRP, not LDR Wt – this register is being overwritten */
            adrp_valid[rd] = 0;
        }
    }

    pr_warn("aslr_disable: scan: not found in %d instructions\n", SCAN_INSNS);
    return NULL;
}

/* ------------------------------------------------------------------ */
/* Hook: load_elf_binary – zero randomize_va_space before the read     */
/* ------------------------------------------------------------------ */

static void before_load_elf_binary(hook_fargs1_t *args, void *udata)
{
    if (!args || !enabled || !randomize_va_space_addr)
        return;
    *randomize_va_space_addr = 0;
}

static void after_load_elf_binary(hook_fargs1_t *args, void *udata) {}

/* ------------------------------------------------------------------ */
/* Hook: arch_mmap_rnd – always return 0                               */
/* ------------------------------------------------------------------ */

static void after_arch_mmap_rnd(hook_fargs0_t *args, void *udata)
{
    if (enabled)
        args->ret = 0;
}

/* ------------------------------------------------------------------ */
/* Hook: arch_randomize_brk – always return 0 random offset            */
/* ------------------------------------------------------------------ */

static void after_arch_randomize_brk(hook_fargs1_t *args, void *udata)
{
    if (enabled)
        args->ret = 0;
}

/* ------------------------------------------------------------------ */
/* Module lifecycle                                                     */
/* ------------------------------------------------------------------ */

static long aslr_disable_init(const char *args, const char *event, void *__user reserved)
{
    hook_err_t err;

    pr_info("aslr_disable: init event=%s args=%s\n",
            event ? event : "(null)",
            args  ? args  : "(null)");

    /* ---- 1. Locate randomize_va_space ---- */
    randomize_va_space_addr = (int *)kallsyms_lookup_name("randomize_va_space");
    if (randomize_va_space_addr) {
        pr_info("aslr_disable: randomize_va_space via kallsyms @ %p = %d\n",
                randomize_va_space_addr, *randomize_va_space_addr);
    }

    /* ---- 2. Locate and hook load_elf_binary ---- */
    load_elf_binary_addr = kallsyms_lookup_name("load_elf_binary");
    if (!load_elf_binary_addr)
        load_elf_binary_addr = kallsyms_lookup_name("load_elf_binary.cfi_jt");

    if (!load_elf_binary_addr) {
        pr_err("aslr_disable: load_elf_binary not found\n");
        return -1;
    }
    pr_info("aslr_disable: load_elf_binary @ 0x%lx\n", load_elf_binary_addr);

    /* Scan for randomize_va_space if kallsyms missed it */
    if (!randomize_va_space_addr) {
        pr_info("aslr_disable: scanning %d instructions for randomize_va_space\n",
                SCAN_INSNS);
        randomize_va_space_addr = scan_for_randomize_va_space(load_elf_binary_addr);
        if (randomize_va_space_addr)
            pr_info("aslr_disable: scan found @ %p = %d\n",
                    randomize_va_space_addr, *randomize_va_space_addr);
        else
            pr_warn("aslr_disable: randomize_va_space not found; relying on fallback hooks\n");
    }

    err = hook_wrap1((void *)load_elf_binary_addr,
                     before_load_elf_binary, after_load_elf_binary, NULL);
    if (err != HOOK_NO_ERR) {
        pr_err("aslr_disable: hook load_elf_binary failed: %d\n", (int)err);
        return -1;
    }
    hooked_leb = 1;

    /* ---- 3. Fallback: hook arch_mmap_rnd ---- */
    arch_mmap_rnd_addr = kallsyms_lookup_name("arch_mmap_rnd");
    if (arch_mmap_rnd_addr) {
        pr_info("aslr_disable: arch_mmap_rnd @ 0x%lx\n", arch_mmap_rnd_addr);
        err = hook_wrap0((void *)arch_mmap_rnd_addr,
                         NULL, after_arch_mmap_rnd, NULL);
        if (err == HOOK_NO_ERR)
            hooked_amr = 1;
        else
            pr_warn("aslr_disable: hook arch_mmap_rnd failed: %d\n", (int)err);
    } else {
        pr_warn("aslr_disable: arch_mmap_rnd not in kallsyms\n");
    }

    /* ---- 4. Fallback: hook arch_randomize_brk ---- */
    arch_randomize_brk_addr = kallsyms_lookup_name("arch_randomize_brk");
    if (arch_randomize_brk_addr) {
        pr_info("aslr_disable: arch_randomize_brk @ 0x%lx\n", arch_randomize_brk_addr);
        err = hook_wrap1((void *)arch_randomize_brk_addr,
                         NULL, after_arch_randomize_brk, NULL);
        if (err == HOOK_NO_ERR)
            hooked_arb = 1;
        else
            pr_warn("aslr_disable: hook arch_randomize_brk failed: %d\n", (int)err);
    } else {
        pr_warn("aslr_disable: arch_randomize_brk not in kallsyms\n");
    }

    pr_info("aslr_disable: ready. hooked_leb=%d hooked_amr=%d hooked_arb=%d rvs=%p\n",
            hooked_leb, hooked_amr, hooked_arb, randomize_va_space_addr);
    return 0;
}

static long aslr_disable_control0(const char *args, char *__user out_msg, int outlen)
{
    char response[320];
    int val = -1;
    int len;

    if (!args || args[0] == '\0' || strncmp(args, "status", 6) == 0) {
        if (randomize_va_space_addr)
            val = *randomize_va_space_addr;
        snprintf(response, sizeof(response),
                 "enabled=%d rvs_addr=%p rvs=%d "
                 "leb=%d amr=%d arb=%d",
                 enabled, randomize_va_space_addr, val,
                 hooked_leb, hooked_amr, hooked_arb);

    } else if (strncmp(args, "enable", 6) == 0) {
        enabled = 1;
        snprintf(response, sizeof(response), "ok: enabled");

    } else if (strncmp(args, "disable", 7) == 0) {
        enabled = 0;
        snprintf(response, sizeof(response), "ok: disabled (hooks stay)");

    } else {
        snprintf(response, sizeof(response), "usage: status | enable | disable");
    }

    if (out_msg && outlen > 0) {
        len = strlen(response) + 1;
        compat_copy_to_user(out_msg, response, len < outlen ? len : outlen);
    }
    return 0;
}

static long aslr_disable_exit(void *__user reserved)
{
    if (hooked_arb) {
        hook_unwrap((void *)arch_randomize_brk_addr, NULL, after_arch_randomize_brk);
        hooked_arb = 0;
    }
    if (hooked_amr) {
        hook_unwrap((void *)arch_mmap_rnd_addr, NULL, after_arch_mmap_rnd);
        hooked_amr = 0;
    }
    if (hooked_leb) {
        hook_unwrap((void *)load_elf_binary_addr,
                    before_load_elf_binary, after_load_elf_binary);
        hooked_leb = 0;
    }
    pr_info("aslr_disable: exit\n");
    return 0;
}

KPM_INIT(aslr_disable_init);
KPM_CTL0(aslr_disable_control0);
KPM_EXIT(aslr_disable_exit);
