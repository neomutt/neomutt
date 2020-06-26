/**
 * @file
 * Shared constants/structs that are private to IMAP
 *
 * @authors
 * Copyright (C) 1996-1999 Brandon Long <blong@fiction.net>
 * Copyright (C) 1999-2009 Brendan Cully <brendan@kublai.com>
 * Copyright (C) 2018 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_IMAP_PRIVATE_H
#define MUTT_IMAP_PRIVATE_H

#include "config.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "hcache/lib.h"

struct Account;
struct ConnAccount;
struct Email;
struct Mailbox;
struct Message;
struct Progress;

#define IMAP_PORT     143  ///< Default port for IMAP
#define IMAP_SSL_PORT 993  ///< Port for IMAP over SSL/TLS

/* logging levels */
#define IMAP_LOG_CMD  2
#define IMAP_LOG_LTRL 3
#define IMAP_LOG_PASS 5

/* IMAP command responses. Used in ImapCommand.state too */
#define IMAP_RES_NO       -2  ///< `<tag> NO ...`
#define IMAP_RES_BAD      -1  ///< `<tag> BAD ...`
#define IMAP_RES_OK        0  ///< `<tag> OK ...`
#define IMAP_RES_CONTINUE  1  ///< `* ...`
#define IMAP_RES_RESPOND   2  ///< `+`
#define IMAP_RES_NEW       3  ///< ImapCommand.state additions

#define SEQ_LEN 16
#define IMAP_MAX_CMDLEN 1024 ///< Maximum length of command lines before they must be split (for lazy servers)

typedef uint8_t ImapOpenFlags;         ///< Flags, e.g. #MUTT_THREAD_COLLAPSE
#define IMAP_OPEN_NO_FLAGS          0  ///< No flags are set
#define IMAP_REOPEN_ALLOW     (1 << 0) ///< Allow re-opening a folder upon expunge
#define IMAP_EXPUNGE_EXPECTED (1 << 1) ///< Messages will be expunged from the server
#define IMAP_EXPUNGE_PENDING  (1 << 2) ///< Messages on the server have been expunged
#define IMAP_NEWMAIL_PENDING  (1 << 3) ///< New mail is waiting on the server
#define IMAP_FLAGS_PENDING    (1 << 4) ///< Flags have changed on the server

typedef uint8_t ImapCmdFlags;          ///< Flags for imap_exec(), e.g. #IMAP_CMD_PASS
#define IMAP_CMD_NO_FLAGS          0   ///< No flags are set
#define IMAP_CMD_PASS        (1 << 0)  ///< Command contains a password. Suppress logging
#define IMAP_CMD_QUEUE       (1 << 1)  ///< Queue a command, do not execute
#define IMAP_CMD_POLL        (1 << 2)  ///< Poll the tcp connection before running the imap command
#define IMAP_CMD_SINGLE      (1 << 3)  ///< Run a single command

/**
 * enum ImapExecResult - imap_exec return code
 */
enum ImapExecResult
{
  IMAP_EXEC_SUCCESS = 0, ///< Imap command executed or queued successfully
  IMAP_EXEC_ERROR,       ///< Imap command failure
  IMAP_EXEC_FATAL,       ///< Imap connection failure
};

/* length of "DD-MMM-YYYY HH:MM:SS +ZZzz" (null-terminated) */
#define IMAP_DATELEN 27

/**
 * enum ImapFlags - IMAP server responses
 */
enum ImapFlags
{
  IMAP_FATAL = 1, ///< Unrecoverable error occurred
  IMAP_BYE,       ///< Logged out from server
};

/**
 * enum ImapState - IMAP connection state
 */
enum ImapState
{
  /* States */
  IMAP_DISCONNECTED = 0, ///< Disconnected from server
  IMAP_CONNECTED,        ///< Connected to server
  IMAP_AUTHENTICATED,    ///< Connection is authenticated
  IMAP_SELECTED,         ///< Mailbox is selected

