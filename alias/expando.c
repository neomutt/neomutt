/**
 * @file
 * Alias Expando definitions
 *
 * @authors
 * Copyright (C) 2024 Richard Russon <rich@flatcap.org>
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
 * @page alias_expando Alias Expando definitions
 *
 * Alias Expando definitions
 */

#include <stdio.h>
#include "expando.h"
#include "lib.h"
#include "expando/lib.h"
#include "alias.h"
#include "gui.h"

/**
 * alias_a - Alias: Alias name - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
static void alias_a(const struct ExpandoNode *node, void *data,
                    MuttFormatFlags flags, struct Buffer *buf)
{
  const struct AliasView *av = data;
  const struct Alias *alias = av->alias;

  const char *s = alias->name;
  buf_strcpy(buf, s);
}

/**
 * alias_c - Alias: Comment - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
static void alias_c(const struct ExpandoNode *node, void *data,
                    MuttFormatFlags flags, struct Buffer *buf)
{
  const struct AliasView *av = data;
  const struct Alias *alias = av->alias;

  const char *s = alias->comment;
  buf_strcpy(buf, s);
}

/**
 * alias_e - Alias: Email Address - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
static void alias_e(const struct ExpandoNode *node, void *data,
                    MuttFormatFlags flags, struct Buffer *buf)
{
  const struct AliasView *av = data;
  const struct Alias *alias = av->alias;

  struct Address *a = NULL;
  TAILQ_FOREACH(a, &alias->addr, entries)
  {
    struct Address *next = TAILQ_NEXT(a, entries);

    buf_add_printf(buf, "<%s>", buf_string(a->mailbox));
    if (next)
      buf_addstr(buf, ", ");
  }
}

/**
 * alias_f_num - Alias: Flags - Implements ExpandoRenderData::get_number() - @ingroup expando_get_number_api
 */
static long alias_f_num(const struct ExpandoNode *node, void *data, MuttFormatFlags flags)
{
  const struct AliasView *av = data;
  return av->is_deleted;
}

/**
 * alias_f - Alias: Flags - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
static void alias_f(const struct ExpandoNode *node, void *data,
                    MuttFormatFlags flags, struct Buffer *buf)
{
  const struct AliasView *av = data;

  // NOTE(g0mb4): use $flag_chars?
  const char *s = av->is_deleted ? "D" : " ";
  buf_strcpy(buf, s);
}

/**
 * alias_n_num - Alias: Index number - Implements ExpandoRenderData::get_number() - @ingroup expando_get_number_api
 */
static long alias_n_num(const struct ExpandoNode *node, void *data, MuttFormatFlags flags)
{
  const struct AliasView *av = data;

  return av->num + 1;
}

/**
 * alias_N - Alias: Personal Name - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
static void alias_N(const struct ExpandoNode *node, void *data,
                    MuttFormatFlags flags, struct Buffer *buf)
{
  const struct AliasView *av = data;
  const struct Alias *alias = av->alias;

  struct Address *a = NULL;
  TAILQ_FOREACH(a, &alias->addr, entries)
  {
    struct Address *next = TAILQ_NEXT(a, entries);

    buf_addstr(buf, buf_string(a->personal));
    if (next)
      buf_addstr(buf, ", ");
  }
}

/**
 * alias_r - Alias: Address - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
static void alias_r(const struct ExpandoNode *node, void *data,
                    MuttFormatFlags flags, struct Buffer *buf)
{
  const struct AliasView *av = data;
  const struct Alias *alias = av->alias;

  mutt_addrlist_write(&alias->addr, buf, true);
}

/**
 * alias_t_num - Alias: Tagged char - Implements ExpandoRenderData::get_number() - @ingroup expando_get_number_api
 */
static long alias_t_num(const struct ExpandoNode *node, void *data, MuttFormatFlags flags)
{
  const struct AliasView *av = data;
  return av->is_tagged;
}

/**
 * alias_t - Alias: Tagged char - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
static void alias_t(const struct ExpandoNode *node, void *data,
                    MuttFormatFlags flags, struct Buffer *buf)
{
  const struct AliasView *av = data;

  // NOTE(g0mb4): use $flag_chars?
  const char *s = av->is_tagged ? "*" : " ";
  buf_strcpy(buf, s);
}

/**
 * alias_Y - Alias: Tags - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
static void alias_Y(const struct ExpandoNode *node, void *data,
                    MuttFormatFlags flags, struct Buffer *buf)
{
  const struct AliasView *av = data;

  alias_tags_to_buffer(&av->alias->tags, buf);
}

/**
 * AliasRenderData - Callbacks for Alias Expandos
 *
 * @sa AliasFormatDef, ExpandoDataAlias, ExpandoDataGlobal
 */
const struct ExpandoRenderData AliasRenderData[] = {
  // clang-format off
  { ED_ALIAS,  ED_ALI_ADDRESS, alias_r,     NULL },
  { ED_ALIAS,  ED_ALI_ALIAS,   alias_a,     NULL },
  { ED_ALIAS,  ED_ALI_COMMENT, alias_c,     NULL },
  { ED_ALIAS,  ED_ALI_EMAIL,   alias_e,     NULL },
  { ED_ALIAS,  ED_ALI_FLAGS,   alias_f,     alias_f_num },
  { ED_ALIAS,  ED_ALI_NAME,    alias_N,     NULL },
  { ED_ALIAS,  ED_ALI_NUMBER,  NULL,        alias_n_num },
  { ED_ALIAS,  ED_ALI_TAGGED,  alias_t,     alias_t_num },
  { ED_ALIAS,  ED_ALI_TAGS,    alias_Y,     NULL },
  { -1, -1, NULL, NULL },
  // clang-format on
};

/**
 * QueryRenderData - Callbacks for Query Expandos
 *
 * @sa QueryFormatDef, ExpandoDataAlias, ExpandoDataGlobal
 */
const struct ExpandoRenderData QueryRenderData[] = {
  // clang-format off
  { ED_ALIAS,  ED_ALI_ADDRESS, alias_r,     NULL },
  { ED_ALIAS,  ED_ALI_COMMENT, alias_c,     NULL },
  { ED_ALIAS,  ED_ALI_EMAIL,   alias_e,     NULL },
  { ED_ALIAS,  ED_ALI_NAME,    alias_N,     NULL },
  { ED_ALIAS,  ED_ALI_NUMBER,  NULL,        alias_n_num },
  { ED_ALIAS,  ED_ALI_TAGGED,  alias_t,     alias_t_num },
  { ED_ALIAS,  ED_ALI_TAGS,    alias_Y,     NULL },
  { -1, -1, NULL, NULL },
  // clang-format on
};
