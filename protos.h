/**
 * Copyright (C) 1996-2000,2007,2010,2013 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 2013 Karel Zak <kzak@redhat.com>
 *
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
#define _MUTT_PROTOS_H 1

#include <inttypes.h>
#include "mbyte.h"

#define MoreArgs(p) (*p->dptr && *p->dptr != ';' && *p->dptr != '#')

#define mutt_make_string(A, B, C, D, E) _mutt_make_string(A, B, C, D, E, 0)
void _mutt_make_string(char *dest, size_t destlen, const char *s, CONTEXT *ctx,
                       HEADER *hdr, format_flag flags);

struct hdr_format_info
{
  CONTEXT *ctx;
  HEADER *hdr;
  const char *pager_progress;
};

typedef enum {
  kXDGConfigHome, /* $XDG_CONFIG_HOME */
  kXDGConfigDirs, /* $XDG_CONFIG_DIRS */
} XDGType;

void mutt_make_string_info(char *dst, size_t dstlen, int cols, const char *s,
                           struct hdr_format_info *hfi, format_flag flags);

void mutt_free_opts(void);

#define mutt_system(x) _mutt_system(x, 0)
int _mutt_system(const char *cmd, int flags);

#define mutt_next_thread(x) _mutt_aside_thread(x, 1, 0)
#define mutt_previous_thread(x) _mutt_aside_thread(x, 0, 0)
#define mutt_next_subthread(x) _mutt_aside_thread(x, 1, 1)
#define mutt_previous_subthread(x) _mutt_aside_thread(x, 0, 1)
int _mutt_aside_thread(HEADER *hdr, short dir, short subthreads);

#define mutt_collapse_thread(x, y) _mutt_traverse_thread(x, y, MUTT_THREAD_COLLAPSE)
#define mutt_uncollapse_thread(x, y) _mutt_traverse_thread(x, y, MUTT_THREAD_UNCOLLAPSE)
#define mutt_get_hidden(x, y) _mutt_traverse_thread(x, y, MUTT_THREAD_GET_HIDDEN)
#define mutt_thread_contains_unread(x, y) _mutt_traverse_thread(x, y, MUTT_THREAD_UNREAD)
#define mutt_thread_next_unread(x, y) _mutt_traverse_thread(x, y, MUTT_THREAD_NEXT_UNREAD)
int _mutt_traverse_thread(CONTEXT *ctx, HEADER *cur, int flag);

typedef const char *format_t(char *, size_t, size_t, int, char, const char *,
                             const char *, const char *, const char *,
                             unsigned long, format_flag);

void mutt_FormatString(char *dest,         /* output buffer */
                       size_t destlen,     /* output buffer len */
                       size_t col,         /* starting column (nonzero when called recursively) */
                       int cols,           /* maximum columns */
                       const char *src,    /* template string */
                       format_t *callback, /* callback for processing */
                       unsigned long data, /* callback data */
                       format_flag flags); /* callback flags */

void mutt_parse_content_type(char *s, BODY *ct);
void mutt_generate_boundary(PARAMETER **parm);
void mutt_delete_parameter(const char *attribute, PARAMETER **p);
void mutt_set_parameter(const char *attribute, const char *value, PARAMETER **p);

#ifdef USE_NOTMUCH
int mutt_parse_virtual_mailboxes(BUFFER *path, BUFFER *s, unsigned long data, BUFFER *err);
int mutt_parse_unvirtual_mailboxes(BUFFER *path, BUFFER *s, unsigned long data, BUFFER *err);
#endif

FILE *mutt_open_read(const char *path, pid_t *thepid);

void set_quadoption(int opt, int flag);
int query_quadoption(int opt, const char *prompt);
int quadoption(int opt);

char *mutt_extract_message_id(const char *s, const char **saveptr);

struct Address *mutt_default_from(void);
struct Address *mutt_get_address(ENVELOPE *env, char **pfxp);
struct Address *mutt_lookup_alias(const char *s);
struct Address *mutt_remove_duplicates(struct Address *addr);
struct Address *mutt_remove_xrefs(struct Address *a, struct Address *b);
struct Address *mutt_expand_aliases(struct Address *a);
struct Address *mutt_parse_adrlist(struct Address *p, const char *s);

