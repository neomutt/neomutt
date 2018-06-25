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

#include "config.h"
#include <stddef.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include "mutt/mutt.h"
#include "config/lib.h"
#include "email/lib.h"
#include "help.h"
#include "account.h"
#include "address/lib.h"
#include "context.h"
#include "globals.h"
#include "mailbox.h"
#include "mutt_header.h"
#include "mutt_options.h"
#include "mutt_thread.h"
#include "muttlib.h"
#include "mx.h"
#include "protos.h"

#define HELP_CACHE_DOCLIST                                                     \
  1 /* whether to cache the DocList between help_mbox_open calls */
#define HELP_FHDR_MAXLINES                                                     \
  -1 /* max help file header lines to read (N < 0 means all) */
#define HELP_LINK_CHAPTERS                                                     \
  0 /* whether to link all help chapter upwards to root box */

static bool __Backup_HTS; /* used to restore $hide_thread_subject on help_mbox_close() */
static char DocDirID[33]; /* MD5 checksum of current $help_doc_dir DT_PATH option */
static HelpList *DocList; /* all valid help documents within $help_doc_dir folder */
static size_t UpLink = 0; /* DocList index, used to uplink a parent thread target */

/**
 * help_list_XXX - Several (helper) functions for a generic list object (HelpList)
 */
static void help_list_free(HelpList **list, void (*item_free)(void **))
{
  if (!list || !*list)
    return;

  for (size_t i = 0; i < (*list)->size; i++)
  {
    item_free(&((*list)->data[i]));
  }
  FREE(&(*list)->data);
  FREE(list);
}

static void help_list_shrink(HelpList *list)
{
  if (!list)
    return;

  mutt_mem_realloc(list->data, list->size * list->item_size);
  list->capa = list->size;
}

static HelpList *help_list_new(size_t item_size)
{
  HelpList *list = NULL;
  if ((item_size == 0) || ((list = mutt_mem_malloc(sizeof(HelpList))) == NULL))
    return NULL;

  list->item_size = item_size;
  list->size = 0;
  list->capa = HELPLIST_INIT_CAPACITY;
  list->data = mutt_mem_calloc(list->capa, sizeof(void *) * list->item_size);
  if (!list->data)
  {
    FREE(list);
    list = NULL;
  }

  return list;
}

static void help_list_append(HelpList *list, void *item)
{
  if (!list || !item)
    return;

  if (list->size >= list->capa)
  {
    list->capa = (list->capa == 0) ? HELPLIST_INIT_CAPACITY : (list->capa * 2);
    mutt_mem_realloc(list->data, list->capa * list->item_size);
  }

  list->data[list->size] = mutt_mem_calloc(1, list->item_size);
  list->data[list->size] = item;
  list->size++;
}

static void help_list_new_append(HelpList **list, size_t item_size, void *item)
{
  if ((item_size == 0) || !item)
    return;

  if (!list || !*list)
    *list = help_list_new(item_size);

  help_list_append(*list, item);
}

static void *help_list_get(HelpList *list, size_t index, void *(*copy)(const void *) )
{
  if (!list || (index >= list->size))
    return NULL;

  return ((copy) ? copy(list->data[index]) : list->data[index]);
}

static HelpList *help_list_clone(HelpList *list, bool shrink, void *(*copy)(const void *) )
{
  if (!list)
    return NULL;

  HelpList *clone = help_list_new(list->item_size);
  for (size_t i = 0; i < list->size; i++)
    help_list_append(clone, help_list_get(list, i, copy));

  if (shrink)
    help_list_shrink(clone);

  return clone;
}

static void help_list_sort(HelpList *list, int (*compare)(const void *, const void *))
{
  if (!list)
    return;

  qsort(list->data, list->size, sizeof(void *), compare);
}

/**
 * help_doc_type_cmp - Callback to compare help document types in a generic list
 *                     used to sort an 'index.md' element to the top of the list
 * @param   a       ... generic list element pointer to a Header object
 * @param   b       ... generic list element pointer to a Header object
 * @retval  -1      ... related to the type, object a is less than object b
 * @retval   0      ... related to the type, object a is equal object b
 * @retval   1      ... related to the type, object a is greater than object b
 */
static int help_doc_type_cmp(const void *a, const void *b)
{
  const struct Email *e1 = *(const struct Email **) a;
  const struct Email *e2 = *(const struct Email **) b;
  const HDType t1 = ((const HDMeta *) e1->edata)->type;
  const HDType t2 = ((const HDMeta *) e2->edata)->type;

  return ((t1 < t2) - (t1 > t2));
}

/**
 * help_file_hdr_free - Callback to release used memory of a file header
 * @param   item    ... generic list element pointer to a file header
 */
static void help_file_hdr_free(void **item)
{
  HFHeader *fhdr = ((HFHeader *) *item);

  FREE(&fhdr->key);
  FREE(&fhdr->val);
  FREE(&fhdr);
}

/**
 * help_doc_meta_free - Callback to release used memory of help doc metadata
 * @param   hdr     ... Header struct that holds the metadata
 *
 * @note: Called by mutt_header_free() to free custom metadata in Context::data.
 */
