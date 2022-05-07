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

#ifndef MUTT_ATTACH_CID_H
#define MUTT_ATTACH_CID_H

#include "config.h"
#include "mutt/lib.h"

/**
 * struct CidMap - List of Content-ID to filename mappings
 */
struct CidMap
{
  char *cid;                    ///< Content-ID
  char *fname;                  ///< Filename
  STAILQ_ENTRY(CidMap) entries; ///< Linked list
};
STAILQ_HEAD(CidMapList, CidMap);

void           cid_map_free        (struct CidMap **ptr);
struct CidMap *cid_map_new         (const char *cid, const char *filename);
void           cid_map_list_clear  (struct CidMapList *cid_map_list);

#endif /* MUTT_ATTACH_CID_H */
