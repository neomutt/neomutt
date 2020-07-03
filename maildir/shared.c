/**
 * @file
 * Maildir/MH local mailbox type
 *
 * @authors
 * Copyright (C) 1996-2002,2007,2009 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 1999-2005 Thomas Roessler <roessler@does-not-exist.org>
 * Copyright (C) 2010,2013 Michael R. Elkins <me@mutt.org>
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
 * @page maildir_shared Maildir/MH local mailbox type
 *
 * Maildir/MH local mailbox type
 */

#include "config.h"
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <utime.h>
#include "private.h"
#include "mutt/lib.h"
#include "config/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "mutt.h"
#include "copy.h"
#include "mutt_globals.h"
#include "muttlib.h"
#include "mx.h"
#include "progress.h"
#include "protos.h"
#include "sort.h"
#include "hcache/lib.h"
#include "maildir/lib.h"
#ifdef USE_NOTMUCH
#include "notmuch/lib.h"
#endif

/* These Config Variables are only used in maildir/mh.c */
bool C_CheckNew; ///< Config: (maildir,mh) Check for new mail while the mailbox is open
bool C_MaildirHeaderCacheVerify; ///< Config: (hcache) Check for maildir changes when opening mailbox
bool C_MhPurge;       ///< Config: Really delete files in MH mailboxes
char *C_MhSeqFlagged; ///< Config: MH sequence for flagged message
char *C_MhSeqReplied; ///< Config: MH sequence to tag replied messages
char *C_MhSeqUnseen;  ///< Config: MH sequence for unseen messages

#define INS_SORT_THRESHOLD 6

/**
 * maildir_mdata_free - Free data attached to the Mailbox
 * @param[out] ptr Maildir data
 */
static void maildir_mdata_free(void **ptr)
{
  if (!ptr || !*ptr)
    return;

  // struct MaildirMboxData *mdata = *ptr;
  FREE(ptr);
}

/**
 * maildir_mdata_new - Create a new MaildirMboxData object
 * @retval ptr New MaildirMboxData struct
 */
static struct MaildirMboxData *maildir_mdata_new(void)
{
  struct MaildirMboxData *mdata = mutt_mem_calloc(1, sizeof(struct MaildirMboxData));
  return mdata;
}

/**
 * maildir_mdata_get - Get the private data for this Mailbox
 * @param m Mailbox
 * @retval ptr MaildirMboxData
 */
struct MaildirMboxData *maildir_mdata_get(struct Mailbox *m)
{
  if (!m || ((m->type != MUTT_MAILDIR) && (m->type != MUTT_MH)))
    return NULL;
  return m->mdata;
}

/**
 * mh_umask - Create a umask from the mailbox directory
 * @param  m   Mailbox
 * @retval num Umask
 */
mode_t mh_umask(struct Mailbox *m)
{
  struct MaildirMboxData *mdata = maildir_mdata_get(m);
  if (mdata && mdata->mh_umask)
    return mdata->mh_umask;

  struct stat st;
  if (stat(mailbox_path(m), &st) != 0)
  {
    mutt_debug(LL_DEBUG1, "stat failed on %s\n", mailbox_path(m));
    return 077;
  }

  return 0777 & ~st.st_mode;
}

/**
 * mh_mkstemp - Create a temporary file
 * @param[in]  m   Mailbox to create the file in
 * @param[out] fp  File handle
 * @param[out] tgt File name
 * @retval  0 Success
 * @retval -1 Failure
 */
int mh_mkstemp(struct Mailbox *m, FILE **fp, char **tgt)
{
  int fd;
  char path[PATH_MAX];

  mode_t omask = umask(mh_umask(m));
  while (true)
  {
    snprintf(path, sizeof(path), "%s/.neomutt-%s-%d-%" PRIu64, mailbox_path(m),
             NONULL(ShortHostname), (int) getpid(), mutt_rand64());
    fd = open(path, O_WRONLY | O_EXCL | O_CREAT, 0666);
    if (fd == -1)
    {
      if (errno != EEXIST)
      {
        mutt_perror(path);
        umask(omask);
        return -1;
      }
    }
    else
    {
      *tgt = mutt_str_dup(path);
      break;
    }
  }
  umask(omask);

  *fp = fdopen(fd, "w");
  if (!*fp)
  {
    FREE(tgt);
    close(fd);
    unlink(path);
    return -1;
  }

  return 0;
}

/**
 * mh_sequences_add_one - Update the flags for one sequence
 * @param m       Mailbox
 * @param n       Sequence number to update
 * @param unseen  Update the unseen sequence
 * @param flagged Update the flagged sequence
 * @param replied Update the replied sequence
 */
static void mh_sequences_add_one(struct Mailbox *m, int n, bool unseen, bool flagged, bool replied)
{
  bool unseen_done = false;
  bool flagged_done = false;
  bool replied_done = false;

  char *tmpfname = NULL;
  char sequences[PATH_MAX];

  char seq_unseen[256];
  char seq_replied[256];
  char seq_flagged[256];

  char *buf = NULL;
  size_t sz;

  FILE *fp_new = NULL;
  if (mh_mkstemp(m, &fp_new, &tmpfname) == -1)
    return;

  snprintf(seq_unseen, sizeof(seq_unseen), "%s:", NONULL(C_MhSeqUnseen));
  snprintf(seq_replied, sizeof(seq_replied), "%s:", NONULL(C_MhSeqReplied));
  snprintf(seq_flagged, sizeof(seq_flagged), "%s:", NONULL(C_MhSeqFlagged));

  snprintf(sequences, sizeof(sequences), "%s/.mh_sequences", mailbox_path(m));
  FILE *fp_old = fopen(sequences, "r");
  if (fp_old)
  {
    while ((buf = mutt_file_read_line(buf, &sz, fp_old, NULL, 0)))
    {
      if (unseen && mutt_strn_equal(buf, seq_unseen, mutt_str_len(seq_unseen)))
      {
        fprintf(fp_new, "%s %d\n", buf, n);
        unseen_done = true;
      }
      else if (flagged && mutt_strn_equal(buf, seq_flagged, mutt_str_len(seq_flagged)))
      {
        fprintf(fp_new, "%s %d\n", buf, n);
        flagged_done = true;
      }
      else if (replied && mutt_strn_equal(buf, seq_replied, mutt_str_len(seq_replied)))
      {
        fprintf(fp_new, "%s %d\n", buf, n);
        replied_done = true;
      }
      else
        fprintf(fp_new, "%s\n", buf);
    }
  }
  mutt_file_fclose(&fp_old);
  FREE(&buf);

  if (!unseen_done && unseen)
    fprintf(fp_new, "%s: %d\n", NONULL(C_MhSeqUnseen), n);
  if (!flagged_done && flagged)
    fprintf(fp_new, "%s: %d\n", NONULL(C_MhSeqFlagged), n);
  if (!replied_done && replied)
    fprintf(fp_new, "%s: %d\n", NONULL(C_MhSeqReplied), n);

  mutt_file_fclose(&fp_new);

  unlink(sequences);
  if (mutt_file_safe_rename(tmpfname, sequences) != 0)
    unlink(tmpfname);

  FREE(&tmpfname);
}

