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

#ifdef   HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#define  __USE_GNU // this is needed for memmem in string.h
#include <string.h>
#undef   __USE_GNU
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <syslog.h>
#include <fcntl.h>
#include <time.h>

#include "error_facility.h"
#include "pulsar.h"
#include "maildir.h"
#include "util.h"
#include "md5.h"


int   maildir_make(strMailstoreHead *head);
char *md5_str(const char md5hash[16]);
char  nibble_str(char num);
int   maildir_file_filter(const struct dirent *file);
int   process_file(char *new_fname, int len, char *filename, strMailstoreMsg *msg);
void  process_flag(int *flags, const char val);
int   process_UIDL(char *UIDL, const char *start);
int   count_lfs(const char *buf, int len);
int   md5hash_file(char *UIDL, const char * const filename);
int   str_append(char *buf, int avail, const char * const add);
int   str_append_num(char *buf, int avail, const int num);
int   append_info(char *fname, int len, const char *const UIDL,
                  int size, int nsiz, int flags);


#define defIsEOF        0 /* no more data   */
#define defIsBlank      1 /* blank line     */
#define defIsNormal     2 /* non From_ line */

typedef struct _strMaildirBuf {
  char  buf[defMailstoreBufSize];
  char *start;
  char *end;
  int   size_POP3;
  int   size_real;
  int   type;
} strMaildirBuf;

strMaildirBuf *buf_init        ();
int            buf_readwriteln (int fd_in, int fd_out, strMaildirBuf *buf);
int            buf_type        (strMaildirBuf *buf);
int            buf_size_pop3   (strMaildirBuf *buf);
int            buf_size_real   (strMaildirBuf *buf);
void           buf_term        (strMaildirBuf *buf);
int            last_net_write  (int fd, const void *buf, size_t count, char *last);


//----------------------------------------------------------------------------------------
int
last_net_write (int fd,
                const void *buf,
                size_t count,
                char *last
               )
{
  if(count && last && buf)
    last[0] = ((char*)buf)[count - 1];
  return net_write(fd, buf, count);
}
//----------------------------------------------------------------------------------------
int
str_append_num(char *buf, int avail, const int num) {
  char tmp[128];

  snprintf(tmp, sizeof(tmp), "%d", num);
  tmp[sizeof(tmp)-1] = '\0';

  return str_append(buf, avail, tmp);
}
//----------------------------------------------------------------------------------------
int
str_append(char *buf, int avail, const char * const add) {
  int tmp = strlen(add);

  if(tmp >= avail - 1) // no space
    return avail; // we would consume everything... no need to actualy do it ;>

  strcat(buf, add);
  return tmp;
}
//----------------------------------------------------------------------------------------
// appends info to the filename
// returns 0 if it runs out of space
int
append_info(char              *fname, /* input buffer   */
            int                len,   /* free buf space */
            const char *const  UIDL,  /* UIDL to append */
            int                size,  /* size of file   */
            int                nsiz,  /* network size   */
            int                flags  /* Deleted, ...   */
           )
{
  len -= str_append(fname, len, ":2,");
  if( flags&defMaildirFlagDraft )
    len -= str_append(fname, len, "D");
  if( flags&defMaildirFlagFlagged )
    len -= str_append(fname, len, "F");
  if( flags&defMaildirFlagPassed )
    len -= str_append(fname, len, "P");
  if( flags&defMaildirFlagReplied )
    len -= str_append(fname, len, "R");
  if( flags&defMaildirFlagSeen )
    len -= str_append(fname, len, "S");
  if( flags&defMaildirFlagTrashed )
    len -= str_append(fname, len, "T");

  if(UIDL[0]) {
    len -= str_append(fname, len, "," defMaildirUIDLStr);
    len -= str_append(fname, len, UIDL);
  }

  if(size>0) {
    len -= str_append(fname, len, "," defMaildirSizeStr);
    len -= str_append_num(fname, len, size);
  }

  if(nsiz>0) {
    len -= str_append(fname, len, "," defMaildirNetSizeStr);
    len -= str_append_num(fname, len, nsiz);
  }

  return len;
}
//----------------------------------------------------------------------------------------
// MD5 hash and count the network size of maildir file.
int
md5hash_file(char *UIDL, const char * const filename) {
  char           buf[defMailstoreBufSize];
  char           md5hash[16]; // 16x8 = 128b
  struct md5_ctx ctx;
  int            nsiz = 0;
  int            fd;
  int            rc;

  UIDL[0] = '\0'; // invalidate here and now!

  err_debug_function();

  fd = open(filename, O_RDONLY);
  if(-1 == fd) {
    err_debug(1, "WARNING: Can't open file \"%s\". Reason: %s\n", filename, strerror(errno));
    err_debug_return(-1);
  }

  md5_init_ctx(&ctx);

  while(1) {
    rc = safe_read(fd, buf, sizeof(buf));
    if(-1 == rc) {
      goto error;
    }

    if(0 == rc)
      break;

    md5_process_bytes(buf, rc, &ctx);
    nsiz += rc;
    nsiz += count_lfs(buf, rc);
  } // ~while

  nsiz += count_lfs(NULL, 0); // count trailing LF ?
  md5_finish_ctx(&ctx, md5hash);
  strcpy(UIDL, md5_str(md5hash));

  close(fd);
  err_debug_return(nsiz);

error:
  close(fd);
  err_debug(1, "WARNING: Can't md5 hash maildir file \"%s\". Reason: %s\n", filename, strerror(errno));
  err_debug_return(-1);
}
//----------------------------------------------------------------------------------------
// count wild '\n's (those without a matching CR)
// flag 0x01 is set if previous char was a CR
int
count_lfs(const char *buf, /* input data, NULL for last line */
          int len          /* size of data in buffer         */
         )
{
  static char prev = 0;
  int         lfs  = 0;
  int         i;

  if(!buf) {
    if('\n' != prev)
      lfs = 2;
    prev = 0; // reset for a new op
    return lfs;
  }

  for(i=0; i<len; i++) {
    if('\n' == buf[i] && '\r' != prev)
      lfs++;

    prev = buf[i];
  }

  return lfs;
}


