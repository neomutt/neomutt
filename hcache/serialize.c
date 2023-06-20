/**
 * @file
 * Email-object serialiser
 *
 * @authors
 * Copyright (C) 2004 Thomas Glanzmann <sithglan@stud.uni-erlangen.de>
 * Copyright (C) 2004 Tobias Werth <sitowert@stud.uni-erlangen.de>
 * Copyright (C) 2004 Brian Fundakowski Feldman <green@FreeBSD.org>
 * Copyright (C) 2016-2019 Pietro Cerutti <gahr@gahr.ch>
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
 * @page hc_serial Email-object serialiser
 *
 * Email-object serialiser
 */

#include "config.h"
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <sys/types.h>
#include "mutt/lib.h"
#include "address/lib.h"
#include "config/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "serialize.h"

/**
 * lazy_realloc - Reallocate some memory
 * @param[in] ptr Pointer to resize
 * @param[in] size Minimum size
 *
 * The minimum size is 4KiB to avoid repeated resizing.
 */
void lazy_realloc(void *ptr, size_t size)
{
  void **p = (void **) ptr;

  if (p && (size < 4096))
    return;

  mutt_mem_realloc(ptr, size);
}

/**
 * serial_dump_int - Pack an integer into a binary blob
 * @param[in]     i   Integer to save
 * @param[in]     d   Binary blob to add to
 * @param[in,out] off Offset into the blob
 * @retval ptr End of the newly packed binary
 */
unsigned char *serial_dump_int(const unsigned int i, unsigned char *d, int *off)
{
  lazy_realloc(&d, *off + sizeof(int));
  memcpy(d + *off, &i, sizeof(int));
  (*off) += sizeof(int);

  return d;
}

/**
 * serial_dump_uint32_t - Pack a uint32_t into a binary blob
 * @param[in]     s   uint32_t to save
 * @param[in]     d   Binary blob to add to
 * @param[in,out] off Offset into the blob
 * @retval ptr End of the newly packed binary
 */
unsigned char *serial_dump_uint32_t(const uint32_t s, unsigned char *d, int *off)
{
  lazy_realloc(&d, *off + sizeof(uint32_t));
  memcpy(d + *off, &s, sizeof(uint32_t));
  (*off) += sizeof(uint32_t);

  return d;
}

/**
 * serial_dump_uint64_t - Pack a uint64_t into a binary blob
 * @param[in]     s   uint64_t to save
 * @param[in]     d   Binary blob to add to
 * @param[in,out] off Offset into the blob
 * @retval ptr End of the newly packed binary
 */
unsigned char *serial_dump_uint64_t(const uint64_t s, unsigned char *d, int *off)
{
  lazy_realloc(&d, *off + sizeof(uint64_t));
  memcpy(d + *off, &s, sizeof(uint64_t));
  (*off) += sizeof(uint64_t);

  return d;
}

/**
 * serial_restore_int - Unpack an integer from a binary blob
 * @param[in]     i   Integer to write to
 * @param[in]     d   Binary blob to read from
 * @param[in,out] off Offset into the blob
 */
void serial_restore_int(unsigned int *i, const unsigned char *d, int *off)
{
  memcpy(i, d + *off, sizeof(int));
  (*off) += sizeof(int);
}

/**
 * serial_restore_uint32_t - Unpack an uint32_t from a binary blob
 * @param[in]     s   uint32_t to write to
 * @param[in]     d   Binary blob to read from
 * @param[in,out] off Offset into the blob
 */
void serial_restore_uint32_t(uint32_t *s, const unsigned char *d, int *off)
{
  memcpy(s, d + *off, sizeof(uint32_t));
  (*off) += sizeof(uint32_t);
}

/**
 * serial_restore_uint64_t - Unpack an uint64_t from a binary blob
 * @param[in]     s   uint64_t to write to
 * @param[in]     d   Binary blob to read from
 * @param[in,out] off Offset into the blob
 */
