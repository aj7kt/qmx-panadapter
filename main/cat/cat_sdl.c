// SDL-port sibling of cat.c — same cat.h contract, desktop transport.
// Compiled ONLY by the SDL port (sim/); the ESP-IDF build's explicit
// SRCS list in main/CMakeLists.txt never includes *_sdl.c. See docs/porting.md.
//
// Every CAT exchange goes through ONE transact() choke point, backed by
// either a real serial port (a QMX's CDC-ACM interface is an ordinary
// serial port to any desktop OS) or the mock rig (sim/desktop/mock_rig.c,
// an in-process QMX emulation speaking the same command strings). Both
// directions of every exchange are mirrored to the CAT Log
// (sim/desktop/cat_log.c), so the File > CAT Log window shows identical
// traffic against mock and metal. The FA/MD poll loop runs against
// whichever backend is selected — same shape as the firmware's poll task.
#include "cat.h"
#include "sim_devices.h"
#include "mock_rig.h"
#include "cat_log.h"
#include <SDL2/SDL.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <time.h>

static SDL_Thread *s_poll_thread = NULL;
static volatile bool s_stop = false;
static int s_fd = -1;
static SDL_mutex *s_mtx = NULL;
static bool s_ready = false;
static uint32_t s_freq_hz = 14074000;
static char s_mode[8] = "USB";
static char s_port_name[256] = "";
static bool s_user_paused = false;
static bool s_poll_paused = false;
static int s_af_gain = -1, s_rf_gain = -1;
static int s_rit_hz = 0;

static bool open_serial(const char *path)
{
    int fd = open(path, O_RDWR | O_NOCTTY);
    if (fd < 0) { fprintf(stderr, "cat: open(%s) failed: %s\n", path, strerror(errno)); return false; }
    struct termios tty;
    if (tcgetattr(fd, &tty) != 0) { close(fd); return false; }
    cfsetispeed(&tty, B38400);
    cfsetospeed(&tty, B38400);
    tty.c_cflag &= ~PARENB; tty.c_cflag &= ~CSTOPB; tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8; tty.c_cflag &= ~CRTSCTS; tty.c_cflag |= CREAD | CLOCAL;
    tty.c_lflag &= ~ICANON; tty.c_lflag &= ~(ECHO | ECHOE | ISIG);
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);
    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL);
    tty.c_oflag &= ~OPOST;
    tty.c_cc[VMIN] = 0; tty.c_cc[VTIME] = 5;
    if (tcsetattr(fd, TCSANOW, &tty) != 0) { close(fd); return false; }
    tcflush(fd, TCIOFLUSH);
    s_fd = fd;
    return true;
}

static void serial_transact(const char *cmd, char *buf, size_t buf_len)
{
    buf[0] = '\0';
    if (s_fd < 0) return;
    char out[32];
    int n = snprintf(out, sizeof(out), "%s;", cmd);
    if (write(s_fd, out, (size_t)n) != n) return;
    size_t got = 0;
    for (int a = 0; a < 4 && got < buf_len - 1; a++) {
        char tmp[64];
        ssize_t r = read(s_fd, tmp, sizeof(tmp));
        if (r > 0) {
            for (ssize_t i = 0; i < r && got < buf_len - 1; i++) {
                if (tmp[i] == ';') { buf[got] = '\0'; return; }
                buf[got++] = tmp[i];
            }
        }
    }
    buf[0] = '\0';
}

// The choke point. cmd WITHOUT the trailing ';'; resp receives the reply
// without ';' ("" if none — normal for set-commands). Logs both directions.
static void transact(const char *cmd, char *resp, size_t resp_sz)
{
    char line[64];
    snprintf(line, sizeof(line), "%s;", cmd);
    cat_log_append("->", line);

    if (s_fd >= 0) serial_transact(cmd, resp, resp_sz);
    else mock_rig_transact(cmd, resp, resp_sz);

    if (resp[0]) {
        snprintf(line, sizeof(line), "%s;", resp);
        cat_log_append("<-", line);
    }
}

static const char *mode_digit_to_name(char d)
{
    switch (d) {
        case '1': return "LSB"; case '2': return "USB"; case '3': return "CW";
        case '4': return "FM";  case '5': return "AM";  case '6': return "DiGi";
        case '7': return "CW-R"; case '8': return "TUNE"; case '9': return "DATA";
        default: return "?";
    }
}

static char mode_name_to_digit(const char *m)
{
    if (!m) return '2';
    if (strcasecmp(m, "LSB") == 0) return '1';
    if (strcasecmp(m, "USB") == 0) return '2';
    if (strcasecmp(m, "CW") == 0) return '3';
    if (strcasecmp(m, "FM") == 0) return '4';
    if (strcasecmp(m, "AM") == 0) return '5';
    if (strcasecmp(m, "DiGi") == 0 || strcasecmp(m, "DIGI") == 0) return '6';
    if (strcasecmp(m, "CW-R") == 0) return '7';
    if (strcasecmp(m, "TUNE") == 0) return '8';
    if (strcasecmp(m, "DATA") == 0) return '9';
    return '2';
}

