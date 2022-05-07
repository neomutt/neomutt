/**
 * @file
 * Attachment Content-ID header functions
 *
 * @authors
 * Copyright (C) 2022 David Purton <dcpurton@marshwiggle.net>
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
 * @page attach_cid Attachment Content-ID header functions
 *
 * Attachment Content-ID header functions
 */

#include "config.h"
#include <stddef.h>
#include "cid.h"

/**
 * cid_map_free - Free a CidMap
 * @param[out] ptr CidMap to free
 */
void cid_map_free(struct CidMap **ptr)
{
  if (!ptr)
    return;

  struct CidMap *cid_map = *ptr;

  FREE(&cid_map->cid);
  FREE(&cid_map->fname);

  FREE(ptr);
}

/**
 * cid_map_new - Initialise a new CidMap
 * @param  cid      Content-ID to replace including "cid:" prefix
 * @param  filename Path to file to replace Content-ID with
 * @retval ptr      Newly allocated CidMap
 */
struct CidMap *cid_map_new(const char *cid, const char *filename)
{
  if (!cid || !filename)
    return NULL;

  struct CidMap *cid_map = mutt_mem_calloc(1, sizeof(struct CidMap));

  cid_map->cid = mutt_str_dup(cid);
  cid_map->fname = mutt_str_dup(filename);

  return cid_map;
}

/**
 * cid_map_list_clear - Empty a CidMapList
 * @param cid_map_list List of Content-ID to filename mappings
 */
void cid_map_list_clear(struct CidMapList *cid_map_list)
{
  if (!cid_map_list)
    return;

  while (!STAILQ_EMPTY(cid_map_list))
  {
    struct CidMap *cid_map = STAILQ_FIRST(cid_map_list);
    STAILQ_REMOVE_HEAD(cid_map_list, entries);
    cid_map_free(&cid_map);
  }
}
