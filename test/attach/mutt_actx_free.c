/**
 * @file
 * Test code for mutt_actx_free()
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
#include "mutt/lib.h"
#include "address/lib.h"
#include "email/lib.h"

void test_mutt_actx_free(void)
{
  // void mutt_actx_free(struct AttachCtx **pactx);

  {
    mutt_actx_free(NULL);
    TEST_CHECK_(1, "mutt_actx_free(NULL)");
  }

  {
    struct AttachCtx *attachctx = NULL;
    mutt_actx_free(&attachctx);
    TEST_CHECK_(1, "mutt_actx_free(&attachctx)");
  }
}