BODY *mutt_make_file_attach(const char *path);
BODY *mutt_make_message_attach(CONTEXT *ctx, HEADER *hdr, int attach_msg);
BODY *mutt_remove_multipart(BODY *b);
BODY *mutt_make_multipart(BODY *b);
BODY *mutt_new_body(void);
BODY *mutt_parse_multipart(FILE *fp, const char *boundary, LOFF_T end_off, int digest);
BODY *mutt_parse_message_rfc822(FILE *fp, BODY *parent);
BODY *mutt_read_mime_header(FILE *fp, int digest);

CONTENT *mutt_get_content_info(const char *fname, BODY *b);

HASH *mutt_make_id_hash(CONTEXT *ctx);

char *mutt_read_rfc822_line(FILE *f, char *line, size_t *linelen);
ENVELOPE *mutt_read_rfc822_header(FILE *f, HEADER *hdr, short user_hdrs, short weed);

void mutt_set_mtime(const char *from, const char *to);
time_t mutt_decrease_mtime(const char *f, struct stat *st);
time_t mutt_local_tz(time_t t);
time_t mutt_mktime(struct tm *t, int local);
time_t mutt_parse_date(const char *s, HEADER *h);
int is_from(const char *s, char *path, size_t pathlen, time_t *tp);
void mutt_touch_atime(int f);

const char *mutt_attach_fmt(char *dest, size_t destlen, size_t col, int cols,
                            char op, const char *src, const char *prefix,
                            const char *ifstring, const char *elsestring,
                            unsigned long data, format_flag flags);


char *mutt_charset_hook(const char *chs);
char *mutt_iconv_hook(const char *chs);
char *mutt_expand_path(char *s, size_t slen);
char *_mutt_expand_path(char *s, size_t slen, int rx);
char *mutt_find_hook(int type, const char *pat);
char *mutt_gecos_name(char *dest, size_t destlen, struct passwd *pw);
char *mutt_get_body_charset(char *d, size_t dlen, BODY *b);
const char *mutt_get_name(struct Address *a);
char *mutt_get_parameter(const char *s, PARAMETER *p);
LIST *mutt_crypt_hook(struct Address *adr);
char *mutt_make_date(char *s, size_t len);
void mutt_timeout_hook(void);
void mutt_startup_shutdown_hook(int type);
int mutt_set_xdg_path(const XDGType type, char *buf, size_t bufsize);

const char *mutt_make_version(void);

const char *mutt_fqdn(short may_hide_host);

group_t *mutt_pattern_group(const char *k);

REGEXP *mutt_compile_regexp(const char *s, int flags);

void mutt_account_hook(const char *url);
void mutt_add_to_reference_headers(ENVELOPE *env, ENVELOPE *curenv, LIST ***pp,
                                   LIST ***qq);
void mutt_adv_mktemp(char *s, size_t l);
void mutt_alias_menu(char *buf, size_t buflen, ALIAS *aliases);
void mutt_allow_interrupt(int disposition);
void mutt_attach_init(BODY *b);
void mutt_block_signals(void);
void mutt_block_signals_system(void);
int mutt_body_handler(BODY *b, STATE *s);
int mutt_bounce_message(FILE *fp, HEADER *h, struct Address *to);
void mutt_break_thread(HEADER *hdr);
void mutt_buffy(char *s, size_t slen);
int mutt_buffy_list(void);
void mutt_canonical_charset(char *dest, size_t dlen, const char *name);
int mutt_count_body_parts(CONTEXT *ctx, HEADER *hdr);
void mutt_check_rescore(CONTEXT *ctx);
void mutt_clear_error(void);
void mutt_clear_pager_position(void);
void mutt_create_alias(ENVELOPE *cur, struct Address *iadr);
void mutt_decode_attachment(BODY *b, STATE *s);
void mutt_decode_base64(STATE *s, long len, int istext, iconv_t cd);
void mutt_default_save(char *path, size_t pathlen, HEADER *hdr);
void mutt_display_address(ENVELOPE *env);
void mutt_draw_statusline(int cols, const char *buf, int buflen);
void mutt_edit_content_type(HEADER *h, BODY *b, FILE *fp);
void mutt_edit_file(const char *editor, const char *data);
void mutt_edit_headers(const char *editor, const char *body, HEADER *msg,
                       char *fcc, size_t fcclen);
