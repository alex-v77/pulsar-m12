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
#include "cfg_mailspool.h"
#include "error_facility.h"

//------------------------------------------------------------------------------------------------------------
int interpret_cfg_data_Mailspool(strValues *val, strOptionsRealm  *realm) {
  char *tmp;
  int   i = 0;
  int   rc = 7;

  if (val->opt_count>1 || val->next) {
    err_debug(0, "Bad mailspool specification at \"%s\".", val->val);
    rc = 6;
    goto error;
  }

  realm->mailspool.type = 0x00;
  free(realm->mailspool.dir); // out with the old one
  if('.' == val->val[0]) { // is it homedir ?
    i = 1;
    realm->mailspool.type |= defMailspoolHome;
  }
  realm->mailspool.dir = strdup(&val->val[i]); // in with a new one
  if(!realm->mailspool.dir) {
    rc = 2;
    goto error;
  }

  i = strlen(realm->mailspool.dir);
  if('/' == (realm->mailspool.dir)[i-1]) {
    realm->mailspool.type |= defMailspoolMaildir;
    (realm->mailspool.dir)[i-1] = '\0';
  }

  if(!val->opt_count) {
    if(realm->mailspool.type & defMailspoolHome)
      realm->mailspool.mode = 0600; // homedir delivery
    else
      realm->mailspool.mode = 0660; // mailspool delivery
    return 0;
  }

  realm->mailspool.mode = strtol(val->opt[0], &tmp, 0);
  if(!tmp[0])
    return 0;

  err_debug(0, "Bad mailspool permissions at \"%s\".", val->opt[0]);
  rc = 6;

error:
  free_all_options_Mailspool(realm);
  return rc;
}
//------------------------------------------------------------------------------------------------------------
void free_all_options_Mailspool(strOptionsRealm  *realm) {
  free(realm->mailspool.dir);
  memset(&realm->mailspool, 0x00, sizeof(realm->mailspool));
  return;
}
//------------------------------------------------------------------------------------------------------------
void dump_cfg_data_Mailspool(FILE *fd, const char *fill, strOptionsRealm  *realm) {
  if(!fd || !fill || !realm)
    return;

  if(!realm->mailspool.dir) { // this is actualy dead code... see below.
    fprintf(fd, "#%smailspool =\n",fill);
    return;
  }

  fprintf(fd,"%smailspool = ",fill);
  if(realm->mailspool.type && defMailspoolHome)
    fprintf(fd,".");
  fprintf(fd,"%s",realm->mailspool.dir);
  if(realm->mailspool.type && defMailspoolMaildir)
    fprintf(fd,"/");
  fprintf(fd, ":%#o\n", realm->mailspool.mode);

  return;
}
//------------------------------------------------------------------------------------------------------------
int add_realm_Mailspool(strOptionsRealm *dst, strOptionsRealm *src) {
  if(!dst)
    return 0;

  if(!src) { // creation of default realm.
    dst->mailspool.type = 0x00;
    dst->mailspool.mode = 0660; // default to mailspool delivery
    dst->mailspool.dir = strdup(defMailspool);
    if(!dst->mailspool.dir)
      return 0;
    return 1;
  }

  free(dst->mailspool.dir);

  dst->mailspool.type = src->mailspool.type;
  dst->mailspool.mode = src->mailspool.mode;
  dst->mailspool.dir = strdup(src->mailspool.dir);
  if(!dst->mailspool.dir)
    return 0;

  return 1;
}
//------------------------------------------------------------------------------------------------------------
