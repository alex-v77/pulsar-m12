/*
 * Copyright (C) 2003 Rok Papez <rok.papez@lugos.si>
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
#include <ctype.h>

#include "error_facility.h"
#include "pulsar.h"
#include "md5.h"

//------------------------------------------------------------------------------------------------------------
int pulsar_apop() {
  char *md5 = NULL;
  int   i   = 0;

  err_debug_function();

  if(defAUTH != g.state)
    err_debug_return(defBadcmd);

  if(g.realm) {
    if( ! (g.realm->auth_cmd & defAuthCmdApop)) {
      err_debug(5, "APOP command not configured!");
      err_debug_return(defBadcmd);
    }
  }

  if(!g.pop3_arg[0])
    err_debug_return(defParam);

  // if previous data exists remove it
  if(g.user)
    free(g.user);
  g.user = NULL;
  if(g.realm_name)
    free(g.realm_name);
  g.realm_name = NULL;

  // locate md5 digest
  for(i=0;isgraph(g.pop3_arg[i]);i++);
  if(!isspace(g.pop3_arg[i]))
    err_debug_return(defParam);
  g.pop3_arg[i] = '\0';
  md5 = &g.pop3_arg[i+1];
  for(;isspace(md5[0]);md5++); // eat blanks

  // locate realm in a username
  if(g.cfg->realm_chars)
    i = strcspn(g.pop3_arg, g.cfg->realm_chars);
  else
    i = strlen(g.pop3_arg);

  if('\0' != g.pop3_arg[i]) { // have we found a realm name ?
    g.realm_name = strdup(&(g.pop3_arg[i+1]));
    if(!g.realm_name) {
        err_malloc_error();
        err_debug_return(defMalloc);
    }
  } else {
      g.realm_name = NULL;
  }
  g.pop3_arg[i] = '\0';

  // copy username
  g.user = strdup(g.pop3_arg);
  if(!g.user) {
    err_malloc_error();
    err_debug_return(defMalloc);
  }

  // authenticate the user
  err_debug_return( pulsar_auth(defCredApop, md5) );
}
//------------------------------------------------------------------------------------------------------------
