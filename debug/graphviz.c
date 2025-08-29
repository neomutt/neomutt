/**
 * @file
 * Create a GraphViz dot file from the NeoMutt objects
 *
 * @authors
 * Copyright (C) 2018-2025 Richard Russon <rich@flatcap.org>
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
 * @page debug_graphviz GraphViz dot file
 *
 * Create a GraphViz dot file from the NeoMutt objects
 */

#include "config.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include "mutt/lib.h"
#include "address/lib.h"
#include "config/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "conn/lib.h"
#include "lib.h"
#include "attach/lib.h"
#include "compmbox/lib.h"
#include "expando/lib.h"
#include "imap/lib.h"
#include "maildir/lib.h"
#include "mbox/lib.h"
#include "ncrypt/lib.h"
#include "nntp/lib.h"
#include "pattern/lib.h"
#include "pop/lib.h"
#include "imap/adata.h"    // IWYU pragma: keep
#include "imap/mdata.h"    // IWYU pragma: keep
#include "imap/private.h"  // IWYU pragma: keep
#include "maildir/edata.h" // IWYU pragma: keep
#include "maildir/mdata.h" // IWYU pragma: keep
#include "mview.h"
#include "nntp/adata.h"  // IWYU pragma: keep
#include "nntp/mdata.h"  // IWYU pragma: keep
#include "pop/adata.h"   // IWYU pragma: keep
#include "pop/private.h" // IWYU pragma: keep
#ifdef USE_NOTMUCH
#include "notmuch/lib.h"
#include "notmuch/adata.h"   // IWYU pragma: keep
#include "notmuch/mdata.h"   // IWYU pragma: keep
#include "notmuch/private.h" // IWYU pragma: keep
#endif

// #define GV_HIDE_MVIEW
#define GV_HIDE_MVIEW_CONTENTS
// #define GV_HIDE_MBOX
// #define GV_HIDE_NEOMUTT
// #define GV_HIDE_CONFIG
// #define GV_HIDE_ADATA
// #define GV_HIDE_MDATA
// #define GV_HIDE_BODY_CONTENT
// #define GV_HIDE_ENVELOPE

void dot_email(FILE *fp, struct Email *e, struct ListHead *links);
void dot_envelope(FILE *fp, struct Envelope *env, struct ListHead *links);
void dot_patternlist(FILE *fp, struct PatternList *pl, struct ListHead *links);
void dot_expando_node(FILE *fp, struct ExpandoNode *node, struct ListHead *links);

void dot_type_bool(FILE *fp, const char *name, bool val)
{
  static const char *values[] = { "false", "true" };
  fprintf(fp, "\t\t<tr>\n");
  fprintf(fp, "\t\t\t<td border=\"0\" align=\"left\">%s</td>\n", name);
  fprintf(fp, "\t\t\t<td border=\"0\">=</td>\n");
  fprintf(fp, "\t\t\t<td border=\"0\" align=\"left\">%s</td>\n", values[val]);
  fprintf(fp, "\t\t</tr>\n");
}

#ifndef GV_HIDE_ADATA
void dot_type_char(FILE *fp, const char *name, char ch)
{
  fprintf(fp, "\t\t<tr>\n");
  fprintf(fp, "\t\t\t<td border=\"0\" align=\"left\">%s</td>\n", name);
  fprintf(fp, "\t\t\t<td border=\"0\">=</td>\n");
  if (ch == '\0')
    fprintf(fp, "\t\t\t<td border=\"0\" align=\"left\">NUL</td>\n");
  else
    fprintf(fp, "\t\t\t<td border=\"0\" align=\"left\">'%c'</td>\n", ch);
  fprintf(fp, "\t\t</tr>\n");
}
#endif

void dot_type_date(char *buf, size_t buflen, time_t timestamp)
{
  mutt_date_localtime_format(buf, buflen, "%Y-%m-%d %H:%M:%S", timestamp);
}

void dot_type_file(FILE *fp, const char *name, FILE *struct_fp)
{
  fprintf(fp, "\t\t<tr>\n");
  fprintf(fp, "\t\t\t<td border=\"0\" align=\"left\">%s</td>\n", name);
  fprintf(fp, "\t\t\t<td border=\"0\">=</td>\n");
  if (struct_fp)
  {
    fprintf(fp, "\t\t\t<td border=\"0\" align=\"left\">%p (%d)</td>\n",
            (void *) struct_fp, fileno(struct_fp));
  }
  else
  {
    fprintf(fp, "\t\t\t<td border=\"0\" align=\"left\">NULL</td>\n");
  }
  fprintf(fp, "\t\t</tr>\n");
}

void dot_type_number(FILE *fp, const char *name, int num)
{
  fprintf(fp, "\t\t<tr>\n");
  fprintf(fp, "\t\t\t<td border=\"0\" align=\"left\">%s</td>\n", name);
  fprintf(fp, "\t\t\t<td border=\"0\">=</td>\n");
  fprintf(fp, "\t\t\t<td border=\"0\" align=\"left\">%d</td>\n", num);
  fprintf(fp, "\t\t</tr>\n");
}

void dot_type_string_escape(struct Buffer *buf)
{
  for (int i = buf_len(buf) - 1; i >= 0; i--)
  {
    if (buf_at(buf, i) == '<')
      buf_inline_replace(buf, i, 1, "&lt;");
    else if (buf_at(buf, i) == '>')
      buf_inline_replace(buf, i, 1, "&gt;");
    else if (buf_at(buf, 1) == '&')
      buf_inline_replace(buf, i, 1, "&amp;");
  }
}

void dot_type_string(FILE *fp, const char *name, const char *str, bool force)
{
  if ((!str || (str[0] == '\0')) && !force)
    return;

  struct Buffer *buf = buf_new(str);

  if (!buf_is_empty(buf))
    dot_type_string_escape(buf);

  bool quoted = ((buf_at(buf, 0) != '[') && (buf_at(buf, 0) != '*'));

  fprintf(fp, "\t\t<tr>\n");
  fprintf(fp, "\t\t\t<td border=\"0\" align=\"left\">%s</td>\n", name);
  fprintf(fp, "\t\t\t<td border=\"0\">=</td>\n");
  if (quoted)
    fprintf(fp, "\t\t\t<td border=\"0\" align=\"left\">\"%s\"</td>\n", buf_string(buf));
  else
    fprintf(fp, "\t\t\t<td border=\"0\" align=\"left\">%s</td>\n", buf_string(buf));
  fprintf(fp, "\t\t</tr>\n");

  buf_free(&buf);
}

#ifndef GV_HIDE_MDATA
void dot_type_umask(char *buf, size_t buflen, int umask)
{
  snprintf(buf, buflen, "0%03o", umask);
}
#endif

void dot_ptr_name(char *buf, size_t buflen, const void *ptr)
{
  snprintf(buf, buflen, "obj_%p", ptr);
}

void dot_ptr(FILE *fp, const char *name, void *ptr, const char *colour)
{
  fprintf(fp, "\t\t<tr>\n");
  fprintf(fp, "\t\t\t<td border=\"0\" align=\"left\">%s</td>\n", name);
  fprintf(fp, "\t\t\t<td border=\"0\">=</td>\n");
  if (colour && ptr)
  {
    fprintf(fp, "\t\t\t<td border=\"0\" align=\"left\" bgcolor=\"%s\">%p</td>\n",
            colour, ptr);
  }
  else
  {
    fprintf(fp, "\t\t\t<td border=\"0\" align=\"left\">%p</td>\n", ptr);
  }
  fprintf(fp, "\t\t</tr>\n");
}

void dot_add_link(struct ListHead *links, void *src, void *dst, const char *label,
                  const char *short_label, bool back, const char *colour)
{
  if (!src || !dst)
    return;
  if (!colour)
    colour = "#c0c0c0";

  char obj1[64] = { 0 };
  char obj2[64] = { 0 };
  char text[512] = { 0 };
  char lstr[128] = { 0 };
  char sstr[128] = { 0 };

  dot_ptr_name(obj1, sizeof(obj1), src);
  dot_ptr_name(obj2, sizeof(obj2), dst);

  if (label)
    snprintf(lstr, sizeof(lstr), "edgetooltip=\"%s\"", label);

  if (short_label)
    snprintf(sstr, sizeof(sstr), "label=\"%s\"", short_label);

  snprintf(text, sizeof(text), "%s -> %s [ %s %s %s color=\"%s\" ]", obj1, obj2,
           back ? "dir=back" : "", lstr, sstr, colour);
  mutt_list_insert_tail(links, mutt_str_dup(text));
}

void dot_graph_header(FILE *fp)
{
  fprintf(fp, "digraph neomutt\n");
  fprintf(fp, "{\n\n");

  fprintf(fp, "\tgraph [\n");
  fprintf(fp, "\t\trankdir=\"TB\"\n");
  fprintf(fp, "\t\tnodesep=\"0.5\"\n");
  fprintf(fp, "\t\tranksep=\"0.5\"\n");
  fprintf(fp, "\t];\n");
  fprintf(fp, "\n");
  fprintf(fp, "\tnode [\n");
  fprintf(fp, "\t\tshape=\"plain\"\n");
  fprintf(fp, "\t];\n");
  fprintf(fp, "\n");
  fprintf(fp, "\tedge [\n");
  fprintf(fp, "\t\tpenwidth=\"4.5\"\n");
  fprintf(fp, "\t\tarrowsize=\"1.0\"\n");
  fprintf(fp, "\t\tcolor=\"#c0c0c0\"\n");
  fprintf(fp, "\t];\n");
  fprintf(fp, "\n");
}

