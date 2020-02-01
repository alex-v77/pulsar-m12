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

#include <unistd.h>
#include <errno.h>

#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <syslog.h>

#include "util.h"
#include "mailbox.h"
#ifdef WITH_SQLITE
#include "sqlite_mailbox.h"
#endif
#include "maildir.h"
#include "error_facility.h"

//----------------------------------------------------------------------------------------
int mailstore_open (const char        *mailbox,
                    int                mailbox_type,
                    int                uidl,
                    int                create_mode,
                    int                op,
                    strMailstoreHead **head
                   )
{
  strMailstoreHead *tmp = NULL;
  int rc;

  err_debug(4, "%s(): mailbox=%s, mailbox_type=%d, uidl=%d, create mode=%#o op=%d",
            __FUNCTION__, mailbox, mailbox_type, uidl, create_mode, op
           );
  assert(mailbox);
  if(!mailbox)
    goto error_sw;

  tmp = safe_malloc(sizeof(*tmp));
  if(!tmp)
    goto error_os;
  memset(tmp,0x00,sizeof(*tmp));

  tmp->filename = strdup(mailbox);
  if(!tmp->filename)
    goto error_os;

  tmp->uidl = uidl;
  tmp->type = mailbox_type;
  tmp->create_mode = create_mode;
  tmp->op = op;

  assert(tmp->type <= defMailstoreEND);
  switch(tmp->type) {
  case defMailstoreMailbox:
      rc = mailstore_mailbox_open(tmp);
      if(rc)
        goto error;
      break;
#ifdef WITH_SQLITE
  case defMailstoreSqliteMailbox:
      rc = mailstore_sqlite_mailbox_open(tmp);
      if(rc)
        goto error;
      break;
#endif
  case defMailstoreMaildir:
      rc = mailstore_maildir_open(tmp);
      if(rc)
        goto error;
      break;
  default:
      goto error_sw;
      break;
  }

  tmp->del_msg_count = tmp->msg_count;
  tmp->del_total_size = tmp->total_size;
  *head = tmp;
  err_debug_return(0);

error_os:
  err_malloc_error();
  goto error;

error_sw:
  err_internal_error();

error:
  if(tmp) {
    free(tmp->filename);
    free(tmp);
  }
  err_debug_return(-1);
}
//----------------------------------------------------------------------------------------
int mailstore_close(strMailstoreHead *head, int commit) {
  int rc;

  err_debug_function();

  assert(head);
  if(!head)
    goto error_sw;

  assert(head->type <= defMailstoreEND);
  switch(head->type) {
  case defMailstoreMailbox:
      rc = mailstore_mailbox_close(head, commit);
      if(rc)
        goto error;
      break;
#ifdef WITH_SQLITE
  case defMailstoreSqliteMailbox:
      rc = mailstore_sqlite_mailbox_close(head, commit);
      if(rc)
        goto error;
      break;
#endif
  case defMailstoreMaildir:
      rc = mailstore_maildir_close(head, commit);
      if(rc)
        goto error;
      break;
  default:
      goto error_sw;
      break;
  }

  rc = 0;
  goto exit;

error_sw:
  err_internal_error();

error:
  rc = -1;

exit:
  free(head->filename);
  free(head);

  err_debug_return(rc);
}
//----------------------------------------------------------------------------------------
int mailstore_deliver(strMailstoreHead *head, int fd) {
  int rc;

  err_debug_function();

  assert(head);
  if(!head)
    goto error_sw;

  if(defMailstoreOpStore != head->op)
    goto error_sw;

  assert(head->type <= defMailstoreEND);
  switch(head->type) {
  case defMailstoreMailbox:
      rc = mailstore_mailbox_deliver(head, fd);
      if(rc)
        goto error;
      break;
#ifdef WITH_SQLITE
  case defMailstoreSqliteMailbox:
      rc = mailstore_sqlite_mailbox_deliver(head, fd);
      if(rc)
        goto error;
      break;
#endif
  case defMailstoreMaildir:
      rc = mailstore_maildir_deliver(head, fd);
      if(rc)
        goto error;
      break;
  default:
      goto error_sw;
      break;
  }
  err_debug_return(0);

error_sw:
  err_error(EX_SOFTWARE,
            "mailstore_deliver: Software internal error. Please report this to maintainer."
           );

error:
  err_debug_return(-1);
}
//----------------------------------------------------------------------------------------
// !!!! ACHTUNG! :-). msg_num starts with 0, *NOT* 1 !!!!
int mailstore_retr (strMailstoreHead  *head,
                    int                msg_num,
                    int                fd,
                    int                lines
                   )
{
  int rc;

  err_debug(4,"%s(): msg_num=%d lines=%d", __FUNCTION__, msg_num, lines);
  assert(head);
  if(!head)
    goto error_sw;

  if(defMailstoreOpRetr != head->op)
    goto error_sw;

  assert(msg_num < head->msg_count);
  if (msg_num >= head->msg_count) {
    err_error(EX_PROTOCOL,
              "Invalid message number (requested %d while max is %d)",
              msg_num,
              head->msg_count-1
             );
    goto error;
  }

  assert(head->type <= defMailstoreEND);
  switch(head->type) {
  case defMailstoreMailbox:
      rc = mailstore_mailbox_retr(head, msg_num, fd, lines);
      if(rc<0)
        goto error;
      break;
#ifdef WITH_SQLITE
  case defMailstoreSqliteMailbox:
      rc = mailstore_sqlite_mailbox_retr(head, msg_num, fd, lines);
      if(rc<0)
        goto error;
      break;
#endif
  case defMailstoreMaildir:
      rc = mailstore_maildir_retr(head, msg_num, fd, lines);
      if(rc<0)
        goto error;
      break;
  default:
      goto error_sw;
      break;
  }
  err_debug_return(0);

error_sw:
  err_internal_error();

error:
  err_debug_return(-1);
}
//----------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------
char *mailstore_UIDL(char *UIDL) {
  int i;

  for(i=0; i<defUIDLSize - 1; i++) {
    if(UIDL[i]<0x21 || 0x7E<UIDL[i]) // invalid UID char. Fix it .-)
      UIDL[i] = 0x21;
    else
      UIDL[i]++;
    if(UIDL[i] <= 0x7E)
      return UIDL;
    UIDL[i] = 0x21;
  }

  // overflow
  err_debug(0, "WARNING: UIDL overflow. Check UIDL consistency.");
  UIDL[0] = 0x21;
  memset(&UIDL[1], 0x00, defUIDLSize - 1); // overwrite unused part of the string
  return UIDL;
}
//----------------------------------------------------------------------------------------
int mailstore_disk_write(int fd, const char *buf, size_t count, int *written) {
  int rc;
  rc = safe_write(fd, buf, count);
  if(rc > 0) {
    if(written)
      written[0] += count;
  }
  return rc;
}
//----------------------------------------------------------------------------------------
