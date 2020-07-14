/**
 * @file
 * Test code for Window visibility notification)
 *
 * @authors
 * Copyright (C) 2020 Richard Russon <rich@flatcap.org>
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
#include <stdint.h>
#include "mutt/lib.h"
#include "gui/mutt_window.h"
#include "gui/reflow.h"

enum TestEvent
{
  TE_NONE,
  TE_VISIBLE,
  TE_HIDDEN,
};

struct TestVisible
{
  bool parent_before;
  bool parent_after;
  enum TestEvent parent_expected;

  bool child_before;
  bool child_after;
  enum TestEvent child_expected;
};

struct NotifyCatcher
{
  enum TestEvent parent_received;
  enum TestEvent child_received;
};

const char *event_name(enum TestEvent event)
{
  switch (event)
  {
    case TE_NONE:
      return "NONE";
    case TE_VISIBLE:
      return "VISIBLE";
    case TE_HIDDEN:
      return "HIDDEN";
  }
  return "UNKNOWN";
}

int visible_observer(struct NotifyCallback *nc)
{
  if (!nc->event_data || !nc->global_data)
    return -1;
  if (nc->event_type != NT_WINDOW)
    return 0;

  struct EventWindow *ew = nc->event_data;
  struct NotifyCatcher *results = nc->global_data;

  if (ew->win->type == WT_ROOT)
  {
    if (ew->flags & WN_VISIBLE)
      results->parent_received = TE_VISIBLE;
    else if (ew->flags & WN_HIDDEN)
      results->parent_received = TE_HIDDEN;
  }
  else if (ew->win->type == WT_DLG_INDEX)
  {
    if (ew->flags & WN_VISIBLE)
      results->child_received = TE_VISIBLE;
    else if (ew->flags & WN_HIDDEN)
      results->child_received = TE_HIDDEN;
  }

  return 0;
}

void test_window_visible(void)
{
  static const struct TestVisible tests[] = {
    // clang-format off
    { false, false, TE_NONE,    false, false, TE_NONE    },
    { false, false, TE_NONE,    false, true,  TE_NONE    },
    { false, false, TE_NONE,    true,  false, TE_NONE    },
    { false, false, TE_NONE,    true,  true,  TE_NONE    },

    { false, true,  TE_VISIBLE, false, false, TE_NONE    },
    { false, true,  TE_VISIBLE, false, true,  TE_VISIBLE },
    { false, true,  TE_VISIBLE, true,  false, TE_NONE    },
    { false, true,  TE_VISIBLE, true,  true,  TE_VISIBLE },

    { true,  false, TE_HIDDEN,  false, false, TE_NONE    },
    { true,  false, TE_HIDDEN,  false, true,  TE_NONE    },
    { true,  false, TE_HIDDEN,  true,  false, TE_HIDDEN  },
    { true,  false, TE_HIDDEN,  true,  true,  TE_HIDDEN  },

    { true,  true,  TE_NONE,    false, false, TE_NONE    },
    { true,  true,  TE_NONE,    false, true,  TE_VISIBLE },
    { true,  true,  TE_NONE,    true,  false, TE_HIDDEN  },
    { true,  true,  TE_NONE,    true,  true,  TE_NONE    },
    // clang-format on
  };

  struct MuttWindow *parent = mutt_window_new(WT_ROOT, MUTT_WIN_ORIENT_VERTICAL, MUTT_WIN_SIZE_FIXED, 80, 24);
  struct MuttWindow *child = mutt_window_new(WT_DLG_INDEX, MUTT_WIN_ORIENT_VERTICAL, MUTT_WIN_SIZE_FIXED, 60, 20);

  TEST_CHECK(parent != NULL);
  TEST_CHECK(child != NULL);

  mutt_window_add_child(parent, child);

  struct NotifyCatcher results;

  notify_observer_add(parent->notify, visible_observer, &results);

  for (size_t i = 0; i < mutt_array_size(tests); i++)
  {
    parent->old.visible   = tests[i].parent_before;
    parent->state.visible = tests[i].parent_after;
    child->old.visible    = tests[i].child_before;
    child->state.visible  = tests[i].child_after;

    results.parent_received = TE_NONE;
    results.child_received = TE_NONE;

    TEST_CASE_("%ld: P%d->%d, C%d->%d", i, tests[i].parent_before, tests[i].parent_after, tests[i].child_before, tests[i].child_after);
    mutt_window_reflow(parent);

    if (!TEST_CHECK(tests[i].parent_expected == results.parent_received))
    {
      TEST_MSG("Expected: %s", event_name(tests[i].parent_expected));
      TEST_MSG("Actual:   %s", event_name(results.parent_received));
    }

    if (!TEST_CHECK(tests[i].child_expected == results.child_received))
    {
      TEST_MSG("Expected: %s", event_name(tests[i].child_expected));
      TEST_MSG("Actual:   %s", event_name(results.child_received));
    }
  }

  notify_observer_remove(parent->notify, visible_observer, &results);

  mutt_window_free(&parent);
}
