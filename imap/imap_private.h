/**
 * @file
 * Shared constants/structs that are private to IMAP
 *
 * @authors
 * Copyright (C) 1996-1999 Brandon Long <blong@fiction.net>
 * Copyright (C) 1999-2009 Brendan Cully <brendan@kublai.com>
 *
 * @copyright
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 2 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _MUTT_IMAP_PRIVATE_H
#define _MUTT_IMAP_PRIVATE_H

#include <stdbool.h>
#include <stdio.h>
#include <time.h>
#include "lib/list.h"
#ifdef USE_HCACHE
#include "hcache/hcache.h"
#endif

struct Account;
struct Buffer;
struct Context;
struct Header;
struct ImapHeaderData;
struct ImapMbox;
struct Message;
struct Progress;

/* -- symbols -- */
#define IMAP_PORT 143
#define IMAP_SSL_PORT 993

/* logging levels */
#define IMAP_LOG_CMD  2
#define IMAP_LOG_LTRL 4
#define IMAP_LOG_PASS 5

/* IMAP command responses. Used in ImapCommand.state too */
#define IMAP_CMD_OK       (0)  /**< `<tag> OK ...` */
#define IMAP_CMD_BAD      (-1) /**< `<tag> BAD ...` */
#define IMAP_CMD_NO       (-2) /**< `<tag> NO ...` */
#define IMAP_CMD_CONTINUE (1)  /**< `* ...` */
#define IMAP_CMD_RESPOND  (2)  /**< `+` */
#define IMAP_CMD_NEW      (3)  /**< ImapCommand.state additions */

/* number of entries in the hash table */
#define IMAP_CACHE_LEN 10

#define SEQLEN 5
/* maximum length of command lines before they must be split (for
 * lazy servers) */
#define IMAP_MAX_CMDLEN 1024

#define IMAP_REOPEN_ALLOW     (1 << 0)
#define IMAP_EXPUNGE_EXPECTED (1 << 1)
#define IMAP_EXPUNGE_PENDING  (1 << 2)
#define IMAP_NEWMAIL_PENDING  (1 << 3)
#define IMAP_FLAGS_PENDING    (1 << 4)

/* imap_exec flags (see imap_exec) */
#define IMAP_CMD_FAIL_OK (1 << 0)
#define IMAP_CMD_PASS    (1 << 1)
#define IMAP_CMD_QUEUE   (1 << 2)
#define IMAP_CMD_POLL    (1 << 3)

/* length of "DD-MMM-YYYY HH:MM:SS +ZZzz" (null-terminated) */
#define IMAP_DATELEN 27

/**
 * enum ImapFlags - IMAP server responses
 */
enum ImapFlags
{
  IMAP_FATAL = 1,
  IMAP_BYE
};

/**
 * enum ImapState - IMAP connection state
 */
enum ImapState
{
  /* States */
  IMAP_DISCONNECTED = 0,
  IMAP_CONNECTED,
  IMAP_AUTHENTICATED,
  IMAP_SELECTED,

  /* and pseudo-states */
  IMAP_IDLE
};

/**
 * enum ImapNamespace - IMAP namespace types
 */
enum ImapNamespace
{
  IMAP_NS_PERSONAL = 0,
  IMAP_NS_OTHER,
  IMAP_NS_SHARED
};

/**
 * enum ImapCaps - Capabilities we are interested in
 */
enum ImapCaps
{
  IMAP4 = 0,
  IMAP4REV1,
  STATUS,
  ACL,           /**< RFC2086: IMAP4 ACL extension */
  NAMESPACE,     /**< RFC2342: IMAP4 Namespace */
  ACRAM_MD5,     /**< RFC2195: CRAM-MD5 authentication */
  AGSSAPI,       /**< RFC1731: GSSAPI authentication */
  AUTH_ANON,     /**< AUTH=ANONYMOUS */
  STARTTLS,      /**< RFC2595: STARTTLS */
  LOGINDISABLED, /**<           LOGINDISABLED */
  IDLE,          /**< RFC2177: IDLE */
  SASL_IR,       /**< SASL initial response draft */
  ENABLE,        /**< RFC5161 */
  X_GM_EXT1,     /**< https://developers.google.com/gmail/imap/imap-extensions */

  CAPMAX
};

/* imap_conn_find flags */
#define MUTT_IMAP_CONN_NONEW    (1 << 0)
#define MUTT_IMAP_CONN_NOSELECT (1 << 1)

/**
 * struct ImapCache - IMAP-specific message cache
 */
struct ImapCache
{
  unsigned int uid;
  char *path;
};

/**
 * struct ImapStatus - Status of an IMAP mailbox
 */
struct ImapStatus
{
  char *name;

  unsigned int messages;
  unsigned int recent;
  unsigned int uidnext;
  unsigned int uidvalidity;
  unsigned int unseen;
};

/**
 * struct ImapList - Items in an IMAP browser
 */
struct ImapList
{
  char *name;
  char delim;
  bool noselect;
  bool noinferiors;
};

/**
 * struct ImapCommand - IMAP command structure
 */
struct ImapCommand
{
  char seq[SEQLEN + 1];
  int state;
};

/**
 * enum ImapCommandType - IMAP command type
 */
enum ImapCommandType
{
  IMAP_CT_NONE = 0,
  IMAP_CT_LIST,
  IMAP_CT_STATUS
};

/**
 * struct ImapData - IMAP-specific server data
 *
 * This data is specific to a Connection to an IMAP server
 */