  /* and pseudo-states */
  IMAP_IDLE, ///< Connection is idle
};

/**
 * typedef ImapCapFlags - Capabilities we are interested in
 *
 * @note This must be kept in the same order as Capabilities.
 */
typedef uint32_t ImapCapFlags;              ///< Flags, e.g. #IMAP_CAP_IMAP4
#define IMAP_CAP_NO_FLAGS                0  ///< No flags are set
#define IMAP_CAP_IMAP4            (1 <<  0) ///< Server supports IMAP4
#define IMAP_CAP_IMAP4REV1        (1 <<  1) ///< Server supports IMAP4rev1
#define IMAP_CAP_STATUS           (1 <<  2) ///< Server supports STATUS command
#define IMAP_CAP_ACL              (1 <<  3) ///< RFC2086: IMAP4 ACL extension
#define IMAP_CAP_NAMESPACE        (1 <<  4) ///< RFC2342: IMAP4 Namespace
#define IMAP_CAP_AUTH_CRAM_MD5    (1 <<  5) ///< RFC2195: CRAM-MD5 authentication
#define IMAP_CAP_AUTH_GSSAPI      (1 <<  6) ///< RFC1731: GSSAPI authentication
#define IMAP_CAP_AUTH_ANONYMOUS   (1 <<  7) ///< AUTH=ANONYMOUS
#define IMAP_CAP_AUTH_OAUTHBEARER (1 <<  8) ///< RFC7628: AUTH=OAUTHBEARER
#define IMAP_CAP_STARTTLS         (1 <<  9) ///< RFC2595: STARTTLS
#define IMAP_CAP_LOGINDISABLED    (1 << 10) ///< RFC2595: LOGINDISABLED
#define IMAP_CAP_IDLE             (1 << 11) ///< RFC2177: IDLE
#define IMAP_CAP_SASL_IR          (1 << 12) ///< SASL initial response draft
#define IMAP_CAP_ENABLE           (1 << 13) ///< RFC5161
#define IMAP_CAP_CONDSTORE        (1 << 14) ///< RFC7162
#define IMAP_CAP_QRESYNC          (1 << 15) ///< RFC7162
#define IMAP_CAP_LIST_EXTENDED    (1 << 16) ///< RFC5258: IMAP4 LIST Command Extensions
#define IMAP_CAP_COMPRESS         (1 << 17) ///< RFC4978: COMPRESS=DEFLATE
#define IMAP_CAP_X_GM_EXT_1       (1 << 18) ///< https://developers.google.com/gmail/imap/imap-extensions

#define IMAP_CAP_ALL             ((1 << 19) - 1)

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
  char seq[SEQ_LEN + 1]; ///< Command tag, e.g. 'a0001'
  int state;            ///< Command state, e.g. #IMAP_RES_NEW
};

/**
 * struct ImapAccountData - IMAP-specific Account data - @extends Account
 *
 * This data is specific to a Connection to an IMAP server
 */
struct ImapAccountData
{
  struct Connection *conn;
  bool recovering;
  bool closing; ///< If true, we are waiting for CLOSE completion
  unsigned char state;  ///< ImapState, e.g. #IMAP_AUTHENTICATED
  unsigned char status; ///< ImapFlags, e.g. #IMAP_FATAL
  /* let me explain capstr: SASL needs the capability string (not bits).
   * we have 3 options:
   *   1. rerun CAPABILITY inside SASL function.
   *   2. build appropriate CAPABILITY string by reverse-engineering from bits.
   *   3. keep a copy until after authentication.
   * I've chosen (3) for now. (2) might not be too bad, but it involves
   * tracking all possible capabilities. bah. (1) I don't like because
   * it's just no fun to get the same information twice */
  char *capstr;
  ImapCapFlags capabilities;
  unsigned char seqid; ///< tag sequence prefix
  unsigned int seqno; ///< tag sequence number, e.g. '{seqid}0001'
  time_t lastread; ///< last time we read a command for the server
  char *buf;
  size_t blen;

