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

#ifdef WITH_MYSQL

#include <stdlib.h>
#include <string.h>
#include <mysql/mysql.h>

#include "pulsar.h"
#include "error_facility.h"
#include "users.h"
#include "util.h"

int users_mysql_construct_query(char **query, strOptionsMySQL *mysql);

//select %pass_column [,%homedir | ,%uid |,%gid ]from %table
//where %user_column='g->user' and %and limit 1
#define defQueryTemplate1 "select %s "
#define defQueryTemplate2 ",%s "
#define defQueryTemplate3 "from %s where %s=\'%s\' and %s limit 1"
#define defQueryTemplateSize (     sizeof(defQueryTemplate1) \
                               + 3*sizeof(defQueryTemplate2) \
                               +   sizeof(defQueryTemplate3) \
                             )
strOptionsMySQL *mysql;
char            *db_pass;
//------------------------------------------------------------------------------------------------------------
int users_mysql_construct_query(char **query, strOptionsMySQL *mysql) {
  char *safe_username = NULL;
  int   size;
  int   index;
  int   rc;

  err_debug_function();

  *query = NULL;

  // sanity checks.
  if(!mysql) {
    err_internal_error();
    return defConf; // mask internal error as misconfiguration error.
  }

  if(!mysql->table) {
    err_error(EX_CONFIG, "MySQL(%s): table not set!", mysql->ID);
    return defConf;
  }

  if(!mysql->user_column) {
    err_error(EX_CONFIG, "MySQL(%s): user_column not set!", mysql->ID);
    return defConf;
  }

  if(!mysql->pass_column) {
    err_error(EX_CONFIG, "MySQL(%s): pass_column not set!", mysql->ID);
    return defConf;
  }

  // calculate the needed size for a query string.
  size = defQueryTemplateSize;

  size += strlen(mysql->pass_column);

  if(mysql->homedir)
    size += strlen(mysql->homedir);
  if(mysql->uid)
    size += strlen(mysql->uid);
  if(mysql->gid)
    size += strlen(mysql->gid);

  size += strlen(mysql->table);
  size += strlen(mysql->user_column);

  if(mysql->and)
    size += strlen(mysql->and);

  index = 2*strlen(g.user);
  safe_username = safe_malloc(index);
  if(!safe_username) {
    rc = defMalloc;
    goto error;
  }
  mysql_escape_string(safe_username, g.user, strlen(g.user));
  size += index;

  // allocate and create query string
  *query = safe_malloc(size);
  if(!*query) {
    rc = defMalloc;
    goto error;
  }

  rc = snprintf(*query, size, defQueryTemplate1, mysql->pass_column);
  if(rc < 0 || rc > size) {
    err_internal_error();
    rc = defConf;
    goto error;
  }
  index = rc;

  if(mysql->homedir) {
    rc = snprintf(&(*query)[index], size - index, defQueryTemplate2,
                  mysql->homedir
                 );
    if(rc < 0 || rc > size - index) {
      err_internal_error();
      rc = defConf;
      goto error;
    }
    index += rc;
  }

  if(mysql->uid) {
    rc = snprintf(&(*query)[index], size - index, defQueryTemplate2,
                  mysql->uid
                 );
    if(rc < 0 || rc > size - index) {
      err_internal_error();
      rc = defConf;
      goto error;
    }
    index += rc;
  }

  if(mysql->gid) {
    rc = snprintf(&(*query)[index], size - index, defQueryTemplate2,
                  mysql->gid
                 );
    if(rc < 0 || rc > size - index) {
      err_internal_error();
      rc = defConf;
      goto error;
    }
    index += rc;
  }

  rc = snprintf(&(*query)[index], size - index, defQueryTemplate3,
                mysql->table, mysql->user_column, safe_username,
                mysql->and ? mysql->and : "1"
               );
  if(rc < 0 || rc > size - index) {
    err_internal_error();
    rc = defConf;
    goto error;
  }

  return defOK;

error:
  if(*query)
    free(*query);
  if(safe_username)
    free(safe_username);
  *query = NULL;
  return rc;
}
//------------------------------------------------------------------------------------------------------------
int users_mysql_auth(const int   type,
                     const char *credentials,
                     const char *ID
                    )
{
  int   rc;

  err_debug_function();

  db_pass = NULL;
  rc = users_mysql_getinfo(ID);
  if(defOK != rc)
    return rc;

  err_debug(5,"Attempting MySQL(%s) authentication for user \"%s\".", ID, g.user);
  switch(type) {
  case defCredPass:
      rc = users_pass_check(credentials, db_pass, &mysql->pass_hash);
      break;
  case defCredApop:
      rc = users_apop_check(credentials, db_pass);
      break;
  default:
      err_internal_error();
      rc = defConf;
      break;
  }

  if(defOK == rc)
    err_debug(5,"MySQL(%s) authentication for user \"%s\" OK.", ID, g.user);
  else
    err_debug(5,"MySQL(%s) authentication for user \"%s\" FAILED.", ID, g.user);
  if(db_pass)
    free(db_pass);

  return rc;
}
//------------------------------------------------------------------------------------------------------------
int users_mysql_getinfo(const char *ID) {
  MYSQL           *conn = NULL;
  MYSQL_RES       *res = NULL;
  MYSQL_ROW        row;
  char            *query = NULL;
  int              rc;
  int              count;

  err_debug(6, "mysql library version %s", mysql_get_client_info());

  mysql = get_mysql(ID, g.cfg);
  if(!mysql) {
    err_error(EX_CONFIG, "MySQL block \"%s\" not found.", ID);
    return defConf;
  }

  rc = users_mysql_construct_query(&query, mysql);
  if(rc)
    goto error;

  conn = mysql_init(NULL);
  if(!conn) {
    err_malloc_error();
    rc = defAuth;
    goto error;
  }

  if (!mysql_real_connect(conn,
                          mysql->host,
                          mysql->user,
                          mysql->pass,
                          mysql->db,
                          mysql->port,
                          NULL,
                          CLIENT_COMPRESS
                         )
     )
  {
    err_error(EX_OSERR, "Connection to MySQL(%s) server failed. Reason: \"%s\"",
              ID,
              mysql_error(conn)
             );
    rc = defAuth;
    goto error;
  }

  err_debug(5, "MySQL(%s) query: \"%s\"", ID, query);
  rc = mysql_real_query(conn, query, strlen(query));
  if(rc) {
    err_error(EX_OSERR, "Query to MySQL(%s) server failed. Reason: \"%s\"",
              ID,
              mysql_error(conn)
             );
    rc = defAuth;
    goto error;
  }

  res = mysql_store_result(conn);
  if(!res) {
    err_error(EX_OSERR, "Retreival of data from MySQL(%s) server failed. Reason: \"%s\"",
              ID,
              mysql_error(conn)
             );
    rc = defAuth;
    goto error;
  }

  if (1 != mysql_num_rows(res)) {
    err_error(EX_NOUSER, "User not known.");
    rc = defAuth; // access denied!
    goto error;
  }

  // get password from the database
  row = mysql_fetch_row(res);
  if(!res) {
    err_error(EX_OSERR, "Retreival of row from MySQL(%s) result set failed. Reason: \"%s\"",
              ID,
              mysql_error(conn)
             );
    rc = defAuth;
    goto error;
  }

  // TODO: additional checks with mysql_num_fields
  db_pass = strdup(row[0]);
  if(!db_pass) {
    err_malloc_error();
    rc = defMalloc;
    goto error;
  }

  count = 1;
  if(mysql->homedir) {
    if(row[count]) {
      g.homedir = strdup(row[count]);
      if(!g.homedir) {
        err_malloc_error();
        rc = defMalloc;
        goto error;
      }
    }
    count++;
  }

  // TODO: Check what atoi returns!
  if(mysql->uid) {
    if(row[count]) {
      g.owner.uid_set = 1;
      g.owner.uid = atoi(row[count]);
    }
    count++;
  }

  if(mysql->gid) {
    if(row[count]) {
      g.owner.gid_set = 1;
      g.owner.gid = atoi(row[count]);
    }
    count++;
  }

  rc = defOK;

error:
  if(res)
    mysql_free_result(res);
  if(conn)
    mysql_close(conn);
  if(query)
    free(query);
  return rc;
}
//------------------------------------------------------------------------------------------------------------
#endif /* WITH_MYSQL */
