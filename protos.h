/**
 * @file
 * Prototypes for many functions
 *
 * @authors
 * Copyright (C) 1996-2000,2007,2010,2013 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 2013 Karel Zak <kzak@redhat.com>
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

#ifndef _MUTT_PROTOS_H
#define _MUTT_PROTOS_H

#include <stddef.h>
#include <ctype.h>
#include <iconv.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include <wctype.h>
#include "format_flags.h"
#include "options.h"

struct Address;
struct Alias;
struct Body;
struct Buffer;
struct Context;
struct EnterState;
struct Envelope;
struct Header;
struct Parameter;
struct Regex;
struct ReplaceList;
struct RegexList;
struct State;
struct ListHead;

struct stat;
struct passwd;

#define mutt_make_string(A, B, C, D, E) _mutt_make_string(A, B, C, D, E, 0)
void _mutt_make_string(char *dest, size_t destlen, const char *s, struct Context *ctx,
                       struct Header *hdr, enum FormatFlag flags);

/**
 * struct HdrFormatInfo - Data passed to hdr_format_str()
 */
struct HdrFormatInfo
{
  struct Context *ctx;
  struct Header *hdr;
  const char *pager_progress;
};

/**
 * enum XdgType - XDG variable types
 */
enum XdgType
{
  XDG_CONFIG_HOME,
  XDG_CONFIG_DIRS,
};

int mutt_extract_token(struct Buffer *dest, struct Buffer *tok, int flags);

void mutt_make_string_info(char *dst, size_t dstlen, int cols, const char *s,
                           struct HdrFormatInfo *hfi, enum FormatFlag flags);

void mutt_free_opts(void);

int mutt_system(const char *cmd);

void mutt_parse_content_type(char *s, struct Body *ct);
void mutt_generate_boundary(struct Parameter **parm);

#ifdef USE_NOTMUCH
int mutt_parse_virtual_mailboxes(struct Buffer *path, struct Buffer *s, unsigned long data, struct Buffer *err);
#endif

FILE *mutt_open_read(const char *path, pid_t *thepid);

void set_quadoption(int opt, int flag);
int query_quadoption(int opt, const char *prompt);
int quadoption(int opt);

char *mutt_extract_message_id(const char *s, const char **saveptr);

struct Address *mutt_default_from(void);
struct Address *mutt_remove_duplicates(struct Address *addr);
struct Address *mutt_remove_xrefs(struct Address *a, struct Address *b);
struct Address *mutt_parse_adrlist(struct Address *p, const char *s);

struct Body *mutt_make_file_attach(const char *path);
struct Body *mutt_make_message_attach(struct Context *ctx, struct Header *hdr, int attach_msg);
struct Body *mutt_remove_multipart(struct Body *b);
struct Body *mutt_make_multipart(struct Body *b);
struct Body *mutt_parse_multipart(FILE *fp, const char *boundary, LOFF_T end_off, int digest);
struct Body *mutt_parse_message_rfc822(FILE *fp, struct Body *parent);
struct Body *mutt_read_mime_header(FILE *fp, int digest);

struct Content *mutt_get_content_info(const char *fname, struct Body *b);

char *mutt_read_rfc822_line(FILE *f, char *line, size_t *linelen);
struct Envelope *mutt_read_rfc822_header(FILE *f, struct Header *hdr, short user_hdrs, short weed);

int is_from(const char *s, char *path, size_t pathlen, time_t *tp);

const char *mutt_attach_fmt(char *dest, size_t destlen, size_t col, int cols,
                            char op, const char *src, const char *prefix,
                            const char *ifstring, const char *elsestring,
                            unsigned long data, enum FormatFlag flags);

char *mutt_charset_hook(const char *chs);
char *mutt_iconv_hook(const char *chs);
char *mutt_expand_path(char *s, size_t slen);
char *_mutt_expand_path(char *s, size_t slen, int regex);
char *mutt_find_hook(int type, const char *pat);
char *mutt_gecos_name(char *dest, size_t destlen, struct passwd *pw);
char *mutt_get_body_charset(char *d, size_t dlen, struct Body *b);
void mutt_crypt_hook(struct ListHead *list, struct Address *adr);
void mutt_timeout_hook(void);
void mutt_startup_shutdown_hook(int type);
int mutt_set_xdg_path(enum XdgType type, char *buf, size_t bufsize);

