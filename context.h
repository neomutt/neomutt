/**
 * @file
 * The "currently-open" mailbox
 *
 * @authors
 * Copyright (C) 2017 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_CONTEXT_H
#define MUTT_CONTEXT_H

#include <stdbool.h>
#include <stdio.h>
#include <sys/types.h>
#include <time.h>

/**
 * struct Context - The "current" mailbox
 */
struct Context
{
  struct timespec mtime;
  off_t vsize;
  char *pattern;                 /**< limit pattern string */
  struct Pattern *limit_pattern; /**< compiled limit pattern */
  struct Header *last_tag;  /**< last tagged msg. used to link threads */
  struct MuttThread *tree;  /**< top of thread tree */
  struct Hash *id_hash;     /**< hash table by msg id */
  struct Hash *subj_hash;   /**< hash table by subject */
  struct Hash *thread_hash; /**< hash table for threading */
  struct Hash *label_hash;  /**< hash table for x-labels */
  int *v2r;                 /**< mapping from virtual to real msgno */
  int vcount;               /**< the number of virtual messages */
  int tagged;               /**< how many messages are tagged? */
  int new;                  /**< how many new messages? */
  int deleted;              /**< how many deleted messages */
  int msgnotreadyet;        /**< which msg "new" in pager, -1 if none */

  struct Menu *menu; /**< needed for pattern compilation */

  bool dontwrite : 1; /**< don't write the mailbox on close */
  bool append : 1;    /**< mailbox is opened in append mode */
  bool collapsed : 1; /**< are all threads collapsed? */
  bool peekonly : 1;  /**< just taking a glance, revert atime */

#ifdef USE_COMPRESSED
  void *compress_info; /**< compressed mbox module private data */
#endif                 /**< USE_COMPRESSED */

  struct Mailbox *mailbox;
};

#endif /* MUTT_CONTEXT_H */
