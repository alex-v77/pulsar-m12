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

#include <stdio.h>
#include <stdlib.h>

#include "error_facility.h"
#include "pulsar.h"
#include "util.h"

//------------------------------------------------------------------------------------------------------------
int  pulsar_list() {
  int              i;
  strMailstoreMsg *my_msg;

  if(defCONN != g.state)
    return defBadcmd;

  if(!g.pop3_arg[0]) {
    char *stream_data = 0;
    size_t stream_len;
    FILE *f = open_memstream( &stream_data, &stream_len );

    fprintf(f, "+OK %d messages (total: %u bytes)\r\n",
                  g.head->del_msg_count,
                  g.head->del_total_size
                 );
    err_debug(5, "reporting: %d messages of %u bytes",
              g.head->del_msg_count,
              g.head->del_total_size
             );
    for(i=0;i<g.head->msg_count;i++) {
      my_msg = &(g.head->msgs[i]);
      if(!my_msg->deleted)
        fprintf(f, "%d %d\r\n",i+1,my_msg->size);
    }
    fprintf(f, ".\r\n");

    fclose( f );
    net_write( g.fd_out, stream_data, stream_len );
    free( stream_data );

    return defNONE;
  }

  i = atoi(g.pop3_arg);

  if(i<1 || i>g.head->msg_count)
    return defParam;

  my_msg = &(g.head->msgs[i-1]);

  if(my_msg->deleted)
    return defDeleted;

  pulsar_printf("+OK %d %d",i,my_msg->size);
  return defNONE;
}
//------------------------------------------------------------------------------------------------------------
