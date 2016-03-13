/*
 * Copyright (C) 1997 Alain Penders <Alain@Finale-Dev.com>
 * Copyright (C) 2016 Richard Russon <rich@flatcap.org>
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
 *     Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef _COMPRESS_H_
#define _COMPRESS_H_

int  comp_can_append    (const char *);
int  comp_can_read      (const char *);
int  comp_check_mailbox (CONTEXT *);
void comp_fast_close    (CONTEXT *);
int  comp_open_append   (CONTEXT *);
int  comp_open_read     (CONTEXT *);
int  comp_slow_close    (CONTEXT *);
int  comp_sync          (CONTEXT *);
int  comp_valid_command (const char *);

#endif /* _COMPRESS_H_ */
