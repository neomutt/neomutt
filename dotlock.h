/*
 * Copyright (C) 1996-2000 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 1998-2000 Thomas Roessler <roessler@does-not-exist.org>
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

#ifndef _DOTLOCK_H
#define _DOTLOCK_H

/* exit values */

#define DL_EX_OK	0	
#define DL_EX_ERROR	1	
#define DL_EX_EXIST	3	
#define DL_EX_NEED_PRIVS 4
#define DL_EX_IMPOSSIBLE 5

/* flags */

#define DL_FL_TRY	(1 << 0)
#define DL_FL_UNLOCK	(1 << 1)
#define DL_FL_USEPRIV	(1 << 2)
#define DL_FL_FORCE	(1 << 3)
#define DL_FL_RETRY	(1 << 4)
#define DL_FL_UNLINK	(1 << 5)

#define DL_FL_ACTIONS (DL_FL_TRY|DL_FL_UNLOCK|DL_FL_UNLINK)

#ifndef DL_STANDALONE
int dotlock_invoke (const char *, int, int, int);
#endif

#endif
