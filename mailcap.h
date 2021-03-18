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

#ifndef MUTT_MAILCAP_H
#define MUTT_MAILCAP_H

#include <stddef.h>
#include <stdbool.h>

struct Body;
struct Buffer;

/**
 * struct MailcapEntry - A mailcap entry
 */
struct MailcapEntry
{
  char *command;
  char *testcommand;
  char *composecommand;
  char *composetypecommand;
  char *editcommand;
  char *printcommand;
  char *nametemplate;
  char *convert;
  bool needsterminal : 1; ///< endwin() and system
  bool copiousoutput : 1; ///< needs pager, basically
  bool xneomuttkeep  : 1; ///< do not remove the file on command exit
  bool xneomuttnowrap: 1; ///< do not wrap the output in the pager
};

/**
 * enum MailcapLookup - Mailcap actions
 */
enum MailcapLookup
{
  MUTT_MC_NO_FLAGS = 0, ///< No flags set
  MUTT_MC_EDIT,         ///< Mailcap edit field
  MUTT_MC_COMPOSE,      ///< Mailcap compose field
  MUTT_MC_PRINT,        ///< Mailcap print field
  MUTT_MC_AUTOVIEW,     ///< Mailcap autoview field
};

void                 mailcap_entry_free(struct MailcapEntry **ptr);
struct MailcapEntry *mailcap_entry_new(void);
int                  mailcap_expand_command(struct Body *a, const char *filename, const char *type, struct Buffer *command);
void                 mailcap_expand_filename(const char *nametemplate, const char *oldfile, struct Buffer *newfile);
bool                 mailcap_lookup(struct Body *a, char *type, size_t typelen, struct MailcapEntry *entry, enum MailcapLookup opt);

#endif /* MUTT_MAILCAP_H */
