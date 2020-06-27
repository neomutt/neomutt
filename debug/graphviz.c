/**
 * @file
 * Create a GraphViz dot file from the NeoMutt objects
 *
 * @authors
 * Copyright (C) 2018-2020 Richard Russon <rich@flatcap.org>
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
 * @page debug_graphviz Create a GraphViz dot file
 *
 * Create a GraphViz dot file from the NeoMutt objects
 */

#include "config.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include "mutt/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "conn/lib.h"
#include "lib.h"
#include "context.h"
#include "imap/private.h"
#include "maildir/private.h"
#include "mutt_globals.h"
#include "notmuch/private.h"
#include "pop/private.h"
#include "compmbox/lib.h"
#include "mbox/lib.h"
#include "nntp/lib.h"
#include "notmuch/lib.h"

// #define GV_HIDE_CONTEXT
#define GV_HIDE_CONTEXT_CONTENTS
// #define GV_HIDE_MBOX
// #define GV_HIDE_NEOMUTT
// #define GV_HIDE_CONFIG
// #define GV_HIDE_MDATA

static void dot_email(FILE *fp, const struct Email *e, struct ListHead *links);
static void dot_envelope(FILE *fp, const struct Envelope *env, struct ListHead *links);

const char *get_content_type(enum ContentType type)
{
  switch (type)
  {
    case TYPE_OTHER:
      return "TYPE_OTHER";
    case TYPE_AUDIO:
      return "TYPE_AUDIO";
    case TYPE_APPLICATION:
      return "TYPE_APPLICATION";
    case TYPE_IMAGE:
      return "TYPE_IMAGE";
    case TYPE_MESSAGE:
      return "TYPE_MESSAGE";
    case TYPE_MODEL:
      return "TYPE_MODEL";
    case TYPE_MULTIPART:
      return "TYPE_MULTIPART";
    case TYPE_TEXT:
      return "TYPE_TEXT";
    case TYPE_VIDEO:
      return "TYPE_VIDEO";
    case TYPE_ANY:
      return "TYPE_ANY";
    default:
      return "UNKNOWN";
  }
}

const char *get_content_encoding(enum ContentEncoding enc)
{
  switch (enc)
  {
    case ENC_OTHER:
      return "ENC_OTHER";
    case ENC_7BIT:
      return "ENC_7BIT";
    case ENC_8BIT:
      return "ENC_8BIT";
    case ENC_QUOTED_PRINTABLE:
      return "ENC_QUOTED_PRINTABLE";
    case ENC_BASE64:
      return "ENC_BASE64";
    case ENC_BINARY:
      return "ENC_BINARY";
    case ENC_UUENCODED:
      return "ENC_UUENCODED";
    default:
      return "UNKNOWN";
  }
}

const char *get_content_disposition(enum ContentDisposition disp)
{
  switch (disp)
  {
    case DISP_INLINE:
      return "DISP_INLINE";
    case DISP_ATTACH:
      return "DISP_ATTACH";
    case DISP_FORM_DATA:
      return "DISP_FORM_DATA";
    case DISP_NONE:
      return "DISP_NONE";
    default:
      return "UNKNOWN";
  }
}

void add_flag(struct Buffer *buf, bool is_set, const char *name)
{
  if (!buf || !name)
    return;

  if (is_set)
  {
    if (!mutt_buffer_is_empty(buf))
      mutt_buffer_addch(buf, ',');
    mutt_buffer_addstr(buf, name);
  }
}

static void dot_type_bool(FILE *fp, const char *name, bool val)
{
  static const char *values[] = { "false", "true" };
  fprintf(fp, "\t\t<tr>\n");
  fprintf(fp, "\t\t\t<td border=\"0\" align=\"left\">%s</td>\n", name);
  fprintf(fp, "\t\t\t<td border=\"0\">=</td>\n");
  fprintf(fp, "\t\t\t<td border=\"0\" align=\"left\">%s</td>\n", values[val]);
  fprintf(fp, "\t\t</tr>\n");
}