/**
 * maildir_entry_new - Create a new Maildir entry
 * @retval ptr New Maildir entry
 */
static struct Maildir *maildir_entry_new(void)
{
  return mutt_mem_calloc(1, sizeof(struct Maildir));
}

/**
 * maildir_entry_free - Free a Maildir object
 * @param[out] ptr Maildir to free
 */
static void maildir_entry_free(struct Maildir **ptr)
{
  if (!ptr || !*ptr)
    return;

  struct Maildir *md = *ptr;
  FREE(&md->canon_fname);
  email_free(&md->email);

  FREE(ptr);
}

/**
 * maildir_free - Free a Maildir list
 * @param[out] md Maildir list to free
 */
static void maildir_free(struct Maildir **md)
{
  if (!md || !*md)
    return;

  struct Maildir *p = NULL, *q = NULL;

  for (p = *md; p; p = q)
  {
    q = p->next;
    maildir_entry_free(&p);
  }
}

/**
 * maildir_update_mtime - Update our record of the Maildir modification time
 * @param m Mailbox
 */
static void maildir_update_mtime(struct Mailbox *m)
{
  char buf[PATH_MAX];
  struct stat st;
  struct MaildirMboxData *mdata = maildir_mdata_get(m);

  if (m->type == MUTT_MAILDIR)
  {
    snprintf(buf, sizeof(buf), "%s/%s", mailbox_path(m), "cur");
    if (stat(buf, &st) == 0)
      mutt_file_get_stat_timespec(&mdata->mtime_cur, &st, MUTT_STAT_MTIME);
    snprintf(buf, sizeof(buf), "%s/%s", mailbox_path(m), "new");
  }
  else
  {
    snprintf(buf, sizeof(buf), "%s/.mh_sequences", mailbox_path(m));
    if (stat(buf, &st) == 0)
      mutt_file_get_stat_timespec(&mdata->mtime_cur, &st, MUTT_STAT_MTIME);

    mutt_str_copy(buf, mailbox_path(m), sizeof(buf));
  }

  if (stat(buf, &st) == 0)
    mutt_file_get_stat_timespec(&m->mtime, &st, MUTT_STAT_MTIME);
}

/**
 * maildir_parse_dir - Read a Maildir mailbox
 * @param[in]  m        Mailbox
 * @param[out] last     Last Maildir
 * @param[in]  subdir   Subdirectory, e.g. 'new'
 * @param[out] count    Counter for the progress bar
 * @param[in]  progress Progress bar
 * @retval  0 Success
 * @retval -1 Error
 * @retval -2 Aborted
 */
int maildir_parse_dir(struct Mailbox *m, struct Maildir ***last,
                      const char *subdir, int *count, struct Progress *progress)
{
  struct dirent *de = NULL;
  int rc = 0;
  bool is_old = false;
  struct Maildir *entry = NULL;
  struct Email *e = NULL;

  struct Buffer *buf = mutt_buffer_pool_get();

  if (subdir)
  {
    mutt_buffer_printf(buf, "%s/%s", mailbox_path(m), subdir);
    is_old = C_MarkOld ? mutt_str_equal("cur", subdir) : false;
  }
  else
    mutt_buffer_strcpy(buf, mailbox_path(m));

  DIR *dirp = opendir(mutt_b2s(buf));
  if (!dirp)
  {
    rc = -1;
    goto cleanup;
  }

  while (((de = readdir(dirp))) && (SigInt != 1))
  {
    if (((m->type == MUTT_MH) && !mh_valid_message(de->d_name)) ||
        ((m->type == MUTT_MAILDIR) && (*de->d_name == '.')))
    {
      continue;
    }

    /* FOO - really ignore the return value? */
    mutt_debug(LL_DEBUG2, "queueing %s\n", de->d_name);

    e = email_new();
    e->old = is_old;
    if (m->type == MUTT_MAILDIR)
      maildir_parse_flags(e, de->d_name);

    if (count)
    {
      (*count)++;
      if (m->verbose && progress)
        mutt_progress_update(progress, *count, -1);
    }

    if (subdir)
    {
      mutt_buffer_printf(buf, "%s/%s", subdir, de->d_name);
      e->path = mutt_buffer_strdup(buf);
    }
    else
      e->path = mutt_str_dup(de->d_name);

    entry = maildir_entry_new();
    entry->email = e;
    entry->inode = de->d_ino;
    **last = entry;
    *last = &entry->next;
  }

  closedir(dirp);

  if (SigInt == 1)
  {
    SigInt = 0;
    return -2; /* action aborted */
  }

cleanup:
  mutt_buffer_pool_release(&buf);

  return rc;
}

/**
 * maildir_move_to_mailbox - Copy the Maildir list to the Mailbox
 * @param[in]  m   Mailbox
 * @param[out] ptr Maildir list to copy, then free
 * @retval num Number of new emails
 * @retval 0   Error
 */
