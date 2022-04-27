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
#include <stdbool.h>
#include <stdio.h>
#include "email/lib.h"
#include "cid.h"
#include "attach.h"
#include "mailcap.h"
#include "mutt_attach.h"

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

/**
 * cid_save_attachment - Save attachment if it has a Content-ID
 * @param[in]  body         Body to check and save
 * @param[out] cid_map_list List of Content-ID to filename mappings
 *
 * If body has a Content-ID, it is saved to disk and a new Content-ID to filename
 * mapping is added to cid_map_list.
 */
static void cid_save_attachment(struct Body *body, struct CidMapList *cid_map_list)
{
  if (!body || !cid_map_list)
    return;

  char *id = mutt_param_get(&body->parameter, "content-id");
  if (!id)
    return;

  struct Buffer *tmpfile = mutt_buffer_pool_get();
  struct Buffer *cid = mutt_buffer_pool_get();
  bool has_tempfile = false;
  FILE *fp = NULL;

  mutt_debug(LL_DEBUG2, "attachment found with \"Content-ID: %s\"\n", id);
  /* get filename */
  char *fname = mutt_str_dup(body->filename);
  if (body->aptr)
    fp = body->aptr->fp;
  mutt_file_sanitize_filename(fname, fp ? true : false);
  mailcap_expand_filename("%s", fname, tmpfile);
  FREE(&fname);

  /* save attachment */
  if (mutt_save_attachment(fp, body, mutt_buffer_string(tmpfile), 0, NULL) == -1)
    goto bail;
  has_tempfile = true;
  mutt_debug(LL_DEBUG2, "attachment with \"Content-ID: %s\" saved to file \"%s\"\n",
             id, mutt_buffer_string(tmpfile));

  /* add Content-ID to filename mapping to list */
  mutt_buffer_printf(cid, "cid:%s", id);
  struct CidMap *cid_map = cid_map_new(mutt_buffer_string(cid), mutt_buffer_string(tmpfile));
  STAILQ_INSERT_TAIL(cid_map_list, cid_map, entries);

bail:

  if ((fp && !mutt_buffer_is_empty(tmpfile)) || has_tempfile)
    mutt_add_temp_attachment(mutt_buffer_string(tmpfile));
  mutt_buffer_pool_release(&tmpfile);
  mutt_buffer_pool_release(&cid);
}

/**
 * cid_save_attachments - Save all attachments in a "multipart/related" group with a Content-ID
 * @param[in]  body         First body in "multipart/related" group
 * @param[out] cid_map_list List of Content-ID to filename mappings
 */
void cid_save_attachments(struct Body *body, struct CidMapList *cid_map_list)
{
  if (!body || !cid_map_list)
    return;

  for (struct Body *b = body; b; b = b->next)
  {
    if (b->parts)
      cid_save_attachments(b->parts, cid_map_list);
    else
      cid_save_attachment(b, cid_map_list);
  }
}
