/* 
 * Copyright (C) 1996-2000 Michael R. Elkins <me@cs.hmc.edu>
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

typedef struct buffy_t
{
  char *path;
#ifdef BUFFY_SIZE
  long size;
#endif				/* BUFFY_SIZE */
  struct buffy_t *next;
  short new;			/* mailbox has new mail */
  short notified;		/* user has been notified */
  short magic;			/* mailbox type */
  short newly_created;		/* mbox or mmdf just popped into existence */
}
BUFFY;

WHERE BUFFY *Incoming INITVAL (0);
WHERE short BuffyTimeout INITVAL (3);

extern time_t BuffyDoneTime;	/* last time we knew for sure how much mail there was */

#ifdef BUFFY_SIZE
BUFFY *mutt_find_mailbox (const char *path);
void mutt_update_mailbox (BUFFY * b);
#endif
