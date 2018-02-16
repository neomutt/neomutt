/**
 * @file
 * GUI display a file/email/help in a viewport with paging
 *
 * @authors
 * Copyright (C) 1996-2000 Michael R. Elkins <me@mutt.org>
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

#ifndef _MUTT_PAGER_H
#define _MUTT_PAGER_H

#include <stdio.h>

struct Context;
struct Menu;

/* dynamic internal flags */
#define MUTT_SHOWFLAT  (1 << 0)
#define MUTT_SHOWCOLOR (1 << 1)
#define MUTT_HIDE      (1 << 2)
#define MUTT_SEARCH    (1 << 3)
#define MUTT_TYPES     (1 << 4)
#define MUTT_SHOW      (MUTT_SHOWCOLOR | MUTT_SHOWFLAT)

/* exported flags for mutt_(do_)?pager */
#define MUTT_PAGER_NSKIP      (1 << 5)    /**< preserve whitespace with smartwrap */
#define MUTT_PAGER_MARKER     (1 << 6)    /**< use markers if option is set */
#define MUTT_PAGER_RETWINCH   (1 << 7)    /**< need reformatting on SIGWINCH */
#define MUTT_PAGER_MESSAGE    (MUTT_SHOWCOLOR | MUTT_PAGER_MARKER)
#define MUTT_PAGER_ATTACHMENT (1 << 8)
#define MUTT_PAGER_NOWRAP     (1 << 9)    /**< format for term width, ignore $wrap */
#define MUTT_PAGER_LOGS       (1 << 10)   /**< Logview mode */

#define MUTT_DISPLAYFLAGS (MUTT_SHOW | MUTT_PAGER_NSKIP | MUTT_PAGER_MARKER | MUTT_PAGER_LOGS)

/**
 * struct Pager - An email being displayed
 */
struct Pager
{
  struct Context *ctx;    /**< current mailbox */
  struct Header *hdr;     /**< current message */
  struct Body *bdy;       /**< current attachment */
  FILE *fp;               /**< source stream */
  struct AttachCtx *actx; /**< attachment information */
};

int mutt_do_pager(const char *banner, const char *tempfile, int do_color, struct Pager *info);
int mutt_pager(const char *banner, const char *fname, int flags, struct Pager *extra);
void update_index(struct Menu *menu, struct Context *ctx, int check, int oldcount, int index_hint);

#endif /* _MUTT_PAGER_H */