char **mutt_envlist(void);
void mutt_envlist_set(const char *name, const char *value, bool overwrite);
int mutt_filter_unprintable(char **s);
int mutt_label_message(HEADER *hdr);
void mutt_make_label_hash(CONTEXT *ctx);
void mutt_label_hash_add(CONTEXT *ctx, HEADER *hdr);
void mutt_label_hash_remove(CONTEXT *ctx, HEADER *hdr);
int mutt_label_complete(char *buffer, size_t len, int numtabs);
void mutt_curses_error(const char *fmt, ...);
void mutt_curses_message(const char *fmt, ...);
void mutt_encode_descriptions(BODY *b, short recurse);
void mutt_encode_path(char *dest, size_t dlen, const char *src);
void mutt_enter_command(void);
void mutt_expand_aliases_env(ENVELOPE *env);
void mutt_expand_file_fmt(char *dest, size_t destlen, const char *fmt, const char *src);
void mutt_expand_fmt(char *dest, size_t destlen, const char *fmt, const char *src);
void mutt_fix_reply_recipients(ENVELOPE *env);
void mutt_folder_hook(char *path);
void mutt_format_string(char *dest, size_t destlen, int min_width, int max_width, int justify,
                        char m_pad_char, const char *s, size_t n, int arboreal);
void mutt_format_s(char *dest, size_t destlen, const char *prefix, const char *s);
void mutt_format_s_tree(char *dest, size_t destlen, const char *prefix, const char *s);
void mutt_forward_intro(FILE *fp, HEADER *cur);
void mutt_forward_trailer(FILE *fp);
void mutt_free_alias(ALIAS **p);
void mutt_free_body(BODY **p);
void mutt_free_color(int fg, int bg);
void mutt_free_enter_state(ENTER_STATE **esp);
void mutt_free_envelope(ENVELOPE **p);
void mutt_free_header(HEADER **h);
void mutt_free_parameter(PARAMETER **p);
void mutt_free_regexp(REGEXP **pp);
void mutt_help(int menu);
void mutt_draw_tree(CONTEXT *ctx);
void mutt_check_lookup_list(BODY *b, char *type, int len);
void mutt_make_attribution(CONTEXT *ctx, HEADER *cur, FILE *out);
void mutt_make_forward_subject(ENVELOPE *env, CONTEXT *ctx, HEADER *cur);
void mutt_make_help(char *d, size_t dlen, const char *txt, int menu, int op);
void mutt_make_misc_reply_headers(ENVELOPE *env, CONTEXT *ctx, HEADER *cur, ENVELOPE *curenv);
void mutt_make_post_indent(CONTEXT *ctx, HEADER *cur, FILE *out);
void mutt_merge_envelopes(ENVELOPE *base, ENVELOPE **extra);
void mutt_message_to_7bit(BODY *a, FILE *fp);
#define mutt_mktemp(a, b) mutt_mktemp_pfx_sfx(a, b, "mutt", NULL)
#define mutt_mktemp_pfx_sfx(a, b, c, d) _mutt_mktemp(a, b, c, d, __FILE__, __LINE__)
void _mutt_mktemp(char *s, size_t slen, const char *prefix, const char *suffix,
                  const char *src, int line);
