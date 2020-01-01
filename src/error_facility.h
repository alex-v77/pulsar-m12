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

#ifndef __ERROR_FACILITY_H__
#define __ERROR_FACILITY_H__

#include <sysexits.h>
#include <syslog.h>
#include <stdlib.h>

/* ifdebug */
#ifdef DEBUG
#define ifdebug() if(1)
#else
#define ifdebug() if(0)
#endif

/* assert replacement */
#ifdef assert
#error assert() is already defined!
#endif

#define err_error(rc, fmt, ...) \
  do { \
    err_error__(__FILE__, __LINE__, __FUNCTION__, \
                rc, fmt, ##__VA_ARGS__); \
  } while(0)


#ifdef DEBUG
#define assert(cond) \
  do { \
    if (!(cond)) { \
      err_debug(0, "CRITICAL ERROR: Assertion failed: (%s) in file: %s function: %s() line %d. "\
                   "Source file date: \"%s\"", \
                    __STRING(cond), __FILE__, __FUNCTION__, __LINE__, __DATE__ \
               ); \
      abort(); \
    } \
  } while(0)
#else
#define assert(cond) ;
#endif

/* err_debug - requires ISO C99 */
#define err_debug(level, ...) \
  do { \
    if(level <= err.debug_level) syslog(LOG_INFO, __VA_ARGS__); \
  } while(0)

/* err_error */
#define err_internal_error() \
  err_error(EX_SOFTWARE, "CRITICAL ERROR: Internal software error.")

#define err_malloc_error() \
  err_error(EX_OSERR, "CRITICAL ERROR: Out of memory!")

#define err_io_error() \
  err_error(EX_SOFTWARE, "CRITICAL ERROR: Unerecoverable I/O error!")
//  err_error(EX_IOERR, "I/O error!");

#define err_debug_function() \
  err_debug(5, "%s()", __FUNCTION__);

#define err_debug_return(a) \
  do { \
    int err_debug_return_rc = (a); \
    err_debug(6, "%s:%d() return(%d)", __FUNCTION__, __LINE__, err_debug_return_rc); \
    return(err_debug_return_rc); \
  } while(0)

#define err_name2long_error(a) \
  err_error(EX_OSERR, "ERROR: Path/filename \"%s\" is too long.", (a))

#define err_suspend() err_close()

typedef struct _strErr {
  int  debug_level;  // effective debug level
  char error[2000];  // error message
  int  sysexits_rc;  // sysexits result code. If set to 0 data in 'data' is invalid.
} strErr;

extern strErr err;

int err_init(void);
int err_init_app(const char *app);
int err_close(void);
int err_resume(void);

// This will set verbosity. Set to 0 to disable logging.
int err_set_debug_level(int new_level);

// setting of error.
void err_error__(const char *file, const int line, const char *func,
                 int sysexits_rc, const char *format, ...);
void err_clear_error();

// retreival of error codes/chars
int     err_get_rc();
char   *err_get_error();
strErr *err_get();

#else

#ifdef DEBUG
#warning file "error_facility.h" already included.
#endif /* DEBUG */

#endif