int maildir_move_to_mailbox(struct Mailbox *m, struct Maildir **ptr)
{
  if (!m)
    return 0;

  struct Maildir *md = *ptr;
  int oldmsgcount = m->msg_count;

  for (; md; md = md->next)
  {
    mutt_debug(LL_DEBUG2, "Considering %s\n", NONULL(md->canon_fname));
    if (!md->email)
      continue;

    mutt_debug(LL_DEBUG2, "Adding header structure. Flags: %s%s%s%s%s\n",
               md->email->flagged ? "f" : "", md->email->deleted ? "D" : "",
               md->email->replied ? "r" : "", md->email->old ? "O" : "",
               md->email->read ? "R" : "");
    if (m->msg_count == m->email_max)
      mx_alloc_memory(m);

    m->emails[m->msg_count] = md->email;
    m->emails[m->msg_count]->index = m->msg_count;
    mailbox_size_add(m, md->email);

    md->email = NULL;
    m->msg_count++;
  }

  int num = 0;
  if (m->msg_count > oldmsgcount)
    num = m->msg_count - oldmsgcount;

  maildir_free(ptr);
  return num;
}

/**
 * maildir_hcache_keylen - Calculate the length of the Maildir path
 * @param fn File name
 * @retval num Length in bytes
 *
 * @note This length excludes the flags, which will vary
 */
size_t maildir_hcache_keylen(const char *fn)
{
  const char *p = strrchr(fn, ':');
  return p ? (size_t)(p - fn) : mutt_str_len(fn);
}

/**
 * md_cmp_inode - Compare two Maildirs by inode number
 * @param a First  Maildir
 * @param b Second Maildir
 * @retval -1 a precedes b
 * @retval  0 a and b are identical
 * @retval  1 b precedes a
 */
static int md_cmp_inode(struct Maildir *a, struct Maildir *b)
{
  return a->inode - b->inode;
}

/**
 * md_cmp_path - Compare two Maildirs by path
 * @param a First  Maildir
 * @param b Second Maildir
 * @retval -1 a precedes b
 * @retval  0 a and b are identical
 * @retval  1 b precedes a
 */
static int md_cmp_path(struct Maildir *a, struct Maildir *b)
{
  return strcmp(a->email->path, b->email->path);
}

/**
 * maildir_merge_lists - Merge two maildir lists
 * @param left    First  Maildir to merge
 * @param right   Second Maildir to merge
 * @param cmp     Comparison function for sorting
 * @retval ptr Merged Maildir
 */
static struct Maildir *maildir_merge_lists(struct Maildir *left, struct Maildir *right,
                                           int (*cmp)(struct Maildir *, struct Maildir *))
{
  struct Maildir *head = NULL;
  struct Maildir *tail = NULL;

  if (left && right)
  {
    if (cmp(left, right) < 0)
    {
      head = left;
      left = left->next;
    }
    else
    {
      head = right;
      right = right->next;
    }
  }
  else
  {
    if (left)
      return left;
    return right;
  }

  tail = head;

  while (left && right)
  {
    if (cmp(left, right) < 0)
    {
      tail->next = left;
      left = left->next;
    }
    else
    {
      tail->next = right;
      right = right->next;
    }
    tail = tail->next;
  }

  if (left)
  {
    tail->next = left;
  }
  else
  {
    tail->next = right;
  }

  return head;
}

/**
 * maildir_ins_sort - Sort maildirs using an insertion sort
 * @param list    Maildir list to sort
 * @param cmp     Comparison function for sorting
 * @retval ptr Sort Maildir list
 */
static struct Maildir *maildir_ins_sort(struct Maildir *list,
                                        int (*cmp)(struct Maildir *, struct Maildir *))
{
  struct Maildir *tmp = NULL, *last = NULL, *back = NULL;

  struct Maildir *ret = list;
  list = list->next;
  ret->next = NULL;

  while (list)
  {
    last = NULL;
    back = list->next;
    for (tmp = ret; tmp && cmp(tmp, list) <= 0; tmp = tmp->next)
      last = tmp;

    list->next = tmp;
    if (last)
      last->next = list;
    else
      ret = list;

    list = back;
  }

  return ret;
}

/**
 * maildir_sort - Sort Maildir list
 * @param list    Maildirs to sort
 * @param len     Number of Maildirs in the list
 * @param cmp     Comparison function for sorting
 * @retval ptr Sort Maildir list
 */
static struct Maildir *maildir_sort(struct Maildir *list, size_t len,
                                    int (*cmp)(struct Maildir *, struct Maildir *))
{
  struct Maildir *left = list;
  struct Maildir *right = list;
  size_t c = 0;

  if (!list || !list->next)
  {
    return list;
  }

  if ((len != (size_t)(-1)) && (len <= INS_SORT_THRESHOLD))
    return maildir_ins_sort(list, cmp);

  list = list->next;
  while (list && list->next)
  {
    right = right->next;
    list = list->next->next;
    c++;
  }

  list = right;
  right = right->next;
  list->next = 0;

  left = maildir_sort(left, c, cmp);
  right = maildir_sort(right, c, cmp);
  return maildir_merge_lists(left, right, cmp);
}

/**
 * mh_sort_natural - Sort a Maildir list into its natural order
 * @param[in]  m  Mailbox
 * @param[out] md Maildir list to sort
 *
 * Currently only defined for MH where files are numbered.
 */
static void mh_sort_natural(struct Mailbox *m, struct Maildir **md)
{
  if (!m || !md || !*md || (m->type != MUTT_MH) || (C_Sort != SORT_ORDER))
    return;
  mutt_debug(LL_DEBUG3, "maildir: sorting %s into natural order\n", mailbox_path(m));
  *md = maildir_sort(*md, (size_t) -1, md_cmp_path);
}

/**
 * skip_duplicates - Find the first non-duplicate message
 * @param[in]  p    Maildir to start
 * @param[out] last Previous Maildir
 * @retval ptr Next Maildir
 */
static struct Maildir *skip_duplicates(struct Maildir *p, struct Maildir **last)
{
  /* Skip ahead to the next non-duplicate message.
   *
   * p should never reach NULL, because we couldn't have reached this point
   * unless there was a message that needed to be parsed.
   *
   * The check for p->header_parsed is likely unnecessary since the dupes will
   * most likely be at the head of the list.  but it is present for consistency
   * with the check at the top of the for() loop in maildir_delayed_parsing().
   */
  while (!p->email || p->header_parsed)
  {
    *last = p;
    p = p->next;
  }
  return p;
}

/**
 * maildir_delayed_parsing - This function does the second parsing pass
 * @param[in]  m  Mailbox
 * @param[out] md Maildir to parse
 * @param[in]  progress Progress bar
 */
