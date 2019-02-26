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
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <syslog.h>
#include <netdb.h>
#include <string.h>
#include <sysexits.h>

#include "cfg.h"
#include "cfg_listen_on.h"
#include "error_facility.h"

//------------------------------------------------------------------------------------------------------------
int interpret_cfg_data_ListenOn(strValues *val, strOptionsGlobal *opts, int type) {
  strValues *tmp = val;
  int count = 0;
  int rc = 0;
  int i;

  while(tmp) {
    if (tmp->opt_count>1) {
      err_debug(0,"Bad interface specification at \"%s\".",tmp->val);
      return 6;
    }
    count++;
    tmp = tmp->next;
  }

  // add to the interface list
  i = opts->ifaces.count;
  opts->ifaces.count += count;
  opts->ifaces.sa = realloc(opts->ifaces.sa, opts->ifaces.count * sizeof(opts->ifaces.sa[0]));
  if(!opts->ifaces.sa) {
    rc = 2;
    goto error;
  }
  opts->ifaces.type = realloc(opts->ifaces.type, opts->ifaces.count * sizeof(opts->ifaces.type[0]));
  if(!opts->ifaces.type) {
    rc = 2;
    goto error;
  }

  tmp = val;
  for(; tmp; i++, tmp = tmp->next) {
    rc = cfg_interface_get(&opts->ifaces.sa[i], tmp, type);
    if(rc)
      goto error;
    opts->ifaces.type[i] = type;
  }

  rc = cfg_interface_check(opts->ifaces.sa, opts->ifaces.count);
  if(rc)
    goto error;

  return 0;

error:
  /*
  if(host)
  freehostent(host);
  */
  free_all_options_ListenOn(opts);
  return rc;
}
//------------------------------------------------------------------------------------------------------------
void free_all_options_ListenOn(strOptionsGlobal *opts) {
  if(opts->ifaces.sa)
    free(opts->ifaces.sa);
  if(opts->ifaces.type)
    free(opts->ifaces.type);
  opts->ifaces.sa = NULL;
  opts->ifaces.type = NULL;
  opts->ifaces.count = 0;
  return;
}
//------------------------------------------------------------------------------------------------------------
void dump_cfg_data_ListenOn(FILE *fd, strOptionsGlobal *opts) {
  char *buf;
  int   i;
  int   plain_printed = 0;
  int   ssl_printed = 0;

  if(!fd)
    return;

  if(!opts->ifaces.sa) {
    fprintf(fd,"#listen_on =\n");
    fprintf(fd,"#ssl_listen_on =\n");
    return;
  }

  // print listen_on
  for(i=0; i<opts->ifaces.count; i++) {
    if(opts->ifaces.type[i])
      continue;
    buf = cfg_interface_print(&(opts->ifaces.sa[i]));
    if(!buf) {
      continue;
    }
    if(!plain_printed) {
      plain_printed = 1;
      fprintf(fd,"listen_on = %s", buf);
      continue;
    }
    fprintf(fd,",\n\t%s", buf);
  }
  if(plain_printed)
    fprintf(fd,"\n");

  // print ssl_listen_on
  for(i=0; i<opts->ifaces.count; i++) {
    if(!opts->ifaces.type[i])
      continue;
    buf = cfg_interface_print(&(opts->ifaces.sa[i]));
    if(!buf) {
      continue;
    }
    if(!ssl_printed) {
      ssl_printed = 1;
      fprintf(fd,"ss_listen_on = %s", buf);
      continue;
    }
    fprintf(fd,",\n\t%s", buf);
  }
  if(ssl_printed)
    fprintf(fd,"\n");

  // is some interface wasn't printed
  if(!plain_printed)
    fprintf(fd,"#listen_on =\n");
  if(!ssl_printed)
    fprintf(fd,"#ssl_listen_on =\n");

  return;
}
//------------------------------------------------------------------------------------------------------------