static void help_doc_meta_free(void **data)
{
  if (!data || !*data)
    return;

  HDMeta *meta = *data;

  FREE(&meta->name);
  help_list_free(&meta->fhdr, help_file_hdr_free);
  *data = NULL;
}

/**
 * help_doc_free - Callback to release used memory of a DocList element
 * @param   item    ... generic list element pointer to a DocList element
 */
static void help_doc_free(void **item)
{
  struct Email *hdoc = ((struct Email *) *item);

  mutt_email_free(&hdoc);
  FREE(hdoc);
}

/**
 * help_doclist_free - Callback to release used memory of the DocList
 */
void help_doclist_free(void)
{
  help_list_free(&DocList, help_doc_free);
  mutt_str_strfcpy(DocDirID, "", sizeof(DocDirID));
  UpLink = 0;
}

/**
 * help_checksum_md5 - Calculate a string MD5 checksum and store it in a buffer
 * @param   string  ... to hash
 * @param   digest  ... buffer for storing the calculated hash
 *
 * @note: The digest buffer _must_ be at least 33 bytes long (requirement by
 *        mutt_md5_toascii() function).
 */
static void help_checksum_md5(const char *string, char *digest)
{
  unsigned char md5[16];

  mutt_md5(NONULL(string), md5);
  mutt_md5_toascii(md5, digest);
}

/**
 * help_docdir_id - Get (and probably previously set) current DocDirID
 * @param   docdir  ... (optional) path to set the DocDirID from
 * @retval  pointer ... to current DocDirID MD5 checksum string
 */
static char *help_docdir_id(const char *docdir)
{
  if (docdir && DocList) /* only set ID if DocList != NULL */
    help_checksum_md5(docdir, DocDirID);

  return DocDirID;
}

/**
 * help_docdir_changed - Determine if $help_doc_dir differs from previous run
 * @retval  true    ... $help_doc_dir path differs (DocList rebuilt recommended)
 * @retval  false   ... same $help_doc_dir path where DocList was built from
 */
static bool help_docdir_changed(void)
{
  char digest[33];
  help_checksum_md5(C_HelpDocDir, digest);

  return (mutt_str_strcmp(DocDirID, digest) != 0);
}

/**
 * help_dirent_type - Get the type of a given dirent entry or its file path
 * @param   item    ... dirent struct that probably holds the wanted entry type
 * @param   path    ... alternatively used with stat() to determine its type
 * @param   as_flag ... return type as d_type value or its representing flag
 * @retval  type    ... obtained type or DT_UNKNOWN (0) otherwise
 *
 * @note: On systems that define macro _DIRENT_HAVE_D_TYPE and supports a d_type
 *        field, this function may be less costly than an extra call of stat().
 */
static DEType help_dirent_type(const struct dirent *item, const char *path, bool as_flag)
{
  unsigned char type = 0;

#ifdef _DIRENT_HAVE_D_TYPE
  type = item->d_type;
#else
  struct stat sb;

  if (stat(path, &sb) == 0)
    type = ((sb.st_mode & 0170000) >> 12);
#endif

  return (as_flag ? DT2DET(type) : type);
}

/**
 * help_file_type - Determine the type of a help file (relative to #C_HelpDocDir)
 * @param   file    ... as full qualified file path of the document to test
 * @retval  type    ... of the file/document (as enum helpdoc_type)
 *
 * @note: The type of a file is determined only from its path string, so it does
 *        not need to exist. That means also, a file can have a proper type, but
 *        the document itself may be invalid (and discarded later by a filter).
 */
static HDType help_file_type(const char *file)
{
  HDType type = HDT_NONE;
  const size_t l = mutt_str_strlen(file);
  const size_t m = mutt_str_strlen(C_HelpDocDir);

  if ((l < 5) || (m == 0) || (l <= m))
    return type; /* relative subpath requirements doesn't match the minimum */

  const char *p = (file + m);
  const char *q = (file + l - 3);

  if ((mutt_str_strncasecmp(q, ".md", 3) != 0) ||
      (mutt_str_strncmp(file, C_HelpDocDir, m) != 0))
    return type; /* path below C_HelpDocDir and ".md" extension are mandatory */

  if (mutt_str_strcasecmp(q = strrchr(p, '/'), "/index.md") == 0)
    type = HDT_INDEX; /* help document is a special named ("index.md") file */
  else
    type = 0;

  if (p == q)
    type |= HDT_ROOTDOC; /* help document lives directly in C_HelpDocDir root */
  else if ((p = strchr(p + 1, '/')) == q)
    type |= HDT_CHAPTER;
  else /* handle all remaining (deeper nested) help documents as a section */
    type |= HDT_SECTION;

  return type;
}