  bool unicode; ///< If true, we can send UTF-8, and the server will use UTF8 rather than mUTF7
  bool qresync; ///< true, if QRESYNC is successfully ENABLE'd

  // if set, the response parser will store results for complicated commands here
  struct ImapList *cmdresult;

  /* command queue */
  struct ImapCommand *cmds;
  int cmdslots;
  int nextcmd;
  int lastcmd;
  struct Buffer cmdbuf;

  char delim;
  struct Mailbox *mailbox;      ///< Current selected mailbox
  struct Mailbox *prev_mailbox; ///< Previously selected mailbox
  struct Account *account;      ///< Parent Account
};

/**
 * struct ImapMboxData - IMAP-specific Mailbox data - @extends Mailbox
 *
 * This data is specific to a Mailbox of an IMAP server
 */
struct ImapMboxData
{
  char *name;        ///< Mailbox name
  char *munge_name;  ///< Munged version of the mailbox name
  char *real_name;   ///< Original Mailbox name, e.g.: INBOX can be just \0

  ImapOpenFlags reopen;        ///< Flags, e.g. #IMAP_REOPEN_ALLOW
  ImapOpenFlags check_status;  ///< Flags, e.g. #IMAP_NEWMAIL_PENDING
  unsigned int new_mail_count; ///< Set when EXISTS notifies of new mail

  // IMAP STATUS information
  struct ListHead flags;
  uint32_t uidvalidity;
  unsigned int uid_next;
  unsigned long long modseq;
  unsigned int messages;
  unsigned int recent;
  unsigned int unseen;

  // Cached data used only when the mailbox is opened
  struct HashTable *uid_hash;
  struct Email **msn_index;   ///< look up headers by (MSN-1)
  size_t msn_index_size;       ///< allocation size
  unsigned int max_msn;        ///< the largest MSN fetched so far
  struct BodyCache *bcache;

  header_cache_t *hcache;
};

/**
 * struct SeqsetIterator - UID Sequence Set Iterator
 */
struct SeqsetIterator
{
  char *full_seqset;
  char *eostr;
  int in_range;
  int down;
  unsigned int range_cur;
  unsigned int range_end;
  char *substr_cur;
  char *substr_end;
};

/* -- private IMAP functions -- */
/* imap.c */
int imap_create_mailbox(struct ImapAccountData *adata, char *mailbox);
int imap_rename_mailbox(struct ImapAccountData *adata, char *oldname, const char *newname);
int imap_exec_msgset(struct Mailbox *m, const char *pre, const char *post,
                     int flag, bool changed, bool invert);
int imap_open_connection(struct ImapAccountData *adata);
void imap_close_connection(struct ImapAccountData *adata);
int imap_read_literal(FILE *fp, struct ImapAccountData *adata, unsigned long bytes, struct Progress *pbar);
void imap_expunge_mailbox(struct Mailbox *m);
int imap_login(struct ImapAccountData *adata);
int imap_sync_message_for_copy(struct Mailbox *m, struct Email *e, struct Buffer *cmd, enum QuadOption *err_continue);
bool imap_has_flag(struct ListHead *flag_list, const char *flag);
int imap_adata_find(const char *path, struct ImapAccountData **adata, struct ImapMboxData **mdata);

/* auth.c */
int imap_authenticate(struct ImapAccountData *adata);

/* command.c */
int imap_cmd_start(struct ImapAccountData *adata, const char *cmdstr);
int imap_cmd_step(struct ImapAccountData *adata);
void imap_cmd_finish(struct ImapAccountData *adata);
bool imap_code(const char *s);
const char *imap_cmd_trailer(struct ImapAccountData *adata);
int imap_exec(struct ImapAccountData *adata, const char *cmdstr, ImapCmdFlags flags);
int imap_cmd_idle(struct ImapAccountData *adata);

