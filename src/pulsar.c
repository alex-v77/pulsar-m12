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

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/poll.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>

#include "pulsar.h"

#include "util.h"
#include "error_facility.h"

strStaticData g;

//------------------------------------------------------------------------------------------------------------
void help(const char *filename) {
  fprintf(stderr,
          "Usage: %s [-c] [-h] [-i] [-d[num]]\n"
          "\t-h\tdisplay this help message.\n"
          "\t-i\tRun from inetd, not as standalone.\n"
          "\t-dnum\tEnable debugging. Optional parameter 'num'\n"
          "\t\tspecifies debug level (default = 5).\n"
          "\t-c\tConfig Check - dump *effective* cfg data to\n"
          "\t\tfile \"var/pulsar/cfg\" and exit\n"
          ,filename
         );
  return;
}
//------------------------------------------------------------------------------------------------------------
void signal_terminate(int sig_no) {
  err_debug(0,"Terminating on signal %d", sig_no);
  exit(EX_OK);
  return;
}
//------------------------------------------------------------------------------------------------------------
void signal_CHLD(int sig_no) {
  pid_t pid;
  int   status;

  // I think it's possible for SIGCHLD to be raised for multiple children deaths.
  for(;;) {
    pid = waitpid(-1, &status, WNOHANG);
    if( 0 == pid || (-1 == pid && ECHILD == errno) ) // no more children to collect
      break;
    if(-1 == pid) {
      err_debug(0, "In SIGCHLD signal handler, waitpid failed, reason: \"%s\"", strerror(errno));
      return;
    }
    err_debug(4, "Child process %d terminated\n", pid);
  }

  return;
}