/*
int
count_lfs(const char *buf, int len) {
  int        lfs   = 0;
  int        i     = 0;

  if(!len || !buf) {
    int tmp = flags;
    flags = 0;
    if(tmp & 0x04) // CRLF
      return 0;
    if(tmp) // CR or LF
      return 1;
    return 2; // no CR and no LF
  }


  for(; i<len; i++) {
    flags &= ~0x04;

    if('\r' == buf[i]) {
      flags |= 0x01;
      continue;
    }
    if('\n' == buf[i]) {
      if(flags & 0x01)
        flags |= 0x04;
      else
        lfs++;
    }
    flags &= ~0x01;
  } // ~for


  flags &= ~0x02; // clear
  if('\n' == buf[len-1])
    flags |= 0x02;

  return lfs;
}
*/

/*
int
count_lfs(const char *buf, int len, int *flags) {
  int lfs = 0;
  int i   = 0;

  if(!len) {
    if (flags[0]&0x01)
      lfs++;
    flags[0] &= ~0x01;
    goto exit;
  }

  if(flags[0]&0x01 && '\r' != buf[0]) {
    flags[0] &= ~0x01;
    lfs++;
    i++;
  }

  for(; i<len; i++) {
    if(flags[0]&0x01 && '\r' != buf[i])
      lfs++;
    if('\n' == buf[i])
      flags[0] |= 0x01;
    else
      flags[0]&= ~0x01;
  }

exit:
  return lfs;
}
*/
//----------------------------------------------------------------------------------------
int
process_UIDL(char *UIDL, const char *start) {
  int i = 0;

  while(start[i] && ',' != start[i] && i < defUIDLSize - 1) {
    UIDL[i] = start[i];
    i++;
  }
  UIDL[i] = '\0';

  return i;
}
//----------------------------------------------------------------------------------------
void
process_flag(int *flags, const char val) {
  switch(val) {
  case 'P':
    flags[0] |= defMaildirFlagPassed;
    break;
  case 'R':
    flags[0] |= defMaildirFlagReplied;
    break;
  case 'S':
    flags[0] |= defMaildirFlagSeen;
    break;
  case 'T':
    flags[0] |= defMaildirFlagTrashed;
    break;
  case 'D':
    flags[0] |= defMaildirFlagDraft;
    break;
  case 'F':
    flags[0] |= defMaildirFlagFlagged;
    break;
  }
}
//----------------------------------------------------------------------------------------
int
process_file(char *new_fname, int len, char *filename, strMailstoreMsg *msg) {
  char           UIDL[defUIDLSize];
  strMaildirMsg *my_msg;
  char          *marker;
  char          *start;
  char          *end;
  int            flags = 0;
  long           nsiz  = -1;
  long           size  = -1;
  int            tmp;
  int            rc = 0;  // set it to 1 if we had to update some info.

  err_debug_function();

  assert(new_fname);
  assert(filename);
  assert(filename[0]);

  memset(UIDL, 0x00, sizeof(UIDL));
  start = strrchr(filename, '/');
  if(!start) {
    start = filename;
  } else {
    start++;
  }

  end = strrchr(filename, ':');
  if(!end) {
    end = &filename[strlen(filename)];
  }

  tmp = end - start;
  if(len <= tmp + 1) {
    goto error_2long;
  }

  // copy filename without the tag
  memcpy(new_fname, start, tmp);
  new_fname[tmp] = '\0';

  // extract as much information from filename as possible
  start = end + 1;
  end = &filename[strlen(filename) - 1];

  while(start < end) {

    if('2' != start[0]) // not our info format.
      break;

    for(;',' != start[0] &&  start<end; start++); // eat chars to ','
    start++; // eat found ','
    if(start >= end)
      break;

    for(;',' != start[0] &&  start<end; start++) // process flags
      process_flag(&flags, start[0]);
    start++;
    if(start >= end)
      break;

    if(strncasecmp(start, defMaildirUIDLStr, end - start)) {
      start += sizeof(defMaildirUIDLStr) - 1;
      start += process_UIDL(UIDL, start);
    }
    for(;',' != start[0] &&  start<end; start++); // eat chars to ','
    start++; // eat ','
    if(start >= end)
      break;;

    if(strncasecmp(start, defMaildirSizeStr, end - start)) {
      start += sizeof(defMaildirSizeStr) - 1;
      size   = strtol(start, &marker, 0);
      start  = marker; // skip possible invalid trailing chars after size
    }
    for(;',' != start[0] &&  start<end; start++); // eat chars to ','
    start++; // eat ','
    if(start >= end)
      break;;

    if(strncasecmp(start, defMaildirNetSizeStr, end - start)) {
      start += sizeof(defMaildirNetSizeStr) - 1;
      nsiz   = strtol(start, NULL, 0);
    }

    break;
  } // ~while



  // We have some or nothing of information.
  // UIDL, nsize must be calculated from a file.
  // size can be stated and flags can't be retreived
  if(-1 == size) {
    struct stat st;
    rc = stat(filename, &st);
    if(-1 == rc) {
      err_debug(1, "WARNING: Can't stat file \"%s\". Reason: %s\n", filename, strerror(errno));
    }
    else {
      size = st.st_size;
    }
    rc = 1;
  }

  if(!UIDL[0] || -1 == nsiz) {
    nsiz = md5hash_file(UIDL, filename);
    rc = 1;
  }
  err_debug(4, "filename: %s, size: %ld, nsiz: %ld\n", filename, size, nsiz);

  // Append information
  tmp = append_info(&new_fname[tmp], len - tmp, UIDL, size, nsiz, flags);
  if(!tmp) {
    goto error_2long;
  }
  err_debug(4, "new_fname: \"%s\"", new_fname);

  // if msg add info about this mail.
  if(msg) {
    my_msg = msg->fsd;
    if(!my_msg) {
      my_msg   = safe_calloc(1, sizeof(my_msg[0]));
      msg->fsd = my_msg;
    }
    msg->size     = nsiz;
    msg->deleted  = 0;
    strcpy(msg->uidl, UIDL);
    my_msg->size  = size;
    my_msg->flags = flags;
    my_msg->fName = strdup( new_fname );
    if(!my_msg->fName) {
      err_malloc_error();
      goto error;
    }

  }
  return rc;

error_2long:
  err_error(EX_USAGE,
            "ERROR: processed filename \"%s\" would exceed max path length\n",
            filename
           );
error:
  return -1;
}
//----------------------------------------------------------------------------------------
char nibble_str(char num) {
  num &= 0x0F;
  if(num<10)
    return '0' + num;
  return 'a' - 0xa + num;
}
//----------------------------------------------------------------------------------------
char *md5_str(const char md5hash[16]) {
  static char buf[33];
  int i;

  for(i=0; i<16; i++) {
    buf[i*2 + 1] = nibble_str( md5hash[i]      );
    buf[i*2 ]    = nibble_str( md5hash[i] >> 4 );
  }

  buf[32] = '\0';
  return buf;
}
//----------------------------------------------------------------------------------------
strMaildirBuf *
buf_init()
{
  strMaildirBuf *buf = safe_calloc(1, sizeof(buf[0]));
  return buf;
}
//----------------------------------------------------------------------------------------
// Send out a line (read in more data if needed) and CRLF
// update counters
// update line type
//
// buf->start points to the beginning of data.
// - locate a LF or CRLF and send data up to it.
// - send a CRLF
// - update counters
// - update type
int
buf_readwriteln (int fd_in,
                 int fd_out,
                 strMaildirBuf *buf
                )
{
  char *eol    = NULL;
  char  last   = 0;
  int   count  = 0;
  int   rc;
  int   old_size_POP3;

  assert(buf);

  buf->type = defIsEOF;
  old_size_POP3 = buf->size_POP3;




  for(;;) {
    //err_debug(0, "-------------------");
    last  = 0;
    count = 0;

    // how much data is in the buffer ? Do we need a refill ?
    if(buf->end <= buf->start) { // we need more data
      rc = safe_read(fd_in, buf->buf, sizeof(buf->buf));
      //err_debug(0, "safe read: %d", rc);
      if(-1 == rc){
        err_io_error();
        goto error;
      }
      buf->start = buf->buf;
      buf->end   = buf->buf;
      eol        = buf->buf;
      if(!rc) // EOF
        break;
      buf->end += rc; // end points to one over.
    } // ~if

    // find NL. -1 becouse buf->end[0] isn't valid...
    //err_debug(0, "AAAA: ddd: %d, start[0]==%d", buf->end - buf->start, buf->start[0]);
    eol = memchr(buf->start, '\n', buf->end - buf->start);

    if(!eol) {
      //err_debug(0, "AAAA: not eol");
      eol = buf->end;
    } else { // ~if
      //err_debug(0, "AAAA: count++");
      count++; // NL was processed
    }

    // eol now points to end of data to TX and tmp to a new start.
    count += eol - buf->start;
    rc = last_net_write(fd_out, buf->start, eol - buf->start, &last);
    if(-1 == rc){
      err_io_error();
      goto error;
    }
    buf->size_POP3 += eol - buf->start; // written data
    buf->size_real += count; // processed bytes
    //err_debug(0, "AAAA: count: %d, eol: %x, start: %x, end: %x, last: %d",
    //          count, eol, buf->start, buf->end, last );
    //err_debug(0, "AAAA: size_real: %d", buf->size_real);

    buf->start += count; // skip processed data.
    //err_debug(0, "AAAA: start: %x", buf->start);

    if(eol < buf->end) // there was a NL
      break;

  } // ~for




  if(buf->size_POP3 - old_size_POP3 ||
     eol < buf->end
    ) // some data was processed!
  { 
    if ( buf->size_POP3 - old_size_POP3 )
      buf->type = defIsNormal; // some data was TXed till now ?
    else
      buf->type = defIsBlank;

    if('\r' == last) { // we had a CR
      rc = net_write(fd_out, "\n", 1);
      buf->size_POP3 += 1;
    } else {           // there was no CR
      rc = net_write(fd_out, "\r\n", 2);
      buf->size_POP3 += 2;
    }

    if(-1 == rc) {
      err_io_error();
      goto error;
    }
  } // ~if




  return 0;


error:
  buf->type = 0;
  return -1;
}
//----------------------------------------------------------------------------------------
int
buf_type (strMaildirBuf *buf
         )
{
  return buf->type;
}
//----------------------------------------------------------------------------------------
int
buf_size_pop3 (strMaildirBuf *buf
              )
{
  return buf->size_POP3;
}
//----------------------------------------------------------------------------------------
int
buf_size_real (strMaildirBuf *buf
              )
{
  return buf->size_real;
}
//----------------------------------------------------------------------------------------
void
buf_term (strMaildirBuf *buf
         )
{
  assert(buf);
  free(buf);
}
//----------------------------------------------------------------------------------------
int
mailstore_maildir_retr (strMailstoreHead *head,
                        int               msg_num,
                        int               fd,
                        int               lines
                       )
{
  strMaildirHead *my_head;
  strMaildirMsg  *my_msg;
  strMaildirBuf  *buf        =  NULL;
  char            file[MAXPATHLEN+1];
  int             line_count =  0;
  int             byte_count =  0;
  int             POP3_sent  =  0;
  int             msg_fd     = -1;
  int             rc         = -1;

  err_debug(4, "%s(): msg_num=%d lines=%d",
            __FUNCTION__, msg_num, lines);

  my_head = head->fsd;
  assert(my_head);
  if(!my_head) {
    err_internal_error();
    goto exit;
  }

  my_msg = head->msgs[msg_num].fsd;
  assert(my_msg);
  if(!my_msg) {
    err_internal_error();
    goto exit;
  }

  strncpy(file, "./cur/", MAXPATHLEN);
  file[MAXPATHLEN] = '\0';
  strncat(file, my_msg->fName, MAXPATHLEN);
  file[MAXPATHLEN] = '\0';

  msg_fd = open(file, O_RDONLY);
  if(-1 == msg_fd) {
    err_error(EX_NOINPUT, "ERROR: Unable to open \"%s\" for reading\n", file);
    goto exit;
  }

  buf = buf_init();
  if(!buf) {
    err_malloc_error();
    goto exit;
  }

  // send full header terminated by LF,LF or CR,LF,CR,LF
  for(;;) {
    buf_readwriteln (msg_fd, fd, buf);
    if(defIsNormal != buf_type(buf)) // EOF or BLANK
      break;
  }

  // send lines terminated by LF,LF or CR,LF,CR,LF
  for(;;) {
    buf_readwriteln (msg_fd, fd, buf);
    if(defIsEOF == buf_type(buf))
      break;

    if(-1 != lines) {
      if(line_count >= lines)
        break;
      line_count++;
    } // ~if
  } // ~for

  POP3_sent  = buf_size_pop3(buf);
  byte_count = buf_size_real(buf);

  //debug check... omit this in final version. (POP3_sent, byte_count).
  // TODO: identical code in mailbox!! BUGBUG FIXME
  //Make a function call for both maildir and mailbox
  ifdebug() {
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
  }
  rc = 0;

exit:
  if(-1 != msg_fd)
    close(msg_fd);
  if(buf)
    buf_term(buf);

  err_debug_return(rc);
}
//----------------------------------------------------------------------------------------
int mailstore_maildir_deliver(strMailstoreHead *head, int fd) {
  strMaildirHead *my_head;
  struct stat     st;
  struct md5_ctx  ctx;
  char            tmp_filename[MAXPATHLEN];
  char            new_filename[MAXPATHLEN];
  char            buf[defMailstoreBufSize];
  char            md5hash[16]; // 16x8 = 128b
  int             file_fd;
  int             size = 0;
  int             nsiz = 0;
  int             rc;
  int             i;

  err_debug_function();

  assert(head);
  assert(head->fsd);
  my_head = head->fsd;




    //////////////////////////////////////////////////////////////////////
  // 2. step: Create filename and stat()it, retrying.
  //////////////////////////////////////////////////////////////////////
  for(i=0; ; i++) {
    if(i >= defMaildirRetry) {
      rc = -1;
      err_error(EX_TEMPFAIL,
                "Unable to create unique filename in maildir: \"%s\","\
                "already tried delivery for %d times.",
                head->filename,
                i
               );
      goto exit;
    }

    rc = snprintf(tmp_filename, sizeof(tmp_filename),
                  "tmp/%d.%d.%s",
                  (int)time(NULL), getpid(), my_head->hostname
                 );
    if(-1 == rc || rc >= sizeof(tmp_filename)) {
      rc = -1;
      err_error(EX_USAGE,
                "Unable to create filename in maildir: \"%s\", reason: "\
                "name would exceed max path length of %d characters\n",
                head->filename,
                MAXPATHLEN
               );
      goto exit;
    }

    rc = stat(tmp_filename, &st);
    if(-1 == rc && ENOENT == errno)
      break;

    sleep(2);
  } // ~for





  // open the file
  rc = open(tmp_filename, O_WRONLY|O_CREAT|O_EXCL|O_TRUNC, head->create_mode);
  if(-1 == rc) {
    err_error(EX_CANTCREAT,
              "Unable to create file \"%s/%s\", reason: %s",
              head->filename,
              tmp_filename,
              strerror(errno)
             );
    goto exit;
  }
  file_fd = rc;

  // do delivery (devilry ;>)
  md5_init_ctx(&ctx);

  while(1) {
    rc = safe_read(fd, buf, sizeof(buf));
    if(-1 == rc) {
      goto io_error;
    }

    if(0 == rc)
      break;

    i = rc;
    md5_process_bytes(buf, i, &ctx);
    rc = safe_write(file_fd, buf, i);
    if(-1 == rc) {
      goto io_error;
    }

    if(rc != i) {
      rc = -1;
      err_error(EX_IOERR,
                "Write error. Unable to deliver message to maildir file: \"%s/%s\","\
                "reason: unable to write all data!",
                head->filename,
                tmp_filename
               );
      goto exit;
    }

    size += rc; // number of processed bytes.
    nsiz += rc;
    nsiz += count_lfs(buf, i);
  } // ~while

  nsiz += count_lfs(NULL, 0);
  md5_finish_ctx(&ctx, md5hash);




  rc = fsync(file_fd);
  if(-1 == rc) {
    goto io_error;
  }
  rc = close(file_fd);
  if(-1 == rc) {
    goto io_error_nc;
  }

  // create a new_filename

  snprintf(new_filename, sizeof(new_filename),
           "new%s", tmp_filename+3
          );
  new_filename[sizeof(new_filename)-1] = '\0';
  append_info(new_filename,
              sizeof(new_filename) - strlen(new_filename),
              md5_str(md5hash), size, nsiz, 0
             );


  rc = link(tmp_filename, new_filename);
  if(-1 == rc) {
    err_error(EX_IOERR,
              "Unable to link message to: \"%s/%s\", reason: %s",
              head->filename,
              new_filename,
              strerror(errno)
             );
    goto exit;
  }
  unlink(tmp_filename); // we don't care if this fails.
  err_debug(5, "Delivered to file: \"%s\"", new_filename);
  goto exit;

io_error:
  close(file_fd);
io_error_nc:
  err_error(EX_IOERR,
            "I/O error. Unable to deliver message to maildir file: \"%s/%s\", reason: %s",
            head->filename,
            tmp_filename,
            strerror(errno)
           );
  rc = unlink(tmp_filename);
  if(-1 == rc) {
    err_debug(0, "Unable to unlink file \"%s/%s\", reason: %s",
              head->filename, tmp_filename, strerror(errno)
             );
  }
  rc = -1;

exit:
  err_debug_return(rc);
}
//----------------------------------------------------------------------------------------
int
open_retr(strMailstoreHead *head) {
  strMaildirHead *my_head;
  struct dirent **files  = NULL;
  char            tmp[MAXPATHLEN];
  int             num;
  int             rc;
  int             i;

  err_debug_function();

  assert(head);
  assert(head->fsd);

  my_head = head->fsd;

  //////////////////////////////////////////////////////////////////////
  // 2. step: Move every message from new to cur.
  //////////////////////////////////////////////////////////////////////
  num = scandir(my_head->maildir_new, &files, maildir_file_filter, alphasort);
  if(num<0) {
    err_error(EX_UNAVAILABLE,
              "Unable to scan files in dir: \"%s\", reason: \"%s\"",
              my_head->maildir_new,
              strerror(errno)
             );
    goto error;
  }



  err_debug(4, "Searching for NEW maildir files\n");
  for(i=0; i<num; i++) {
    err_debug(5, "NEW File: %s\n", files[i]->d_name);
    my_head->maildir_cur[my_head->maildir_strlen] = '/';
    my_head->maildir_cur[my_head->maildir_strlen+1] = '\0'; // maildir_open already checked size!
    my_head->maildir_new[my_head->maildir_strlen] = '/';
    my_head->maildir_new[my_head->maildir_strlen+1] = '\0';
    strncat(my_head->maildir_new, files[i]->d_name,
            MAXPATHLEN - my_head->maildir_strlen );
    my_head->maildir_new[MAXPATHLEN - 1] = '\0';
    if(MAXPATHLEN <= strlen(my_head->maildir_new)) {
      err_name2long_error(files[i]->d_name);
      goto finish_for; // this is a soft error.
    }

    rc = process_file(&my_head->maildir_cur[my_head->maildir_strlen + 1],
                      MAXPATHLEN - my_head->maildir_strlen - 2,
                      my_head->maildir_new,
                      NULL
                     );

    if(-1 == rc) {
      err_debug(0, "ERROR: Can't process info data for maildir file \"%s\"\n", files[i]->d_name);
      goto finish_for;
    }

    err_debug(5, "Renaming \"%s\" to \"%s\"",
                my_head->maildir_new, my_head->maildir_cur );
    rc = rename(my_head->maildir_new, my_head->maildir_cur );
    if(-1 == rc) {
      err_debug(0, "ERROR: Can't rename \"%s\" to \"%s\", reason: %s",
                my_head->maildir_new, my_head->maildir_cur, strerror(errno));
      goto finish_for;
    }

  finish_for:
    free(files[i]);
    files[i] = NULL;
  } // ~for



  free(files);
  files = NULL;

  my_head->maildir_cur[my_head->maildir_strlen] = '\0';
  my_head->maildir_new[my_head->maildir_strlen] = '\0';






  //////////////////////////////////////////////////////////////////////
  // 3. step: Open cur, read in all messages; rename those without info.
  //////////////////////////////////////////////////////////////////////
  num = scandir(my_head->maildir_cur, &files, maildir_file_filter, alphasort);
  if(num<0) {
    err_error(EX_UNAVAILABLE,
              "Unable to scan files in dir: \"%s\", reason: \"%s\"",
              my_head->maildir_cur,
              strerror(errno)
             );
    goto error;
  }

  head->total_size = 0;
  head->msg_count = num;
  head->msgs = safe_calloc(head->msg_count, sizeof(head->msgs[0]));
  if(num && !head->msgs) {
    err_malloc_error();
    goto error;
  }

  my_head->maildir_cur[my_head->maildir_strlen] = '/';
  my_head->maildir_cur[my_head->maildir_strlen+1] = '\0';
  strcpy(tmp, my_head->maildir_cur);



  err_debug(4, "Searching for CUR maildir files\n");
  for(i=0; i<num; i++) {
    err_debug(5, "CUR File: %s\n", files[i]->d_name);
    my_head->maildir_cur[my_head->maildir_strlen+1] = '\0';
    strncat(my_head->maildir_cur, files[i]->d_name,
            MAXPATHLEN - my_head->maildir_strlen );
    my_head->maildir_cur[MAXPATHLEN - 1] = '\0';
    if(MAXPATHLEN <= strlen(my_head->maildir_cur)) {
      err_name2long_error(files[i]->d_name);
      goto finish_for2; // this is a soft error. BUGBUG todo.. make it hard!!!
    }

    rc = process_file(&tmp[my_head->maildir_strlen + 1],
                      MAXPATHLEN - my_head->maildir_strlen - 2,
                      my_head->maildir_cur,
                      &head->msgs[i]
                     );

    if(-1 == rc) {
      err_debug(0, "ERROR: Can't process info data for maildir file \"%s\"\n", files[i]->d_name);
      goto finish_for2;
    }

    if(1 == rc) { // fileNAME was changed!
      err_debug(5, "Renaming \"%s\" to \"%s\"",
                my_head->maildir_cur, tmp );
      rc = rename(my_head->maildir_cur, tmp);
      if(-1 == rc) {
        err_debug(0, "ERROR: Can't rename \"%s\" to \"%s\", reason: %s",
                  my_head->maildir_cur, tmp, strerror(errno));
        goto finish_for2; // this is a soft error. BUGBUG todo.. make it hard!!!
      }
    }
    head->total_size += head->msgs[i].size;

  finish_for2:
    free(files[i]);
    files[i] = NULL;
  } // ~for



  free(files);
  files = NULL;
  my_head->maildir_cur[my_head->maildir_strlen] = '\0';

  head->del_msg_count = head->msg_count;
  head->del_total_size = head->total_size;

  /*
  if(NULL != files) {
    for(i=0; i<num; i++)
      free(files[i]);
    free(files);
  }
  */
  err_debug_return(0);

error:
  err_debug_return(-1);
}
//----------------------------------------------------------------------------------------
int mailstore_maildir_open(strMailstoreHead *head) {
  strMaildirHead *my_head;
  int             rc = 0;

  err_debug_function();

  head->msgs = NULL;
  head->fsd  = NULL;

  // Catch MAXPATHLEN problems here
  if(strlen(head->filename) + sizeof("/new") + 2 >= MAXPATHLEN) {
      err_error(EX_USAGE,
                "Unable to access maildir: \"%s\", reason: "\
                "name would exceed max path length of %d characters\n",
                head->filename,
                MAXPATHLEN
               );
      goto error;
  }
  my_head = safe_calloc(1, sizeof(*my_head));
  if (!my_head)
    goto error;
  head->fsd = my_head;

  strcpy(my_head->maildir_new, head->filename);
  strcat(my_head->maildir_new, "/new");
  strcpy(my_head->maildir_cur, head->filename);
  strcat(my_head->maildir_cur, "/cur");
  strcpy(my_head->maildir_tmp, head->filename);
  strcat(my_head->maildir_tmp, "/tmp");
  my_head->maildir_strlen = strlen(my_head->maildir_new);

  rc = gethostname(my_head->hostname, sizeof(my_head->hostname));
  if(-1 == rc)
  {
    err_error(EX_OSERR,
              "Unable to get hostname, reason: \"%s\"",
              strerror(errno)
             );
    goto error;
  }
  my_head->hostname[sizeof(my_head->hostname)-1] = '\0'; // NULL terminate
  my_head->hostname_strlen = strlen(my_head->hostname);

  //////////////////////////////////////////////////////////////////////
  // 1. step: chdir/validate maildir
  //////////////////////////////////////////////////////////////////////
  rc = chdir(head->filename);

  if(-1 == rc) {
    if(ENOENT != errno)
      goto chdir_error;
    rc = maildir_make(head);
    if(-1 == rc)
      goto error;
    rc = chdir(head->filename);
    if(-1 == rc)
      goto chdir_error;
  }

  if(defMailstoreOpRetr == head->op) {
    rc = open_retr(head);
    if(-1 == rc)
      goto error;
  }
  err_debug_return(0);

chdir_error:
  err_error(EX_UNAVAILABLE,
            "Unable to chdir to: \"%s\", reason: \"%s\"",
            head->filename,
            strerror(errno)
           );
error:
  free(head->fsd);
  head->fsd = NULL;
  free(head->msgs);
  head->msgs = NULL;
  head->msg_count = 0;
  err_debug_return(rc);
}
//----------------------------------------------------------------------------------------
int mailstore_maildir_close  (strMailstoreHead *head, int commit) {
  strMaildirHead *my_head = NULL;
  strMaildirMsg  *my_msg  = NULL;
  int             rc      = 0;
  int             i       = 0;

  err_debug_function();
  err_debug(5, "  commit=%d\n", commit);

  assert(head);
  my_head = head->fsd;




  if(defMailstoreOpRetr == head->op && head->msgs && commit) {
    char tmp[MAXPATHLEN];

    for(i=0; i<head->msg_count; i++) {
      if(!head->msgs[i].deleted)
        continue;
      my_msg = head->msgs[i].fsd;
      strcpy(tmp, my_head->maildir_cur);
      strcat(tmp, "/");
      strcat(tmp, my_msg->fName);
      rc = unlink(tmp);
      if(-1 == rc) { // this is a soft error.
        err_debug(0, "ERROR: Unable, to remove the deleted message file %s, reason %s",
                  tmp,
                  strerror(errno)
                 );
        continue;
      } // ~if
      err_debug(5, "Removed maildir file \"%s\"", tmp);
    } // ~for

  } // ~if




  rc = 0;

//error:
  for(i=0; i<head->msg_count; i++)
    if(head->msgs[i].fsd) {
      my_msg = head->msgs[i].fsd;
      free(my_msg->fName);
      free(my_msg);
    }
  free(head->msgs);

  err_debug_return(rc);
}
//----------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------
int maildir_file_filter(const struct dirent *file) {
  if('.' == file->d_name[0]) // skip files with a leading dot.
    return 0;
  return 1;
}
//----------------------------------------------------------------------------------------
int maildir_mkdir(const char *dir, mode_t mode, uid_t uid, gid_t gid) {
  int rc = 0;

  rc = mkdir(dir, mode);
  if(-1 == rc) {
    if(EEXIST != errno) { // there is no error if it already exists
      err_error(EX_CANTCREAT,
                "Unable to create dir \"%s\", reason: \"%s\"",
                dir,
                strerror(errno)
               );
      goto error;
    }
  }

  rc = chown(dir, uid, gid);
  if(-1 == rc) {
    err_error(EX_NOPERM,
              "Unable to chown dir \"%s\", reason: \"%s\"",
              dir,
              strerror(errno)
             );
    goto error;
  }

  rc = chmod(dir, mode);
  if(-1 == rc) {
    err_error(EX_NOPERM,
              "Unable to chmod dir \"%s\", reason: \"%s\"",
              dir,
              strerror(errno)
             );
    goto error;
  }

error:
  return rc;
}
//----------------------------------------------------------------------------------------
// this function creates a maildir directory structure.
int maildir_make(strMailstoreHead *head) {
  strMaildirHead *my_head;
  uid_t           uid;
  gid_t           gid;
  mode_t          mode;
  int             rc = 0;

  err_debug_function();

  assert(head->fsd);
  my_head = head->fsd;
  mode = head->create_mode;

  // repair modes so every +r also gets a +x. We are
  // working with directories here.. :>
  if(mode&0400)
    mode|=0100;
  if(mode&0040)
    mode|=0010;
  if(mode&0004)
    mode|=0001;

  // what UID to use ?
  if(g.owner.uid_set)
    uid = g.owner.uid;
  else
    uid = -1;

  // what GID to use ?
  if(g.owner.gid_set)
    gid = g.owner.gid;
  else
    gid = -1;

  // create maildir
  rc = maildir_mkdir(head->filename, mode, uid, gid);
  if(-1 == rc)
    goto error;

  // create maildir/new
  rc = maildir_mkdir(my_head->maildir_new, mode, uid, gid);
  if(-1 == rc)
    goto error;

  // create maildir/cur
  rc = maildir_mkdir(my_head->maildir_cur, mode, uid, gid);
  if(-1 == rc)
    goto error;

  // create maildir/tmp
  rc = maildir_mkdir(my_head->maildir_tmp, mode, uid, gid);
  if(-1 == rc)
    goto error;

error:
  return rc;
}