void serial_restore_uint64_t(uint64_t *s, const unsigned char *d, int *off)
{
  memcpy(s, d + *off, sizeof(uint64_t));
  (*off) += sizeof(uint64_t);
}

/**
 * serial_dump_char_size - Pack a fixed-length string into a binary blob
 * @param[in]     c       String to pack
 * @param[in]     d       Binary blob to add to
 * @param[in]     size    Size of the string
 * @param[in,out] off     Offset into the blob
 * @param[in]     convert If true, the strings will be converted to utf-8
 * @retval ptr End of the newly packed binary
 */
unsigned char *serial_dump_char_size(const char *c, ssize_t size,
                                     unsigned char *d, int *off, bool convert)
{
  char *p = NULL;

  if (!c || (*c == '\0') || (size == 0))
  {
    return serial_dump_int(0, d, off);
  }

  if (convert && !mutt_str_is_ascii(c, size))
  {
    p = mutt_strn_dup(c, size);
    if (mutt_ch_convert_string(&p, cc_charset(), "utf-8", MUTT_ICONV_NO_FLAGS) == 0)
    {
      size = mutt_str_len(p) + 1;
    }
  }

  d = serial_dump_int(size, d, off);
  lazy_realloc(&d, *off + size);
  memcpy(d + *off, p ? p : c, size);
  *off += size;

  FREE(&p);

  return d;
}

/**
 * serial_dump_char - Pack a variable-length string into a binary blob
 * @param[in]     c       String to pack
 * @param[in]     d       Binary blob to add to
 * @param[in,out] off     Offset into the blob
 * @param[in]     convert If true, the strings will be converted to utf-8
 * @retval ptr End of the newly packed binary
 */
unsigned char *serial_dump_char(const char *c, unsigned char *d, int *off, bool convert)
{
  return serial_dump_char_size(c, mutt_str_len(c) + 1, d, off, convert);
}

/**
 * serial_restore_char - Unpack a variable-length string from a binary blob
 * @param[out]    c       Store the unpacked string here
 * @param[in]     d       Binary blob to read from
 * @param[in,out] off     Offset into the blob
 * @param[in]     convert If true, the strings will be converted to utf-8
 */
void serial_restore_char(char **c, const unsigned char *d, int *off, bool convert)
{
  unsigned int size = 0;
  serial_restore_int(&size, d, off);

  if (size == 0)
  {
    *c = NULL;
    return;
  }

  *c = mutt_mem_malloc(size);
  memcpy(*c, d + *off, size);
  if (convert && !mutt_str_is_ascii(*c, size))
  {
    char *tmp = mutt_str_dup(*c);
    if (mutt_ch_convert_string(&tmp, "utf-8", cc_charset(), MUTT_ICONV_NO_FLAGS) == 0)
    {
      FREE(c);
      *c = tmp;
    }
    else
    {
      FREE(&tmp);
    }
  }
  *off += size;
}

/**
 * serial_dump_address - Pack an Address into a binary blob
 * @param[in]     al      AddressList to pack
 * @param[in]     d       Binary blob to add to
 * @param[in,out] off     Offset into the blob
 * @param[in]     convert If true, the strings will be converted to utf-8
 * @retval ptr End of the newly packed binary
 */
unsigned char *serial_dump_address(const struct AddressList *al,
                                   unsigned char *d, int *off, bool convert)
{
  unsigned int counter = 0;
  unsigned int start_off = *off;

  d = serial_dump_int(0xdeadbeef, d, off);

  struct Address *a = NULL;
  TAILQ_FOREACH(a, al, entries)
  {
    d = serial_dump_buffer(a->personal, d, off, convert);
    d = serial_dump_buffer(a->mailbox, d, off, convert);
    d = serial_dump_int(a->group, d, off);
    counter++;
  }

  memcpy(d + start_off, &counter, sizeof(int));

  return d;
}

