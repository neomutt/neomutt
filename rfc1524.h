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

#ifndef _RFC1524_H
#define _RFC1524_H

typedef struct rfc1524_mailcap_entry {
/*  char *contenttype; */ /* we don't need this, as we search for it */
  char *command;
  char *testcommand;
  char *composecommand;
  char *composetypecommand;
  char *editcommand;
  char *printcommand;
  char *nametemplate;
  char *convert;
/*  char *description; */ /* we don't need this */
  unsigned int needsterminal : 1;  /* endwin() and system */
  unsigned int copiousoutput : 1;  /* needs pager, basically */
} rfc1524_entry;

rfc1524_entry *rfc1524_new_entry (void);
void rfc1524_free_entry (rfc1524_entry **);
int rfc1524_expand_command (BODY *, char *, char *, char *, int);
int rfc1524_expand_filename (char *, char *, char *, size_t);
int rfc1524_mailcap_lookup (BODY *, char *, rfc1524_entry *, int);
int mutt_rename_file (char *, char *);

#endif /* _RFC1524_H */
