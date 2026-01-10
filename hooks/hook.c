/**
 * @file
 * User-defined Hooks
 *
 * @authors
 * Copyright (C) 1996-2007 Michael R. Elkins <me@mutt.org>, and others
 * Copyright (C) 2016 Thomas Adam <thomas@xteddy.org>
 * Copyright (C) 2016-2025 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2017-2021 Pietro Cerutti <gahr@gahr.ch>
 * Copyright (C) 2019 Federico Kircheis <federico.kircheis@gmail.com>
 * Copyright (C) 2019 Naveen Nathan <naveen@lastninja.net>
 * Copyright (C) 2022 Oliver Bandel <oliver@first.in-berlin.de>
 * Copyright (C) 2023 Dennis Schön <mail@dennis-schoen.de>
 * Copyright (C) 2023-2024 Tóth János <gomba007@gmail.com>
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

/**
 * @page hooks_hook User-defined Hooks
 *
 * User-defined Hooks
 */

#include "config.h"
#include "mutt/lib.h"
#include "hook.h"
#include "expando/lib.h"
#include "pattern/lib.h"

/**
 * hook_free - Free a Hook
 * @param ptr Hook to free
 */
void hook_free(struct Hook **ptr)
{
  if (!ptr || !*ptr)
    return;

  struct Hook *h = *ptr;

  FREE(&h->command);
  FREE(&h->source_file);
  FREE(&h->regex.pattern);
  if (h->regex.regex)
  {
    regfree(h->regex.regex);
    FREE(&h->regex.regex);
  }
  mutt_pattern_free(&h->pattern);
  expando_free(&h->expando);
  FREE(ptr);
}

/**
 * hook_new - Create a Hook
 * @retval ptr New Hook
 */
struct Hook *hook_new(void)
{
  return mutt_mem_calloc_T(1, struct Hook);
}