static void dot_type_char(FILE *fp, const char *name, char ch)
{
  fprintf(fp, "\t\t<tr>\n");
  fprintf(fp, "\t\t\t<td border=\"0\" align=\"left\">%s</td>\n", name);
  fprintf(fp, "\t\t\t<td border=\"0\">=</td>\n");
  fprintf(fp, "\t\t\t<td border=\"0\" align=\"left\">%c</td>\n", ch);
  fprintf(fp, "\t\t</tr>\n");
}

static void dot_type_date(char *buf, size_t buflen, time_t timestamp)
{
  mutt_date_localtime_format(buf, buflen, "%Y-%m-%d %H:%M:%S", timestamp);
}

static void dot_type_file(FILE *fp, const char *name, FILE *struct_fp)
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

static void dot_type_number(FILE *fp, const char *name, int num)
{
  fprintf(fp, "\t\t<tr>\n");
  fprintf(fp, "\t\t\t<td border=\"0\" align=\"left\">%s</td>\n", name);
  fprintf(fp, "\t\t\t<td border=\"0\">=</td>\n");
  fprintf(fp, "\t\t\t<td border=\"0\" align=\"left\">%d</td>\n", num);
  fprintf(fp, "\t\t</tr>\n");
}

static void dot_type_string_escape(char *buf, size_t buflen)
{
  for (; buf[0]; buf++)
  {
    if (buf[0] == '<')
      mutt_str_inline_replace(buf, buflen, 1, "&lt;");
    else if (buf[0] == '>')
      mutt_str_inline_replace(buf, buflen, 1, "&gt;");
    else if (buf[0] == '&')
      mutt_str_inline_replace(buf, buflen, 1, "&amp;");
  }
}

static void dot_type_string(FILE *fp, const char *name, const char *str, bool force)
{
  if ((!str || (str[0] == '\0')) && !force)
    return;

  char buf[1024] = "[NULL]";

  if (str)
  {
    mutt_str_copy(buf, str, sizeof(buf));
    dot_type_string_escape(buf, sizeof(buf));
  }

  bool quoted = ((buf[0] != '[') && (buf[0] != '*'));

  fprintf(fp, "\t\t<tr>\n");
  fprintf(fp, "\t\t\t<td border=\"0\" align=\"left\">%s</td>\n", name);
  fprintf(fp, "\t\t\t<td border=\"0\">=</td>\n");
  if (quoted)
    fprintf(fp, "\t\t\t<td border=\"0\" align=\"left\">\"%s\"</td>\n", buf);
  else
    fprintf(fp, "\t\t\t<td border=\"0\" align=\"left\">%s</td>\n", buf);
  fprintf(fp, "\t\t</tr>\n");
}

static void dot_type_umask(char *buf, size_t buflen, int umask)
{
  snprintf(buf, buflen, "0%03o", umask);
}

static void dot_ptr_name(char *buf, size_t buflen, const void *ptr)
{
  snprintf(buf, buflen, "obj_%p", ptr);
}

static void dot_ptr(FILE *fp, const char *name, void *ptr, const char *colour)
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

static void dot_add_link(struct ListHead *links, const void *src, const void *dst,
                         const char *label, bool back, const char *colour)
{
  if (!src || !dst)
    return;
  if (!colour)
    colour = "#c0c0c0";

  char obj1[16] = { 0 };
  char obj2[16] = { 0 };
  char text[256] = { 0 };
  char lstr[128] = { 0 };

  dot_ptr_name(obj1, sizeof(obj1), src);
  dot_ptr_name(obj2, sizeof(obj2), dst);

  if (label)
    snprintf(lstr, sizeof(lstr), "edgetooltip=\"%s\"", label);

  snprintf(text, sizeof(text), "%s -> %s [ %s %s color=\"%s\" ]", obj1, obj2,
           back ? "dir=back" : "", lstr, colour);
  mutt_list_insert_tail(links, mutt_str_dup(text));
}

static void dot_graph_header(FILE *fp)
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

static void dot_graph_footer(FILE *fp, struct ListHead *links)
{
  fprintf(fp, "\n");
  struct ListNode *np = NULL;
  STAILQ_FOREACH(np, links, entries)
  {
    fprintf(fp, "\t%s;\n", np->data);
  }
  fprintf(fp, "\n}\n");
}

