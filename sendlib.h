/**
 * @file
 * Miscellaneous functions for sending an email
 *
 * @authors
 * Copyright (C) 2018 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2019 Pietro Cerutti <gahr@gahr.ch>
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

#ifndef MUTT_SENDLIB_H
#define MUTT_SENDLIB_H

#include <stdbool.h>
#include <stdio.h>
#include "email/lib.h"
#include "copy.h"

struct AddressList;
struct ListHead;
struct Mailbox;

/* These Config Variables are only used in sendlib.c */
extern bool  C_Allow8bit;
extern struct Slist *C_AttachCharset;
extern bool  C_BounceDelivered;
extern bool  C_EncodeFrom;
extern bool  C_ForwardDecrypt;
extern bool  C_HiddenHost;
extern char *C_Inews;
extern bool  C_MimeForwardDecode;
extern bool  C_MimeSubject; ///< encode subject line with RFC2047
extern char *C_MimeTypeQueryCommand;
extern bool  C_MimeTypeQueryFirst;
extern char *C_Sendmail;
extern short C_SendmailWait;
extern bool  C_Use8bitmime;
extern bool  C_UseEnvelopeFrom;
extern bool  C_UserAgent;
extern short C_WrapHeaders;

/**
 * enum MuttWriteHeaderMode - Modes for mutt_rfc822_write_header()
 */
enum MuttWriteHeaderMode
{
  MUTT_WRITE_HEADER_NORMAL,   ///< A normal Email, write full header + MIME headers
  MUTT_WRITE_HEADER_FCC,      ///< fcc mode, like normal mode but for Bcc header
  MUTT_WRITE_HEADER_POSTPONE, ///< A postponed Email, just the envelope info
  MUTT_WRITE_HEADER_EDITHDRS, ///< "light" mode (used for edit_hdrs)
  MUTT_WRITE_HEADER_MIME,     ///< Write protected headers
};

char *          mutt_body_get_charset(struct Body *b, char *buf, size_t buflen);
int             mutt_bounce_message(FILE *fp, struct Email *e, struct AddressList *to);
const char *    mutt_fqdn(bool may_hide_host);
void            mutt_generate_boundary(struct ParameterList *pl);
struct Content *mutt_get_content_info(const char *fname, struct Body *b);
int             mutt_invoke_sendmail(struct AddressList *from, struct AddressList *to, struct AddressList *cc, struct AddressList *bcc, const char *msg, int eightbit);
enum ContentType mutt_lookup_mime_type(struct Body *att, const char *path);
struct Body *   mutt_make_file_attach(const char *path);
struct Body *   mutt_make_message_attach(struct Mailbox *m, struct Email *e, bool attach_msg);
struct Body *   mutt_make_multipart(struct Body *b);
void            mutt_message_to_7bit(struct Body *a, FILE *fp);
void            mutt_prepare_envelope(struct Envelope *env, bool final);
struct Body *   mutt_remove_multipart(struct Body *b);
int             mutt_rfc822_write_header(FILE *fp, struct Envelope *env, struct Body *attach, enum MuttWriteHeaderMode mode, bool privacy, bool hide_protected_subject);
void            mutt_stamp_attachment(struct Body *a);
void            mutt_unprepare_envelope(struct Envelope *env);
void            mutt_update_encoding(struct Body *a);
void            mutt_write_addrlist(struct AddressList *addr, FILE *fp, int linelen, bool display);
int             mutt_write_fcc(const char *path, struct Email *e, const char *msgid, bool post, const char *fcc, char **finalpath);
int             mutt_write_mime_body(struct Body *a, FILE *fp);
int             mutt_write_mime_header(struct Body *a, FILE *fp);
int             mutt_write_multiple_fcc(const char *path, struct Email *e, const char *msgid, bool post, char *fcc, char **finalpath);
int             mutt_write_one_header(FILE *fp, const char *tag, const char *value, const char *pfx, int wraplen, CopyHeaderFlags chflags);
void            mutt_write_references(const struct ListHead *r, FILE *fp, size_t trim);

#endif /* MUTT_SENDLIB_H */
