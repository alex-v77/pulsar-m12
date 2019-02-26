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


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pwd.h>
#include <grp.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sysexits.h>
#define __USE_ISOC99
#define __USE_GNU
#include <ctype.h>

#include "error_facility.h"
#include "cfg.h"
#include "util.h"

#include "cfg_debug.h"
#include "cfg_inetd.h"
#include "cfg_realm_chars.h"
#include "cfg_listen_on.h"
#include "cfg_auth_cmd.h"
#include "cfg_auth_db.h"
#include "cfg_mailspool.h"
#include "cfg_mailspool_owner.h"
#include "cfg_certificate.h"
#include "cfg_realm_interface.h"

#include "cfg_mysql_host.h"
#include "cfg_mysql_user.h"
#include "cfg_mysql_pass.h"
#include "cfg_mysql_db.h"
#include "cfg_mysql_table.h"
#include "cfg_mysql_user_column.h"
#include "cfg_mysql_pass_column.h"
#include "cfg_mysql_and.h"
#include "cfg_mysql_homedir.h"
#include "cfg_mysql_uid.h"
#include "cfg_mysql_gid.h"

/*
 * If you plan on adding/removing options:
 * Check cfg.h and add types for "your_option"
 * Add/Remove files cfg_<YourOption>.c and .h
 * Modify following functions until you reach
 * four commented dashed lines
 * Changing isn't easy so study an existing option
 * until you have a clue of what is going on.
 *
 * These functions are *always* needed:
 * interpret_cfg_data_<YourOption>
 * dump_cfg_data_<YourOption>
 *
 * And these depend on your data type:
 * free_all_options_<YourOption>
 * add_realm_<YourOption>
 *
 * interpret_cfg_data_<YourOption>:
 * --------------------------------
 * This function can be called multiple times
 * (once for every occurance of <YourOption> variable
 * in a config file). Make sure it will remove the
 * old value if it exists before making a new one.
 *
 * dump_cfg_data_<YourOption>:
 * ---------------------------
 * is needed so a server can dump active configuration
 * to a file. Do *NOT* be tempted not to implement this!
 *
 * free_all_options_<YourOption>:
 * ------------------------------
 * this depends on your data type. If you have to allocate
 * memory for your options data than you will need to 
 * deallocate it here.
 *
 * add_realm_<YourOption>:
 * -----------------------
 * Only needed for realm specific options.
 *
 * When a new realm is added it is initialized with values
 * already set for a default realm. If your data type
 * is complex this function should "do the job" otherwise
 * just modify the add_realm function in this file.
 *
 * Be sure to check the add_realm function below and
 * implement default values for a default realm if needed.
 * By default all variables are set to 0 with memset.
 *
 */