/**
 * help_file_header - Process and extract a YAML header of a potential help file
 * @param   fhdr    ... list where to store the final header information
 * @param   file    ... path to the (potential help document) file to parse
 * @param   max     ... how many header lines to read (N < 0 means all)
 * @retval   N >= 0 ... success, N valid header lines read from file
 * @retval  -1      ... file isn't a helpdoc: extension doesn't match ".md"
 * @retval  -2      ... file header not read: file cannot be open for read
 * @retval  -3      ... found invalid header: no triple-dashed start mark
 * @retval  -4      ... found invalid header: no triple-dashed end mark
 */
static int help_file_header(HelpList **fhdr, const char *file, int max)
{
  const char *bfn = mutt_path_basename(NONULL(file));
  const char *ext = strrchr(bfn, '.');
  if (!bfn || ext == bfn || mutt_str_strncasecmp(ext, ".md", 3) != 0)
    return -1;

  FILE *fp = mutt_file_fopen(file, "r");
  if (!fp)
    return -2;

  int lineno = 0;
  size_t linelen;
  const char *mark = "---";

  char *p = mutt_file_read_line(NULL, &linelen, fp, &lineno, 0);
  if (mutt_str_strcmp(p, mark) != 0)
  {
    mutt_file_fclose(&fp);
    return -3;
  }

  HelpList *list = NULL;
  char *q = NULL;
  bool endmark = false;
  int count = 0, limit = ((max < 0) ? -1 : max);

  while ((mutt_file_read_line(p, &linelen, fp, &lineno, 0) != NULL) &&
         !(endmark = (mutt_str_strcmp(p, mark) == 0)) && ((q = strpbrk(p, ": \t")) != NULL))
  {
    if (limit == 0)
      continue; /* to find the end mark that qualify the header as valid */
    else if ((p == q) || (*q != ':'))
      continue; /* to skip wrong keyworded lines, XXX: or should we abort? */

    mutt_str_remove_trailing_ws(p);
    HFHeader *item = mutt_mem_calloc(1, sizeof(HFHeader));
    item->key = mutt_str_substr_dup(p, q);
    item->val = mutt_str_strdup(mutt_str_skip_whitespace(NONULL(++q)));
    help_list_new_append(&list, sizeof(HFHeader), item);

    count++;
    limit--;
  }
  mutt_file_fclose(&fp);
  FREE(&p);

  if (!endmark)
  {
    help_list_free(&list, help_file_hdr_free);
    count = -4;
  }
  else
  {
    help_list_shrink(list);
    *fhdr = list;
  }

  return count;
}

/**
 * help_file_hdr_find - Find a help document header line by its key(word)
 * @param   key     ... string to search for in fhdr list (case-sensitive)
 * @param   fhdr    ... list of HFHeader elements to search for key
 * @retval  header  ... on success, struct containing the found key
 * @retval  NULL    ... on failure, or when key could not be found
 */
static HFHeader *help_file_hdr_find(const char *key, const HelpList *fhdr)
{
  if (!fhdr || !key || !*key)
    return NULL;

  HFHeader *hdr = NULL;
  for (size_t i = 0; i < fhdr->size; i++)
  {
    if (mutt_str_strcmp(((HFHeader *) fhdr->data[i])->key, key) != 0)
      continue;

    hdr = fhdr->data[i];
    break;
  }

  return hdr;
}

/**
 * help_doc_msg_id - Return a simple message ID
 * @param   tm      ... struct (time.h(0p)) used for date part in the message ID
 * @retval  string  ... with generated message ID
 */
static char *help_doc_msg_id(const struct tm *tm)
{
  char buf[128];
  unsigned char rndid[MUTT_RANDTAG_LEN + 1];
  mutt_rand_base32(rndid, sizeof(rndid) - 1);
  rndid[MUTT_RANDTAG_LEN] = 0;

  snprintf(buf, sizeof(buf), "<%d%02d%02d%02d%02d%02d.%s>", tm->tm_year + 1900,
           tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec, rndid);

  return mutt_str_strdup(buf);
}

/**
 * help_doc_subject - Build a message subject, based on a string format and
 *                    from the keyword value(s) of help file header line(s)
 * @param   fhdr    ... list of HFHeader elements to search for key
 * @param   defsubj ... this given default subject will be used on failures
 * @param   strfmt  ... the string format to use for the subject line (only
 *                      string conversion placeholder ("%s") are supported)
 * @param   key     ... a header line keyword whose value will be used at the
 *                      related strfmt placeholder position
 * @param   ...     ... additional keywords to find (not limited in count)
 * @retval  string  ... on success, holds the built subject or defsubj otherwise
 *
 * @note: Whether length, flag nor precision specifier currently supported, only
 *        poor "%s" placeholder will be recognised.
 * XXX: - Should the length of the subject line be limited by an additional
 *        parameter? Currently an internal limit of STRING (256) is used.
 *      - This function use variable argument list (va_list), that may provide
 *        more than the real specified arguments and there seems to be no way to
 *        stop processing after the last real parameter. Therefore, the last
 *        parameter MUST be NULL currently or weird things happens when count of
 *        placeholder in strfmt and specified keys differs.
 */