void mutt_normalize_time(struct tm *tm);
void mutt_paddstr(int n, const char *s);
void mutt_parse_mime_message(CONTEXT *ctx, HEADER *cur);
void mutt_parse_part(FILE *fp, BODY *b);
void mutt_perror(const char *s);
void mutt_prepare_envelope(ENVELOPE *env, int final);
void mutt_unprepare_envelope(ENVELOPE *env);
void mutt_pretty_mailbox(char *s, size_t buflen);
void mutt_pretty_size(char *s, size_t len, LOFF_T n);
void mutt_pipe_message(HEADER *h);
void mutt_print_message(HEADER *h);
void mutt_query_exit(void);
void mutt_query_menu(char *buf, size_t buflen);
void mutt_safe_path(char *s, size_t l, struct Address *a);
void mutt_save_path(char *d, size_t dsize, struct Address *a);
void mutt_score_message(CONTEXT *ctx, HEADER *hdr, int upd_ctx);
void mutt_select_fcc(char *path, size_t pathlen, HEADER *hdr);
#define mutt_select_file(A, B, C) _mutt_select_file(A, B, C, NULL, NULL)
void _mutt_select_file(char *f, size_t flen, int flags, char ***files, int *numfiles);
void mutt_message_hook(CONTEXT *ctx, HEADER *hdr, int type);
void _mutt_set_flag(CONTEXT *ctx, HEADER *h, int flag, int bf, int upd_ctx);
#define mutt_set_flag(a, b, c, d) _mutt_set_flag(a, b, c, d, 1)
void mutt_set_followup_to(ENVELOPE *e);
void mutt_shell_escape(void);
void mutt_show_error(void);
void mutt_signal_init(void);
void mutt_stamp_attachment(BODY *a);
void mutt_tag_set_flag(int flag, int bf);
bool mutt_ts_capability(void);
void mutt_unblock_signals(void);
void mutt_unblock_signals_system(int catch);
void mutt_update_encoding(BODY *a);
void mutt_version(void);
void mutt_view_attachments(HEADER *hdr);
void mutt_write_address_list(struct Address *adr, FILE *fp, int linelen, int display);
void mutt_set_virtual(CONTEXT *ctx);
int mutt_add_to_rx_list(RX_LIST **list, const char *s, int flags, BUFFER *err);
bool mutt_addr_is_user(struct Address *addr);
int mutt_addwch(wchar_t wc);
int mutt_alias_complete(char *s, size_t buflen);
void mutt_alias_add_reverse(ALIAS *t);
void mutt_alias_delete_reverse(ALIAS *t);
int mutt_alloc_color(int fg, int bg);
int mutt_any_key_to_continue(const char *s);
char *mutt_apply_replace(char *dbuf, size_t dlen, char *sbuf, REPLACE_LIST *rlist);
int mutt_buffy_check(int force);
int mutt_buffy_notify(void);
int mutt_builtin_editor(const char *path, HEADER *msg, HEADER *cur);
int mutt_can_decode(BODY *a);
int mutt_change_flag(HEADER *h, int bf);
int mutt_check_encoding(const char *c);

int mutt_check_mime_type(const char *s);
int mutt_check_month(const char *s);
int mutt_check_overwrite(const char *attname, const char *path, char *fname,
                         size_t flen, int *append, char **directory);
int mutt_check_traditional_pgp(HEADER *h, int *redraw);
int mutt_command_complete(char *buffer, size_t len, int pos, int numtabs);
int mutt_var_value_complete(char *buffer, size_t len, int pos);
#ifdef USE_NOTMUCH
bool mutt_nm_query_complete(char *buffer, size_t len, int pos, int numtabs);
bool mutt_nm_tag_complete(char *buffer, size_t len, int pos, int numtabs);
#endif
int mutt_complete(char *s, size_t slen);
int mutt_compose_attachment(BODY *a);
int mutt_copy_body(FILE *fp, BODY **tgt, BODY *src);
int mutt_decode_save_attachment(FILE *fp, BODY *m, char *path, int displaying, int flags);
int mutt_display_message(HEADER *cur);
int mutt_dump_variables(int hide_sensitive);
int mutt_edit_attachment(BODY *a);
int mutt_edit_message(CONTEXT *ctx, HEADER *hdr);
int mutt_fetch_recips(ENVELOPE *out, ENVELOPE *in, int flags);
int mutt_chscmp(const char *s, const char *chs);
#define mutt_is_utf8(a) mutt_chscmp(a, "utf-8")
#define mutt_is_us_ascii(a) mutt_chscmp(a, "us-ascii")
int mutt_parent_message(CONTEXT *ctx, HEADER *hdr, int find_root);
int mutt_prepare_template(FILE *fp, CONTEXT *ctx, HEADER *newhdr, HEADER *hdr, short resend);
int mutt_resend_message(FILE *fp, CONTEXT *ctx, HEADER *cur);
int mutt_compose_to_sender(HEADER *hdr);
#define mutt_enter_fname(A, B, C, D) _mutt_enter_fname(A, B, C, D, 0, NULL, NULL, 0)
#define mutt_enter_vfolder(A, B, C, D) _mutt_enter_fname(A, B, C, D, 0, NULL, NULL, MUTT_SEL_VFOLDER)
int _mutt_enter_fname(const char *prompt, char *buf, size_t blen, int buffy,
                      int multiple, char ***files, int *numfiles, int flags);
