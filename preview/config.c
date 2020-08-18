#include <stddef.h>
#include <stdbool.h>

#include "config/lib.h"
#include "config/types.h"

#include "private.h"

#define PREVIEW_CONFIG_PREFIX "devel_preview_"

// clang-format off
bool *C_PreviewEnabled;   ///< Config: (preview) Enable the preview window
short C_PreviewHeight; ///< Config: (preview) Height of the preview window
short C_PreviewLines;  ///< Config: (preview) Number of extracted lines to display in the preview window
// clang-format on

struct ConfigDef PreviewVars[] = {
  // clang-format off
  { PREVIEW_CONFIG_PREFIX "enabled", DT_BOOL|R_PREVIEW, &C_PreviewEnabled, false, 0, NULL,
    "(preview) Enable the preview window"
  },
  { PREVIEW_CONFIG_PREFIX "height", DT_NUMBER|DT_NOT_NEGATIVE|R_PREVIEW, &C_PreviewHeight, 25, 0, NULL,
    "(preview) Height of the preview window"
  },
  { PREVIEW_CONFIG_PREFIX "lines", DT_NUMBER|DT_NOT_NEGATIVE|R_PREVIEW, &C_PreviewLines, 3, 0, NULL,
    "(preview) Number of lines to preview"
  },
  { NULL, 0, NULL, 0, 0, NULL, NULL },
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