//------------------------------------------------------------------------------------------------------------
// 1 .. file open error
// 2 .. malloc error
// 3 .. syntax error
// 4 .. internal check - assertion failed
// 5 .. Error - Line too long
// 6 .. Error - Invalid parameter
const char *options_errors[] = {
  "No error.",                                         // 0
  "Can't open file!",                                  // 1
  "Out of memory?",                                    // 2
  "Syntax error.",                                     // 3
  "Internal check - assertion failed.",                // 4
  "Error - Line too long.",                            // 5
  "Error - Invalid parameter.",                        // 6
  "Unknown error." // this has to be the last one!!!   // 7
};
//------------------------------------------------------------------------------------------------------------
int interpret_cfg_data_mysql(char **var, strValues **val,strOptionsMySQL *mysql,strOptionsGlobal *opts) {
  int rc = 0;

  if(!*var || !*val || !mysql || !opts ) {
    rc = 4;
    goto error;
  }

  if(!(*val)->val) {
    rc = 6;
    goto error;
  }

  if(!strcasecmp(*var, defCfgMySQLHost))
    rc = interpret_cfg_data_mysql_Host(*val, mysql);

  else if(!strcasecmp(*var, defCfgMySQLUser))
    rc = interpret_cfg_data_mysql_User(*val, mysql);

  else if(!strcasecmp(*var, defCfgMySQLPass))
    rc = interpret_cfg_data_mysql_Pass(*val, mysql);

  else if(!strcasecmp(*var, defCfgMySQLDb))
    rc = interpret_cfg_data_mysql_Db(*val, mysql);

  else if(!strcasecmp(*var, defCfgMySQLTable))
    rc = interpret_cfg_data_mysql_Table(*val, mysql);

  else if(!strcasecmp(*var, defCfgMySQLUserColumn))
    rc = interpret_cfg_data_mysql_UserColumn(*val, mysql);

  else if(!strcasecmp(*var, defCfgMySQLPassColumn))
    rc = interpret_cfg_data_mysql_PassColumn(*val, mysql);

  else if(!strcasecmp(*var, defCfgMySQLAnd))
    rc = interpret_cfg_data_mysql_And(*val, mysql);

  else if(!strcasecmp(*var, defCfgMySQLHomedir))
    rc = interpret_cfg_data_mysql_Homedir(*val, mysql);

  else if(!strcasecmp(*var, defCfgMySQLUid))
    rc = interpret_cfg_data_mysql_Uid(*val, mysql);

  else if(!strcasecmp(*var, defCfgMySQLGid))
    rc = interpret_cfg_data_mysql_Gid(*val, mysql);

  else
    rc = 6; // unknown option

error:
  free(*var);
  *var = NULL;
  strop_free(*val);
  *val = NULL;
  return rc;
}
//------------------------------------------------------------------------------------------------------------
int interpret_cfg_data_realm(char **var, strValues **val, strOptionsRealm  *realm, strOptionsGlobal *opts) {
  int rc = 0;

  if(!*var || !*val || !realm || !opts ) {
    rc = 4;
    goto error;
  }

  if(!(*val)->val) {
    rc = 6;
    goto error;
  }

  if(!strcasecmp(*var, defCfgDebug))
    rc = interpret_cfg_data_Debug(*val, opts);

  else if (!strcasecmp(*var, defCfgInetd))
    rc = interpret_cfg_data_Inetd(*val, opts);

  else if (!strcasecmp(*var, defCfgRealmChars))
    rc = interpret_cfg_data_RealmChars(*val, opts);

  else if (!strcasecmp(*var, defCfgListenOn))
    rc = interpret_cfg_data_ListenOn(*val, opts, 0x00);

  else if (!strcasecmp(*var, defCfgSSLListenOn))
    rc = interpret_cfg_data_ListenOn(*val, opts, defIfaceSSL);

  else if (!strcasecmp(*var, defCfgAuthCmd))
    rc = interpret_cfg_data_AuthCmd(*val, realm);

  else if (!strcasecmp(*var, defCfgAuthDb))
    rc = interpret_cfg_data_AuthDb(*val, realm);

  else if (!strcasecmp(*var, defCfgMailspool))
    rc = interpret_cfg_data_Mailspool(*val, realm);

  else if (!strcasecmp(*var, defCfgMailspoolOwner))
    rc = interpret_cfg_data_MailspoolOwner(*val, realm);

  else if (!strcasecmp(*var, defCfgCertificate))
    rc = interpret_cfg_data_Certificate(*val, opts);

  else if (!strcasecmp(*var, defCfgRealmInterface))
    rc = interpret_cfg_data_RealmInterface(*val, realm);

  else
    rc = 6; // unknown option

error:
  free(*var);
  *var = NULL;
  strop_free(*val);
  *val = NULL;
  return rc;
}
//------------------------------------------------------------------------------------------------------------
void free_all_options(strOptionsGlobal **opts) {
  strOptionsRealm *realm;
  strOptionsMySQL *mysql;
  int              i;

  if(!*opts)
    return;

  for(i=0;i<(*opts)->realms_count;i++) {
    realm = &((*opts)->realms[i]);
    if(realm->realm_name)
      free(realm->realm_name);
    // Add realm specific free_all_options_<YourOption> here:
    free_all_options_Mailspool(realm);
    free_all_options_RealmInterface(realm);
  } // ~for

  for(i=0;i<(*opts)->mysql_count;i++) {
    mysql = &((*opts)->mysql[i]);
    assert(mysql->ID);
    free(mysql->ID);
    // Add mysql specific free_all_options_<YourOption> here:
    free_all_options_mysql_Host(mysql);
    free_all_options_mysql_User(mysql);
    free_all_options_mysql_Pass(mysql);
    free_all_options_mysql_Db(mysql);
    free_all_options_mysql_Table(mysql);
    free_all_options_mysql_UserColumn(mysql);
    free_all_options_mysql_PassColumn(mysql);
    free_all_options_mysql_And(mysql);
    free_all_options_mysql_Homedir(mysql);
    free_all_options_mysql_Uid(mysql);
    free_all_options_mysql_Gid(mysql);
  }// ~for

  // Add global free_all_options_<YourOption> here:
  free_all_options_RealmChars(*opts);
  free_all_options_Certificate(*opts);
  free((*opts)->mysql);
  free((*opts)->realms);
  free(*opts);
  *opts = NULL;
  return;
}
//------------------------------------------------------------------------------------------------------------
void dump_cfg_data(FILE *fd, strOptionsGlobal *opts) {
  strOptionsRealm *realm;
  strOptionsMySQL *mysql;
  char            *fill = NULL;
  int              i;

  if (!opts)
    return;

  // Dump of global options.
  dump_cfg_data_Debug(fd, opts);
  dump_cfg_data_Inetd(fd, opts);
  dump_cfg_data_RealmChars(fd, opts);
  dump_cfg_data_Certificate(fd, opts);
  dump_cfg_data_ListenOn(fd, opts);

  for(i=0; i<opts->mysql_count; i++) { // loop thru mysql blocks
    mysql = &(opts->mysql[i]);
    fprintf(fd,"\nmysql \"%s\" {\n",mysql->ID);
    // Dump of mysql options.
    dump_cfg_data_mysql_Host(fd, mysql);
    dump_cfg_data_mysql_User(fd, mysql);
    dump_cfg_data_mysql_Pass(fd, mysql);
    dump_cfg_data_mysql_Db(fd, mysql);
    dump_cfg_data_mysql_Table(fd, mysql);
    dump_cfg_data_mysql_UserColumn(fd, mysql);
    dump_cfg_data_mysql_PassColumn(fd, mysql);
    dump_cfg_data_mysql_And(fd, mysql);
    dump_cfg_data_mysql_Homedir(fd, mysql);
    dump_cfg_data_mysql_Uid(fd, mysql);
    dump_cfg_data_mysql_Gid(fd, mysql);
    fprintf(fd, "}\n");
  }

  fprintf(fd, "\n");

  for(i=0;i<opts->realms_count;i++) { // loop thru realms
    realm = &(opts->realms[i]);
    if(realm->realm_name) {
      fill = "\t";
      fprintf(fd,"\nrealm \"%s\" {\n",realm->realm_name);
    } else {
      fill = "";
    }
    // Dump of per-realm options
    dump_cfg_data_AuthCmd(fd, fill, realm);
    dump_cfg_data_AuthDb(fd, fill, realm);
    dump_cfg_data_Mailspool(fd, fill, realm);
    dump_cfg_data_MailspoolOwner(fd, fill, realm);
    dump_cfg_data_RealmInterface(fd, fill, realm);
    if(realm->realm_name)
      fprintf(fd, "}\n");
  } // ~for

  return;
}
//------------------------------------------------------------------------------------------------------------
strOptionsRealm *add_realm(char *realm_name, strOptionsGlobal *opts) {
  strOptionsRealm *def = NULL;
  strOptionsRealm *last;

  if(!opts)
    return NULL;

  last = realloc(opts->realms,(opts->realms_count+1)*sizeof(*(opts->realms)));
  if(!last)
    return NULL;
  opts->realms_count++;
  opts->realms = last;

  last = &(opts->realms[opts->realms_count-1]);
  memset(last, 0x00, sizeof(last[0]));

  if(realm_name) { //duplicate values from 'def'
    def = get_realm(NULL, opts);
    if(!def)
      return NULL;

    last->realm_name = strdup(realm_name);
    if(!last->realm_name)
      return NULL;

    // copy per realm options from default
    last->auth_cmd = def->auth_cmd;
    if(!add_realm_Mailspool(last, def))
      return NULL;
    if(!add_realm_AuthDb(last, def))
      return NULL;
    if(!add_realm_RealmInterface(last, def))
      return NULL;
    if(!add_realm_MailspoolOwner(last, def))
      return NULL;

  } else { // insert default (#defined) values

    last->auth_cmd = defAuthCmdUser;
    if(!add_realm_Mailspool(last, NULL))
      return NULL;
    if(!add_realm_AuthDb(last, NULL))
      return NULL;
    if(!add_realm_RealmInterface(last, NULL))
      return NULL;
    if(!add_realm_MailspoolOwner(last, def))
      return NULL;

  } // ~if(realm_name)

  return last;
}
//------------------------------------------------------------------------------------------------------------
strOptionsGlobal *create_global() {
  strOptionsGlobal *opts;
  opts = safe_malloc(sizeof(*opts));
  if(!opts) return NULL;
  memset(opts, 0x00, sizeof(*opts));
  // set global options.
  opts->debug = 1; // default debug level is 1
  opts->inetd = 0; // inetd disabled
  return opts;
}
//------------------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------------------
// ** Do NOT change functions beyond this line when adding new options! **
//------------------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------------------
strOptionsGlobal *cfg_read_config(const char *conf_file) {
  FILE             *fd = NULL;
  strOptionsRealm  *realm = NULL;
  strOptionsRealm  *def_realm = NULL;
  strOptionsMySQL  *mysql = NULL;
  strOptionsGlobal *opts = NULL;
  strValues        *vals = NULL;
  char             *var = NULL;
  char             *buf = NULL;
  char             *block_name;
  int               rc = 0;
  int               tmp = 0;
  int               line = 0;

  fd = fopen(conf_file,"rb");
  if(!fd) {
    err_error(EX_OSFILE, "Unable to open config file: \"%s\"", conf_file);
    goto error;
  }

  buf = safe_malloc(2*defCfgMaxLine); // allocate multiple buffers of size defCfgMaxLine
  if (!buf) {
    err_malloc_error();
    goto error;
  }
  block_name = &buf[defCfgMaxLine];

  opts = create_global();
  def_realm = add_realm(NULL, opts); // create "default" realm
  realm = def_realm;

  if (!realm) {
    err_malloc_error();
    goto error;
  }

  rc = get_cfg_line(fd, buf, &line, conf_file);
  if (rc < 0)
    goto error;


  // analyse every line...
  while(buf[0]) {

    // start of a block ?
    tmp = get_block(buf, block_name);

    // Parameter error in call to get_block()
    if(defBlockParameterError == tmp) {
      err_internal_error();
      goto error;
    }

    // Syntax error in block definition
    else if(defBlockSyntaxError == tmp) {
      err_error(EX_OSFILE, "%s(%d): Syntax error", conf_file, line);
      goto error;
    }

    // Found end of block
    else if(defBlockEnd == tmp) {
      if(mysql) {
        mysql = NULL;
      }
      else if(realm != def_realm) {
        realm = def_realm;
      }
      else {
        err_error(EX_OSFILE, "%s(%d): Syntax error", conf_file, line);
        goto error;
      }
    }

    // Found block "realm"
    else if(defBlockRealm == tmp) {
      if(realm != def_realm) {
        err_error(EX_OSFILE, "%s(%d): Nesting of realms is not supported", conf_file, line);
        goto error;
      }
      if(mysql) {
        err_error(EX_OSFILE, "%s(%d): Found realm start in mysql block", conf_file, line);
        goto error;
      }
      realm = get_realm(block_name, opts);
      if(!realm)
        realm = add_realm(block_name, opts);
      if(!realm) {
        err_malloc_error();
        goto error;
      }
    }

    // Found block "mysql"
    else if(defBlockMySQL == tmp) {
      if(mysql) {
        err_error(EX_OSFILE, "%s(%d): Nesting of mysql blocks is not supported", conf_file, line);
        goto error;
      }
      mysql = get_mysql(block_name, opts);
      if(!mysql)
        mysql = add_mysql(block_name, opts);
      if(!mysql) {
        err_malloc_error();
        goto error;
      }
    }

    // Not a block definition.. most probably a regular option
    else if (defBlockUNKNOWN == tmp) {
      rc = strop_varval(buf, &var, &vals);
      while(defExpectMoreData == rc) {
        rc = get_cfg_line(fd, buf, &line, conf_file);
        if (rc < 0)
          goto error;
        rc = strop_varval(buf, &var, &vals);
      }
      switch(rc) {
      case defNoError:
          break;
      case defMallocError:
          err_malloc_error();
          goto error;
          break;
      case defSyntaxError:
          err_error(EX_OSFILE, "%s(%d): Syntax error", conf_file, line);
          goto error;
          break;
      case defInvalidParameter:
          err_internal_error(); // invalid parameter in call to strop_varval(). That's an internal problem!!
          goto error;
          break;
      }

      // interpret tokenized strings as data
      if(!mysql)
        rc = interpret_cfg_data_realm(&var, &vals, realm, opts);
      else
        rc = interpret_cfg_data_mysql(&var, &vals, mysql, opts);

      if (rc) {
        err_error(EX_OSFILE, "%s(%d): %s", conf_file, line, options_errors[rc]);
        goto error;
      }

    } // defBlockUNKNOWN

    rc = get_cfg_line(fd, buf, &line, conf_file);
    if (rc < 0)
      goto error;
  }

  if (fd) fclose(fd);
  if (buf) free(buf);
  return opts;

error:
  if (fd) fclose(fd);
  if (opts) free_all_options(&opts);
  if (buf) free(buf);
  return NULL;
}
//------------------------------------------------------------------------------------------------------------
int get_cfg_line(FILE *fd, char *buf, int *line, const char *conf_file) {
  int i = 0;
  if(!fd || !buf || !line) {
    err_internal_error();
    return -1;
  }
  buf[0] = '\0';
  while(fgets(buf,defCfgMaxLine,fd)) {
    (*line)++;
    if(strlen(buf) == defCfgMaxLine-1) {
      err_error(EX_OSFILE, "%s(%d): Line too long", conf_file, *line);
    }
    for(i=0;isblank(buf[i]);i++); // eat blanks
    if('#'!=buf[i] && '\n'!=buf[i])
      break;
  } // ~fgets
  if('#'==buf[i] || '\n'==buf[i]) // if there is a blank line or comment on the last line
    buf[0]='\0';
  return 0;
}
//------------------------------------------------------------------------------------------------------------
strOptionsRealm *get_realm(const char *realm_name,strOptionsGlobal *opts) {
  strOptionsRealm *realm;
  int              i;

  err_debug(5, "%s(%s)", __FUNCTION__, realm_name);

  if(!opts) return NULL;
  for(i=0;i<opts->realms_count;i++) {
    realm=&opts->realms[i];
    if (!realm_name && !realm->realm_name) {
        return realm; // default realm
    }
    if(realm_name && realm->realm_name) {
      if(!strcasecmp(realm_name,realm->realm_name))
        return realm; // named realm
    }
  }
  return NULL;
}
//------------------------------------------------------------------------------------------------------------
/*
int end_block(char *buf) {
  int i;
  if(!buf)
    return 0;
  for (i=0; isblank(buf[i]); i++);
  if('}'!=buf[i] || !buf[i])
    return 0;
  i++;
  for (;isspace(buf[i]); i++);
  if(buf[i])
    return 0;
  return 1;
  }
  */
