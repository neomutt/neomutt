#pragma once

struct NotifyCallback;
struct MuttWindow;

/* observers.c */

int preview_insertion_observer  (struct NotifyCallback *nc);

/* preview.c */

void preview_win_init       (struct MuttWindow *dlg);
void preview_win_shutdown   (struct MuttWindow *dlg);