void dot_graph_footer(FILE *fp, struct ListHead *links)
{
  fprintf(fp, "\n");
  struct ListNode *np = NULL;
  STAILQ_FOREACH(np, links, entries)
  {
    fprintf(fp, "\t%s;\n", np->data);
  }
  fprintf(fp, "\n}\n");
}

void dot_object_header(FILE *fp, const void *ptr, const char *name, const char *colour)
{
  char obj[64] = { 0 };
  dot_ptr_name(obj, sizeof(obj), ptr);

  if (!colour)
    colour = "#ffff80";

  fprintf(fp, "\t%s [\n", obj);
  fprintf(fp, "\t\tlabel=<<table cellspacing=\"0\" border=\"1\" rows=\"*\" "
              "color=\"#d0d0d0\">\n");
  fprintf(fp, "\t\t<tr>\n");
  fprintf(fp, "\t\t\t<td border=\"0\" align=\"left\" bgcolor=\"%s\" port=\"top\" colspan=\"3\"><font color=\"#000000\" point-size=\"20\"><b>%s</b></font> <font point-size=\"12\">(%p)</font></td>\n",
          colour, name, ptr);
  fprintf(fp, "\t\t</tr>\n");
}

void dot_object_footer(FILE *fp)
{
  fprintf(fp, "\t\t</table>>\n");
  fprintf(fp, "\t];\n");
  fprintf(fp, "\n");
}

void dot_node(FILE *fp, void *ptr, const char *name, const char *colour)
{
  char obj[64] = { 0 };
  dot_ptr_name(obj, sizeof(obj), ptr);

  fprintf(fp, "\t%s [\n", obj);
  fprintf(fp, "\t\tlabel=<<table cellspacing=\"0\" border=\"1\" rows=\"*\" "
              "color=\"#d0d0d0\">\n");
  fprintf(fp, "\t\t<tr>\n");
  fprintf(fp, "\t\t\t<td border=\"0\" bgcolor=\"%s\" port=\"top\"><font color=\"#000000\" point-size=\"20\"><b>%s</b></font></td>\n",
          colour, name);
  fprintf(fp, "\t\t</tr>\n");
  dot_object_footer(fp);
}

void dot_node_link(FILE *fp, void *ptr, const char *name, void *link, const char *colour)
{
  char obj[64] = { 0 };
  dot_ptr_name(obj, sizeof(obj), ptr);

  fprintf(fp, "\t%s [\n", obj);
  fprintf(fp, "\t\tlabel=<<table cellspacing=\"0\" border=\"1\" rows=\"*\" "
              "color=\"#d0d0d0\">\n");
  fprintf(fp, "\t\t<tr>\n");
  fprintf(fp, "\t\t\t<td border=\"0\" bgcolor=\"%s\" port=\"top\"><font color=\"#000000\" point-size=\"20\"><b>%s</b></font></td>\n",
          colour, name);
  fprintf(fp, "\t\t</tr>\n");

  fprintf(fp, "\t\t<tr>\n");
  fprintf(fp, "\t\t\t<td border=\"0\" align=\"left\" bgcolor=\"%s\">%p</td>\n", colour, link);
  fprintf(fp, "\t\t</tr>\n");

  dot_object_footer(fp);
}

void dot_path_fs(char *buf, size_t buflen, const char *path)
{
  if (!path)
  {
    buf[0] = '\0';
    return;
  }

  const char *slash = strrchr(path, '/');
  if (slash)
    slash++;
  else
    slash = path;

  mutt_str_copy(buf, slash, buflen);
}

void dot_path_imap(char *buf, size_t buflen, const char *path)
{
  char tmp[1024] = { 0 };
  mutt_str_copy(tmp, path, sizeof(tmp));

  struct Url *u = url_parse(tmp);

  if (u->path && (u->path[0] != '\0'))
    mutt_str_copy(buf, u->path, buflen);
  else
    snprintf(buf, buflen, "%s:%s", u->host, u->user);

  url_free(&u);
}

#ifndef GV_HIDE_CONFIG
void dot_config(FILE *fp, const char *name, int type, struct ConfigSubset *sub,
                struct ListHead *links)
{
  if (!sub)
    return;

  struct Buffer *value = buf_pool_get();
  dot_object_header(fp, (void *) name, "Config", "#ffff80");
  dot_type_string(fp, "scope", sub->name, true);

  if (sub->name)
  {
    char scope[256];
    snprintf(scope, sizeof(scope), "%s:", sub->name);

    struct HashElemArray hea = get_elem_list(sub->cs, GEL_ALL_CONFIG);
    struct HashElem **hep = NULL;
    ARRAY_FOREACH(hep, &hea)
    {
      struct HashElem *item = *hep;
      if ((item->type & type) == 0)
        continue;

      const char *iname = item->key.strkey;
      size_t slen = strlen(scope);
      if (mutt_str_startswith(iname, scope) != 0)
      {
        if (strchr(iname + slen, ':'))
          continue;
        if ((CONFIG_TYPE(item->type) == DT_STRING) && (item->type & D_SENSITIVE))
        {
          dot_type_string(fp, iname + slen, "***", true);
        }
        else
        {
          buf_reset(value);
          cs_subset_he_string_get(sub, item, value);
          dot_type_string(fp, iname + slen, buf_string(value), true);
        }
      }
    }
    ARRAY_FREE(&hea);
  }
  else
  {
    struct HashElemArray hea = get_elem_list(sub->cs, GEL_ALL_CONFIG);
    dot_type_number(fp, "count", ARRAY_SIZE(&hea));
    ARRAY_FREE(&hea);
  }

  dot_object_footer(fp);
  buf_pool_release(&value);
}
#endif

void dot_comp(FILE *fp, struct CompressInfo *ci, struct ListHead *links)
{
  dot_object_header(fp, ci, "CompressInfo", "#c0c060");
  dot_type_string(fp, "append", ci->cmd_append->string, true);
  dot_type_string(fp, "close", ci->cmd_close->string, true);
  dot_type_string(fp, "open", ci->cmd_open->string, true);
  dot_object_footer(fp);
}

void dot_mailbox_type(FILE *fp, const char *name, enum MailboxType type)
{
  const char *typestr = NULL;

  switch (type)
  {
    case MUTT_MBOX:
      typestr = "MBOX";
      break;
    case MUTT_MMDF:
      typestr = "MMDF";
      break;
    case MUTT_MH:
      typestr = "MH";
      break;
    case MUTT_MAILDIR:
      typestr = "MAILDIR";
      break;
    case MUTT_NNTP:
      typestr = "NNTP";
      break;
    case MUTT_IMAP:
      typestr = "IMAP";
      break;
    case MUTT_NOTMUCH:
      typestr = "NOTMUCH";
      break;
    case MUTT_POP:
      typestr = "POP";
      break;
    case MUTT_COMPRESSED:
      typestr = "COMPRESSED";
      break;
    default:
      typestr = "UNKNOWN";
  }

  fprintf(fp, "\t\t<tr>\n");
  fprintf(fp, "\t\t\t<td border=\"0\" align=\"left\">%s</td>\n", name);
  fprintf(fp, "\t\t\t<td border=\"0\">=</td>\n");
  fprintf(fp, "\t\t\t<td border=\"0\" align=\"left\">%s</td>\n", typestr);
  fprintf(fp, "\t\t</tr>\n");
}

#ifndef GV_HIDE_MDATA
void dot_mailbox_imap(FILE *fp, struct ImapMboxData *mdata, struct ListHead *links)
{
  dot_object_header(fp, mdata, "ImapMboxData", "#60c060");
  dot_type_string(fp, "name", mdata->name, true);
  dot_type_string(fp, "munge_name", mdata->munge_name, true);
  dot_type_string(fp, "real_name", mdata->real_name, true);
  dot_object_footer(fp);
}

void dot_mailbox_maildir(FILE *fp, struct MaildirMboxData *mdata, struct ListHead *links)
{
  char buf[64] = { 0 };

  dot_object_header(fp, mdata, "MaildirMboxData", "#60c060");

  dot_type_date(buf, sizeof(buf), mdata->mtime_cur.tv_sec);
  dot_type_string(fp, "mtime_cur", buf, true);

  dot_type_umask(buf, sizeof(buf), mdata->umask);
  dot_type_string(fp, "umask", buf, true);
  dot_object_footer(fp);
}

void dot_mailbox_mbox(FILE *fp, struct MboxAccountData *mdata, struct ListHead *links)
{
  char buf[64] = { 0 };

  dot_object_header(fp, mdata, "MboxAccountData", "#60c060");
  dot_ptr(fp, "fp", mdata->fp, NULL);

  dot_type_date(buf, sizeof(buf), mdata->atime.tv_sec);
  dot_type_string(fp, "atime", buf, true);

  dot_object_footer(fp);
}

