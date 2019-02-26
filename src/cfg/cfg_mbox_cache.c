#include <stdio.h>
#include "cfg.h"

int interpret_cfg_data_MboxCache(strValues *val, strOptionsGlobal *opts) {
  if (val->opt_count)
    return 6;
  return interpret_cfg_data_bool(val->val, &opts->mbox_cache_enable);
}

void dump_cfg_data_MboxCache(FILE *fd, strOptionsGlobal *opts) {
  if(!fd)
    return;
  fprintf(fd,"mbox_cache = %d\n",opts->mbox_cache_enable);
}
