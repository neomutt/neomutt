/**
 * @file
 * Singly-linked list type
 *
 * @authors
 * Copyright (C) 2017 Richard Russon <rich@flatcap.org>
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

#ifndef _MUTT_LIST_H
#define _MUTT_LIST_H

#include <string.h>
#include "lib/lib.h"

/**
 * struct List - Singly-linked List type
 */
struct List
{
  char *data;
  struct List *next;
};

static inline struct List *mutt_new_list(void)
{
  return safe_calloc(1, sizeof(struct List));
}

/**
 * New implementation using macros from queue.h
 */

#include "queue.h"

STAILQ_HEAD(STailQHead, STailQNode);
struct STailQNode
{
    char *data;
    STAILQ_ENTRY(STailQNode) entries;
};

static inline struct STailQNode* mutt_stailq_insert_head(struct STailQHead *h, char *s)
{
  struct STailQNode *np = safe_calloc(1, sizeof(struct STailQNode));
  np->data = s;
  STAILQ_INSERT_HEAD(h, np, entries);
  return np;
}

static inline struct STailQNode* mutt_stailq_insert_tail(struct STailQHead *h, char * s)
{
  struct STailQNode *np = safe_calloc(1, sizeof(struct STailQNode));
  np->data = s;
  STAILQ_INSERT_TAIL(h, np, entries);
  return np;
}

static inline struct STailQNode *mutt_stailq_find(struct STailQHead *h, const char *data)
{
  struct STailQNode *np = NULL;
  STAILQ_FOREACH(np, h, entries)
  {
    if (np->data == data || mutt_strcmp(np->data, data) == 0)
    {
      return np;
    }
  }
  return NULL;
}

static inline void mutt_stailq_free(struct STailQHead *h)
{
  struct STailQNode *np = STAILQ_FIRST(h), *next = NULL;
  while (np)
  {
      next = STAILQ_NEXT(np, entries);
      FREE(&np->data);
      FREE(&np);
      np = next;
  }
  STAILQ_INIT(h);
}

static inline bool mutt_stailq_match(const char *s, struct STailQHead *h)
{
  struct STailQNode *np;
  STAILQ_FOREACH(np, h, entries)
  {
    if (*np->data == '*' || mutt_strncasecmp(s, np->data, strlen(np->data)) == 0)
      return true;
  }
  return false;
}

#endif /* _MUTT_LIST_H */
