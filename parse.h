/* $Id$ */

/* XXX this file duplicates content from protos.h */

BODY *mutt_parse_multipart (FILE *, const char *, long, int);
BODY *mutt_parse_messageRFC822 (FILE *, BODY *);
BODY *mutt_read_mime_header (FILE *, int);

ENVELOPE *mutt_read_rfc822_header (FILE *, HEADER *, short);

time_t is_from (const char *, char *, size_t);
