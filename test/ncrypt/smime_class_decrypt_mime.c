/**
 * @file
 * Test code for smime_class_decrypt_mime()
 *
 * @authors
 * Copyright (C) 2026 Alejandro Pacheco <alepacheco99@gmail.com>
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

#define TEST_NO_MAIN
#include "config.h"
#include "acutest.h"
#include <stdio.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "ncrypt/smime.h"

void test_smime_class_decrypt_mime(void)
{
  // int smime_class_decrypt_mime(FILE *fp_in, FILE **fp_out, struct Body *b, struct Body **b_dec);

  {
    struct Buffer *err = buf_pool_get();
    int rc = cs_str_string_set(NeoMutt->sub->cs, "smime_verify_opaque_command", "true", err);
    buf_pool_release(&err);
    if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
      return;

    FILE *fp_in = mutt_file_mkstemp();
    TEST_CHECK(fp_in != NULL);
    fputs("MIAGCSqGSIb3DQEHAqCAMIACAQExDzANBglghkgBZQMEAgEFADCABgkqhkiG9w0B\n", fp_in);
    fflush(fp_in);

    struct Body *b = mutt_body_new();
    b->type = TYPE_APPLICATION;
    b->subtype = mutt_str_dup("pkcs7-mime");
    b->encoding = ENC_7BIT;
    mutt_param_set(&b->parameter, "smime-type", "signed-data");
    b->offset = 0;
    b->length = ftello(fp_in);

    FILE *fp_out = NULL;
    struct Body *b_dec = NULL;
    TEST_CHECK(smime_class_decrypt_mime(fp_in, &fp_out, b, &b_dec) == -1);
    TEST_CHECK(fp_out == NULL);
    TEST_CHECK(b_dec == NULL);
    TEST_CHECK(b->offset == 0);

    mutt_body_free(&b);
    mutt_file_fclose(&fp_in);
    cs_str_reset(NeoMutt->sub->cs, "smime_verify_opaque_command", NULL);
  }
}
