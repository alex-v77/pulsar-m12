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

#include <errno.h>
#include <syslog.h>
#include <stdlib.h>
#define  __USE_GNU // this is needed for memmem in string.h
#include <string.h>
#undef   __USE_GNU

#include "mailbox.h"

#include "util.h"
#include "error_facility.h"

void get_eol(strMailboxBuf *buf);
int  get_more_data(strMailboxBuf *buf);

//----------------------------------------------------------------------------------------
/* The problem description:

   Message in mailbox format has the following deficiencies:
   - theoreticaly unlimited lines
   - from quoting and unquoating

   We need to support 3 operations:
   - parsing (checking sizes and positions of all the mail in a mailbox)
   - delivery (quote)
   - retreival (unqoute)

   Mailbox is a line oriented format with potentialy unlimited line lenght.

   Becouse of this and not to allocate a new buffer for every line read we
   have to use a "window buffer". A buffer that only holds a partial line.

   These routines only fetch data as needed and only identify a type of line:
   - blank line
   - quoted from line
   - unquoted from line
   - normal
   - EOF = there is no more data
   When anlysing data they run lenght encode a potential From_ or quoted From_
   line up to the first missmatch. So you need to restore processed data with
   a mbox_ll_write() or by doing it yourself from data in buf->quoted_chars and/or
   buf->from_chars.

   These routines are only ment to help process the Mailbox format file or data
   sent from a pipe or socket or whatever. Quoting parsing and unqotaing must
   be done by higher routines.


   How to process Mailbox data ?
   1. create buffer with mbox_ll_create()
   2. process every line with mbox_ll_start()
   2.1 while(!buf->end) mbox_ll_cont();

   This is how you process every Mailbox line. When there isn't enough data
   in a buffer window to determine line type more data is fetched. Processed
   data is discarded but can be restored with information from buf->quoted_chars
   and buf->from_chars.
   Call mbox_ll_write(fd_out) to have data in a buffer sent to a given fd.

   After you've finished processing call mbox_ll_destroy().

   buf->quoted_chars and buf->quoted_from should be checked enven when defIsNormal
   is set becouse they could contain compressed data from a failed (quoted) From_
   check.

   Total line lenght is buf->quoted_chars + buf->from_chars + buf->len

*/
//----------------------------------------------------------------------------------------
int mbox_ll_buf_size(strMailboxBuf *buf) {
  int tmp;

  if(buf->end)
    tmp = buf->end - buf->start;
  else
    tmp = buf->buf + buf->size - buf->start;

  return tmp;
}
//----------------------------------------------------------------------------------------
/*
 Returns a \0 terminated decompressed line. Use the other two functions.. If they are
 too slow complain and I'll improve them.
 */
char *mbox_ll_mem(strMailboxBuf *buf) {
  const char  mask[] = defMailboxFrom;
  char       *tmp;

  tmp = safe_malloc(buf->from_chars + buf->quoted_chars + 1);
  if(!tmp)
    return NULL;

  memset(tmp, '>', buf->quoted_chars);
  memcpy(tmp + buf->quoted_chars, mask, buf->from_chars);
  tmp[buf->from_chars + buf->quoted_chars] = '\0'; // NULL terminate.

  return tmp;
}
//----------------------------------------------------------------------------------------
/*
 Writes with safe_write
 */
int mbox_ll_safe_write(int fd, strMailboxBuf *buf) {
  const char mask[] = defMailboxFrom;
  int        i;
  int        rc;

  for(i=0; i < buf->quoted_chars; i++) {
    rc = safe_write(fd, ">", 1);
    if(-1 == rc)
      goto io_error;
  }

  if(buf->from_chars) {
    rc = safe_write(fd, mask, buf->from_chars);
    if(-1 == rc)
      goto io_error;
  }

  i = buf->quoted_chars + buf->from_chars;
  return i;

io_error:
  return -1;
}
//----------------------------------------------------------------------------------------
/*
 Writes with net_write
 */