static char *help_doc_subject(HelpList *fhdr, const char *defaultsubject,
                              const char *strfmt, const char *key, ...)
{
  va_list ap;
  char subject[256]; /* XXX: Should trailing white space (from strfmt) be removed? */

  HFHeader *hdr = NULL;
  const char *f = NULL;
  const char *p = strfmt;
  const char *q = NULL;
  size_t l = 0;
  int size = 0;

  va_start(ap, key);
  while ((q = strstr(p, "%s")) && key)
  {
    if ((hdr = help_file_hdr_find(key, fhdr)) == NULL)
    {
      mutt_str_strfcpy(subject, defaultsubject, sizeof(subject));
      break;
    }

    f = mutt_str_substr_dup(p, strstr(q + 2, "%s"));
    size = snprintf(subject + l, sizeof(subject) - l, f, hdr->val);

    if (size < 0)
    {
      mutt_str_strfcpy(subject, defaultsubject, sizeof(subject));
      break;
    }

    p += mutt_str_strlen(f);
    l += size;
    key = va_arg(ap, const char *);
  }
  va_end(ap);

  return mutt_str_strdup(subject);
}

/**
 * help_path_transpose - Convert (vice versa) a scheme prefixed into a file path
 * @param   path    ... to transpose (starts with "help[://]" or $help_doc_dir)
 * @param   validate... specifies the strictness, if true, file path must exist
 * @retval  string  ... on success, contains the transposed contrary path
 * @retval  NULL    ... on failure, path may not exist or was invalid
 *
 * @note: The resulting path is sanitised, which means any trailing slash(es) of
 *        the input path are stripped.
 */
static char *help_path_transpose(const char *path, bool validate)
{
  if (!path || !*path)
    return NULL;

  char fqp[PATH_MAX];
  char url[PATH_MAX];

  const char *docdir = C_HelpDocDir;
  const char *scheme = "help"; /* TODO: don't hardcode, use sth. like: UrlMap[U_HELP]->name */
  const char *result = NULL;
  size_t i = 0;
  size_t j = 0;

  if (mutt_str_strncasecmp(path, scheme, (j = mutt_str_strlen(scheme))) == 0)
  { /* unlike url_check_scheme(), allow scheme alone (without a separator) */
    if ((path[j] != ':') && (path[j] != '\0'))
      return NULL;
    else if (path[j] == ':')
      j += 1;

    result = fqp;
    i = mutt_str_strlen(docdir);
  }
  else if (mutt_str_strncmp(path, docdir, (j = mutt_str_strlen(docdir))) == 0)
  {
    if ((path[j] != '/') && (path[j] != '\0'))
      return NULL;

    result = url;
    i = mutt_str_strlen(scheme) + 3;
  }
  else
  {
    return NULL;
  }

  j += strspn(path + j, "/");
  snprintf(fqp, sizeof(fqp), "%s/%s", docdir, path + j);
  snprintf(url, sizeof(url), "%s://%s", scheme, path + j);
  j = mutt_str_strlen(result);

  while ((i < j) && (result[j - 1] == '/'))
    j--;

  return (validate && !realpath(fqp, NULL)) ? NULL : strndup(result, j);
}

/**
 * help_dirent_filter - Prefilter callback function for help_dir_scan()
 * @param   de      ... dirent strut, that can be filtered
 * @param   path    ... dirent related file path, that can be filtered
 * @param   type    ... help file type, that can be filtered
 *
 * @retval  N > 0   ... implies a success, force help_dir_scan() to continue (without recursion)
 * @retval      0   ... implies a success, causes help_dir_scan() to call its item handler
 * @retval  N < 0   ... implies a failure, will cause help_dir_scan() to abort
 *
 * @note: This function is currently not used.
 *
static int help_dirent_filter(const struct dirent *de, const char *path,
                              const DEType type)
{
  if (type != DET_REG)
    return 0;

  return (help_file_type(path) == HDT_NONE);
}
*/

/**
 * help_dir_scan - Traverse a directory for specific entry types, filter/gather
 *                 the found result(s)
 * @param   path    ... directory in which to start the investigation
 * @param   recursive ... Whether or not to iterate the given path recursively
 * @param   mask    ... a bit mask that defines which entry type(s) to filter
 * @param   filter  ... (optional) preselection filter callback function
 * @param   gather  ... handler callback function for a single result that prior
 *                      passed the bit mask and preselection filter
 * @param   items   ... list that can be used to interchange all results between
 *                      caller and handler function
 * @retval   0      ... on success, execution ended normally
 * @retval  -1      ... on failure, when path cannot be opened for reading
 *
 * @note: Function only aborts an iteration when the end of directory stream has
 *        been reached or the callback filter function returns with N < 0, but
 *        not on failures, to grab as much as possible entries from the stream.
 *        An entry named "", "." or "..", will always be skipped (and thus never
 *        delegated to callback functions), but will be solved/expanded for the
 *        initial path parameter.
 */