static int poll_thread(void *arg)
{
    (void)arg;
    int cycle = 0;
    while (!s_stop) {
        if (s_user_paused || s_poll_paused) { SDL_Delay(200); continue; }
        char resp[32];
        if ((cycle % 3) != 2) {
            transact("FA", resp, sizeof(resp));
            if (strncmp(resp, "FA", 2) == 0 && isdigit((unsigned char)resp[2])) {
                SDL_LockMutex(s_mtx);
                s_freq_hz = (uint32_t)strtoul(resp + 2, NULL, 10);
                s_ready = true;
                SDL_UnlockMutex(s_mtx);
            }
        } else {
            transact("MD", resp, sizeof(resp));
            if (strncmp(resp, "MD", 2) == 0 && resp[2] != '\0') {
                SDL_LockMutex(s_mtx);
                snprintf(s_mode, sizeof(s_mode), "%s", mode_digit_to_name(resp[2]));
                SDL_UnlockMutex(s_mtx);
            }
        }
        cycle++;
        SDL_Delay(200);
    }
    return 0;
}

static void stop_poll(void)
{
    if (s_poll_thread) { s_stop = true; SDL_WaitThread(s_poll_thread, NULL); s_poll_thread = NULL; }
}

static void start_poll(void)
{
    s_stop = false;
    s_poll_thread = SDL_CreateThread(poll_thread, "cat_poll", NULL);
}

esp_err_t cat_init(void)
{
    s_mtx = SDL_CreateMutex();
    if (!s_mtx) return ESP_FAIL;
    cat_log_init();
    start_poll(); // polls the mock rig from boot; sim_cat_select_port() reroutes it
    return ESP_OK;
}

bool sim_cat_select_port(const char *device_path)
{
    stop_poll();
    if (s_fd >= 0) { close(s_fd); s_fd = -1; }
    s_port_name[0] = '\0';
    bool ok = open_serial(device_path);
    if (ok) {
        snprintf(s_port_name, sizeof(s_port_name), "%s", device_path);
        s_ready = false; // until the first real FA reply lands
        fprintf(stderr, "cat: polling %s\n", device_path);
    }
    start_poll(); // either way - fall back to the mock on failure
    return ok;
}

void sim_cat_select_none(void)
{
    stop_poll();
    if (s_fd >= 0) { close(s_fd); s_fd = -1; }
    s_port_name[0] = '\0';
    s_ready = false;
    start_poll();
}

const char *sim_cat_get_port(void) { return s_port_name; }

esp_err_t cat_set_frequency(uint32_t freq_hz)
{
    char cmd[24], resp[8];
    snprintf(cmd, sizeof(cmd), "FA%011u", freq_hz);
    transact(cmd, resp, sizeof(resp));
    SDL_LockMutex(s_mtx); s_freq_hz = freq_hz; SDL_UnlockMutex(s_mtx);
    return ESP_OK;
}
esp_err_t cat_set_frequency_forced(uint32_t freq_hz) { return cat_set_frequency(freq_hz); }
uint32_t cat_get_frequency(void) { SDL_LockMutex(s_mtx); uint32_t f = s_freq_hz; SDL_UnlockMutex(s_mtx); return f; }
const char *cat_get_mode_str(void)
{
    static char out[8];
    SDL_LockMutex(s_mtx); snprintf(out, sizeof(out), "%s", s_mode); SDL_UnlockMutex(s_mtx);
    return out;
}
esp_err_t cat_set_mode(const char *mode)
{
    char cmd[8], resp[8];
    snprintf(cmd, sizeof(cmd), "MD%c", mode_name_to_digit(mode));
    transact(cmd, resp, sizeof(resp));
    SDL_LockMutex(s_mtx); snprintf(s_mode, sizeof(s_mode), "%s", mode ? mode : "USB"); SDL_UnlockMutex(s_mtx);
    return ESP_OK;
}
esp_err_t cat_set_passband_hz(uint32_t hz) { (void)hz; return ESP_OK; }
bool cat_is_ready(void) { SDL_LockMutex(s_mtx); bool r = s_ready; SDL_UnlockMutex(s_mtx); return r; }
int cat_get_cw_offset_hz(void) { return 700; }
bool cat_cw_tx_offset_engaged(void) { return false; }
int cat_probe_extra_cdc_ports(void) { return -1; }
int cat_probe_terminal(void) { return -1; }
const char *cat_get_qmx_fw(void) { return s_fd >= 0 ? "" : mock_rig_fw(); }
bool cat_qmx_fw_at_least(int major, int minor, int patch)
{ (void)major; (void)minor; (void)patch; return s_fd < 0; } // the mock is 1_04; a real radio's fw is unparsed here
bool cat_get_iq_mode_confirmed(void) { return true; }
bool cat_get_vox_disabled(void) { return true; }
esp_err_t cat_send_raw_cmd(const char *fmt, ...) { (void)fmt; return ESP_OK; }
esp_err_t cat_query_power_swr(float *power_w, float *swr)
{
    if (s_fd < 0) { mock_rig_power_swr(power_w, swr); return ESP_OK; }
    *power_w = -1.0f; *swr = -1.0f; return ESP_ERR_TIMEOUT;
}
esp_err_t cat_pwr_swr_async_send(void) { return ESP_OK; }
esp_err_t cat_pwr_swr_async_read(float *power_w, float *swr) { return cat_query_power_swr(power_w, swr); }
void cat_tune_poll_set_active(bool active) { (void)active; }
void cat_request_mode(const char *mode) { cat_set_mode(mode); }
void cat_request_ssb_bandwidth(uint32_t hz) { (void)hz; }

