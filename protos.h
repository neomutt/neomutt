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
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include "mutt.h"

struct Address;
struct AliasList;
struct Body;
struct Buffer;
struct Context;
struct EnterState;
struct Envelope;
struct Header;
struct ListHead;
struct ParameterList;

struct stat;
struct passwd;

/**
 * struct HdrFormatInfo - Data passed to index_format_str()
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

int mutt_system(const char *cmd);

void mutt_parse_content_type(char *s, struct Body *ct);
void mutt_generate_boundary(struct ParameterList *parm);

FILE *mutt_open_read(const char *path, pid_t *thepid);

char *mutt_extract_message_id(const char *s, const char **saveptr);

struct Address *mutt_default_from(void);
struct Address *mutt_remove_duplicates(struct Address *addr);
struct Address *mutt_remove_xrefs(struct Address *a, struct Address *b);

struct Body *mutt_make_file_attach(const char *path);
struct Body *mutt_make_message_attach(struct Context *ctx, struct Header *hdr, bool attach_msg);
struct Body *mutt_remove_multipart(struct Body *b);
struct Body *mutt_make_multipart(struct Body *b);
struct Body *mutt_parse_multipart(FILE *fp, const char *boundary, LOFF_T end_off, bool digest);
struct Body *mutt_rfc822_parse_message(FILE *fp, struct Body *parent);
struct Body *mutt_read_mime_header(FILE *fp, bool digest);

struct Content *mutt_get_content_info(const char *fname, struct Body *b);

char *mutt_rfc822_read_line(FILE *f, char *line, size_t *linelen);
struct Envelope *mutt_rfc822_read_header(FILE *f, struct Header *hdr, short user_hdrs, short weed);

int is_from(const char *s, char *path, size_t pathlen, time_t *tp);

char *mutt_expand_path(char *s, size_t slen);
char *mutt_expand_path_regex(char *s, size_t slen, bool regex);
char *mutt_find_hook(int type, const char *pat);
char *mutt_gecos_name(char *dest, size_t destlen, struct passwd *pw);
char *mutt_body_get_charset(struct Body *b, char *buf, size_t buflen);
void mutt_crypt_hook(struct ListHead *list, struct Address *addr);
void mutt_timeout_hook(void);
void mutt_startup_shutdown_hook(int type);
int mutt_set_xdg_path(enum XdgType type, char *buf, size_t bufsize);

const char *mutt_make_version(void);

const char *mutt_fqdn(bool may_hide_host);

void mutt_account_hook(const char *url);
void mutt_add_to_reference_headers(struct Envelope *env, struct Envelope *curenv);
void mutt_adv_mktemp(char *s, size_t l);
void mutt_alias_menu(char *buf, size_t buflen, struct AliasList *aliases);
int mutt_bounce_message(FILE *fp, struct Header *h, struct Address *to);
void mutt_check_stats(void);
int mutt_count_body_parts(struct Context *ctx, struct Header *hdr);
void mutt_default_save(char *path, size_t pathlen, struct Header *hdr);
void mutt_display_address(struct Envelope *env);
int mutt_edit_content_type (struct Header *h, struct Body *b, FILE *fp);
void mutt_edit_file(const char *editor, const char *data);
void mutt_encode_descriptions(struct Body *b, short recurse);
void mutt_encode_path(char *dest, size_t dlen, const char *src);
void mutt_enter_command(void);
void mutt_expand_file_fmt(char *dest, size_t destlen, const char *fmt, const char *src);
void mutt_expand_fmt(char *dest, size_t destlen, const char *fmt, const char *src);
void mutt_fix_reply_recipients(struct Envelope *env);
void mutt_folder_hook(const char *path);
void mutt_simple_format(char *buf, size_t buflen, int min_width, int max_width,
                        int justify, char pad_char, const char *s, size_t n, int arboreal);
void mutt_format_s(char *buf, size_t buflen, const char *prec, const char *s);
void mutt_format_s_tree(char *buf, size_t buflen, const char *prec, const char *s);
void mutt_forward_intro(struct Context *ctx, struct Header *cur, FILE *fp);
void mutt_forward_trailer(struct Context *ctx, struct Header *cur, FILE *fp);
void mutt_free_color(int fg, int bg);
void mutt_free_colors(void);
void mutt_help(int menu);
void mutt_make_attribution(struct Context *ctx, struct Header *cur, FILE *out);
void mutt_make_forward_subject(struct Envelope *env, struct Context *ctx, struct Header *cur);
void mutt_make_help(char *d, size_t dlen, const char *txt, int menu, int op);
void mutt_make_misc_reply_headers(struct Envelope *env, struct Envelope *curenv);
void mutt_make_post_indent(struct Context *ctx, struct Header *cur, FILE *out);
void mutt_message_to_7bit(struct Body *a, FILE *fp);
void mutt_mktemp_full(char *s, size_t slen, const char *prefix, const char *suffix, const char *src, int line);
#define mutt_mktemp_pfx_sfx(a, b, c, d) mutt_mktemp_full(a, b, c, d, __FILE__, __LINE__)
#define mutt_mktemp(a, b) mutt_mktemp_pfx_sfx(a, b, "neomutt", NULL)
void mutt_paddstr(int n, const char *s);
void mutt_parse_mime_message(struct Context *ctx, struct Header *cur);
void mutt_parse_part(FILE *fp, struct Body *b);
void mutt_perror_debug(const char *s);
void mutt_prepare_envelope(struct Envelope *env, bool final);
void mutt_unprepare_envelope(struct Envelope *env);
void mutt_pretty_mailbox(char *s, size_t buflen);
void mutt_pipe_message(struct Header *h);
void mutt_print_message(struct Header *h);
void mutt_query_exit(void);
void mutt_safe_path(char *s, size_t l, struct Address *a);
void mutt_save_path(char *d, size_t dsize, struct Address *a);
void mutt_select_fcc(char *path, size_t pathlen, struct Header *hdr);
void mutt_message_hook(struct Context *ctx, struct Header *hdr, int type);
void mutt_set_flag_update(struct Context *ctx, struct Header *h, int flag, bool bf, bool upd_ctx);
#define mutt_set_flag(a, b, c, d) mutt_set_flag_update(a, b, c, d, true)
void mutt_set_followup_to(struct Envelope *e);
void mutt_shell_escape(void);
void mutt_show_error(void);
void mutt_signal_init(void);
void mutt_stamp_attachment(struct Body *a);
void mutt_tag_set_flag(int flag, int bf);
void mutt_update_encoding(struct Body *a);
void mutt_version(void);
void mutt_write_address_list(struct Address *addr, FILE *fp, int linelen, bool display);
int mutt_addwch(wchar_t wc);
int mutt_alloc_color(int fg, int bg);
int mutt_combine_color(int fg_attr, int bg_attr);
int mutt_any_key_to_continue(const char *s);
int mutt_change_flag(struct Header *h, int bf);
int mutt_check_encoding(const char *c);

int mutt_check_mime_type(const char *s);
int mutt_check_overwrite(const char *attname, const char *path, char *fname,
                         size_t flen, int *append, char **directory);
int mutt_check_traditional_pgp(struct Header *h, int *redraw);
int mutt_complete(char *buf, size_t buflen);
int mutt_display_message(struct Header *cur);
int mutt_edit_message(struct Context *ctx, struct Header *hdr);
int mutt_view_message(struct Context *ctx, struct Header *hdr);
int mutt_fetch_recips(struct Envelope *out, struct Envelope *in, int flags);
int mutt_prepare_template(FILE *fp, struct Context *ctx, struct Header *newhdr, struct Header *hdr, short resend);
int mutt_resend_message(FILE *fp, struct Context *ctx, struct Header *cur);
int mutt_compose_to_sender(struct Header *hdr);
#define mutt_enter_fname(A, B, C, D)   mutt_enter_fname_full(A, B, C, D, 0, NULL, NULL, 0)
#define mutt_enter_vfolder(A, B, C, D) mutt_enter_fname_full(A, B, C, D, 0, NULL, NULL, MUTT_SEL_VFOLDER)
int mutt_enter_fname_full(const char *prompt, char *buf, size_t blen, int buffy,
                      int multiple, char ***files, int *numfiles, int flags);
int mutt_enter_string(char *buf, size_t buflen, int col, int flags);
int mutt_enter_string_full(char *buf, size_t buflen, int col, int flags, int multiple,
                       char ***files, int *numfiles, struct EnterState *state);
#define mutt_get_field(A, B, C, D) mutt_get_field_full(A, B, C, D, 0, NULL, NULL)
int mutt_get_field_full(const char *field, char *buf, size_t buflen, int complete,
                    int multiple, char ***files, int *numfiles);
int mutt_get_field_unbuffered(char *msg, char *buf, size_t buflen, int flags);
#define mutt_get_password(A, B, C) mutt_get_field_unbuffered(A, B, C, MUTT_PASS)

int mutt_get_postponed(struct Context *ctx, struct Header *hdr, struct Header **cur, char *fcc, size_t fcclen);
int mutt_parse_crypt_hdr(const char *p, int set_empty_signas, int crypt_app);
int mutt_invoke_sendmail(struct Address *from, struct Address *to, struct Address *cc, struct Address *bcc,
                         const char *msg, int eightbit);
bool mutt_is_text_part(struct Body *b);
int mutt_lookup_mime_type(struct Body *att, const char *path);
int mutt_multi_choice(char *prompt, char *letters);
bool mutt_needs_mailcap(struct Body *m);
int mutt_num_postponed(int force);
int mutt_parse_color(struct Buffer *buf, struct Buffer *s, unsigned long data, struct Buffer *err);
int mutt_parse_uncolor(struct Buffer *buf, struct Buffer *s, unsigned long data, struct Buffer *err);
int mutt_parse_hook(struct Buffer *buf, struct Buffer *s, unsigned long data, struct Buffer *err);
int mutt_parse_mono(struct Buffer *buf, struct Buffer *s, unsigned long data, struct Buffer *err);
int mutt_parse_unmono(struct Buffer *buf, struct Buffer *s, unsigned long data, struct Buffer *err);
int mutt_rfc822_parse_line(struct Envelope *e, struct Header *hdr, char *line, char *p,
                           short user_hdrs, short weed, short do_2047);
int mutt_parse_unhook(struct Buffer *buf, struct Buffer *s, unsigned long data, struct Buffer *err);
void mutt_delete_hooks(int type);
int mutt_save_message_ctx(struct Header *h, int delete, int decode, int decrypt, struct Context *ctx);
int mutt_save_message(struct Header *h, int delete, int decode, int decrypt);

size_t mutt_wstr_trunc(const char *src, size_t maxlen, size_t maxwid, size_t *width);
int mutt_strwidth(const char *s);
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
int mutt_rfc822_write_header(FILE *fp, struct Envelope *env, struct Body *attach, int mode, bool privacy);
void mutt_write_references(const struct ListHead *r, FILE *f, size_t trim);
int mutt_yesorno(const char *msg, int def);
void mutt_sleep(short s);
int mutt_save_confirm(const char *s, struct stat *st);

void mutt_get_parent_path(char *output, char *path, size_t olen);
size_t mutt_realpath(char *buf);

#define MUTT_RANDTAG_LEN 16
void mutt_rand_base32(void *out, size_t len);
uint32_t mutt_rand32(void);
uint64_t mutt_rand64(void);
int mutt_randbuf(void *out, size_t len);

int getdnsdomainname(char *d, size_t len);

void ci_bounce_message(struct Header *h);
int ci_send_message(int flags, struct Header *msg, char *tempfile, struct Context *ctx, struct Header *cur);

#ifndef HAVE_WCSCASECMP
int wcscasecmp(const wchar_t *a, const wchar_t *b);
#endif

bool message_is_tagged(struct Context *ctx, int index);
bool message_is_visible(struct Context *ctx, int index);

#endif /* _MUTT_PROTOS_H */
