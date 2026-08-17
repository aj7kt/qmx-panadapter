// CAT traffic log for the SDL port: every command/reply through cat_sdl.c's
// transact() choke point lands here, whether the far end is a real serial
// port or the mock rig. A small ring keeps recent lines so the CAT Log
// window shows history from before it was opened.
#pragma once

void cat_log_init(void);

// dir is "->" (host to rig) or "<-" (rig to host); text is the full
// command/reply including the ';'. Thread-safe (poll thread + LVGL thread).
void cat_log_append(const char *dir, const char *text);

// One listener (the CAT Log window). Registering replays the buffered ring
// first, then delivers live lines - possibly from the CAT poll thread, so
// the listener must marshal to its own UI thread itself.
typedef void (*cat_log_listener_t)(const char *line);
void cat_log_set_listener(cat_log_listener_t fn);
