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

#include "util.h"

//------------------------------------------------------------------------------------------------------------
int safe_read(int fd, void *buf, size_t count) {
  char *tmp;
  int rc;
  int total = 0;

  while(total < count) {
    tmp = &(((char *)buf)[total]);
    rc = read(fd, tmp, count - total);
    if (-1 == rc && EINTR == errno)
      continue;  // interrupted by signal.. try again with the same parameters
    if (rc < 0)
      return -1; // error.
    if (!rc) // no more data
      break;
    total += rc;
  }

  return total;
}
//----------------------------------------------------------------------------------------
int safe_write(int fd, const void *buf, size_t count) {
  char *tmp;
  int   rc;
  int   total = 0;

  while (total < count) {
    tmp = &(((char *)buf)[total]);
    rc = write(fd, tmp, count - total);
    if (-1 == rc && EINTR == errno)
      continue;  // interrupted by signal.. try again with the same parameters
    if (rc < 0)
      return -1; // error.
    if (!rc) // no more dat
      break;
    total += rc;
  }

  if(total != count)
    return -1; // error

  return total;
}
//------------------------------------------------------------------------------------------------------------
