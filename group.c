/*
 * Copyright (C) 2006 Thomas Roessler <roessler@does-not-exist.org>
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

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include "mutt.h"
#include "mutt_curses.h"
#include "mutt_regex.h"
#include "mbyte.h"
#include "charset.h"

#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/utsname.h>
#include <errno.h>
#include <sys/wait.h>

group_t *mutt_pattern_group (const char *k)
{
  group_t *p;
  
  if (!k)
    return 0;
  
  if (!(p = hash_find (Groups, k)))
  {
    dprint (2, (debugfile, "mutt_pattern_group: Creating group %s.\n", k));
    p = safe_calloc (1, sizeof (group_t));
    p->name = safe_strdup (k);
    hash_insert (Groups, p->name, p, 0);
  }
  
  return p;
}

void mutt_group_context_add (group_context_t **ctx, group_t *group)
{
  for (; *ctx; ctx = &((*ctx)->next))
  {
    if ((*ctx)->g == group)
      return;
  }
  
  *ctx = safe_calloc (1, sizeof (group_context_t));
  (*ctx)->g = group;
}

void mutt_group_context_destroy (group_context_t **ctx)
{
  group_context_t *p;
  for (; *ctx; *ctx = p)
  {
    p = (*ctx)->next;
    FREE (ctx);		/* __FREE_CHECKED__ */
  }
}

void mutt_group_add_adrlist (group_t *g, ADDRESS *a)
{
  ADDRESS **p, *q;

  if (!g)
    return;
  if (!a)
    return;
  
  for (p = &g->as; *p; p = &((*p)->next))
    ;
  
  q = rfc822_cpy_adr (a, 0);
  q = mutt_remove_xrefs (g->as, q);
  *p = q;
}

static int mutt_group_add_rx (group_t *g, const char *s, int flags, BUFFER *err)
{
  return mutt_add_to_rx_list (&g->rs, s, flags, err);
}

void mutt_group_context_add_adrlist (group_context_t *ctx, ADDRESS *a)
{
  for (; ctx; ctx = ctx->next)
    mutt_group_add_adrlist (ctx->g, a);
}

int mutt_group_context_add_rx (group_context_t *ctx, const char *s, int flags, BUFFER *err)
{
  int rv = 0;
  
  for (; (!rv) && ctx; ctx = ctx->next)
    rv = mutt_group_add_rx (ctx->g, s, flags, err);

  return rv;
}

int mutt_group_match (group_t *g, const char *s)
{
  ADDRESS *ap;
  
  if (s && g)
  {
    if (mutt_match_rx_list (s, g->rs))
      return 1;
    for (ap = g->as; ap; ap = ap->next)
      if (ap->mailbox && !mutt_strcasecmp (s, ap->mailbox))
	return 1;
  }
  return 0;
}

