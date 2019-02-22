/**
 * @file
 * Routines for querying and external address book
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

#ifndef MUTT_QUERY_H
#define MUTT_QUERY_H

#include <stdio.h>

/* These Config Variables are only used in query.c */
extern char *C_QueryCommand;
extern char *C_QueryFormat;

int  mutt_query_complete(char *buf, size_t buflen);
void mutt_query_menu(char *buf, size_t buflen);

#endif /* MUTT_QUERY_H */