void maildir_delayed_parsing(struct Mailbox *m, struct Maildir **md, struct Progress *progress)
{
  struct Maildir *p = NULL, *last = NULL;
  char fn[PATH_MAX];
  int count;
  bool sort = false;

#ifdef USE_HCACHE
  header_cache_t *hc = mutt_hcache_open(C_HeaderCache, mailbox_path(m), NULL);
#endif

  for (p = *md, count = 0; p; p = p->next, count++)
  {
    if (!(p && p->email && !p->header_parsed))
    {
      last = p;
      continue;
    }

    if (m->verbose && progress)
      mutt_progress_update(progress, count, -1);

    if (!sort)
    {
      mutt_debug(LL_DEBUG3, "maildir: need to sort %s by inode\n", mailbox_path(m));
      p = maildir_sort(p, (size_t) -1, md_cmp_inode);
      if (last)
        last->next = p;
      else
        *md = p;
      sort = true;
      p = skip_duplicates(p, &last);
      snprintf(fn, sizeof(fn), "%s/%s", mailbox_path(m), p->email->path);
    }

    snprintf(fn, sizeof(fn), "%s/%s", mailbox_path(m), p->email->path);

#ifdef USE_HCACHE
    struct stat lastchanged = { 0 };
    int rc = 0;
    if (C_MaildirHeaderCacheVerify)
    {
      rc = stat(fn, &lastchanged);
    }

    const char *key = NULL;
    size_t keylen = 0;
    if (m->type == MUTT_MH)
    {
      key = p->email->path;
      keylen = strlen(key);
    }
    else
    {
      key = p->email->path + 3;
      keylen = maildir_hcache_keylen(key);
    }
    struct HCacheEntry hce = mutt_hcache_fetch(hc, key, keylen, 0);

    if (hce.email && (rc == 0) && (lastchanged.st_mtime <= hce.uidvalidity))
    {
      hce.email->old = p->email->old;
      hce.email->path = mutt_str_dup(p->email->path);
      email_free(&p->email);
      p->email = hce.email;
      if (m->type == MUTT_MAILDIR)
        maildir_parse_flags(p->email, fn);
    }
    else
#endif
    {
      if (maildir_parse_message(m->type, fn, p->email->old, p->email))
      {
        p->header_parsed = true;
#ifdef USE_HCACHE
        if (m->type == MUTT_MH)
        {
          key = p->email->path;
          keylen = strlen(key);
        }
        else
        {
          key = p->email->path + 3;
          keylen = maildir_hcache_keylen(key);
        }
        mutt_hcache_store(hc, key, keylen, p->email, 0);
#endif
      }
      else
        email_free(&p->email);
    }
    last = p;
  }
#ifdef USE_HCACHE
  mutt_hcache_close(hc);
#endif

  mh_sort_natural(m, md);
}

/**
 * mh_read_dir - Read a MH/maildir style mailbox
 * @param m      Mailbox
 * @param subdir NULL for MH mailboxes,
 *               otherwise the subdir of the maildir mailbox to read from
 * @retval  0 Success
 * @retval -1 Failure
 */
int mh_read_dir(struct Mailbox *m, const char *subdir)
{
  if (!m)
    return -1;

  struct Maildir *md = NULL;
  struct MhSequences mhs = { 0 };
  struct Maildir **last = NULL;
  struct Progress progress;

  if (m->verbose)
  {
    char msg[PATH_MAX];
    snprintf(msg, sizeof(msg), _("Scanning %s..."), mailbox_path(m));
    mutt_progress_init(&progress, msg, MUTT_PROGRESS_READ, 0);
  }

  struct MaildirMboxData *mdata = maildir_mdata_get(m);
  if (!mdata)
  {
    mdata = maildir_mdata_new();
    m->mdata = mdata;
    m->mdata_free = maildir_mdata_free;
  }

  maildir_update_mtime(m);

  md = NULL;
  last = &md;
  int count = 0;
  if (maildir_parse_dir(m, &last, subdir, &count, &progress) < 0)
    return -1;

  if (m->verbose)
  {
    char msg[PATH_MAX];
    snprintf(msg, sizeof(msg), _("Reading %s..."), mailbox_path(m));
    mutt_progress_init(&progress, msg, MUTT_PROGRESS_READ, count);
  }
  maildir_delayed_parsing(m, &md, &progress);

  if (m->type == MUTT_MH)
  {
    if (mh_read_sequences(&mhs, mailbox_path(m)) < 0)
    {
      maildir_free(&md);
      return -1;
    }
    mh_update_maildir(md, &mhs);
    mhs_sequences_free(&mhs);
  }

  maildir_move_to_mailbox(m, &md);

  if (!mdata->mh_umask)
    mdata->mh_umask = mh_umask(m);

  return 0;
}

/**
 * mh_commit_msg - Commit a message to an MH folder
 * @param m   Mailbox
 * @param msg Message to commit
 * @param e   Email
 * @param updseq  If true, update the sequence number
 * @retval  0 Success
 * @retval -1 Failure
 */
int mh_commit_msg(struct Mailbox *m, struct Message *msg, struct Email *e, bool updseq)
{
  struct dirent *de = NULL;
  char *cp = NULL, *dep = NULL;
  unsigned int n, hi = 0;
  char path[PATH_MAX];
  char tmp[16];

  if (mutt_file_fsync_close(&msg->fp))
  {
    mutt_perror(_("Could not flush message to disk"));
    return -1;
  }

  DIR *dirp = opendir(mailbox_path(m));
  if (!dirp)
  {
    mutt_perror(mailbox_path(m));
    return -1;
  }

  /* figure out what the next message number is */
  while ((de = readdir(dirp)))
  {
    dep = de->d_name;
    if (*dep == ',')
      dep++;
    cp = dep;
    while (*cp)
    {
      if (!isdigit((unsigned char) *cp))
        break;
      cp++;
    }
    if (*cp == '\0')
    {
      if (mutt_str_atoui(dep, &n) < 0)
        mutt_debug(LL_DEBUG2, "Invalid MH message number '%s'\n", dep);
      if (n > hi)
        hi = n;
    }
  }
  closedir(dirp);

  /* Now try to rename the file to the proper name.
   * Note: We may have to try multiple times, until we find a free slot.  */

  while (true)
  {
    hi++;
    snprintf(tmp, sizeof(tmp), "%u", hi);
    snprintf(path, sizeof(path), "%s/%s", mailbox_path(m), tmp);
    if (mutt_file_safe_rename(msg->path, path) == 0)
    {
      if (e)
        mutt_str_replace(&e->path, tmp);
      mutt_str_replace(&msg->committed_path, path);
      FREE(&msg->path);
      break;
    }
    else if (errno != EEXIST)
    {
      mutt_perror(mailbox_path(m));
      return -1;
    }
  }
  if (updseq)
  {
    mh_sequences_add_one(m, hi, !msg->flags.read, msg->flags.flagged, msg->flags.replied);
  }
  return 0;
}