static void dot_object_header(FILE *fp, const void *ptr, const char *name, const char *colour)
{
  char obj[16] = { 0 };
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

static void dot_object_footer(FILE *fp)
{
  fprintf(fp, "\t\t</table>>\n");
  fprintf(fp, "\t];\n");
  fprintf(fp, "\n");
}

static void dot_node(FILE *fp, void *ptr, const char *name, const char *colour)
{
  char obj[16] = { 0 };
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

static void dot_node_link(FILE *fp, void *ptr, const char *name, void *link, const char *colour)
{
  char obj[16] = { 0 };
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

static void dot_path_fs(char *buf, size_t buflen, const char *path)
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

static void dot_path_imap(char *buf, size_t buflen, const char *path)
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

static void dot_config(FILE *fp, const char *name, int type,
                       struct ConfigSubset *sub, struct ListHead *links)
{
  if (!sub)
    return;

  struct Buffer value = mutt_buffer_make(256);
  dot_object_header(fp, (void *) name, "Config", "#ffff80");
  dot_type_string(fp, "scope", sub->name, true);

  if (sub->name)
  {
    char scope[256];
    snprintf(scope, sizeof(scope), "%s:", sub->name);

    struct HashElem **list = get_elem_list(sub->cs);
    for (size_t i = 0; list[i]; i++)
    {
      struct HashElem *item = list[i];
      if ((item->type & type) == 0)
        continue;

      const char *iname = item->key.strkey;
      size_t slen = strlen(scope);
      if (mutt_str_startswith(iname, scope) != 0)
      {
        if (strchr(iname + slen, ':'))
          continue;
        if ((DTYPE(item->type) == DT_STRING) && (item->type & DT_SENSITIVE))
        {
          dot_type_string(fp, iname + slen, "***", true);
        }
        else
        {
          mutt_buffer_reset(&value);
          cs_subset_he_string_get(sub, item, &value);
          dot_type_string(fp, iname + slen, value.data, true);
        }
      }
    }
  }
  else
  {
    struct HashElem **list = get_elem_list(sub->cs);
    int i = 0;
    for (; list[i]; i++)
      ; // do nothing

    dot_type_number(fp, "count", i);
  }

  dot_object_footer(fp);
  mutt_buffer_dealloc(&value);
}

static void dot_comp(FILE *fp, struct CompressInfo *ci, struct ListHead *links)
{
  dot_object_header(fp, ci, "CompressInfo", "#c0c060");
  dot_type_string(fp, "append", ci->cmd_append, true);
  dot_type_string(fp, "close", ci->cmd_close, true);
  dot_type_string(fp, "open", ci->cmd_open, true);
  dot_object_footer(fp);
}

static void dot_mailbox_type(FILE *fp, const char *name, enum MailboxType type)
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

static void dot_mailbox_imap(FILE *fp, struct ImapMboxData *mdata, struct ListHead *links)
{
  dot_object_header(fp, mdata, "ImapMboxData", "#60c060");
  dot_type_string(fp, "name", mdata->name, true);
  dot_type_string(fp, "munge_name", mdata->munge_name, true);
  dot_type_string(fp, "real_name", mdata->real_name, true);
  dot_object_footer(fp);
}

static void dot_mailbox_maildir(FILE *fp, struct MaildirMboxData *mdata, struct ListHead *links)
{
  char buf[64] = { 0 };

  dot_object_header(fp, mdata, "MaildirMboxData", "#60c060");

  dot_type_date(buf, sizeof(buf), mdata->mtime_cur.tv_sec);
  dot_type_string(fp, "mtime_cur", buf, true);

  dot_type_umask(buf, sizeof(buf), mdata->mh_umask);
  dot_type_string(fp, "mh_umask", buf, true);
  dot_object_footer(fp);
}

static void dot_mailbox_mbox(FILE *fp, struct MboxAccountData *mdata, struct ListHead *links)
{
  char buf[64] = { 0 };

  dot_object_header(fp, mdata, "MboxAccountData", "#60c060");
  dot_ptr(fp, "fp", mdata->fp, NULL);

  dot_type_date(buf, sizeof(buf), mdata->atime.tv_sec);
  dot_type_string(fp, "atime", buf, true);

  dot_object_footer(fp);
}

static void dot_mailbox_nntp(FILE *fp, struct NntpMboxData *mdata, struct ListHead *links)
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

static void dot_mailbox_notmuch(FILE *fp, struct NmMboxData *mdata, struct ListHead *links)
{
  dot_object_header(fp, mdata, "NmMboxData", "#60c060");
  dot_type_number(fp, "db_limit", mdata->db_limit);
  dot_object_footer(fp);
}

static void dot_mailbox_pop(FILE *fp, struct PopAccountData *mdata, struct ListHead *links)
{
  dot_object_header(fp, mdata, "PopAccountData", "#60c060");
  dot_ptr(fp, "conn", mdata->conn, "#ff8080");
  dot_object_footer(fp);
}

static void dot_mailbox(FILE *fp, struct Mailbox *m, struct ListHead *links)
{
  char buf[64] = { 0 };

  dot_object_header(fp, m, "Mailbox", "#80ff80");
  dot_mailbox_type(fp, "type", m->type);
  dot_type_string(fp, "name", m->name, false);

  if ((m->type == MUTT_IMAP) || (m->type == MUTT_POP))
  {
    dot_path_imap(buf, sizeof(buf), mutt_b2s(&m->pathbuf));
    dot_type_string(fp, "pathbuf", buf, true);
    dot_path_imap(buf, sizeof(buf), m->realpath);
    dot_type_string(fp, "realpath", buf, true);
  }
  else
  {
    dot_path_fs(buf, sizeof(buf), mutt_b2s(&m->pathbuf));
    dot_type_string(fp, "pathbuf", buf, true);
    dot_path_fs(buf, sizeof(buf), m->realpath);
    dot_type_string(fp, "realpath", buf, true);
  }

#ifdef GV_HIDE_MDATA
  dot_ptr(fp, "mdata", m->mdata, NULL);
#endif
  dot_ptr(fp, "account", m->account, "#80ffff");

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

  // dot_add_link(links, m, m->mdata, false, NULL);

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
    else if (m->type == MUTT_NOTMUCH)
      dot_mailbox_notmuch(fp, m->mdata, links);

    dot_add_link(links, m, m->mdata, "Mailbox->mdata", false, NULL);
  }
#endif

  if (m->compress_info)
  {
    dot_comp(fp, m->compress_info, links);
    dot_add_link(links, m, m->compress_info, "Mailbox->compress_info", false, NULL);
  }

#ifndef GV_HIDE_CONFIG
  if (m->name)
  {
    dot_config(fp, m->name, DT_INHERIT_MBOX, m->sub, links);
    dot_add_link(links, m, m->name, "Mailbox Config", false, NULL);
  }
#endif
}

static void dot_mailbox_node(FILE *fp, struct MailboxNode *mn, struct ListHead *links)
{
  dot_node(fp, mn, "MN", "#80ff80");

  dot_mailbox(fp, mn->mailbox, links);

  dot_add_link(links, mn, mn->mailbox, "MailboxNode->mailbox", false, NULL);

  struct Buffer buf;
  mutt_buffer_init(&buf);

  char name[256] = { 0 };
  mutt_buffer_addstr(&buf, "{ rank=same ");

  dot_ptr_name(name, sizeof(name), mn);
  mutt_buffer_add_printf(&buf, "%s ", name);

  dot_ptr_name(name, sizeof(name), mn->mailbox);
  mutt_buffer_add_printf(&buf, "%s ", name);

#ifndef GV_HIDE_MDATA
  if (mn->mailbox->mdata)
  {
    dot_ptr_name(name, sizeof(name), mn->mailbox->mdata);
    mutt_buffer_add_printf(&buf, "%s ", name);
  }
#endif

#ifndef GV_HIDE_CONFIG
  if (mn->mailbox->name)
  {
    dot_ptr_name(name, sizeof(name), mn->mailbox->name);
    mutt_buffer_add_printf(&buf, "%s ", name);
  }
#endif

  mutt_buffer_addstr(&buf, "}");

  mutt_list_insert_tail(links, mutt_str_dup(buf.data));
  mutt_buffer_dealloc(&buf);
}

static void dot_mailbox_list(FILE *fp, struct MailboxList *ml, struct ListHead *links, bool abbr)
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
      dot_add_link(links, prev, np, "MailboxNode->next", false, NULL);
    prev = np;
  }
}