//------------------------------------------------------------------------------------------------------------
int interpret_cfg_data_port(const char *src, int *dst) {
  struct servent *serv = NULL;
  int             i;

  for(i=0; isdigit(src[i]); i++); 
  if(!src[i]) { // numbers only.. this is a numeric port specification
    *dst = atoi(src);
    return 0;
  }

  serv = getservbyname(src, "tcp"); // port specified by name
  if(!serv) {
    err_debug(0, "Can't locate service/port: \"%s\".", src);
    return 6;
  }
  *dst = serv->s_port;

  return 0;
}
//------------------------------------------------------------------------------------------------------------
int interpret_cfg_data_int(const char *src, int *dst) {
  int i;

  for(i=0; isdigit(src[i]); i++);
  if(!src[i]) {
    *dst = atoi(src);
    return 0;
  }

  return 6;
}
//------------------------------------------------------------------------------------------------------------
int interpret_cfg_data_string(const char *src, char **dst) {
  if(*dst)
    free(*dst);
  *dst = strdup(src);
  if(!*dst)
    return 2;
  return 0;
}
//------------------------------------------------------------------------------------------------------------
int interpret_cfg_data_bool(const char *src, int *dst) {
  if(!strcasecmp(src, defCfgYES1) ||
     !strcasecmp(src, defCfgYES2) ||
     !strcasecmp(src, defCfgYES3))
  {
    *dst=1;
    return 0;
  }
  if(!strcasecmp(src, defCfgNO1) ||
     !strcasecmp(src, defCfgNO2) ||
     !strcasecmp(src, defCfgNO2))
  {
    *dst=0;
    return 0;
  }
  return 6;
}
//------------------------------------------------------------------------------------------------------------
int cfg_interface_get(struct sockaddr_in *sa, strValues *val, int type) {
  struct servent *serv = NULL;
  struct hostent *host = NULL;
  int i;

  sa->sin_family = AF_INET;

  // host = getipnodebyname(tmp->val, AF_INET, 0, &rc);
  host = gethostbyname(val->val);
  if(!host) {
    goto error;
  }
  if(!host->h_addr_list || 4!=host->h_length || AF_INET != host->h_addrtype) {
    goto error;
  }
  sa->sin_addr.s_addr = *(( u_int32_t *)(host->h_addr_list[0]));
  //freehostent(host);
  host = NULL;

  if(!val->opt_count) { // no port is specified
    serv = type ?
        getservbyname(defPOP3SPortName, "tcp") :
        getservbyname(defPOP3PortName, "tcp");

    if(!serv) { // service name lookup failed
      sa->sin_port = type ?
          htons(defPOP3SPort) :
          htons(defPOP3Port);
      return 0;
    } // ~if

    sa->sin_port = serv->s_port;
    return 0;
  }

  for(i=0; isdigit((val->opt[0])[i]); i++);
  if(!(val->opt[0])[i]) { // port specified numericaly
    sa->sin_port = htons(atoi(val->opt[0]));
    return 0;
  }

  serv = getservbyname(val->opt[0], "tcp"); // port specified by name
  if(!serv) {
    err_debug(0, "Bad port/service: \"%s:%s\".", val->val, val->opt[0]);
    return 6;
  }
  sa->sin_port = serv->s_port;

  return 0;

error:
  err_debug(0, "Unknown interface specification at \"%s\".", val->val);
  return 6;
}
//------------------------------------------------------------------------------------------------------------
int cfg_interface_check(struct sockaddr_in *sa, int count) {
  int i;
  int j;

  // check for duplicate interfaces.
  for(i=1; i<count; i++) {
    for(j=0; j<i; j++) {
      if(sa[i].sin_family      == sa[j].sin_family &&  // memcmp isn't used becouse of hidden fields
         sa[i].sin_port        == sa[j].sin_port   &&
         sa[i].sin_addr.s_addr == sa[j].sin_addr.s_addr
        )
      {
        err_debug(0,"Duplicate interface %s (%d of %d).",
                  cfg_interface_print(&sa[i]), j, count );
        return 6;
      } // ~if
    } // ~for
  } // ~for

  return 0;
}
//------------------------------------------------------------------------------------------------------------
char *cfg_interface_print(struct sockaddr_in *sa) {
  static char buf[INET_ADDRSTRLEN + 256]; // 640K^H^H^H^H256 is more than enough for everyone.
  int siz;

  if(!inet_ntop(AF_INET, &sa->sin_addr, buf, sizeof(buf))) {
    err_internal_error();
    return NULL;
  }
  siz = strlen(buf);
  snprintf(&buf[siz], sizeof(buf)-siz, ":%d", htons(sa->sin_port));
  return buf;
}
//------------------------------------------------------------------------------------------------------------
/*
#define defBlockSyntaxError      -2
#define defBlockParameterError   -1
#define defBlockUNKNOWN          0
#define defBlockRealm            1
#define defBlockMySQL            2
*/
int get_block(const char *buf, char *block_name) {
  int rc;
  int i;
  int j   = 0;
  int esc = 0;

  if(!buf) {
    err_internal_error();
    return defBlockParameterError;
  }

  for(i=0; isblank(buf[i]); i++); // eat blanks

  // is it an end of block ?
  if('}' == buf[i]) {
    for (j=i+1; isspace(buf[j]); j++); // ignore trailing spaces
    if(!buf[j])
      return defBlockEnd;
    j = 0;
  }

  // realm starts with "realm" keywoard
  if(!strncasecmp(&buf[i], defBlockRealmStr, sizeof(defBlockRealmStr)-1)) {
    rc = defBlockRealm;
    i += sizeof(defBlockRealmStr)-1;
  }
  // mysql block starts with "mysql" keywoard
  else if(!strncasecmp(&buf[i], defBlockMySQLStr, sizeof(defBlockMySQLStr)-1)) {
    rc = defBlockMySQL;
    i += sizeof(defBlockMySQLStr)-1;
  }
  // nothing of that
  else
    return defBlockUNKNOWN;

  if(!isblank(buf[i])) // followed by at least one blank if not that this is not our keywoard.
    return defBlockUNKNOWN;

  for(;isblank(buf[i]);i++); // eat remaining blanks

  if('"' == buf[i]) { // is a block name quoted ?
    while(buf[i]) { // handle quoted string copying.
      i++;
      if('\\' == buf[i] && !esc) { // set escape mode, if not in it already
        esc = 1;
        continue;
      }
      if('"' == buf[i] && !esc) { // end of string
        i++;
        break;
      }

      block_name[j] = buf[i]; // copy character
      j++;
      esc = 0;
    }
  } else {
    for(j=0;isgraph(buf[i]);i++,j++) // copy unqoted block name to buffer.
      block_name[j] = buf[i];
  }
  block_name[j] = '\0';

  for(;isblank(buf[i]);i++); // eat remaining blanks.
  if ('{' != buf[i] || !buf[i])
    return defBlockSyntaxError;
  i++;
  for(;isblank(buf[i]);i++); // eat remaining blanks.
  if (buf[i] && '\n'!=buf[i])
    return defBlockSyntaxError; // syntax error?

  return rc;
}
//------------------------------------------------------------------------------------------------------------
/*
// rc:
// -1 - incorrect parameter/error
// 0 - realm keywoard
// 1 - it isn't realm
// 2 - syntax error
// BUGBUG ... fix this mess.
int realm_start(char *buf,char *realm_name) {
  int i;
  int j   = 0;
  int esc = 0;

  if(!buf || !realm_name) {
    err_internal_error();
    return -1;
  }

  for(i=0; isblank(buf[i]); i++); // eat blanks
  if(strncasecmp(&buf[i], defBlockRealmStr, sizeof(defBlockRealmStr)-1)) // realm starts with "realm" keywoard
    return 1;
  i += sizeof(defBlockRealmStr)-1; // skip realm keywoard
  if(!isblank(buf[i])) // "realm" is followed by at least one blank.
    return 1;
  for(;isblank(buf[i]);i++); // eat remaining blanks

  if('"'==buf[i]) { // is realm name quoted ?
    while(buf[i]) { // handle quoted string copying.
      i++;
      if('\\'==buf[i] && !esc) { // set escape mode, if not in it already
        esc = 1;
        continue;
      }
      if('"'==buf[i] && !esc) { // end of string
        i++;
        break;
      }

      realm_name[j] = buf[i]; // copy character
      j++;
      esc = 0;
    }
  } else {
    for(j=0;isgraph(buf[i]);i++,j++) // copy unqoted realm name to buffer.
      realm_name[j] = buf[i];
  }
  realm_name[j] = '\0';

  for(;isblank(buf[i]);i++); // eat remaining blanks.
  if ('{'!=buf[i] || !buf[i])
    return 2; // syntax error?
  i++;
  for(;isblank(buf[i]);i++); // eat remaining blanks.
  if (buf[i] && '\n'!=buf[i])
    return 2; // syntax error?
  return 0;
}
 */