const char *mutt_make_version(void);

const char *mutt_fqdn(short may_hide_host);

struct Regex *mutt_compile_regex(const char *s, int flags);

void mutt_account_hook(const char *url);
void mutt_add_to_reference_headers(struct Envelope *env, struct Envelope *curenv);
void mutt_adv_mktemp(char *s, size_t l);
void mutt_alias_menu(char *buf, size_t buflen, struct Alias *aliases);
void mutt_allow_interrupt(int disposition);
void mutt_block_signals(void);
void mutt_block_signals_system(void);
int mutt_body_handler(struct Body *b, struct State *s);
int mutt_bounce_message(FILE *fp, struct Header *h, struct Address *to);
void mutt_buffy(char *s, size_t slen);
int mutt_buffy_list(void);
void mutt_canonical_charset(char *dest, size_t dlen, const char *name);
int mutt_count_body_parts(struct Context *ctx, struct Header *hdr);
void mutt_check_rescore(struct Context *ctx);
void mutt_clear_error(void);
void mutt_clear_pager_position(void);
void mutt_decode_attachment(struct Body *b, struct State *s);
void mutt_decode_base64(struct State *s, long len, int istext, iconv_t cd);
void mutt_default_save(char *path, size_t pathlen, struct Header *hdr);
void mutt_display_address(struct Envelope *env);
void mutt_draw_statusline(int cols, const char *buf, int buflen);
int mutt_edit_content_type (struct Header *h, struct Body *b, FILE *fp);
void mutt_edit_file(const char *editor, const char *data);
void mutt_edit_headers(const char *editor, const char *body, struct Header *msg,
                       char *fcc, size_t fcclen);
char **mutt_envlist(void);
void mutt_envlist_set(const char *name, const char *value, bool overwrite);
int mutt_label_message(struct Header *hdr);
void mutt_make_label_hash(struct Context *ctx);
void mutt_label_hash_add(struct Context *ctx, struct Header *hdr);
void mutt_label_hash_remove(struct Context *ctx, struct Header *hdr);
int mutt_label_complete(char *buffer, size_t len, int numtabs);
void mutt_curses_error(const char *fmt, ...);
void mutt_curses_message(const char *fmt, ...);
void mutt_encode_descriptions(struct Body *b, short recurse);
void mutt_encode_path(char *dest, size_t dlen, const char *src);
void mutt_enter_command(void);
void mutt_expand_file_fmt(char *dest, size_t destlen, const char *fmt, const char *src);
void mutt_expand_fmt(char *dest, size_t destlen, const char *fmt, const char *src);
void mutt_fix_reply_recipients(struct Envelope *env);
void mutt_folder_hook(char *path);
void mutt_simple_format(char *dest, size_t destlen, int min_width, int max_width, int justify,
                        char m_pad_char, const char *s, size_t n, int arboreal);
void mutt_format_s(char *dest, size_t destlen, const char *prefix, const char *s);
void mutt_format_s_tree(char *dest, size_t destlen, const char *prefix, const char *s);
void mutt_forward_intro(struct Context *ctx, struct Header *cur, FILE *fp);
void mutt_forward_trailer(struct Context *ctx, struct Header *cur, FILE *fp);
void mutt_free_alias(struct Alias **p);
void mutt_free_color(int fg, int bg);
void mutt_free_enter_state(struct EnterState **esp);
void mutt_free_header(struct Header **h);
void mutt_free_regex(struct Regex **pp);
void mutt_help(int menu);
void mutt_check_lookup_list(struct Body *b, char *type, int len);
void mutt_make_attribution(struct Context *ctx, struct Header *cur, FILE *out);
void mutt_make_forward_subject(struct Envelope *env, struct Context *ctx, struct Header *cur);
void mutt_make_help(char *d, size_t dlen, const char *txt, int menu, int op);
void mutt_make_misc_reply_headers(struct Envelope *env, struct Context *ctx, struct Header *cur, struct Envelope *curenv);
void mutt_make_post_indent(struct Context *ctx, struct Header *cur, FILE *out);
void mutt_message_to_7bit(struct Body *a, FILE *fp);
#define mutt_mktemp(a, b) mutt_mktemp_pfx_sfx(a, b, "neomutt", NULL)
#define mutt_mktemp_pfx_sfx(a, b, c, d) _mutt_mktemp(a, b, c, d, __FILE__, __LINE__)
void _mutt_mktemp(char *s, size_t slen, const char *prefix, const char *suffix,
                  const char *src, int line);
