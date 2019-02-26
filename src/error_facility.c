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

#include <syslog.h>
#include <stdio.h>
#include <stdarg.h>
#include <sysexits.h>

#include "error_facility.h"

strErr      err;
const char *err_facility_app = "";

//------------------------------------------------------------------------------------------------------------
int err_close() {
  closelog();
  return 0;
}
//------------------------------------------------------------------------------------------------------------
int err_resume() {
  openlog(err_facility_app, LOG_PID, LOG_MAIL);
  return 0;
}
//------------------------------------------------------------------------------------------------------------
int err_init() {
  return err_init_app("Pulsar");
}
//------------------------------------------------------------------------------------------------------------
int err_init_app(const char *app) {
  err_facility_app = app;
  openlog(app, LOG_PID, LOG_MAIL);
  syslog(LOG_INFO, VERSION);
  err_set_debug_level(0);
  return 0;
}
//------------------------------------------------------------------------------------------------------------
int err_set_debug_level(int new_level) {
  int tmp = err.debug_level;
  err.debug_level = new_level;
  return tmp;
}
//------------------------------------------------------------------------------------------------------------
void
err_error__(const char *file, const int line, const char *func,
            int sysexits_rc, const char *format, ... )
{
  // TODO: make dynamic buffer. This isn't a frequently called func but it is OK for now.
  va_list ap;
  va_start(ap, format);
  vsnprintf(err.error, sizeof(err.error), format, ap);
  va_end(ap);
  err.sysexits_rc = sysexits_rc;
  err_debug(0, "%s >> Source: %s:%d - %s() << RC=%d",
            err.error, file, line, func, err.sysexits_rc);
  return;
}
//------------------------------------------------------------------------------------------------------------
void err_clear_error() {
  err.sysexits_rc = EX_OK;
  err.error[0] = '\0';
  return;
}
//------------------------------------------------------------------------------------------------------------
int err_get_rc() {
  return err.sysexits_rc;
}
//------------------------------------------------------------------------------------------------------------
char *err_get_error() {
  return err.error;
}
//------------------------------------------------------------------------------------------------------------
strErr *err_get() {
  return &err;
}
//------------------------------------------------------------------------------------------------------------