struct ImapData
{
  struct Connection *conn;
  bool recovering;
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
  char *capstr;
  unsigned char capabilities[(CAPMAX + 7) / 8];
  unsigned int seqno;
  time_t lastread; /**< last time we read a command for the server */
  char *buf;
  unsigned int blen;

  /* If nonzero, we can send UTF-8, and the server will use UTF8 rather
   * than mUTF7 */
  int unicode;

  /* if set, the response parser will store results for complicated commands
   * here. */
  enum ImapCommandType cmdtype;
  void *cmddata;

  /* command queue */
  struct ImapCommand *cmds;
  int cmdslots;
  int nextcmd;
  int lastcmd;
  struct Buffer *cmdbuf;

  /* cache ImapStatus of visited mailboxes */
  struct ListHead mboxcache;

  /* The following data is all specific to the currently SELECTED mbox */
  char delim;
  struct Context *ctx;
  char *mailbox;
  unsigned short check_status;
  unsigned char reopen;
  unsigned int new_mail_count; /**< Set when EXISTS notifies of new mail */
  struct ImapCache cache[IMAP_CACHE_LEN];
  struct Hash *uid_hash;
  unsigned int uid_validity;
  unsigned int uidnext;
  struct Header **msn_index;   /**< look up headers by (MSN-1) */
  unsigned int msn_index_size; /**< allocation size */
  unsigned int max_msn;        /**< the largest MSN fetched so far */
  struct BodyCache *bcache;

  /* all folder flags - system AND custom flags */
  struct ListHead flags;
#ifdef USE_HCACHE
  header_cache_t *hcache;
#endif
};
/* I wish that were called IMAP_CONTEXT :( */

/* -- private IMAP functions -- */
/* imap.c */
int imap_check(struct ImapData *idata, int force);
int imap_create_mailbox(struct ImapData *idata, char *mailbox);
int imap_rename_mailbox(struct ImapData *idata, struct ImapMbox *mx, const char *newname);
struct ImapStatus *imap_mboxcache_get(struct ImapData *idata, const char *mbox, int create);
void imap_mboxcache_free(struct ImapData *idata);
int imap_exec_msgset(struct ImapData *idata, const char *pre, const char *post,
                     int flag, int changed, int invert);
int imap_open_connection(struct ImapData *idata);
void imap_close_connection(struct ImapData *idata);
struct ImapData *imap_conn_find(const struct Account *account, int flags);
int imap_read_literal(FILE *fp, struct ImapData *idata, long bytes, struct Progress *pbar);
void imap_expunge_mailbox(struct ImapData *idata);
void imap_logout(struct ImapData **idata);
int imap_sync_message_for_copy(struct ImapData *idata, struct Header *hdr, struct Buffer *cmd, int *err_continue);
bool imap_has_flag(struct ListHead *flag_list, const char *flag);

/* auth.c */
int imap_authenticate(struct ImapData *idata);

/* command.c */
int imap_cmd_start(struct ImapData *idata, const char *cmd);
int imap_cmd_step(struct ImapData *idata);
void imap_cmd_finish(struct ImapData *idata);
int imap_code(const char *s);
const char *imap_cmd_trailer(struct ImapData *idata);
int imap_exec(struct ImapData *idata, const char *cmd, int flags);
int imap_cmd_idle(struct ImapData *idata);

/* message.c */
void imap_free_header_data(struct ImapHeaderData **data);
int imap_read_headers(struct ImapData *idata, unsigned int msn_begin, unsigned int msn_end);
char *imap_set_flags(struct ImapData *idata, struct Header *h, char *s, int *server_changes);
int imap_cache_del(struct ImapData *idata, struct Header *h);
int imap_cache_clean(struct ImapData *idata);

int imap_fetch_message(struct Context *ctx, struct Message *msg, int msgno);
int imap_close_message(struct Context *ctx, struct Message *msg);
int imap_commit_message(struct Context *ctx, struct Message *msg);

/* util.c */
#ifdef USE_HCACHE
header_cache_t *imap_hcache_open(struct ImapData *idata, const char *path);
void imap_hcache_close(struct ImapData *idata);
struct Header *imap_hcache_get(struct ImapData *idata, unsigned int uid);
int imap_hcache_put(struct ImapData *idata, struct Header *h);
int imap_hcache_del(struct ImapData *idata, unsigned int uid);
#endif

int imap_continue(const char *msg, const char *resp);
void imap_error(const char *where, const char *msg);
struct ImapData *imap_new_idata(void);
void imap_free_idata(struct ImapData **idata);
char *imap_fix_path(struct ImapData *idata, const char *mailbox, char *path, size_t plen);
void imap_cachepath(struct ImapData *idata, const char *mailbox, char *dest, size_t dlen);
int imap_get_literal_count(const char *buf, long *bytes);
char *imap_get_qualifier(char *buf);
int imap_mxcmp(const char *mx1, const char *mx2);
char *imap_next_word(char *s);
void imap_qualify_path(char *dest, size_t len, struct ImapMbox *mx, char *path);
void imap_quote_string(char *dest, size_t slen, const char *src);
void imap_unquote_string(char *s);
void imap_munge_mbox_name(struct ImapData *idata, char *dest, size_t dlen, const char *src);
void imap_unmunge_mbox_name(struct ImapData *idata, char *s);

/* utf7.c */
void imap_utf_encode(struct ImapData *idata, char **s);
void imap_utf_decode(struct ImapData *idata, char **s);

#ifdef USE_HCACHE
#define imap_hcache_keylen mutt_strlen
#endif /* USE_HCACHE */

#endif /* _MUTT_IMAP_PRIVATE_H */
