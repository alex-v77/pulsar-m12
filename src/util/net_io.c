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

#include "pulsar.h"
#include "util.h"

//------------------------------------------------------------------------------------------------------------
// BUGBUG: check rc from SSL_read() :-)
#ifdef WITH_SSL
int net_partial_read(int fd, void *buf, size_t count) {
  if(!g.ssl)
    return read(fd, buf, count);
  return SSL_read(g.ssl_con, buf, count);
}
#else
int net_partial_read(int fd, void *buf, size_t count) {
  return read(fd, buf, count);
}
#endif /* WITH_SSL */
//------------------------------------------------------------------------------------------------------------
// BUGBUG: check rc from SSL_read() :-)
#ifdef WITH_SSL
int net_read(int fd, void *buf, size_t count) {
  if(!g.ssl)
    return safe_read(fd, buf, count);
  return SSL_read(g.ssl_con, buf, count);
}
#else
int net_read(int fd, void *buf, size_t count) {
  return safe_read(fd, buf, count);
}
#endif /* WITH_SSL */
//------------------------------------------------------------------------------------------------------------
#ifdef WITH_SSL
int net_write(int fd, const void *buf, size_t count) {
  if(!g.ssl)
    return safe_write(fd, buf, count);
  return SSL_write(g.ssl_con, buf, count);
}
#else
int net_write(int fd, const void *buf, size_t count) {
  return safe_write(fd, buf, count);
}
#endif /* WITH_SSL */
//------------------------------------------------------------------------------------------------------------
