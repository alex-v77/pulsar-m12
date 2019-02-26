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
#define  __USE_GNU // this is needed for memmem in string.h
#include <string.h>
#undef   __USE_GNU
#include <ctype.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>

#include "pulsar.h"

#include "util.h"
#include "error_facility.h"

//------------------------------------------------------------------------------------------------------------
int pulsar_main_read() {
  struct pollfd  pfd;
  char          *tmp;
  char          *seek;
  char          *found;
  int            bytes_read = 0;
  int            bytes_size = defBufSize;
  int            rc;
  int            i;

  err_debug_function();

  // TODO: add check if we realy need to allocate a new buffer...
  free(g.pop3_cmd);      // do some more cleanups/optimizers
  g.pop3_cmd = NULL;

  tmp = safe_malloc(defBufSize+1);
  if (!tmp) {
    err_malloc_error();
    goto error;
  }

  seek = tmp; // seek will search for \n
  pfd.fd = g.fd_in;
  pfd.events = POLLIN;

  while(1) {
    do {
      pfd.revents = 0; // is this needed ?!
      err_debug(6, "Will start poll() and wait for %d seconds", 10*defPollMinute/1000);
      rc = poll(&pfd, 1, 10*defPollMinute);
    } while(-1 == rc && errno == EINTR);
    if(-1 == rc) {
      err_error(EX_OSERR, "Error in %s - poll(): %s", __FUNCTION__, strerror(errno));
      goto error;
    }

    if(!rc) {
      err_error(EX_IOERR, "Client timeout!");
      goto error;
    }

    if(rc && pfd.revents&POLLIN) { // data is waiting.
      rc = net_partial_read(g.fd_in, &tmp[bytes_read], bytes_size - bytes_read);
      if (-1 == rc) {
        err_error(EX_OSERR, "Error in %s - read(): %s", __FUNCTION__, strerror(errno));
        goto error;
      }
      if(!rc) {
        err_error(EX_IOERR, "No data. Connection lost ?");
        goto error;
      }
      bytes_read += rc;
    } else {
      err_internal_error();
      assert(0);
      goto error;
    }

    found = memchr(seek,'\n',tmp + bytes_read - seek); // search for newline!
    if (found)
      break;

    if(bytes_size - bytes_read < defBufMin) { // more buffer space for data might be needed
      if(bytes_size + defBufSize >= defBufMax) {
        err_error(EX_DATAERR,"POP3 request exceeds maximum allowed buffer size of %d KiB!",defBufMax/1024);
        goto error;
      }
      bytes_size += defBufSize;
      seek = realloc(tmp, bytes_size+1);
      if (!seek) {
        err_malloc_error();
        goto error;
      }
      tmp = seek;
    }

    seek = tmp + bytes_read;
  } // ~while

  tmp[bytes_read] = '\0';
  g.pop3_cmd = tmp;
  g.pop3_arg = NULL;
  err_debug(4,"Received: %s",tmp);

  // locate arguments
  for(i=0;i<bytes_read;tmp++) {
    if(tmp[0]==' ' && !g.pop3_arg) { // possible parameter
      tmp[0] = '\0';
      g.pop3_arg = tmp + 1;
      g.cmd_size = g.pop3_arg - g.pop3_cmd - 1;
    }
    if(tmp[0]=='\n' || tmp[0]=='\r') {
      tmp[0] = '\0';
      if(!g.pop3_arg) {
        g.cmd_size = tmp - g.pop3_cmd;
        g.pop3_arg = tmp;
      }
      break;
    }
  }
  return 0;

error:
  free(tmp);
  g.pop3_cmd = NULL;
  return -1;
}
//------------------------------------------------------------------------------------------------------------
// match must be all in uppercase
// rc == 0 -> no match
// rc == 1 -> match
int pulsar_main_checkcmd(const char *match) {
  int i;
  int match_size;

  match_size = strlen(match);
  if(g.cmd_size<match_size)
    return 0;

  for(i=0;i<match_size;i++)
    if(toupper(g.pop3_cmd[i])!=match[i])
      return 0;

  if(g.pop3_cmd[i])
    return 0;

  err_debug(3,"cmd(%s) = %s %s",g.user ? g.user : "unknown user",g.pop3_cmd,g.pop3_arg);
  return 1;
}
//------------------------------------------------------------------------------------------------------------
int pulsar_main() {
  int  rc;

  err_debug_function();

  g.head = NULL;
  g.pop3_cmd = NULL;
  g.state = defAUTH;

  rc = gethostname(g.hostname, sizeof(g.hostname)); // TODO: Move this up-stream so it is called sooner.
  assert(!rc);
  err_debug(3, "hostname is: %s", g.hostname);

  // select default or interface based realm.
  // we need to do it here so we can process auth_cmd options
  g.realm = get_realm_by_interface();
  if(!g.realm)
    g.realm = get_realm(NULL, g.cfg); // default realm
  assert(g.realm);

  // create APOP challange string only if APOP is selected
  if(g.realm->auth_cmd & defAuthCmdApop) {
    rc = snprintf(g.apop_string, sizeof(g.apop_string),
                  "<%d.%ld@%s>",
                  getpid(), time(NULL), g.hostname
                 );
    assert(-1 < rc && rc < sizeof(g.apop_string));
  } else {
    g.apop_string[0] = '\0';
  }

  pulsar_printf("+OK %s", g.apop_string);


  while(defUPDATE != g.state && defABORT != g.state) {
    rc = pulsar_main_read();
    if (-1 == rc)
      goto error;

    if(pulsar_main_checkcmd("QUIT"))
      rc = pulsar_quit();
    else if(pulsar_main_checkcmd("APOP"))
      rc = pulsar_apop();
    else if(pulsar_main_checkcmd("USER"))
      rc = pulsar_user();
    else if(pulsar_main_checkcmd("PASS"))
      rc = pulsar_pass();
    else if(pulsar_main_checkcmd("NOOP"))
      rc = pulsar_noop();
    else if(pulsar_main_checkcmd("STAT"))
      rc = pulsar_stat();
    else if(pulsar_main_checkcmd("LIST"))
      rc = pulsar_list();
    else if(pulsar_main_checkcmd("UIDL"))
      rc = pulsar_uidl();
    else if(pulsar_main_checkcmd("RETR"))
      rc = pulsar_retr();
    else if(pulsar_main_checkcmd("TOP"))
      rc = pulsar_top();
    else if(pulsar_main_checkcmd("CAPA"))
      rc = pulsar_capa();
    else if(pulsar_main_checkcmd("STLS"))
      rc = pulsar_stls();
    else if(pulsar_main_checkcmd("DELE"))
      rc = pulsar_dele();
    else if(pulsar_main_checkcmd("RSET"))
      rc = pulsar_rset();
    else
      rc = pulsar_badcmd();

    if(-1 == rc)
      goto error;

    // code that based on rc returns error or OK messages to clients.
    switch(rc) {
    case defOK:
        pulsar_printf(defOKMsg);
        break;
    default:
    case defERR:
        pulsar_printf(defERRMsg);
        break;
    case defMalloc:
        pulsar_printf(defMallocMsg);
        break;
    case defBadcmd:
        pulsar_printf(defBadcmdMsg);
        break;
    case defMailbox:
        pulsar_printf(defMailboxMsg);
        break;
    case defAuth:
        pulsar_printf(defAuthMsg);
        break;
    case defParam:
        pulsar_printf(defParamMsg);
        break;
    case defDeleted:
        pulsar_printf(defDeletedMsg);
        break;
    case defConf:
        pulsar_printf(defConfMsg);
        break;
    case defMailboxLocked:
        pulsar_printf(defMailboxLockedMsg);
        break;
    case defNONE:
        break;
    }
  }

  g.head = NULL;
  rc = 0;

error:
  if(g.head)
    mailstore_close(g.head, 0); // don't commit.
  close(g.fd_in);
  close(g.fd_out);
  free(g.pop3_cmd);

  err_debug_return(rc);
}
//------------------------------------------------------------------------------------------------------------
