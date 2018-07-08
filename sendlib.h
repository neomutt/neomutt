/**
 * @file
 * Miscellaneous functions for sending an email
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

#ifndef MUTT_SENDLIB_H
#define MUTT_SENDLIB_H

#include <stdbool.h>
#include <stdio.h>

struct Address;
struct Body;
struct Context;
struct Envelope;
struct Header;
struct ListHead;
struct ParameterList;

/* These Config Variables are only used in sendlib.c */
extern bool  Allow8bit;
extern char *AttachCharset;
extern bool  BounceDelivered;
extern bool  EncodeFrom;
extern bool  ForwardDecrypt;
extern bool  HiddenHost;
extern char *Inews;
extern bool  MimeForwardDecode;
extern bool  MimeSubject; /**< encode subject line with RFC2047 */
extern char *MimeTypeQueryCommand;
extern bool  MimeTypeQueryFirst;
extern char *Sendmail;
extern short SendmailWait;
extern bool  Use8bitmime;
extern bool  UseEnvelopeFrom;
extern bool  UserAgent;
extern short WrapHeaders;

char *          mutt_body_get_charset(struct Body *b, char *buf, size_t buflen);
int             mutt_bounce_message(FILE *fp, struct Header *h, struct Address *to);
const char *    mutt_fqdn(bool may_hide_host);
void            mutt_generate_boundary(struct ParameterList *parm);
struct Content *mutt_get_content_info(const char *fname, struct Body *b);
int             mutt_invoke_sendmail(struct Address *from, struct Address *to, struct Address *cc, struct Address *bcc, const char *msg, int eightbit);
int             mutt_lookup_mime_type(struct Body *att, const char *path);
struct Body *   mutt_make_file_attach(const char *path);
struct Body *   mutt_make_message_attach(struct Context *ctx, struct Header *hdr, bool attach_msg);
struct Body *   mutt_make_multipart(struct Body *b);
void            mutt_message_to_7bit(struct Body *a, FILE *fp);
void            mutt_prepare_envelope(struct Envelope *env, bool final);
struct Address *mutt_remove_duplicates(struct Address *addr);
struct Body *   mutt_remove_multipart(struct Body *b);
int             mutt_rfc822_write_header(FILE *fp, struct Envelope *env, struct Body *attach, int mode, bool privacy);
void            mutt_stamp_attachment(struct Body *a);
void            mutt_unprepare_envelope(struct Envelope *env);
void            mutt_update_encoding(struct Body *a);
void            mutt_write_address_list(struct Address *addr, FILE *fp, int linelen, bool display);
int             mutt_write_fcc(const char *path, struct Header *hdr, const char *msgid, int post, char *fcc, char **finalpath);
int             mutt_write_mime_body(struct Body *a, FILE *f);
int             mutt_write_mime_header(struct Body *a, FILE *f);
int             mutt_write_multiple_fcc(const char *path, struct Header *hdr, const char *msgid, int post, char *fcc, char **finalpath);
int             mutt_write_one_header(FILE *fp, const char *tag, const char *value, const char *pfx, int wraplen, int flags);
void            mutt_write_references(const struct ListHead *r, FILE *f, size_t trim);

#endif /* MUTT_SENDLIB_H */
