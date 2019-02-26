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
int  pulsar_retr() {
  int              num;
  int              rc;
  strMailstoreMsg *my_msg;

  if(defCONN != g.state)
    return defBadcmd;

  if(!g.pop3_arg[0])
    return defParam;

  num = atoi(g.pop3_arg);
  if(num<1 || num>g.head->msg_count)
    return defParam;

  my_msg = &(g.head->msgs[num-1]);

  if(my_msg->deleted)
    return defDeleted;

  pulsar_printf("+OK %lu octets", my_msg->size);
  rc = mailstore_retr(g.head, num-1, g.fd_out, -1);
  if(-1 == rc)
    return -1; // error on transmit.
  pulsar_printf(".");

  return defNONE;
}
//------------------------------------------------------------------------------------------------------------