static void dot_connection(FILE *fp, struct Connection *c, struct ListHead *links)
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

  dot_add_link(links, c, c->inbuf, "Connection.ConnAccount", false, NULL);
}

static void dot_account_imap(FILE *fp, struct ImapAccountData *adata, struct ListHead *links)
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
    dot_add_link(links, adata, adata->conn, "ImapAccountData->conn", false, NULL);
  }
}

static void dot_account_mbox(FILE *fp, struct MboxAccountData *adata, struct ListHead *links)
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

static void dot_account_nntp(FILE *fp, struct NntpAccountData *adata, struct ListHead *links)
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
    dot_add_link(links, adata, adata->conn, "NntpAccountData->conn", false, NULL);
  }
}

static void dot_account_notmuch(FILE *fp, struct NmAccountData *adata, struct ListHead *links)
{
  dot_object_header(fp, adata, "NmAccountData", "#60c0c0");
  dot_ptr(fp, "db", adata->db, NULL);
  dot_object_footer(fp);
}

static void dot_account_pop(FILE *fp, struct PopAccountData *adata, struct ListHead *links)
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

  if (adata->conn)
  {
    dot_connection(fp, adata->conn, links);
    dot_add_link(links, adata, adata->conn, "PopAccountData->conn", false, NULL);
  }
}