/**
 * md_commit_message - Commit a message to a maildir folder
 * @param m   Mailbox
 * @param msg Message to commit
 * @param e   Email
 * @retval  0 Success
 * @retval -1 Failure
 *
 * msg->path contains the file name of a file in tmp/. We take the
 * flags from this file's name.
 *
 * m is the mail folder we commit to.
 *
 * e is a header structure to which we write the message's new
 * file name.  This is used in the mh and maildir folder sync
 * routines.  When this routine is invoked from mx_msg_commit(),
 * e is NULL.
 *
 * msg->path looks like this:
 *
 *    tmp/{cur,new}.neomutt-HOSTNAME-PID-COUNTER:flags
 *
 * See also maildir_msg_open_new().
 */
int md_commit_message(struct Mailbox *m, struct Message *msg, struct Email *e)
{
  char subdir[4];
  char suffix[16];
  int rc = 0;

  if (mutt_file_fsync_close(&msg->fp))
  {
    mutt_perror(_("Could not flush message to disk"));
    return -1;
  }

  /* extract the subdir */
  char *s = strrchr(msg->path, '/') + 1;
  mutt_str_copy(subdir, s, 4);

  /* extract the flags */
  s = strchr(s, ':');
  if (s)
    mutt_str_copy(suffix, s, sizeof(suffix));
  else
    suffix[0] = '\0';

  /* construct a new file name. */
  struct Buffer *path = mutt_buffer_pool_get();
  struct Buffer *full = mutt_buffer_pool_get();
  while (true)
  {
    mutt_buffer_printf(path, "%s/%lld.R%" PRIu64 ".%s%s", subdir,
                       (long long) mutt_date_epoch(), mutt_rand64(),
                       NONULL(ShortHostname), suffix);
    mutt_buffer_printf(full, "%s/%s", mailbox_path(m), mutt_b2s(path));

    mutt_debug(LL_DEBUG2, "renaming %s to %s\n", msg->path, mutt_b2s(full));

    if (mutt_file_safe_rename(msg->path, mutt_b2s(full)) == 0)
    {
      /* Adjust the mtime on the file to match the time at which this
       * message was received.  Currently this is only set when copying
       * messages between mailboxes, so we test to ensure that it is
       * actually set.  */
      if (msg->received)
      {
        struct utimbuf ut;

        ut.actime = msg->received;
        ut.modtime = msg->received;
        if (utime(mutt_b2s(full), &ut))
        {
          mutt_perror(_("md_commit_message(): unable to set time on file"));
          rc = -1;
          goto cleanup;
        }
      }

#ifdef USE_NOTMUCH
      if (m->type == MUTT_NOTMUCH)
        nm_update_filename(m, e->path, mutt_b2s(full), e);
#endif
      if (e)
        mutt_str_replace(&e->path, mutt_b2s(path));
      mutt_str_replace(&msg->committed_path, mutt_b2s(full));
      FREE(&msg->path);

      goto cleanup;
    }
    else if (errno != EEXIST)
    {
      mutt_perror(mailbox_path(m));
      rc = -1;
      goto cleanup;
    }
  }

cleanup:
  mutt_buffer_pool_release(&path);
  mutt_buffer_pool_release(&full);

  return rc;
}

/**
 * mh_rewrite_message - Sync a message in an MH folder
 * @param m     Mailbox
 * @param msgno Index number
 * @retval  0 Success
 * @retval -1 Error
 *
 * This code is also used for attachment deletion in maildir folders.
 */
int mh_rewrite_message(struct Mailbox *m, int msgno)
{
  if (!m || !m->emails || (msgno >= m->msg_count))
    return -1;

  struct Email *e = m->emails[msgno];
  if (!e)
    return -1;

  bool restore = true;

  long old_body_offset = e->content->offset;
  long old_body_length = e->content->length;
  long old_hdr_lines = e->lines;

  struct Message *dest = mx_msg_open_new(m, e, MUTT_MSG_NO_FLAGS);
  if (!dest)
    return -1;

  int rc = mutt_copy_message(dest->fp, m, e, MUTT_CM_UPDATE, CH_UPDATE | CH_UPDATE_LEN, 0);
  if (rc == 0)
  {
    char oldpath[PATH_MAX];
    char partpath[PATH_MAX];
    snprintf(oldpath, sizeof(oldpath), "%s/%s", mailbox_path(m), e->path);
    mutt_str_copy(partpath, e->path, sizeof(partpath));

    if (m->type == MUTT_MAILDIR)
      rc = md_commit_message(m, dest, e);
    else
      rc = mh_commit_msg(m, dest, e, false);

    mx_msg_close(m, &dest);

    if (rc == 0)
    {
      unlink(oldpath);
      restore = false;
    }

    /* Try to move the new message to the old place.
     * (MH only.)
     *
     * This is important when we are just updating flags.
     *
     * Note that there is a race condition against programs which
     * use the first free slot instead of the maximum message
     * number.  NeoMutt does _not_ behave like this.
     *
     * Anyway, if this fails, the message is in the folder, so
     * all what happens is that a concurrently running neomutt will
     * lose flag modifications.  */

    if ((m->type == MUTT_MH) && (rc == 0))
    {
      char newpath[PATH_MAX];
      snprintf(newpath, sizeof(newpath), "%s/%s", mailbox_path(m), e->path);
      rc = mutt_file_safe_rename(newpath, oldpath);
      if (rc == 0)
        mutt_str_replace(&e->path, partpath);
    }
  }
  else
    mx_msg_close(m, &dest);

  if ((rc == -1) && restore)
  {
    e->content->offset = old_body_offset;
    e->content->length = old_body_length;
    e->lines = old_hdr_lines;
  }

  mutt_body_free(&e->content->parts);
  return rc;
}

