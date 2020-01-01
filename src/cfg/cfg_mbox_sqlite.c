#include <stdio.h>
#include "cfg.h"

int interpret_cfg_data_SqliteEnable(strValues *val, strOptionsRealm *opts) {
  if (val->opt_count)
    return 6;
  return interpret_cfg_data_bool(val->val, &opts->sqlite_enable);
}

void dump_cfg_data_SqliteEnable(FILE *fd, strOptionsRealm *opts) {
  if(!fd)
    return;
  fprintf(fd,"mbox_cache = %d\n",opts->sqlite_enable);
}

int add_realm_SqliteEnable(strOptionsRealm *dst, strOptionsRealm *src) {
  if ( !dst ) return 0;

  dst->sqlite_enable = src? src->sqlite_enable: 0;
  return 1;
}