/**
 * serial_restore_address - Unpack an Address from a binary blob
 * @param[out]    al      Store the unpacked AddressList here
 * @param[in]     d       Binary blob to read from
 * @param[in,out] off     Offset into the blob
 * @param[in]     convert If true, the strings will be converted from utf-8
 */
void serial_restore_address(struct AddressList *al, const unsigned char *d,
                            int *off, bool convert)
{
  unsigned int counter = 0;
  unsigned int g = 0;

  serial_restore_int(&counter, d, off);

  while (counter)
  {
    struct Address *a = mutt_addr_new();

    a->personal = buf_new(NULL);
    serial_restore_buffer(a->personal, d, off, convert);
    if (buf_is_empty(a->personal))
    {
      buf_free(&a->personal);
    }

    a->mailbox = buf_new(NULL);
    serial_restore_buffer(a->mailbox, d, off, false);
    if (buf_is_empty(a->mailbox))
    {
      buf_free(&a->mailbox);
    }

    serial_restore_int(&g, d, off);
    a->group = !!g;
    mutt_addrlist_append(al, a);
    counter--;
  }
}

/**
 * serial_dump_stailq - Pack a STAILQ into a binary blob
 * @param[in]     l       List to read from
 * @param[in]     d       Binary blob to add to
 * @param[in,out] off     Offset into the blob
 * @param[in]     convert If true, the strings will be converted to utf-8
 * @retval ptr End of the newly packed binary
 */
unsigned char *serial_dump_stailq(const struct ListHead *l, unsigned char *d,
                                  int *off, bool convert)
{
  unsigned int counter = 0;
  unsigned int start_off = *off;

  d = serial_dump_int(0xdeadbeef, d, off);

  struct ListNode *np = NULL;
  STAILQ_FOREACH(np, l, entries)
  {
    d = serial_dump_char(np->data, d, off, convert);
    counter++;
  }

  memcpy(d + start_off, &counter, sizeof(int));

  return d;
}

/**
 * serial_restore_stailq - Unpack a STAILQ from a binary blob
 * @param[in]     l       List to add to
 * @param[in]     d       Binary blob to read from
 * @param[in,out] off     Offset into the blob
 * @param[in]     convert If true, the strings will be converted from utf-8
 */
void serial_restore_stailq(struct ListHead *l, const unsigned char *d, int *off, bool convert)
{
  unsigned int counter = 0;

  serial_restore_int(&counter, d, off);

  struct ListNode *np = NULL;
  while (counter)
  {
    np = mutt_list_insert_tail(l, NULL);
    serial_restore_char(&np->data, d, off, convert);
    counter--;
  }
}

/**
 * serial_dump_buffer - Pack a Buffer into a binary blob
 * @param[in]     buf     Buffer to pack
 * @param[in]     d       Binary blob to add to
 * @param[in,out] off     Offset into the blob
 * @param[in]     convert If true, the strings will be converted to utf-8
 * @retval ptr End of the newly packed binary
 */
unsigned char *serial_dump_buffer(const struct Buffer *buf, unsigned char *d,
                                  int *off, bool convert)
{
  if (!buf)
  {
    d = serial_dump_int(0, d, off);
    return d;
  }

  d = serial_dump_int(1, d, off);

  d = serial_dump_char(buf->data, d, off, convert);

  return d;
}

/**
 * serial_restore_buffer - Unpack a Buffer from a binary blob
 * @param[out]    buf     Store the unpacked Buffer here
 * @param[in]     d       Binary blob to read from
 * @param[in,out] off     Offset into the blob
 * @param[in]     convert If true, the strings will be converted from utf-8
 */
void serial_restore_buffer(struct Buffer *buf, const unsigned char *d, int *off, bool convert)
{
  unsigned int used = 0;
  serial_restore_int(&used, d, off);
  if (used == 0)
    return;

  char *str = NULL;
  serial_restore_char(&str, d, off, convert);
  FREE(&str);

  buf_addstr(buf, str);
}

