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
#include "cfg_mysql_uid.h"

#include "error_facility.h"

//------------------------------------------------------------------------------------------------------------
int interpret_cfg_data_mysql_Uid(strValues *val, strOptionsMySQL *mysql) {
  if (val->opt_count || val->next) {
    err_debug(0, "Bad uid specification at \"%s\".", val->val);
    return 6;
  }

  return interpret_cfg_data_string(val->val, &mysql->uid);
}
//------------------------------------------------------------------------------------------------------------
void free_all_options_mysql_Uid(strOptionsMySQL *mysql) {
  if (mysql->uid)
    free(mysql->uid);
  mysql->uid = NULL;
  return;
}
//------------------------------------------------------------------------------------------------------------
void dump_cfg_data_mysql_Uid(FILE *fd, strOptionsMySQL *mysql) {
  if(!fd)
    return;

  if(mysql->uid)
    fprintf(fd, "\tuid = %s\n", mysql->uid);
  else
    fprintf(fd, "#\tuid =\n");
  return;
}
//------------------------------------------------------------------------------------------------------------
