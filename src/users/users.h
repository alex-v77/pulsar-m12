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

#ifndef __USERS_H__
#define __USERS_H__

#include "users_unix.h"
#include "users_file.h"
#include "users_pam.h"
#include "users_mysql.h"

/*
 * Authenticates user and retreives homedir information
 * (if possible).
 * For use by POP3 daemon
 */
int users_auth(const int type, const char *credentials);

/*
 * Checks if user exists (if possible) and retreives
 * homedir information.
 * For use by delivery agent
 */
int users_getinfo();

/*
 * These functions are private for users_ function!
 */
int users_open_mailstore(int op);
int users_pass_check(const char *user_pass, const char *db_pass, const strPassHash *hash);
int users_pass_check_crypt(const char *user_pass, const char *db_pass);
int users_pass_check_plain(const char *user_pass, const char *db_pass);
int users_apop_check(const char *credentials, const char *db_pass);


#else

#ifdef DEBUG
#warning file "users.h" already included.
#endif /* DEBUG */

#endif