void mutt_paddstr(int n, const char *s);
void mutt_parse_mime_message(struct Context *ctx, struct Header *cur);
void mutt_parse_part(FILE *fp, struct Body *b);
void mutt_perror_debug(const char *s);
void mutt_prepare_envelope(struct Envelope *env, int final);
void mutt_unprepare_envelope(struct Envelope *env);
void mutt_pretty_mailbox(char *s, size_t buflen);
void mutt_pretty_size(char *s, size_t len, LOFF_T n);
void mutt_pipe_message(struct Header *h);
void mutt_print_message(struct Header *h);
void mutt_query_exit(void);
void mutt_query_menu(char *buf, size_t buflen);
void mutt_safe_path(char *s, size_t l, struct Address *a);
void mutt_save_path(char *d, size_t dsize, struct Address *a);
void mutt_score_message(struct Context *ctx, struct Header *hdr, int upd_ctx);
void mutt_select_fcc(char *path, size_t pathlen, struct Header *hdr);
#define mutt_select_file(A, B, C) _mutt_select_file(A, B, C, NULL, NULL)
void _mutt_select_file(char *f, size_t flen, int flags, char ***files, int *numfiles);
void mutt_message_hook(struct Context *ctx, struct Header *hdr, int type);
void _mutt_set_flag(struct Context *ctx, struct Header *h, int flag, int bf, int upd_ctx);
#define mutt_set_flag(a, b, c, d) _mutt_set_flag(a, b, c, d, 1)
void mutt_set_followup_to(struct Envelope *e);
void mutt_shell_escape(void);
void mutt_show_error(void);
void mutt_signal_init(void);
void mutt_stamp_attachment(struct Body *a);
void mutt_tag_set_flag(int flag, int bf);
bool mutt_ts_capability(void);
void mutt_unblock_signals(void);
void mutt_unblock_signals_system(int catch);
void mutt_update_encoding(struct Body *a);
void mutt_version(void);
void mutt_view_attachments(struct Header *hdr);
void mutt_write_address_list(struct Address *adr, FILE *fp, int linelen, int display);
int mutt_add_to_regex_list(struct RegexList **list, const char *s, int flags, struct Buffer *err);
bool mutt_addr_is_user(struct Address *addr);
int mutt_addwch(wchar_t wc);
int mutt_alias_complete(char *s, size_t buflen);
void mutt_alias_add_reverse(struct Alias *t);
void mutt_alias_delete_reverse(struct Alias *t);
int mutt_alloc_color(int fg, int bg);
int mutt_combine_color(int fg_attr, int bg_attr);
int mutt_any_key_to_continue(const char *s);
char *mutt_apply_replace(char *dbuf, size_t dlen, char *sbuf, struct ReplaceList *rlist);
int mutt_buffy_check(bool force);
int mutt_buffy_notify(void);
int mutt_builtin_editor(const char *path, struct Header *msg, struct Header *cur);
int mutt_can_decode(struct Body *a);
int mutt_change_flag(struct Header *h, int bf);
int mutt_check_encoding(const char *c);

int mutt_check_mime_type(const char *s);
int mutt_check_month(const char *s);
int mutt_check_overwrite(const char *attname, const char *path, char *fname,
                         size_t flen, int *append, char **directory);
