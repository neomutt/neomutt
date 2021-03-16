/**
 * @file
 * Pop-specific Account data
 *
 * @authors
 * Copyright (C) 2021 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_POP_ADATA_H
#define MUTT_POP_ADATA_H

#include <time.h>
#include <stdbool.h>
#include "private.h"
#include "mutt/lib.h"

struct Mailbox;

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

void                   pop_adata_free(void **ptr);
struct PopAccountData *pop_adata_get (struct Mailbox *m);
struct PopAccountData *pop_adata_new (void);

#endif /* MUTT_POP_ADATA_H */