void dot_mailbox_nntp(FILE *fp, struct NntpMboxData *mdata, struct ListHead *links)
{
  dot_object_header(fp, mdata, "NntpMboxData", "#60c060");
  dot_type_string(fp, "group", mdata->group, true);
  dot_type_string(fp, "desc", mdata->desc, true);

  dot_type_number(fp, "first_message", mdata->first_message);
  dot_type_number(fp, "last_message", mdata->last_message);
  dot_type_number(fp, "last_loaded", mdata->last_loaded);
  dot_type_number(fp, "last_cached", mdata->last_cached);
  dot_type_number(fp, "unread", mdata->unread);

  dot_type_bool(fp, "subscribed", mdata->subscribed);
  dot_type_bool(fp, "has_new_mail", mdata->has_new_mail);
  dot_type_bool(fp, "allowed", mdata->allowed);
  dot_type_bool(fp, "deleted", mdata->deleted);

  dot_object_footer(fp);
}

#ifdef USE_NOTMUCH
void dot_mailbox_notmuch(FILE *fp, struct NmMboxData *mdata, struct ListHead *links)
{
  dot_object_header(fp, mdata, "NmMboxData", "#60c060");
  dot_type_number(fp, "db_limit", mdata->db_limit);
  dot_object_footer(fp);
}
#endif

void dot_mailbox_pop(FILE *fp, struct PopAccountData *adata, struct ListHead *links)
{
  dot_object_header(fp, adata, "PopAccountData", "#60c060");
  dot_ptr(fp, "conn", adata->conn, "#ff8080");
  dot_object_footer(fp);
}
#endif

void dot_mailbox(FILE *fp, struct Mailbox *m, struct ListHead *links)
{
  char buf[64] = { 0 };

  dot_object_header(fp, m, "Mailbox", "#80ff80");
  dot_mailbox_type(fp, "type", m->type);
  dot_type_string(fp, "name", m->name, false);

  if ((m->type == MUTT_IMAP) || (m->type == MUTT_POP))
  {
    dot_path_imap(buf, sizeof(buf), buf_string(&m->pathbuf));
    dot_type_string(fp, "pathbuf", buf, true);
    dot_path_imap(buf, sizeof(buf), m->realpath);
    dot_type_string(fp, "realpath", buf, true);
  }
  else
  {
    dot_path_fs(buf, sizeof(buf), buf_string(&m->pathbuf));
    dot_type_string(fp, "pathbuf", buf, true);
    dot_path_fs(buf, sizeof(buf), m->realpath);
    dot_type_string(fp, "realpath", buf, true);
  }

#ifdef GV_HIDE_MDATA
  dot_ptr(fp, "mdata", m->mdata, NULL);
#endif
  dot_ptr(fp, "account", m->account, "#80ffff");
  dot_type_number(fp, "opened", m->opened);

  dot_type_number(fp, "msg_count", m->msg_count);
  // dot_type_number(fp, "msg_unread", m->msg_unread);
  // dot_type_number(fp, "msg_flagged", m->msg_flagged);
  // dot_type_number(fp, "msg_new", m->msg_new);
  // dot_type_number(fp, "msg_deleted", m->msg_deleted);
  // dot_type_number(fp, "msg_tagged", m->msg_tagged);

  dot_ptr(fp, "emails", m->emails, NULL);
  dot_type_number(fp, "email_max", m->email_max);
  dot_ptr(fp, "v2r", m->v2r, NULL);
  dot_type_number(fp, "vcount", m->vcount);

  dot_object_footer(fp);

  // dot_add_link(links, m, m->mdata, NULL, NULL, false, NULL);

#ifndef GV_HIDE_MDATA
  if (m->mdata)
  {
    if (m->type == MUTT_MAILDIR)
      dot_mailbox_maildir(fp, m->mdata, links);
    else if (m->type == MUTT_IMAP)
      dot_mailbox_imap(fp, m->mdata, links);
    else if (m->type == MUTT_POP)
      dot_mailbox_pop(fp, m->mdata, links);
    else if (m->type == MUTT_MBOX)
      dot_mailbox_mbox(fp, m->mdata, links);
    else if (m->type == MUTT_NNTP)
      dot_mailbox_nntp(fp, m->mdata, links);
#ifdef USE_NOTMUCH
    else if (m->type == MUTT_NOTMUCH)
      dot_mailbox_notmuch(fp, m->mdata, links);
#endif

    dot_add_link(links, m, m->mdata, "Mailbox->mdata", NULL, false, NULL);
  }
#endif

  if (m->compress_info)
  {
    dot_comp(fp, m->compress_info, links);
    dot_add_link(links, m, m->compress_info, "Mailbox->compress_info", NULL, false, NULL);
  }

#ifndef GV_HIDE_CONFIG
  if (m->name)
  {
    dot_config(fp, m->name, 0, m->sub, links);
    dot_add_link(links, m, m->name, "Mailbox Config", NULL, false, NULL);
  }
#endif
}

void dot_mailbox_node(FILE *fp, struct MailboxNode *mn, struct ListHead *links)
{
  dot_node(fp, mn, "MN", "#80ff80");

  dot_mailbox(fp, mn->mailbox, links);

  dot_add_link(links, mn, mn->mailbox, "MailboxNode->mailbox", NULL, false, NULL);

  struct Buffer *buf = buf_pool_get();

  char name[256] = { 0 };
  buf_addstr(buf, "{ rank=same ");

  dot_ptr_name(name, sizeof(name), mn);
  buf_add_printf(buf, "%s ", name);

  dot_ptr_name(name, sizeof(name), mn->mailbox);
  buf_add_printf(buf, "%s ", name);

#ifndef GV_HIDE_MDATA
  if (mn->mailbox->mdata)
  {
    dot_ptr_name(name, sizeof(name), mn->mailbox->mdata);
    buf_add_printf(buf, "%s ", name);
  }
#endif

#ifndef GV_HIDE_CONFIG
  if (mn->mailbox->name)
  {
    dot_ptr_name(name, sizeof(name), mn->mailbox->name);
    buf_add_printf(buf, "%s ", name);
  }
#endif

  buf_addstr(buf, "}");

  mutt_list_insert_tail(links, buf_strdup(buf));
  buf_pool_release(&buf);
}

void dot_mailbox_list(FILE *fp, struct MailboxList *ml, struct ListHead *links, bool abbr)
{
  struct MailboxNode *prev = NULL;
  struct MailboxNode *np = NULL;
  STAILQ_FOREACH(np, ml, entries)
  {
    if (abbr)
      dot_node_link(fp, np, "MN", np->mailbox, "#80ff80");
    else
      dot_mailbox_node(fp, np, links);
    if (prev)
      dot_add_link(links, prev, np, "MailboxNode->next", NULL, false, NULL);
    prev = np;
  }
}

#ifndef GV_HIDE_ADATA
void dot_connection(FILE *fp, struct Connection *c, struct ListHead *links)
{
  dot_object_header(fp, c, "Connection", "#ff8080");
  // dot_ptr(fp, "sockdata", c->sockdata, "#60c0c0");
  dot_type_number(fp, "fd", c->fd);
  dot_object_footer(fp);

  dot_object_header(fp, c->inbuf, "ConnAccount", "#ff8080");
  dot_type_string(fp, "user", c->account.user, true);
  dot_type_string(fp, "host", c->account.host, true);
  dot_type_number(fp, "port", c->account.port);
  dot_object_footer(fp);

  dot_add_link(links, c, c->inbuf, "Connection.ConnAccount", NULL, false, NULL);
}

void dot_account_imap(FILE *fp, struct ImapAccountData *adata, struct ListHead *links)
{
  dot_object_header(fp, adata, "ImapAccountData", "#60c0c0");
  // dot_type_string(fp, "mbox_name", adata->mbox_name, true);
  // dot_type_string(fp, "login", adata->conn->account.login, true);
  dot_type_string(fp, "user", adata->conn->account.user, true);
  dot_type_string(fp, "pass", adata->conn->account.pass[0] ? "***" : "", true);
  dot_type_number(fp, "port", adata->conn->account.port);
  // dot_ptr(fp, "conn", adata->conn, "#ff8080");
  dot_type_bool(fp, "unicode", adata->unicode);
  dot_type_bool(fp, "qresync", adata->qresync);
  dot_type_char(fp, "seqid", adata->seqid);
  dot_ptr(fp, "mailbox", adata->mailbox, "#80ff80");
  dot_object_footer(fp);

  if (adata->conn)
  {
    dot_connection(fp, adata->conn, links);
    dot_add_link(links, adata, adata->conn, "ImapAccountData->conn", NULL, false, NULL);
  }
}

void dot_account_mbox(FILE *fp, struct MboxAccountData *adata, struct ListHead *links)
{
  char buf[64] = { 0 };

  dot_object_header(fp, adata, "MboxAccountData", "#60c0c0");
  dot_ptr(fp, "fp", adata->fp, NULL);

  dot_type_date(buf, sizeof(buf), adata->atime.tv_sec);
  dot_type_string(fp, "atime", buf, true);
  dot_type_bool(fp, "locked", adata->locked);
  dot_type_bool(fp, "append", adata->append);

  dot_object_footer(fp);
}

