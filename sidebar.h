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

typedef struct _context CONTEXT;
typedef struct buffy_t  BUFFY;

void         sb_change_mailbox (int op);
void         sb_draw (void);
const char * sb_get_highlight (void);
void         sb_init (void);
void         sb_notify_mailbox (BUFFY *b, int created);
void         sb_set_buffystats (const CONTEXT *ctx);
BUFFY *      sb_set_open_buffy (const char *path);
void         sb_set_update_time (void);
int          sb_should_refresh (void);
void         sb_toggle_virtual (void);

#endif /* SIDEBAR_H */
