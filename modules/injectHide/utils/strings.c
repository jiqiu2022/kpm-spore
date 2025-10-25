#include "strings.h"
#include "linux/string.h"

int parse_config_line(const char *line, struct config *cfg) {
    int len = strlen(line);

    if (len == 0) {
        return -1;
    }

    cfg->keyword = kf_vmalloc(len + 1);
    if (!cfg->keyword) {
        pr_err("[yuuki] alloc keyword failed\n");
        return -1;
    }

    strcpy(cfg->keyword, line);

    return 0;
}
