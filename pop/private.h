/**
 * @file
 * POP network mailbox
 *
 * @authors
 * Copyright (C) 2000-2003 Vsevolod Volkov <vvv@mutt.org.ua>
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

#ifndef MUTT_POP_PRIVATE_H
#define MUTT_POP_PRIVATE_H

#include <stdbool.h>
#include <time.h>
#include "mutt/lib.h"
#include "core/lib.h"
#include "conn/lib.h"

struct Email;
struct Mailbox;
struct Path;
struct Progress;
struct stat;


#define POP_PORT 110
#define POP_SSL_PORT 995

/* number of entries in the hash table */
#define POP_CACHE_LEN 10

/* maximal length of the server response (RFC1939) */
#define POP_CMD_RESPONSE 512

/**
 * enum PopStatus - POP server responses
 */
enum PopStatus
{
  POP_NONE = 0,     ///< No connected to server
  POP_CONNECTED,    ///< Connected to server
  POP_DISCONNECTED, ///< Disconnected from server
};

/**
 * enum PopAuthRes - POP authentication responses
 */
enum PopAuthRes
{
  POP_A_SUCCESS = 0, ///< Authenticated successfully
  POP_A_SOCKET,      ///< Connection lost
  POP_A_FAILURE,     ///< Authentication failed
  POP_A_UNAVAIL,     ///< No valid authentication method
};

/**
 * struct PopCache - POP-specific email cache
 */
struct PopCache
{
  unsigned int index;
  char *path;
};

/**
 * struct PopAccountData - POP-specific Account data - @extends Account
 */
struct PopAccountData
{
  struct Connection *conn;
  unsigned int status : 2;
  bool capabilities : 1;
  unsigned int use_stls : 2;
  bool cmd_capa : 1;         ///< optional command CAPA
  bool cmd_stls : 1;         ///< optional command STLS
  unsigned int cmd_user : 2; ///< optional command USER
  unsigned int cmd_uidl : 2; ///< optional command UIDL
  unsigned int cmd_top : 2;  ///< optional command TOP
  bool resp_codes : 1;       ///< server supports extended response codes
  bool expire : 1;           ///< expire is greater than 0
  bool clear_cache : 1;
  size_t size;
  time_t check_time;
  time_t login_delay; ///< minimal login delay  capability
  struct Buffer auth_list; ///< list of auth mechanisms
  char *timestamp;
  struct BodyCache *bcache; ///< body cache
  char err_msg[POP_CMD_RESPONSE];
  struct PopCache cache[POP_CACHE_LEN];
};

/**
 * struct PopEmailData - POP-specific Email data - @extends Email
 */
struct PopEmailData
{
  const char *uid;
  int refno;                   ///< Message number on server
};

/**
 * struct PopAuth - POP authentication multiplexor
 */
struct PopAuth
{
  /**
   * authenticate - Authenticate a POP connection
   * @param adata Pop Account data
   * @param method Use this named method, or any available method if NULL
   * @retval #ImapAuthRes Result, e.g. #IMAP_AUTH_SUCCESS
   */
  enum PopAuthRes (*authenticate)(struct PopAccountData *adata, const char *method);

  const char *method; ///< Name of authentication method supported, NULL means variable.
                      ///< If this is not null, authenticate may ignore the second parameter.
};

/* pop_auth.c */
int pop_authenticate(struct PopAccountData *adata);
void pop_apop_timestamp(struct PopAccountData *adata, char *buf);

/**
 * typedef pop_fetch_t - Callback function to handle POP server responses
 * @param line String to parse
 * @param data Private data passed to pop_fetch_data()
 * @retval  0 Success
 * @retval -1 Failure
 */
typedef int (*pop_fetch_t)(const char *str, void *data);

/* pop_lib.c */
#define pop_query(adata, buf, buflen) pop_query_d(adata, buf, buflen, NULL)
int pop_parse_path(const char *path, struct ConnAccount *acct);
int pop_connect(struct PopAccountData *adata);
int pop_open_connection(struct PopAccountData *adata);
int pop_query_d(struct PopAccountData *adata, char *buf, size_t buflen, char *msg);
int pop_fetch_data(struct PopAccountData *adata, const char *query,
                   struct Progress *progress, pop_fetch_t callback, void *data);
int pop_reconnect(struct Mailbox *m);
void pop_logout(struct Mailbox *m);
struct PopAccountData *pop_adata_get(struct Mailbox *m);
struct PopEmailData *pop_edata_get(struct Email *e);
const char *pop_get_field(enum ConnAccountField field);

#endif /* MUTT_POP_PRIVATE_H */
