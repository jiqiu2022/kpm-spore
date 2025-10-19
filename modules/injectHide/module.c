#include <compiler.h>
#include <kpmodule.h>
#include <linux/printk.h>
#include <linux/err.h>
#include "./proc/maps.h"
#include "./utils/config.h"
#include "symbols.h"

KPM_NAME("hideInject");
KPM_VERSION("0.0.1");
KPM_AUTHOR("yuuki");
KPM_DESCRIPTION("clean up some traces of user injection");

static long mod_init(const char *args, const char *event, void *__user reserved){
    pr_info("[yuuki] Initializing...\n");
    if(init_symbols()) {
        pr_info("[yuuki] init_symbols failed\n");
        goto exit;
    }

    if(init_config()) {
        pr_info("[yuuki] init_config failed\n");
        goto exit;
    }

    add_maps_hooks();

    exit:
    return 0;
}

static long mod_control0(const char *args, char *__user out_msg, int outlen) {
    pr_info("[yuuki] kpm hello control0, args: %s\n", args);

    return 0;
}

static long mod_exit(void *__user reserved) {
    pr_info("[yuuki] mod_exit, uninstalled hook.\n");
    remove_maps_hooks();
    free_config();
    return 0;
}

KPM_INIT(mod_init);
KPM_CTL0(mod_control0);
KPM_EXIT(mod_exit);