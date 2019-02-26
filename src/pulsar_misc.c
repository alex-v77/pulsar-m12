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

#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <unistd.h>

#include "pulsar.h"
#include "users.h"
#include "util.h"
#include "error_facility.h"

#ifdef WITH_TCP_WRAPPERS
#include <tcpd.h>
int allow_severity = LOG_INFO;
int deny_severity = LOG_INFO;
#endif /* WITH_TCP_WRAPPERS */

int cmp_iface(struct sockaddr_in *a, struct sockaddr_in *b);

//------------------------------------------------------------------------------------------------------------
int cmp_iface(struct sockaddr_in *a, struct sockaddr_in *b) {

  if(a->sin_family != b->sin_family)
    return 1;
  if(a->sin_port != b->sin_port)
    return 1;
  if(a->sin_addr.s_addr != b->sin_addr.s_addr)
    return 1;

  return 0;
}
//------------------------------------------------------------------------------------------------------------
int pulsar_auth(int type, const char *credentials) {
  int rc;

  err_debug_function();

  // set realm
  if(g.realm_name)
    g.realm = get_realm(g.realm_name, g.cfg);
  else
    g.realm = get_realm_by_interface();

  if(!g.realm)
    g.realm = get_realm(NULL, g.cfg); // default realm

  if (!g.realm) {
    err_internal_error();
    return -1;
  }

  err_debug(3, "Realm \"%s\" selected", g.realm->realm_name);

  // authenticate
  rc = users_auth(type, credentials);
  if(rc) {
    g.state = defABORT;
    err_debug(1, "%s", err_get_error());
    err_debug_return( rc );
  }

  err_debug(1, "User \"%s\" logging into realm \"%s\"",
            g.user,
            g.realm->realm_name ? g.realm->realm_name : "_default_"
           );
  g.state = defCONN;
  err_debug_return( defOK );
}
//------------------------------------------------------------------------------------------------------------
int pulsar_printf(const char *format, ...) {
  int     rc;
  int     size;
  int     written;
  va_list ap;

  va_start(ap, format);
  rc = vsnprintf(g.tx_buf, defBufSize, format, ap);
  va_end(ap);

  if(rc<0 || rc + 2 > defBufSize) { // buffer too small + we need space for 'CR'+'LF'
    err_internal_error();           // we shouldn't have replies longer than tx_buf.
    return -1;
  }

  assert(rc+1 < defBufSize);
  assert('\0' == g.tx_buf[rc]);
  g.tx_buf[rc    ] = '\r'; // CR
  g.tx_buf[rc + 1] = '\n'; // LF

  size = rc+2; // from 0 to rc+1 -> +2
  written = 0;

  while (written < size) {
    rc = net_write(g.fd_out, &g.tx_buf[written], size - written);
    if (-1 == rc && EINTR == errno)
      continue;  // interrupted by signal.. try again with the same parameters
    if (rc < 0) {
      err_error(EX_IOERR, "Error while transmiting data. Reason: \"%s\"", strerror(errno));
      return -1; // error.
    }
    if (!rc) // no more data.. just a safeguard
      break;
    written += rc;
  }

  if(written != size) {
    err_error(EX_IOERR, "Failed to transmit all the data. Client lost ?");
    return -1; // error
  }

  return written;
}
//------------------------------------------------------------------------------------------------------------
strOptionsRealm *get_realm_by_interface() {
  strOptionsRealm *realm;
  int i,j;

  err_debug_function();
  if(g.local_len != sizeof(g.local)) // interface not present
    return NULL;

  err_debug(5, "Will try to match interface: \"%s\"", cfg_interface_print(&g.local));

  for(i = 0; i < g.cfg->realms_count; i++) {
    realm = &(g.cfg->realms[i]);
    for(j = 0; j < realm->ifaces.count; j++) {
      err_debug(5,"Checking with interface: \"%s\" for realm: \"%s\"",
                cfg_interface_print(&realm->ifaces.sa[j]),
                realm->realm_name
               );
      if(!cmp_iface(&realm->ifaces.sa[j], &g.local))
        return realm;
    } // ~for
  } // ~for
  return NULL;
}
//------------------------------------------------------------------------------------------------------------
#ifdef WITH_TCP_WRAPPERS
int pulsar_tcpwrap_check() {
  struct request_info req;
  struct request_info *res;

  // TODO: check this... Wietse made (!! NOT *again* !!) some realy great documentation.
  res = request_init(&req,
                     RQ_DAEMON, "pulsar",
                     RQ_FILE, g.fd_in,
                     0
                    );
  if(!res) {
    err_error(EX_OSERR, "ERROR: request_init() in %s() failed!", __FUNCTION__);
    return -1;
  }

  if(g.remote_len && g.local_len) {
    res = request_set(&req,
                      RQ_CLIENT_SIN, &g.remote,
                      RQ_SERVER_SIN, &g.local,
                      0
                     );
    if(!res) {
      err_error(EX_OSERR, "ERROR: request_set() in %s() failed!", __FUNCTION__);
      return -1;
    }
  }

  fromhost(&req);
  if(!hosts_access(&req)) {
    close(g.fd_in);
    close(g.fd_out);
    err_error(EX_NOPERM, "Access denied by tcp wrappers, connection closed!");
    return -1;
  }

  return 0;
}
#else
int pulsar_tcpwrap_check() {
  return 0;
}
#endif /* WITH_TCP_WRAPPERS */
//------------------------------------------------------------------------------------------------------------
#ifdef WITH_SSL
int init_ssl() {
  char *key_file = g.cfg->key_file ? g.cfg->key_file : g.cfg->cert_file;
  int   rc;

  err_debug_function();
  if(!g.cfg->cert_file) {
    err_debug(2, "No certificate specified, SSL disabled.");
    return EX_OK;
  }

  SSL_load_error_strings();
  SSL_library_init();
  g.ssl_method = SSLv23_server_method(); /* TODO: add options to disable v2/v3 */
  if(!g.ssl_method) {
    err_error(EX_OSERR, "SSL ERROR: Unable to create server method.");
    return -1;
  }
  g.ssl_ctx = SSL_CTX_new(g.ssl_method);
  if(!g.ssl_ctx) {
    err_error(EX_OSERR, "SSL ERROR: Unable to create server context.");
    return -1;
  }
  rc = SSL_CTX_use_certificate_file(g.ssl_ctx, g.cfg->cert_file, SSL_FILETYPE_PEM);
  if(rc <= 0) {
    err_error(EX_OSFILE,
              "SSL ERROR: PEM certificate chain file missing or in invalid format: \"%s\"",
              g.cfg->cert_file);
    return -1;
  }
  rc = SSL_CTX_use_PrivateKey_file(g.ssl_ctx, key_file, SSL_FILETYPE_PEM);
  if(rc <= 0) {
    err_error(EX_OSFILE,
              "SSL ERROR: PEM private key file missing or in invalid format: \"%s\"",
              key_file);
    return -1;
  }
  rc = SSL_CTX_check_private_key(g.ssl_ctx);
  if(rc <= 0) {
    err_error(EX_DATAERR,
              "SSL ERROR: key \"%s\" doesn't verify with certificate \"%s\"",
              key_file, g.cfg->cert_file);
    return -1;
  }
  g.ssl_con = SSL_new(g.ssl_ctx);
  if(!g.ssl_con) {
    err_error(EX_OSERR, "SSL ERROR: Can't create SSL structure");
    return -1;
  }

  err_debug(4, "OpenSSL initialization complete.");
  return EX_OK;
}
#else
int init_ssl() {
  return EX_OK;
}
#endif /* WITH_SSL */
//------------------------------------------------------------------------------------------------------------
#ifdef WITH_SSL
int ssl_on() {
  int rc;

  err_debug_function();

  if(!g.ssl) // not our beer.
    return EX_OK;

  rc = SSL_set_rfd(g.ssl_con, g.fd_in);
  if(rc <= 0) {
    err_error(EX_OSERR, "SSL ERROR: Unable to set read fd.");
    return -1;
  }
  rc = SSL_set_wfd(g.ssl_con, g.fd_out);
  if(rc <= 0) {
    err_error(EX_OSERR, "SSL ERROR: Unable to set write fd.");
    return -1;
  }

  err_debug(5, "Will call SSL_accept()."); // BUGBUG.. set timer for this one and bail-out when timeout.
  rc = SSL_accept(g.ssl_con);
  if(rc != 1) {
    err_error(EX_IOERR, "SSL ERROR: Unable to negotiate SSL connection.");
    return -1;
  }
  err_debug(5, "SSL_accept() OK. SSL layer active.");

  return EX_OK;
}
#else
int ssl_on() {
  return EX_OK;
}
#endif /* WITH_SSL */
//------------------------------------------------------------------------------------------------------------
