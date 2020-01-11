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
#include "sqlite_mailbox.h"
#include "error_facility.h"

static int mailbox_commit       (strMailstoreHead *head);
static int parse_mailbox        (strMailstoreHead *head);

//----------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------
int mailstore_sqlite_mailbox_open(strMailstoreHead *head) {
  strMailboxHead_sqlite *my_head;

  err_debug_function();

  head->msgs = NULL;

  my_head = safe_calloc(1, sizeof(*my_head));
  if (!my_head) goto error;

  head->fsd = my_head;

  char filename[4096];
  snprintf( filename, sizeof(filename), "%s.db", head->filename );

  int rc = sqlite3_open(filename, &my_head->db);
  if ( rc != SQLITE_OK ) {
    err_error(EX_NOINPUT,
              "Unable to open mailbox file: \"%s\", reason: \"%s\"",
              filename,
              sqlite3_errstr( rc )
             );
    goto error;
  }

  // perform exclusive lock
  sqlite3_stmt *query = 0;

  sqlite3_prepare_v2( my_head->db,
	"BEGIN TRANSACTION", -1, &query, 0 );
  for (;;) {
	rc = sqlite3_step( query );
	if ( rc == SQLITE_DONE )
		break;
	else if ( rc == SQLITE_ROW )
		;
	else if ( rc == SQLITE_BUSY )
		usleep(200000);
	else {
  		sqlite3_finalize( query );
		sqlite3_close( my_head->db );
		puts( "sqlite failed to start transaction" );
		goto error;
	}
  }
  sqlite3_finalize( query );

  rc = parse_mailbox(head);
  if (-1 == rc) // writes its own error messages
    goto error;

  err_debug_return(0);

error:
  free(my_head);
  head->fsd = NULL;
  err_debug_return(-1);
}
//----------------------------------------------------------------------------------------
int mailstore_sqlite_mailbox_close(strMailstoreHead *head, int commit) {
  strMailboxHead_sqlite *my_head;
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
  sqlite3_close( my_head->db );
  free(head->msgs);

  err_debug_return(rc);
}
//----------------------------------------------------------------------------------------
int mailstore_sqlite_mailbox_retr (strMailstoreHead *head, int msg_num, int fd, int lines) {
  strMailboxHead_sqlite *my_head;

  err_debug_function();

  my_head = head->fsd;
  assert(my_head);
  if(!my_head) {
    err_internal_error();
    goto error;
  }

  sqlite3_stmt *query = 0;
  sqlite3_prepare_v2( my_head->db, "SELECT text FROM messages WHERE msg_id = :msg_id", -1, &query, 0 );

  int64_t uidl;
  sscanf( head->msgs[msg_num].uidl, "%llx", (long long unsigned*)&uidl );
  sqlite3_bind_int64( query, 1, uidl );

  for (;;) {
	int rc = sqlite3_step( query );
	if ( rc == SQLITE_DONE )
		break;
	else if ( rc == SQLITE_ROW )
  		rc = net_write(fd, sqlite3_column_text(query, 0), sqlite3_column_bytes(query, 0));
	else if ( rc == SQLITE_BUSY )
		usleep(200000);
	else {
		err_debug( 0, "sqlite failed to get message text for %s.db", head->filename );
  		sqlite3_finalize( query );
		goto error;
	}
  }
  sqlite3_finalize( query );

  err_debug_return(0);

error:
  err_debug_return(-1);
}
//----------------------------------------------------------------------------------------
// From with timestamp and envelope from must already be appended
// >From quoting will be performed by this function
// data is being read from file descriptor 'fd' and written to the mailbox.
int mailstore_sqlite_mailbox_deliver(strMailstoreHead *head, int fd) {
  err_error(EX_IOERR,
            "Delivery currently not implemented"
           );

  err_debug_return(-1);
}
//----------------------------------------------------------------------------------------
static int mailbox_commit(strMailstoreHead *head) {
  strMailboxHead_sqlite *my_head;
  sqlite3_stmt *query = 0;

  err_debug_function();
  assert(head);

  my_head = head->fsd;

  sqlite3_prepare_v2( my_head->db, "DELETE FROM messages WHERE msg_id = :msg_id", -1, &query, 0 );
  for ( int i = 0; i < head->msg_count; i++ ) {
	  if ( !head->msgs[i].deleted ) continue;

	  int64_t uidl;
	  sscanf( head->msgs[i].uidl, "%llx", (long long unsigned*)&uidl );
	  sqlite3_bind_int64( query, 1, uidl );

	  for (;;) {
		int rc = sqlite3_step( query );
		if ( rc == SQLITE_DONE )
			break;
		else if ( rc == SQLITE_ROW )
			;
		else if ( rc == SQLITE_BUSY )
			usleep(200000);
		else {
			err_debug( 0, "sqlite failed to delete message for %s.db", head->filename );
  			sqlite3_finalize( query );
			err_debug_return(-1);
		}
	  }
	  sqlite3_reset( query );
  }
  sqlite3_finalize( query );

  sqlite3_prepare_v2( my_head->db, "COMMIT", -1, &query, 0 );

  for (;;) {
	int rc = sqlite3_step( query );
	if ( rc == SQLITE_DONE )
		break;
	else if ( rc == SQLITE_ROW )
		;
	else if ( rc == SQLITE_BUSY )
		usleep(200000);
	else {
		err_debug( 0, "sqlite failed to commit transaction for %s.db", head->filename );
  		sqlite3_finalize( query );
		err_debug_return(-1);
	}
  }
  sqlite3_finalize( query );

  err_debug_return(0);
}
//----------------------------------------------------------------------------------------
static int parse_mailbox(strMailstoreHead *head) {
  strMailboxHead_sqlite *my_head = head->fsd;

  err_debug_function();

  head->msg_count = 0;
  head->total_size = 0;

  if(!head->msgs) {
    my_head->msg_alloc = defMailboxMsgCount_sqlite;
    head->msgs = safe_calloc(my_head->msg_alloc, sizeof(head->msgs[0]));
  }

  sqlite3_stmt *query = 0;
  int rc;

  for (;;) {
	rc = sqlite3_prepare_v2( my_head->db, "SELECT msg_id, text FROM messages ORDER BY msg_id", -1, &query, 0 );
	if ( rc == SQLITE_OK )
		break;
	else if ( rc == SQLITE_BUSY )
		usleep( 200000 );
	else {
		err_debug( 0, "sqlite failed (%d) to prepare get messages list for %s.db", rc, head->filename );
		err_debug_return(-1);
	}
  }

  for (;;) {
	int rc = sqlite3_step( query );
	if ( rc == SQLITE_DONE )
		break;
	else if ( rc == SQLITE_ROW ) {
		head->msg_count++;

		if(head->msg_count > my_head->msg_alloc) {
			strMailstoreMsg *new_msgs = realloc(head->msgs, 2*my_head->msg_alloc * sizeof(head->msgs[0]));
			if(!new_msgs)
				goto malloc_error;
			memset( &new_msgs[my_head->msg_alloc], 0x00,
				sizeof(new_msgs[0]) * my_head->msg_alloc );
			my_head->msg_alloc *= 2;
			head->msgs = new_msgs;
		}

		int64_t uidl = sqlite3_column_int64( query, 0 );
		sprintf( head->msgs[head->msg_count-1].uidl, "%llx", (long long unsigned)uidl );

		int size = sqlite3_column_bytes( query, 1 );
		head->msgs[head->msg_count-1].size = size;

		head->total_size += size;
	}
	else if ( rc == SQLITE_BUSY )
		usleep(200000);
	else {
		err_debug( 0, "sqlite failed (%d) to get messages list for %s.db", rc, head->filename );
		goto io_error;
	}
  }
  sqlite3_finalize( query );

  err_debug_return(0);

io_error:
  err_io_error();
  goto error;
malloc_error:
  err_malloc_error();
error:
  sqlite3_finalize( query );

  mailstore_sqlite_mailbox_close(head, 0);
  memset(head->msgs, 0x00, sizeof(head->msgs[0]) * head->msg_count); // all data is invalid

  err_debug_return(-1);
}
