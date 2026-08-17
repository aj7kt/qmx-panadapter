// Non-macOS builds have no native menu bar (yet) - source selection is CLI
// args (see desktop/main.c). Keeps the Linux/CI build linking.
#include "sim_devices.h"

void native_menu_install(void) { }
