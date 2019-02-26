/*
 * Copyright (C) 2001 Rok Papez <rok.papez@lugos.si>
 * Rok Papez
 * Hribovska pot 17
 * 1231 Ljubljana - Crnuce
 * EUROPE, Slovenia
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
 *
 */

#ifdef   HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "error_facility.h"
#include "pulsar.h"
#include "users.h"

//------------------------------------------------------------------------------------------------------------
int pulsar_user() {
  int i = 0;

  err_debug_function();

  if(defAUTH != g.state)
    return defBadcmd;

  if(g.realm) {
    if( ! (g.realm->auth_cmd & defAuthCmdUser)) {
      err_debug(5, "USER command not configured!");
      err_debug_return(defBadcmd);
    }
  }

  if(!g.pop3_arg[0])
    return defParam;

  // if previous data exists remove it
  if(g.user)
    free(g.user);
  g.user = NULL;
  if(g.realm_name)
    free(g.realm_name);
  g.realm_name = NULL;

  // locate realm in a username
  if(g.cfg->realm_chars)
    i = strcspn(g.pop3_arg, g.cfg->realm_chars);
  else
    i = strlen(g.pop3_arg);

  if('\0' != g.pop3_arg[i]) { // have we found a realm name ?
    g.realm_name = strdup(&(g.pop3_arg[i+1]));
    if(!g.realm_name) {
        err_malloc_error();
        return defMalloc;
    }
  } else {
      g.realm_name = NULL;
  }
  g.pop3_arg[i] = '\0';

  // copy username
  g.user = strdup(g.pop3_arg);
  if(!g.user) {
    err_malloc_error();
    return defMalloc;
  }

  return defOK;
}
//------------------------------------------------------------------------------------------------------------