/**
 * maildir_canon_filename - Generate the canonical filename for a Maildir folder
 * @param dest   Buffer for the result
 * @param src    Buffer containing source filename
 *
 * @note         maildir filename is defined as: \<base filename\>:2,\<flags\>
 *               but \<base filename\> may contain additional comma separated
 *               fields.
 */
void maildir_canon_filename(struct Buffer *dest, const char *src)
{
  if (!dest || !src)
    return;

  char *t = strrchr(src, '/');
  if (t)
    src = t + 1;

  mutt_buffer_strcpy(dest, src);
  char *u = strpbrk(dest->data, ",:");
  if (u)
  {
    *u = '\0';
    dest->dptr = u;
  }
}

/**
 * md_open_find_message - Find a message in a maildir folder
 * @param[in]  folder    Base folder
 * @param[in]  unique    Unique part of filename
 * @param[in]  subfolder Subfolder to search, e.g. 'cur'
 * @param[out] newname   File's new name
 * @retval ptr File handle
 *
 * These functions try to find a message in a maildir folder when it
 * has moved under our feet.  Note that this code is rather expensive, but
 * then again, it's called rarely.
 */
static FILE *md_open_find_message(const char *folder, const char *unique,
                                  const char *subfolder, char **newname)
{
  struct Buffer *dir = mutt_buffer_pool_get();
  struct Buffer *tunique = mutt_buffer_pool_get();
  struct Buffer *fname = mutt_buffer_pool_get();

  struct dirent *de = NULL;

  FILE *fp = NULL;
  int oe = ENOENT;

  mutt_buffer_printf(dir, "%s/%s", folder, subfolder);

  DIR *dp = opendir(mutt_b2s(dir));
  if (!dp)
  {
    errno = ENOENT;
    goto cleanup;
  }

  while ((de = readdir(dp)))
  {
    maildir_canon_filename(tunique, de->d_name);

    if (mutt_str_equal(mutt_b2s(tunique), unique))
    {
      mutt_buffer_printf(fname, "%s/%s/%s", folder, subfolder, de->d_name);
      fp = fopen(mutt_b2s(fname), "r");
      oe = errno;
      break;
    }
  }

  closedir(dp);

  if (newname && fp)
    *newname = mutt_buffer_strdup(fname);

  errno = oe;

cleanup:
  mutt_buffer_pool_release(&dir);
  mutt_buffer_pool_release(&tunique);
  mutt_buffer_pool_release(&fname);

  return fp;
}

/**
 * maildir_parse_flags - Parse Maildir file flags
 * @param e    Email
 * @param path Path to email file
 */
void maildir_parse_flags(struct Email *e, const char *path)
{
  char *q = NULL;

  e->flagged = false;
  e->read = false;
  e->replied = false;

  char *p = strrchr(path, ':');
  if (p && mutt_str_startswith(p + 1, "2,"))
  {
    p += 3;

    mutt_str_replace(&e->maildir_flags, p);
    q = e->maildir_flags;

    while (*p)
    {
      switch (*p)
      {
        case 'F':
          e->flagged = true;
          break;

        case 'R': /* replied */
          e->replied = true;
          break;

        case 'S': /* seen */
          e->read = true;
          break;

        case 'T': /* trashed */
          if (!e->flagged || !C_FlagSafe)
          {
            e->trash = true;
            e->deleted = true;
          }
          break;

        default:
          *q++ = *p;
          break;
      }
      p++;
    }
  }

  if (q == e->maildir_flags)
    FREE(&e->maildir_flags);
  else if (q)
    *q = '\0';
}

/**
 * maildir_parse_stream - Parse a Maildir message
 * @param type   Mailbox type, e.g. #MUTT_MAILDIR
 * @param fp     Message file handle
 * @param fname  Message filename
 * @param is_old true, if the email is old (read)
 * @param e      Email
 * @retval ptr Populated Email
 *
 * Actually parse a maildir message.  This may also be used to fill
 * out a fake header structure generated by lazy maildir parsing.
 */
struct Email *maildir_parse_stream(enum MailboxType type, FILE *fp,
                                   const char *fname, bool is_old, struct Email *e)
{
  if (!e)
    e = email_new();
  e->env = mutt_rfc822_read_header(fp, e, false, false);

  struct stat st;
  fstat(fileno(fp), &st);

  if (!e->received)
    e->received = e->date_sent;

  /* always update the length since we have fresh information available. */
  e->content->length = st.st_size - e->content->offset;

  e->index = -1;

  if (type == MUTT_MAILDIR)
  {
    /* maildir stores its flags in the filename, so ignore the
     * flags in the header of the message */

    e->old = is_old;
    maildir_parse_flags(e, fname);
  }
  return e;
}

/**
 * maildir_parse_message - Actually parse a maildir message
 * @param type   Mailbox type, e.g. #MUTT_MAILDIR
 * @param fname  Message filename
 * @param is_old true, if the email is old (read)
 * @param e      Email to populate (OPTIONAL)
 * @retval ptr Populated Email
 *
 * This may also be used to fill out a fake header structure generated by lazy
 * maildir parsing.
 */
struct Email *maildir_parse_message(enum MailboxType type, const char *fname,
                                    bool is_old, struct Email *e)
{
  FILE *fp = fopen(fname, "r");
  if (!fp)
    return NULL;

  e = maildir_parse_stream(type, fp, fname, is_old, e);
  mutt_file_fclose(&fp);
  return e;
}

/**
 * mh_sync_mailbox_message - Save changes to the mailbox
 * @param m     Mailbox
 * @param msgno Index number
 * @param hc    Header cache handle
 * @retval  0 Success
 * @retval -1 Error
 */