void dot_account_nntp(FILE *fp, struct NntpAccountData *adata, struct ListHead *links)
{
  dot_object_header(fp, adata, "NntpAccountData", "#60c0c0");
  dot_type_number(fp, "groups_num", adata->groups_num);

  dot_type_bool(fp, "hasCAPABILITIES", adata->hasCAPABILITIES);
  dot_type_bool(fp, "hasSTARTTLS", adata->hasSTARTTLS);
  dot_type_bool(fp, "hasDATE", adata->hasDATE);
  dot_type_bool(fp, "hasLIST_NEWSGROUPS", adata->hasLIST_NEWSGROUPS);
  dot_type_bool(fp, "hasXGTITLE", adata->hasXGTITLE);
  dot_type_bool(fp, "hasLISTGROUP", adata->hasLISTGROUP);
  dot_type_bool(fp, "hasLISTGROUPrange", adata->hasLISTGROUPrange);
  dot_type_bool(fp, "hasOVER", adata->hasOVER);
  dot_type_bool(fp, "hasXOVER", adata->hasXOVER);
  dot_type_bool(fp, "cacheable", adata->cacheable);
  dot_type_bool(fp, "newsrc_modified", adata->newsrc_modified);

  dot_type_string(fp, "authenticators", adata->authenticators, true);
  dot_type_string(fp, "overview_fmt", adata->overview_fmt, true);
  dot_type_string(fp, "newsrc_file", adata->newsrc_file, true);
  dot_type_file(fp, "newsrc_fp", adata->fp_newsrc);

  dot_type_number(fp, "groups_num", adata->groups_num);
  dot_type_number(fp, "groups_max", adata->groups_max);

  char buf[128];
  dot_type_date(buf, sizeof(buf), adata->mtime);
  dot_type_string(fp, "mtime", buf, true);
  dot_type_date(buf, sizeof(buf), adata->newgroups_time);
  dot_type_string(fp, "newgroups_time", buf, true);
  dot_type_date(buf, sizeof(buf), adata->check_time);
  dot_type_string(fp, "check_time", buf, true);

  dot_object_footer(fp);

  if (adata->conn)
  {
    dot_connection(fp, adata->conn, links);
    dot_add_link(links, adata, adata->conn, "NntpAccountData->conn", NULL, false, NULL);
  }
}

#ifdef USE_NOTMUCH
void dot_account_notmuch(FILE *fp, struct NmAccountData *adata, struct ListHead *links)
{
  dot_object_header(fp, adata, "NmAccountData", "#60c0c0");
  dot_ptr(fp, "db", adata->db, NULL);
  dot_object_footer(fp);
}
#endif

void dot_account_pop(FILE *fp, struct PopAccountData *adata, struct ListHead *links)
{
  char buf[64] = { 0 };

  dot_object_header(fp, adata, "PopAccountData", "#60c0c0");

  dot_type_date(buf, sizeof(buf), adata->check_time);
  dot_type_string(fp, "check_time", buf, true);

  dot_type_string(fp, "login", adata->conn->account.login, true);
  dot_type_string(fp, "user", adata->conn->account.user, true);
  dot_type_string(fp, "pass", adata->conn->account.pass[0] ? "***" : "", true);
  dot_type_number(fp, "port", adata->conn->account.port);
  // dot_ptr(fp, "conn", adata->conn, "#ff8080");
  dot_object_footer(fp);

  dot_connection(fp, adata->conn, links);
  dot_add_link(links, adata, adata->conn, "PopAccountData->conn", NULL, false, NULL);
}
#endif

void dot_account(FILE *fp, struct Account *a, struct ListHead *links)
{
  dot_object_header(fp, a, "Account", "#80ffff");
  dot_mailbox_type(fp, "type", a->type);
  dot_type_string(fp, "name", a->name, true);
  // dot_ptr(fp, "adata", a->adata, "#60c0c0");
  dot_object_footer(fp);

#ifndef GV_HIDE_ADATA
  if (a->adata)
  {
    if (a->type == MUTT_IMAP)
      dot_account_imap(fp, a->adata, links);
    else if (a->type == MUTT_POP)
      dot_account_pop(fp, a->adata, links);
    else if (a->type == MUTT_MBOX)
      dot_account_mbox(fp, a->adata, links);
    else if (a->type == MUTT_NNTP)
      dot_account_nntp(fp, a->adata, links);
#ifdef USE_NOTMUCH
    else if (a->type == MUTT_NOTMUCH)
      dot_account_notmuch(fp, a->adata, links);
#endif

    dot_add_link(links, a, a->adata, "Account->adata", NULL, false, NULL);
  }
#endif

#ifndef GV_HIDE_CONFIG
  if (a->name)
  {
    dot_config(fp, a->name, 0, a->sub, links);
    dot_add_link(links, a, a->name, "Config", NULL, false, NULL);

    char name[256] = { 0 };
    struct Buffer *buf = buf_pool_get();

    buf_addstr(buf, "{ rank=same ");

    dot_ptr_name(name, sizeof(name), a);
    buf_add_printf(buf, "%s ", name);

    dot_ptr_name(name, sizeof(name), a->name);
    buf_add_printf(buf, "%s ", name);

    buf_addstr(buf, "}");
    mutt_list_insert_tail(links, buf_strdup(buf));
    buf_pool_release(&buf);
  }
#endif

  struct MailboxNode *first = STAILQ_FIRST(&a->mailboxes);
  dot_add_link(links, a, first, "Account->mailboxes", NULL, false, NULL);
  dot_mailbox_list(fp, &a->mailboxes, links, false);
}

void dot_account_list(FILE *fp, struct AccountList *al, struct ListHead *links)
{
  struct Account *prev = NULL;
  struct Account *np = NULL;
  TAILQ_FOREACH(np, al, entries)
  {
#ifdef GV_HIDE_MBOX
    if (np->type == MUTT_MBOX)
      continue;
#endif
    dot_account(fp, np, links);
    if (prev)
      dot_add_link(links, prev, np, "Account->next", NULL, false, NULL);

    prev = np;
  }
}

#ifndef GV_HIDE_MVIEW
void dot_mview(FILE *fp, struct MailboxView *mv, struct ListHead *links)
{
  dot_object_header(fp, mv, "MailboxView", "#ff80ff");
  dot_ptr(fp, "mailbox", mv->mailbox, "#80ff80");
#ifdef GV_HIDE_MVIEW_CONTENTS
  dot_type_number(fp, "vsize", mv->vsize);
  dot_type_string(fp, "pattern", mv->pattern, true);
  dot_type_bool(fp, "collapsed", mv->collapsed);
#endif
  dot_object_footer(fp);
}
#endif

void dump_graphviz(const char *title, struct MailboxView *mv)
{
  char name[256] = { 0 };
  struct ListHead links = STAILQ_HEAD_INITIALIZER(links);

  time_t now = time(NULL);
  if (title)
  {
    char date[128];
    mutt_date_localtime_format(date, sizeof(date), "%T", now);
    snprintf(name, sizeof(name), "%s-%s.gv", date, title);
  }
  else
  {
    mutt_date_localtime_format(name, sizeof(name), "%T.gv", now);
  }

  umask(022);
  FILE *fp = fopen(name, "w");
  if (!fp)
    return;

  dot_graph_header(fp);

#ifndef GV_HIDE_NEOMUTT
  dot_node(fp, NeoMutt, "NeoMutt", "#ffa500");
  dot_add_link(&links, NeoMutt, TAILQ_FIRST(&NeoMutt->accounts),
               "NeoMutt->accounts", NULL, false, NULL);
#ifndef GV_HIDE_CONFIG
  dot_config(fp, (const char *) NeoMutt->sub, 0, NeoMutt->sub, &links);
  dot_add_link(&links, NeoMutt, NeoMutt->sub, "NeoMutt Config", NULL, false, NULL);
  struct Buffer *buf = buf_pool_get();
  char obj1[64] = { 0 };
  char obj2[64] = { 0 };
  dot_ptr_name(obj1, sizeof(obj1), NeoMutt);
  dot_ptr_name(obj2, sizeof(obj2), NeoMutt->sub);
  buf_printf(buf, "{ rank=same %s %s }", obj1, obj2);
  mutt_list_insert_tail(&links, buf_strdup(buf));
  buf_pool_release(&buf);
#endif
#endif

  dot_account_list(fp, &NeoMutt->accounts, &links);

#ifndef GV_HIDE_MVIEW
  if (mv)
    dot_mview(fp, mv, &links);

#ifndef GV_HIDE_NEOMUTT
  /* Globals */
  fprintf(fp, "\t{ rank=same ");
  if (mv)
  {
    dot_ptr_name(name, sizeof(name), mv);
    fprintf(fp, "%s ", name);
  }
  dot_ptr_name(name, sizeof(name), NeoMutt);
  fprintf(fp, "%s ", name);
  fprintf(fp, "}\n");
#endif
#endif

  fprintf(fp, "\t{ rank=same ");
  struct Account *np = NULL;
  TAILQ_FOREACH(np, &NeoMutt->accounts, entries)
  {
#ifdef GV_HIDE_MBOX
    if (np->type == MUTT_MBOX)
      continue;
#endif
    dot_ptr_name(name, sizeof(name), np);
    fprintf(fp, "%s ", name);
  }
  fprintf(fp, "}\n");

  dot_graph_footer(fp, &links);
  fclose(fp);
  mutt_list_free(&links);
}

#ifndef GV_HIDE_BODY_CONTENT
void dot_parameter_list(FILE *fp, const char *name, const struct ParameterList *pl)
{
  if (!pl)
    return;
  if (TAILQ_EMPTY(pl))
    return;

  dot_object_header(fp, pl, "ParameterList", "#00ff00");

  struct Parameter *np = NULL;
  TAILQ_FOREACH(np, pl, entries)
  {
    dot_type_string(fp, np->attribute, np->value, false);
  }

  dot_object_footer(fp);
}