//------------------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------------------
strOptionsMySQL *add_mysql(char *mysql_name, strOptionsGlobal *opts) {
  strOptionsMySQL *mysql = NULL;

  if(!opts || !mysql_name)
    return NULL;

  if(!mysql_name[0]) // empty string not allowed.
    return NULL;

  mysql = realloc(opts->mysql,(opts->mysql_count+1)*sizeof(*(opts->mysql)));
  if(!mysql)
    return NULL;
  opts->mysql_count++;
  opts->mysql = mysql;

  mysql = &(opts->mysql[opts->mysql_count-1]);
  memset(mysql, 0x00, sizeof(*mysql));

  mysql->ID = strdup(mysql_name);
  if(!mysql->ID) {
    opts->mysql_count--;
    return NULL;
  }

  return mysql;
}
//------------------------------------------------------------------------------------------------------------
strOptionsMySQL *get_mysql(const char *mysql_name, strOptionsGlobal *opts) {
  strOptionsMySQL *mysql;
  int              i;

  err_debug_function();

  if(!mysql_name || !opts)
    return NULL;

  for(i=0;i<opts->mysql_count;i++) {
    mysql = &opts->mysql[i];
    if(!strcasecmp(mysql_name, mysql->ID))
      return mysql;
  }

  return NULL;
}
//------------------------------------------------------------------------------------------------------------
//TODO: add interpreter for both listen_on and realm_interfaces!
//------------------------------------------------------------------------------------------------------------
