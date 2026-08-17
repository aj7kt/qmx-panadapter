#pragma once
#include "esp_err.h"
// Tier-1 shim: webserver_ws.h names httpd_handle_t in a signature that's
// never called in this build (net/webserver_ws.c is stubbed) - only needs
// to exist as a type.
typedef void *httpd_handle_t;
