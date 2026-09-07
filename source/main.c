#include <3ds.h>
#include <arpa/inet.h>
#include <malloc.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "scanner.h"
#include "ui.h"

static const uint16_t common[] = {21,22,23,25,53,80,110,139,143,443,445,
                                 554,631,993,995,1883,3389,5900,8000,8080,8443};


static bool input(char *text, size_t size, const char *hint) {
    SwkbdState keyboard;
    swkbdInit(&keyboard, SWKBD_TYPE_NORMAL, 2, (int)size - 1);
    swkbdSetHintText(&keyboard, hint);
    swkbdSetInitialText(&keyboard, text);
    swkbdSetValidation(&keyboard, SWKBD_NOTEMPTY_NOTBLANK, 0, 0);
    char temporary[64] = {0};
    if (swkbdInputText(&keyboard, temporary, size) != SWKBD_BUTTON_CONFIRM)
        return false;
    snprintf(text, size, "%s", temporary);
    return true;
}

static bool port_input(unsigned *value, const char *hint) {
    char text[6];
    snprintf(text, sizeof(text), "%u", *value);
    if (!input(text, sizeof(text), hint)) return false;
    char *end;
    unsigned long port = strtoul(text, &end, 10);
    if (!*text || *end || port < 1 || port > 65535) return false;
    *value = (unsigned)port;
    return true;
}

