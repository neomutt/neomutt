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

#ifndef _MUTT_CONTEXT_H
#define _MUTT_CONTEXT_H

#include <stdbool.h>
#include <stdio.h>
#include <sys/types.h>
#include <time.h>

/**
 * enum AclRights - ACL Rights
 */
enum AclRights
{
  MUTT_ACL_LOOKUP = 0,
  MUTT_ACL_READ,
  MUTT_ACL_SEEN,
  MUTT_ACL_WRITE,
  MUTT_ACL_INSERT,
  MUTT_ACL_POST,
  MUTT_ACL_CREATE,
  MUTT_ACL_DELMX,
  MUTT_ACL_DELETE,
  MUTT_ACL_EXPUNGE,
  MUTT_ACL_ADMIN,

  RIGHTSMAX
};

/**
 * struct Context - The "current" mailbox
 */
struct Context
{
  char *path;
  char *realpath; /**< used for buffy comparison and the sidebar */
  FILE *fp;
  time_t atime;
  time_t mtime;
  off_t size;
  off_t vsize;
  char *pattern;                 /**< limit pattern string */
  struct Pattern *limit_pattern; /**< compiled limit pattern */
  struct Header **hdrs;
  struct Header *last_tag;  /**< last tagged msg. used to link threads */
  struct MuttThread *tree;  /**< top of thread tree */
  struct Hash *id_hash;     /**< hash table by msg id */
  struct Hash *subj_hash;   /**< hash table by subject */
  struct Hash *thread_hash; /**< hash table for threading */
  struct Hash *label_hash;  /**< hash table for x-labels */
  int *v2r;                 /**< mapping from virtual to real msgno */
  int hdrmax;               /**< number of pointers in hdrs */
  int msgcount;             /**< number of messages in the mailbox */
  int vcount;               /**< the number of virtual messages */
  int tagged;               /**< how many messages are tagged? */
  int new;                  /**< how many new messages? */
  int unread;               /**< how many unread messages? */
  int deleted;              /**< how many deleted messages */
  int flagged;              /**< how many flagged messages */
  int msgnotreadyet;        /**< which msg "new" in pager, -1 if none */

  struct Menu *menu; /**< needed for pattern compilation */

  short magic; /**< mailbox type */

  unsigned char rights[(RIGHTSMAX + 7) / 8]; /**< ACL bits */

  bool locked : 1;    /**< is the mailbox locked? */
  bool changed : 1;   /**< mailbox has been modified */
  bool readonly : 1;  /**< don't allow changes to the mailbox */
  bool dontwrite : 1; /**< don't write the mailbox on close */
  bool append : 1;    /**< mailbox is opened in append mode */
  bool quiet : 1;     /**< inhibit status messages? */
  bool collapsed : 1; /**< are all threads collapsed? */
  bool closing : 1;   /**< mailbox is being closed */
  bool peekonly : 1;  /**< just taking a glance, revert atime */

#ifdef USE_COMPRESSED
  void *compress_info; /**< compressed mbox module private data */
#endif                 /**< USE_COMPRESSED */

  /* driver hooks */
  void *data; /**< driver specific data */
  struct MxOps *mx_ops;
};

#endif /* _MUTT_CONTEXT_H */
