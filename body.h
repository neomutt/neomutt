/**
 * @file
 * Representation of the body of an email
 *
 * @authors
 * Copyright (C) 2017 Richard Russon <rich@flatcap.org>
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

#ifndef _MUTT_BODY_H
#define _MUTT_BODY_H

#include <stdbool.h>
#include <stdio.h>
#include <time.h>

#include "mutt/parameter.h"

/**
 * struct Body - The body of an email
 */
struct Body
{
  char *xtype;                  /**< content-type if x-unknown */
  char *subtype;                /**< content-type subtype */
  char *language;               /**< content-language (RFC8255) */
  struct ParameterList parameter;  /**< parameters of the content-type */
  char *description;            /**< content-description */
  char *form_name;              /**< Content-Disposition form-data name param */
  long hdr_offset;              /**< offset in stream where the headers begin.
                                 * this info is used when invoking metamail,
                                 * where we need to send the headers of the
                                 * attachment */
  LOFF_T offset;                /**< offset where the actual data begins */
  LOFF_T length;                /**< length (in bytes) of attachment */
  char *filename;               /**< when sending a message, this is the file
                                 * to which this structure refers */
  char *d_filename;             /**< filename to be used for the
                                 * content-disposition header.
                                 * If NULL, filename is used
                                 * instead.  */
  char *charset;                /**< charset of attached file */
  struct Content *content;      /**< structure used to store detailed info about
                                 * the content of the attachment.  this is used
                                 * to determine what content-transfer-encoding
                                 * is required when sending mail.  */
  struct Body *next;            /**< next attachment in the list */
  struct Body *parts;           /**< parts of a multipart or message/rfc822 */
  struct Header *hdr;           /**< header information for message/rfc822 */

  struct AttachPtr *aptr;       /**< Menu information, used in recvattach.c */

  signed short attach_count;

  time_t stamp;                 /**< time stamp of last encoding update.  */

  unsigned int type : 4;        /**< content-type primary type */
  unsigned int encoding : 3;    /**< content-transfer-encoding */
  unsigned int disposition : 2; /**< content-disposition */
  bool use_disp : 1;            /**< Content-Disposition uses filename= ? */
  bool unlink : 1;              /**< flag to indicate the file named by
                                 * "filename" should be unlink()ed before
                                 * free()ing this structure */
  bool tagged : 1;
  bool deleted : 1;             /**< attachment marked for deletion */

  bool noconv : 1;              /**< don't do character set conversion */
  bool force_charset : 1;
                                /**< send mode: don't adjust the character
   * set when in send-mode.
   */
  bool is_signed_data : 1;      /**< A lot of MUAs don't indicate S/MIME
                                 * signed-data correctly, e.g. they use foo.p7m
                                 * even for the name of signed data.  This flag
                                 * is used to keep track of the actual message
                                 * type.  It gets set during the verification
                                 * (which is done if the encryption try failed)
                                 * and check by the function to figure the type
                                 * of the message. */

  bool goodsig : 1;             /**< good cryptographic signature */
  bool warnsig : 1;             /**< maybe good signature */
  bool badsig : 1;              /**< bad cryptographic signature (needed to
                                 * check encrypted s/mime-signatures) */

  bool collapsed : 1;           /**< used by recvattach */
  bool attach_qualifies : 1;

};

struct Body *mutt_new_body(void);
int mutt_copy_body(FILE *fp, struct Body **tgt, struct Body *src);
void mutt_free_body(struct Body **p);

#endif /* _MUTT_BODY_H */