int mh_sync_mailbox_message(struct Mailbox *m, int msgno, header_cache_t *hc)
{
  if (!m || !m->emails || (msgno >= m->msg_count))
    return -1;

  struct Email *e = m->emails[msgno];
  if (!e)
    return -1;

  if (e->deleted && ((m->type != MUTT_MAILDIR) || !C_MaildirTrash))
  {
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/%s", mailbox_path(m), e->path);
    if ((m->type == MUTT_MAILDIR) || (C_MhPurge && (m->type == MUTT_MH)))
    {
#ifdef USE_HCACHE
      if (hc)
      {
        const char *key = NULL;
        size_t keylen;
        if (m->type == MUTT_MH)
        {
          key = e->path;
          keylen = strlen(key);
        }
        else
        {
          key = e->path + 3;
          keylen = maildir_hcache_keylen(key);
        }
        mutt_hcache_delete_header(hc, key, keylen);
      }
#endif
      unlink(path);
    }
    else if (m->type == MUTT_MH)
    {
      /* MH just moves files out of the way when you delete them */
      if (*e->path != ',')
      {
        char tmp[PATH_MAX];
        snprintf(tmp, sizeof(tmp), "%s/,%s", mailbox_path(m), e->path);
        unlink(tmp);
        rename(path, tmp);
      }
    }
  }
  else if (e->changed || e->attach_del ||
           ((m->type == MUTT_MAILDIR) && (C_MaildirTrash || e->trash) &&
            (e->deleted != e->trash)))
  {
    if (m->type == MUTT_MAILDIR)
    {
      if (maildir_sync_message(m, msgno) == -1)
        return -1;
    }
    else
    {
      if (mh_sync_message(m, msgno) == -1)
        return -1;
    }
  }

#ifdef USE_HCACHE
  if (hc && e->changed)
  {
    const char *key = NULL;
    size_t keylen;
    if (m->type == MUTT_MH)
    {
      key = e->path;
      keylen = strlen(key);
    }
    else
    {
      key = e->path + 3;
      keylen = maildir_hcache_keylen(key);
    }
    mutt_hcache_store(hc, key, keylen, e, 0);
  }
#endif

  return 0;
}

/**
 * maildir_update_flags - Update the mailbox flags
 * @param m     Mailbox
 * @param e_old Old Email
 * @param e_new New Email
 * @retval true  If the flags changed
 * @retval false Otherwise
 */
bool maildir_update_flags(struct Mailbox *m, struct Email *e_old, struct Email *e_new)
{
  if (!m)
    return false;

  /* save the global state here so we can reset it at the
   * end of list block if required.  */
  bool context_changed = m->changed;

  /* user didn't modify this message.  alter the flags to match the
   * current state on disk.  This may not actually do
   * anything. mutt_set_flag() will just ignore the call if the status
   * bits are already properly set, but it is still faster not to pass
   * through it */
  if (e_old->flagged != e_new->flagged)
    mutt_set_flag(m, e_old, MUTT_FLAG, e_new->flagged);
  if (e_old->replied != e_new->replied)
    mutt_set_flag(m, e_old, MUTT_REPLIED, e_new->replied);
  if (e_old->read != e_new->read)
    mutt_set_flag(m, e_old, MUTT_READ, e_new->read);
  if (e_old->old != e_new->old)
    mutt_set_flag(m, e_old, MUTT_OLD, e_new->old);

  /* mutt_set_flag() will set this, but we don't need to
   * sync the changes we made because we just updated the
   * context to match the current on-disk state of the
   * message.  */
  bool header_changed = e_old->changed;
  e_old->changed = false;

  /* if the mailbox was not modified before we made these
   * changes, unset the changed flag since nothing needs to
   * be synchronized.  */
  if (!context_changed)
    m->changed = false;

  return header_changed;
}

/**
 * maildir_open_find_message - Find a new
 * @param[in]  folder  Maildir path
 * @param[in]  msg     Email path
 * @param[out] newname New name, if it has moved
 * @retval ptr File handle
 */
FILE *maildir_open_find_message(const char *folder, const char *msg, char **newname)
{
  static unsigned int new_hits = 0, cur_hits = 0; /* simple dynamic optimization */

  struct Buffer *unique = mutt_buffer_pool_get();
  maildir_canon_filename(unique, msg);

  FILE *fp = md_open_find_message(folder, mutt_b2s(unique),
                                  (new_hits > cur_hits) ? "new" : "cur", newname);
  if (fp || (errno != ENOENT))
  {
    if ((new_hits < UINT_MAX) && (cur_hits < UINT_MAX))
    {
      new_hits += ((new_hits > cur_hits) ? 1 : 0);
      cur_hits += ((new_hits > cur_hits) ? 0 : 1);
    }

    goto cleanup;
  }
  fp = md_open_find_message(folder, mutt_b2s(unique),
                            (new_hits > cur_hits) ? "cur" : "new", newname);
  if (fp || (errno != ENOENT))
  {
    if ((new_hits < UINT_MAX) && (cur_hits < UINT_MAX))
    {
      new_hits += ((new_hits > cur_hits) ? 0 : 1);
      cur_hits += ((new_hits > cur_hits) ? 1 : 0);
    }

    goto cleanup;
  }

  fp = NULL;

cleanup:
  mutt_buffer_pool_release(&unique);

  return fp;
}

/**
 * maildir_mh_open_message - Open a Maildir or MH message
 * @param m          Mailbox
 * @param msg        Message to open
 * @param msgno      Index number
 * @param is_maildir true, if a Maildir
 * @retval  0 Success
 * @retval -1 Failure
 */
int maildir_mh_open_message(struct Mailbox *m, struct Message *msg, int msgno, bool is_maildir)
{
  if (!m || !m->emails || (msgno >= m->msg_count))
    return -1;

  struct Email *e = m->emails[msgno];
  if (!e)
    return -1;

  char path[PATH_MAX];

  snprintf(path, sizeof(path), "%s/%s", mailbox_path(m), e->path);

  msg->fp = fopen(path, "r");
  if (!msg->fp && (errno == ENOENT) && is_maildir)
    msg->fp = maildir_open_find_message(mailbox_path(m), e->path, NULL);

  if (!msg->fp)
  {
    mutt_perror(path);
    mutt_debug(LL_DEBUG1, "fopen: %s: %s (errno %d)\n", path, strerror(errno), errno);
    return -1;
  }

  return 0;
}

/**
 * maildir_check_empty - Is the mailbox empty
 * @param path Mailbox to check
 * @retval 1 Mailbox is empty
 * @retval 0 Mailbox contains mail
 * @retval -1 Error
 */
