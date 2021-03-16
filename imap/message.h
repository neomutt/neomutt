/**
 * @file
 * Manage IMAP messages
 *
 * @authors
 * Copyright (C) 1996-1999 Brandon Long <blong@fiction.net>
 * Copyright (C) 1999-2000,2005 Brendan Cully <brendan@kublai.com>
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

#ifndef MUTT_IMAP_MESSAGE_H
#define MUTT_IMAP_MESSAGE_H

#include <time.h>

/**
 * struct ImapHeader - IMAP-specific header
 */
struct ImapHeader
{
  struct ImapEmailData *edata;

  time_t received;
  long content_length;
};

#endif /* MUTT_IMAP_MESSAGE_H */
