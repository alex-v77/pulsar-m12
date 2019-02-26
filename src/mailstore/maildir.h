/*
 * Copyright (C) 2003 Rok Papez <rok.papez@lugos.si>
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

#ifndef __MAILDIR_H__
#define __MAILDIR_H__

#include <sys/param.h> // needed for MAXPATHLEN

#ifndef defMaildirRetry
#define defMaildirRetry 10
#endif

#define defMaildirFlagPassed  0x01
#define defMaildirFlagReplied 0x02
#define defMaildirFlagSeen    0x04
#define defMaildirFlagTrashed 0x08
#define defMaildirFlagDraft   0x10
#define defMaildirFlagFlagged 0x20

#define defMaildirUIDLStr    "UIDL="
#define defMaildirNetSizeStr "NSIZ="
#define defMaildirSizeStr    "SIZE="

typedef struct _strMaildirMsg {
  int   size;  // *actual* size of single message (file)
  int   flags; // read flags from :2 maildir info
  char *fName; // strduped()!
} strMaildirMsg;

typedef struct _strMaildirHead {
  char maildir_new[MAXPATHLEN]; // these buffers can be missused as temporary
  char maildir_cur[MAXPATHLEN]; // filenames storage. But a missuser has to
  char maildir_tmp[MAXPATHLEN]; // restore them with .._new[maildir_strlen]='\0';
  int  maildir_strlen;          // They are all of the same size (new,cur,tmp)
  char hostname[HOST_NAME_MAX+1];
  int  hostname_strlen;
} strMaildirHead;

int mailstore_maildir_open   (strMailstoreHead *head);
int mailstore_maildir_close  (strMailstoreHead *head, int commit);
int mailstore_maildir_retr   (strMailstoreHead *head, int msg_num,
                              int fd, int lines
                             );
int mailstore_maildir_deliver(strMailstoreHead *head, int fd);

#else

#ifdef DEBUG
#warning file "maildir.h" already included.
#endif /* DEBUG */

#endif
