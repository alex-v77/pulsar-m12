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
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sysexits.h>
#include <netinet/in.h>

#include <syslog.h>

#include <stdlib.h>
#define  __USE_GNU // this is needed for memmem in string.h
#include <string.h>
#undef   __USE_GNU

#include "util.h"
#include "mailbox.h"
#include "error_facility.h"

int mailbox_commit       (strMailstoreHead *head);
int mailbox_commit_repos (int fd, int offset, int data_offset, int data_size, char *buf);
int counting_net_write   (int fd, const void *buf, size_t count, int *written);
//int counting_disk_write  (int fd, const char *buf, size_t count, int *written1, int *written2);
int open_cache           (strMailstoreHead *head);
int close_cache          (strMailstoreHead *head, int commit);
int get_msg_hash         (strMailstoreHead *head);
int parse_mailbox        (strMailstoreHead *head);

//----------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------
int mailstore_mailbox_open(strMailstoreHead *head) {
  strMailboxMsg  *my_msg;
  strMailboxHead *my_head;
  int             rc;

  err_debug_function();

  head->msgs = NULL;

  my_head = safe_calloc(1, sizeof(*my_head));
  if (!my_head)
    goto error;
  head->fsd = my_head;

  // open mailbox file
  my_head->fd = open(head->filename, O_RDWR|O_CREAT, head->create_mode);
  if(-1 == my_head->fd) {
    err_error(EX_NOINPUT,
              "Unable to open mailbox file: \"%s\", reason: \"%s\"",
              head->filename,
              strerror(errno)
             );
    goto error;
  }

  // perform exclusive lock
  my_head->mailbox_lock.l_type = F_WRLCK;
  my_head->mailbox_lock.l_start = 0;
  my_head->mailbox_lock.l_whence = SEEK_SET;
  my_head->mailbox_lock.l_len = 0;
  my_head->mailbox_lock.l_pid = getpid();
  do {
    rc = fcntl(my_head->fd, F_SETLK, &my_head->mailbox_lock);
  } while(-1 == rc && EINTR == errno);
  if(-1 == rc) {
    err_error(EX_TEMPFAIL,
              "Unable to lock mailbox file: \"%s\", reason: \"%s\"",
              head->filename,
              strerror(errno)
             );
    goto error;
  }

  // Get message hash from cache
  rc = get_msg_hash(head);
  if (-1 == rc) // writes its own error messages
    goto error;

  ifdebug() {
    for(rc=0; rc < head->msg_count; rc++) {
      my_msg = head->msgs[rc].fsd;
      err_debug(9, "i=%d, offset = %u, size(POP3_size) = %u(%u), uidl = %s",
                rc,
                my_msg->offset,
                my_msg->size,
                head->msgs[rc].size,
                head->msgs[rc].uidl
               );
    }
  }
  // UIDL engine enabled by default. It's up to upper layers to use
  // them or not but we have to keep track. That's why we just live
  // head->uidl the way it is.
  //head->uidl = 0;

  err_debug_return(0);

error:
  if(my_head) {
    if(-1 != my_head->fd) // close ? unlock will happen on close!
      close(my_head->fd);
    if(-1 != my_head->cache_fd) // close ?
      close_cache(head, 0);
    free(my_head);
  }
  head->fsd = NULL;
  err_debug_return(-1);
}
//----------------------------------------------------------------------------------------
int mailstore_mailbox_close(strMailstoreHead *head, int commit) {
  strMailboxHead *my_head;
  int             i;
  int             rc;

  err_debug_function();

  assert(head);
  my_head = head->fsd;
  if(head->msgs && commit) {
    rc = mailbox_commit(head);
    if(-1 == rc)
      goto error;
  }
  rc = 0;

error:
  close_cache(head, commit);
  close(my_head->fd); // after close_cache as close() will also do unlock.
  for(i=0; i<head->msg_count; i++)
    if(head->msgs[i].fsd)
      free(head->msgs[i].fsd);
  if(head->msgs)
    free(head->msgs);

  err_debug_return(rc);
}
//----------------------------------------------------------------------------------------
int mailstore_mailbox_retr (strMailstoreHead *head, int msg_num, int fd, int lines) {
  strMailboxHead *my_head;
  strMailboxMsg  *my_msg;
  strMailboxBuf  *buf        = NULL;
  int             byte_count = 0;
  int             POP3_sent  = 0;
  int             line_count = 0;
  int             was_blank  = 0;
  int             is_body    = 0;
  int             tmp;
  int             rc;

  err_debug_function();

  my_head = head->fsd;
  assert(my_head);
  if(!my_head) {
    err_internal_error();
    goto error;
  }

  my_msg = head->msgs[msg_num].fsd;
  assert(my_msg);
  if(!my_msg) {
    err_internal_error();
    goto error;
  }

  rc = lseek(my_head->fd, my_msg->offset, SEEK_SET); // go to the specified offset
  if(-1 == rc) {
    err_io_error();
    goto error;
  }

  buf = mbox_ll_create(my_head->fd);
  if(!buf) {
    err_malloc_error();
    goto error;
  }

  // start a data read.
  rc  = mbox_ll_start(buf);
  if(rc < 0) {
    err_io_error();
    goto error;
  }

  // mailbox starts with a From_ line. skip it.
  if(defIsFrom != buf->type) {
    err_debug(0, "Critical error in mailbox format, message doesn't start with a From_ line.");
    goto error;
  }
  while(!buf->end) {
    rc = mbox_ll_cont(buf);
    if(rc < 0) {
      err_io_error();
      goto error;
    }
    //byte_count += mbox_ll_buf_size(buf);
  }
  byte_count = buf->len;

  while(1) { // line iteration -- LINES
    rc  = mbox_ll_start(buf);
    if(rc < 0)  {
      err_io_error();
      goto error;
    }

    // no more data
    if(defIsEOF  == buf->type ||
       defIsFrom == buf->type
      )
      break;

    // blank line
    if(was_blank) {
      rc = counting_net_write(fd, "\r\n", 2, &POP3_sent);
      if(-1 == rc) {
        err_io_error();
        goto error;
      }
      byte_count++;
    }
    was_blank = 0;
    if(defIsBlank == buf->type) {
      assert(buf->end);
      was_blank = 1;
      is_body = 1;
      continue;
    }

    // check line count for TOP command
    if(-1 != lines && is_body) {
      if(line_count >= lines)
        break;
      line_count++;
    }

    // quoted From_ line
    if(defIsQuotedFrom == buf->type) {
      buf->quoted_chars--; // unquote
      byte_count++;        // so we don't miss this one!
    }

    // do we need to dot-stuff this line ?
    if('.' == buf->start[0]) {
      rc = counting_net_write(fd, ".", 1, &POP3_sent);
      if(-1 == rc) {
        err_io_error();
        goto error;
      }
    }

    // send compressed data
    rc = mbox_ll_net_write(fd, buf, &POP3_sent);
    if(-1 == rc) {
      err_io_error();
      goto error;
    }
    byte_count += rc;

    // send incomplete line data (NL not in buffer window)
    while(!buf->end) {
      tmp = mbox_ll_buf_size(buf);
      rc = counting_net_write(fd, buf->start, tmp, &POP3_sent);
      if(-1 == rc) {
        err_io_error();
        goto error;
      }
      byte_count += tmp;
      rc = mbox_ll_cont(buf);
      if(rc < 0) {
        err_io_error();
        goto error;
      }
    }

    // found end of line...
    tmp = mbox_ll_buf_size(buf);
    rc = counting_net_write(fd, buf->start, tmp - 1, &POP3_sent);
    if(-1 == rc) {
      //err_debug(0, "PAZI>>>> %d, END: %x, START: %x, SIZE: %d", tmp, buf->end, buf->start, buf->size);
      err_io_error();
      goto error;
    }
    byte_count += tmp;

    // terminate with a CRLF
    rc = counting_net_write(fd, "\r\n", 2, &POP3_sent);
    if(-1 == rc)  {
      err_io_error();
      goto error;
    }
  } //~while -- LINES

  //debug check... omit this in final version. (POP3_sent, byte_count).
  ifdebug() {
    if(was_blank)
      byte_count++;
    if(-1 == lines) {
      if(my_msg->size != byte_count) {
        err_debug(2, "WARNING: Message real size is %d but %d bytes were processed. Please file a bug report.",
                  my_msg->size, byte_count
                 );
      }
      if(head->msgs[msg_num].size != POP3_sent) {
        err_debug(2, "WARNING: Message POP3 size is %d but %d bytes were sent. Please file a bug report.",
                  head->msgs[msg_num].size,
                  POP3_sent
                 );
      }
    }
  } // ~ifdebug()

  mbox_ll_destroy(buf);
  err_debug_return(0);

error:
  if(buf)
    mbox_ll_destroy(buf);

  err_debug_return(-1);
}
//----------------------------------------------------------------------------------------
// From with timestamp and envelope from must already be appended
// >From quoting will be performed by this function
// data is being read from file descriptor 'fd' and written to the mailbox.
int mailstore_mailbox_deliver(strMailstoreHead *head, int fd) {
  /*
  char             buf[defMailstoreBufSize]; // BUGBUG, TODO: bad man.. bad..
  struct stat      mbox_stat;
  strMailboxMsg   *my_msg;
  strMailboxHead  *my_head;
  strMailstoreMsg *msgs;

  char            *start;
  char            *eol;
  char            *ptr;
  char             match[] = "From ";

  int              rc = 0;
  int              size;

  int              mbox_size = 0;
  int              POP3_size = 0;

  err_debug_function();
  assert(head);
  my_head = head->fsd;

  rc = fstat(my_head->fd, &mbox_stat);
  if (-1 == rc)
    goto io_error;

  rc = lseek(my_head->fd, 0, SEEK_END);
  if(-1 == rc)
    goto io_error;

  // add another entry for a message being delivered
  if(head->msg_count == my_head->msg_alloc) {
    msgs = realloc(head->msgs, sizeof(msgs[0]) * my_head->msg_alloc + 1);
    if(!msgs)
      goto malloc_error;
    head->msgs = msgs;
    memset(&msgs[my_head->msg_alloc], 0x00, sizeof(msgs[0]));
    my_head->msg_alloc++;
  }
  msgs = head->msgs;
  my_msg = msgs[head->msg_count].fsd;
  if(!my_msg) {
    my_msg = safe_calloc(1, sizeof(my_msg[0]));
    if(!my_msg)
      goto malloc_error;
  }
  msgs[head->msg_count].fsd = my_msg;

  // ----------------------------
  // process envelope From_ line
  // ----------------------------
  start = buf; // make it invalid to force a read
  rc = safe_read(fd, buf, defMailstoreBufSize); // read data from mailer
  if (-1 == rc)
    goto io_error;
  size = rc;

  while(1) {
    eol = memchr(start, '\n', buf+size - start);
    if(!eol) { // read in the new data.
      if(start != buf) {
        size = buf+size - start;
        memmove(buf, start, size);
        rc = safe_read(fd, buf+size, defMailstoreBufSize - size); // read data from mailer
        if (-1 == rc)
          goto io_error;
        size += rc;
        if(!rc) // no more data
          break;
        continue; // we wrapped the buffer.. try again.
      }
      if(size >= defMailstoreBufSize - 1) {
        err_error(EX_DATAERR, "ERROR: Line too long. Increase defMailstoreBufSize!");
        goto error;
      }
      eol = buf+size;
      eol[0] = '\n'; // append a newline. it is missing.
    } // ~if

    ptr = start;
    while(ptr[0] == '>' && ptr<eol)
      ptr++;
    if(eol - ptr >= 5)
      if(memcmp(ptr, match, 5) && !mbox_size) {
        rc = counting_disk_write(my_head->fd, ">", 1, &mbox_size, NULL);
        if(-1 == rc)
          goto io_error;
      }
    rc = counting_disk_write(my_head->fd, start, eol - start + 1, &mbox_size, &POP3_size);
    if(-1 == rc)
      goto io_error;
    start = eol + 1;
  } // ~while

  // ----------------
  // append newline!
  // ----------------
  rc = counting_disk_write(my_head->fd, "\n", 1, &mbox_size, NULL);
  if (-1 == rc)
    goto io_error;

  // message delivery is complete... update data
  strcpy(msgs[head->msg_count].uidl, my_head->UIDL);
  mailstore_UIDL(my_head->UIDL); // increment UIDL for next message
  msgs[head->msg_count].size = POP3_size;
  head->total_size += POP3_size;
  my_msg->size = mbox_size;
  my_msg->offset = mbox_stat.st_size; // offset equals file size before delivery
  head->msg_count++;
  err_debug(5, "10");
  return 0;

malloc_error:
  err_error(EX_OSERR,
            "Out of memory? Unable to deliver message to mailbox file: \"%s\"",
            head->filename
           );
  goto error;

io_error:
  err_error(EX_IOERR,
            "I/O error. Unable to deliver message to mailbox file: \"%s\"",
            head->filename
           );

error:
ftruncate(my_head->fd, mbox_stat.st_size); // no error checking becouse we can't recover
*/
  err_error(EX_IOERR,
            "Delivery currently not implemented"
           );

  err_debug_return(-1);
}
//----------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------
/*
int counting_disk_write(int fd, const char *buf, size_t count, int *written1, int *written2) {
  int rc;
  rc = safe_write(fd, buf, count);
  if(rc > 0) {
    if(written1)
      written1[0] += count;
    if(written2)
      written2[0] += count;
  }
  return rc;
}
*/
//----------------------------------------------------------------------------------------
int counting_net_write(int fd, const void *buf, size_t count, int *written) {
  int rc;
  rc = net_write(fd, buf, count);
  if(rc > 0)
    *written += count;
  return rc;
}
//----------------------------------------------------------------------------------------
int mailbox_commit_repos(int fd, int offset, int data_offset, int data_size, char *buf) {
  int rc;
  int tmp;

  while(data_size > 0) {
    tmp = min(defMailstoreBufSize, data_size);
    rc = lseek(fd, data_offset, SEEK_SET);
    if(-1 == rc)
      goto error;
    rc = safe_read(fd, buf, tmp);
    if(rc != tmp)
      goto error;
    data_size -= tmp;
    data_offset += tmp;

    rc = lseek(fd, offset, SEEK_SET);
    if(-1 == rc)
      goto error;
    rc = safe_write(fd, buf, tmp);
    if(rc != tmp)
      goto error;
    offset += tmp;
  }

  return 0;

error:
  return -1;
}
//----------------------------------------------------------------------------------------
int mailbox_commit(strMailstoreHead *head) {
  strMailboxMsg  *my_msg;
  strMailboxHead *my_head;
  char           *buf = NULL;
  int             offset; // offset of processed data
  int             i;
  int             rc;

  err_debug_function();
  assert(head);

  if(!head->msg_count)
    err_debug_return(0);

  my_head = head->fsd;
  my_msg  = head->msgs[head->msg_count-1].fsd;
  offset  = my_msg->offset + my_msg->size;

  for(i=0; i<head->msg_count; i++) { // skip all non-deleted msgs
    if(head->msgs[i].deleted) {
      my_msg = head->msgs[i].fsd;
      buf = safe_malloc(defMailstoreBufSize);
      if(!buf) {
        err_malloc_error();
        goto error;
      }
      offset = my_msg->offset;
      break;
    }
  }

  for(;i<head->msg_count;i++) {
    assert(buf);
    if(!head->msgs[i].deleted) {
      my_msg = head->msgs[i].fsd;
      rc = mailbox_commit_repos(my_head->fd, offset, my_msg->offset, my_msg->size, buf);
      if(-1 == rc)
        goto error;;
      offset += my_msg->size; // +1 discarded. TEST THIS BUGBUG TODO
    }
  }

  rc = ftruncate(my_head->fd, offset);
  if(buf)
    free(buf);

  err_debug_return(0);

error:
  err_debug(0, "CRITICAL I/O ERROR: Mailbox file \"%s\" might have been corrupted.", head->filename);
  if(buf)
    free(buf);

  err_debug_return(-1);
}
//----------------------------------------------------------------------------------------
int open_cache (strMailstoreHead *head) {
  strCacheFileHdr      master_hdr;
  strCacheFileHdr_1_0  sec_hdr;
  strCacheFileRec_1_0  record;
  strMailboxHead      *my_head = NULL;
  strMailboxMsg       *my_msg = NULL;
  int                  rc;
  int                  i;

  err_debug_function();

  my_head = head->fsd;
  my_head->cache_fd = -1;

  // cache filename = ?
  my_head->cache_filename = safe_malloc(strlen(head->filename) + sizeof(defCacheSuffix));
  if(!my_head->cache_filename) {
    err_debug(0, "ERROR: malloc() failed! Out of memory ?\n");
    goto error;
  }
  strcpy(my_head->cache_filename, head->filename);
  strcat(my_head->cache_filename, defCacheSuffix);

  // open cache
  my_head->cache_fd = open(my_head->cache_filename, O_RDWR|O_CREAT, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP);
  if(-1 == my_head->cache_fd) {
    err_debug(0, "ERROR: Unable to open cache file \"%s\": \"%s\"", my_head->cache_filename, strerror(errno));
    goto error;
  }
  rc = lseek(my_head->cache_fd, 0, SEEK_SET);
  if(-1 == rc) {
    err_debug(0, "ERROR: Unable to seek in cache file \"%s\": \"%s\"", my_head->cache_filename, strerror(errno));
    goto error;
  }

  my_head->cache_lock.l_type = F_WRLCK;
  my_head->cache_lock.l_start = 0;
  my_head->cache_lock.l_whence = SEEK_SET;
  my_head->cache_lock.l_len = 0;
  my_head->cache_lock.l_pid = getpid();
  do {
    rc = fcntl(my_head->cache_fd, F_SETLK, &my_head->cache_lock);
  } while(-1 == rc && EINTR == errno);
  if(-1 == rc) {
    err_debug(0, "ERROR: Unable to lock mailbox cache file: \"%s\"", my_head->cache_filename);
    goto error;
  }
  err_debug(6, "Cache opened and locked: %s", my_head->cache_filename);

  // check if it is a valid cache file.
  rc = safe_read(my_head->cache_fd, &master_hdr, sizeof(master_hdr));
  if(sizeof(master_hdr) != rc) {
    err_debug(0, "ERROR: Read error in cache file: %s", my_head->cache_filename);
    goto exit;
  }
  master_hdr.magic = ntohl(master_hdr.magic);
  master_hdr.major = ntohl(master_hdr.major);
  master_hdr.minor = ntohl(master_hdr.minor);

  if(defCacheMagic != master_hdr.magic) {
    err_debug(0, "ERROR: Not a valid cache file: %s", my_head->cache_filename);
    goto exit;
  }
  // TODO: add version check.

  // read the secondary header
  rc = safe_read(my_head->cache_fd, &sec_hdr, sizeof(sec_hdr));
  if(sizeof(sec_hdr) != rc) {
    err_debug(0, "ERROR: Read error in cache file: %s", my_head->cache_filename);
    goto exit;
  }
  sec_hdr.records = ntohl(sec_hdr.records);
  sec_hdr.offset = ntohl(sec_hdr.offset);
  my_head->msg_alloc = sec_hdr.records;
  head->msg_count = sec_hdr.records;
  strcpy(my_head->UIDL, sec_hdr.UIDL);

  if(!my_head->msg_alloc)
    my_head->msg_alloc = defMailboxMsgCount;
  head->msgs = safe_calloc(my_head->msg_alloc, sizeof(head->msgs[0]));
  if(!head->msgs) {
    err_debug(0, "ERROR: Malloc error processing cache file: %s", my_head->cache_filename); // TODO: possibly bogus error report.
    goto exit;
  }

  rc = lseek(my_head->cache_fd, sec_hdr.offset, SEEK_SET);
  if(-1 == rc) {
    err_debug(0, "ERROR: Seek failed in cache file: %s", my_head->cache_filename);
    goto exit;
  }

  // read in every message record
  for(i=0; i < head->msg_count; i++) {
    rc = safe_read(my_head->cache_fd, &record, sizeof(record));
    if(sizeof(record) != rc) {
      err_debug(0, "ERROR: Read error in cache file: %s", my_head->cache_filename);
      goto exit;
    }
    record.offset = ntohl(record.offset);
    record.POP3_size = ntohl(record.POP3_size);
    my_msg = safe_calloc(1, sizeof(my_msg[0]));
    if(!my_msg) {
      err_debug(0, "ERROR: Malloc error processing cache file: %s", my_head->cache_filename);
      goto exit;
    }
    head->msgs[i].fsd = my_msg;
    head->msgs[i].size = record.POP3_size;
    strcpy(head->msgs[i].uidl, record.UIDL);
    my_msg->offset = record.offset;

    /*
    err_debug(0, "oc: size = %d, off = %d, uidl = %s",
              head->msgs[i].size,
              my_msg->offset,
              head->msgs[i].uidl
             );
    */
  }

  // TODO: data consistency check. ???
  err_debug_return(0);

error:
  // TODO.. kill msgs structures. They are not valid.
  err_debug(0, "ERROR openning cache file: \"%s\"", my_head->cache_filename);
  if(my_head->cache_filename) {
    free(my_head->cache_filename);
    my_head->cache_filename = NULL;
  }
  if(-1 != my_head->cache_fd) {
    close(my_head->cache_fd);
    my_head->cache_fd = -1;
  }

exit:
  err_debug_return(-1);
}
//----------------------------------------------------------------------------------------
int close_cache(strMailstoreHead *head, int commit) {
  strCacheFileHdr      master_hdr;
  strCacheFileHdr_1_0  sec_hdr;
  strCacheFileRec_1_0  record;
  strMailboxHead      *my_head = NULL;
  strMailboxMsg       *my_msg = NULL;
  int                  offset;
  int                  rc = 0;
  int                  i;

  err_debug_function();
  err_debug(6, "  commit = %d", commit);

  my_head = head->fsd;
  if(-1 == my_head->cache_fd) {
    err_debug(2, "WARNING: Cache file was not open! Mailbox information is not cached!");
    goto exit;
  }

  if(!commit) {
    goto exit;
  }

  // store master header
  rc = lseek(my_head->cache_fd, 0, SEEK_SET);
  if(-1 == rc)
    goto error;
  master_hdr.magic = htonl(defCacheMagic);
  master_hdr.major = htonl(defCacheMajor);
  master_hdr.minor = htonl(defCacheMinor);
  rc = safe_write(my_head->cache_fd, &master_hdr, sizeof(master_hdr));
  if(sizeof(master_hdr) != rc)
    goto error;

  // store secondary header
  sec_hdr.records = htonl(head->del_msg_count);
  sec_hdr.offset = htonl(sizeof(master_hdr) + sizeof(sec_hdr));
  strcpy(sec_hdr.UIDL, my_head->UIDL);
  i = strlen(sec_hdr.UIDL);
  memset(&sec_hdr.UIDL[i], 0x00, defUIDLSize - i);
  rc = safe_write(my_head->cache_fd, &sec_hdr, sizeof(sec_hdr));
  if(sizeof(sec_hdr) != rc)
    goto error;

  // store a record for every message
  offset = 0;
  for(i=0; i<head->msg_count; i++) {
    if(head->msgs[i].deleted)
      continue;
    my_msg = head->msgs[i].fsd;
    record.offset = htonl(offset);
    record.POP3_size = htonl(head->msgs[i].size);
    strcpy(record.UIDL, head->msgs[i].uidl);
    rc = safe_write(my_head->cache_fd, &record, sizeof(record));
    if(sizeof(record) != rc)
      goto error;
    offset += my_msg->size;
  }

  // cut the file here and now!
  i = lseek(my_head->cache_fd, 0, SEEK_CUR);
  rc = ftruncate(my_head->cache_fd, i);
  if(-1 == rc)
    goto error;

  rc = 0;
  goto exit;

error:
  err_debug(0, "ERROR: %s() - Cache file will be removed!", __FUNCTION__ );
  rc = -1;
  if(my_head->cache_filename)
    unlink(my_head->cache_filename);

exit:
  if(-1 != my_head->cache_fd) {
      close(my_head->cache_fd);
      my_head->cache_fd = -1;
  }
  if(my_head->cache_filename)
    free(my_head->cache_filename);
  my_head->cache_filename = NULL;
  err_debug(6, "Exiting function %s() with rc = %d", __FUNCTION__, rc);

  err_debug_return(rc);
}
//----------------------------------------------------------------------------------------
int parse_mailbox(strMailstoreHead *head) {
  strMailstoreMsg  *new_msgs;
  strMailboxHead   *my_head;
  strMailboxMsg    *my_this;
  strMailboxMsg    *my_prev;
  strMailboxBuf    *buf;
  int               was_blank;
  int               POP3_size;
  int               offset;
  int               set_offset;
  int               rc;
  int               tmp;

  err_debug_function();

  my_head = head->fsd;
  was_blank = 0;
  set_offset = 0;
  POP3_size = 0;
  offset = 0;
  head->msg_count = 0; // discard open_cache supplied value.

  buf = mbox_ll_create(my_head->fd);
  if(!buf)
    goto malloc_error;

  if(!head->msgs) {
    my_head->msg_alloc = defMailboxMsgCount;
    head->msgs = safe_calloc(my_head->msg_alloc, sizeof(head->msgs[0]));
  }
  if(!head->msgs)
    goto malloc_error;
  //err_debug(0, ">mbox_ll created.");

  // process lines
  while(1) {
    rc  = mbox_ll_start(buf);
    if(rc < 0)
      goto error;

    /*
    if(buf->type) {
      if(buf->end)
        buf->end[-1] = '\0';
      else
        buf->buf[buf->size-1] = '\0';
      test = mbox_ll_mem(buf);
      err_debug(0, ">Line(%d/off=%d): \"%s%s\"", buf->type, set_offset, test, buf->start);
      if(test)
        free(test);
      if(buf->end)
        buf->end[-1] = '\n';
      else
        buf->buf[buf->size-1] = 'X';
    } else {
      err_debug(0, ">Line(0/off=%d): dejta nje spljoh", set_offset);
    }
    */

    if(defIsEOF == buf->type) // no more data
      break;

    if('.' == buf->start[0]) {
      POP3_size++;
    }

    // process - count the data
    while(!buf->end) {
      //err_debug(0, ">mbox->end");
      rc = mbox_ll_cont(buf);
      if(rc < 0)
        goto io_error;
    }
    tmp = buf->len;

    if(defIsFrom == buf->type) { // ENVELOPE FROM -- begin new message header.
      head->msg_count++;

      // need more space ?
      if(head->msg_count > my_head->msg_alloc) {
        new_msgs = realloc(head->msgs, 2*my_head->msg_alloc * sizeof(head->msgs[0]));
        if(!new_msgs)
          goto malloc_error;
        memset(&new_msgs[my_head->msg_alloc], 0x00,
               sizeof(new_msgs[0]) * my_head->msg_alloc
              );
        my_head->msg_alloc *= 2;
        head->msgs = new_msgs;
      }

      // allocate structures
      my_this = head->msgs[head->msg_count-1].fsd;
      if(!my_this)
        my_this = safe_calloc(1, sizeof(my_this[0]));
      if(!my_this)
        goto malloc_error;
      head->msgs[head->msg_count-1].fsd = my_this;

      if(head->msg_count <= 1) { // first message... no data to input and no POP3_size!
        set_offset = offset;
        offset += tmp;
        continue;
      }

      if(was_blank) // last blank line will be stripped.
        POP3_size -= 2;

      // insert new data.
      my_prev = head->msgs[head->msg_count-2].fsd;
      if(my_prev->offset != set_offset ||
         head->msgs[head->msg_count-2].size != POP3_size
        )
      { // change data from open_cache -- this entry is invalid.
        my_prev->offset = set_offset;
        head->msgs[head->msg_count-2].size = POP3_size;
        strcpy(head->msgs[head->msg_count-2].uidl, mailstore_UIDL(my_head->UIDL));
      }
      POP3_size = 0;
      set_offset = offset;
      offset += tmp;
      continue;
    } // ~if - ENVELOPE FROM

    POP3_size += tmp + 1; // becouse of CR
    offset += tmp;

    if(defIsQuotedFrom == buf->type)
      POP3_size--;        // becouse of '>'

    was_blank = 0;
    if(defIsBlank == buf->type)
      was_blank = 1;
  } // ~while(1) -- END OF LINE PROCESSING




  //err_debug(0, ">> last msg.");
  if(head->msg_count) {
    if(was_blank)
      POP3_size -= 2; // last blank line will be stripped away.
    my_this = head->msgs[head->msg_count-1].fsd;
    if(my_this->offset != set_offset ||
       head->msgs[head->msg_count-1].size != POP3_size
      )
    {
      my_this->offset = set_offset;
      head->msgs[head->msg_count-1].size = POP3_size;
      strcpy(head->msgs[head->msg_count-1].uidl, mailstore_UIDL(my_head->UIDL));
    } // ~if
  } // ~if

  err_debug_return(0);




io_error:
  err_io_error();
  goto error;
malloc_error:
  err_malloc_error();
error:
  if(buf)
    mbox_ll_destroy(buf);
  mailstore_mailbox_close(head, 0);
  memset(head->msgs, 0x00, sizeof(head->msgs[0]) * head->msg_count); // all data is invalid

  err_debug_return(-1);
}
//----------------------------------------------------------------------------------------
int get_msg_hash(strMailstoreHead *head) {
  strMailboxHead *my_head = NULL;
  strMailboxMsg  *my_msg = NULL;
  strMailboxMsg  *prev_msg = NULL;
  struct stat     st_mbox;
  struct stat     st_cache;
  int             rc;
  int             i;

  err_debug_function();

  my_head = head->fsd;

  // stat mailbox file
  rc = fstat(my_head->fd, &st_mbox);
  if(-1 == rc) {
    err_debug(0, "Unable to stat mailbox file: \"%s\", reason: \"%s\"",
              head->filename,
              strerror(errno)
             );
    goto error;
  }

  // first open cache and retreive information.
  rc = open_cache(head);
  if(-1 == rc)
    goto parse_mailbox;

  // stat cache file
  rc = fstat(my_head->cache_fd, &st_cache);
  if(-1 == rc) {
    err_debug(0, "Unable to stat mailbox cache file: \"%s\", reason: \"%s\"",
              my_head->cache_filename,
              strerror(errno)
             );
    goto parse_mailbox;
  }

  // is cache file up-2-date ?
  if(st_mbox.st_mtime <= st_cache.st_mtime)
    goto finish; // yes
  err_debug(4, "Cache file is older than mailbox file. Will have to merge UIDL information.");

parse_mailbox:
  rc = parse_mailbox(head);
  if(-1 == rc)
    err_debug_return(rc);

finish:
  // calculate in the missing data (the actual message sizes in a mailbox file)
  // TODO: check that we actualy have messages! BUGBUG ERROR!!!
  if(!head->msg_count) {
    head->total_size = 0;
    err_debug_return(0);
  }
  head->total_size = head->msgs[0].size;
  my_msg = head->msgs[0].fsd;
  for(i=1; i < head->msg_count; i++) {
    prev_msg = my_msg;
    my_msg = head->msgs[i].fsd;
    prev_msg->size = my_msg->offset - prev_msg->offset;
    head->total_size += head->msgs[i].size;
  }
  my_msg->size = st_mbox.st_size - my_msg->offset;

  err_debug_return(0);

error:
    // TODO: cleanup ?
  err_debug_return(-1);
}
//----------------------------------------------------------------------------------------