int mbox_ll_net_write(int fd, strMailboxBuf *buf, int *written) {
  const char mask[] = defMailboxFrom;
  int        i;
  int        rc;

  for(i=0; i < buf->quoted_chars; i++) {
    rc = net_write(fd, ">", 1);
    if(-1 == rc)
      goto io_error;
  }

  if(buf->from_chars) {
    rc = net_write(fd, mask, buf->from_chars);
    if(-1 == rc)
      goto io_error;
  }

  i = buf->quoted_chars + buf->from_chars;
  if(written)
    written[0] += i;
  return i;

io_error:
  return -1;
}
//----------------------------------------------------------------------------------------
/*
 This function reads in more data when needed into the Mailbox buffer.
 Used to traverse data after line identification has been completed.
*/
int mbox_ll_cont(strMailboxBuf *buf) {
  int rc = 0;

  //err_debug_function();
  if(buf->end)
    return 0; // nothing to do

  buf->start = buf->buf;
  rc = safe_read(buf->fd, buf->buf, defMailstoreBufSize);
  if(-1 == rc)
    return -1;
  buf->size = rc;

  // place end marker
  buf->end = memchr(buf->start, '\n', buf->size);
  if(buf->end) {
    buf->end++;
    buf->len += buf->end - buf->start;
  } else {
    buf->len += buf->buf + buf->size - buf->start;
  }

  return rc;
}
//----------------------------------------------------------------------------------------
strMailboxBuf *mbox_ll_create(int fd) {
  strMailboxBuf *tmp;
  tmp = calloc(1, sizeof(tmp[0]));
  if(tmp) {
    tmp->fd = fd;
    tmp->end = tmp->buf;
  }

  return tmp;
}
//----------------------------------------------------------------------------------------
void mbox_ll_destroy(strMailboxBuf *buf) {
  assert(buf);
  free(buf);
  return;
}
//----------------------------------------------------------------------------------------
/*
 Read start of line and identify line type. If more data is needed it is fetched.
 From_ and quoted From_ lines are run lenght compressed. Use mbox_ll_write() to restore.
 */
int mbox_ll_start(strMailboxBuf *buf) {
  const char mask[] = defMailboxFrom;
  int rc;
  int i;

  assert(buf->end);
  if(!buf->end)
    goto sw_error;

  buf->len = 0;
  buf->type = 0;
  buf->quoted_chars = 0;
  buf->from_chars = 0;
  buf->start = buf->end;
  get_eol(buf);

  // get more data ?
  rc = get_more_data(buf);
  if(-1 == rc)
    goto io_error;
  if(!rc) { // no more data.
    buf->type = defIsEOF;
    return 0;
  }

  // scan quote '>' characters
  while('>' == buf->start[0]) {
    rc = get_more_data(buf);
    if(-1 == rc)
      goto io_error;
    if(!rc) {
      buf->type = defIsNormal;
      goto exit;
    }
    buf->start++;
    buf->quoted_chars++;
  } // ~while

  // scan From_ characters
  i = min(5, buf->buf + buf->size - buf->start);
  if(!memcmp(buf->start, mask, i)) {
    buf->from_chars += i;
    buf->start += i;
  }

  // more data needed for From_ matching?
  // There is no way From_ can span more than 2 buffer windows.
  if(i<5) {
    rc = get_more_data(buf);
    if(-1 == rc)
      goto io_error;
    if(rc >= 5 - i) {
      if(!memcmp(buf->start, mask + i, 5 - i)) {
        buf->from_chars += 5 - i;
        buf->start += 5 - i;
      }
    }
  } //~if

  // OK.. here we analyse what's up.
  if (5 == buf->from_chars) {
    if(buf->quoted_chars)
      buf->type = defIsQuotedFrom;
    else
      buf->type = defIsFrom;
    goto exit;
  }

  if(!buf->quoted_chars && !buf->from_chars && 0 == buf->size) {
    buf->type = defIsEOF;
    goto exit;
  }

  if(!buf->quoted_chars && !buf->from_chars && '\n' == buf->start[0]) {
    buf->type = defIsBlank;
    goto exit;
  }

  buf->type = defIsNormal;

exit:
  buf->len = buf->from_chars + buf->quoted_chars;
  if(buf->end)
    buf->len += buf->end - buf->start;
  else
    buf->len += buf->buf + buf->size - buf->start;
  return 0;

io_error:
sw_error:
  return -1;
}
//----------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------
/* FOR INTERNAL USE ONLY!
 This routine get's more data from a file description
 (Refills the buffer window) if needed! It doesn't place the
 end of line marker except on buffer refill.
 -1 -> io error
  0 -> eof
 >0 -> there is data
*/
int get_more_data(strMailboxBuf *buf) {
  int rc = 0;

  //err_debug(0, ">get_more_data()");

  if(buf->start < buf->buf + buf->size) // you still have data.
    return 1;

  buf->start = buf->buf;
  rc = safe_read(buf->fd, buf->buf, defMailstoreBufSize);
  //err_debug(0, "*** refill the buffer: %d", rc);
  if(-1 == rc)
    return -1;
  buf->size = rc;
  get_eol(buf);

  return rc;
}
//----------------------------------------------------------------------------------------
void get_eol(strMailboxBuf *buf) {
  assert(buf->start);

  //err_debug(0, ">get_eol()");

  // place end marker
  buf->end = memchr(buf->start, '\n', buf->buf + buf->size - buf->start);
  if(buf->end) {
    buf->end++;
  }

  return;
}
//----------------------------------------------------------------------------------------
