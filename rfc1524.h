/**
 * @file
 * RFC1524 Mailcap routines
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

#ifndef _MUTT_RFC1524_H
#define _MUTT_RFC1524_H

#include <stdbool.h>
#include <stddef.h>

struct Body;

/**
 * struct Rfc1524MailcapEntry - A mailcap entry
 */
struct Rfc1524MailcapEntry
{
  char *command;
  char *testcommand;
  char *composecommand;
  char *composetypecommand;
  char *editcommand;
  char *printcommand;
  char *nametemplate;
  char *convert;
  bool needsterminal : 1; /**< endwin() and system */
  bool copiousoutput : 1; /**< needs pager, basically */
};

struct Rfc1524MailcapEntry *rfc1524_new_entry(void);
void rfc1524_free_entry(struct Rfc1524MailcapEntry **entry);
int rfc1524_expand_command(struct Body *a, char *filename, char *_type, char *command, int clen);
int rfc1524_expand_filename(char *nametemplate, char *oldfile, char *newfile, size_t nflen);
int rfc1524_mailcap_lookup(struct Body *a, char *type, struct Rfc1524MailcapEntry *entry, int opt);
int mutt_rename_file(char *oldfile, char *newfile);

#endif /* _MUTT_RFC1524_H */