/**
 * serial_dump_parameter - Pack a Parameter into a binary blob
 * @param[in]     pl      Parameter to pack
 * @param[in]     d       Binary blob to add to
 * @param[in,out] off     Offset into the blob
 * @param[in]     convert If true, the strings will be converted to utf-8
 * @retval ptr End of the newly packed binary
 */
unsigned char *serial_dump_parameter(const struct ParameterList *pl,
                                     unsigned char *d, int *off, bool convert)
{
  unsigned int counter = 0;
  unsigned int start_off = *off;

  d = serial_dump_int(0xdeadbeef, d, off);

  struct Parameter *np = NULL;
  TAILQ_FOREACH(np, pl, entries)
  {
    d = serial_dump_char(np->attribute, d, off, false);
    d = serial_dump_char(np->value, d, off, convert);
    counter++;
  }

  memcpy(d + start_off, &counter, sizeof(int));

  return d;
}

/**
 * serial_restore_parameter - Unpack a Parameter from a binary blob
 * @param[in]     pl      Store the unpacked Parameter here
 * @param[in]     d       Binary blob to read from
 * @param[in,out] off     Offset into the blob
 * @param[in]     convert If true, the strings will be converted from utf-8
 */
void serial_restore_parameter(struct ParameterList *pl, const unsigned char *d,
                              int *off, bool convert)
{
  unsigned int counter = 0;

  serial_restore_int(&counter, d, off);

  struct Parameter *np = NULL;
  while (counter)
  {
    np = mutt_param_new();
    serial_restore_char(&np->attribute, d, off, false);
    serial_restore_char(&np->value, d, off, convert);
    TAILQ_INSERT_TAIL(pl, np, entries);
    counter--;
  }
}

/**
 * body_pack_flags - Pack the Body flags into a uint32_t
 * @param b Body to pack
 * @retval num uint32_t of packed flags
 *
 * @note Order of packing must match body_unpack_flags()
 */
static inline uint32_t body_pack_flags(const struct Body *b)
{
  if (!b)
    return 0;

  // clang-format off
  uint32_t packed = b->type +
                   (b->encoding      <<  4) +
                   (b->disposition   <<  7) +
                   (b->badsig        <<  9) +
                   (b->force_charset << 10) +
                   (b->goodsig       << 11) +
                   (b->noconv        << 12) +
                   (b->use_disp      << 13) +
                   (b->warnsig       << 14);
  // clang-format on
#ifdef USE_AUTOCRYPT
  packed += (b->is_autocrypt << 15);
#endif

  return packed;
}

/**
 * body_unpack_flags - Unpack the Body flags from a uint32_t
 * @param b      Body to unpack into
 * @param packed Packed flags
 *
 * @note Order of packing must match body_pack_flags()
 */
static inline void body_unpack_flags(struct Body *b, uint32_t packed)
{
  if (!b)
    return;

  // clang-format off
  b->type         =  (packed       & ((1 << 4) - 1)); // bits 0-3 (4)
  b->encoding     = ((packed >> 4) & ((1 << 3) - 1)); // bits 4-6 (3)
  b->disposition  = ((packed >> 7) & ((1 << 2) - 1)); // bits 7-8 (2)

  b->badsig        = (packed & (1 <<  9));
  b->force_charset = (packed & (1 << 10));
  b->goodsig       = (packed & (1 << 11));
  b->noconv        = (packed & (1 << 12));
  b->use_disp      = (packed & (1 << 13));
  b->warnsig       = (packed & (1 << 14));
#ifdef USE_AUTOCRYPT
  b->is_autocrypt  = (packed & (1 << 15));
#endif
  // clang-format on
}

/**
 * serial_dump_body - Pack an Body into a binary blob
 * @param[in]     b       Body to pack
 * @param[in]     d       Binary blob to add to
 * @param[in,out] off     Offset into the blob
 * @param[in]     convert If true, the strings will be converted to utf-8
 * @retval ptr End of the newly packed binary
 */
