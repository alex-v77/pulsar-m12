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
#include <string.h>
#include <errno.h>
#define __USE_XOPEN
#include <unistd.h>
#undef __USE_XOPEN

#include <pwd.h>
#include <sys/types.h>

#include "pulsar.h"
#include "error_facility.h"
#include "users.h"

int   get_user_record(const char *filename);
char *password = NULL;

//------------------------------------------------------------------------------------------------------------
int get_user_record(const char *filename) {
  FILE *fd;
  char *start;
  char *end;
  char  buf[1024];
  int   rc = defConf; // if defMaxFileUsers == 0 return configuration error
  int   buf_siz;
  int   i;

  err_debug_function();

  assert(!password);
  assert(filename);

  free(g.homedir);
  g.homedir = NULL;

  fd = fopen(filename, "rb");
  if(!fd) {
    err_error(EX_NOINPUT, "FILE authentication failed; file \"%s\" can not be opened; Reason: \"%s\"", filename, strerror(errno));
    return defConf;
  }
  rewind(fd);

  // iterate thru passwd like file records
  for(i=0; i<defMaxFileUsers; i++) {
    if (!fgets(buf, sizeof(buf), fd)) { // out of entries.
      err_error(EX_NOPERM,"User \"%s\" not found in file \"%s\".", g.user, filename);
      rc = defAuth;
      break;
    }

    buf_siz = strlen(buf);
    if(sizeof(buf) <= buf_siz + 1) {
      err_error(EX_OSFILE, "User record in file \"%s\", line %d is too long!", filename, i);
      rc = defConf;
    }

    start = buf;
    // check username entry
    end = strchr(start, ':');
    if(!end)
      goto syntax_error;
    end[0] = '\0';
    if(strcmp(g.user, start)) // match user with a record
      continue;
    err_debug(5, "user %s matched.", g.user);
    end[0] = ':';
    start = end;

    // get password entry
    end = strchr(start + 1, ':');
    if(!end)
      goto syntax_error;
    end[0] = '\0';
    password = strdup(start + 1);
    end[0] = ':';
    if(!password)
      goto malloc_error;

    // get UID entry
    start = end;
    end = strchr(start + 1, ':');
    if(!end)
      goto syntax_error;
    g.owner.uid = strtol(start + 1, NULL, 10);

    // get GID entry
    start = end;
    end = strchr(start + 1, ':');
    if(!end)
      goto syntax_error;
    g.owner.gid = strtol(start + 1, NULL, 10);

    // skip GECOS
    start = end;
    end = strchr(start + 1, ':');
    if(!end)
      goto syntax_error;

    // get homedir
    start = end;
    end = strchr(start + 1, ':');
    if(!end)
      goto syntax_error;
    //g.homedir = strndup(start + 1, end - start);
    end[0] = '\0';
    g.homedir = strdup(start + 1);
    end[0] = ':';
    if(!g.homedir)
      goto malloc_error;

    g.owner.uid_set = 1;
    g.owner.gid_set = 1;

    rc = defOK;
    break;
  } // ~for

  if(defMaxFileUsers == i) {
    err_error(EX_SOFTWARE, "ERROR: Search for user \"%s\" in file \"%s\" hit the defMaxFileUsers limit.", g.user, filename);
    rc = defConf;
  }
  goto exit;

malloc_error:
  err_malloc_error();
  rc = defMalloc;
  goto exit;

syntax_error:
  err_error(EX_OSFILE, "Syntax error in passwd file \"%s\" in line %d.", filename, i);
  rc = defConf;

exit:
  fclose(fd);
  return rc;
}
//------------------------------------------------------------------------------------------------------------
int users_file_getinfo(const char *filename) {
  int rc;

  rc = get_user_record(filename);
  free(password);
  password = NULL;

  return rc;
}
//------------------------------------------------------------------------------------------------------------
int users_file_auth(const int        type,
                    const char      *credentials,
                    const strAuthDb *auth_db
                   )
{
  int   rc;

  err_debug_function();

  rc = get_user_record(auth_db->filename);
  if(defOK != rc)
    return rc;

  if(!password) { // playing safe
    err_error(EX_NOPERM, "Password in file \"%s\" not set for user \"%s\".", auth_db->filename, g.user);
    rc = defAuth;
    goto exit;
  }

  if(!password[0]) {
    err_error(EX_NOPERM, "Password in file \"%s\" not set for user \"%s\".", auth_db->filename, g.user);
    rc = defAuth;
    goto exit;
  }

  if('!' == password[0] || '*' == password[0]) {
    err_error(EX_NOPERM, "Account for user \"%s\" in file \"%s\" is locked.", g.user, auth_db->filename);
    rc = defAuth;
    goto exit;
  }

  err_debug(5, "Attempting file: \"%s\" authentication for user \"%s\".", auth_db->filename, g.user);
  switch(type) {
  case defCredPass:
      rc = users_pass_check(credentials, password, &auth_db->hash);
      break;
  case defCredApop:
      rc = users_apop_check(credentials, password);
      break;
  default:
      err_internal_error();
      rc = defConf;
      break;
  }
  if(rc != defOK) {
    err_error(EX_NOPERM,"Authentication for user \"%s\" in file \"%s\" FAILED.", g.user, auth_db->filename);
    return rc;
  }

  err_debug(5, "File \"%s\" authentication for user \"%s\" OK.", auth_db->filename, g.user);
  rc = defOK;

exit:
  free(password);
  password = NULL;
  return rc;
}
//------------------------------------------------------------------------------------------------------------
