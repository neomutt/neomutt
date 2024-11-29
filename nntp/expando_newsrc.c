/**
 * @file
 * Browse NNTP groups
 *
 * @authors
 * Copyright (C) 2018-2024 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2020 Pietro Cerutti <gahr@gahr.ch>
 * Copyright (C) 2023-2024 Tóth János <gomba007@gmail.com>
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

/**
 * @page nntp_expando_newsrc Browse NNTP groups
 *
 * Browse NNTP groups
 */

#include "config.h"
#include <stdio.h>
#include <string.h>
#include "mutt/lib.h"
#include "email/lib.h"
#include "conn/lib.h"
#include "lib.h"
#include "expando/lib.h"
#include "adata.h"

/**
 * nntp_account - Newsrc: Account url - Implements ::get_string_t - @ingroup expando_get_string_api
 */
static void nntp_account(const struct ExpandoNode *node, void *data,
                         MuttFormatFlags flags, struct Buffer *buf)
{
  struct NntpAccountData *adata = data;
  struct ConnAccount *cac = &adata->conn->account;

  char tmp[128] = { 0 };

  struct Url url = { 0 };
  account_to_url(cac, &url);
  url_tostring(&url, tmp, sizeof(tmp), U_PATH);
  char *p = strchr(tmp, '/');
  if (p)
  {
    *p = '\0';
  }

  buf_strcpy(buf, tmp);
}

/**
 * nntp_port - Newsrc: Port - Implements ::get_number_t - @ingroup expando_get_number_api
 */
static long nntp_port(const struct ExpandoNode *node, void *data, MuttFormatFlags flags)
{
  const struct NntpAccountData *adata = data;
  const struct ConnAccount *cac = &adata->conn->account;

  return cac->port;
}

/**
 * nntp_port_if_num - Newsrc: Port if specified - Implements ::get_number_t - @ingroup expando_get_number_api
 */
static long nntp_port_if_num(const struct ExpandoNode *node, void *data, MuttFormatFlags flags)
{
  const struct NntpAccountData *adata = data;
  const struct ConnAccount *cac = &adata->conn->account;

  if (cac->flags & MUTT_ACCT_PORT)
    return cac->port;

  return 0;
}

/**
 * nntp_port_if - Newsrc: Port if specified - Implements ::get_string_t - @ingroup expando_get_string_api
 */
static void nntp_port_if(const struct ExpandoNode *node, void *data,
                         MuttFormatFlags flags, struct Buffer *buf)
{
  const struct NntpAccountData *adata = data;
  const struct ConnAccount *cac = &adata->conn->account;

  if (cac->flags & MUTT_ACCT_PORT)
  {
    buf_add_printf(buf, "%hd", cac->port);
  }
}

/**
 * nntp_server - Newsrc: News server name - Implements ::get_string_t - @ingroup expando_get_string_api
 */
static void nntp_server(const struct ExpandoNode *node, void *data,
                        MuttFormatFlags flags, struct Buffer *buf)
{
  const struct NntpAccountData *adata = data;
  const struct ConnAccount *cac = &adata->conn->account;

  char tmp[128] = { 0 };

  mutt_str_copy(tmp, cac->host, sizeof(tmp));
  mutt_str_lower(tmp);

  buf_strcpy(buf, tmp);
}

/**
 * nntp_schema - Newsrc: Url schema - Implements ::get_string_t - @ingroup expando_get_string_api
 */
static void nntp_schema(const struct ExpandoNode *node, void *data,
                        MuttFormatFlags flags, struct Buffer *buf)
{
  struct NntpAccountData *adata = data;
  struct ConnAccount *cac = &adata->conn->account;

  char tmp[128] = { 0 };

  struct Url url = { 0 };
  account_to_url(cac, &url);
  url_tostring(&url, tmp, sizeof(tmp), U_PATH);
  char *p = strchr(tmp, ':');
  if (p)
  {
    *p = '\0';
  }

  buf_strcpy(buf, tmp);
}

/**
 * nntp_username - Newsrc: Username - Implements ::get_string_t - @ingroup expando_get_string_api
 */
static void nntp_username(const struct ExpandoNode *node, void *data,
                          MuttFormatFlags flags, struct Buffer *buf)
{
  const struct NntpAccountData *adata = data;
  const struct ConnAccount *cac = &adata->conn->account;

  const char *s = cac->user;
  buf_strcpy(buf, s);
}

/**
 * NntpRenderCallbacks - Callbacks for Newsrc Expandos
 *
 * @sa NntpFormatDef, ExpandoDataNntp
 */
const struct ExpandoRenderCallback NntpRenderCallbacks[] = {
  // clang-format off
  { ED_NNTP, ED_NTP_ACCOUNT,  nntp_account,  NULL },
  { ED_NNTP, ED_NTP_PORT,     NULL,          nntp_port },
  { ED_NNTP, ED_NTP_PORT_IF,  nntp_port_if,  nntp_port_if_num },
  { ED_NNTP, ED_NTP_SCHEMA,   nntp_schema,   NULL },
  { ED_NNTP, ED_NTP_SERVER,   nntp_server,   NULL },
  { ED_NNTP, ED_NTP_USERNAME, nntp_username, NULL },
  { -1, -1, NULL, NULL },
  // clang-format on
};