unsigned char *serial_dump_body(const struct Body *b, unsigned char *d, int *off, bool convert)
{
  uint32_t packed = body_pack_flags(b);
  d = serial_dump_uint32_t(packed, d, off);

  uint64_t big = b->offset;
  d = serial_dump_uint64_t(big, d, off);

  big = b->length;
  d = serial_dump_uint64_t(big, d, off);

  d = serial_dump_char(b->xtype, d, off, false);
  d = serial_dump_char(b->subtype, d, off, false);

  d = serial_dump_parameter(&b->parameter, d, off, convert);

  d = serial_dump_char(b->description, d, off, convert);
  d = serial_dump_char(b->form_name, d, off, convert);
  d = serial_dump_char(b->filename, d, off, convert);
  d = serial_dump_char(b->d_filename, d, off, convert);

  return d;
}

/**
 * serial_restore_body - Unpack a Body from a binary blob
 * @param[in]     b       Store the unpacked Body here
 * @param[in]     d       Binary blob to read from
 * @param[in,out] off     Offset into the blob
 * @param[in]     convert If true, the strings will be converted from utf-8
 */
void serial_restore_body(struct Body *b, const unsigned char *d, int *off, bool convert)
{
  uint32_t packed = 0;
  serial_restore_uint32_t(&packed, d, off);
  body_unpack_flags(b, packed);

  uint64_t big = 0;
  serial_restore_uint64_t(&big, d, off);
  b->offset = big;

  big = 0;
  serial_restore_uint64_t(&big, d, off);
  b->length = big;

  serial_restore_char(&b->xtype, d, off, false);
  serial_restore_char(&b->subtype, d, off, false);

  TAILQ_INIT(&b->parameter);
  serial_restore_parameter(&b->parameter, d, off, convert);

  serial_restore_char(&b->description, d, off, convert);
  serial_restore_char(&b->form_name, d, off, convert);
  serial_restore_char(&b->filename, d, off, convert);
  serial_restore_char(&b->d_filename, d, off, convert);
}

/**
 * serial_dump_envelope - Pack an Envelope into a binary blob
 * @param[in]     env     Envelope to pack
 * @param[in]     d       Binary blob to add to
 * @param[in,out] off     Offset into the blob
 * @param[in]     convert If true, the strings will be converted to utf-8
 * @retval ptr End of the newly packed binary
 */
unsigned char *serial_dump_envelope(const struct Envelope *env,
                                    unsigned char *d, int *off, bool convert)
{
  d = serial_dump_address(&env->return_path, d, off, convert);
  d = serial_dump_address(&env->from, d, off, convert);
  d = serial_dump_address(&env->to, d, off, convert);
  d = serial_dump_address(&env->cc, d, off, convert);
  d = serial_dump_address(&env->bcc, d, off, convert);
  d = serial_dump_address(&env->sender, d, off, convert);
  d = serial_dump_address(&env->reply_to, d, off, convert);
  d = serial_dump_address(&env->mail_followup_to, d, off, convert);

  d = serial_dump_char(env->list_post, d, off, convert);
  d = serial_dump_char(env->list_subscribe, d, off, convert);
  d = serial_dump_char(env->list_unsubscribe, d, off, convert);
  d = serial_dump_char(env->subject, d, off, convert);

  if (env->real_subj)
    d = serial_dump_int(env->real_subj - env->subject, d, off);
  else
    d = serial_dump_int(-1, d, off);

  d = serial_dump_char(env->message_id, d, off, false);
  d = serial_dump_char(env->supersedes, d, off, false);
  d = serial_dump_char(env->date, d, off, false);
  d = serial_dump_char(env->x_label, d, off, convert);
  d = serial_dump_char(env->organization, d, off, convert);

  d = serial_dump_buffer(&env->spam, d, off, convert);

  d = serial_dump_stailq(&env->references, d, off, false);
  d = serial_dump_stailq(&env->in_reply_to, d, off, false);
  d = serial_dump_stailq(&env->userhdrs, d, off, convert);

#ifdef USE_NNTP
  d = serial_dump_char(env->xref, d, off, false);
  d = serial_dump_char(env->followup_to, d, off, false);
  d = serial_dump_char(env->x_comment_to, d, off, convert);
#endif

  return d;
}

