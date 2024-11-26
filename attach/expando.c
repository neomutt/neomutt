/**
 * @file
 * Attach Expando definitions
 *
 * @authors
 * Copyright (C) 2024 Richard Russon <rich@flatcap.org>
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
 * @page attach_expando Attach Expando definitions
 *
 * Attach Expando definitions
 */

#include <stdbool.h>
#include <stdio.h>
#include "mutt/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "expando.h"
#include "lib.h"
#include "color/lib.h"
#include "expando/lib.h"
#include "index/lib.h"
#include "attach.h"
#include "muttlib.h"

static void attach_F(const struct ExpandoNode *node, void *data,
                     MuttFormatFlags flags, struct Buffer *buf);

/**
 * attach_c - Attachment: Requires conversion flag - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
static void attach_c(const struct ExpandoNode *node, void *data,
                     MuttFormatFlags flags, struct Buffer *buf)
{
  const struct AttachPtr *aptr = data;

  // NOTE(g0mb4): use $to_chars?
  const char *s = ((aptr->body->type != TYPE_TEXT) || aptr->body->noconv) ? "n" : "c";
  buf_strcpy(buf, s);
}

/**
 * attach_C - Attachment: Charset - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
static void attach_C(const struct ExpandoNode *node, void *data,
                     MuttFormatFlags flags, struct Buffer *buf)
{
  const struct AttachPtr *aptr = data;

  char tmp[128] = { 0 };

  if (mutt_is_text_part(aptr->body) && mutt_body_get_charset(aptr->body, tmp, sizeof(tmp)))
  {
    buf_strcpy(buf, tmp);
  }
}

/**
 * attach_d - Attachment: Description - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
static void attach_d(const struct ExpandoNode *node, void *data,
                     MuttFormatFlags flags, struct Buffer *buf)
{
  const struct AttachPtr *aptr = data;

  const struct Expando *c_message_format = cs_subset_expando(NeoMutt->sub, "message_format");
  if (aptr->body->description)
  {
    const char *s = aptr->body->description;
    buf_strcpy(buf, s);
    return;
  }

  if (mutt_is_message_type(aptr->body->type, aptr->body->subtype) &&
      c_message_format && aptr->body->email)
  {
    mutt_make_string(buf, -1, c_message_format, NULL, -1, aptr->body->email,
                     MUTT_FORMAT_FORCESUBJ | MUTT_FORMAT_ARROWCURSOR, NULL);

    return;
  }

  if (!aptr->body->d_filename && !aptr->body->filename)
  {
    const char *s = "<no description>";
    buf_strcpy(buf, s);
    return;
  }

  attach_F(node, data, flags, buf);
}

/**
 * attach_D_num - Attachment: Deleted - Implements ExpandoRenderData::get_number() - @ingroup expando_get_number_api
 */
static long attach_D_num(const struct ExpandoNode *node, void *data, MuttFormatFlags flags)
{
  const struct AttachPtr *aptr = data;
  return aptr->body->deleted;
}

