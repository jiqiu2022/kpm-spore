#ifndef KPM_BUILD_ANYWHERE_MAPS_H
#define KPM_BUILD_ANYWHERE_MAPS_H
#include "../symbols.h"
#include "asm/current.h"

long add_maps_hooks();
long remove_maps_hooks();

struct seq_file {
    char *buf;
    size_t size;
    size_t from;
    size_t count;
    //...
};

inline bool is_proc_eff(){
    if(likely(kf_get_task_mm))
        if(current && !(kf_get_task_mm(current)))
            return false;

    return true;
}

#endif
