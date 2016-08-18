/* Copyright (C) 2004 Justin Hibbits <jrh29@po.cwru.edu>
 * Copyright (C) 2004 Thomer M. Gil <mutt@thomer.com>
 * Copyright (C) 2015-2016 Richard Russon <rich@flatcap.org>
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
 *     Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 */

#ifndef SIDEBAR_H
#define SIDEBAR_H

#include "mutt.h"
#include "buffy.h"

void         mutt_sb_change_mailbox (int op);
void         mutt_sb_draw (void);
const char * mutt_sb_get_highlight (void);
void         mutt_sb_notify_mailbox (BUFFY *b, int created);
void         mutt_sb_set_buffystats (const CONTEXT *ctx);
BUFFY *      mutt_sb_set_open_buffy (void);

#endif /* SIDEBAR_H */