/**
 * attach_D - Attachment: Deleted - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
static void attach_D(const struct ExpandoNode *node, void *data,
                     MuttFormatFlags flags, struct Buffer *buf)
{
  const struct AttachPtr *aptr = data;

  // NOTE(g0mb4): use $to_chars?
  const char *s = aptr->body->deleted ? "D" : " ";
  buf_strcpy(buf, s);
}

/**
 * attach_e - Attachment: MIME type - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
static void attach_e(const struct ExpandoNode *node, void *data,
                     MuttFormatFlags flags, struct Buffer *buf)
{
  const struct AttachPtr *aptr = data;

  const char *s = ENCODING(aptr->body->encoding);
  buf_strcpy(buf, s);
}

/**
 * attach_f - Attachment: Filename - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
static void attach_f(const struct ExpandoNode *node, void *data,
                     MuttFormatFlags flags, struct Buffer *buf)
{
  const struct AttachPtr *aptr = data;

  if (aptr->body->filename && (*aptr->body->filename == '/'))
  {
    struct Buffer *path = buf_pool_get();

    buf_strcpy(path, aptr->body->filename);
    buf_pretty_mailbox(path);
    buf_copy(buf, path);
    buf_pool_release(&path);
  }
  else
  {
    const char *s = aptr->body->filename;
    buf_strcpy(buf, s);
  }
}

/**
 * attach_F - Attachment: Filename in header - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
static void attach_F(const struct ExpandoNode *node, void *data,
                     MuttFormatFlags flags, struct Buffer *buf)
{
  const struct AttachPtr *aptr = data;

  if (aptr->body->d_filename)
  {
    const char *s = aptr->body->d_filename;
    buf_strcpy(buf, s);
    return;
  }

  attach_f(node, data, flags, buf);
}

/**
 * attach_I - Attachment: Disposition flag - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
static void attach_I(const struct ExpandoNode *node, void *data,
                     MuttFormatFlags flags, struct Buffer *buf)
{
  const struct AttachPtr *aptr = data;

  // NOTE(g0mb4): use $to_chars?
  static const char dispchar[] = { 'I', 'A', 'F', '-' };
  char ch;

  if (aptr->body->disposition < sizeof(dispchar))
  {
    ch = dispchar[aptr->body->disposition];
  }
  else
  {
    mutt_debug(LL_DEBUG1, "ERROR: invalid content-disposition %d\n", aptr->body->disposition);
    ch = '!';
  }

  buf_printf(buf, "%c", ch);
}

/**
 * attach_m - Attachment: Major MIME type - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
static void attach_m(const struct ExpandoNode *node, void *data,
                     MuttFormatFlags flags, struct Buffer *buf)
{
  const struct AttachPtr *aptr = data;

  const char *s = TYPE(aptr->body);
  buf_strcpy(buf, s);
}

/**
 * attach_M - Attachment: MIME subtype - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
static void attach_M(const struct ExpandoNode *node, void *data,
                     MuttFormatFlags flags, struct Buffer *buf)
{
  const struct AttachPtr *aptr = data;

  const char *s = aptr->body->subtype;
  buf_strcpy(buf, s);
}

/**
 * attach_n_num - Attachment: Index number - Implements ExpandoRenderData::get_number() - @ingroup expando_get_number_api
 */
static long attach_n_num(const struct ExpandoNode *node, void *data, MuttFormatFlags flags)
{
  const struct AttachPtr *aptr = data;

  return aptr->num + 1;
}

/**
 * attach_Q_num - Attachment: Attachment counting - Implements ExpandoRenderData::get_number() - @ingroup expando_get_number_api
 */
static long attach_Q_num(const struct ExpandoNode *node, void *data, MuttFormatFlags flags)
{
  const struct AttachPtr *aptr = data;
  return aptr->body->attach_qualifies;
}

/**
 * attach_Q - Attachment: Attachment counting - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
static void attach_Q(const struct ExpandoNode *node, void *data,
                     MuttFormatFlags flags, struct Buffer *buf)
{
  const struct AttachPtr *aptr = data;

  // NOTE(g0mb4): use $to_chars?
  const char *s = aptr->body->attach_qualifies ? "Q" : " ";
  buf_strcpy(buf, s);
}

/**
 * attach_s_num - Attachment: Size - Implements ExpandoRenderData::get_number() - @ingroup expando_get_number_api
 */
static long attach_s_num(const struct ExpandoNode *node, void *data, MuttFormatFlags flags)
{
  const struct AttachPtr *aptr = data;

  if (aptr->body->filename && (flags & MUTT_FORMAT_STAT_FILE))
    return mutt_file_get_size(aptr->body->filename);

  return aptr->body->length;
}

/**
 * attach_s - Attachment: Size - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
static void attach_s(const struct ExpandoNode *node, void *data,
                     MuttFormatFlags flags, struct Buffer *buf)
{
  const struct AttachPtr *aptr = data;

  char tmp[128] = { 0 };

  size_t l = 0;
  if (aptr->body->filename && (flags & MUTT_FORMAT_STAT_FILE))
  {
    l = mutt_file_get_size(aptr->body->filename);
  }
  else
  {
    l = aptr->body->length;
  }

  mutt_str_pretty_size(tmp, sizeof(tmp), l);
  buf_strcpy(buf, tmp);
}

/**
 * attach_t_num - Attachment: Is Tagged - Implements ExpandoRenderData::get_number() - @ingroup expando_get_number_api
 */