//------------------------------------------------------------------------------------------------------------
void signal_USR1(int sig_no) {
  const char *filename = "/var/pulsar/cfg";
  FILE *fd = fopen(filename,"wb");

  err_debug(2,"Received request to dump configuration data via signal USR1.");
  if(!fd) {
    err_debug(0, "Warning: Unable to open file \"%s\" to dump configuration.",filename);
    return;
  }
  dump_cfg_data(fd, g.cfg);
  fclose(fd);
  return;
}
//------------------------------------------------------------------------------------------------------------
int signal_init() {
  struct sigaction sa;
  int              rc;

  memset(&sa,0x00,sizeof(sa));
  sa.sa_handler = SIG_IGN;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  rc = sigaction(SIGPIPE, &sa, NULL);
  if(-1 == rc)
    goto error;
  rc = sigaction(SIGHUP, &sa, NULL);
  if(-1 == rc)
    goto error;

  memset(&sa,0x00,sizeof(sa));
  sa.sa_handler = signal_USR1;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  rc = sigaction(SIGUSR1, &sa, NULL);
  if(-1 == rc)
    goto error;

  memset(&sa,0x00,sizeof(sa));
  sa.sa_handler = signal_CHLD;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  rc = sigaction(SIGCHLD, &sa, NULL);
  if(-1 == rc)
    goto error;

  memset(&sa,0x00,sizeof(sa));
  sa.sa_handler = signal_terminate;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  rc = sigaction(SIGINT, &sa, NULL);
  if(-1 == rc)
    goto error;
  rc = sigaction(SIGQUIT, &sa, NULL);
  if(-1 == rc)
    goto error;
  rc = sigaction(SIGILL, &sa, NULL);
  if(-1 == rc)
    goto error;
  rc = sigaction(SIGBUS, &sa, NULL);
  if(-1 == rc)
    goto error;
  rc = sigaction(SIGFPE, &sa, NULL);
  if(-1 == rc)
    goto error;
  rc = sigaction(SIGSEGV, &sa, NULL);
  if(-1 == rc)
    goto error;
  rc = sigaction(SIGTERM, &sa, NULL);
  if(-1 == rc)
    goto error;

  return 0;

error:
  err_internal_error();
  return -1;
}
//------------------------------------------------------------------------------------------------------------
int daemonize() {
  int rc;
  int fd;
  int i;

  err_debug_function();
  rc = fork();
  if (-1 == rc) {
    err_error(EX_OSERR, "ERROR: Unable to fork()");
    return -1;
  }
  if (rc>0) exit(0); //parent should exit and return control, it's OK.

  rc = setsid();
  if (-1 == rc) {
    err_error(EX_OSERR, "ERROR: Unable to fork()");
    return -1;
  }

  rc = fork();
  if (-1 == rc) {
    err_error(EX_OSERR, "ERROR: Unable to fork()");
    return -1;
  }
  if (rc>0) exit(0); //parent should exit and return control, it's OK.

  fprintf(stdout,"%d\n",getpid()); // report our PID

  // reopen stdin, out and err with /dev/null.
  for (i=0;i<3;i++)
    close(i);
  fd = open("/dev/null",O_RDWR);
  if(-1 == fd) {
    err_error(EX_OSERR, "ERROR: Unable to open \"/dev/null\"");
    return -1;
  }
  for (i=0;i<3;i++) {
    if(-1 == dup2(fd,i)){
      err_error(EX_OSERR, "ERROR: Unable to dup2(%d)",i);
      return -1;
    }
  }
  close(fd);

  return 0;
}
//------------------------------------------------------------------------------------------------------------
int init_tcp_server() {
  int rc;
  int i;
  int one = 1;

  err_debug_function();

  // BUGBUG: if ifaces.count == 0 ???

  // allocate space for listening sockets
  g.socket_count = g.cfg->ifaces.count;
  g.polls = safe_calloc(g.socket_count, sizeof(g.polls[0]));
  if(!g.polls) {
    err_malloc_error();
    goto error;
  }

  // create/bind/listen every socket for every interface
  for(i=0; i<g.socket_count; i++) {
    g.polls[i].fd = socket(PF_INET, SOCK_STREAM, 0);
    if(-1 == g.polls[i].fd) {
      err_error(EX_OSERR,"ERROR: Unable to create server socket for interface \"%s\", reason: \"%s\"",
                cfg_interface_print(&g.cfg->ifaces.sa[i]),
                strerror(errno)
               );
      goto error;
    }
    rc = setsockopt(g.polls[i].fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    if(-1 == rc) {
      err_error(EX_OSERR,"Unable to set SO_REUSEADDR on a server socket for interface \"%s\", reason: \"%s\"",
                cfg_interface_print(&g.cfg->ifaces.sa[i]),
                strerror(errno)
               );
      goto error;
    }
    rc = bind(g.polls[i].fd, (struct sockaddr *)&g.cfg->ifaces.sa[i], sizeof(g.cfg->ifaces.sa[0]));
    if(-1 == rc) {
      err_error(EX_OSERR,"Unable to bind on a server socket for interface \"%s\", reason: \"%s\"",
                cfg_interface_print(&g.cfg->ifaces.sa[i]),
                strerror(errno)
               );
      goto error;
    }
    rc = listen(g.polls[i].fd, defBacklog);
    if(-1 == rc) {
      err_error(EX_OSERR,"Unable to listen on a server socket for interface \"%s\", reason: \"%s\"",
                cfg_interface_print(&g.cfg->ifaces.sa[i]),
                strerror(errno)
               );
      goto error;
    }

    g.polls[i].events = POLLIN;
  }

  return 0;

error:
  if(g.polls)
    free(g.polls);
  g.polls = NULL;
  g.socket_count = 0;
  return -1;
}
//------------------------------------------------------------------------------------------------------------
// BUGBUG TODO ... make this more robust.
int get_client() {
  int   i;
  int   rc;
  int   served;
  pid_t pid;
  int   new_socket;

  err_debug_function();

  for(;;) {

    do {
      rc = poll(g.polls, g.socket_count, -1);
    } while(-1 == rc && errno == EINTR);
    if(-1 == rc) {
      err_error(EX_OSERR, "Failed while waiting for client connections, reason: \"%s\"", strerror(errno));
      goto error;
    }

    for(i=0, served=0; i<g.socket_count && served<rc; i++) {
      if(!(g.polls[i].revents & POLLIN))
        continue;
      served++;

      g.remote_len = sizeof(g.remote);
      new_socket = accept(g.polls[i].fd, (struct sockaddr *)&g.remote, &g.remote_len);
      if(-1 == new_socket) {
        err_error(EX_OSERR,"Failed to accept incoming connection on interface \"%s\", reason: \"%s\"",
                  cfg_interface_print(&g.cfg->ifaces.sa[i]),
                  strerror(errno)
                 );
        goto error;
      }
      memcpy(&g.local, &g.cfg->ifaces.sa[i], sizeof(g.local));
      g.local_len = sizeof(g.local);
      g.ssl = g.cfg->ifaces.type[i] & defIfaceSSL;

      err_debug(1,"Incoming connection from: \"%s\"",
                cfg_interface_print(&g.remote)
               );
      err_debug(3,"Received on %sinterface: \"%s\"",
                g.ssl ? "SSL " : "",
                cfg_interface_print(&g.local)
               );

      pid = fork();
      if(!pid) { // child
        g.fd_in  = g.fd_out = new_socket;
        return 0;
      }
      if(-1 == pid) { // error
        err_error(EX_OSERR,"Failed to create a child process, reason: \"%s\"",strerror(errno));
        goto error;
      }
      err_debug(5,"Child with pid %d created", pid);
      g.polls[i].revents = 0;
      close(new_socket);
    }// ~for

  }// ~for

error:
  return -1;
}
//------------------------------------------------------------------------------------------------------------
int main(int argc, char *argv[]) {
  int dump_and_exit = 0;
  int rc = EX_OK;

  err_init();

  g.fd_in = 0;  //stdin
  g.fd_out = 1; //stdout
  umask(0117); // octal -> kills u+x g+x o+rwx
  rc = chdir("/");
  if (-1 == rc) {
    fprintf(stderr,"ERROR: Unable to chdir()\n");
    err_debug(0, "ERROR: Unable to chdir()");
    return EX_OSERR;
  }

  // read in the options from config file.
  g.cfg = cfg_read_config(defConfigFile);
  if (!g.cfg) {
    fprintf(stderr,"%s\n", err_get_error());
    err_debug(0,"%s", err_get_error());
    return err_get_rc();
  }
  err_set_debug_level(g.cfg->debug);
  //dump_cfg_data(stderr, g.cfg);

  // read options from command line
  opterr = 0;
  while(1) {
    rc = getopt(argc,argv,"chid::");
    if(-1 == rc) // no more options
      break;
    switch(rc){
    case 'c':
        dump_and_exit = 1;
        break;
    case 'd':
        if(optarg) {
          err_set_debug_level(atoi(optarg));
        } else {
          err_set_debug_level(defDefaultDebugLevel);
        }
        break;
    case 'i':
        g.cfg->inetd = 1;
        break;
    case 'h':
        help(argv[0]);
        return EX_USAGE;
    case '?':
        fprintf(stderr,"ERROR: unknown switch -%c\n",optopt);
        err_debug(0,"ERROR: unknown switch -%c",optopt);
    default:
        help(argv[0]);
        return EX_USAGE;
    }
  }

  // if not INETD
  if(!g.cfg->inetd) {
    rc = daemonize(); // slip into background.
    if (-1 == rc) {
      syslog(LOG_ERR,"%s", err_get_error());
      rc = err_get_rc();
      goto error;
    }
  } // ~if INETD

  // init signal handlers
  rc = signal_init();
  if(-1 == rc) {
    fprintf(stderr,"%s\n", err_get_error());
    err_debug(0,"%s", err_get_error());
    rc = err_get_rc();
    goto error;
  }

  // Perform OpenSSL initialization
  rc = init_ssl();
  if(-1 == rc) {
    fprintf(stderr,"%s\n", err_get_error());
    err_debug(0,"%s", err_get_error());
    rc = err_get_rc();
    goto error;
  }

  // this is for checking effective configuration data.
  if(dump_and_exit) {
    raise(SIGUSR1);
    return EX_OK;
  }

  // if not INETD
  if(!g.cfg->inetd) {
    rc = init_tcp_server(); // ready up tcp sockets
    if (-1 == rc) {
      syslog(LOG_ERR,"%s", err_get_error());
      rc = err_get_rc();
      goto error;
    }

    rc = get_client(); // accept a connection from a client.
    if (-1 == rc) {
      syslog(LOG_ERR,"%s", err_get_error());
      rc = err_get_rc();
      goto error;
    }
  }// ~if INETD

  // BUGBUG: for inetd mode add iface check so we know if turn SSL on or off.

  // do a tcp wrappers check
  rc = pulsar_tcpwrap_check();
  if(-1 == rc) {
    syslog(LOG_ERR,"%s", err_get_error());
    rc = err_get_rc();
    goto error;
  }

  // associate SSL with a socket.
  rc = ssl_on();
  if(-1 == rc) {
    syslog(LOG_ERR,"%s", err_get_error());
    rc = err_get_rc();
    goto error;
  }

  // run client conversation processing and then exit.
  rc = pulsar_main();
  if(-1 == rc) {
    syslog(LOG_ERR,"%s", err_get_error());
    rc = err_get_rc();
    goto error;
  }

  rc = EX_OK;
error:
  free_all_options(&g.cfg);
  err_close();
  return rc;
}
