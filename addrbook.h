/**
 * @file
 * Address book handling aliases
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

#ifndef MUTT_ADDRBOOK_H
#define MUTT_ADDRBOOK_H

#include <stdio.h>

struct AliasList;

/* These Config Variables are only used in addrbook.c */
extern char *C_AliasFormat;
extern short C_SortAlias;

void mutt_alias_menu(char *buf, size_t buflen, struct AliasList *aliases);

#endif /* MUTT_ADDRBOOK_H */
