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

#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <netdb.h>

#ifdef HAVE_CRYPT_H
#include <crypt.h>
#endif /* HAVE_CRYPT_H */

#include "pulsar.h"
#include "users.h"
#include "util.h"
#include "md5.h"
#include "error_facility.h"

//------------------------------------------------------------------------------------------------------------
int users_open_mailstore(int op) {
  int tmp;
  int rc;

  err_debug_function();

  if(!g.owner.uid_set || !g.owner.gid_set) {
    err_error(EX_OSFILE, "ERROR: Mailstore owner (uid or gid) are not set.");
    err_debug_return(defConf);
  }

  // sanity checks for UID and GID
  if(g.owner.uid_set && !g.owner.gid) {
    err_debug(4, "WARNING: GID == 0 for user \"%s\" in realm \"%s\".", g.user, g.realm->realm_name);
  }
  if(g.owner.uid_set && !g.owner.uid) {
    err_debug(4, "WARNING: UID == 0 for user \"%s\" in realm \"%s\"", g.user, g.realm->realm_name);
  }
  err_debug(5, "Will switch to: uid = %d, gid = %d", g.owner.uid, g.owner.gid);

  // drop gid privs
  if (g.owner.gid != getgid()) {
    rc = setgid(g.owner.gid);
    if(-1 == rc) {
      g.state = defABORT;
      err_error(EX_NOPERM,"ERROR: Can't setgid(): \"%s\"", strerror(errno));
      err_debug_return(defConf);
    }
  }

  // drop uid privs
  if (g.owner.uid != getuid()) {
    rc = setuid(g.owner.uid);
    if(-1 == rc) {
      g.state = defABORT;
      err_error(EX_NOPERM,"ERROR: Can't setuid(): \"%s\"", strerror(errno));
      err_debug_return(defConf);
    }
  }

  // construct path to this users mailbox.
  if(g.realm->mailspool.type & defMailspoolHome) {
    if(!g.homedir) {
      err_error(EX_UNAVAILABLE, "ERROR: Homedir unavailable for user \"%s\"", g.user);
      err_debug_return(defConf);
    }
    tmp = strlen(g.realm->mailspool.dir) + strlen(g.homedir) + 2; // homedir + '/' + mailspool + '\0';
    g.mailbox_path = safe_malloc(tmp);
    if(!g.mailbox_path) {
      err_malloc_error();
      err_debug_return(-1);
    }
    strcpy(g.mailbox_path, g.homedir);
    strcat(g.mailbox_path, "/");
    strcat(g.mailbox_path, g.realm->mailspool.dir);
  } else {
    tmp = strlen(g.realm->mailspool.dir) + strlen(g.user) + 2; // mailspool + '/' + username + '\0';
    g.mailbox_path = safe_malloc(tmp);
    if(!g.mailbox_path) {
      err_malloc_error();
      err_debug_return(-1);
    }
    strcpy(g.mailbox_path, g.realm->mailspool.dir);
    strcat(g.mailbox_path, "/");
    strcat(g.mailbox_path, g.user);
  }

  // open mailstore
  if(g.realm->mailspool.type & defMailspoolMaildir)
    tmp = mailstore_open(g.mailbox_path, defMailstoreMaildir, 0, g.realm->mailspool.mode, op, &g.head);
  else if (g.realm->sqlite_enable)
    tmp = mailstore_open(g.mailbox_path, defMailstoreSqliteMailbox, 0, g.realm->mailspool.mode, op, &g.head);
  else
    tmp = mailstore_open(g.mailbox_path, defMailstoreMailbox, 0, g.realm->mailspool.mode, op, &g.head);

  if (-1 == tmp) {
    if (EX_TEMPFAIL == err_get_rc()) // temporary failure on mailbox open? It must be a lock by somebody else.
      err_debug_return(defMailboxLocked);
    else
      err_debug_return(defMailbox);
  }

  err_debug_return(defOK);
}
//------------------------------------------------------------------------------------------------------------
// TODO.. overwrite credentials after you are done with it!!!
int users_auth(const int type, const char *credentials) {
  int rc = defConf;
  int i;

  err_debug_function();

  assert(!g.homedir);
  assert(credentials);

  // TODO: overwrite all credentials variables
  // authenticate user with a PASS credentia
  for(i=0; i<g.realm->auth_db_count; i++) { // try all authentication methods in a row.
    switch(g.realm->auth_db[i].type) {
    case defAuthDbUnixEnum:
        err_debug(6,"Trying UNIX authentication (%d/%d).",i+1,g.realm->auth_db_count);
        rc = users_unix_auth(type, credentials);
        if(rc) {
          err_debug(2,"%s",err_get_error());
        } else {
          err_debug(6,"UNIX authentication (%d/%d) OK.",i+1,g.realm->auth_db_count);
        }
        break;
    case defAuthDbFileEnum:
        err_debug(6,"Trying FILE authentication (%d/%d).",i+1,g.realm->auth_db_count);
        rc = users_file_auth(type, credentials, &g.realm->auth_db[i]);
        if(rc) {
          err_debug(2,"%s",err_get_error());
        } else {
          err_debug(6,"FILE authentication (%d/%d) OK.",i+1,g.realm->auth_db_count);
        }
        break;
    #ifdef WITH_PAM
    case defAuthDbPAMEnum:
        err_debug(6,"Trying PAM authentication (%d/%d).",i+1,g.realm->auth_db_count);
        rc = users_pam_auth(type, credentials);
        if(rc) {
          err_debug(2,"%s",err_get_error());
        } else {
          err_debug(6,"PAM authentication (%d/%d) OK.",i+1,g.realm->auth_db_count);
        }
        break;
    #endif /* WITH_PAM */
    #ifdef WITH_MYSQL
    case defAuthDbMySQLEnum:
        err_debug(6,"Trying MySQL authentication (%d/%d).",i+1,g.realm->auth_db_count);
        rc = users_mysql_auth(type, credentials, g.realm->auth_db[i].filename);
        if(rc) {
          err_debug(2,"%s",err_get_error());
        } else {
          err_debug(6,"MySQL authentication (%d/%d) OK.",i+1,g.realm->auth_db_count);
        }
        break;
    #endif /* WITH_MYSQL */
    default:
        err_internal_error();
        err_debug_return(defERR);
        break;
    }

    if(defOK == rc) { // authentication succesfull
      if(g.realm->mailspool_owner.uid_set) {
        g.owner.uid_set = 1;
        g.owner.uid = g.realm->mailspool_owner.uid;
      }
      if(g.realm->mailspool_owner.gid_set) {
        g.owner.gid_set = 1;
        g.owner.gid = g.realm->mailspool_owner.gid;
      }
      break;
    } // ~if
  } // ~for

  if(defOK != rc) {
    g.owner.uid_set = 0;
    g.owner.gid_set = 0;
    err_debug_return(rc);
  }

  err_debug_return( users_open_mailstore(defMailstoreOpRetr) );
}
//------------------------------------------------------------------------------------------------------------
int users_getinfo() {
  int rc = defConf;
  int i;

  err_debug_function();

  for(i=0; i<g.realm->auth_db_count; i++) { // try all authentication methods in a row.
    switch(g.realm->auth_db[i].type) {
    case defAuthDbUnixEnum:
        err_debug(6,"Trying UNIX information retreival (%d/%d).",i+1,g.realm->auth_db_count);
        rc = users_unix_getinfo();
        if(rc) {
          err_debug(2,"%s",err_get_error());
        } else {
          err_debug(6,"UNIX information retreival (%d/%d) OK.",i+1,g.realm->auth_db_count);
        }
        break;
    case defAuthDbFileEnum:
        err_debug(6,"Trying FILE information retreival (%d/%d).",i+1,g.realm->auth_db_count);
        rc = users_file_getinfo(g.realm->auth_db[i].filename);
        if(rc) {
          err_debug(2,"%s",err_get_error());
        } else {
          err_debug(6,"FILE information retreival (%d/%d) OK.",i+1,g.realm->auth_db_count);
        }
        break;
    #ifdef WITH_PAM
    case defAuthDbPAMEnum:
        err_debug(6,"Trying PAM information retreival (%d/%d).",i+1,g.realm->auth_db_count);
        rc = users_pam_getinfo();
        if(rc) {
          err_debug(2,"%s",err_get_error());
        } else {
          err_debug(6,"PAM information retreival (%d/%d) OK.",i+1,g.realm->auth_db_count);
        }
        break;
    #endif /* WITH_PAM */
    #ifdef WITH_MYSQL
    case defAuthDbMySQLEnum:
        err_debug(6,"Trying MySQL information retreival (%d/%d).",i+1,g.realm->auth_db_count);
        rc = users_mysql_getinfo(g.realm->auth_db[i].filename);
        if(rc) {
          err_debug(2,"%s",err_get_error());
        } else {
          err_debug(6,"MySQL information retreival (%d/%d) OK.",i+1,g.realm->auth_db_count);
        }
        break;
    #endif /* WITH_MYSQL */
    default:
        err_internal_error();
        err_debug_return(defERR);
        break;
    }

    if(defOK == rc) { // authentication succesfull
      if(g.realm->mailspool_owner.uid_set) {
        g.owner.uid_set = 1;
        g.owner.uid = g.realm->mailspool_owner.uid;
      }
      if(g.realm->mailspool_owner.gid_set) {
        g.owner.gid_set = 1;
        g.owner.gid = g.realm->mailspool_owner.gid;
      }
      break;
    } // ~if
  } // ~for

  if(defOK != rc) {
    g.owner.uid_set = 0;
    g.owner.gid_set = 0;
    return rc;
  }

  err_debug_return( users_open_mailstore(defMailstoreOpStore) );
}
//------------------------------------------------------------------------------------------------------------
int users_pass_check_plain(const char *user_pass, const char *db_pass) {
  if(!strcmp(user_pass, db_pass))
    return defOK;

  return defAuth;
}
//------------------------------------------------------------------------------------------------------------
int users_pass_check_crypt(const char *user_pass, const char *db_pass) {
  char *hash;

  hash = crypt(user_pass, db_pass); // DES or MD5 check
  if(!strcmp(hash, db_pass))
    return defOK;

  return defAuth;
}
//------------------------------------------------------------------------------------------------------------
int users_pass_check(const char *user_pass, const char *db_pass, const strPassHash *hash) {
  int i;
  int rc;

  err_debug_function();
  //assert(hash->count);
  if(!hash->count) {
    err_debug(0, "ERROR: No hash types defined!");
    return defConf;
  }

  for(i=0; i<hash->count; i++) {
    rc = defAuth;
    switch(hash->hash[i]) {
    case defPassHashPlain:
        err_debug(5, "Checking plaintext password.");
        rc = users_pass_check_plain(user_pass, db_pass);
        break;
    case defPassHashCrypt:
        err_debug(5, "Checking crypted password.");
        rc = users_pass_check_crypt(user_pass, db_pass);
        break;
    default:
        err_debug(0, "ERROR: Unknown hash type %d", hash->hash[i]);
        rc = defConf;   // access denied!
        break;
    }
    if(defOK == rc) {
      err_debug(5, "Password OK.");
      err_debug_return(rc);
    }
  }

  err_debug(5, "Password doesn't match.");
  err_debug_return(defAuth);
}
//------------------------------------------------------------------------------------------------------------
int users_apop_check(const char *credentials, const char *db_pass) {
  int   md5hash[4]; // 16 bytes
  char  md5hash_hex[33];
  char *full_string;
  int   len;
  int   rc;

  err_debug_function();

  len = strlen(g.apop_string) + strlen(db_pass);
  full_string = safe_malloc(len + 1);
  if(!full_string) {
    err_malloc_error();
    return defMalloc;
  }

  strcpy(full_string, g.apop_string);
  strcat(full_string, db_pass);
  err_debug(5, "String to hash: \"%s\"", full_string);

  md5_buffer(full_string, len, md5hash);
  sprintf(md5hash_hex,
          "%08x%08x%08x%08x",
          ntohl(md5hash[0]),
          ntohl(md5hash[1]),
          ntohl(md5hash[2]),
          ntohl(md5hash[3])
         );

  err_debug(5, "comparing: \"%s\" == \"%s\"", credentials, md5hash_hex);
  if(!strcmp(credentials, md5hash_hex))
    rc = defOK;
  else
    rc = defAuth;

  free(full_string);

  err_debug_return(rc);
}
//------------------------------------------------------------------------------------------------------------
