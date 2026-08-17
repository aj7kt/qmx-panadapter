// Host harness for the SDL port's mock transceiver (sim/desktop/mock_rig.c),
// which emulates the QMX CAT protocol per the vendor manual for firmware
// 1_04_004 (docs/qmx-reference/cat_1_04_004.pdf). Links the REAL mock, same
// pattern as the other harnesses here. Covers: FA 11-digit get + set/echo,
// the MD8 pre-tune-mode readback quirk, FW per-mode (0300 CW / 3200 Digi),
// VN, AG0nnn, RG, Q9 (including the firmware's "Q9 1" spelling with the
// space), TM set-silent/get-format, unknown->"?", and per-variant band lists.
//
// Build & run:
//   cc -o mock_rig_harness test/mock_rig_harness.c sim/desktop/mock_rig.c \
//      -Isim/desktop -Isim/esp_shim -Imain/cat -Imain && ./mock_rig_harness
#include "mock_rig.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

static char r[32];
static const char *t(const char *cmd) { mock_rig_transact(cmd, r, sizeof r); return r; }

int main(void)
{
    assert(strcmp(t("FA"), "FA00014074000") == 0);            // 11-digit get
    t("FA00007030000");                                        // set, no reply
    assert(r[0] == 0 && strcmp(t("FA"), "FA00007030000") == 0);
    assert(strcmp(t("MD"), "MD2") == 0);
    t("MD8");                                                  // enter SWR Tune
    assert(strcmp(t("MD"), "MD2") == 0);                       // reports PRE-tune mode
    t("MD3");                                                  // CW
    assert(strcmp(t("FW"), "FW0300") == 0);                    // CW filter
    t("MD6");
    assert(strcmp(t("FW"), "FW3200") == 0);                    // Digi top edge
    assert(strcmp(t("VN"), "VN1_04_004QMX") == 0);
    assert(strcmp(t("AG0"), "AG0120") == 0);
    t("AG0091"); assert(strcmp(t("AG0"), "AG0091") == 0);      // manual's own example value
    assert(strcmp(t("RG"), "RG054") == 0);                     // radio default 54 dB
    t("RG063"); assert(strcmp(t("RG"), "RG063") == 0);         // manual's own example value
    assert(strcmp(t("Q9"), "Q91") == 0);
    t("Q9 0"); assert(strcmp(t("Q9"), "Q90") == 0);
    t("Q9 1"); assert(strcmp(t("Q9"), "Q91") == 0);
    t("TM135532"); assert(r[0] == 0);                          // set: silent
    assert(strncmp(t("TM"), "TM", 2) == 0 && strlen(r) == 8);  // get: TMhhmmss
    assert(strcmp(t("XX"), "?") == 0);                         // unknown command
    int n; const cat_band_entry_t *b = mock_rig_band_list(&n);
    assert(n == 12 && strcmp(b[0].name, "160") == 0);          // QMX+ default
    mock_rig_select(0); b = mock_rig_band_list(&n);
    assert(n == 5 && strcmp(b[0].name, "80") == 0);            // QMX 80-20 variant
    printf("mock_rig: all checks PASS\n");
    return 0;
}
