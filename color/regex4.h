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

#ifndef MUTT_COLOR_REGEX4_H
#define MUTT_COLOR_REGEX4_H

#include "config.h"
#include <stdbool.h>
#include <stdint.h>
#include "mutt/lib.h"
#include "attr.h"
#include "color.h"

/**
 * struct RegexColor - A regular expression and a color to highlight a line
 */
struct RegexColor
{
  struct AttrColor attr_color;       ///< Colour and attributes to apply
  char *pattern;                     ///< Pattern to match
  regex_t regex;                     ///< Compiled regex
  int match;                         ///< Substring to match, 0 for old behaviour
  struct PatternList *color_pattern; ///< Compiled pattern to speed up index color calculation

  bool stop_matching : 1;            ///< Used by the pager for body patterns, to prevent the color from being retried once it fails

  STAILQ_ENTRY(RegexColor) entries;   ///< Linked list
};
STAILQ_HEAD(RegexColorList, RegexColor);

#ifdef USE_DEBUG_COLOR
extern struct RegexColorList AttachList;
extern struct RegexColorList BodyList;
extern struct RegexColorList HeaderList;
extern struct RegexColorList IndexAuthorList;
extern struct RegexColorList IndexCollapsedList;
extern struct RegexColorList IndexDateList;
extern struct RegexColorList IndexFlagsList;
extern struct RegexColorList IndexLabelList;
extern struct RegexColorList IndexList;
extern struct RegexColorList IndexNumberList;
extern struct RegexColorList IndexSizeList;
extern struct RegexColorList IndexSubjectList;
extern struct RegexColorList IndexTagList;
extern struct RegexColorList IndexTagsList;
extern struct RegexColorList StatusList;
#endif

void                   regex_color_clear(struct RegexColor *rcol);
void                   regex_color_free(struct RegexColorList *list, struct RegexColor **ptr);
struct RegexColor *    regex_color_new (void);

void                   regex_colors_cleanup(void);
struct RegexColorList *regex_colors_get_list(enum ColorId cid);
void                   regex_colors_init(void);

void                   regex_color_list_clear(struct RegexColorList *rcl);

bool regex_colors_parse_color_list (enum ColorId cid, const char *pat, uint32_t fg, uint32_t bg, int attrs, int *rc,   struct Buffer *err);
int  regex_colors_parse_status_list(enum ColorId cid, const char *pat, uint32_t fg, uint32_t bg, int attrs, int match, struct Buffer *err);
bool regex_colors_parse_uncolor    (enum ColorId cid, const char *pat, bool uncolor);

#endif /* MUTT_COLOR_REGEX4_H */