void dot_content(FILE *fp, struct Content *cont, struct ListHead *links)
{
  struct Buffer *buf = buf_pool_get();

  dot_object_header(fp, cont, "Content", "#800080");

  dot_type_number(fp, "hibin", cont->hibin);
  dot_type_number(fp, "lobin", cont->lobin);
  dot_type_number(fp, "nulbin", cont->nulbin);
  dot_type_number(fp, "crlf", cont->crlf);
  dot_type_number(fp, "ascii", cont->ascii);
  dot_type_number(fp, "linemax", cont->linemax);

#define ADD_BOOL(F) add_flag(buf, cont->F, #F)
  ADD_BOOL(space);
  ADD_BOOL(binary);
  ADD_BOOL(from);
  ADD_BOOL(dot);
  ADD_BOOL(cr);
#undef ADD_BOOL

  dot_object_footer(fp);

  buf_pool_release(&buf);
}
#endif

void dot_attach_ptr(FILE *fp, struct AttachPtr *aptr, struct ListHead *links)
{
  if (!aptr)
    return;

  dot_object_header(fp, aptr, "AttachPtr", "#ff0000");

  dot_type_file(fp, "fp", aptr->fp);

  dot_type_string(fp, "parent_type", name_content_type(aptr->parent_type), false);

  dot_type_number(fp, "level", aptr->level);
  dot_type_number(fp, "num", aptr->num);

  dot_type_bool(fp, "unowned", aptr->unowned);
  dot_type_bool(fp, "collapsed", aptr->collapsed);
  dot_type_bool(fp, "decrypted", aptr->decrypted);

  dot_object_footer(fp);

  dot_add_link(links, aptr->body, aptr, "AttachPtr->body", NULL, true, NULL);
}

void dot_body(FILE *fp, struct Body *b, struct ListHead *links, bool link_next)
{
  struct Buffer *buf = buf_pool_get();

  dot_object_header(fp, b, "Body", "#2020ff");

  char file[256];
  dot_path_fs(file, sizeof(file), b->filename);
  dot_type_string(fp, "file", file, false);

  dot_type_string(fp, "charset", b->charset, false);
  dot_type_string(fp, "description", b->description, false);
  dot_type_string(fp, "d_filename", b->d_filename, false);
  dot_type_string(fp, "form_name", b->form_name, false);
  dot_type_string(fp, "language", b->language, false);
  dot_type_string(fp, "subtype", b->subtype, false);
  dot_type_string(fp, "xtype", b->xtype, false);

  dot_type_string(fp, "type", name_content_type(b->type), true);
  dot_type_string(fp, "encoding", name_content_encoding(b->encoding), true);
  dot_type_string(fp, "disposition", name_content_disposition(b->disposition), true);

  if (b->stamp != 0)
  {
    char arr[64];
    dot_type_date(arr, sizeof(arr), b->stamp);
    dot_type_string(fp, "stamp", arr, true);
  }

#define ADD_BOOL(F) add_flag(buf, b->F, #F)
  ADD_BOOL(attach_qualifies);
  ADD_BOOL(badsig);
  ADD_BOOL(deleted);
  ADD_BOOL(force_charset);
  ADD_BOOL(goodsig);
#ifdef USE_AUTOCRYPT
  ADD_BOOL(is_autocrypt);
#endif
  ADD_BOOL(noconv);
  ADD_BOOL(tagged);
  ADD_BOOL(unlink);
  ADD_BOOL(use_disp);
  ADD_BOOL(warnsig);
#undef ADD_BOOL
  dot_type_string(fp, "bools", buf_is_empty(buf) ? "[NONE]" : buf_string(buf), true);

  dot_type_number(fp, "attach_count", b->attach_count);
  dot_type_number(fp, "hdr_offset", b->hdr_offset);
  dot_type_number(fp, "length", b->length);
  dot_type_number(fp, "offset", b->offset);

  dot_ptr(fp, "aptr", b->aptr, "#3bcbc4");

#ifdef GV_HIDE_BODY_CONTENT
  if (!TAILQ_EMPTY(&b->parameter))
  {
    struct Parameter *param = TAILQ_FIRST(&b->parameter);
    if (mutt_str_equal(param->attribute, "boundary"))
    {
      dot_type_string(fp, "boundary", param->value, false);
    }
  }
#endif

  dot_object_footer(fp);

#ifndef GV_HIDE_BODY_CONTENT
  if (!TAILQ_EMPTY(&b->parameter))
  {
    dot_parameter_list(fp, "parameter", &b->parameter);
    dot_add_link(links, b, &b->parameter, "Body->mime_headers", NULL, false, NULL);
  }
#endif

#ifndef GV_HIDE_ENVELOPE
  if (b->mime_headers)
  {
    dot_envelope(fp, b->mime_headers, links);
    dot_add_link(links, b, b->mime_headers, "Body->mime_headers", NULL, false, NULL);
  }
#endif

  if (b->email)
  {
    dot_email(fp, b->email, links);
    dot_add_link(links, b, b->email, "Body->email", NULL, false, NULL);
  }

  if (b->parts)
  {
    if (!b->email)
      dot_body(fp, b->parts, links, true);
    dot_add_link(links, b, b->parts, "Body->parts", NULL, false, "#ff0000");
  }

  if (b->next && link_next)
  {
    char name[256] = { 0 };
    buf_reset(buf);

    buf_addstr(buf, "{ rank=same ");

    dot_ptr_name(name, sizeof(name), b);
    buf_add_printf(buf, "%s ", name);

    for (; b->next; b = b->next)
    {
      dot_body(fp, b->next, links, false);
      dot_add_link(links, b, b->next, "Body->next", NULL, false, "#008000");

      dot_ptr_name(name, sizeof(name), b->next);
      buf_add_printf(buf, "%s ", name);
    }

    buf_addstr(buf, "}");
    mutt_list_insert_tail(links, buf_strdup(buf));
  }
  else
  {
#ifndef GV_HIDE_BODY_CONTENT
    if (b->content)
    {
      dot_content(fp, b->content, links);
      dot_add_link(links, b, b->content, "Body->content", NULL, false, NULL);
    }
#endif

    // if (b->aptr)
    // {
    //   dot_attach_ptr(fp, b->aptr, links);
    //   dot_add_link(links, b, b->aptr, "Body->aptr", NULL, false, NULL);
    // }
  }

  buf_pool_release(&buf);
}

#ifndef GV_HIDE_ENVELOPE
void dot_list_head(FILE *fp, const char *name, const struct ListHead *list)
{
  if (!list || !name)
    return;
  if (STAILQ_EMPTY(list))
    return;

  struct Buffer *buf = buf_pool_get();

  struct ListNode *np = NULL;
  STAILQ_FOREACH(np, list, entries)
  {
    if (!buf_is_empty(buf))
      buf_addch(buf, ',');
    buf_addstr(buf, np->data);
  }

  dot_type_string(fp, name, buf_string(buf), false);
  buf_pool_release(&buf);
}

void dot_addr_list(FILE *fp, const char *name, const struct AddressList *al,
                   struct ListHead *links)
{
  if (!al)
    return;
  if (TAILQ_EMPTY(al))
    return;

  struct Buffer *buf = buf_pool_get();
  mutt_addrlist_write(al, buf, true);
  dot_type_string(fp, name, buf_string(buf), false);
  buf_pool_release(&buf);
}

void dot_envelope(FILE *fp, struct Envelope *env, struct ListHead *links)
{
  struct Buffer *buf = buf_pool_get();

  dot_object_header(fp, env, "Envelope", "#ffff00");

#define ADD_FLAG(F) add_flag(buf, (env->changed & F), #F)
  ADD_FLAG(MUTT_ENV_CHANGED_IRT);
  ADD_FLAG(MUTT_ENV_CHANGED_REFS);
  ADD_FLAG(MUTT_ENV_CHANGED_XLABEL);
  ADD_FLAG(MUTT_ENV_CHANGED_SUBJECT);
#undef ADD_BOOL
  dot_type_string(fp, "changed", buf_is_empty(buf) ? "[NONE]" : buf_string(buf), true);

#define ADDR_LIST(AL) dot_addr_list(fp, #AL, &env->AL, links)
  ADDR_LIST(return_path);
  ADDR_LIST(from);
  ADDR_LIST(to);
  ADDR_LIST(cc);
  ADDR_LIST(bcc);
  ADDR_LIST(sender);
  ADDR_LIST(reply_to);
  ADDR_LIST(mail_followup_to);
  ADDR_LIST(x_original_to);
#undef ADDR_LIST

  dot_type_string(fp, "date", env->date, false);
  dot_type_string(fp, "disp_subj", env->disp_subj, false);
  dot_type_string(fp, "followup_to", env->followup_to, false);
  dot_type_string(fp, "list_post", env->list_post, false);
  dot_type_string(fp, "list_subscribe", env->list_subscribe, false);
  dot_type_string(fp, "list_unsubscribe", env->list_unsubscribe, false);
  dot_type_string(fp, "message_id", env->message_id, false);
  dot_type_string(fp, "newsgroups", env->newsgroups, false);
  dot_type_string(fp, "organization", env->organization, false);
  dot_type_string(fp, "real_subj", env->real_subj, false);
  dot_type_string(fp, "spam", buf_string(&env->spam), false);
  dot_type_string(fp, "subject", env->subject, false);
  dot_type_string(fp, "supersedes", env->supersedes, false);
  dot_type_string(fp, "xref", env->xref, false);
  dot_type_string(fp, "x_comment_to", env->x_comment_to, false);
  dot_type_string(fp, "x_label", env->x_label, false);

  if (0)
  {
    dot_list_head(fp, "references", &env->references);
    dot_list_head(fp, "in_reply_to", &env->in_reply_to);
    dot_list_head(fp, "userhdrs", &env->userhdrs);
  }

#ifdef USE_AUTOCRYPT
  dot_ptr(fp, "autocrypt", env->autocrypt, NULL);
  dot_ptr(fp, "autocrypt_gossip", env->autocrypt_gossip, NULL);
#endif

  dot_object_footer(fp);

  buf_pool_release(&buf);
}
#endif