static void dot_account(FILE *fp, struct Account *a, struct ListHead *links)
{
  dot_object_header(fp, a, "Account", "#80ffff");
  dot_mailbox_type(fp, "type", a->type);
  dot_type_string(fp, "name", a->name, true);
  // dot_ptr(fp, "adata", a->adata, "#60c0c0");
  dot_object_footer(fp);

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
    else if (a->type == MUTT_NOTMUCH)
      dot_account_notmuch(fp, a->adata, links);

    dot_add_link(links, a, a->adata, "Account->adata", false, NULL);
  }

#ifndef GV_HIDE_CONFIG
  if (a->name)
  {
    dot_config(fp, a->name, DT_INHERIT_ACC, a->sub, links);
    dot_add_link(links, a, a->name, "Config", false, NULL);

    char name[256] = { 0 };
    struct Buffer buf;
    mutt_buffer_init(&buf);

    mutt_buffer_addstr(&buf, "{ rank=same ");

    dot_ptr_name(name, sizeof(name), a);
    mutt_buffer_add_printf(&buf, "%s ", name);

    dot_ptr_name(name, sizeof(name), a->name);
    mutt_buffer_add_printf(&buf, "%s ", name);

    mutt_buffer_addstr(&buf, "}");
    mutt_list_insert_tail(links, mutt_str_dup(buf.data));
    mutt_buffer_dealloc(&buf);
  }
#endif

  struct MailboxNode *first = STAILQ_FIRST(&a->mailboxes);
  dot_add_link(links, a, first, "Account->mailboxes", false, NULL);
  dot_mailbox_list(fp, &a->mailboxes, links, false);
}

static void dot_account_list(FILE *fp, struct AccountList *al, struct ListHead *links)
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
      dot_add_link(links, prev, np, "Account->next", false, NULL);

    prev = np;
  }
}

static void dot_context(FILE *fp, struct Context *ctx, struct ListHead *links)
{
  dot_object_header(fp, ctx, "Context", "#ff80ff");
  dot_ptr(fp, "mailbox", ctx->mailbox, "#80ff80");
#ifdef GV_HIDE_CONTEXT_CONTENTS
  dot_type_number(fp, "vsize", ctx->vsize);
  dot_type_string(fp, "pattern", ctx->pattern, true);
  dot_type_bool(fp, "collapsed", ctx->collapsed);
#endif
  dot_object_footer(fp);
}

