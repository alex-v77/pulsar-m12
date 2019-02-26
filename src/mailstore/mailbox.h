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

#ifndef __MAILBOX_H__
#define __MAILBOX_H__

#ifndef defMailboxMsgCount
#define defMailboxMsgCount 100
#endif

#define defCacheMagic  0x1ee7f11e
#define defCacheSuffix ".pulsar_cache"
#define defCacheMajor  1
#define defCacheMinor  0

#define defMailboxFrom "From "

// this is needed for struct flock
#include <stdio.h>
#include <fcntl.h>
#include "pulsar.h"

typedef struct _strMailboxBuf {
  char  buf[defMailstoreBufSize];
  int   size;         /* quantity of data in buffer    */
  int   fd;           /* input file descriptor         */

  char *start;        /* start of the line in a buf    */
  char *end;          /* end of the line in a buf      */

  int   len;          /* line lenght                   */
  int   type;         /* line type                     */
  int   quoted_chars; /* number of quoted chars        */
  int   from_chars;   /* number of From_ chars matched */
} strMailboxBuf;

#define defIsEOF        0 /* no more data   */
#define defIsBlank      1 /* blank line     */
#define defIsNormal     2 /* non From_ line */
#define defIsFrom       3 /* From_ line     */
#define defIsQuotedFrom 4 /* >..>From_ line */

typedef struct _strMailboxMsg {
  int   size;                   // *actual* size of single message
  int   offset;                 // *actual* offset of message in mailbox file
} strMailboxMsg;

typedef struct _strMailboxHead {
  int           fd;                // mailbox file descriptor
  int           cache_fd;          // cache file descriptor
  int           msg_alloc;         // number of allocated message slots
  char          UIDL[defUIDLSize]; // next to be used UIDL
  char         *cache_filename;    // the filename of mailbox cache file
  struct flock  mailbox_lock;      // file (mailbox) lock structure
  struct flock  cache_lock;        // file (cache) lock structure
} strMailboxHead;

// These structures are needed for caching
typedef struct _strCacheFileHdr {
  unsigned int magic;
  unsigned int major;
  unsigned int minor;
} strCacheFileHdr;

typedef struct _strCacheFileHdr_1_0 {
  unsigned int  records;
  unsigned int  offset;
  unsigned char UIDL[defUIDLSize];
} strCacheFileHdr_1_0;

typedef struct _strCacheFileRec_1_0 {
  unsigned int  offset;
  unsigned int  POP3_size;
  unsigned char UIDL[defUIDLSize];
} strCacheFileRec_1_0;

int mailstore_mailbox_open   (strMailstoreHead *head);
int mailstore_mailbox_close  (strMailstoreHead *head, int commit);
int mailstore_mailbox_retr   (strMailstoreHead *head, int msg_num, int fd, int lines);
int mailstore_mailbox_deliver(strMailstoreHead *head, int fd);

/* These are private low-level functions */
strMailboxBuf *mbox_ll_create(int fd);                            /* create buffer             */
void           mbox_ll_destroy(strMailboxBuf *buf);               /* destroy buffer            */
int            mbox_ll_start(strMailboxBuf *buf);                 /* start line check          */
int            mbox_ll_cont(strMailboxBuf *buf);                  /* continue data traversing  */
char          *mbox_ll_mem(strMailboxBuf *buf);                   /* decompress to memory      */
int            mbox_ll_safe_write(int fd, strMailboxBuf *buf);    /* write to non-ssl layer    */
int            mbox_ll_net_write(int fd, strMailboxBuf *buf,      /* write to ssl layer        */
                                 int *written);
int            mbox_ll_buf_size(strMailboxBuf *buf);              /* size of uncompressed data */

#else

#ifdef DEBUG
#warning file "mailbox.h" already included.
#endif /* DEBUG */

#endif
