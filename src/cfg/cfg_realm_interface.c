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
#include "cfg_realm_interface.h"

#include "util.h"
#include "error_facility.h"

//------------------------------------------------------------------------------------------------------------
int interpret_cfg_data_RealmInterface(strValues *val, strOptionsRealm  *realm) {
  int        i;
  int        rc;
  int        count;
  strValues *tmp;

  tmp = val;
  count = 0;
  while(tmp) {
    if (tmp->opt_count>1) {
      err_debug(0,"Bad interface specification at \"%s\".",tmp->val);
      return 6;
    }
    count++;
    tmp = tmp->next;
  }

  i = realm->ifaces.count;
  realm->ifaces.count += count;

  realm->ifaces.sa = realloc(realm->ifaces.sa, count * sizeof(realm->ifaces.sa[0]));
  if(!realm->ifaces.sa) {
    rc = 2;
    goto error;
  }
  realm->ifaces.type = realloc(realm->ifaces.type, count * sizeof(realm->ifaces.type[0]));
  if(!realm->ifaces.type) {
    rc = 2;
    goto error;
  }

  // record every interface
  tmp = val;
  for(; tmp; i++, tmp = tmp->next) {
    rc = cfg_interface_get(&realm->ifaces.sa[i], tmp, 0x00);
    if(rc)
      goto error;
    //realm->ifaces.type[i] = 0; // calloc makes it 0 no need for this
  } // ~for

  // check for duplicates
  rc = cfg_interface_check(realm->ifaces.sa, realm->ifaces.count);
  if(rc)
    goto error;

  return 0;
error:
  free_all_options_RealmInterface(realm);
  return rc;
}
//------------------------------------------------------------------------------------------------------------
void free_all_options_RealmInterface(strOptionsRealm  *realm) {
  if(realm->ifaces.sa)
    free(realm->ifaces.sa);
  if(realm->ifaces.type)
    free(realm->ifaces.type);
  realm->ifaces.sa = NULL;
  realm->ifaces.type = NULL;
  realm->ifaces.count = 0;
  return;
}
//------------------------------------------------------------------------------------------------------------
void dump_cfg_data_RealmInterface(FILE *fd, const char *fill, strOptionsRealm  *realm) {
  char *buf;
  int   i;

  if(!fd || !fill)
    return;

  if(!realm->ifaces.sa) {
    fprintf(fd, "#%srealm_interface =\n", fill);
    return;
  }

  for(i=0; i<realm->ifaces.count; i++) {
    buf = cfg_interface_print(&(realm->ifaces.sa[i]));
    if(!buf) {
      continue;
    }
    if(!i) {
      fprintf(fd,"%srealm_interface = %s", fill, buf);
      continue;
    }
    fprintf(fd,",\n%s\t%s", fill, buf);
  }

  fprintf(fd,"\n");
  return;
}
//------------------------------------------------------------------------------------------------------------
int  add_realm_RealmInterface(strOptionsRealm *dst, strOptionsRealm *src) {
  if(!dst)
    return 0;

  if(!src) // creation of default realm
    return 1; // nothing to do !! We can't just make up some valid values for this!

  if(!src->ifaces.sa || !src->ifaces.type || !src->ifaces.count)
    return 1;

  if(dst->ifaces.sa || dst->ifaces.type) // out with the old one
    free_all_options_RealmInterface(dst);

  dst->ifaces.count = src->ifaces.count;
  dst->ifaces.sa = safe_malloc(dst->ifaces.count * sizeof(dst->ifaces.sa[0]));
  if(!dst->ifaces.sa)
    goto malloc_error;
  dst->ifaces.type = safe_malloc(dst->ifaces.count * sizeof(dst->ifaces.type[0]));
  if(!dst->ifaces.type)
    goto malloc_error;

  memcpy(dst->ifaces.sa,   src->ifaces.sa,   dst->ifaces.count * sizeof(dst->ifaces.sa[0]  ));
  memcpy(dst->ifaces.type, src->ifaces.type, dst->ifaces.count * sizeof(dst->ifaces.type[0]));
  return 1;

malloc_error:
  free_all_options_RealmInterface(dst);
  return 0;
}
//------------------------------------------------------------------------------------------------------------