int mutt_check_traditional_pgp(struct Header *h, int *redraw);
int mutt_command_complete(char *buffer, size_t len, int pos, int numtabs);
int mutt_var_value_complete(char *buffer, size_t len, int pos);
#ifdef USE_NOTMUCH
bool mutt_nm_query_complete(char *buffer, size_t len, int pos, int numtabs);
bool mutt_nm_tag_complete(char *buffer, size_t len, int pos, int numtabs);
#endif
int mutt_complete(char *s, size_t slen);
int mutt_compose_attachment(struct Body *a);
int mutt_decode_save_attachment(FILE *fp, struct Body *m, char *path, int displaying, int flags);
int mutt_display_message(struct Header *cur);
int mutt_dump_variables(int hide_sensitive);
int mutt_edit_attachment(struct Body *a);
int mutt_edit_message(struct Context *ctx, struct Header *hdr);
int mutt_fetch_recips(struct Envelope *out, struct Envelope *in, int flags);
int mutt_chscmp(const char *s, const char *chs);
#define mutt_is_utf8(a) mutt_chscmp(a, "utf-8")
#define mutt_is_us_ascii(a) mutt_chscmp(a, "us-ascii")
int mutt_prepare_template(FILE *fp, struct Context *ctx, struct Header *newhdr, struct Header *hdr, short resend);
int mutt_resend_message(FILE *fp, struct Context *ctx, struct Header *cur);
int mutt_compose_to_sender(struct Header *hdr);
#define mutt_enter_fname(A, B, C, D) _mutt_enter_fname(A, B, C, D, 0, NULL, NULL, 0)
#define mutt_enter_vfolder(A, B, C, D) _mutt_enter_fname(A, B, C, D, 0, NULL, NULL, MUTT_SEL_VFOLDER)
int _mutt_enter_fname(const char *prompt, char *buf, size_t blen, int buffy,
                      int multiple, char ***files, int *numfiles, int flags);
int mutt_enter_string(char *buf, size_t buflen, int col, int flags);
int _mutt_enter_string(char *buf, size_t buflen, int col, int flags, int multiple,
                       char ***files, int *numfiles, struct EnterState *state);
#define mutt_get_field(A, B, C, D) _mutt_get_field(A, B, C, D, 0, NULL, NULL)
int _mutt_get_field(const char *field, char *buf, size_t buflen, int complete,
                    int multiple, char ***files, int *numfiles);
int mutt_get_hook_type(const char *name);
int mutt_get_field_unbuffered(char *msg, char *buf, size_t buflen, int flags);
#define mutt_get_password(A, B, C) mutt_get_field_unbuffered(A, B, C, MUTT_PASS)

int mutt_get_postponed(struct Context *ctx, struct Header *hdr, struct Header **cur, char *fcc, size_t fcclen);
int mutt_get_tmp_attachment(struct Body *a);
int mutt_index_menu(void);
int mutt_invoke_sendmail(struct Address *from, struct Address *to, struct Address *cc, struct Address *bcc,
                         const char *msg, int eightbit);
bool mutt_is_mail_list(struct Address *addr);
bool mutt_is_message_type(int type, const char *subtype);
bool mutt_is_subscribed_list(struct Address *addr);
bool mutt_is_text_part(struct Body *b);
int mutt_lookup_mime_type(struct Body *att, const char *path);
bool mutt_match_regex_list(const char *s, struct RegexList *l);
bool mutt_match_spam_list(const char *s, struct ReplaceList *l, char *text, int textsize);
int mutt_multi_choice(char *prompt, char *letters);
bool mutt_needs_mailcap(struct Body *m);
int mutt_num_postponed(int force);
int mutt_parse_bind(struct Buffer *buf, struct Buffer *s, unsigned long data, struct Buffer *err);
int mutt_parse_exec(struct Buffer *buf, struct Buffer *s, unsigned long data, struct Buffer *err);
int mutt_parse_color(struct Buffer *buff, struct Buffer *s, unsigned long data, struct Buffer *err);
int mutt_parse_uncolor(struct Buffer *buf, struct Buffer *s, unsigned long data, struct Buffer *err);
int mutt_parse_hook(struct Buffer *buf, struct Buffer *s, unsigned long data, struct Buffer *err);
int mutt_parse_macro(struct Buffer *buf, struct Buffer *s, unsigned long data, struct Buffer *err);
int mutt_parse_mailboxes(struct Buffer *path, struct Buffer *s, unsigned long data, struct Buffer *err);
int mutt_parse_unmailboxes(struct Buffer *path, struct Buffer *s, unsigned long data, struct Buffer *err);
int mutt_parse_mono(struct Buffer *buff, struct Buffer *s, unsigned long data, struct Buffer *err);
int mutt_parse_unmono(struct Buffer *buf, struct Buffer *s, unsigned long data, struct Buffer *err);
int mutt_parse_push(struct Buffer *buf, struct Buffer *s, unsigned long data, struct Buffer *err);
int mutt_parse_rc_line(/* const */ char *line, struct Buffer *token, struct Buffer *err);
int mutt_parse_rfc822_line(struct Envelope *e, struct Header *hdr, char *line, char *p,
                           short user_hdrs, short weed, short do_2047);