static int help_dir_scan(const char *path, bool recursive, const DETMask mask,
                         int (*filter)(const struct dirent *, const char *, DEType),
                         int (*gather)(HelpList **, const char *), HelpList **items)
{
  char curpath[PATH_MAX];

  errno = 0;
  DIR *dp = opendir(NONULL(realpath(path, curpath)));

  if (!dp)
  {
    //mutt_debug(1, "unable to open dir '%s': %s (errno %d).\n", path, strerror(errno), errno);
    /* XXX: Fake a localised error message by extending an existing one */
    mutt_error("%s '%s': %s (errno %d).", _("Error opening mailbox"), path,
               strerror(errno), errno);
    return -1;
  }

  while (1)
  {
    errno = 0; /* reset errno to distinguish between end-of-(dir)stream and an error */
    const struct dirent *ep = readdir(dp);

    if (!ep)
    {
      if (!errno)
        break; /* we reached the end-of-stream */

      mutt_debug(1, "unable to read dir: %s (errno %d).\n", strerror(errno), errno);
      continue; /* this isn't the end-of-stream */
    }

    const char *np = ep->d_name;
    if (!np[0] || ((np[0] == '.') && (!np[1] || ((np[1] == '.') && !np[2]))))
    {
      continue; /* to skip "", ".", ".." entries */
    }

    char abspath[mutt_str_strlen(curpath) + mutt_str_strlen(np) + 2];
    mutt_path_concat(abspath, curpath, np, sizeof(abspath));

    const DEType flag = help_dirent_type(ep, abspath, true);
    if (mask & flag)
    { /* delegate preselection processing */
      int rc = filter ? filter(ep, abspath, flag) : 0;
      if (rc < 0)
        break; /* handler wants to abort */
      else if (0 < rc)
        continue; /* but skip a recursion */
      else
        gather(items, abspath);
    }

    if ((flag == DET_DIR) && recursive)
    { /* descend this directory recursive */
      help_dir_scan(abspath, recursive, mask, filter, gather, items);
    }
  }
  closedir(dp);

  return 0;
}

/**
 * help_file_hdr_clone - Callback to clone a file header object (HFHeader)
 * @param   item    ... list element pointer to the object to copy
 * @retval  pointer ... on success, to the duplicated object
 * @retval  NULL    ... on failure, otherwise
 */
static void *help_file_hdr_clone(const void *item)
{
  if (!item)
    return NULL;

  HFHeader *src = (HFHeader *) item;
  HFHeader *dup = mutt_mem_calloc(1, sizeof(HFHeader));

  dup->key = mutt_str_strdup(src->key);
  dup->val = mutt_str_strdup(src->val);

  return dup;
}

/**
 * help_doc_meta_clone - Callback to clone a help metadata object (HDMeta)
 * @param   item    ... list element pointer to the object to copy
 * @retval  pointer ... on success, to the duplicated object
 * @retval  NULL    ... on failure, otherwise
 */
static void *help_doc_meta_clone(const void *item)
{
  if (!item)
    return NULL;

  HDMeta *src = (HDMeta *) item;
  HDMeta *dup = mutt_mem_calloc(1, sizeof(HDMeta));

  dup->fhdr = help_list_clone(src->fhdr, true, help_file_hdr_clone);
  dup->name = mutt_str_strdup(src->name);
  dup->type = src->type;

  return dup;
}

/**
 * help_doc_clone - Callback to clone a help document object (Header)
 * @param   item    ... list element pointer to the object to copy
 * @retval  pointer ... on success, to the duplicated object
 * @retval  NULL    ... on failure, otherwise
 *
 * @note: This function should only duplicate statically defined attributes from
 *        a Header object that help_doc_from() build and return.
 */
static void *help_doc_clone(const void *item)
{
  if (!item)
    return NULL;

  struct Email *src = (struct Email *) item;
  struct Email *dup = mutt_email_new();
  /* struct Email */
  dup->date_sent = src->date_sent;
  dup->display_subject = src->display_subject;
  dup->index = src->index;
  dup->path = mutt_str_strdup(src->path);
  dup->read = src->read;
  dup->received = src->received;
  /* struct Email::data (custom metadata) */
  dup->edata = help_doc_meta_clone(src->edata);
  dup->free_edata = help_doc_meta_free;
  /* struct Body */
  dup->content = mutt_body_new();
  dup->content->disposition = src->content->disposition;
  dup->content->encoding = src->content->encoding;
  dup->content->length = src->content->length;
  dup->content->subtype = mutt_str_strdup(src->content->subtype);
  dup->content->type = src->content->type;
  /* struct Envelope */
  dup->env = mutt_env_new();
  mutt_addrlist_copy(&dup->env->from, &src->env->from, false);
  dup->env->message_id = mutt_str_strdup(src->env->message_id);
  dup->env->organization = mutt_str_strdup(src->env->organization);
  dup->env->subject = mutt_str_strdup(src->env->subject);
  /* struct Envelope::references */
  struct ListNode *src_np = NULL, *dup_np = NULL;
  STAILQ_FOREACH(src_np, &src->env->references, entries)
  {
    dup_np = mutt_mem_calloc(1, sizeof(struct ListNode));
    dup_np->data = mutt_str_strdup(src_np->data);
    STAILQ_INSERT_TAIL(&dup->env->references, dup_np, entries);
  }

  return dup;
}

