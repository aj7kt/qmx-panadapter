// Stub replacing main/ui/reader_view.c for this pass - the real one needs
// cJSON + the embedded manual.bin blob + a FreeRTOS queue, none of which
// this pass wires up. See sim/README.md.
#include "reader_view.h"

void reader_view_init(lv_obj_t *parent) { (void)parent; }
void reader_view_open_help(const char *page_rel, const char *anchor) { (void)page_rel; (void)anchor; }
void reader_view_open_index(void) { }
void reader_view_show(void) { }
void reader_view_hide(void) { }
bool reader_view_is_active(void) { return false; }
lv_obj_t *reader_view_get_container(void) { return NULL; }
void reader_view_notify_loaded(bool from_cache) { (void)from_cache; }
void reader_view_notify_status(const char *status) { (void)status; }
void reader_view_notify_toc_loaded(void) { }
void reader_view_set_update_available(const char *latest_version) { (void)latest_version; }