static long attach_t_num(const struct ExpandoNode *node, void *data, MuttFormatFlags flags)
{
  const struct AttachPtr *aptr = data;
  return aptr->body->tagged;
}

/**
 * attach_t - Attachment: Is Tagged - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
static void attach_t(const struct ExpandoNode *node, void *data,
                     MuttFormatFlags flags, struct Buffer *buf)
{
  const struct AttachPtr *aptr = data;

  // NOTE(g0mb4): use $to_chars?
  const char *s = aptr->body->tagged ? "*" : " ";
  buf_strcpy(buf, s);
}

/**
 * attach_T - Attachment: Tree characters - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
static void attach_T(const struct ExpandoNode *node, void *data,
                     MuttFormatFlags flags, struct Buffer *buf)
{
  const struct AttachPtr *aptr = data;

  node_expando_set_color(node, MT_COLOR_TREE);
  node_expando_set_has_tree(node, true);
  const char *s = aptr->tree;
  buf_strcpy(buf, s);
}

/**
 * attach_u_num - Attachment: Unlink flag - Implements ExpandoRenderData::get_number() - @ingroup expando_get_number_api
 */
static long attach_u_num(const struct ExpandoNode *node, void *data, MuttFormatFlags flags)
{
  const struct AttachPtr *aptr = data;
  return aptr->body->unlink;
}

/**
 * attach_u - Attachment: Unlink flag - Implements ExpandoRenderData::get_string() - @ingroup expando_get_string_api
 */
static void attach_u(const struct ExpandoNode *node, void *data,
                     MuttFormatFlags flags, struct Buffer *buf)
{
  const struct AttachPtr *aptr = data;

  // NOTE(g0mb4): use $to_chars?
  const char *s = aptr->body->unlink ? "-" : " ";
  buf_strcpy(buf, s);
}

/**
 * attach_X_num - Attachment: Number of MIME parts - Implements ExpandoRenderData::get_number() - @ingroup expando_get_number_api
 */
static long attach_X_num(const struct ExpandoNode *node, void *data, MuttFormatFlags flags)
{
  const struct AttachPtr *aptr = data;
  const struct Body *body = aptr->body;

  return body->attach_count + body->attach_qualifies;
}

/**
 * AttachRenderData - Callbacks for Attachment Expandos
 *
 * @sa AttachFormatDef, ExpandoDataAttach, ExpandoDataBody, ExpandoDataGlobal
 */
const struct ExpandoRenderData AttachRenderData[] = {
  // clang-format off
  { ED_ATTACH, ED_ATT_CHARSET,          attach_C,     NULL },
  { ED_BODY,   ED_BOD_CHARSET_CONVERT,  attach_c,     NULL },
  { ED_BODY,   ED_BOD_DELETED,          attach_D,     attach_D_num },
  { ED_BODY,   ED_BOD_DESCRIPTION,      attach_d,     NULL },
  { ED_BODY,   ED_BOD_MIME_ENCODING,    attach_e,     NULL },
  { ED_BODY,   ED_BOD_FILE,             attach_f,     NULL },
  { ED_BODY,   ED_BOD_FILE_DISPOSITION, attach_F,     NULL },
  { ED_BODY,   ED_BOD_DISPOSITION,      attach_I,     NULL },
  { ED_BODY,   ED_BOD_MIME_MAJOR,       attach_m,     NULL },
  { ED_BODY,   ED_BOD_MIME_MINOR,       attach_M,     NULL },
  { ED_ATTACH, ED_ATT_NUMBER,           NULL,         attach_n_num },
  { ED_BODY,   ED_BOD_ATTACH_QUALIFIES, attach_Q,     attach_Q_num },
  { ED_BODY,   ED_BOD_FILE_SIZE,        attach_s,     attach_s_num },
  { ED_BODY,   ED_BOD_TAGGED,           attach_t,     attach_t_num },
  { ED_ATTACH, ED_ATT_TREE,             attach_T,     NULL },
  { ED_BODY,   ED_BOD_UNLINK,           attach_u,     attach_u_num },
  { ED_BODY,   ED_BOD_ATTACH_COUNT,     NULL,         attach_X_num },
  { -1, -1, NULL, NULL },
  // clang-format on
};
