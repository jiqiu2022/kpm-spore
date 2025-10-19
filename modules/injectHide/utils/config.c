#include "config.h"
#include "fopt.h"
#include "linux/string.h"
#include "strings.h"

static struct config_manager *manager = NULL;
int init_config(void) {
    loff_t len = 0;
    char *file_content = NULL;
    char *line_start = NULL;
    char *line_end = NULL;
    int line_len = 0;
    char line_buf[MAX_LINE_LEN];

    file_content = kernel_read_file(CONFIG_PATH, &len);
    if (!file_content) {
        pr_err("[yuuki] read config failed\n");
        return -1;
    }

    manager = kf_vmalloc(sizeof(struct config_manager));
    if (!manager) {
        pr_err("[yuuki] alloc config manager failed\n");
        kf_vfree(file_content);
        return -1;
    }

    manager->capacity = MAX_CONFIGS;
    manager->count = 0;
    manager->configs = kf_vmalloc(sizeof(struct config) * MAX_CONFIGS);
    if (!manager->configs) {
        pr_err("[yuuki] alloc configs array failed\n");
        kf_vfree(manager);
        kf_vfree(file_content);
        manager = NULL;
        return -1;
    }

    memset(manager->configs, 0, sizeof(struct config) * MAX_CONFIGS);

    line_start = file_content;
    while (line_start - file_content < len) {
        line_end = line_start;
        while (line_end - file_content < len && *line_end != '\n' && *line_end != '\r') {
            line_end++;
        }

        line_len = line_end - line_start;

        if (line_len > 0 && line_len < MAX_LINE_LEN) {
            memcpy(line_buf, line_start, line_len);
            line_buf[line_len] = '\0';

            if (parse_config_line(line_buf, &manager->configs[manager->count]) == 0) {
                manager->count++;

                if (manager->count >= MAX_CONFIGS) {
                    pr_warn("[yuuki] max configs limit reached\n");
                    break;
                }
            }
        }

        line_start = line_end;
        while (line_start - file_content < len &&
               (*line_start == '\n' || *line_start == '\r')) {
            line_start++;
        }
    }

    kf_vfree(file_content);
    pr_info("[yuuki] config initialized, %d items loaded\n", manager->count);

    return 0;
}

struct config *get_config(int index) {
    if (!manager) {
        pr_err("[yuuki] config not initialized\n");
        return NULL;
    }

    if (index < 0 || index >= manager->count) {
        pr_err("[yuuki] invalid config index: %d\n", index);
        return NULL;
    }

    return &manager->configs[index];
}

const char *get_config_content(int index) {
    struct config *cfg = get_config(index);
    if (!cfg) {
        return NULL;
    }
    return cfg->keyword;
}

int set_config(const char *config_content) {
    if (!manager) {
        pr_err("[yuuki] config not initialized\n");
        return -1;
    }

    if (manager->count >= manager->capacity) {
        pr_err("[yuuki] config is full\n");
        return -1;
    }

    if (!config_content) {
        pr_err("[yuuki] config_content is NULL\n");
        return -1;
    }

    if (parse_config_line(config_content, &manager->configs[manager->count]) != 0) {
        pr_err("[yuuki] parse config line failed\n");
        return -1;
    }

    pr_info("[yuuki] config[%d] added: %s\n", manager->count, config_content);

    return manager->count++;
}

int get_config_count(void) {
    if (!manager) {
        return 0;
    }
    return manager->count;
}

void free_config(void) {
    int i;

    if (!manager) {
        return;
    }

    if (manager->configs) {
        for (i = 0; i < manager->count; i++) {
            if (manager->configs[i].keyword) {
                kf_vfree(manager->configs[i].keyword);
            }
        }
        kf_vfree(manager->configs);
    }

    kf_vfree(manager);
    manager = NULL;

    pr_info("[yuuki] config freed\n");
}