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
#include "cfg_mysql_host.h"

#include "error_facility.h"

//------------------------------------------------------------------------------------------------------------
int interpret_cfg_data_mysql_Host(strValues *val, strOptionsMySQL *mysql) {
  int rc;

  if (val->opt_count>1 || val->next) {
    err_debug(0, "Bad host specification at \"%s\".", val->val);
    return 6;
  }

  rc = interpret_cfg_data_string(val->val, &mysql->host);
  if(rc)
    goto error;

  mysql->port = 0;
  if(val->opt_count) {
    rc = interpret_cfg_data_port(val->opt[0], &mysql->port);
    if(rc)
      goto error;
  }

  return 0;
error:
  free_all_options_mysql_Host(mysql);
  return rc;
}
//------------------------------------------------------------------------------------------------------------
void free_all_options_mysql_Host(strOptionsMySQL *mysql) {
  if (mysql->host)
    free(mysql->host);
  mysql->host = NULL;
  mysql->port = 0;
  return;
}
//------------------------------------------------------------------------------------------------------------
void dump_cfg_data_mysql_Host(FILE *fd, strOptionsMySQL *mysql) {
  if(!fd)
    return;

  if(mysql->host) {
    if(mysql->port)
      fprintf(fd, "\thost = %s:%d\n", mysql->host, mysql->port);
    else
      fprintf(fd, "\thost = %s\n", mysql->host);
  }
  else {
    fprintf(fd, "#\thost =\n");
  }

  return;
}
//------------------------------------------------------------------------------------------------------------
