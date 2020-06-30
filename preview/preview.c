#include "lib.h"

#include "core/neomutt.h"
#include "mutt_globals.h"

#include "private.h"

void preview_init(void)
{
  notify_observer_add(NeoMutt->notify, NT_WINDOW, preview_insertion_observer, NULL);
}

void preview_shutdown(void)
{
  if (NeoMutt)
    notify_observer_remove(NeoMutt->notify, preview_insertion_observer, NULL);
}