void dot_email(FILE *fp, struct Email *e, struct ListHead *links)
{
  struct Buffer *buf = buf_pool_get();
  char arr[256];

  dot_object_header(fp, e, "Email", "#ff80ff");

  dot_type_string(fp, "path", e->path, true);

#define ADD_BOOL(F) add_flag(buf, e->F, #F)
  ADD_BOOL(active);
  ADD_BOOL(attach_del);
  ADD_BOOL(attach_valid);
  ADD_BOOL(changed);
  ADD_BOOL(collapsed);
  ADD_BOOL(deleted);
  ADD_BOOL(display_subject);
  ADD_BOOL(expired);
  ADD_BOOL(flagged);
  ADD_BOOL(matched);
  ADD_BOOL(mime);
  ADD_BOOL(old);
  ADD_BOOL(purge);
  ADD_BOOL(quasi_deleted);
  ADD_BOOL(read);
  ADD_BOOL(recip_valid);
  ADD_BOOL(replied);
  ADD_BOOL(searched);
  ADD_BOOL(subject_changed);
  ADD_BOOL(superseded);
  ADD_BOOL(tagged);
  ADD_BOOL(threaded);
  ADD_BOOL(trash);
  ADD_BOOL(visible);
#undef ADD_BOOL
  dot_type_string(fp, "bools", buf_is_empty(buf) ? "[NONE]" : buf_string(buf), true);

  buf_reset(buf);
#define ADD_BOOL(F) add_flag(buf, (e->security & F), #F)
  ADD_BOOL(SEC_ENCRYPT);
  ADD_BOOL(SEC_SIGN);
  ADD_BOOL(SEC_GOODSIGN);
  ADD_BOOL(SEC_BADSIGN);
  ADD_BOOL(SEC_PARTSIGN);
  ADD_BOOL(SEC_SIGNOPAQUE);
  ADD_BOOL(SEC_KEYBLOCK);
  ADD_BOOL(SEC_INLINE);
  ADD_BOOL(SEC_OPPENCRYPT);
  ADD_BOOL(SEC_AUTOCRYPT);
  ADD_BOOL(SEC_AUTOCRYPT_OVERRIDE);
  ADD_BOOL(APPLICATION_PGP);
  ADD_BOOL(APPLICATION_SMIME);
  ADD_BOOL(PGP_TRADITIONAL_CHECKED);
#undef ADD_BOOL
  dot_type_string(fp, "security", buf_is_empty(buf) ? "[NONE]" : buf_string(buf), true);

  dot_type_number(fp, "num_hidden", e->num_hidden);
  dot_type_number(fp, "offset", e->offset);
  dot_type_number(fp, "lines", e->lines);
  dot_type_number(fp, "index", e->index);
  dot_type_number(fp, "msgno", e->msgno);
  dot_type_number(fp, "vnum", e->vnum);
  dot_type_number(fp, "score", e->score);
  dot_type_number(fp, "attach_total", e->attach_total);

  // struct MaildirEmailData *edata = maildir_edata_get(e);
  // if (edata)
  //   dot_type_string(fp, "maildir_flags", edata->maildir_flags, false);

  if (e->date_sent != 0)
  {
    char zone[32];
    dot_type_date(arr, sizeof(arr), e->date_sent);
    snprintf(zone, sizeof(zone), " (%c%02u%02u)", e->zoccident ? '-' : '+',
             e->zhours, e->zminutes);
    struct Buffer *date = buf_pool_get();
    buf_printf(date, "%s%s", arr, zone);
    dot_type_string(fp, "date_sent", buf_string(date), false);
    buf_pool_release(&date);
  }

  if (e->received != 0)
  {
    dot_type_date(arr, sizeof(arr), e->received);
    dot_type_string(fp, "received", arr, false);
  }

  dot_object_footer(fp);

  if (e->body)
  {
    dot_body(fp, e->body, links, true);
    dot_add_link(links, e, e->body, "Email->body", NULL, false, NULL);
  }

#ifndef GV_HIDE_ENVELOPE
  if (e->env)
  {
    dot_envelope(fp, e->env, links);
    dot_add_link(links, e, e->env, "Email->env", NULL, false, NULL);

    buf_reset(buf);
    buf_addstr(buf, "{ rank=same ");

    dot_ptr_name(arr, sizeof(arr), e);
    buf_add_printf(buf, "%s ", arr);

    dot_ptr_name(arr, sizeof(arr), e->env);
    buf_add_printf(buf, "%s ", arr);

    buf_addstr(buf, "}");

    mutt_list_insert_tail(links, buf_strdup(buf));
  }
#endif

  // struct TagList tags;

  buf_pool_release(&buf);
}

void dump_graphviz_body(struct Body *b)
{
  char name[256] = { 0 };
  struct ListHead links = STAILQ_HEAD_INITIALIZER(links);

  time_t now = time(NULL);
  mutt_date_localtime_format(name, sizeof(name), "%T-email.gv", now);

  umask(022);
  FILE *fp = fopen(name, "w");
  if (!fp)
    return;

  dot_graph_header(fp);

  dot_body(fp, b, &links, true);

  dot_graph_footer(fp, &links);
  fclose(fp);
  mutt_list_free(&links);
}

void dump_graphviz_email(struct Email *e, const char *title)
{
  char name[256] = { 0 };
  struct ListHead links = STAILQ_HEAD_INITIALIZER(links);

  if (!title)
    title = "email";

  char format[64];
  snprintf(format, sizeof(format), "%%T-%s.gv", title);

  time_t now = time(NULL);
  mutt_date_localtime_format(name, sizeof(name), format, now);

  umask(022);
  FILE *fp = fopen(name, "w");
  if (!fp)
    return;

  dot_graph_header(fp);

  dot_email(fp, e, &links);

  dot_graph_footer(fp, &links);
  fclose(fp);
  mutt_list_free(&links);
}

void dot_attach_ptr2(FILE *fp, struct AttachPtr *aptr, struct ListHead *links)
{
  if (!aptr)
    return;

  dot_object_header(fp, aptr, "AttachPtr", "#3bcbc4");

  dot_ptr(fp, "body", aptr->body, "#2020ff");
  dot_type_file(fp, "fp", aptr->fp);

  dot_type_string(fp, "parent_type", name_content_type(aptr->parent_type), false);
  dot_type_number(fp, "level", aptr->level);
  dot_type_number(fp, "num", aptr->num);
  dot_type_bool(fp, "unowned", aptr->unowned);
  dot_type_bool(fp, "collapsed", aptr->collapsed);
  dot_type_bool(fp, "decrypted", aptr->decrypted);

  // dot_type_string(fp, "tree", aptr->tree, false);

  dot_object_footer(fp);
}

void dot_array_actx_idx(FILE *fp, struct AttachPtr **idx, short idxlen,
                        short idxmax, struct ListHead *links)
{
  dot_object_header(fp, idx, "AttachCtx-&gt;idx", "#9347de");

  dot_type_number(fp, "idxlen", idxlen);
  dot_type_number(fp, "idxmax", idxmax);

  char arr[32];
  for (size_t i = 0; i < idxmax; i++)
  {
    snprintf(arr, sizeof(arr), "idx[%zu]", i);
    dot_ptr(fp, arr, idx[i], "#3bcbc4");
  }

  dot_object_footer(fp);

  for (size_t i = 0; i < idxlen; i++)
  {
    dot_attach_ptr2(fp, idx[i], links);
    dot_add_link(links, idx, idx[i], "AttachCtx-&gt;idx", NULL, false, NULL);
  }
}

void dot_array_actx_v2r(FILE *fp, short *v2r, short vcount, struct ListHead *links)
{
  dot_object_header(fp, v2r, "AttachCtx-&gt;v2r", "#9347de");

  dot_type_number(fp, "vcount", vcount);

  char arr[32];
  for (size_t i = 0; i < vcount; i++)
  {
    snprintf(arr, sizeof(arr), "v2r[%zu]", i);
    dot_type_number(fp, arr, v2r[i]);
  }

  dot_object_footer(fp);
}

void dot_array_actx_fp_idx(FILE *fp, FILE **fp_idx, short fp_len, short fp_max,
                           struct ListHead *links)
{
  dot_object_header(fp, fp_idx, "AttachCtx-&gt;fp_idx", "#f86e28");

  dot_type_number(fp, "fp_len", fp_len);
  dot_type_number(fp, "fp_max", fp_max);