/**
 * help_doc_from - Provides a validated/newly created help document (Header) from
 *                 a full qualified file path
 * @param   file    ... that is related to be a help document
 * @retval  pointer ... on success, to a Header structure
 * @retval  NULL    ... on failure, otherwise
 *
 * @note: This function only statically set specific member of a Header structure
 *        and some attributes, like Header::index, should be reset/updated.
 *        It also use Header::data to store additional help document information.
 */
static struct Email *help_doc_from(const char *file)
{
  HDType type = HDT_NONE;

  if ((type = help_file_type(file)) == HDT_NONE)
    return NULL; /* file is not a valid help doc */

  HelpList *fhdr = NULL;
  int len = help_file_header(&fhdr, file, HELP_FHDR_MAXLINES);
  if (!fhdr || (len < 1))
    return NULL; /* invalid or empty file header */

  /* from here, it should be safe to treat file as a valid help document */
  const char *bfn = mutt_path_basename(file);
  const char *pdn = mutt_path_basename(mutt_path_dirname(file));
  const char *rfp = (file + mutt_str_strlen(C_HelpDocDir) + 1);
  /* default timestamp, based on PACKAGE_VERSION */
  struct tm *tm = mutt_mem_calloc(1, sizeof(struct tm));
  strptime(PACKAGE_VERSION, "%Y%m%d", tm);
  time_t epoch = mutt_date_make_time(tm, 0);
  /* default subject, final may come from file header, e.g. "[title]: description" */
  char sbj[256];
  snprintf(sbj, sizeof(sbj), "[%s]: %s", pdn, bfn);
  /* bundle metadata */
  HDMeta *meta = mutt_mem_calloc(1, sizeof(HDMeta));
  meta->fhdr = fhdr;
  meta->name = mutt_str_strdup(bfn);
  meta->type = type;

  struct Email *hdoc = mutt_email_new();
  /* struct Email */
  hdoc->date_sent = epoch;
  hdoc->display_subject = true;
  hdoc->index = 0;
  hdoc->path = mutt_str_strdup(rfp);
  hdoc->read = true;
  hdoc->received = epoch;
  /* struct Email::data (custom metadata) */
  hdoc->edata = meta;
  hdoc->free_edata = help_doc_meta_free;
  /* struct Body */
  hdoc->content = mutt_body_new();
  hdoc->content->disposition = DISP_INLINE;
  hdoc->content->encoding = ENC_8BIT;
  hdoc->content->length = -1;
  hdoc->content->subtype = mutt_str_strdup("plain");
  hdoc->content->type = TYPE_TEXT;
  /* struct Envelope */
  hdoc->env = mutt_env_new();
  mutt_addrlist_parse(&hdoc->env->from, "Richard Russon <rich@flatcap.org>");
  hdoc->env->message_id = help_doc_msg_id(tm);
  FREE(tm);
  hdoc->env->organization = mutt_str_strdup("NeoMutt");
  hdoc->env->subject =
      help_doc_subject(fhdr, sbj, "[%s]: %s", "title", "description", NULL);

  return hdoc;
}

/**
 * help_doc_gather - Handler callback function for help_dir_scan()
 *                   Builds a list of help document objects
 * @param   list    ... generic list, for successfully processed item paths
 * @param   path    ... absolute path of a dir entry that pass preselection
 * @retval       0  ... on success,
 * @retval  N != 0  ... on failure, (not used currently, failures are externally
 *                      checked and silently suppressed herein)
 */
static int help_doc_gather(HelpList **list, const char *path)
{
  help_list_new_append(list, sizeof(struct Email *), help_doc_from(path));

  return 0;
}

/**
 * help_doc_uplink - Set a reference (threading) of one help document to an other
 * @param   target  ... document to refer to (via Header::message_id)
 * @param   source  ... document to link
 */
static void help_doc_uplink(const struct Email *target, const struct Email *source)
{
  if (!target || !source)
    return;

  char *tgt_msgid = target->env->message_id;
  if (!tgt_msgid || !*tgt_msgid)
    return;

  mutt_list_insert_tail(&source->env->references, mutt_str_strdup(tgt_msgid));
}

/**
 * help_dir_read - Read a directory and process its entries (not recursively) to
 *                 find and link all help documents
 * @param   path    ... absolute path of a directory
 *
 * @note: All sections are linked to their parent chapter regardless how deeply
 *        they're nested on the filesystem. Empty directories are ignored.
 */
