/*
 * Copyright (C) 1996-2000 Michael R. Elkins <me@mutt.org>
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

#ifndef _HISTORY_H
#define _HISTORY_H

enum history_class
{
  HC_CMD,
  HC_ALIAS,
  HC_COMMAND,
  HC_FILE,
  HC_PATTERN,
  HC_OTHER,
  HC_MBOX,
  /* insert new items here to keep history file working */
  HC_LAST
};

#define HC_FIRST HC_CMD

typedef enum history_class history_class_t;

void mutt_init_history(void);
void mutt_read_histfile(void);
void mutt_history_add(history_class_t, const char *, int);
char *mutt_history_next(history_class_t);
char *mutt_history_prev(history_class_t);

#endif