int mutt_enter_string(char *buf, size_t buflen, int col, int flags);
int _mutt_enter_string(char *buf, size_t buflen, int col, int flags, int multiple,
                       char ***files, int *numfiles, ENTER_STATE *state);
#define mutt_get_field(A, B, C, D) _mutt_get_field(A, B, C, D, 0, NULL, NULL)
int _mutt_get_field(const char *field, char *buf, size_t buflen, int complete,
                    int multiple, char ***files, int *numfiles);
int mutt_get_hook_type(const char *name);
int mutt_get_field_unbuffered(char *msg, char *buf, size_t buflen, int flags);
#define mutt_get_password(A, B, C) mutt_get_field_unbuffered(A, B, C, MUTT_PASS)

int mutt_get_postponed(CONTEXT *ctx, HEADER *hdr, HEADER **cur, char *fcc, size_t fcclen);
int mutt_get_tmp_attachment(BODY *a);
int mutt_index_menu(void);
int mutt_invoke_sendmail(struct Address *from, struct Address *to, struct Address *cc, struct Address *bcc,
                         const char *msg, int eightbit);
bool mutt_is_mail_list(struct Address *addr);
bool mutt_is_message_type(int type, const char *subtype);
int mutt_is_list_cc(int alladdr, struct Address *a1, struct Address *a2);
int mutt_is_list_recipient(int alladdr, struct Address *a1, struct Address *a2);
bool mutt_is_subscribed_list(struct Address *addr);
bool mutt_is_text_part(BODY *b);
int mutt_link_threads(HEADER *cur, HEADER *last, CONTEXT *ctx);
int mutt_lookup_mime_type(BODY *att, const char *path);
bool mutt_match_rx_list(const char *s, RX_LIST *l);
bool mutt_match_spam_list(const char *s, REPLACE_LIST *l, char *text, int textsize);
int mutt_messages_in_thread(CONTEXT *ctx, HEADER *hdr, int flag);
int mutt_multi_choice(char *prompt, char *letters);
bool mutt_needs_mailcap(BODY *m);
int mutt_num_postponed(int force);
int mutt_parse_bind(BUFFER *buf, BUFFER *s, unsigned long data, BUFFER *err);
int mutt_parse_exec(BUFFER *buf, BUFFER *s, unsigned long data, BUFFER *err);
int mutt_parse_color(BUFFER *buff, BUFFER *s, unsigned long data, BUFFER *err);
int mutt_parse_uncolor(BUFFER *buf, BUFFER *s, unsigned long data, BUFFER *err);
int mutt_parse_hook(BUFFER *buf, BUFFER *s, unsigned long data, BUFFER *err);
int mutt_parse_macro(BUFFER *buf, BUFFER *s, unsigned long data, BUFFER *err);
int mutt_parse_mailboxes(BUFFER *path, BUFFER *s, unsigned long data, BUFFER *err);
int mutt_parse_mono(BUFFER *buff, BUFFER *s, unsigned long data, BUFFER *err);
int mutt_parse_unmono(BUFFER *buf, BUFFER *s, unsigned long data, BUFFER *err);
int mutt_parse_push(BUFFER *buf, BUFFER *s, unsigned long data, BUFFER *err);
int mutt_parse_rc_line(/* const */ char *line, BUFFER *token, BUFFER *err);
int mutt_parse_rfc822_line(ENVELOPE *e, HEADER *hdr, char *line, char *p,
                           short user_hdrs, short weed, short do_2047, LIST **lastp);
int mutt_parse_score(BUFFER *buf, BUFFER *s, unsigned long data, BUFFER *err);
int mutt_parse_unscore(BUFFER *buf, BUFFER *s, unsigned long data, BUFFER *err);
int mutt_parse_unhook(BUFFER *buf, BUFFER *s, unsigned long data, BUFFER *err);
int mutt_pattern_func(int op, char *prompt);
int mutt_pipe_attachment(FILE *fp, BODY *b, const char *path, char *outfile);
int mutt_print_attachment(FILE *fp, BODY *a);
int mutt_query_complete(char *buf, size_t buflen);
int mutt_query_variables(LIST *queries);
int mutt_save_attachment(FILE *fp, BODY *m, char *path, int flags, HEADER *hdr);
int _mutt_save_message(HEADER *h, CONTEXT *ctx, int delete, int decode, int decrypt);
int mutt_save_message(HEADER *h, int delete, int decode, int decrypt);
int mutt_search_command(int cur, int op);
#ifdef USE_SMTP
int mutt_smtp_send(const struct Address *from, const struct Address *to, const struct Address *cc,
                   const struct Address *bcc, const char *msgfile, int eightbit);
