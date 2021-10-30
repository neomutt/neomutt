/**
 * @file
 * Colour Debugging
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

#ifndef MUTT_COLOR_DEBUG_H
#define MUTT_COLOR_DEBUG_H

#include "config.h"
#include <stdbool.h>
#include "mutt/lib.h"
#include "color.h"

struct AttrColorList;
struct RegexColor;
struct RegexColorList;

const char *color_debug_log_color_attrs(int fg, int bg, int attrs);
const char *color_debug_log_name(char *buf, int buflen, int color);
const char *color_debug_log_attrs_list(int attrs);

#ifdef USE_DEBUG_COLOR

const char *color_debug_log_attrs(int attrs);
const char *color_debug_log_color(int fg, int bg);
void attr_color_dump       (struct AttrColor *ac, const char *prefix);
void attr_color_list_dump  (struct AttrColorList *acl, const char *title);

void curses_color_dump     (struct CursesColor *cc, const char *prefix);
void curses_colors_dump    (void);

void merged_colors_dump    (void);

void quoted_color_dump     (struct AttrColor *ac, int q_level, const char *prefix);
void quoted_color_list_dump(void);

void regex_color_dump      (struct RegexColor *rcol, const char *prefix);
void regex_color_list_dump (const char *name, struct RegexColorList *rcl);
void regex_colors_dump_all (void);

void simple_color_dump     (enum ColorId cid, const char *prefix);
void simple_colors_dump    (bool force);

int color_debug(enum LogLevel level, const char *format, ...);

#else

static inline const char *color_debug_log_attrs(int attrs) { return ""; }
static inline const char *color_debug_log_color(int fg, int bg) { return ""; }

static inline void attr_color_dump       (struct AttrColor *ac, const char *prefix) {}
static inline void attr_color_list_dump  (struct AttrColorList *acl, const char *title) {}

static inline void curses_color_dump     (struct CursesColor *cc, const char *prefix) {}
static inline void curses_colors_dump    (void) {}

static inline void merged_colors_dump    (void) {}

static inline void quoted_color_dump     (struct AttrColor *ac, int q_level, const char *prefix) {}
static inline void quoted_color_list_dump(void) {}

static inline void regex_color_dump      (struct RegexColor *rcol, const char *prefix) {}
static inline void regex_color_list_dump (const char *name, struct RegexColorList *rcl) {}
static inline void regex_colors_dump_all (void) {}

static inline void simple_color_dump     (enum ColorId cid, const char *prefix) {}
static inline void simple_colors_dump    (bool force) {}

static inline int color_debug(enum LogLevel level, const char *format, ...) { return 0; }

#endif

#endif /* MUTT_COLOR_DEBUG_H */