int maildir_check_empty(const char *path)
{
  DIR *dp = NULL;
  struct dirent *de = NULL;
  int rc = 1; /* assume empty until we find a message */
  char realpath[PATH_MAX];
  int iter = 0;

  /* Strategy here is to look for any file not beginning with a period */

  do
  {
    /* we do "cur" on the first iteration since it's more likely that we'll
     * find old messages without having to scan both subdirs */
    snprintf(realpath, sizeof(realpath), "%s/%s", path, (iter == 0) ? "cur" : "new");
    dp = opendir(realpath);
    if (!dp)
      return -1;
    while ((de = readdir(dp)))
    {
      if (*de->d_name != '.')
      {
        rc = 0;
        break;
      }
    }
    closedir(dp);
    iter++;
  } while (rc && iter < 2);

  return rc;
}

/**
 * mh_check_empty - Is mailbox empty
 * @param path Mailbox to check
 * @retval 1 Mailbox is empty
 * @retval 0 Mailbox contains mail
 * @retval -1 Error
 */
int mh_check_empty(const char *path)
{
  struct dirent *de = NULL;
  int rc = 1; /* assume empty until we find a message */

  DIR *dp = opendir(path);
  if (!dp)
    return -1;
  while ((de = readdir(dp)))
  {
    if (mh_valid_message(de->d_name))
    {
      rc = 0;
      break;
    }
  }
  closedir(dp);

  return rc;
}

/**
 * maildir_ac_find - Find an Account that matches a Mailbox path - Implements MxOps::ac_find()
 */
struct Account *maildir_ac_find(struct Account *a, const char *path)
{
  if (!a || !path)
    return NULL;

  return a;
}

/**
 * maildir_ac_add - Add a Mailbox to an Account - Implements MxOps::ac_add()
 */
int maildir_ac_add(struct Account *a, struct Mailbox *m)
{
  if (!a || !m || ((m->type != MUTT_MAILDIR) && (m->type != MUTT_MH)))
    return -1;
  return 0;
}

/**
 * maildir_path_canon - Canonicalise a Mailbox path - Implements MxOps::path_canon()
 */
int maildir_path_canon(char *buf, size_t buflen)
{
  if (!buf)
    return -1;

  mutt_path_canon(buf, buflen, HomeDir, true);
  return 0;
}

/**
 * maildir_path_pretty - Abbreviate a Mailbox path - Implements MxOps::path_pretty()
 */
int maildir_path_pretty(char *buf, size_t buflen, const char *folder)
{
  if (!buf)
    return -1;

  if (mutt_path_abbr_folder(buf, buflen, folder))
    return 0;

  if (mutt_path_pretty(buf, buflen, HomeDir, false))
    return 0;

  return -1;
}

/**
 * maildir_path_parent - Find the parent of a Mailbox path - Implements MxOps::path_parent()
 */
int maildir_path_parent(char *buf, size_t buflen)
{
  if (!buf)
    return -1;

  if (mutt_path_parent(buf, buflen))
    return 0;

  if (buf[0] == '~')
    mutt_path_canon(buf, buflen, HomeDir, true);

  if (mutt_path_parent(buf, buflen))
    return 0;

  return -1;
}

/**
 * mh_mbox_sync - Save changes to the Mailbox - Implements MxOps::mbox_sync()
 * @retval #MUTT_REOPENED  mailbox has been externally modified
 * @retval #MUTT_NEW_MAIL  new mail has arrived
 * @retval  0 Success
 * @retval -1 Error
 *
 * @note The flag retvals come from a call to a backend sync function
 */
int mh_mbox_sync(struct Mailbox *m)
{
  if (!m)
    return -1;

  int i, j;
  header_cache_t *hc = NULL;
  struct Progress progress;
  int check;

  if (m->type == MUTT_MH)
    check = mh_mbox_check(m);
  else
    check = maildir_mbox_check(m);

  if (check < 0)
    return check;

#ifdef USE_HCACHE
  if ((m->type == MUTT_MAILDIR) || (m->type == MUTT_MH))
    hc = mutt_hcache_open(C_HeaderCache, mailbox_path(m), NULL);
#endif

  if (m->verbose)
  {
    char msg[PATH_MAX];
    snprintf(msg, sizeof(msg), _("Writing %s..."), mailbox_path(m));
    mutt_progress_init(&progress, msg, MUTT_PROGRESS_WRITE, m->msg_count);
  }

  for (i = 0; i < m->msg_count; i++)
  {
    if (m->verbose)
      mutt_progress_update(&progress, i, -1);

    if (mh_sync_mailbox_message(m, i, hc) == -1)
      goto err;
  }

#ifdef USE_HCACHE
  if ((m->type == MUTT_MAILDIR) || (m->type == MUTT_MH))
    mutt_hcache_close(hc);
#endif

  if (m->type == MUTT_MH)
    mh_update_sequences(m);

  /* XXX race condition? */

  maildir_update_mtime(m);

  /* adjust indices */

  if (m->msg_deleted)
  {
    for (i = 0, j = 0; i < m->msg_count; i++)
    {
      struct Email *e = m->emails[i];
      if (!e)
        break;

      if (!e->deleted || ((m->type == MUTT_MAILDIR) && C_MaildirTrash))
        e->index = j++;
    }
  }

  return check;

err:
#ifdef USE_HCACHE
  if ((m->type == MUTT_MAILDIR) || (m->type == MUTT_MH))
    mutt_hcache_close(hc);
#endif
  return -1;
}

/**
 * mh_mbox_close - Close a Mailbox - Implements MxOps::mbox_close()
 * @retval 0 Always
 */
int mh_mbox_close(struct Mailbox *m)
{
  return 0;
}

/**
 * mh_msg_close - Close an email - Implements MxOps::msg_close()
 *
 * @note May also return EOF Failure, see errno
 */
int mh_msg_close(struct Mailbox *m, struct Message *msg)
{
  return mutt_file_fclose(&msg->fp);
}

/**
 * mh_msg_save_hcache - Save message to the header cache - Implements MxOps::msg_save_hcache()
 */
int mh_msg_save_hcache(struct Mailbox *m, struct Email *e)
{
  int rc = 0;
#ifdef USE_HCACHE
  header_cache_t *hc = mutt_hcache_open(C_HeaderCache, mailbox_path(m), NULL);
  rc = mutt_hcache_store(hc, e->path, strlen(e->path), e, 0);
  mutt_hcache_close(hc);
#endif
  return rc;
}
