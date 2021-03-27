#include <stddef.h>
#include <stdbool.h>

#include "config/lib.h"
#include "config/types.h"

#include "private.h"

struct ConfigDef PreviewVars[] = {
  // clang-format off
  { PREVIEW_CONFIG_PREFIX "enabled", DT_BOOL|R_PREVIEW, false, 0, NULL,
    "(preview) Enable the preview window"
  },
  { PREVIEW_CONFIG_PREFIX "height", DT_NUMBER|DT_NOT_NEGATIVE|R_PREVIEW, 15, 0, NULL,
    "(preview) Height of the preview window"
  },
  { PREVIEW_CONFIG_PREFIX "lines", DT_NUMBER|DT_NOT_NEGATIVE|R_PREVIEW, 3, 0, NULL,
    "(preview) Number of lines to preview"
  },
  { PREVIEW_CONFIG_PREFIX "divider_horizontal", DT_STRING|R_PREVIEW, 0, 0, NULL,
    "(preview) Character to draw between the preview, the index and the sidebar (horizontal)"
  },
  { NULL },
  // clang-format on
};

/**
 * config_init_preview - Register preview config variables
 */
bool config_init_preview(struct ConfigSet *cs)
{
  if (!cs_register_variables(cs, PreviewVars, 0))
  {
    abort();
  }

  return true;
}
