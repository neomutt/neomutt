/**
 * @file
 * Regex Colour
 *
 * @authors
 * Copyright (C) 2021 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_COLOR_REGEX_H
#define MUTT_COLOR_REGEX_H

#include "config.h"
#include <stdbool.h>
#include <stdint.h>
#include "mutt/lib.h"
#include "lib.h"

/**
 * struct RegexColor - A regular expression and a color to highlight a line
 */
struct RegexColor
{
  char *pattern;                     ///< Pattern to match
  regex_t regex;                     ///< Compiled regex
  int match;                         ///< Substring to match, 0 for old behaviour
  struct PatternList *color_pattern; ///< Compiled pattern to speed up index color calculation
  uint32_t fg;                       ///< Foreground colour
  uint32_t bg;                       ///< Background colour
  int pair;                          ///< Colour pair index

  bool stop_matching : 1;            ///< Used by the pager for body patterns, to prevent the color from being retried once it fails

  STAILQ_ENTRY(RegexColor) entries;   ///< Linked list
};
STAILQ_HEAD(RegexColorList, RegexColor);

void                   regex_color_free(struct RegexColor **ptr, bool free_colors);
void                   regex_color_list_clear(struct RegexColorList *rcl);
struct RegexColor *    regex_color_new (void);
void                   regex_colors_clear(void);
struct RegexColorList *regex_colors_get_list(enum ColorId id);
void                   regex_colors_init(void);

enum CommandResult add_pattern(struct RegexColorList *top, const char *s, bool sensitive, uint32_t fg, uint32_t bg, int attr, struct Buffer *err, bool is_index, int match);

#endif /* MUTT_COLOR_REGEX_H */