#endif

size_t mutt_wstr_trunc(const char *src, size_t maxlen, size_t maxwid, size_t *width);
int mutt_charlen(const char *s, int *width);
int mutt_strwidth(const char *s);
int mutt_compose_menu(HEADER *msg, char *fcc, size_t fcclen, HEADER *cur, int flags);
int mutt_thread_set_flag(HEADER *hdr, int flag, int bf, int subthread);
void mutt_update_num_postponed(void);
int mutt_which_case(const char *s);
int mutt_write_fcc(const char *path, HEADER *hdr, const char *msgid, int post,
                   char *fcc, char **finalpath);
int mutt_write_multiple_fcc(const char *path, HEADER *hdr, const char *msgid,
                            int post, char *fcc, char **finalpath);
int mutt_write_mime_body(BODY *a, FILE *f);
int mutt_write_mime_header(BODY *a, FILE *f);
int mutt_write_one_header(FILE *fp, const char *tag, const char *value,
                          const char *pfx, int wraplen, int flags);
int mutt_write_rfc822_header(FILE *fp, ENVELOPE *env, BODY *attach, int mode, int privacy);
void mutt_write_references(LIST *r, FILE *f, int trim);
int mutt_yesorno(const char *msg, int def);
void mutt_set_header_color(CONTEXT *ctx, HEADER *curhdr);
void mutt_sleep(short s);
int mutt_save_confirm(const char *s, struct stat *st);

void mutt_browser_select_dir(char *f);
void mutt_get_parent_path(char *output, char *path, size_t olen);

#define MUTT_RANDTAG_LEN (16)
void mutt_rand_base32(void *out, size_t len);
uint32_t mutt_rand32(void);
uint64_t mutt_rand64(void);


struct Address *alias_reverse_lookup(struct Address *a);

/* base64.c */
size_t mutt_to_base64(char *out, const char *cin, size_t len, size_t olen);
int mutt_from_base64(char *out, const char *in);

/* utf8.c */
int mutt_wctoutf8(char *s, unsigned int c, size_t buflen);

#ifdef LOCALES_HACK
#define IsPrint(c) (isprint((unsigned char) (c)) || ((unsigned char) (c) >= 0xa0))
#define IsWPrint(wc) (iswprint(wc) || wc >= 0xa0)
#else
#define IsPrint(c)                                                             \
  (isprint((unsigned char) (c)) ||                                             \
   (option(OPTLOCALES) ? 0 : ((unsigned char) (c) >= 0xa0)))
#define IsWPrint(wc) (iswprint(wc) || (option(OPTLOCALES) ? 0 : (wc >= 0xa0)))
#endif

static inline pattern_t *new_pattern(void)
{
  return safe_calloc(1, sizeof(pattern_t));
}

int mutt_pattern_exec(struct pattern_t *pat, pattern_exec_flag flags,
                      CONTEXT *ctx, HEADER *h, pattern_cache_t *cache);
pattern_t *mutt_pattern_comp(/* const */ char *s, int flags, BUFFER *err);
void mutt_check_simple(char *s, size_t len, const char *simple);
void mutt_pattern_free(pattern_t **pat);

int getdnsdomainname(char *, size_t);

/* unsorted */
void ci_bounce_message(HEADER *h);
int ci_send_message(int flags, HEADER *msg, char *tempfile, CONTEXT *ctx, HEADER *cur);

/* prototypes for compatibility functions */

#ifndef HAVE_WCSCASECMP
int wcscasecmp(const wchar_t *a, const wchar_t *b);
#endif

#ifndef HAVE_STRCASESTR
char *strcasestr(const char *s1, const char *s2);
#endif

#endif /* _MUTT_PROTOS_H */