void cat_request_af_gain(uint16_t ag)
{
    char cmd[12], resp[12];
    snprintf(cmd, sizeof(cmd), "AG0%03u", ag);
    transact(cmd, resp, sizeof(resp));
    s_af_gain = ag;
}
void cat_query_af_gain(void)
{
    char resp[12];
    transact("AG0", resp, sizeof(resp));
    if (strncmp(resp, "AG0", 3) == 0) s_af_gain = atoi(resp + 3);
}
int cat_get_af_gain(void) { return s_af_gain; }

void cat_request_rf_gain(uint8_t db)
{
    char cmd[12], resp[12];
    snprintf(cmd, sizeof(cmd), "RG%03u", db);
    transact(cmd, resp, sizeof(resp));
    s_rf_gain = db;
}
void cat_query_rf_gain(void)
{
    char resp[12];
    transact("RG", resp, sizeof(resp));
    if (strncmp(resp, "RG", 2) == 0 && isdigit((unsigned char)resp[2])) s_rf_gain = atoi(resp + 2);
}
int cat_get_rf_gain(void) { return s_rf_gain; }

void cat_request_rit_hz(int hz)
{
    // Same recipe as the firmware: RC; first (unambiguous under either "CAT
    // RU and RD" menu setting), then a single RU/RD to the requested value.
    char cmd[12], resp[8];
    transact("RC", resp, sizeof(resp));
    if (hz > 0) { snprintf(cmd, sizeof(cmd), "RU%03d", hz); transact(cmd, resp, sizeof(resp)); }
    else if (hz < 0) { snprintf(cmd, sizeof(cmd), "RD%03d", -hz); transact(cmd, resp, sizeof(resp)); }
    s_rit_hz = hz;
}
int cat_get_rit_hz(void) { return s_rit_hz; }

void cat_user_pause_set(bool paused) { s_user_paused = paused; }
bool cat_user_pause_active(void) { return s_user_paused; }
void cat_request_iq_reassert(void) { char r[8]; transact("Q9 1", r, sizeof(r)); }
void cat_request_cw_passband(uint32_t hz) { (void)hz; }
void cat_poll_set_paused(bool paused) { s_poll_paused = paused; }

void cat_usb_shutdown(void)
{
    stop_poll();
    if (s_fd >= 0) { close(s_fd); s_fd = -1; }
}

const cat_band_entry_t *cat_get_band_list(int *out_count)
{
    if (s_fd < 0) return mock_rig_band_list(out_count);
    *out_count = 0;
    static cat_band_entry_t empty[1];
    return empty;
}
esp_err_t cat_set_qmx_time(int hour, int min, int sec) { (void)hour; (void)min; (void)sec; return ESP_OK; }
esp_err_t cat_query_qmx_time(int *out_hour, int *out_min, int *out_sec)
{
    char resp[16];
    transact("TM", resp, sizeof(resp));
    if (strncmp(resp, "TM", 2) == 0 && strlen(resp) >= 8) {
        *out_hour = (resp[2] - '0') * 10 + (resp[3] - '0');
        *out_min  = (resp[4] - '0') * 10 + (resp[5] - '0');
        *out_sec  = (resp[6] - '0') * 10 + (resp[7] - '0');
        return ESP_OK;
    }
    return ESP_ERR_TIMEOUT;
}
esp_err_t cat_gps_tick_sync(int *out_hour, int *out_min, int *out_sec, int64_t *out_flip_us)
{
    (void)out_hour; (void)out_min; (void)out_sec; (void)out_flip_us;
    return ESP_ERR_TIMEOUT;
}
