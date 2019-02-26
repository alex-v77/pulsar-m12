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

#ifndef __UTIL_H__
#define __UTIL_H__

#include <unistd.h>
#include <errno.h>

/* safe_alloc.c */
void *safe_calloc(size_t nmemb, size_t size);
void *safe_malloc(size_t size);

/* safe_io.c */
int   safe_read (int fd, void       *buf, size_t count);
int   safe_write(int fd, const void *buf, size_t count);

/* net_io.c */
int   net_partial_read(int fd, void *buf, size_t count);
int   net_read (int fd, void        *buf, size_t count);
int   net_write(int fd, const void  *buf, size_t count);

#else

#ifdef DEBUG
#warning file "util.h" already included.
#endif /* DEBUG */

#endif /* __UTIL_H__ */
