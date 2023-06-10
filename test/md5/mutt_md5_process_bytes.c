/**
 * @file
 * Test code for mutt_md5_process_bytes()
 *
 * @authors
 * Copyright (C) 2019 Richard Russon <rich@flatcap.org>
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
#include <stddef.h>
#include <string.h>
#include "mutt/lib.h"
#include "test_common.h"

static const char *raven1 = "Once upon a midnight dreary, while I pondered, weak and weary,\n"
                            "Over many a quaint and curious volume of forgotten lore\n"
                            "While I nodded, nearly napping, suddenly there came a tapping,\n"
                            "As of some one gently rapping, rapping at my chamber door.\n"
                            "''Tis some visitor,' I muttered, 'tapping at my chamber door\n"
                            "Only this and nothing more.'\n";

static const char *raven2 = "Ah, distinctly I remember it was in the bleak December;\n"
                            "And each separate dying ember wrought its ghost upon the floor.\n"
                            "Eagerly I wished the morrow; vainly I had sought to borrow\n"
                            "From my books surcease of sorrow sorrow for the lost Lenore\n"
                            "For the rare and radiant maiden whom the angels name Lenore\n"
                            "Nameless here for evermore.\n";

void test_mutt_md5_process_bytes(void)
{
  // void mutt_md5_process_bytes(const void *buffer, size_t len, struct Md5Ctx *md5ctx);

  // Degenerate tests
  {
    struct Md5Ctx md5ctx = { 0 };
    mutt_md5_process_bytes(NULL, 10, &md5ctx);
    TEST_CHECK_(1, "mutt_md5_process_bytes(NULL, 10, &md5ctx)");

    char buf[32] = { 0 };
    mutt_md5_process_bytes(&buf, sizeof(buf), NULL);
    TEST_CHECK_(1, "mutt_md5_process_bytes(&buf, sizeof(buf), NULL)");
  }

  {
    struct Md5Ctx md5ctx = { 0 };
    mutt_md5_init_ctx(&md5ctx);

    mutt_md5_process_bytes(raven1, strlen(raven1), &md5ctx);
    mutt_md5_process_bytes(raven2, strlen(raven2), &md5ctx);

    unsigned char digest[16];
    char hash[33] = { 0 };
    mutt_md5_finish_ctx(&md5ctx, digest);
    mutt_md5_toascii(digest, hash);
    TEST_CHECK_STR_EQ(hash, "f49f6134963b4c16320099342a4b91ad");
  }
}
