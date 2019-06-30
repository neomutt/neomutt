/**
 * @file
 * Help system
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

#ifndef _HELP_HELP_H
#define _HELP_HELP_H

#include <stdbool.h>
#include "mx.h"
#include <ftw.h>

extern struct MxOps MxHelpOps;

/**
 * enum helpdoc_type - Describes the type of a help file/document
 */
typedef enum helpdoc_type
{
  HDT_NONE    = (1 << 0), /* file isn't a help document */
  HDT_INDEX   = (1 << 1), /* document is treated as help index (index.md) */
  HDT_ROOTDOC = (1 << 2), /* document lives directly in root of #C_HelpDocDir */
  HDT_CHAPTER = (1 << 3), /* document is treated as help chapter */
  HDT_SECTION = (1 << 4)  /* document is treated as help section */
} HDType;
typedef unsigned int HDTMask;

/**
 * struct helplist - Generic list to hold several help elements
 */
typedef struct helplist
{
  size_t item_size;       /* size of a single element */
  size_t size;            /* list length */
  size_t capa;            /* list capacity */
  void **data;            /* internal list data pointers */
} HelpList;
#define HELPLIST_INIT_CAPACITY 10

/**
 * struct helpfile_header - Describes the header of a help file
 */
typedef struct helpfile_header
{
  char *key;
  char *val;
} HFHeader;

/**
 * struct helpdoc_meta - Bundle additional information to a help document
 */
typedef struct helpdoc_meta
{
  HelpList *fhdr;         /* file header lines (list of key/value pairs) */
  char *name;             /* base file name */
  HDType type;            /* type of the help document */
} HDMeta;

void help_doclist_free(void); /* release memory of the cached document list */
int help_doclist_init(void);  /* can be used for pre-caching help documents */

#endif /* _HELP_HELP_H */
