// Stub replacing main/ui/qmx_term_view.c for this pass - the real one opens
// a second real serial port to the QMX's menu system, a hardware-only
// feature. See sim/README.md.
#include "qmx_term_view.h"

void qmx_term_view_open(void) { }
void qmx_term_view_close(void) { }
bool qmx_term_view_is_open(void) { return false; }
