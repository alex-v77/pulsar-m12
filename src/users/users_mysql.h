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

#ifndef __USERS_MYSQL_H__
#define __USERS_MYSQL_H__

#ifdef WITH_MYSQL
int users_mysql_getinfo(const char *ID);
int users_mysql_auth(const int   type,
                     const char *credentials,
                     const char *ID);
#endif /* WITH_MYSQL */

#else

#ifdef DEBUG
#warning file "users_mysql.h" already included.
#endif /* DEBUG */

#endif
