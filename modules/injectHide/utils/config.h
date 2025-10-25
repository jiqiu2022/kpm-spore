#ifndef KPM_BUILD_ANYWHERE_CONFIG_H
#define KPM_BUILD_ANYWHERE_CONFIG_H

#include "../symbols.h"

#define MAX_CONFIGS 256
#define MAX_LINE_LEN 512

struct config_manager {
    struct config *configs;
    int count;
    int capacity;
};

int init_config(void);
struct config *get_config(int index);
const char *get_config_content(int index);
int set_config(const char *config_content);
void free_config(void);
int get_config_count(void);

#endif
