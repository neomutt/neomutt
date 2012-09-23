/*
 * Copyright (C) 2011 Karel Zak <kzak@redhat.com>
 */
#ifndef _MUTT_NOTMUCH_H_
#define _MUTT_NOTMUCH_H_ 1

int nm_read_query(CONTEXT *ctx);
int nm_sync(CONTEXT * ctx, int *index_hint);
int nm_check_database(CONTEXT * ctx, int *index_hint);
char *nm_header_get_folder(HEADER *h);
int nm_header_get_magic(HEADER *h);
char *nm_header_get_fullpath(HEADER *h, char *buf, size_t bufsz);
char *nm_header_get_tags(HEADER *h);
int nm_update_filename(CONTEXT *ctx, const char *o, const char *n, HEADER *h);
char *nm_uri_from_query(CONTEXT *ctx, char *buf, size_t bufsz);
int nm_modify_message_tags(CONTEXT *ctx, HEADER *hdr, char *tags);

void nm_longrun_init(CONTEXT *cxt, int writable);
void nm_longrun_done(CONTEXT *cxt);

char *nm_get_description(CONTEXT *ctx);

int nm_record_message(CONTEXT *ctx, char *path, HEADER *h);

void nm_debug_check(CONTEXT *ctx);
int nm_get_all_tags(CONTEXT *ctx, char **tag_list, int *tag_count);

/*
 * functions usable outside notmuch CONTEXT
 */
int nm_nonctx_get_count(char *path, int *all, int *new);

#endif /* _MUTT_NOTMUCH_H_ */
