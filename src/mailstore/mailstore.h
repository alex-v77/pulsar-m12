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

#ifndef __MAILSTORE_H__
#define __MAILSTORE_H__

#ifndef min
#define min(x,y)  ((x)>(y) ? (y) : (x))
#endif
#ifndef max
#define max(x,y)  ((x)>(y) ? (x) : (y))
#endif

#ifndef defMailstoreBufSize
#define defMailstoreBufSize (sizeof(char)*4*1024)
#endif

#define defMailstoreOpRetr  0x00 // Retreival
#define defMailstoreOpStore 0x01 // Delivery / storing

#define defMailstoreMailbox 0x00 // standard format: mailbox
#define defMailstoreMaildir 0x01 // maildir format !!
// TODO: indexed mailbox (support for IMAP + personal information + UIDL)
// TODO: pulsar format
#define defMailstoreEND     defMailstoreMaildir

#define defUIDLSize         71

typedef struct _strMailstoreMsg {
  int   size;                   // size of single message (POP3; not actual)
  int   deleted;                // is the message deleted ? 0 - no, 0!= - yes
  char  uidl[defUIDLSize];      // optional uidl
  void *fsd;                    // _F_ormat _S_pecific _D_ata
} strMailstoreMsg;

typedef struct _strMailstoreHead {  // This is a Mailstore descriptor!
  int              op;              // operation type (retreival or storing)
  int              type;            // type of mailstore format (also specifies in what format is fsd)
  int              uidl;            // Uidl supported ?
  int              create_mode;     // mode (permissions) to create new mailstore files with
  int              msg_count;       // number of messages in mailstore
  int              total_size;      // size of all messages in mailstore (POP3; not actual)
  int              del_msg_count;   // number of NOT DELETED messages in mailstore
  int              del_total_size;  // size of all NOT DELETED  messages in mailstore (POP3; not actual)
  char            *filename;        // the filename of mailbox (dir name of maildir, ...)
  strMailstoreMsg *msgs;            // pointer to arraya of message information
  void            *fsd;             // _F_ormat _S_pecific _D_ata
} strMailstoreHead;

/*
 * Calling:
 * 1x open()  -> n x retr()    -> 1 x close();
 * 1x open()  -> n x deliver() -> 1 x close();
 *
 * DON'T MIX CALLS TO RETR AND DELIVER!
 * The "mailstore descriptors" are not compatible!
 */

int mailstore_open   (const char        *mailbox,
                      int                mailbox_type,
                      int                uidl,
                      int                create_mode,
                      int                op,
                      strMailstoreHead **head
                     );
int mailstore_retr   (strMailstoreHead *head, int msg_num, int fd, int lines);
int mailstore_deliver(strMailstoreHead *head, int fd);
int mailstore_close  (strMailstoreHead *head, int commit);

// for use by mailstore modules!
char *mailstore_UIDL (char *UIDL); // Increments/initializes UIDL
int   mailstore_disk_write (int fd, const char *buf,
                            size_t count, int *written
                           );

#else

#ifdef DEBUG
#warning file "mailstore.h" already included.
#endif /* DEBUG */

#endif