static void help_dir_read(const char *path)
{
  HelpList *list = NULL;

  if ((help_dir_scan(path, false, DET_REG, NULL, help_doc_gather, &list) != 0) ||
      (list == NULL))
    return; /* skip errors and empty folder */

  /* sort any 'index.md' in list to the top */
  help_list_sort(list, help_doc_type_cmp);

  struct Email *help_msg_top = help_list_get(list, 0, NULL), *help_msg_cur = NULL;
  HDType help_msg_top_type = ((HDMeta *) help_msg_top->edata)->type;

  /* uplink a help chapter/section top node */
  if (help_msg_top_type & HDT_CHAPTER)
  {
    if (HELP_LINK_CHAPTERS != 0)
      help_doc_uplink(help_list_get(DocList, 0, NULL), help_msg_top);

    UpLink = DocList->size;
  }
  else if (help_msg_top_type & HDT_SECTION)
    help_doc_uplink(help_list_get(DocList, UpLink, NULL), help_msg_top);
  else
    UpLink = 0;

  help_msg_top->index = DocList->size;
  help_list_append(DocList, help_msg_top);

  /* link remaining docs to first list item */
  for (size_t i = 1; i < list->size; i++)
  {
    help_msg_cur = help_list_get(list, i, NULL);
    help_doc_uplink(help_msg_top, help_msg_cur);

    help_msg_cur->index = DocList->size;
    help_list_append(DocList, help_msg_cur);
  }
}

/**
 * help_dir_gather - Handler callback function for help_dir_scan()
 *                   Simple invoke help_dir_read() to search for help documents
 * @param   list    ... generic list, for successfully processed item paths (not used)
 * @param   path    ... absolute path of a dir entry that pass preselection
 * @retval       0  ... on success,
 * @retval  N != 0  ... on failure, (not used currently, failures are externally
 *                      checked and silently suppressed herein)
 *
 * @note: The list parameter isn't used herein, because every single result from
 *        help_scan_dir() will be processed directly.
 */
static int help_dir_gather(HelpList **list, const char *path)
{
  help_dir_read(path);

  return 0;
}

/**
 * help_doclist_init - Initialise the DocList at $help_doc_dir
 * @retval   0      ... on success,
 * @retval  -1      ... on failure, when help_dir_scan() of $help_doc_dir fails
 *
 * @note: Initialisation depends on several things, like $help_doc_dir changed,
 *        DocList isn't (and should not) be cached, DocList is empty.
 */
int help_doclist_init(void)
{
  if ((HELP_CACHE_DOCLIST != 0) && (DocList && !help_docdir_changed()))
    return 0;

  help_doclist_free();
  DocList = help_list_new(sizeof(struct Email));
  help_dir_read(C_HelpDocDir);
  help_docdir_id(C_HelpDocDir);

  return help_dir_scan(C_HelpDocDir, true, DET_DIR, NULL, help_dir_gather, NULL);
}

/**
 * help_doclist_parse - Evaluate and copy the DocList items to Context struct
 * @param   ctx     ... the current help mailbox object
 * @retval   0      ... on success,
 * @retval  -1      ... on failure, e.g. DocList initialisation failed
 *
 * @note: XXX This function also sets the status of a help document to unread,
 *        when its path match the user input, so the index line will mark it.
 *        This is just a test, has room for improvements and is less-than-ideal,
 *        because the user needs some knowledge about helpbox folder structure.
 */
static int help_doclist_parse(struct Mailbox *m)
{
  if ((help_doclist_init() != 0) || (DocList->size == 0))
    return -1;

  m->emails = (struct Email **) (help_list_clone(DocList, true, help_doc_clone))->data;
  m->msg_count = m->email_max = DocList->size;
  mutt_mem_realloc(&m->v2r, sizeof(int) * m->email_max);

  mutt_make_label_hash(m);

  m->readonly = true;
  /* all document paths are relative to C_HelpDocDir, so no transpose of ctx->path */
  mutt_str_replace(&m->realpath, C_HelpDocDir);

  /* check (none strict) what the user wants to see */
  const char *request = help_path_transpose(mutt_b2s(m->pathbuf), false);
  m->emails[0]->read = false;
  if (request)
  {
    mutt_buffer_increase_size(m->pathbuf, PATH_MAX);
    mutt_str_strfcpy(m->pathbuf->data, help_path_transpose(request, false), m->pathbuf->dsize); /* just sanitise */
    request += mutt_str_strlen(C_HelpDocDir) + 1;
    for (size_t i = 0; i < m->msg_count; i++)
    { /* TODO: prioritise folder (chapter/section) over root file names */
      if (mutt_str_strncmp(m->emails[i]->path, request, mutt_str_strlen(request)) == 0)
      {
        m->emails[0]->read = true;
        m->emails[i]->read = false;
        break;
      }
    }
  }

  return 0;
}

/**
 * help_ac_find - Find a Account that matches a Mailbox path
 */
struct Account *help_ac_find(struct Account *a, const char *path)
{
  if (!a || !path)
    return NULL;

  return a;
}

/**
 * help_ac_add - Add a Mailbox to a Account
 */
int help_ac_add(struct Account *a, struct Mailbox *m)
{
  if (!a || !m)
    return -1;

  if (m->magic != MUTT_HELP)
    return -1;

  m->account = a;

  struct MailboxNode *np = mutt_mem_calloc(1, sizeof(*np));
  np->mailbox = m;
  STAILQ_INSERT_TAIL(&a->mailboxes, np, entries);
  return 0;
}

