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
#include <pwd.h>
#include <grp.h>
#include <sys/types.h>

#include "pulsar.h"
#include "users.h"
#include "error_facility.h"

strStaticData g;
//------------------------------------------------------------------------------------------------------------
void help(const char *filename) {
  fprintf(stderr,
          "Usage: %s [-h] [-d[num]] [-rrealm] username\n"
          "\t-rrealm\tDeliver to user 'username' in realm 'realm'\n"
          "\t-dnum\tEnable debugging. Optional parameter num \n"
          "\t\tspecifies debug level (default = 1).\n"
          "\t-h\tdisplay this help message.\n"
          , filename
         );
  return;
}
//------------------------------------------------------------------------------------------------------------
int main(int argc, char *argv[]) {
  int rc = EX_OK;

  if(argc<2) {
    fprintf(stderr,"ERROR: This command should not be run by hand!\n");
    return EX_USAGE;
  }

  err_init_app("pulsar_deliver");
  err_set_debug_level(defDefaultDebugLevel);

  // read in the options from config file.
  g.cfg = cfg_read_config(defConfigFile);
  if (!g.cfg) {
    fprintf(stderr,"%s\n", err_get_error());
    err_close();
    return err_get_rc();
  }
  err_set_debug_level(g.cfg->debug);
  g.realm = get_realm(NULL, g.cfg); // select default realm!
  if (!g.realm) {
    err_internal_error();
    goto error;
  }

  // read options from command line
  opterr = 0;
  while(1) {
    rc = getopt(argc,argv,"hd::r:");
    if(-1 == rc) // no more options
      break;
    switch(rc){
    case 'r':
        g.realm = get_realm(optarg, g.cfg); // select default realm!
        if (!g.realm) {
          err_error(EX_DATAERR,"Realm \"%s\" not found\n",optarg);
          goto error;
        }
        err_debug(5, "Realm \"%s\" located", optarg);
        break;
    case 'd':
        if(optarg)
          err_set_debug_level(atoi(optarg));
        else
          err_set_debug_level(defDefaultDebugLevel);
        break;
    case '?':
        fprintf(stderr,"ERROR: unknown switch -%c\n",optopt);
    case 'h':
    default:
        help(argv[0]);
        err_error(EX_USAGE, "");
        goto error;
    }
  }
  if(argc-1 != optind) {
    fprintf(stderr,"ERROR: This command should not be run by hand!\n");
    err_error(EX_USAGE, "Insufficient parameters");
    goto error;
  }

  g.user = argv[optind];
  err_debug(3, "Delivery for user \"%s\"", g.user);

  // Get's user data, drops privs and open mailstore
  rc = users_getinfo();
  err_debug(0, "getinfo rc = %d", rc);
  if(defOK != rc) // all getinfos have to set err_error.
    goto error;   // TODO: shouldn't we use def* errors ? We have to unify error reports.

  // do delivery
  rc = mailstore_deliver(g.head, 0); // read from stdin
  if(-1 == rc) {
    mailstore_close(g.head, 0); // try to cleanup what we can...
    goto error;
  }

  // close mailbox, free options, do a nice cleanup.
  rc = mailstore_close(g.head, 0);
  if(-1 == rc)
    goto error;
  free_all_options(&g.cfg);
  err_close();
  return EX_OK;

error:
  fprintf(stderr, "%s\n", err_get_error());
  free_all_options(&g.cfg);
  err_close();
  return err_get_rc();
}
//------------------------------------------------------------------------------------------------------------
