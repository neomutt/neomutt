/**
 * @file
 * Display version and copyright about NeoMutt
 *
 * @authors
 * Copyright (C) 2017-2025 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_VERSION_H
#define MUTT_VERSION_H

#include <stdbool.h>
#include <stdio.h>
#include "mutt/lib.h"

/**
 * struct KeyValue - Key/Value pairs
 */
struct KeyValue
{
  const char *key;
  const char *value;
};
ARRAY_HEAD(KeyValueArray, struct KeyValue);

/**
 * struct CompileOption - Built-in capability
 */
struct CompileOption
{
  const char *name; ///< Option name
  int enabled;      ///< 0 Disabled, 1 Enabled, 2 Devel only
};

/**
 * struct NeoMuttVersion - Version info about NeoMutt
 */
struct NeoMuttVersion
{
  const char *version;                    ///< Version of NeoMutt: YYYYMMDD-NUM-HASH (number of commits, git hash)

  struct KeyValueArray system;            ///< System information

  struct Slist *storage;                  ///< Storage backends, e.g. lmdb
  struct Slist *compression;              ///< Compression methods, e.g. zlib

  struct Slist *configure;                ///< Configure options
  struct Slist *compilation;              ///< Compilation CFLAGS

  const struct CompileOption *feature;    ///< Compiled-in features
  const struct CompileOption *devel;      ///< Compiled-in development features

  struct KeyValueArray paths;             ///< Compiled-in paths
};

struct NeoMuttVersion *version_get (void);
void                   version_free(struct NeoMuttVersion **ptr);

const char *mutt_make_version(void);
bool print_version(FILE *fp, bool use_ansi);
bool print_copyright(void);
bool feature_enabled(const char *name);

#endif /* MUTT_VERSION_H */
