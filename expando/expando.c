/**
 * @file
 * Parsed Expando
 *
 * @authors
 * Copyright (C) 2023-2024 Tóth János <gomba007@gmail.com>
 * Copyright (C) 2023-2024 Richard Russon <rich@flatcap.org>
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
 * @page expando_expando Parsed Expando
 *
 * This represents a fully-parsed Expando Format String.
 */

#include "config.h"
#include <stddef.h>
#include "mutt/lib.h"
#include "expando.h"
#include "node.h"
#include "parse.h"
#include "render.h"

/**
 * expando_new - Create an Expando from a string
 * @param format Format string to parse
 * @retval ptr New Expando object
 */
struct Expando *expando_new(const char *format)
{
  struct Expando *exp = MUTT_MEM_CALLOC(1, struct Expando);
  exp->string = mutt_str_dup(format);
  return exp;
}

/**
 * expando_free - Free an Expando object
 * @param[out] ptr Expando to free
 */
void expando_free(struct Expando **ptr)
{
  if (!ptr || !*ptr)
    return;

  struct Expando *exp = *ptr;

  node_tree_free(&exp->node);
  FREE(&exp->string);

  FREE(ptr);
}

/**
 * expando_parse - Parse an Expando string
 * @param str  String to parse
 * @param defs Data defining Expando
 * @param err  Buffer for error messages
 * @retval ptr New Expando
 */
struct Expando *expando_parse(const char *str, const struct ExpandoDefinition *defs,
                              struct Buffer *err)
{
  if (!str || !defs)
    return NULL;

  struct Expando *exp = expando_new(str);

  struct ExpandoParseError error = { 0 };
  struct ExpandoNode *root = NULL;

  node_tree_parse(&root, exp->string, defs, &error);

  if (error.position)
  {
    buf_strcpy(err, error.message);
    node_tree_free(&root);
    expando_free(&exp);
    return NULL;
  }

  exp->node = root;
  return exp;
}

/**
 * expando_render - Render an Expando + data into a string
 * @param[in]  exp      Expando containing the expando tree
 * @param[in]  rdata    Expando render data
 * @param[in]  data     Callback data
 * @param[in]  flags    Callback flags
 * @param[in]  max_cols Number of screen columns (-1 means unlimited)
 * @param[out] buf      Buffer in which to save string
 * @retval obj Number of bytes written to buf and screen columns used
 */
int expando_render(const struct Expando *exp, const struct ExpandoRenderData *rdata,
                   void *data, MuttFormatFlags flags, int max_cols, struct Buffer *buf)
{
  if (!exp || !exp->node || !rdata)
    return 0;

  // Give enough space for a long command line
  if (max_cols == -1)
    max_cols = 8192;

  return node_render(exp->node, rdata, buf, max_cols, data, flags);
}

/**
 * expando_equal - Compare two expandos
 * @param a First  Expando
 * @param b Second Expando
 * @retval true They are identical
 */
bool expando_equal(const struct Expando *a, const struct Expando *b)
{
  if (!a && !b) /* both empty */
    return true;
  if (!a ^ !b) /* one is empty, but not the other */
    return false;

  return mutt_str_equal(a->string, b->string);
}