void dump_graphviz(const char *title)
{
  char name[256] = { 0 };
  struct ListHead links = STAILQ_HEAD_INITIALIZER(links);

  time_t now = time(NULL);
  if (title)
  {
    char date[128];
    mutt_date_localtime_format(date, sizeof(date), "%R", now);
    snprintf(name, sizeof(name), "%s-%s.gv", date, title);
  }
  else
  {
    mutt_date_localtime_format(name, sizeof(name), "%R.gv", now);
  }

  umask(022);
  FILE *fp = fopen(name, "w");
  if (!fp)
    return;

  dot_graph_header(fp);

#ifndef GV_HIDE_NEOMUTT
  dot_node(fp, NeoMutt, "NeoMutt", "#ffa500");
  dot_add_link(&links, NeoMutt, TAILQ_FIRST(&NeoMutt->accounts),
               "NeoMutt->accounts", false, NULL);
#ifndef GV_HIDE_CONFIG
  dot_config(fp, (const char *) NeoMutt->sub, 0, NeoMutt->sub, &links);
  dot_add_link(&links, NeoMutt, NeoMutt->sub, "NeoMutt Config", false, NULL);
  struct Buffer buf = mutt_buffer_make(256);
  char obj1[16] = { 0 };
  char obj2[16] = { 0 };
  dot_ptr_name(obj1, sizeof(obj1), NeoMutt);
  dot_ptr_name(obj2, sizeof(obj2), NeoMutt->sub);
  mutt_buffer_printf(&buf, "{ rank=same %s %s }", obj1, obj2);
  mutt_list_insert_tail(&links, mutt_str_dup(mutt_b2s(&buf)));
  mutt_buffer_dealloc(&buf);
#endif
#endif

  dot_account_list(fp, &NeoMutt->accounts, &links);

#ifndef GV_HIDE_CONTEXT
  if (Context)
    dot_context(fp, Context, &links);

#ifndef GV_HIDE_NEOMUTT
  /* Globals */
  fprintf(fp, "\t{ rank=same ");
  if (Context)
  {
    dot_ptr_name(name, sizeof(name), Context);
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

static void dot_parameter_list(FILE *fp, const char *name, const struct ParameterList *pl)
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

static void dot_content(FILE *fp, struct Content *cont, struct ListHead *links)
{
  struct Buffer buf = mutt_buffer_make(256);

  dot_object_header(fp, cont, "Content", "#800080");

  dot_type_number(fp, "hibin", cont->hibin);
  dot_type_number(fp, "lobin", cont->lobin);
  dot_type_number(fp, "nulbin", cont->nulbin);
  dot_type_number(fp, "crlf", cont->crlf);
  dot_type_number(fp, "ascii", cont->ascii);
  dot_type_number(fp, "linemax", cont->linemax);

#define ADD_BOOL(F) add_flag(&buf, cont->F, #F)
  ADD_BOOL(space);
  ADD_BOOL(binary);
  ADD_BOOL(from);
  ADD_BOOL(dot);
  ADD_BOOL(cr);
#undef ADD_BOOL

  dot_object_footer(fp);

  mutt_buffer_dealloc(&buf);
}

static void dot_attach_ptr(FILE *fp, struct AttachPtr *aptr, struct ListHead *links)
{
  struct Buffer buf = mutt_buffer_make(256);

  dot_object_header(fp, aptr, "AttachPtr", "#ff0000");

  dot_type_file(fp, "fp", aptr->fp);

  dot_type_string(fp, "parent_type", get_content_type(aptr->parent_type), false);

  dot_type_number(fp, "level", aptr->level);
  dot_type_number(fp, "num", aptr->num);

  dot_type_bool(fp, "unowned", aptr->unowned);
  dot_type_bool(fp, "decrypted", aptr->decrypted);

  dot_object_footer(fp);

  dot_add_link(links, aptr->content, aptr, "AttachPtr->content", true, NULL);

  mutt_buffer_dealloc(&buf);
}

static void dot_body(FILE *fp, const struct Body *b, struct ListHead *links, bool link_next)
{
  struct Buffer buf = mutt_buffer_make(256);

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

  dot_type_string(fp, "type", get_content_type(b->type), true);
  dot_type_string(fp, "encoding", get_content_encoding(b->encoding), true);
  dot_type_string(fp, "disposition", get_content_disposition(b->disposition), true);

  if (b->stamp != 0)
  {
    char arr[64];
    dot_type_date(arr, sizeof(arr), b->stamp);
    dot_type_string(fp, "stamp", arr, true);
  }

#define ADD_BOOL(F) add_flag(&buf, b->F, #F)
  ADD_BOOL(attach_qualifies);
  ADD_BOOL(badsig);
  ADD_BOOL(collapsed);
  ADD_BOOL(deleted);
  ADD_BOOL(force_charset);
  ADD_BOOL(goodsig);
  ADD_BOOL(is_autocrypt);
  ADD_BOOL(noconv);
  ADD_BOOL(tagged);
  ADD_BOOL(unlink);
  ADD_BOOL(use_disp);
  ADD_BOOL(warnsig);
#undef ADD_BOOL
  dot_type_string(fp, "bools", mutt_buffer_is_empty(&buf) ? "[NONE]" : mutt_b2s(&buf), true);

  dot_type_number(fp, "attach_count", b->attach_count);
  dot_type_number(fp, "hdr_offset", b->hdr_offset);
  dot_type_number(fp, "length", b->length);
  dot_type_number(fp, "offset", b->offset);

  dot_object_footer(fp);

  if (!TAILQ_EMPTY(&b->parameter))
  {
    dot_parameter_list(fp, "parameter", &b->parameter);
    dot_add_link(links, b, &b->parameter, "Body->mime_headers", false, NULL);
  }

  if (b->mime_headers)
  {
    dot_envelope(fp, b->mime_headers, links);
    dot_add_link(links, b, b->mime_headers, "Body->mime_headers", false, NULL);
  }

  if (b->email)
  {
    dot_email(fp, b->email, links);
    dot_add_link(links, b, b->email, "Body->email", false, NULL);
  }

  if (b->parts)
  {
    if (!b->email)
      dot_body(fp, b->parts, links, true);
    dot_add_link(links, b, b->parts, "Body->parts", false, "#ff0000");
  }

  if (b->next && link_next)
  {
    char name[256] = { 0 };
    mutt_buffer_reset(&buf);

    mutt_buffer_addstr(&buf, "{ rank=same ");

    dot_ptr_name(name, sizeof(name), b);
    mutt_buffer_add_printf(&buf, "%s ", name);

    for (; b->next; b = b->next)
    {
      dot_body(fp, b->next, links, false);
      dot_add_link(links, b, b->next, "Body->next", false, "#008000");

      dot_ptr_name(name, sizeof(name), b->next);
      mutt_buffer_add_printf(&buf, "%s ", name);
    }

    mutt_buffer_addstr(&buf, "}");
    mutt_list_insert_tail(links, mutt_str_dup(buf.data));
  }
  else
  {
    if (b->content)
    {
      dot_content(fp, b->content, links);
      dot_add_link(links, b, b->content, "Body->content", false, NULL);
    }

    if (b->aptr)
    {
      dot_attach_ptr(fp, b->aptr, links);
      dot_add_link(links, b, b->aptr, "Body->aptr", false, NULL);
    }
  }

  mutt_buffer_dealloc(&buf);
}

static void dot_list_head(FILE *fp, const char *name, const struct ListHead *list)
{
  if (!list || !name)
    return;
  if (STAILQ_EMPTY(list))
    return;

  struct Buffer buf = mutt_buffer_make(256);

  struct ListNode *np = NULL;
  STAILQ_FOREACH(np, list, entries)
  {
    if (!mutt_buffer_is_empty(&buf))
      mutt_buffer_addch(&buf, ',');
    mutt_buffer_addstr(&buf, np->data);
  }

  dot_type_string(fp, name, mutt_b2s(&buf), false);
}

static void dot_addr_list(FILE *fp, const char *name,
                          const struct AddressList *al, struct ListHead *links)
{
  if (!al)
    return;
  if (TAILQ_EMPTY(al))
    return;

  char buf[1024] = { 0 };

  mutt_addrlist_write(al, buf, sizeof(buf), true);
  dot_type_string(fp, name, buf, false);
}

static void dot_envelope(FILE *fp, const struct Envelope *env, struct ListHead *links)
{
  struct Buffer buf = mutt_buffer_make(256);

  dot_object_header(fp, env, "Envelope", "#ffff00");

#define ADD_FLAG(F) add_flag(&buf, (env->changed & F), #F)
  ADD_FLAG(MUTT_ENV_CHANGED_IRT);
  ADD_FLAG(MUTT_ENV_CHANGED_REFS);
  ADD_FLAG(MUTT_ENV_CHANGED_XLABEL);
  ADD_FLAG(MUTT_ENV_CHANGED_SUBJECT);
#undef ADD_BOOL
  dot_type_string(fp, "changed",
                  mutt_buffer_is_empty(&buf) ? "[NONE]" : mutt_b2s(&buf), true);

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
  dot_type_string(fp, "message_id", env->message_id, false);
  dot_type_string(fp, "newsgroups", env->newsgroups, false);
  dot_type_string(fp, "organization", env->organization, false);
  dot_type_string(fp, "real_subj", env->real_subj, false);
  dot_type_string(fp, "spam", mutt_b2s(&env->spam), false);
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

  mutt_buffer_dealloc(&buf);
}

static void dot_email(FILE *fp, const struct Email *e, struct ListHead *links)
{
  struct Buffer buf = mutt_buffer_make(256);
  char arr[256];

  dot_object_header(fp, e, "Email", "#ff80ff");

  dot_type_string(fp, "path", e->path, true);

#define ADD_BOOL(F) add_flag(&buf, e->F, #F)
  ADD_BOOL(active);
  ADD_BOOL(attach_del);
  ADD_BOOL(attach_valid);
  ADD_BOOL(changed);
  ADD_BOOL(collapsed);
  ADD_BOOL(deleted);
  ADD_BOOL(display_subject);
  ADD_BOOL(expired);
  ADD_BOOL(flagged);
  ADD_BOOL(limited);
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
#undef ADD_BOOL
  dot_type_string(fp, "bools", mutt_buffer_is_empty(&buf) ? "[NONE]" : mutt_b2s(&buf), true);

  mutt_buffer_reset(&buf);
#define ADD_BOOL(F) add_flag(&buf, (e->security & F), #F)
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
  dot_type_string(fp, "security",
                  mutt_buffer_is_empty(&buf) ? "[NONE]" : mutt_b2s(&buf), true);

  dot_type_number(fp, "num_hidden", e->num_hidden);
  dot_type_number(fp, "offset", e->offset);
  dot_type_number(fp, "lines", e->lines);
  dot_type_number(fp, "index", e->index);
  dot_type_number(fp, "msgno", e->msgno);
  dot_type_number(fp, "vnum", e->vnum);
  dot_type_number(fp, "score", e->score);
  dot_type_number(fp, "attach_total", e->attach_total);

  dot_type_string(fp, "maildir_flags", e->maildir_flags, false);

  if (e->date_sent != 0)
  {
    char zone[32];
    dot_type_date(arr, sizeof(arr), e->date_sent);
    snprintf(zone, sizeof(zone), " (%c%02u%02u)", e->zoccident ? '-' : '+',
             e->zhours, e->zminutes);
    mutt_str_cat(arr, sizeof(arr), zone);
    dot_type_string(fp, "date_sent", arr, false);
  }

  if (e->received != 0)
  {
    dot_type_date(arr, sizeof(arr), e->received);
    dot_type_string(fp, "received", arr, false);
  }

  dot_object_footer(fp);

  if (e->content)
  {
    dot_body(fp, e->content, links, true);
    dot_add_link(links, e, e->content, "Email->content", false, NULL);
  }

  if (e->env)
  {
    dot_envelope(fp, e->env, links);
    dot_add_link(links, e, e->env, "Email->env", false, NULL);

    mutt_buffer_reset(&buf);
    mutt_buffer_addstr(&buf, "{ rank=same ");

    dot_ptr_name(arr, sizeof(arr), e);
    mutt_buffer_add_printf(&buf, "%s ", arr);

    dot_ptr_name(arr, sizeof(arr), e->env);
    mutt_buffer_add_printf(&buf, "%s ", arr);

    mutt_buffer_addstr(&buf, "}");

    mutt_list_insert_tail(links, mutt_str_dup(buf.data));
  }

  // struct TagList tags;

  mutt_buffer_dealloc(&buf);
}

void dump_graphviz_email(const struct Email *e)
{
  char name[256] = { 0 };
  struct ListHead links = STAILQ_HEAD_INITIALIZER(links);

  time_t now = time(NULL);
  mutt_date_localtime_format(name, sizeof(name), "%R.gv", now);

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
