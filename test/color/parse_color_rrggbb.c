/**
 * @file
 * Test code for Colour Parsing
 *
 * @authors
 * Copyright (C) 2023 Richard Russon <rich@flatcap.org>
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
#include <stdbool.h>
#include "mutt/lib.h"
#include "core/lib.h"
#include "color/lib.h"

enum CommandResult parse_color_rrggbb(const char *s, struct ColorElement *elem,
                                      struct Buffer *err);

struct RgbTest
{
  const char *str;
  long color;
};

void test_parse_color_rrggbb(void)
{
  // enum CommandResult parse_color_rrggbb(const char *s, struct ColorElement *elem, struct Buffer *err);

  {
    struct Buffer *err = buf_pool_get();
    enum CommandResult rc;
    const char *str = "#12AB89";
    struct ColorElement elem = { 0 };

    rc = parse_color_rrggbb(NULL, &elem, err);
    TEST_CHECK(rc == MUTT_CMD_ERROR);

    rc = parse_color_rrggbb(str, NULL, err);
    TEST_CHECK(rc == MUTT_CMD_ERROR);

    buf_pool_release(&err);
  }

  {
    struct RgbTest tests[] = {
      // clang-format off
      // Start
      { "#000000", 0x000000 }, { "#000001", 0x000001 }, { "#000002", 0x000002 }, { "#000003", 0x000003 }, { "#000004", 0x000004 }, { "#000005", 0x000005 }, { "#000006", 0x000006 }, { "#000007", 0x000007 }, { "#000008", 0x000008 }, { "#000009", 0x000009 }, { "#00000a", 0x00000a }, { "#00000b", 0x00000b }, { "#00000c", 0x00000c }, { "#00000d", 0x00000d }, { "#00000e", 0x00000e }, { "#00000f", 0x00000f },

      // Random
      { "#CF4144", 0xcf4144 }, { "#19F5EA", 0x19f5ea }, { "#A01642", 0xa01642 }, { "#A8FF5E", 0xa8ff5e }, { "#11A708", 0x11a708 }, { "#87F1DA", 0x87f1da }, { "#19D732", 0x19d732 }, { "#284FC1", 0x284fc1 }, { "#9C6277", 0x9c6277 }, { "#CFC961", 0xcfc961 }, { "#F437D8", 0xf437d8 }, { "#90CE8E", 0x90ce8e }, { "#D98D81", 0xd98d81 }, { "#6EB3DA", 0x6eb3da }, { "#B09F00", 0xb09f00 }, { "#DA9F7A", 0xda9f7a },
      { "#0D267A", 0x0d267a }, { "#F0F240", 0xf0f240 }, { "#0385CC", 0x0385cc }, { "#93200F", 0x93200f }, { "#F26296", 0xf26296 }, { "#8C5896", 0x8c5896 }, { "#323BE6", 0x323be6 }, { "#C4F73A", 0xc4f73a }, { "#42E907", 0x42e907 }, { "#ED732A", 0xed732a }, { "#A88888", 0xa88888 }, { "#1FEA37", 0x1fea37 }, { "#62341D", 0x62341d }, { "#939E63", 0x939e63 }, { "#7914E4", 0x7914e4 }, { "#CDF98E", 0xcdf98e },
      { "#2C314C", 0x2c314c }, { "#59BE9E", 0x59be9e }, { "#925F21", 0x925f21 }, { "#33F620", 0x33f620 }, { "#B8D8BC", 0xb8d8bc }, { "#706513", 0x706513 }, { "#F32758", 0xf32758 }, { "#AFF6A6", 0xaff6a6 }, { "#C4A243", 0xc4a243 }, { "#669B53", 0x669b53 }, { "#C516CA", 0xc516ca }, { "#CB0014", 0xcb0014 }, { "#25DD14", 0x25dd14 }, { "#A64784", 0xa64784 }, { "#34925D", 0x34925d }, { "#735267", 0x735267 },
      { "#25203B", 0x25203b }, { "#09D7CD", 0x09d7cd }, { "#79F705", 0x79f705 }, { "#6C9E18", 0x6c9e18 }, { "#9FAD47", 0x9fad47 }, { "#915A05", 0x915a05 }, { "#C138AD", 0xc138ad }, { "#A699EF", 0xa699ef }, { "#934667", 0x934667 }, { "#8686ED", 0x8686ed }, { "#C9F464", 0xc9f464 }, { "#879860", 0x879860 }, { "#72369A", 0x72369a }, { "#9B20ED", 0x9b20ed }, { "#7767B1", 0x7767b1 }, { "#9B8942", 0x9b8942 },

      // End
      { "#fffff0", 0xfffff0 }, { "#fffff1", 0xfffff1 }, { "#fffff2", 0xfffff2 }, { "#fffff3", 0xfffff3 }, { "#fffff4", 0xfffff4 }, { "#fffff5", 0xfffff5 }, { "#fffff6", 0xfffff6 }, { "#fffff7", 0xfffff7 }, { "#fffff8", 0xfffff8 }, { "#fffff9", 0xfffff9 }, { "#fffffa", 0xfffffa }, { "#fffffb", 0xfffffb }, { "#fffffc", 0xfffffc }, { "#fffffd", 0xfffffd }, { "#fffffe", 0xfffffe }, { "#ffffff", 0xffffff },
      { NULL, 0 },
      // clang-format on
    };

    struct Buffer *err = buf_pool_get();
    enum CommandResult rc;

    for (int i = 0; tests[i].str; i++)
    {
      struct ColorElement elem = { 0 };

      rc = parse_color_rrggbb(tests[i].str, &elem, err);
      TEST_CHECK(rc == MUTT_CMD_SUCCESS);
      TEST_MSG("rc: Expected %d, Got %d", MUTT_CMD_SUCCESS, rc);

      TEST_CHECK(elem.color == tests[i].color);
      TEST_MSG("color: Expected %d, Got %d", tests[i].color, elem.color);

      TEST_CHECK(elem.type == CT_RGB);
      TEST_CHECK(elem.prefix == COLOR_PREFIX_NONE);
    }

    buf_pool_release(&err);
  }

  {
    struct Buffer *err = buf_pool_get();
    enum CommandResult rc;
    const char *tests[] = { "1234",      "#",          "#1",      "#12",
                            "#123",      "#1234",      "#12345",  "#1234567",
                            "#12345678", "#123456789", "#abcdeg", "#red",
                            NULL };

    for (int i = 0; tests[i]; i++)
    {
      struct ColorElement elem = { 0 };

      rc = parse_color_rrggbb(tests[i], &elem, err);
      TEST_CHECK(rc < 0);
      TEST_MSG("Case: %s", tests[i]);
    }

    buf_pool_release(&err);
  }
}
