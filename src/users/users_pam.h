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

#ifndef __USERS_PAM_H__
#define __USERS_PAM_H__



#ifdef WITH_PAM

#include <security/pam_appl.h>
int users_pam_getinfo();
int users_pam_auth(const int   type,
                   const char *credentials );
/* Private function! */
int users_pam_auth_cb(int                        num_msg,
                      const struct pam_message **msg,
                      struct pam_response      **resp,
                      void                      *appdata_ptr );
#endif /* WITH_PAM */



#else

#ifdef DEBUG
#warning file "users_pam.h" already included.
#endif /* DEBUG */

#endif
