/**
 * @file
 * Handling for email address groups
 *
 * @authors
 * Copyright (C) 2006 Thomas Roessler <roessler@does-not-exist.org>
 * Copyright (C) 2009 Rocco Rutte <pdmef@gmx.net>
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

#include "config.h"
#include <stdlib.h>
#include "mutt/mutt.h"
#include "group.h"
#include "address.h"
#include "globals.h"
#include "protos.h"

struct Group *mutt_pattern_group(const char *k)
{
  struct Group *p = NULL;

  if (!k)
    return 0;

  p = mutt_hash_find(Groups, k);
  if (!p)
  {
    mutt_debug(2, "Creating group %s.\n", k);
    p = mutt_mem_calloc(1, sizeof(struct Group));
    p->name = mutt_str_strdup(k);
    mutt_hash_insert(Groups, p->name, p);
  }

  return p;
}

static void group_remove(struct Group *g)
{
  if (!g)
    return;
  mutt_hash_delete(Groups, g->name, g);
  mutt_addr_free(&g->as);
  mutt_regexlist_free(&g->rs);
  FREE(&g->name);
  FREE(&g);
}

int mutt_group_context_clear(struct GroupContext **ctx)
{
  struct GroupContext *t = NULL;
  for (; ctx && *ctx; (*ctx) = t)
  {
    group_remove((*ctx)->g);
    t = (*ctx)->next;
    FREE(ctx);
  }
  return 0;
}

static int empty_group(struct Group *g)
{
  if (!g)
    return -1;
  return !g->as && !g->rs;
}

void mutt_group_context_add(struct GroupContext **ctx, struct Group *group)
{
  for (; *ctx; ctx = &((*ctx)->next))
  {
    if ((*ctx)->g == group)
      return;
  }

  *ctx = mutt_mem_calloc(1, sizeof(struct GroupContext));
  (*ctx)->g = group;
}

void mutt_group_context_destroy(struct GroupContext **ctx)
{
  struct GroupContext *p = NULL;
  for (; *ctx; *ctx = p)
  {
    p = (*ctx)->next;
    FREE(ctx);
  }
}

static void group_add_addrlist(struct Group *g, struct Address *a)
{
  struct Address **p = NULL, *q = NULL;

  if (!g)
    return;
  if (!a)
    return;

  for (p = &g->as; *p; p = &((*p)->next))
    ;

  q = mutt_addr_copy_list(a, false);
  q = mutt_remove_xrefs(g->as, q);
  *p = q;
}

static int group_remove_addrlist(struct Group *g, struct Address *a)
{
  struct Address *p = NULL;

  if (!g)
    return -1;
  if (!a)
    return -1;

  for (p = a; p; p = p->next)
    mutt_addr_remove_from_list(&g->as, p->mailbox);

  return 0;
}

static int group_add_regex(struct Group *g, const char *s, int flags, struct Buffer *err)
{
  return mutt_regexlist_add(&g->rs, s, flags, err);
}

static int group_remove_regex(struct Group *g, const char *s)
{
  return mutt_regexlist_remove(&g->rs, s);
}

void mutt_group_context_add_addrlist(struct GroupContext *ctx, struct Address *a)
{
  for (; ctx; ctx = ctx->next)
    group_add_addrlist(ctx->g, a);
}

int mutt_group_context_remove_addrlist(struct GroupContext *ctx, struct Address *a)
{
  int rc = 0;

  for (; (!rc) && ctx; ctx = ctx->next)
  {
    rc = group_remove_addrlist(ctx->g, a);
    if (empty_group(ctx->g))
      group_remove(ctx->g);
  }

  return rc;
}

int mutt_group_context_add_regex(struct GroupContext *ctx, const char *s,
                                 int flags, struct Buffer *err)
{
  int rc = 0;

  for (; (!rc) && ctx; ctx = ctx->next)
    rc = group_add_regex(ctx->g, s, flags, err);

  return rc;
}

int mutt_group_context_remove_regex(struct GroupContext *ctx, const char *s)
{
  int rc = 0;

  for (; (!rc) && ctx; ctx = ctx->next)
  {
    rc = group_remove_regex(ctx->g, s);
    if (empty_group(ctx->g))
      group_remove(ctx->g);
  }

  return rc;
}

bool mutt_group_match(struct Group *g, const char *s)
{
  struct Address *ap = NULL;

  if (s && g)
  {
    if (mutt_regexlist_match(g->rs, s))
      return true;
    for (ap = g->as; ap; ap = ap->next)
      if (ap->mailbox && (mutt_str_strcasecmp(s, ap->mailbox) == 0))
        return true;
  }
  return false;
}
