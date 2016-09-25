/**
 * @file
 * Information commands
 *
 * @authors
 * Copyright (C) 2016 Christopher John Czettel <chris@meicloud.at>
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

#ifndef MUTT_ICOMMANDS_H
#define MUTT_ICOMMANDS_H

static const char *ICOMMAND_NOT_FOUND = "ICOMMAND_NOT_FOUND";

#include <ctype.h>
#include <stdio.h>
#include "mutt.h"

struct Buffer;

struct ICommand
{
  char *name;
  int (*func)(struct Buffer *, struct Buffer *, unsigned long, struct Buffer *);
  unsigned long data;
};

int neomutt_parse_icommand(/* const */ char *line, struct Buffer *err);

#endif /* MUTT_ICOMMANDS_H */
