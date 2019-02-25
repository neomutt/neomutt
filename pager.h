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

#ifndef MUTT_PAGER_H
#define MUTT_PAGER_H

#include <stdbool.h>
#include <stdio.h>

/* These Config Variables are only used in pager.c */
extern bool          AllowAnsi;
extern bool          HeaderColorPartial;
extern short         PagerContext;
extern short         PagerIndexLines;
extern bool          PagerStop;
extern short         SearchContext;
extern short         SkipQuotedOffset;
extern bool          SmartWrap;
extern struct Regex *Smileys;
extern bool          Tilde;

/* dynamic internal flags */
#define MUTT_SHOWFLAT         (1 << 0)    ///< Show characters (used for displaying help)
#define MUTT_SHOWCOLOR        (1 << 1)    ///< Show characters in color otherwise don't show characters
#define MUTT_HIDE             (1 << 2)    ///< Don't show quoted text
#define MUTT_SEARCH           (1 << 3)    ///< Resolve search patterns
#define MUTT_TYPES            (1 << 4)    ///< Compute line's type
#define MUTT_SHOW             (MUTT_SHOWCOLOR | MUTT_SHOWFLAT)

/* exported flags for mutt_(do_)?pager */
#define MUTT_PAGER_NSKIP      (1 << 5)    ///< Preserve whitespace with smartwrap
#define MUTT_PAGER_MARKER     (1 << 6)    ///< Use markers if option is set
#define MUTT_PAGER_RETWINCH   (1 << 7)    ///< Need reformatting on SIGWINCH
#define MUTT_PAGER_ATTACHMENT (1 << 8)    ///< Attachments may exist
#define MUTT_PAGER_NOWRAP     (1 << 9)    ///< Format for term width, ignore $wrap
#define MUTT_PAGER_LOGS       (1 << 10)   ///< Logview mode
#define MUTT_PAGER_MESSAGE    (MUTT_SHOWCOLOR | MUTT_PAGER_MARKER)

#define MUTT_DISPLAYFLAGS (MUTT_SHOW | MUTT_PAGER_NSKIP | MUTT_PAGER_MARKER | MUTT_PAGER_LOGS)

/**
 * struct Pager - An email being displayed
 */
struct Pager
{
  struct Context *ctx;    /**< current mailbox */
  struct Email *email;    /**< current message */
  struct Body *body;      /**< current attachment */
  FILE *fp;               /**< source stream */
  struct AttachCtx *actx; /**< attachment information */
};

int mutt_pager(const char *banner, const char *fname, int flags, struct Pager *extra);

void mutt_clear_pager_position(void);

#endif /* MUTT_PAGER_H */
