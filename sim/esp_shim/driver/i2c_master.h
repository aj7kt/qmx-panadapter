#pragma once
// Tier-1 shim: battery.h/time_sync.h take an i2c_master_bus_handle_t but
// nothing in this build's compiled set (battery.c, time_sync.c are stubbed)
// actually dereferences it - it only needs to be SOME pointer type.
typedef void *i2c_master_bus_handle_t;
