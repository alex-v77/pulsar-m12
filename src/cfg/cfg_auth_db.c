/*
 * Copyright (C) 2001 Rok Papez <rok.papez@lugos.si>
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
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "cfg.h"
#include "util.h"
#include "cfg_auth_db.h"
//------------------------------------------------------------------------------------------------------------
int interpret_cfg_data_AuthDb(strValues *val, strOptionsRealm  *realm) {
  strValues *tmp;
  int        i;
  int        j;
  int        rc;

  tmp = val;
  for(i=0;tmp;tmp = tmp->next, i++);

  if(realm->auth_db)
    free(realm->auth_db);
  realm->auth_db = safe_calloc(i, sizeof(*realm->auth_db));
  if(!realm->auth_db) {
    rc = 2;
    goto error;
  }
  realm->auth_db_count = i;

  tmp = val;
  for(i=0; i<realm->auth_db_count; i++) {
    if(!strcasecmp(tmp->val,defAuthDbUnixStr)) { // UNIX passwd authentication
      if(tmp->opt_count) {
        rc = 6;
        goto error;
      }
      realm->auth_db[i].type = defAuthDbUnixEnum;
    } // ~unix

    #ifdef WITH_PAM
    else if(!strcasecmp(tmp->val, defAuthDbPAMStr)) { // PAM authentication
      if(tmp->opt_count) {
        rc = 6;
        goto error;
      }
      realm->auth_db[i].type = defAuthDbPAMEnum;
    } // ~PAM
    #endif /* WITH PAM */

    else if(!strcasecmp(tmp->val, defAuthDbFileStr)) { // file authentication
      if(tmp->opt_count < 1) {
        rc = 6;
        goto error;
      }
      realm->auth_db[i].type = defAuthDbFileEnum;
      realm->auth_db[i].filename = strdup(tmp->opt[0]);
      if(!realm->auth_db[i].filename) {
        rc = 2;
        goto error;
      }
      for(j=1; j<tmp->opt_count; j++) {
        if(!strcmp(defPassHashPlainStr, val->opt[j]))
          realm->auth_db[i].hash.hash[j-1] = defPassHashPlain;
        else if(!strcmp(defPassHashCryptStr, val->opt[j]))
          realm->auth_db[i].hash.hash[j-1] = defPassHashCrypt;
        else {
          rc = 6;
          goto error;
        }
      } // ~for
      realm->auth_db[i].hash.count = tmp->opt_count - 1;
      // default to plaintext authentication when not specified
      if(!realm->auth_db[i].hash.count) { 
        realm->auth_db[i].hash.count = 1;
        realm->auth_db[i].hash.hash[0] = defPassHashPlain;
      }
    } // ~file

    #ifdef WITH_MYSQL
    else if(!strcasecmp(tmp->val,defAuthDbMySQLStr)) { // MySQL authentication
      if(tmp->opt_count != 1) {
        rc = 6;
        goto error;
      }
      realm->auth_db[i].type = defAuthDbMySQLEnum;
      realm->auth_db[i].filename = strdup(tmp->opt[0]);
      if(!realm->auth_db[i].filename) {
        rc = 2;
        goto error;
      }
    } // ~mysql
    #endif /* WITH MYSQL */

    else {
      rc = 6;
      goto error;
    }
    tmp = tmp->next;
  } // ~for

  return 0;

error:
  free_all_options_AuthDb(realm);
  return rc;
}
//------------------------------------------------------------------------------------------------------------
void free_all_options_AuthDb(strOptionsRealm *realm) {
  int i;

  if(realm->auth_db) {
    for(i=0; i<realm->auth_db_count ;i++) {
      if(realm->auth_db[i].filename)
        free(realm->auth_db[i].filename);
    }
    free(realm->auth_db);
  }
  realm->auth_db_count = 0;
}
//------------------------------------------------------------------------------------------------------------
void dump_cfg_data_AuthDb(FILE *fd, const char *fill, strOptionsRealm  *realm) {
  char *param = NULL;
  char *opt   = NULL;
  int   i;
  int   j;

  if(!fd || !fill || !realm->auth_db || !realm->auth_db_count)
    return;

  for(i=0 ;i<realm->auth_db_count; i++) {

    param = NULL;
    switch(realm->auth_db[i].type) {
    case defAuthDbUnixEnum:
        opt = defAuthDbUnixStr;
        break;
    #ifdef WITH_PAM
    case defAuthDbPAMEnum:
        opt = defAuthDbPAMStr;
        break;
    #endif /* WITH PAM */
    case defAuthDbFileEnum:
        opt = defAuthDbFileStr;
        param = realm->auth_db[i].filename;
        break;
    #ifdef WITH_MYSQL
    case defAuthDbMySQLEnum:
        opt = defAuthDbMySQLStr;
        param = realm->auth_db[i].filename;
        break;
    #endif /* WITH MYSQL */
    }

    if(!i)
      fprintf(fd,"%sauth_db = %s",fill,opt);
    else
      fprintf(fd,",\n%s %s",fill,opt);
    if(param)
      fprintf(fd,":%s",param);

    for(j=0; j<realm->auth_db[i].hash.count; j++) {
      switch(realm->auth_db[i].hash.hash[j]) {
      case defPassHashPlain:
          fprintf(fd, ":%s", defPassHashPlainStr);
          break;
      case defPassHashCrypt:
          fprintf(fd, ":%s", defPassHashCryptStr);
          break;
      default:
          fprintf(fd, ":<error - invalid value>");
      } // ~switch
    } // ~for
  } // ~for

  fprintf(fd,"\n");
  return;
}
//------------------------------------------------------------------------------------------------------------
int add_realm_AuthDb(strOptionsRealm *dst, strOptionsRealm *src) {
  int error = 0;
  int i;

  if(!dst)
    return 0;

  if(!src) { // creation of default realm.
    dst->auth_db = safe_calloc(1,sizeof(dst->auth_db));
    if(!dst->auth_db)
      return 0;
    dst->auth_db_count = 1;
    dst->auth_db[0].type = defAuthDbUnixEnum;
    return 1;
  }

  if(!src->auth_db_count) // error.
    return 0;

  dst->auth_db = safe_malloc(src->auth_db_count * sizeof(src->auth_db));
  if(!dst->auth_db)
    return 0;
  memcpy(dst->auth_db, src->auth_db, src->auth_db_count * sizeof(src->auth_db));

  for(i=0;i<src->auth_db_count;i++) {
    if(error) { // we must take care that cleanup doesn't SIGSEGV on double free().
      dst->auth_db[i].filename = NULL;
      continue;
    }
    if(src->auth_db[i].filename) { // make copies of the strings!
      dst->auth_db[i].filename = strdup(src->auth_db[i].filename);
      if(!dst->auth_db[i].filename)
        error = 1;
    }
  }

  if(error)
    return 0;

  return 1;
}
//------------------------------------------------------------------------------------------------------------
