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

#ifndef _BROWSER_H
#define _BROWSER_H 1

struct folder_file
{
  mode_t mode;
  off_t size;
  time_t mtime;
  struct stat *st;

  char *name;
  char *desc;

  unsigned short new;
#ifdef USE_IMAP
  char delim;
  
  unsigned imap : 1;
  unsigned selectable : 1;
  unsigned inferiors : 1;
#endif
  unsigned tagged : 1;
};

struct browser_state
{
  struct folder_file *entry;
  unsigned int entrylen;	/* number of real entries */
  unsigned int entrymax;	/* max entry */
#ifdef USE_IMAP
  short imap_browse;
  char *folder;
  unsigned noselect : 1;
  unsigned marked : 1;
  unsigned unmarked : 1;
#endif
};

struct mailbox_state
{
  unsigned int new;
  unsigned int old;
  unsigned int messages;
};
#endif /* _BROWSER_H */