/* message.c */
void imap_edata_free(void **ptr);
struct ImapEmailData *imap_edata_get(struct Email *e);
int imap_read_headers(struct Mailbox *m, unsigned int msn_begin, unsigned int msn_end, bool initial_download);
char *imap_set_flags(struct Mailbox *m, struct Email *e, char *s, bool *server_changes);
int imap_cache_del(struct Mailbox *m, struct Email *e);
int imap_cache_clean(struct Mailbox *m);
int imap_append_message(struct Mailbox *m, struct Message *msg);

int imap_msg_open(struct Mailbox *m, struct Message *msg, int msgno);
int imap_msg_close(struct Mailbox *m, struct Message *msg);
int imap_msg_commit(struct Mailbox *m, struct Message *msg);
int imap_msg_save_hcache(struct Mailbox *m, struct Email *e);

/* util.c */
struct ImapAccountData *imap_adata_get(struct Mailbox *m);
struct ImapMboxData *imap_mdata_get(struct Mailbox *m);
#ifdef USE_HCACHE
void imap_hcache_open(struct ImapAccountData *adata, struct ImapMboxData *mdata);
void imap_hcache_close(struct ImapMboxData *mdata);
struct Email *imap_hcache_get(struct ImapMboxData *mdata, unsigned int uid);
int imap_hcache_put(struct ImapMboxData *mdata, struct Email *e);
int imap_hcache_del(struct ImapMboxData *mdata, unsigned int uid);
int imap_hcache_store_uid_seqset(struct ImapMboxData *mdata);
int imap_hcache_clear_uid_seqset(struct ImapMboxData *mdata);
char *imap_hcache_get_uid_seqset(struct ImapMboxData *mdata);
#endif

enum QuadOption imap_continue(const char *msg, const char *resp);
void imap_error(const char *where, const char *msg);
struct ImapAccountData *imap_adata_new(struct Account *a);
void imap_adata_free(void **ptr);
struct ImapMboxData *imap_mdata_new(struct ImapAccountData *adata, const char* name);
void imap_mdata_free(void **ptr);
void imap_mdata_cache_reset(struct ImapMboxData *mdata);
char *imap_fix_path(char delim, const char *mailbox, char *path, size_t plen);
void imap_cachepath(char delim, const char *mailbox, struct Buffer *dest);
int imap_get_literal_count(const char *buf, unsigned int *bytes);
char *imap_get_qualifier(char *buf);
char *imap_next_word(char *s);
void imap_qualify_path(char *buf, size_t buflen, struct ConnAccount *conn_account, char *path);
void imap_quote_string(char *dest, size_t dlen, const char *src, bool quote_backtick);
void imap_unquote_string(char *s);
void imap_munge_mbox_name(bool unicode, char *dest, size_t dlen, const char *src);
void imap_unmunge_mbox_name(bool unicode, char *s);
struct SeqsetIterator *mutt_seqset_iterator_new(const char *seqset);
int mutt_seqset_iterator_next(struct SeqsetIterator *iter, unsigned int *next);
void mutt_seqset_iterator_free(struct SeqsetIterator **ptr);
bool imap_account_match(const struct ConnAccount *a1, const struct ConnAccount *a2);
void imap_get_parent(const char *mbox, char delim, char *buf, size_t buflen);
bool  mutt_account_match(const struct ConnAccount *a1, const struct ConnAccount *a2);

/* utf7.c */
void imap_utf_encode(bool unicode, char **s);
void imap_utf_decode(bool unicode, char **s);
void imap_allow_reopen(struct Mailbox *m);
void imap_disallow_reopen(struct Mailbox *m);

/* search.c */
void cmd_parse_search(struct ImapAccountData *adata, const char *s);

#endif /* MUTT_IMAP_PRIVATE_H */
