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
#include <string.h>

#include <pwd.h>
#ifdef HAVE_SHADOW_H
#include <shadow.h>
#endif /* HAVE_SHADOW_H */
#define __USE_XOPEN
#include <unistd.h>
#undef __USE_XOPEN
#include <sys/types.h>

#include "pulsar.h"
#include "error_facility.h"
#include "users_unix.h"

struct passwd *pw;
//------------------------------------------------------------------------------------------------------------
#ifdef HAVE_SHADOW_H
int users_unix_auth_shadow(const char *pass) {
  struct spwd *sp;
  char        *hash;

  assert(g.user);
  assert(pass);

  err_debug(5,"Attempting shadow authentication for user \"%s\".",g.user);
  sp = getspnam(g.user);
  if(!sp) {
    err_error(EX_NOUSER,"User \"%s\" doesn't exist in shadow database.",g.user);
    return defAuth;
  }

  if(!sp->sp_pwdp) {
    err_error(EX_NOPERM,"Shadow password not set for user \"%s\".",g.user);
    return defAuth;
  }

  if(!sp->sp_pwdp[0]) {
    err_error(EX_NOPERM,"Shadow password not set for user \"%s\".",g.user);
    return defAuth;
  }

  if('!' == sp->sp_pwdp[0]) {
    err_error(EX_NOPERM,"Shadow account is locked for user \"%s\".",g.user);
    return defAuth;
  }

  hash = crypt(pass, sp->sp_pwdp); // TODO add pass overwriting. + g.pop3_argv
  if(strcmp(hash, sp->sp_pwdp)) {
    err_error(EX_NOPERM,"Shadow authentication for user %s failed: Bad password.",g.user);
    return defAuth;
  }

  err_debug(5,"Shadow authentication for user \"%s\" OK.",g.user);
  return defOK;
}
#else
int users_unix_auth_shadow(const char *pass) {
  return defAuth;
}
#endif /* HAVE_SHADOW_H */
//------------------------------------------------------------------------------------------------------------
int users_unix_auth(const int   type,
                    const char *credentials
                   )
{
  char *hash;
  int   rc;

  err_debug_function();
  assert(g.user);



  if(defCredPass != type) {
    err_debug(5, "PAM authentication only works with PASS");
    return defERR;
  }



  err_debug(5, "Attempting UNIX system database authentication for user \"%s\".", g.user);

  rc = users_unix_getinfo();
  if(defOK != rc)
    return rc;

  if(!pw->pw_passwd) { // playing safe
    err_error(EX_NOPERM,"Password not set for user \"%s\".",g.user);
    return defAuth;
  }

  if(!pw->pw_passwd[0]) {
    err_error(EX_NOPERM,"Password not set for user \"%s\".",g.user);
    return defAuth;
  }

  if('!' == pw->pw_passwd[0]) {
    err_error(EX_NOPERM,"Account is locked for user \"%s\".",g.user);
    return defAuth;
  }

  if(1 == strlen(pw->pw_passwd) && pw->pw_passwd[0] == 'x')
    return users_unix_auth_shadow(credentials); // check shadow

  err_debug(5,"Attempting passwd authentication for user \"%s\".",g.user);
  hash = crypt(credentials, pw->pw_passwd); // TODO add pass overwriting. + g.pop3_argv
  if(strcmp(hash, pw->pw_passwd)) {
    err_error(EX_NOPERM,"Authentication for user %s failed: Bad password.",g.user);
    return defAuth;
  }
  err_debug(5,"Passwd authentication for user \"%s\" OK.",g.user);

  return defOK;
}
//------------------------------------------------------------------------------------------------------------
int users_unix_getinfo() {

  assert(g.user);

  pw = getpwnam(g.user);
  if(!pw) {
    err_error(EX_NOUSER, "User \"%s\" doesn't exist in system database.", g.user);
    return defAuth;
  }

  g.owner.uid = pw->pw_uid;
  g.owner.uid_set = 1;
  g.owner.gid = pw->pw_gid;
  g.owner.gid_set = 1;

  // out with the old one...
  free(g.homedir);
  g.homedir = strdup(pw->pw_dir); //... in with a new one
  if(!g.homedir) {
    err_malloc_error();
    return defMalloc;
  }
  return defOK;
}
//------------------------------------------------------------------------------------------------------------