int mutt_parse_score(struct Buffer *buf, struct Buffer *s, unsigned long data, struct Buffer *err);
int mutt_parse_unscore(struct Buffer *buf, struct Buffer *s, unsigned long data, struct Buffer *err);
int mutt_parse_unhook(struct Buffer *buf, struct Buffer *s, unsigned long data, struct Buffer *err);
int mutt_pipe_attachment(FILE *fp, struct Body *b, const char *path, char *outfile);
int mutt_print_attachment(FILE *fp, struct Body *a);
int mutt_query_complete(char *buf, size_t buflen);
int mutt_query_variables(struct ListHead *queries);
int mutt_save_attachment(FILE *fp, struct Body *m, char *path, int flags, struct Header *hdr);
int _mutt_save_message(struct Header *h, struct Context *ctx, int delete, int decode, int decrypt);
int mutt_save_message(struct Header *h, int delete, int decode, int decrypt);
#ifdef USE_SMTP
int mutt_smtp_send(const struct Address *from, const struct Address *to, const struct Address *cc,
                   const struct Address *bcc, const char *msgfile, int eightbit);
#endif

size_t mutt_wstr_trunc(const char *src, size_t maxlen, size_t maxwid, size_t *width);
int mutt_charlen(const char *s, int *width);
int mutt_strwidth(const char *s);
int mutt_compose_menu(struct Header *msg, char *fcc, size_t fcclen, struct Header *cur, int flags);
int mutt_thread_set_flag(struct Header *hdr, int flag, int bf, int subthread);
void mutt_update_num_postponed(void);
int mutt_write_fcc(const char *path, struct Header *hdr, const char *msgid, int post,
                   char *fcc, char **finalpath);
int mutt_write_multiple_fcc(const char *path, struct Header *hdr, const char *msgid,
                            int post, char *fcc, char **finalpath);
int mutt_write_mime_body(struct Body *a, FILE *f);
int mutt_write_mime_header(struct Body *a, FILE *f);
int mutt_write_one_header(FILE *fp, const char *tag, const char *value,
                          const char *pfx, int wraplen, int flags);
int mutt_write_rfc822_header(FILE *fp, struct Envelope *env, struct Body *attach, int mode, int privacy);
void mutt_write_references(const struct ListHead *r, FILE *f, size_t trim);
int mutt_yesorno(const char *msg, int def);
void mutt_set_header_color(struct Context *ctx, struct Header *curhdr);
void mutt_sleep(short s);
int mutt_save_confirm(const char *s, struct stat *st);

void mutt_browser_select_dir(char *f);
void mutt_get_parent_path(char *output, char *path, size_t olen);

#define MUTT_RANDTAG_LEN (16)
void mutt_rand_base32(void *out, size_t len);
uint32_t mutt_rand32(void);
uint64_t mutt_rand64(void);

struct Address *alias_reverse_lookup(struct Address *a);

/* utf8.c */
int mutt_wctoutf8(char *s, unsigned int c, size_t buflen);

#ifdef LOCALES_HACK
#define IsPrint(c) (isprint((unsigned char) (c)) || ((unsigned char) (c) >= 0xa0))
#define IsWPrint(wc) (iswprint(wc) || wc >= 0xa0)
#else
#define IsPrint(c)                                                             \
  (isprint((unsigned char) (c)) ||                                             \
   (option(OPT_LOCALES) ? 0 : ((unsigned char) (c) >= 0xa0)))
#define IsWPrint(wc) (iswprint(wc) || (option(OPT_LOCALES) ? 0 : (wc >= 0xa0)))
#endif

int getdnsdomainname(char *d, size_t len);

/* unsorted */
void ci_bounce_message(struct Header *h);
int ci_send_message(int flags, struct Header *msg, char *tempfile, struct Context *ctx, struct Header *cur);

/* prototypes for compatibility functions */

#ifndef HAVE_WCSCASECMP
int wcscasecmp(const wchar_t *a, const wchar_t *b);
#endif

bool message_is_tagged(struct Context *ctx, int index);
bool message_is_visible(struct Context *ctx, int index);

#endif /* _MUTT_PROTOS_H */
