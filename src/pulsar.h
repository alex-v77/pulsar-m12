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

#ifndef __PULSAR_H__
#define __PULSAR_H__

#include <sys/types.h>
#include <poll.h>
#include <netdb.h>
#include <netinet/in.h>

#ifdef WITH_SSL
#include <openssl/ssl.h>
#endif /* WITH_SSL */

#include "cfg.h"
#include "mailstore.h"

#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX 1024
#endif

#define defDefaultDebugLevel 1
#define defBufSize           (4*1024)      // default buffer size and increment of receive buffer.
#define defBufMin            256           // minimum buffer free before we allocate a new one.
#define defBufMax            (32*1024)     // maximum buffer size
#define defPollMinute        (60*1000)
#define defMaxFileUsers      (5000)        // maximum numbers of users in a auth_db = file
#define defBacklog           3             // tcp/listen backlog size

#define defCredPass          0
#define defCredApop          1

#define defAUTH              0 // AUTHORIZATION
#define defCONN              1 // TRANSACTION
#define defUPDATE            2 // UPDATE
#define defABORT             3

#define defOK                0
#define defOKMsg             "+OK"
#define defERR               1
#define defERRMsg            "-ERR"
#define defMalloc            2
#define defMallocMsg         "-ERR Out of memory."
#define defBadcmd            3
#define defBadcmdMsg         "-ERR Invalid command."
#define defMailbox           4
#define defMailboxMsg        "-ERR Unable to open mailbox."
#define defAuth              5
#define defAuthMsg           "-ERR Authentication failed."
#define defParam             6
#define defParamMsg          "-ERR Invalid parameter!"
#define defDeleted           7
#define defDeletedMsg        "-ERR Message already deleted!"
#define defConf              8
#define defConfMsg           "-ERR server misconfiguration error"
#define defMailboxLocked     9
#define defMailboxLockedMsg  "-ERR Mailbox locked. Try later."
#define defNONE              10 // string was handled internaly by command processing function

typedef struct _strStaticData {
  strOptionsGlobal   *cfg;
  char                hostname[HOST_NAME_MAX+1]; // FQDN

  // server specific data (only needed when run in stand-alone "tcp-server" mode)
  int                 socket_count; // matches g->cfg.ifaces.count
  struct pollfd      *polls;

  // data associated with request
  int                 state;        // state of the client connection (AUTH, TRANSACTION, ...)
  int                 fd_in;
  int                 fd_out;
  int                 cmd_size;
  char               *pop3_cmd;
  char               *pop3_arg;

  // OpenSSL structures
  int                 ssl;          // is SSLv23 type or not ? Also set by STLS command.
  #ifdef WITH_SSL     // TODO: create SSL/TLS structure
  SSL_METHOD         *ssl_method;
  SSL_CTX            *ssl_ctx;
  SSL                *ssl_con;
  #endif /* WITH_SSL */

  // for POP3 cmd replies use!
  // Mailstore modules *may* use this buffer.
  char                tx_buf[defBufSize];

  // client request specific data
  char                apop_string[HOST_NAME_MAX+1 + defBufSize]; // TODO: alloc() ?
  char               *user;
  char               *realm_name;
  char               *homedir;
  char               *mailbox_path;
  strMailspoolOwner   owner;

  struct sockaddr_in  remote;
  socklen_t           remote_len;
  struct sockaddr_in  local;
  socklen_t           local_len;

  // *the* selected realm
  strOptionsRealm    *realm;

  // mail hash structure
  strMailstoreHead   *head;
} strStaticData;

extern strStaticData g;

void help(const char *filename);
int  signal_init();
int  pulsar_main();
int  pulsar_main_read();
int  pulsar_main_checkcmd(const char *match);

/* functions coresponding to POP3 commands. File: pulsar_'CMD'.c */
int  pulsar_badcmd();
int  pulsar_quit();
int  pulsar_stat();
int  pulsar_list();
int  pulsar_uidl();
int  pulsar_noop();
int  pulsar_retr();
int  pulsar_capa();
int  pulsar_stls();
int  pulsar_top();
int  pulsar_dele();
int  pulsar_rset();
int  pulsar_user();
int  pulsar_pass();
int  pulsar_apop();

/* pulsar_misc.c */
int   init_ssl(); // ssl
int   ssl_on();   // ssl
int   pulsar_printf(const char *format, ...);
int   pulsar_tcpwrap_check();
int   pulsar_auth(int type, const char *credentials);

strOptionsRealm *get_realm_by_interface();

#else

#ifdef DEBUG
#warning file "pulsar.h" already included.
#endif /* DEBUG */

#endif
