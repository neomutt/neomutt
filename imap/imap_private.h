/*
 * Copyright (C) 1996-9 Brandon Long <blong@fiction.net>
 * Copyright (C) 1999 Brendan Cully <brendan@kublai.com>
 * 
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 * 
 *     This program is distributed in the hope that it will be useful,
 *     but WITHOUT ANY WARRANTY; without even the implied warranty of
 *     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *     GNU General Public License for more details.
 * 
 *     You should have received a copy of the GNU General Public License
 *     along with this program; if not, write to the Free Software
 *     Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */ 

#ifndef _IMAP_PRIVATE_H
#define _IMAP_PRIVATE_H 1

#include "imap_socket.h"

/* -- symbols -- */
#define IMAP_PORT 143

/* number of entries in the hash table */
#define IMAP_CACHE_LEN 10

#define SEQLEN 5

enum
{
  IMAP_FATAL = 1,
  IMAP_NEW_MAIL,
  IMAP_EXPUNGE,
  IMAP_BYE,
  IMAP_OK_FAIL,
  IMAP_REOPENED,
  IMAP_LOGOUT
};

enum
{
  /* States */
  IMAP_DISCONNECTED = 0,
  IMAP_CONNECTED,
  IMAP_AUTHENTICATED,
  IMAP_SELECTED
};

enum
{
  /* Namespace types */
  IMAP_NS_PERSONAL = 0,
  IMAP_NS_OTHER,
  IMAP_NS_SHARED
};

/* ACL Rights */
enum
{
  IMAP_ACL_LOOKUP = 0,
  IMAP_ACL_READ,
  IMAP_ACL_SEEN,
  IMAP_ACL_WRITE,
  IMAP_ACL_INSERT,
  IMAP_ACL_POST,
  IMAP_ACL_CREATE,
  IMAP_ACL_DELETE,
  IMAP_ACL_ADMIN,

  RIGHTSMAX
};

/* Capabilities */
enum
{
  IMAP4 = 0,
  IMAP4REV1,
  STATUS,
  ACL,				/* RFC 2086: IMAP4 ACL extension */
  NAMESPACE,                   	/* RFC 2342: IMAP4 Namespace */
  ACRAM_MD5,			/* RFC 2195: CRAM-MD5 authentication */
  AKERBEROS_V4,			/* AUTH=KERBEROS_V4 */
  AGSSAPI,			/* RFC 1731: GSSAPI authentication */
  /* From here down, we don't care */
  ALOGIN,			/* AUTH=LOGIN */
  AUTH_LOGIN,			/* AUTH-LOGIN */
  APLAIN,			/* AUTH=PLAIN */
  ASKEY,			/* AUTH=SKEY */
  IDLE,				/* RFC 2177: IMAP4 IDLE command */
  LOGIN_REFERRALS,		/* LOGIN-REFERRALS */
  MAILBOX_REFERRALS,		/* MAILBOX-REFERRALS */
  SCAN,
  SORT,
  TORDEREDSUBJECT,		/* THREAD=ORDEREDSUBJECT */
  UIDPLUS,			/* RFC 2859: IMAP4 UIDPLUS extension */
  AUTH_ANON,			/* AUTH=ANONYMOUS */
  
  CAPMAX
};

/* -- data structures -- */
typedef struct
{
  unsigned int index;
  char *path;
} IMAP_CACHE;

typedef struct 
{
  int type;
  int listable;
  char *prefix;
  char delim;
  int home_namespace;
  /* We get these when we check if namespace exists - cache them */
  int noselect;			
  int noinferiors;
} IMAP_NAMESPACE_INFO;

typedef struct
{
  /* This data is specific to a CONNECTION to an IMAP server */
  short status;
  short state;
  short check_status;
  char delim;
  unsigned char capabilities[(CAPMAX + 7)/8];
  CONNECTION *conn;

  /* The following data is all specific to the currently SELECTED mbox */
  CONTEXT *selected_ctx;
  char *selected_mailbox;
  unsigned char rights[(RIGHTSMAX + 7)/8];
  unsigned int newMailCount;
  IMAP_CACHE cache[IMAP_CACHE_LEN];
  /* all folder flags - system flags AND keywords */
  LIST *flags;
} IMAP_DATA;

/* -- macros -- */
#define CTX_DATA ((IMAP_DATA *) ctx->data)
#define CONN_DATA ((IMAP_DATA *) conn->data)

/* -- private IMAP functions -- */
/* imap.c */
int imap_code (const char* s);
int imap_create_mailbox (IMAP_DATA* idata, char* mailbox);
int imap_exec (char* buf, size_t buflen, IMAP_DATA* idata, const char* cmd,
  int flags);
int imap_handle_untagged (IMAP_DATA* idata, char* s);
int imap_make_msg_set (char* buf, size_t buflen, CONTEXT* ctx, int flag,
  int changed);
int imap_open_connection (IMAP_DATA* idata, CONNECTION* conn);
time_t imap_parse_date (char* s);
int imap_parse_list_response(CONNECTION* conn, char* buf, int buflen,
  char** name, int* noselect, int* noinferiors, char* delim);
int imap_read_bytes (FILE* fp, CONNECTION* conn, long bytes);

/* auth.c */
int imap_authenticate (IMAP_DATA *idata, CONNECTION *conn);

/* message.c */
void imap_add_keywords (char* s, HEADER* keywords, LIST* mailbox_flags);
void imap_free_header_data (void** data);
int imap_read_headers (CONTEXT* ctx, int msgbegin, int msgend);

/* util.c */
int imap_error (const char* where, const char* msg);
char* imap_fix_path (IMAP_DATA* idata, char* mailbox, char* path, 
  size_t plen);
int imap_get_literal_count (const char* buf, long* bytes);
void imap_make_sequence (char *buf, size_t buflen);
char* imap_next_word (char* s);
void imap_quote_string (char* dest, size_t slen, const char* src);
void imap_unquote_string (char* s);
#endif