int main(void) {
    gfxInitDefault();
    if (!ui_init()) { gfxExit(); return 1; }
    Result ac_result = acInit();
    void *buffer = memalign(0x1000, 0x100000);
    Result soc_result = buffer ? socInit(buffer, 0x100000) : (Result)-1;
    Scanner scanner;
    scanner_init(&scanner);
    UiState view = { .first = 1, .last = 1024 };
    view.rows = calloc(65535, sizeof(*view.rows));
    view.visible = calloc(65535, sizeof(*view.visible));
    view.network_ready = R_SUCCEEDED(ac_result) && R_SUCCEEDED(soc_result) && view.rows && view.visible;
    snprintf(view.message, sizeof(view.message), "Tap Target to begin");
    if (!view.rows || !view.visible) snprintf(view.message, sizeof(view.message), "Unable to allocate result storage");
    else if (R_FAILED(ac_result)) snprintf(view.message, sizeof(view.message), "AC error: %08lX", (unsigned long)(u32)ac_result);
    else if (R_FAILED(soc_result)) snprintf(view.message, sizeof(view.message), "SOC error: %08lX", (unsigned long)(u32)soc_result);
    uint16_t port = 0;
    uint64_t refresh = 0;

    while (aptMainLoop()) {
        hidScanInput();
        uint64_t now = osGetTime();
        if (now >= refresh) {
            u32 status = 0;
            view.connected = R_SUCCEEDED(ac_result) && R_SUCCEEDED(ACU_GetWifiStatus(&status)) && status;
            snprintf(view.local_ip, sizeof(view.local_ip), "--");
            if (view.connected && R_SUCCEEDED(soc_result)) {
                struct in_addr local = { .s_addr = (in_addr_t)gethostid() };
                snprintf(view.local_ip, sizeof(view.local_ip), "%s", inet_ntoa(local));
            }
            refresh = now + 1000;
        }
        UiAction action = ui_input(&view);
        if (action == UI_EXIT || (hidKeysDown() & KEY_START)) break;
        if (action == UI_PREV && view.page) view.page--;
        if (action == UI_NEXT && (view.page+1)*UI_ROWS < view.visible_count) view.page++;
        if (action == UI_FILTER) view.filter_menu = true;
        if (action == UI_FILTER_CLOSE) view.filter_menu = false;
        if (action >= UI_ALL && action <= UI_ERRORS) {
            view.filter = (ResultFilter)(action - UI_ALL);
            ui_refilter(&view);
            view.filter_menu = false;
        }
        if (action == UI_CODE && !view.running) {
            char code[16];
            snprintf(code, sizeof(code), "%d", view.error_code);
            if (input(code, sizeof(code), "Exact error code shown in results (signed)")) {
                char *end;
                errno = 0;
                long value = strtol(code, &end, 10);
                if (*code && !*end && !errno && value >= INT_MIN && value <= INT_MAX) {
                    view.error_code = (int)value;
                    view.filter = FILTER_CODE;
                    ui_refilter(&view);
                    view.filter_menu = false;
                } else {
                    snprintf(view.message, sizeof(view.message), "Invalid error code");
                    view.filter_menu = false;
                }
            }
        }
        if (action == UI_SCAN && view.running) {
            scanner_cancel(&scanner);
            view.running = false;
            snprintf(view.message, sizeof(view.message), "Cancelled: %u/%u checked", view.checked, view.total);
        } else if (!view.running) {
            if (action == UI_TARGET) {
                char candidate[16];
                snprintf(candidate, sizeof(candidate), "%s", view.target);
                if (input(candidate, sizeof(candidate), "Target IPv4 (e.g. 192.168.1.10)")) {
                    if (scanner_valid_ipv4(candidate)) {
                        snprintf(view.target, sizeof(view.target), "%s", candidate);
                        snprintf(view.message, sizeof(view.message), "Target ready. Tap Start scan.");
                    } else snprintf(view.message, sizeof(view.message), "Invalid IPv4 address");
                }
            }
            if (action == UI_MODE) view.range = !view.range;
            if (action == UI_RANGE) {
                unsigned a = view.first, b = view.last;
                if (port_input(&a, "First TCP port (1-65535)") &&
                    port_input(&b, "Last TCP port (1-65535)") && a <= b) {
                    view.first = a; view.last = b;
                    snprintf(view.message, sizeof(view.message), "Range updated");
                } else snprintf(view.message, sizeof(view.message), "Range unchanged");
            }
            if (action == UI_SCAN && view.network_ready && scanner_valid_ipv4(view.target)) {
                scanner_cancel(&scanner);
                view.running = true; view.checked = 0; view.visible_count = 0; view.opened = 0; view.page = 0;
                view.total = view.range ? view.last-view.first+1 : sizeof(common)/sizeof(common[0]);
                snprintf(view.result_target, sizeof(view.result_target), "%s", view.target);
                snprintf(view.message, sizeof(view.message), "Scanning...");
            }
        }
        if (view.running) {
            if (scanner.state == SCAN_IDLE) {
                port = view.range ? (uint16_t)(view.first+view.checked) : common[view.checked];
                scanner_start(&scanner, view.target, port, now, 1000);
                snprintf(view.message, sizeof(view.message), "Connecting to %u/tcp...", port);
            }
            ScanState state = scanner_tick(&scanner, now);
            if (state != SCAN_PENDING) {
                bool follow = !view.visible_count || view.page == (view.visible_count-1)/UI_ROWS;
                view.rows[view.checked++] = (UiRow){ port, state, scanner.error };
                if (ui_row_matches(&view, &view.rows[view.checked-1]))
                    view.visible[view.visible_count++] = view.checked-1;
                if (follow && view.visible_count) view.page = (view.visible_count-1)/UI_ROWS;
                if (state == SCAN_OPEN) view.opened++;
                snprintf(view.message, sizeof(view.message), "%u/tcp: %s", port, scanner_label(state));
                scanner_cancel(&scanner);
                if (view.checked == view.total) {
                    view.running = false;
                    snprintf(view.message, sizeof(view.message), "Complete: %u open / %u checked", view.opened, view.total);
                }
            }
        }
        ui_draw(&view);
    }
    scanner_cancel(&scanner);
    free(view.rows);
    free(view.visible);
    if (R_SUCCEEDED(soc_result)) socExit();
    free(buffer);
    if (R_SUCCEEDED(ac_result)) acExit();
    ui_exit();
    gfxExit();
    return 0;
}
