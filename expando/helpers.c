/**
 * @file
 * Shared code
 *
 * @authors
 * Copyright (C) 2023-2024 Tóth János <gomba007@gmail.com>
 * Copyright (C) 2023-2024 Richard Russon <rich@flatcap.org>
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
 * @page expando_helpers Shared code
 *
 * Shared code
 */

#include "config.h"
#include <stddef.h>
#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include "mutt/lib.h"
#include "helpers.h"
#include "definition.h"
#include "render.h"

/**
 * find_get_number - Find a get_number() callback function
 * @param rdata Render data to search
 * @param did   Domain ID to match
 * @param uid   Unique ID to match
 * @retval ptr Matching Render data
 */
const struct ExpandoRenderData *find_get_number(const struct ExpandoRenderData *rdata,
                                                int did, int uid)
{
  if (!rdata)
    return NULL;

  for (; rdata->did != -1; rdata++)
  {
    if ((rdata->did == did) && (rdata->uid == uid) && rdata->get_number)
    {
      return rdata;
    }
  }

  return NULL;
}

/**
 * find_get_string - Find a get_string() callback function
 * @param rdata Render data to search
 * @param did   Domain ID to match
 * @param uid   Unique ID to match
 * @retval ptr Matching Render data
 */
const struct ExpandoRenderData *find_get_string(const struct ExpandoRenderData *rdata,
                                                int did, int uid)
{
  if (!rdata)
    return NULL;

  for (; rdata->did != -1; rdata++)
  {
    if ((rdata->did == did) && (rdata->uid == uid) && rdata->get_string)
    {
      return rdata;
    }
  }

  return NULL;
}

/**
 * skip_until_ch - Search a string for a terminator character
 * @param start      Start of string
 * @param terminator Character to find
 * @retval ptr Position of terminator character, or end-of-string
 */
const char *skip_until_ch(const char *start, char terminator)
{
  while (*start)
  {
    if (*start == terminator)
    {
      break;
    }

    start++;
  }

  return start;
}

/**
 * is_valid_classic_expando - Is this a valid Expando character?
 * @param ch Character to test
 * @retval true Valid Expando character
 */
static bool is_valid_classic_expando(char ch)
{
  // NOTE(g0mb4): Maybe rework this?
  // if special expandos are added this list must be updated!
  return isalpha(ch) || (ch == ' ') || (ch == '!') || (ch == '(') ||
         (ch == '*') || (ch == '>') || (ch == '@') || (ch == '[') ||
         (ch == '^') || (ch == '{') || (ch == '|');
}

/**
 * skip_until_classic_expando - Search through string until we reach an Expando character
 * @param start Where to start looking
 * @retval ptr Match, or end-of-string
 */
const char *skip_until_classic_expando(const char *start)
{
  while (*start && !is_valid_classic_expando(*start))
  {
    start++;
  }

  return start;
}

/**
 * skip_classic_expando - Skip over the text of an Expando
 * @param str  Starting place
 * @param defs Expando definitions
 * @retval ptr Character after Expando, or end-of-string
 */
const char *skip_classic_expando(const char *str, const struct ExpandoDefinition *defs)
{
  assert(*str != '\0');
  const struct ExpandoDefinition *definitions = defs;

  while (definitions && definitions->short_name)
  {
    const bool is_two_char = mutt_str_len(definitions->short_name) == 2;
    const char *name = definitions->short_name;

    if (is_two_char && (name[0] == *str) && (name[1] == *(str + 1)))
    {
      str++;
      break;
    }

    definitions++;
  }

  str++;
  return str;
}