  char arr[32];
  for (size_t i = 0; i < fp_max; i++)
  {
    snprintf(arr, sizeof(arr), "fp_idx[%zu]", i);
    dot_type_file(fp, arr, fp_idx[i]);
  }

  dot_object_footer(fp);
}

void dot_array_actx_body_idx(FILE *fp, struct Body **body_idx, short body_len,
                             short body_max, struct ListHead *links)
{
  dot_object_header(fp, body_idx, "AttachCtx-&gt;body_idx", "#4ff270");

  dot_type_number(fp, "body_len", body_len);
  dot_type_number(fp, "body_max", body_max);

  char arr[32];
  for (size_t i = 0; i < body_max; i++)
  {
    snprintf(arr, sizeof(arr), "body_idx[%zu]", i);
    dot_ptr(fp, arr, body_idx[i], "#2020ff");
  }

  dot_object_footer(fp);

  for (size_t i = 0; i < body_max; i++)
  {
    if (!body_idx[i])
      continue;
    dot_body(fp, body_idx[i], links, true);
    dot_add_link(links, body_idx, body_idx[i], "AttachCtx->Body", NULL, false, "#008000");
  }
}

void dot_attach_ctx(FILE *fp, struct AttachCtx *actx, struct ListHead *links)
{
  dot_object_header(fp, actx, "AttachCtx", "#9347de");

  dot_ptr(fp, "email", actx->email, "#ff80ff");
  dot_type_file(fp, "fp_root", actx->fp_root);

  dot_object_footer(fp);

  if (actx->idx)
  {
    dot_array_actx_idx(fp, actx->idx, actx->idxlen, actx->idxmax, links);
    dot_add_link(links, actx, actx->idx, "AttachCtx-&gt;idx", NULL, false, NULL);
  }

  if (actx->v2r)
  {
    dot_array_actx_v2r(fp, actx->v2r, actx->vcount, links);
    dot_add_link(links, actx, actx->v2r, "AttachCtx-&gt;v2r", NULL, false, NULL);
  }

  if (actx->fp_idx)
  {
    dot_array_actx_fp_idx(fp, actx->fp_idx, actx->fp_len, actx->fp_max, links);
    dot_add_link(links, actx, actx->fp_idx, "AttachCtx-&gt;fp_idx", NULL, false, NULL);
  }

  if (actx->body_idx)
  {
    dot_array_actx_body_idx(fp, actx->body_idx, actx->body_len, actx->body_max, links);
    dot_add_link(links, actx, actx->body_idx, "AttachCtx-&gt;body_idx", NULL, false, NULL);
  }
}

void dump_graphviz_attach_ctx(struct AttachCtx *actx)
{
  char name[256] = { 0 };
  struct ListHead links = STAILQ_HEAD_INITIALIZER(links);

  time_t now = time(NULL);
  mutt_date_localtime_format(name, sizeof(name), "%T-actx.gv", now);

  umask(022);
  FILE *fp = fopen(name, "w");
  if (!fp)
    return;

  dot_graph_header(fp);

  dot_attach_ctx(fp, actx, &links);

  dot_graph_footer(fp, &links);
  fclose(fp);
  mutt_list_free(&links);
}

const char *pattern_type_name(int type)
{
  static struct Mapping PatternNames[] = {
    // clang-format off
    { "address",         MUTT_PAT_ADDRESS },
    { "AND",             MUTT_PAT_AND },
    { "bcc",             MUTT_PAT_BCC },
    { "body",            MUTT_PAT_BODY },
    { "broken",          MUTT_PAT_BROKEN },
    { "cc",              MUTT_PAT_CC },
    { "children",        MUTT_PAT_CHILDREN },
    { "collapsed",       MUTT_PAT_COLLAPSED },
    { "crypt_encrypt",   MUTT_PAT_CRYPT_ENCRYPT },
    { "crypt_sign",      MUTT_PAT_CRYPT_SIGN },
    { "crypt_verified",  MUTT_PAT_CRYPT_VERIFIED },
    { "date",            MUTT_PAT_DATE },
    { "date_received",   MUTT_PAT_DATE_RECEIVED },
    { "driver_tags",     MUTT_PAT_DRIVER_TAGS },
    { "duplicated",      MUTT_PAT_DUPLICATED },
    { "from",            MUTT_PAT_FROM },
    { "header",          MUTT_PAT_HEADER },
    { "hormel",          MUTT_PAT_HORMEL },
    { "id",              MUTT_PAT_ID },
    { "id_external",     MUTT_PAT_ID_EXTERNAL },
    { "list",            MUTT_PAT_LIST },
    { "message",         MUTT_PAT_MESSAGE },
    { "mimeattach",      MUTT_PAT_MIMEATTACH },
    { "mimetype",        MUTT_PAT_MIMETYPE },
    { "newsgroups",      MUTT_PAT_NEWSGROUPS },
    { "OR",              MUTT_PAT_OR },
    { "parent",          MUTT_PAT_PARENT },
    { "personal_from",   MUTT_PAT_PERSONAL_FROM },
    { "personal_recip",  MUTT_PAT_PERSONAL_RECIP },
    { "pgp_key",         MUTT_PAT_PGP_KEY },
    { "recipient",       MUTT_PAT_RECIPIENT },
    { "reference",       MUTT_PAT_REFERENCE },
    { "score",           MUTT_PAT_SCORE },
    { "sender",          MUTT_PAT_SENDER },
    { "serversearch",    MUTT_PAT_SERVERSEARCH },
    { "size",            MUTT_PAT_SIZE },
    { "subject",         MUTT_PAT_SUBJECT },
    { "subscribed_list", MUTT_PAT_SUBSCRIBED_LIST },
    { "thread",          MUTT_PAT_THREAD },
    { "to",              MUTT_PAT_TO },
    { "unreferenced",    MUTT_PAT_UNREFERENCED },
    { "whole_msg",       MUTT_PAT_WHOLE_MSG },
    { "xlabel",          MUTT_PAT_XLABEL },
    { NULL, 0 },
    // clang-format on
  };

  return mutt_map_get_name(type, PatternNames);
}

void dot_pattern(FILE *fp, struct Pattern *pat, struct ListHead *links)
{
  struct Buffer *buf = buf_pool_get();
  dot_object_header(fp, pat, "Pattern", "#c040c0");

  dot_type_string(fp, "op", pattern_type_name(pat->op), true);
  if ((pat->min != 0) || (pat->max != 0))
  {
    dot_type_number(fp, "min", pat->min);
    dot_type_number(fp, "max", pat->max);
  }

#define ADD_BOOL(F) add_flag(buf, pat->F, #F)
  ADD_BOOL(pat_not);
  ADD_BOOL(all_addr);
  ADD_BOOL(string_match);
  ADD_BOOL(group_match);
  ADD_BOOL(ign_case);
  ADD_BOOL(is_alias);
  ADD_BOOL(dynamic);
  ADD_BOOL(sendmode);
  ADD_BOOL(is_multi);
#undef ADD_BOOL
  dot_type_string(fp, "flags", buf_is_empty(buf) ? "[NONE]" : buf_string(buf), true);

  if (pat->group_match)
  {
    // struct Group *group;         ///< Address group if group_match is set
  }
  else if (pat->string_match)
  {
    dot_type_string(fp, "str", pat->p.str, true);
  }
  else if (pat->is_multi)
  {
    // struct ListHead multi_cases; ///< Multiple strings for ~I pattern
  }
  else
  {
    if (pat->p.regex)
    {
      dot_ptr(fp, "regex", pat->p.regex, NULL);
      dot_type_string(fp, "pattern", pat->raw_pattern, true);
    }
  }

  dot_object_footer(fp);

  if (pat->child)
  {
    dot_patternlist(fp, pat->child, links);
    struct Pattern *first = SLIST_FIRST(pat->child);
    dot_add_link(links, pat, first, "Pattern->child", NULL, false, "#00ff00");
  }
  buf_pool_release(&buf);
}

void dot_patternlist(FILE *fp, struct PatternList *pl, struct ListHead *links)
{
  struct Buffer *buf = buf_pool_get();

  char name[256] = { 0 };
  buf_addstr(buf, "{ rank=same ");

  struct Pattern *prev = NULL;
  struct Pattern *np = NULL;
  SLIST_FOREACH(np, pl, entries)
  {
    dot_pattern(fp, np, links);
    if (prev)
      dot_add_link(links, prev, np, "PatternList->next", NULL, false, "#ff0000");
    prev = np;

    dot_ptr_name(name, sizeof(name), np);
    buf_add_printf(buf, "%s ", name);
  }

  buf_addstr(buf, "}");

  mutt_list_insert_tail(links, buf_strdup(buf));
  buf_pool_release(&buf);
}

void dump_graphviz_patternlist(struct PatternList *pl)
{
  char name[256] = { 0 };
  struct ListHead links = STAILQ_HEAD_INITIALIZER(links);

  time_t now = time(NULL);
  mutt_date_localtime_format(name, sizeof(name), "%T-pattern.gv", now);

  umask(022);
  FILE *fp = fopen(name, "w");
  if (!fp)
    return;

  dot_graph_header(fp);

  dot_patternlist(fp, pl, &links);

  dot_graph_footer(fp, &links);
  fclose(fp);
  mutt_list_free(&links);
}

