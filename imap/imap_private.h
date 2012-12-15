/*
 * Copyright (C) 1996-9 Brandon Long <blong@fiction.net>
 * Copyright (C) 1999-2008 Brendan Cully <brendan@kublai.com>
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
 *     Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */ 

#ifndef _IMAP_PRIVATE_H
#define _IMAP_PRIVATE_H 1

#include "imap.h"
#include "mutt_curses.h"
#include "mutt_socket.h"
#include "bcache.h"
#ifdef USE_HCACHE
#include "hcache.h"
#endif
#include "message.h"  /* for IMAP_HEADER_DATA */

/* -- symbols -- */
#define IMAP_PORT 143
#define IMAP_SSL_PORT 993

/* logging levels */
#define IMAP_LOG_CMD  2
#define IMAP_LOG_LTRL 4
#define IMAP_LOG_PASS 5

/* IMAP command responses. Used in IMAP_COMMAND.state too */
/* <tag> OK ... */
#define IMAP_CMD_OK       (0)
/* <tag> BAD ... */
#define IMAP_CMD_BAD      (-1)
/* <tag> NO ... */
#define IMAP_CMD_NO       (-2)
/* * ... */
#define IMAP_CMD_CONTINUE (1)
/* + */
#define IMAP_CMD_RESPOND  (2)
/* IMAP_COMMAND.state additions */
#define IMAP_CMD_NEW    (3)

/* number of entries in the hash table */
#define IMAP_CACHE_LEN 10

#define SEQLEN 5
/* maximum length of command lines before they must be split (for
 * lazy servers) */
#define IMAP_MAX_CMDLEN 1024

#define IMAP_REOPEN_ALLOW     (1<<0)
#define IMAP_EXPUNGE_EXPECTED (1<<1)
#define IMAP_EXPUNGE_PENDING  (1<<2)
#define IMAP_NEWMAIL_PENDING  (1<<3)
#define IMAP_FLAGS_PENDING    (1<<4)

/* imap_exec flags (see imap_exec) */
#define IMAP_CMD_FAIL_OK (1<<0)
#define IMAP_CMD_PASS    (1<<1)
#define IMAP_CMD_QUEUE   (1<<2)

/* length of "DD-MMM-YYYY HH:MM:SS +ZZzz" (null-terminated) */
#define IMAP_DATELEN 27

enum
{
  IMAP_FATAL = 1,
  IMAP_BYE
};

enum
{
  /* States */
  IMAP_DISCONNECTED = 0,
  IMAP_CONNECTED,
  IMAP_AUTHENTICATED,
  IMAP_SELECTED,
  
  /* and pseudo-states */
  IMAP_IDLE
};

enum
{
  /* Namespace types */
  IMAP_NS_PERSONAL = 0,
  IMAP_NS_OTHER,
  IMAP_NS_SHARED
};

/* Capabilities we are interested in */
enum
{
  IMAP4 = 0,
  IMAP4REV1,
  STATUS,
  ACL,				/* RFC 2086: IMAP4 ACL extension */
  NAMESPACE,                   	/* RFC 2342: IMAP4 Namespace */
  ACRAM_MD5,			/* RFC 2195: CRAM-MD5 authentication */
  AGSSAPI,			/* RFC 1731: GSSAPI authentication */
  AUTH_ANON,			/* AUTH=ANONYMOUS */
  STARTTLS,			/* RFC 2595: STARTTLS */
  LOGINDISABLED,		/*           LOGINDISABLED */
  IDLE,                         /* RFC 2177: IDLE */
  SASL_IR,                      /* SASL initial response draft */

  CAPMAX
};

/* imap_conn_find flags */
#define M_IMAP_CONN_NONEW    (1<<0)
#define M_IMAP_CONN_NOSELECT (1<<1)

/* -- data structures -- */
typedef struct
{
  unsigned int uid;
  char* path;
} IMAP_CACHE;

typedef struct
{
  char* name;

  unsigned int messages;
  unsigned int recent;
  unsigned int uidnext;
  unsigned int uidvalidity;
  unsigned int unseen;
} IMAP_STATUS;

typedef struct
{
  char* name;
  
  char delim;
  /* if we end up storing a lot of these we could turn this into a bitfield */
  unsigned char noselect;
  unsigned char noinferiors;
} IMAP_LIST;

/* IMAP command structure */
typedef struct
{
  char seq[SEQLEN+1];
  int state;
} IMAP_COMMAND;

typedef enum
{
  IMAP_CT_NONE = 0,
  IMAP_CT_LIST,
  IMAP_CT_STATUS
} IMAP_COMMAND_TYPE;

