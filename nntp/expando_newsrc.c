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
 * nntp_a - Newsrc: Account url - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
static void nntp_a(const struct ExpandoNode *node, void *data,
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
 * nntp_p_num - Newsrc: Port - Implements ExpandoRenderData::get_number() - @ingroup expando_get_number_api
 */
static long nntp_p_num(const struct ExpandoNode *node, void *data, MuttFormatFlags flags)
{
  const struct NntpAccountData *adata = data;
  const struct ConnAccount *cac = &adata->conn->account;

  return cac->port;
}

/**
 * nntp_P_num - Newsrc: Port if specified - Implements ExpandoRenderData::get_number() - @ingroup expando_get_number_api
 */
static long nntp_P_num(const struct ExpandoNode *node, void *data, MuttFormatFlags flags)
{
  const struct NntpAccountData *adata = data;
  const struct ConnAccount *cac = &adata->conn->account;

  if (cac->flags & MUTT_ACCT_PORT)
    return cac->port;

  return 0;
}

/**
 * nntp_P - Newsrc: Port if specified - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
static void nntp_P(const struct ExpandoNode *node, void *data,
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
 * nntp_s - Newsrc: News server name - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
static void nntp_s(const struct ExpandoNode *node, void *data,
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
 * nntp_S - Newsrc: Url schema - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
static void nntp_S(const struct ExpandoNode *node, void *data,
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
 * nntp_u - Newsrc: Username - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
static void nntp_u(const struct ExpandoNode *node, void *data,
                   MuttFormatFlags flags, struct Buffer *buf)
{
  const struct NntpAccountData *adata = data;
  const struct ConnAccount *cac = &adata->conn->account;

  const char *s = cac->user;
  buf_strcpy(buf, s);
}

/**
 * NntpRenderData - Callbacks for Newsrc Expandos
 *
 * @sa NntpFormatDef, ExpandoDataNntp
 */
const struct ExpandoRenderData NntpRenderData[] = {
  // clang-format off
  { ED_NNTP, ED_NTP_ACCOUNT,  nntp_a, NULL },
  { ED_NNTP, ED_NTP_PORT,     NULL,   nntp_p_num },
  { ED_NNTP, ED_NTP_PORT_IF,  nntp_P, nntp_P_num },
  { ED_NNTP, ED_NTP_SCHEMA,   nntp_S, NULL },
  { ED_NNTP, ED_NTP_SERVER,   nntp_s, NULL },
  { ED_NNTP, ED_NTP_USERNAME, nntp_u, NULL },
  { -1, -1, NULL, NULL },
  // clang-format on
};
