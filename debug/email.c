/**
 * @file
 * Dump an Email
 *
 * @authors
 * Copyright (C) 2020 Richard Russon <rich@flatcap.org>
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
 * @page debug_email Dump an Email
 *
 * Dump an Email
 */

#include "config.h"
#include <stdbool.h>
#include <stdio.h>
#include "mutt/lib.h"
#include "address/lib.h"
#include "email/lib.h"
#include "lib.h"
#include "ncrypt/lib.h"

void dump_addr_list(char *buf, size_t buflen, const struct AddressList *al, const char *name)
{
  if (!buf || !al)
    return;
  if (TAILQ_EMPTY(al))
    return;

  buf[0] = '\0';
  mutt_addrlist_write(al, buf, buflen, true);
  mutt_debug(LL_DEBUG1, "\t%s: %s\n", name, buf);
}

void dump_list_head(const struct ListHead *list, const char *name)
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

  mutt_debug(LL_DEBUG1, "\t%s: %s\n", name, mutt_buffer_string(&buf));
  mutt_buffer_dealloc(&buf);
}

void dump_envelope(const struct Envelope *env)
{
  struct Buffer buf = mutt_buffer_make(256);
  char arr[1024];

  mutt_debug(LL_DEBUG1, "Envelope\n");

#define ADD_FLAG(F) add_flag(&buf, (env->changed & F), #F)
  ADD_FLAG(MUTT_ENV_CHANGED_IRT);
  ADD_FLAG(MUTT_ENV_CHANGED_REFS);
  ADD_FLAG(MUTT_ENV_CHANGED_XLABEL);
  ADD_FLAG(MUTT_ENV_CHANGED_SUBJECT);
#undef ADD_FLAG
  mutt_debug(LL_DEBUG1, "\tchanged: %s\n",
             mutt_buffer_is_empty(&buf) ? "[NONE]" : mutt_buffer_string(&buf));

#define ADDR_LIST(AL) dump_addr_list(arr, sizeof(arr), &env->AL, #AL)
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

#define OPT_STRING(S)                                                          \
  if (env->S)                                                                  \
  mutt_debug(LL_DEBUG1, "\t%s: %s\n", #S, env->S)
  OPT_STRING(list_post);
  OPT_STRING(list_unsubscribe);
  OPT_STRING(subject);
  OPT_STRING(real_subj);
  OPT_STRING(disp_subj);
  OPT_STRING(message_id);
  OPT_STRING(supersedes);
  OPT_STRING(date);
  OPT_STRING(x_label);
  OPT_STRING(organization);
#ifdef USE_NNTP
  OPT_STRING(newsgroups);
  OPT_STRING(xref);
  OPT_STRING(followup_to);
  OPT_STRING(x_comment_to);
#endif
#undef OPT_STRING

  dump_list_head(&env->references, "references");
  dump_list_head(&env->in_reply_to, "in_reply_to");
  dump_list_head(&env->userhdrs, "userhdrs");

  if (!mutt_buffer_is_empty(&env->spam))
    mutt_debug(LL_DEBUG1, "\tspam: %s\n", mutt_buffer_string(&env->spam));

#ifdef USE_AUTOCRYPT
  if (env->autocrypt)
    mutt_debug(LL_DEBUG1, "\tautocrypt: %p\n", (void *) env->autocrypt);
  if (env->autocrypt_gossip)
    mutt_debug(LL_DEBUG1, "\tautocrypt_gossip: %p\n", (void *) env->autocrypt_gossip);
#endif

  mutt_buffer_dealloc(&buf);
}

void dump_email(const struct Email *e)
{
  if (!e)
    return;

  struct Buffer buf = mutt_buffer_make(256);
  char arr[256];

  mutt_debug(LL_DEBUG1, "Email\n");
  mutt_debug(LL_DEBUG1, "\tpath: %s\n", e->path);

#define ADD_FLAG(F) add_flag(&buf, e->F, #F)
  ADD_FLAG(active);
  ADD_FLAG(attach_del);
  ADD_FLAG(attach_valid);
  ADD_FLAG(changed);
  ADD_FLAG(collapsed);
  ADD_FLAG(deleted);
  ADD_FLAG(display_subject);
  ADD_FLAG(expired);
  ADD_FLAG(flagged);
  ADD_FLAG(matched);
  ADD_FLAG(mime);
  ADD_FLAG(old);
  ADD_FLAG(purge);
  ADD_FLAG(quasi_deleted);
  ADD_FLAG(read);
  ADD_FLAG(recip_valid);
  ADD_FLAG(replied);
  ADD_FLAG(searched);
  ADD_FLAG(subject_changed);
  ADD_FLAG(superseded);
  ADD_FLAG(tagged);
  ADD_FLAG(threaded);
  ADD_FLAG(trash);
  ADD_FLAG(visible);
#undef ADD_FLAG
  mutt_debug(LL_DEBUG1, "\tFlags: %s\n",
             mutt_buffer_is_empty(&buf) ? "[NONE]" : mutt_buffer_string(&buf));

#define ADD_FLAG(F) add_flag(&buf, (e->security & F), #F)
  mutt_buffer_reset(&buf);
  ADD_FLAG(SEC_ENCRYPT);
  ADD_FLAG(SEC_SIGN);
  ADD_FLAG(SEC_GOODSIGN);
  ADD_FLAG(SEC_BADSIGN);
  ADD_FLAG(SEC_PARTSIGN);
  ADD_FLAG(SEC_SIGNOPAQUE);
  ADD_FLAG(SEC_KEYBLOCK);
  ADD_FLAG(SEC_INLINE);
  ADD_FLAG(SEC_OPPENCRYPT);
  ADD_FLAG(SEC_AUTOCRYPT);
  ADD_FLAG(SEC_AUTOCRYPT_OVERRIDE);
  ADD_FLAG(APPLICATION_PGP);
  ADD_FLAG(APPLICATION_SMIME);
  ADD_FLAG(PGP_TRADITIONAL_CHECKED);
#undef ADD_FLAG
  mutt_debug(LL_DEBUG1, "\tSecurity: %s\n",
             mutt_buffer_is_empty(&buf) ? "[NONE]" : mutt_buffer_string(&buf));

  mutt_date_make_tls(arr, sizeof(arr), e->date_sent);
  mutt_debug(LL_DEBUG1, "\tSent: %s (%c%02u%02u)\n", arr,
             e->zoccident ? '-' : '+', e->zhours, e->zminutes);

  mutt_date_make_tls(arr, sizeof(arr), e->received);
  mutt_debug(LL_DEBUG1, "\tRecv: %s\n", arr);

  mutt_buffer_dealloc(&buf);

  mutt_debug(LL_DEBUG1, "\tnum_hidden: %ld\n", e->num_hidden);
  mutt_debug(LL_DEBUG1, "\trecipient: %d\n", e->recipient);
  mutt_debug(LL_DEBUG1, "\toffset: %ld\n", e->offset);
  mutt_debug(LL_DEBUG1, "\tlines: %d\n", e->lines);
  mutt_debug(LL_DEBUG1, "\tindex: %d\n", e->index);
  mutt_debug(LL_DEBUG1, "\tmsgno: %d\n", e->msgno);
  mutt_debug(LL_DEBUG1, "\tvnum: %d\n", e->vnum);
  mutt_debug(LL_DEBUG1, "\tscore: %d\n", e->score);
  mutt_debug(LL_DEBUG1, "\tattach_total: %d\n", e->attach_total);
  // if (e->maildir_flags)
  //   mutt_debug(LL_DEBUG1, "\tmaildir_flags: %s\n", e->maildir_flags);

  // struct MuttThread *thread
  // struct Envelope *env
  // struct Body *content
  // struct TagList tags

  // void *edata
  mutt_buffer_dealloc(&buf);
}

void dump_param_list(const struct ParameterList *pl)
{
  if (!pl || TAILQ_EMPTY(pl))
    return;

  mutt_debug(LL_DEBUG1, "\tparameters\n");
  struct Parameter *np = NULL;
  TAILQ_FOREACH(np, pl, entries)
  {
    mutt_debug(LL_DEBUG1, "\t\t%s = %s\n", NONULL(np->attribute), NONULL(np->value));
  }
}

void dump_body(const struct Body *body)
{
  if (!body)
    return;

  struct Buffer buf = mutt_buffer_make(256);
  char arr[256];

  mutt_debug(LL_DEBUG1, "Body\n");

#define ADD_FLAG(F) add_flag(&buf, body->F, #F)
  ADD_FLAG(attach_qualifies);
  ADD_FLAG(badsig);
  ADD_FLAG(collapsed);
  ADD_FLAG(deleted);
  ADD_FLAG(force_charset);
  ADD_FLAG(goodsig);
#ifdef USE_AUTOCRYPT
  ADD_FLAG(is_autocrypt);
#endif
  ADD_FLAG(noconv);
  ADD_FLAG(tagged);
  ADD_FLAG(unlink);
  ADD_FLAG(use_disp);
  ADD_FLAG(warnsig);
#undef ADD_FLAG
  mutt_debug(LL_DEBUG1, "\tFlags: %s\n",
             mutt_buffer_is_empty(&buf) ? "[NONE]" : mutt_buffer_string(&buf));

#define OPT_STRING(S)                                                          \
  if (body->S)                                                                 \
  mutt_debug(LL_DEBUG1, "\t%s: %s\n", #S, body->S)
  OPT_STRING(charset);
  OPT_STRING(description);
  OPT_STRING(d_filename);
  OPT_STRING(filename);
  OPT_STRING(form_name);
  OPT_STRING(language);
  OPT_STRING(subtype);
  OPT_STRING(xtype);
#undef OPT_STRING

  mutt_debug(LL_DEBUG1, "\thdr_offset: %ld\n", body->hdr_offset);
  mutt_debug(LL_DEBUG1, "\toffset: %ld\n", body->offset);
  mutt_debug(LL_DEBUG1, "\tlength: %ld\n", body->length);
  mutt_debug(LL_DEBUG1, "\tattach_count: %d\n", body->attach_count);

  mutt_debug(LL_DEBUG1, "\tcontent type: %s\n", get_content_type(body->type));
  mutt_debug(LL_DEBUG1, "\tcontent encoding: %s\n", get_content_encoding(body->encoding));
  mutt_debug(LL_DEBUG1, "\tcontent disposition: %s\n",
             get_content_disposition(body->disposition));

  if (body->stamp != 0)
  {
    mutt_date_make_tls(arr, sizeof(arr), body->stamp);
    mutt_debug(LL_DEBUG1, "\tstamp: %s\n", arr);
  }

  dump_param_list(&body->parameter);

  // struct Content *content;        ///< Detailed info about the content of the attachment.
  // struct Body *next;              ///< next attachment in the list
  // struct Body *parts;             ///< parts of a multipart or message/rfc822
  // struct Email *email;            ///< header information for message/rfc822
  // struct AttachPtr *aptr;         ///< Menu information, used in recvattach.c
  // struct Envelope *mime_headers;  ///< Memory hole protected headers

  if (body->next)
  {
    mutt_debug(LL_DEBUG1, "-NEXT-------------------------\n");
    dump_body(body->next);
  }
  if (body->parts)
  {
    mutt_debug(LL_DEBUG1, "-PARTS-------------------------\n");
    dump_body(body->parts);
  }
  if (body->next || body->parts)
    mutt_debug(LL_DEBUG1, "--------------------------\n");
  mutt_buffer_dealloc(&buf);
}

void dump_attach(const struct AttachPtr *att)
{
  if (!att)
    return;

  struct Buffer buf = mutt_buffer_make(256);

  mutt_debug(LL_DEBUG1, "AttachPtr\n");

#define ADD_FLAG(F) add_flag(&buf, att->F, #F)
  ADD_FLAG(unowned);
  ADD_FLAG(decrypted);
#undef ADD_FLAG

  if (att->fp)
    mutt_debug(LL_DEBUG1, "\tfp: %p (%d)\n", att->fp, fileno(att->fp));
  mutt_debug(LL_DEBUG1, "\tparent_type: %d\n", att->parent_type);
  mutt_debug(LL_DEBUG1, "\tlevel: %d\n", att->level);
  mutt_debug(LL_DEBUG1, "\tnum: %d\n", att->num);

  // struct Body *content; ///< Attachment
  mutt_buffer_dealloc(&buf);
}