/**
 * serial_restore_envelope - Unpack an Envelope from a binary blob
 * @param[in]     env     Store the unpacked Envelope here
 * @param[in]     d       Binary blob to read from
 * @param[in,out] off     Offset into the blob
 * @param[in]     convert If true, the strings will be converted from utf-8
 */
void serial_restore_envelope(struct Envelope *env, const unsigned char *d, int *off, bool convert)
{
  int real_subj_off = 0;

  serial_restore_address(&env->return_path, d, off, convert);
  serial_restore_address(&env->from, d, off, convert);
  serial_restore_address(&env->to, d, off, convert);
  serial_restore_address(&env->cc, d, off, convert);
  serial_restore_address(&env->bcc, d, off, convert);
  serial_restore_address(&env->sender, d, off, convert);
  serial_restore_address(&env->reply_to, d, off, convert);
  serial_restore_address(&env->mail_followup_to, d, off, convert);

  serial_restore_char(&env->list_post, d, off, convert);
  serial_restore_char(&env->list_subscribe, d, off, convert);
  serial_restore_char(&env->list_unsubscribe, d, off, convert);

  const bool c_auto_subscribe = cs_subset_bool(NeoMutt->sub, "auto_subscribe");
  if (c_auto_subscribe)
    mutt_auto_subscribe(env->list_post);

  serial_restore_char(&env->subject, d, off, convert);
  serial_restore_int((unsigned int *) (&real_subj_off), d, off);

  size_t len = mutt_str_len(env->subject);
  if ((real_subj_off < 0) || (real_subj_off >= len))
    env->real_subj = NULL;
  else
    env->real_subj = env->subject + real_subj_off;

  serial_restore_char(&env->message_id, d, off, false);
  serial_restore_char(&env->supersedes, d, off, false);
  serial_restore_char(&env->date, d, off, false);
  serial_restore_char(&env->x_label, d, off, convert);
  serial_restore_char(&env->organization, d, off, convert);

  serial_restore_buffer(&env->spam, d, off, convert);

  serial_restore_stailq(&env->references, d, off, false);
  serial_restore_stailq(&env->in_reply_to, d, off, false);
  serial_restore_stailq(&env->userhdrs, d, off, convert);

#ifdef USE_NNTP
  serial_restore_char(&env->xref, d, off, false);
  serial_restore_char(&env->followup_to, d, off, false);
  serial_restore_char(&env->x_comment_to, d, off, convert);
#endif
}

/**
 * serial_dump_tags - Pack a TagList into a binary blob
 * @param[in]     tags TagList to pack
 * @param[in]     d    Binary blob to add to
 * @param[in,out] off  Offset into the blob
 * @retval ptr End of the newly packed binary
 */
unsigned char *serial_dump_tags(const struct TagList *tags, unsigned char *d, int *off)
{
  unsigned int counter = 0;
  unsigned int start_off = *off;

  d = serial_dump_int(0xdeadbeef, d, off);

  struct Tag *t = NULL;
  STAILQ_FOREACH(t, tags, entries)
  {
    d = serial_dump_char(t->name, d, off, false);
    counter++;
  }

  memcpy(d + start_off, &counter, sizeof(int));

  return d;
}

/**
 * serial_restore_tags - Unpack a TagList from a binary blob
 * @param[in]     tags TagList to unpack
 * @param[in]     d    Binary blob to add to
 * @param[in,out] off  Offset into the blob
 */
void serial_restore_tags(struct TagList *tags, const unsigned char *d, int *off)
{
  unsigned int counter = 0;

  serial_restore_int(&counter, d, off);

  while (counter)
  {
    char *name = NULL;
    serial_restore_char(&name, d, off, false);
    driver_tags_add(tags, name);
    counter--;
  }
}
