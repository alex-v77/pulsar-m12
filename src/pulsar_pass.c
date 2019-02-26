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

#include <string.h>

#include "error_facility.h"
#include "pulsar.h"
#include "users.h"

//------------------------------------------------------------------------------------------------------------
int pulsar_pass() {
  err_debug_function();

  if(defAUTH != g.state)
    return defBadcmd;

  if(g.realm) {
    if( ! (g.realm->auth_cmd & defAuthCmdUser)) {
      err_debug(5, "PASS command not configured!");
      err_debug_return(defBadcmd);
    }
  }

  if(!g.user) {
    pulsar_printf("-ERR USER first");
    return defNONE;
  }

  if(!g.pop3_arg[0])
    return defParam;

  return pulsar_auth(defCredPass, g.pop3_arg);
}
//------------------------------------------------------------------------------------------------------------