static int help_mbox_open(struct Mailbox *m)
{
  mutt_debug(1, "entering help_mbox_open\n");

  if (m->magic != MUTT_HELP)
    return -1;

  /* TODO: ensure either mutt_option_set()/mutt_expand_path() sanitise a DT_PATH
   * option or let help_docdir_changed() treat "/path" and "/path///" as equally
   * to avoid a useless re-caching of the same directory */
  if (help_docdir_changed())
  {
    if (access(C_HelpDocDir, F_OK) == 0)
    { /* ensure a proper path, especially without any trailing slashes */
      mutt_str_replace(&C_HelpDocDir, NONULL(realpath(C_HelpDocDir, NULL)));
    }
    else
    {
      mutt_debug(1, "unable to access help mailbox '%s': %s (errno %d).\n",
                 C_HelpDocDir, strerror(errno), errno);
      return -1;
    }
  }

  __Backup_HTS = C_HideThreadSubject; /* backup the current global setting */
  C_HideThreadSubject = false; /* temporarily ensure subject is shown in thread view */

  return help_doclist_parse(m);
}

static int help_mbox_open_append(struct Mailbox *m, OpenMailboxFlags flags)
{
  mutt_debug(1, "entering help_mbox_open_append\n");
  return -1;
}

static int help_mbox_check(struct Mailbox *m, int *index_hint)
{
  mutt_debug(1, "entering help_mbox_check\n");
  return 0;
}

static int help_mbox_sync(struct Mailbox *m, int *index_hint)
{
  mutt_debug(1, "entering help_mbox_sync\n");
  return 0;
}

static int help_mbox_close(struct Mailbox *m)
{
  mutt_debug(1, "entering help_mbox_close\n");

  C_HideThreadSubject = __Backup_HTS; /* restore the previous global setting */

  return 0;
}

static int help_msg_open(struct Mailbox *m, struct Message *msg, int msgno)
{
  mutt_debug(1, "entering help_msg_open: %d, %s\n", msgno, m->emails[msgno]->env->subject);

  char path[PATH_MAX];
  snprintf(path, sizeof(path), "%s/%s", m->realpath, m->emails[msgno]->path);

  m->emails[msgno]->read = true; /* reset a probably previously set unread status */

  msg->fp = fopen(path, "r");
  if (!msg->fp)
  {
    mutt_perror(path);
    mutt_debug(1, "fopen: %s: %s (errno %d).\n", path, strerror(errno), errno);
    return -1;
  }

  return 0;
}

static int help_msg_open_new(struct Mailbox *m, struct Message *msg, struct Email *e)
{
  mutt_debug(1, "entering help_msg_open_new\n");
  return -1;
}

static int help_msg_commit(struct Mailbox *m, struct Message *msg)
{
  mutt_debug(1, "entering help_msg_commit\n");
  return -1;
}

static int help_msg_close(struct Mailbox *m, struct Message *msg)
{
  mutt_debug(1, "entering help_msg_close\n");
  mutt_file_fclose(&msg->fp);
  return 0;
}

static int help_msg_padding_size(struct Mailbox *m)
{
  mutt_debug(1, "entering help_msg_padding_size\n");
  return -1;
}

static int help_tags_edit(struct Mailbox *m, const char *tags, char *buf, size_t buflen)
{
  mutt_debug(1, "entering help_tags_edit\n");
  return -1;
}

static int help_tags_commit(struct Mailbox *m, struct Email *e, char *buf)
{
  mutt_debug(1, "entering help_tags_commit\n");
  return -1;
}

static int help_path_probe(const char *path, const struct stat *st)
{
  if (!path)
    return MUTT_UNKNOWN;

  if (mutt_str_strncasecmp(path, "help://", 7) == 0)
    return MUTT_HELP;

  return MUTT_UNKNOWN;
}

static int help_path_canon(char *buf, size_t buflen)
{
  mutt_debug(1, "entering help_path_canon\n");
  return 0;
}

static int help_path_pretty(char *buf, size_t buflen, const char *folder)
{
  mutt_debug(1, "entering help_path_pretty\n");
  return -1;
}

static int help_path_parent(char *buf, size_t buflen)
{
  mutt_debug(1, "entering help_path_parent\n");
  return -1;
}

// clang-format off
/**
 * mx_help_ops - Help Mailbox callback functions
 */
struct MxOps mx_help_ops = {
  .magic            = MUTT_HELP,
  .name             = "help",
  .ac_find          = help_ac_find,
  .ac_add           = help_ac_add,
  .mbox_open        = help_mbox_open,
  .mbox_open_append = help_mbox_open_append,
  .mbox_check       = help_mbox_check,
  .mbox_sync        = help_mbox_sync,
  .mbox_close       = help_mbox_close,
  .msg_open         = help_msg_open,
  .msg_open_new     = help_msg_open_new,
  .msg_commit       = help_msg_commit,
  .msg_close        = help_msg_close,
  .msg_padding_size = help_msg_padding_size,
  .tags_edit        = help_tags_edit,
  .tags_commit      = help_tags_commit,
  .path_probe       = help_path_probe,
  .path_canon       = help_path_canon,
  .path_pretty      = help_path_pretty,
  .path_parent      = help_path_parent,
};
// clang-format on
