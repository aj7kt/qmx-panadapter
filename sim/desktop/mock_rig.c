// Command formats verified against the official QMX CAT manual for firmware
// 1_04_004 (docs/qmx-reference/cat_1_04_004.pdf - gitignored vendor cache,
// see SOURCES.md there): FA 11-digit get, MD digit list (8 = SWR Tune, and
// MD; reports the pre-Tune mode), FW 3200 Digi / 0300 CW, AG0nnn in 0.25 dB
// steps, RG plain dB 3-digit, RC/RU/RD, Q9 session-only, TMhhmmss, VN.
#include "mock_rig.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// All bands any QMX variant can have, centres on the common digi/FT8 spots.
static const cat_band_entry_t k_bands[] = {
    { "160", 1840000 }, { "80", 3573000 },  { "60", 5357000 },
    { "40", 7074000 },  { "30", 10136000 }, { "20", 14074000 },
    { "17", 18100000 }, { "15", 21074000 }, { "12", 24915000 },
    { "11", 27245000 }, { "10", 28074000 }, { "6", 50313000 },
};

typedef struct {
    const char *name;
    int first, count; // index range into k_bands
} variant_t;

static const variant_t k_variants[] = {
    { "Mock QMX 80-20 m", 1, 5 },  // 80 60 40 30 20
    { "Mock QMX 20-10 m", 5, 6 },  // 20 17 15 12 11 10
    { "Mock QMX+ 160-6 m", 0, 12 },
};

static int s_sel = 2; // QMX+ by default - the widest, least surprising mock

// Session state, mirroring what the real radio remembers.
static uint32_t s_freq_hz = 14074000;
static char s_mode_digit = '2';    // USB
static char s_pretune_digit = '2'; // what MD; reports while in Tune (see below)
static int s_af_gain = 120;        // AG units, 0.25 dB steps (= 30 dB)
static int s_rf_gain = 54;         // plain dB, the radio's default
static int s_iq_mode = 1;
static int s_rit_hz = 0;

int mock_rig_count(void) { return (int)(sizeof(k_variants) / sizeof(k_variants[0])); }
const char *mock_rig_name(int idx) { return (idx >= 0 && idx < mock_rig_count()) ? k_variants[idx].name : ""; }
void mock_rig_select(int idx) { if (idx >= 0 && idx < mock_rig_count()) s_sel = idx; }
int mock_rig_selected(void) { return s_sel; }
const char *mock_rig_fw(void) { return "1_04_004QMX"; } // the manual version this mock implements

const cat_band_entry_t *mock_rig_band_list(int *out_count)
{
    *out_count = k_variants[s_sel].count;
    return &k_bands[k_variants[s_sel].first];
}

void mock_rig_transact(const char *cmd, char *resp, size_t resp_sz)
{
    resp[0] = '\0';
    size_t len = strlen(cmd);

    if (strncmp(cmd, "FA", 2) == 0) {
        if (len > 2) s_freq_hz = (uint32_t)strtoul(cmd + 2, NULL, 10);
        else snprintf(resp, resp_sz, "FA%011u", s_freq_hz);
    } else if (strncmp(cmd, "MD", 2) == 0) {
        if (len > 2) {
            // Entering Tune remembers where to return to; MD; then reports the
            // PRE-tune mode, exactly like the real radio (CLAUDE.md, "MD;
            // reports the PRE-Tune mode while the radio is tuning").
            if (cmd[2] == '8') { s_pretune_digit = s_mode_digit; s_mode_digit = '8'; }
            else s_mode_digit = cmd[2];
        } else {
            snprintf(resp, resp_sz, "MD%c", s_mode_digit == '8' ? s_pretune_digit : s_mode_digit);
        }
    } else if (strncmp(cmd, "VN", 2) == 0) {
        snprintf(resp, resp_sz, "VN%s", mock_rig_fw());
    } else if (strncmp(cmd, "FW", 2) == 0) {
        // Get-only. Manual: "Returns 3200 in Digi mode, and 0300 for CW mode"
        // (in DiGi the number is the filter's TOP EDGE, not a width).
        char m = (s_mode_digit == '8') ? s_pretune_digit : s_mode_digit;
        snprintf(resp, resp_sz, (m == '3' || m == '7') ? "FW0300" : "FW3200");
    } else if (strncmp(cmd, "Q9", 2) == 0) {
        if (len > 2) s_iq_mode = atoi(cmd + 2) != 0;
        else snprintf(resp, resp_sz, "Q9%d", s_iq_mode);
    } else if (strncmp(cmd, "AG0", 3) == 0) {
        if (len > 3) s_af_gain = atoi(cmd + 3);
        else snprintf(resp, resp_sz, "AG0%03d", s_af_gain);
    } else if (strncmp(cmd, "RG", 2) == 0) {
        if (len > 2) s_rf_gain = atoi(cmd + 2);
        else snprintf(resp, resp_sz, "RG%03d", s_rf_gain);
    } else if (strncmp(cmd, "RC", 2) == 0) {
        s_rit_hz = 0;
    } else if (strncmp(cmd, "RU", 2) == 0) {
        s_rit_hz = (len > 2) ? atoi(cmd + 2) : 0; // post-RC, absolute == relative
    } else if (strncmp(cmd, "RD", 2) == 0) {
        s_rit_hz = (len > 2) ? -atoi(cmd + 2) : 0;
    } else if (strncmp(cmd, "TM", 2) == 0) {
        if (len > 2) {
            // Set accepted silently; the mock's clock is the host's clock.
        } else {
            time_t t = time(NULL);
            struct tm tmv; gmtime_r(&t, &tmv);
            snprintf(resp, resp_sz, "TM%02d%02d%02d", tmv.tm_hour, tmv.tm_min, tmv.tm_sec);
        }
    } else {
        snprintf(resp, resp_sz, "?"); // what the real radio says to anything it doesn't speak
    }
}

void mock_rig_power_swr(float *power_w, float *swr)
{
    *power_w = 4.6f + (float)(rand() % 40) / 100.0f; // 4.6-5.0 W
    *swr = 1.10f + (float)(rand() % 15) / 100.0f;    // 1.10-1.25
}
