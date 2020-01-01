#pragma once

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

#include <sqlite3.h>
#include "pulsar.h"

#ifndef defMailboxMsgCount_sqlite
#define defMailboxMsgCount_sqlite 100
#endif

typedef struct _strMailboxBuf_sqlite {
  char  buf[defMailstoreBufSize];
  int   size;         /* quantity of data in buffer    */
  int   fd;           /* input file descriptor         */

  char *start;        /* start of the line in a buf    */
  char *end;          /* end of the line in a buf      */

  int   len;          /* line lenght                   */
  int   type;         /* line type                     */
  int   quoted_chars; /* number of quoted chars        */
  int   from_chars;   /* number of From_ chars matched */
} strMailboxBuf_sqlite;

#define defIsEOF        0 /* no more data   */
#define defIsBlank      1 /* blank line     */
#define defIsNormal     2 /* non From_ line */
#define defIsFrom       3 /* From_ line     */
#define defIsQuotedFrom 4 /* >..>From_ line */

typedef struct _strMailboxHead_sqlite {
  sqlite3      *db;
  int           msg_alloc;         // number of allocated message slots
} strMailboxHead_sqlite;

int mailstore_sqlite_mailbox_open   (strMailstoreHead *head);
int mailstore_sqlite_mailbox_close  (strMailstoreHead *head, int commit);
int mailstore_sqlite_mailbox_retr   (strMailstoreHead *head, int msg_num, int fd, int lines);
int mailstore_sqlite_mailbox_deliver(strMailstoreHead *head, int fd);
