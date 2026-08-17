// Mock transceiver for the SDL port - a fake PERIPHERAL, per docs/porting.md:
// the port supplies the transport, and here the "transport" terminates in an
// in-process QMX emulation instead of a serial port. It speaks the same
// Kenwood-CAT command strings the real radio does (FA/MD/VN/FW/AG/RG/RC/RU/
// RD/Q9/TM), so cat_sdl.c's single transact() choke point - and the CAT Log -
// work identically against mock and metal.
#pragma once
#include "cat.h" // cat_band_entry_t
#include <stddef.h>

// Selectable QMX variants (band coverage differs; CAT behaviour is shared).
int mock_rig_count(void);
const char *mock_rig_name(int idx);
void mock_rig_select(int idx);
int mock_rig_selected(void);

const char *mock_rig_fw(void); // e.g. "1_04_002QMX", as VN; reports it
const cat_band_entry_t *mock_rig_band_list(int *out_count);

// One CAT exchange. cmd is the command WITHOUT the trailing ';' (e.g. "FA"
// or "FA00014074000"); resp receives the reply without ';' ("" for
// set-commands, which the real radio also leaves unanswered).
void mock_rig_transact(const char *cmd, char *resp, size_t resp_sz);

// Plausible jittered TX readings for the tune modal (the mock never keys).
void mock_rig_power_swr(float *power_w, float *swr);
