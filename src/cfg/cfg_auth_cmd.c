/*
 * Copyright (C) 2003 Rok Papez <rok.papez@lugos.si>
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
#include "util.h"
#include "cfg_auth_cmd.h"
//------------------------------------------------------------------------------------------------------------
int interpret_cfg_data_AuthCmd(strValues *val, strOptionsRealm  *realm) {
  realm->auth_cmd = 0; // purge any previous values

  for(; val; val = val->next) {
    if(val->opt_count) // no option takes a parameter
      return 6;

    if(!strcasecmp(val->val, defAuthCmdUserStr)) { // USER command authentication
      realm->auth_cmd |= defAuthCmdUser;
      continue;
    }

    if(!strcasecmp(val->val, defAuthCmdApopStr)) { // APOP command authentication
      realm->auth_cmd |= defAuthCmdApop;
      continue;
    }

    return 6;
  } // ~for

  if(0 == realm->auth_cmd)
    return 6;
  return 0;
}
//------------------------------------------------------------------------------------------------------------
void dump_cfg_data_AuthCmd(FILE *fd, const char *fill, strOptionsRealm  *realm) {
  int is_next = 0; // should ',' be printed ?

  if(!fd || !fill)
    return;

  fprintf(fd,"%sauth_cmd = ", fill);

  if(realm->auth_cmd & defAuthCmdUser) {
    if(is_next)
      fprintf(fd, ", ");
    is_next = 1;
    fprintf(fd, "%s", defAuthCmdUserStr);
  }

  if(realm->auth_cmd & defAuthCmdApop) {
    if(is_next)
      fprintf(fd, ", ");
    is_next = 1;
    fprintf(fd, "%s", defAuthCmdApopStr);
  }

  fprintf(fd, "\n");
  return;
}
//------------------------------------------------------------------------------------------------------------