void dot_format(FILE *fp, struct ExpandoFormat *fmt)
{
  if (!fmt)
    return;

  dot_type_number(fp, "min_cols", fmt->min_cols);
  dot_type_number(fp, "max_cols", fmt->max_cols);

  char *just = "UNKNOWN";
  switch (fmt->justification)
  {
    case JUSTIFY_LEFT:
      just = "JUSTIFY_LEFT";
      break;
    case JUSTIFY_CENTER:
      just = "JUSTIFY_CENTER";
      break;
    case JUSTIFY_RIGHT:
      just = "JUSTIFY_RIGHT";
      break;
  }
  dot_type_string(fp, "justification", just, true);
  dot_type_char(fp, "leader", fmt->leader);
}

void dot_expando_node_empty(FILE *fp, struct ExpandoNode *node, struct ListHead *links)
{
  dot_object_header(fp, node, "Empty", "#ffffff");
  // dot_type_string(fp, "type", "ENT_EMPTY", true);
  dot_object_footer(fp);
}

void dot_expando_node_text(FILE *fp, struct ExpandoNode *node, struct ListHead *links)
{
  struct Buffer *buf = buf_pool_get();
  dot_object_header(fp, node, "Text", "#ffff80");
  // dot_type_string(fp, "type", "ENT_TEXT", true);
  dot_type_string(fp, "text", node->text, false);

  dot_object_footer(fp);

  buf_pool_release(&buf);
}

void dot_expando_node_pad(FILE *fp, struct ExpandoNode *node, struct ListHead *links)
{
  struct Buffer *buf = buf_pool_get();
  dot_object_header(fp, node, "Pad", "#80ffff");
  // dot_type_string(fp, "type", "ENT_PADDING", true);

  struct NodePaddingPrivate *priv = node->ndata;
  char *pad = "UNKNOWN";
  switch (priv->pad_type)
  {
    case EPT_FILL_EOL:
      pad = "EPT_FILL_EOL";
      break;
    case EPT_HARD_FILL:
      pad = "EPT_HARD_FILL";
      break;
    case EPT_SOFT_FILL:
      pad = "EPT_SOFT_FILL";
      break;
  }
  dot_type_string(fp, "type", pad, true);
  dot_type_string(fp, "char", node->text, false);

  dot_object_footer(fp);

  struct ExpandoNode *left = node_get_child(node, ENP_LEFT);
  if (left)
  {
    dot_expando_node(fp, left, links);
    dot_add_link(links, node, left, "Pad->left", "left", false, "#80ff80");
  }

  struct ExpandoNode *right = node_get_child(node, ENP_RIGHT);
  if (right)
  {
    dot_expando_node(fp, right, links);
    dot_add_link(links, node, right, "Pad->right", "right", false, "#ff8080");
  }

  buf_pool_release(&buf);
}

void dot_expando_node_condition(FILE *fp, struct ExpandoNode *node, struct ListHead *links)
{
  struct Buffer *buf = buf_pool_get();
  dot_object_header(fp, node, "Condition", "#ff8080");
  // dot_type_string(fp, "type", "ENT_CONDITION", true);
  dot_type_string(fp, "text", node->text, false);
  dot_format(fp, node->format);

  dot_object_footer(fp);

  struct ExpandoNode *node_cond = node_get_child(node, ENC_CONDITION);
  struct ExpandoNode *node_true = node_get_child(node, ENC_TRUE);
  struct ExpandoNode *node_false = node_get_child(node, ENC_FALSE);

  dot_expando_node(fp, node_cond, links);
  dot_add_link(links, node, node_cond, "Condition->condition", "condition", false, "#ff80ff");
  if (node_true)
  {
    dot_expando_node(fp, node_true, links);
    dot_add_link(links, node, node_true, "Condition->true", "true", false, "#80ff80");
  }
  if (node_false)
  {
    dot_expando_node(fp, node_false, links);
    dot_add_link(links, node, node_false, "Condition->false", "false", false, "#ff8080");
  }

  buf_pool_release(&buf);
}

void dot_expando_node_conditional_bool(FILE *fp, struct ExpandoNode *node,
                                       struct ListHead *links)
{
  struct Buffer *buf = buf_pool_get();
  dot_object_header(fp, node, "CondBool", "#c0c0ff");
  // dot_type_string(fp, "type", "ENT_CONDBOOL", true);
  dot_type_string(fp, "did", name_expando_domain(node->did), true);
  dot_type_string(fp, "uid", name_expando_uid(node->did, node->uid), true);
  dot_type_string(fp, "text", node->text, false);

  dot_object_footer(fp);

  buf_pool_release(&buf);
}

void dot_expando_node_conditional_date(FILE *fp, struct ExpandoNode *node,
                                       struct ListHead *links)
{
  struct Buffer *buf = buf_pool_get();
  dot_object_header(fp, node, "CondDate", "#c0c0ff");
  // dot_type_string(fp, "type", "ENT_CONDDATE", true);
  dot_type_string(fp, "did", name_expando_domain(node->did), true);
  dot_type_string(fp, "uid", name_expando_uid(node->did, node->uid), true);
  dot_type_string(fp, "text", node->text, false);

  struct NodeCondDatePrivate *priv = node->ndata;
  if (priv)
  {
    dot_type_number(fp, "count", priv->count);
    dot_type_char(fp, "period", priv->period);
  }

  dot_object_footer(fp);

  buf_pool_release(&buf);
}

void dot_expando_node_container(FILE *fp, struct ExpandoNode *node, struct ListHead *links)
{
  dot_object_header(fp, node, "Container", "#80ffff");
  // dot_type_string(fp, "type", "ENT_CONTAINER", true);
  dot_type_number(fp, "children", ARRAY_SIZE(&node->children));
  dot_format(fp, node->format);
  dot_object_footer(fp);

  struct ExpandoNode **enp = NULL;
  enp = ARRAY_FIRST(&node->children);
  if (!enp)
    return;

  struct ExpandoNode *child = *enp;
  dot_add_link(links, node, child, "Node->children", "children", false, "#80ff80");

  char name[256] = { 0 };
  struct Buffer *rank = buf_pool_get();
  buf_addstr(rank, "{ rank=same ");

  struct ExpandoNode *prev = NULL;
  ARRAY_FOREACH(enp, &node->children)
  {
    child = *enp;

    dot_expando_node(fp, child, links);
    if (prev)
    {
      dot_add_link(links, prev, child, "Node->next", "next", false, "#80ff80");
    }
    prev = child;

    dot_ptr_name(name, sizeof(name), child);
    buf_add_printf(rank, "%s ", name);
  }

  buf_addstr(rank, "}");
  mutt_list_insert_tail(links, buf_strdup(rank));
  buf_pool_release(&rank);
}

void dot_expando_node_expando(FILE *fp, struct ExpandoNode *node, struct ListHead *links)
{
  struct Buffer *buf = buf_pool_get();
  dot_object_header(fp, node, "Expando", "#80ff80");

  // dot_type_number(fp, "type", node->type);
  dot_type_string(fp, "did", name_expando_domain(node->did), true);
  dot_type_string(fp, "uid", name_expando_uid(node->did, node->uid), true);
  dot_type_string(fp, "text", node->text, false);

  dot_format(fp, node->format);

  dot_object_footer(fp);

  buf_pool_release(&buf);
}

void dot_expando_node_unknown(FILE *fp, struct ExpandoNode *node, struct ListHead *links)
{
  struct Buffer *buf = buf_pool_get();
  dot_object_header(fp, node, "UNKNOWN", "#ff0000");

  dot_type_number(fp, "type", node->type);
  dot_type_number(fp, "did", node->did);
  dot_type_number(fp, "uid", node->uid);
  dot_type_string(fp, "text", node->text, false);

  dot_object_footer(fp);

  buf_pool_release(&buf);
}

void dot_expando_node(FILE *fp, struct ExpandoNode *node, struct ListHead *links)
{
  switch (node->type)
  {
    case ENT_CONDITION:
      dot_expando_node_condition(fp, node, links);
      break;
    case ENT_CONDBOOL:
      dot_expando_node_conditional_bool(fp, node, links);
      break;
    case ENT_CONDDATE:
      dot_expando_node_conditional_date(fp, node, links);
      break;
    case ENT_CONTAINER:
      dot_expando_node_container(fp, node, links);
      break;
    case ENT_EMPTY:
      dot_expando_node_empty(fp, node, links);
      break;
    case ENT_EXPANDO:
      dot_expando_node_expando(fp, node, links);
      break;
    case ENT_PADDING:
      dot_expando_node_pad(fp, node, links);
      break;
    case ENT_TEXT:
      dot_expando_node_text(fp, node, links);
      break;
    default:
      dot_expando_node_unknown(fp, node, links);
      break;
  }
}

void dump_graphviz_expando_node(struct ExpandoNode *node)
{
  char name[256] = { 0 };
  struct ListHead links = STAILQ_HEAD_INITIALIZER(links);

  time_t now = time(NULL);
  mutt_date_localtime_format(name, sizeof(name), "%T-expando.gv", now);

  umask(022);
  FILE *fp = fopen(name, "w");
  if (!fp)
    return;

  dot_graph_header(fp);

  dot_expando_node(fp, node, &links);

  dot_graph_footer(fp, &links);
  fclose(fp);
  mutt_list_free(&links);
}