typedef struct
{
  /* This data is specific to a CONNECTION to an IMAP server */
  CONNECTION *conn;
  unsigned char state;
  unsigned char status;
  /* let me explain capstr: SASL needs the capability string (not bits).
   * we have 3 options:
   *   1. rerun CAPABILITY inside SASL function.
   *   2. build appropriate CAPABILITY string by reverse-engineering from bits.
   *   3. keep a copy until after authentication.
   * I've chosen (3) for now. (2) might not be too bad, but it involves
   * tracking all possible capabilities. bah. (1) I don't like because
   * it's just no fun to get the same information twice */
  char* capstr;
  unsigned char capabilities[(CAPMAX + 7)/8];
  unsigned int seqno;
  time_t lastread; /* last time we read a command for the server */
  char* buf;
  unsigned int blen;
  
  /* if set, the response parser will store results for complicated commands
   * here. */
  IMAP_COMMAND_TYPE cmdtype;
  void* cmddata;

  /* command queue */
  IMAP_COMMAND* cmds;
  int cmdslots;
  int nextcmd;
  int lastcmd;
  BUFFER* cmdbuf;

  /* cache IMAP_STATUS of visited mailboxes */
  LIST* mboxcache;

  /* The following data is all specific to the currently SELECTED mbox */
  char delim;
  CONTEXT *ctx;
  char *mailbox;
  unsigned short check_status;
  unsigned char reopen;
  unsigned int newMailCount;
  IMAP_CACHE cache[IMAP_CACHE_LEN];
  unsigned int uid_validity;
  unsigned int uidnext;
  body_cache_t *bcache;

  /* all folder flags - system flags AND keywords */
  LIST *flags;
#ifdef USE_HCACHE
  header_cache_t *hcache;
#endif
} IMAP_DATA;
/* I wish that were called IMAP_CONTEXT :( */

/* -- macros -- */
#define CTX_DATA ((IMAP_DATA *) ctx->data)

/* -- private IMAP functions -- */
/* imap.c */
int imap_create_mailbox (IMAP_DATA* idata, char* mailbox);
int imap_rename_mailbox (IMAP_DATA* idata, IMAP_MBOX* mx, const char* newname);
IMAP_STATUS* imap_mboxcache_get (IMAP_DATA* idata, const char* mbox,
                                 int create);
void imap_mboxcache_free (IMAP_DATA* idata);
int imap_exec_msgset (IMAP_DATA* idata, const char* pre, const char* post,
                      int flag, int changed, int invert);
int imap_open_connection (IMAP_DATA* idata);
void imap_close_connection (IMAP_DATA* idata);
IMAP_DATA* imap_conn_find (const ACCOUNT* account, int flags);
int imap_read_literal (FILE* fp, IMAP_DATA* idata, long bytes, progress_t*);
void imap_expunge_mailbox (IMAP_DATA* idata);
void imap_logout (IMAP_DATA** idata);
int imap_sync_message (IMAP_DATA *idata, HEADER *hdr, BUFFER *cmd,
  int *err_continue);
int imap_has_flag (LIST* flag_list, const char* flag);

/* auth.c */
int imap_authenticate (IMAP_DATA* idata);

/* command.c */
int imap_cmd_start (IMAP_DATA* idata, const char* cmd);
int imap_cmd_step (IMAP_DATA* idata);
void imap_cmd_finish (IMAP_DATA* idata);
int imap_code (const char* s);
const char* imap_cmd_trailer (IMAP_DATA* idata);
int imap_exec (IMAP_DATA* idata, const char* cmd, int flags);
int imap_cmd_idle (IMAP_DATA* idata);

/* message.c */
void imap_add_keywords (char* s, HEADER* keywords, LIST* mailbox_flags, size_t slen);
void imap_free_header_data (IMAP_HEADER_DATA** data);
int imap_read_headers (IMAP_DATA* idata, int msgbegin, int msgend);
char* imap_set_flags (IMAP_DATA* idata, HEADER* h, char* s);
int imap_cache_del (IMAP_DATA* idata, HEADER* h);
int imap_cache_clean (IMAP_DATA* idata);

/* util.c */
#ifdef USE_HCACHE
header_cache_t* imap_hcache_open (IMAP_DATA* idata, const char* path);
void imap_hcache_close (IMAP_DATA* idata);
HEADER* imap_hcache_get (IMAP_DATA* idata, unsigned int uid);
int imap_hcache_put (IMAP_DATA* idata, HEADER* h);
int imap_hcache_del (IMAP_DATA* idata, unsigned int uid);
#endif

int imap_continue (const char* msg, const char* resp);
void imap_error (const char* where, const char* msg);
IMAP_DATA* imap_new_idata (void);
void imap_free_idata (IMAP_DATA** idata);
char* imap_fix_path (IMAP_DATA* idata, const char* mailbox, char* path, 
  size_t plen);
void imap_cachepath(IMAP_DATA* idata, const char* mailbox, char* dest,
                    size_t dlen);
int imap_get_literal_count (const char* buf, long* bytes);
char* imap_get_qualifier (char* buf);
int imap_mxcmp (const char* mx1, const char* mx2);
char* imap_next_word (char* s);
time_t imap_parse_date (char* s);
void imap_make_date (char* buf, time_t timestamp);
void imap_qualify_path (char *dest, size_t len, IMAP_MBOX *mx, char* path);
void imap_quote_string (char* dest, size_t slen, const char* src);
void imap_unquote_string (char* s);
void imap_munge_mbox_name (char *dest, size_t dlen, const char *src);
void imap_unmunge_mbox_name (char *s);
int imap_wordcasecmp(const char *a, const char *b);

/* utf7.c */
void imap_utf7_encode (char **s);
void imap_utf7_decode (char **s);

#if USE_HCACHE
/* typedef size_t (*hcache_keylen_t)(const char* fn); */
#define imap_hcache_keylen mutt_strlen
#endif /* USE_HCACHE */

#endif
