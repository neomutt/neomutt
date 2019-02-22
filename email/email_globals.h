/**
 * @file
 * Email Global Variables
 *
 * @authors
 * Copyright (C) 2018 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_EMAIL_EMAIL_GLOBALS_H
#define MUTT_EMAIL_EMAIL_GLOBALS_H

#include <stdbool.h>
#include "mutt/mutt.h"

/* Config items */
extern bool                C_MarkOld;
extern struct Regex *      C_ReplyRegex;
extern char *              C_SendCharset;
extern char *              C_SpamSeparator;
extern bool                C_Weed;

/* Global variables */
extern struct ListHead Ignore;      ///< List of header patterns to ignore
extern struct RegexList NoSpamList; ///< List of regexes to whitelist non-spam emails
extern struct ReplaceList SpamList; ///< List of regexes and patterns to match spam emails
extern struct ListHead UnIgnore;    ///< List of header patterns to unignore (see)

#endif /* MUTT_EMAIL_EMAIL_GLOBALS_H */
