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

#include "error_facility.h"
#include "pulsar.h"

//------------------------------------------------------------------------------------------------------------
int  pulsar_uidl() {
  int              i;
  strMailstoreMsg *my_msg;

  if(defCONN != g.state)
    return defBadcmd;

  if(!g.pop3_arg[0]) {
    pulsar_printf("+OK %d messages", g.head->del_msg_count);
    for(i=0;i<g.head->msg_count;i++) {
      my_msg = &(g.head->msgs[i]);
      if(my_msg->deleted)
        continue;
      pulsar_printf("%d %s", i+1, my_msg->uidl);
    }
    pulsar_printf(".");
    return defNONE;
  }

  i = atoi(g.pop3_arg);

  if(i<1 || i>g.head->msg_count)
    return defParam;

  my_msg = &(g.head->msgs[i-1]);

  if(my_msg->deleted)
    return defDeleted;

  pulsar_printf("+OK %d %s", i, my_msg->uidl);
  return defNONE;
}
//------------------------------------------------------------------------------------------------------------
