/**
 * @file
 * Color and attribute parsing
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

/**
 * @page lib_color Color
 *
 * Colour handling code
 *
 * | File                | Description                |
 * | :------------------ | :------------------------- |
 * | color/color.c       | @subpage color_color       |
 * | color/command.c     | @subpage color_command     |
 * | color/notify.c      | @subpage color_notify      |
 * | color/regex.c       | @subpage color_regex       |
 */

#ifndef MUTT_COLOR_LIB_H
#define MUTT_COLOR_LIB_H

#include "config.h"
#include <stdbool.h>
#include <stdint.h>
#include "mutt/lib.h"
#include "core/lib.h"

// IWYU pragma: begin_exports
#include "color.h"
#include "command2.h"
#include "notify2.h"
#include "regex4.h"
// IWYU pragma: end_exports

/// Ten colours, quoted0..quoted9 (quoted and quoted0 are equivalent)
#define COLOR_QUOTES_MAX 10

/**
 * struct ColorLine - A regular expression and a color to highlight a line
 */
struct ColorLine
{
  regex_t regex;                     ///< Compiled regex
  int match;                         ///< Substring to match, 0 for old behaviour
  char *pattern;                     ///< Pattern to match
  struct PatternList *color_pattern; ///< Compiled pattern to speed up index color calculation
  uint32_t fg;                       ///< Foreground colour
  uint32_t bg;                       ///< Background colour
  int pair;                          ///< Colour pair index

  bool stop_matching : 1;            ///< Used by the pager for body patterns, to prevent the color from being retried once it fails

  STAILQ_ENTRY(ColorLine) entries;   ///< Linked list
};
STAILQ_HEAD(ColorLineList, ColorLine);

bool mutt_color_is_header(enum ColorId color_id);
int mutt_color_alloc  (uint32_t fg, uint32_t bg);
int mutt_color_combine(uint32_t fg_attr, uint32_t bg_attr);
void mutt_color_free  (uint32_t fg, uint32_t bg);

void mutt_colors_init(void);
void mutt_colors_cleanup(void);

int mutt_color(enum ColorId id);
struct ColorLineList *mutt_color_status_line(void);
struct ColorLineList *mutt_color_index(void);
struct ColorLineList *mutt_color_headers(void);
struct ColorLineList *mutt_color_body(void);
struct ColorLineList *mutt_color_attachments(void);
struct ColorLineList *mutt_color_index_author(void);
struct ColorLineList *mutt_color_index_flags(void);
struct ColorLineList *mutt_color_index_subject(void);
struct ColorLineList *mutt_color_index_tags(void);
int mutt_color_quote(int quote);
int mutt_color_quotes_used(void);

void color_line_list_clear(struct ColorLineList *list);
void color_line_free(struct ColorLine **ptr, bool free_colors);
void colors_clear(void);

/**
 * Wrapper for all the colours
 */
struct Colors
{
  int defs[MT_COLOR_MAX]; ///< Array of all fixed colours, see enum ColorId

  /* These are lazily initialized, so make sure to always refer to them using
   * the mutt_color_<object>() wrappers. */
  // clang-format off
  struct ColorLineList attach_list;        ///< List of colours applied to the attachment headers
  struct ColorLineList body_list;          ///< List of colours applied to the email body
  struct ColorLineList hdr_list;           ///< List of colours applied to the email headers
  struct ColorLineList index_author_list;  ///< List of colours applied to the author in the index
  struct ColorLineList index_flags_list;   ///< List of colours applied to the flags in the index
  struct ColorLineList index_list;         ///< List of default colours applied to the index
  struct ColorLineList index_subject_list; ///< List of colours applied to the subject in the index
  struct ColorLineList index_tag_list;     ///< List of colours applied to tags in the index
  struct ColorLineList status_list;        ///< List of colours applied to the status bar
  // clang-format on

  int quotes[COLOR_QUOTES_MAX]; ///< Array of colours for quoted email text
  int quotes_used;              ///< Number of colours for quoted email text

  struct ColorList *user_colors; ///< Array of user colours
  int num_user_colors;           ///< Number of user colours

  struct Notify *notify; ///< Notifications: #ColorId, #EventColor
};

extern struct Colors Colors;

#endif /* MUTT_COLOR_LIB_H */
