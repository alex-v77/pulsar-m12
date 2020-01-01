#pragma once

int  interpret_cfg_data_SqliteEnable(strValues *val, strOptionsRealm *opts);
void dump_cfg_data_SqliteEnable(FILE *fd, strOptionsRealm *opts);
int add_realm_SqliteEnable(strOptionsRealm *dst, strOptionsRealm *src);
