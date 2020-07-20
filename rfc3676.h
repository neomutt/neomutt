/**
 * @file
 * RFC3676 Format Flowed routines
 *
 * @authors
 * Copyright (C) 2005 Andreas Krennmair <ak@synflood.at>
 * Copyright (C) 2005 Peter J. Holzer <hjp@hjp.net>
 * Copyright (C) 2005,2007 Rocco Rutte <pdmef@gmx.net>
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

#ifndef MUTT_RFC3676_H
#define MUTT_RFC3676_H

#include <stdbool.h>

struct Body;
struct Email;
struct State;

/* These Config Variables are only used in rfc3676.c */
extern bool  C_ReflowSpaceQuotes;
extern short C_ReflowWrap;

int rfc3676_handler(struct Body *a, struct State *s);
bool mutt_rfc3676_is_format_flowed(struct Body *b);
void mutt_rfc3676_space_stuff (struct Email *e);
void mutt_rfc3676_space_unstuff (struct Email *e);
void mutt_rfc3676_space_unstuff_attachment(struct Body *b, const char *filename);
void mutt_rfc3676_space_stuff_attachment(struct Body *b, const char *filename);

#endif /* MUTT_RFC3676_H */
