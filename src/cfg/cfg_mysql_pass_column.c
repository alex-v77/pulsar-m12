/*
 * Copyright (C) 2001 Rok Papez <rok.papez@lugos.si>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "cfg.h"
#include "cfg_mysql_pass_column.h"

#include "error_facility.h"

//------------------------------------------------------------------------------------------------------------
int interpret_cfg_data_mysql_PassColumn(strValues *val, strOptionsMySQL *mysql) {
  int rc;
  int i;

  if (val->opt_count > defPassHashMax || val->next)
    goto error;

  rc = interpret_cfg_data_string(val->val, &mysql->pass_column);
  if(rc)
    return rc;

  for(i=0; i<val->opt_count; i++) {
    if(!strcmp(defPassHashPlainStr, val->opt[i]))
      mysql->pass_hash.hash[i] = defPassHashPlain;
    else if(!strcmp(defPassHashCryptStr, val->opt[i]))
      mysql->pass_hash.hash[i] = defPassHashCrypt;
    else
      goto error;
  }
  mysql->pass_hash.count = val->opt_count;

  // default to plaintext authentication when not specified
  if(!mysql->pass_hash.count) {
    mysql->pass_hash.count = 1;
    mysql->pass_hash.hash[0] = defPassHashPlain;
  }

  return 0;
error:
  err_debug(0, "Bad pass_column specification at \"%s\".", val->val);
  return 6;
}
//------------------------------------------------------------------------------------------------------------
void free_all_options_mysql_PassColumn(strOptionsMySQL *mysql) {
  free(mysql->pass_column);
  mysql->pass_column = NULL;
  mysql->pass_hash.count = 0;
  return;
}
//------------------------------------------------------------------------------------------------------------
void dump_cfg_data_mysql_PassColumn(FILE *fd, strOptionsMySQL *mysql) {
  int i;

  if(!fd)
    return;

  if(!mysql->pass_column) {
    fprintf(fd, "#\tpass_column =\n");
    return;
  }

  fprintf(fd, "\tpass_column = %s", mysql->pass_column);
  for(i=0; i<mysql->pass_hash.count; i++) {
    switch(mysql->pass_hash.hash[i]) {
    case defPassHashPlain:
        fprintf(fd, ":%s", defPassHashPlainStr);
        break;
    case defPassHashCrypt:
        fprintf(fd, ":%s", defPassHashCryptStr);
        break;
    default:
        fprintf(fd, ":<error - invalid value>");
    }
  } // ~for
  fprintf(fd, "\n");

  return;
}
//------------------------------------------------------------------------------------------------------------
