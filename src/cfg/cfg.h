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

#ifndef __CFG_H__
#define __CFG_H__

#include <stdio.h>
#include <sys/types.h>
#include <netinet/in.h>
#include "strop.h"

#ifndef defConfigFile
#define defConfigFile            "/etc/pulsar.conf"
#endif

#ifndef defMailspool
#define defMailspool             "/var/spool/mail"
#endif

#ifndef defCfgMaxLine
#define defCfgMaxLine            256
#endif

#ifndef defPOP3Port
#define defPOP3Port              110
#endif

#ifndef defPOP3SPort
#define defPOP3SPort             995
#endif

#define defPOP3PortName          "pop3"
#define defPOP3SPortName         "pop3s"

#define defBlockParameterError   -2
#define defBlockSyntaxError      -1
#define defBlockUNKNOWN          0
#define defBlockEnd              1
#define defBlockRealm            2
#define defBlockMySQL            3
#define defBlockRealmStr         "realm"
#define defBlockMySQLStr         "mysql"

#define defCfgDebug              "debug"
#define defCfgInetd              "inetd"
#define defCfgMailspool          "mailspool"
#define defCfgMailspoolOwner     "mailspool_owner"
#define defCfgMboxCache          "mbox_cache"
#define defCfgEnableSqlite       "enable_sqlite"
#define defCfgCertificate        "certificate"
#define defCfgAuthCmd            "auth_cmd"
#define defCfgAuthDb             "auth_db"
#define defCfgRealmChars         "realm_chars"
#define defCfgListenOn           "listen_on"
#define defCfgSSLListenOn        "ssl_listen_on"
#define defCfgRealmInterface     "realm_interface"

#define defCfgMySQLHost          "host"
#define defCfgMySQLUser          "user"
#define defCfgMySQLPass          "pass"
#define defCfgMySQLDb            "db"
#define defCfgMySQLTable         "table"
#define defCfgMySQLUserColumn    "user_column"
#define defCfgMySQLPassColumn    "pass_column"
#define defCfgMySQLAnd           "and"
#define defCfgMySQLHomedir       "homedir"
#define defCfgMySQLUid           "uid"
#define defCfgMySQLGid           "gid"

#define defCfgYES1               "yes"
#define defCfgYES2               "on"
#define defCfgYES3               "1"
#define defCfgNO1                "no"
#define defCfgNO2                "off"
#define defCfgNO3                "0"

#define defAuthCmdUserStr        "USER"
#define defAuthCmdUser           0x01
#define defAuthCmdApopStr        "APOP"
#define defAuthCmdApop           0x02

#define defAuthDbUnixStr         "unix"
#define defAuthDbUnixEnum        0
#define defAuthDbFileStr         "file"
#define defAuthDbFileEnum        1

#ifdef  WITH_PAM
#define defAuthDbPAMStr          "PAM"
#define defAuthDbPAMEnum         2
#endif  /* WITH PAM */

#ifdef  WITH_MYSQL
#define defAuthDbMySQLStr        "mysql"
#define defAuthDbMySQLEnum       3
#endif  /* WITH MYSQL */

// mailspool_type
#define defMailspoolHome         1  // Search for mailbox in user home dir
#define defMailspoolMaildir      2  // Maildir format
#define defMailspoolSqlite       4  // Sqlite mbox

#define defPassHashPlainStr      "plaintext"
#define defPassHashPlain         0
#define defPassHashCryptStr      "crypt"
#define defPassHashCrypt         1
#define defPassHashMax           2

// interfaceface type (bitwise!)
#define defIfaceSSL              0x01

// complex configuration data types
typedef struct _strIfaces {
  int                 count; // number of elements in type and sa arrays
  int                *type;
  struct sockaddr_in *sa;
} strIfaces;

typedef struct _strPassHash {
  int count;
  int hash[defPassHashMax];
} strPassHash;

typedef struct _strAuthDb {
  int          type;
  char        *filename; // used as ID if type == mysql
  strPassHash  hash;     // used with 'file' ath type. MySQL has its own.
} strAuthDb;

typedef struct _strMailspoolOwner {
  int   uid_set;
  int   gid_set;
  uid_t uid;
  gid_t gid;
} strMailspoolOwner;

// mysql options
typedef struct _strOptionsMySQL {
  char        *ID;
  char        *host;
  int          port;
  char        *user;
  char        *pass;
  char        *db;

  char        *table;
  char        *user_column;
  char        *pass_column;
  strPassHash  pass_hash;

  char        *and;
  char        *homedir;
  char        *uid;
  char        *gid;
  // TODO: add alt. query template
} strOptionsMySQL;

typedef struct _strMailspool {
  char         *dir;
  unsigned int  type;
  unsigned int  mode;
} strMailspool;

// realm specific options
typedef struct _strOptionsRealm {
  char               *realm_name;
  int                 auth_cmd;
  strIfaces           ifaces;

  strMailspool        mailspool;
  strMailspoolOwner   mailspool_owner;

  int                 sqlite_enable;

  int                 auth_db_count;
  strAuthDb          *auth_db;
} strOptionsRealm;

// global options
typedef struct _strOptionsGlobal {
  char               *realm_chars;
  int                 debug;
  int                 inetd;
  strIfaces           ifaces;
  char               *cert_file; // certificate file
  char               *key_file;  // if NULL use cert_file as key_file.

  int                 realms_count;
  strOptionsRealm    *realms;
  int                 mbox_cache_enable;

  int                 mysql_count;  // Number of mysql blocks defined.
  strOptionsMySQL    *mysql;
} strOptionsGlobal;

/* Public function */
strOptionsGlobal *cfg_read_config(const char *conf_file);
void dump_cfg_data(FILE *fd,strOptionsGlobal *opts);
void free_all_options(strOptionsGlobal **opts);
strOptionsRealm *get_realm(const char *realm_name, strOptionsGlobal *opts);
strOptionsMySQL *get_mysql(const char *mysql_name, strOptionsGlobal *opts);

/* Private functions for internal use */
strOptionsGlobal *create_global();
strOptionsRealm  *add_realm(char *realm_name, strOptionsGlobal *opts);
strOptionsMySQL  *add_mysql(char *mysql_name, strOptionsGlobal *opts);
int get_block(const char *buf, char *block_name);
int end_block(char *buf);
int get_cfg_line(FILE *fd, char *buf, int *line, const char *conf_file);

int interpret_cfg_data_realm(char **var, strValues **val, strOptionsRealm *realm, strOptionsGlobal *opts);
int interpret_cfg_data_mysql(char **var, strValues **val, strOptionsMySQL *mysql, strOptionsGlobal *opts);

int interpret_cfg_data_string   (const char *src, char **dst);
int interpret_cfg_data_int      (const char *src, int   *dst);
int interpret_cfg_data_bool     (const char *src, int   *dst);
int interpret_cfg_data_port     (const char *src, int   *dst);
int interpret_cfg_data_pass_hash(int from, strValues *val, strPassHash *pass_hash);

int   cfg_interface_get  (struct sockaddr_in *sa, strValues *val, int type);
int   cfg_interface_check(struct sockaddr_in *sa, int count);
char *cfg_interface_print(struct sockaddr_in *sa);

#else

#ifdef DEBUG
#warning file "cfg.h" already included.
#endif /* DEBUG */

#endif /* __CFG_H__ */
