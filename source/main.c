#include <3ds.h>
#include <arpa/inet.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "scanner.h"

static const uint16_t common[] = {21,22,23,25,53,80,110,139,143,443,445,
                                 554,631,993,995,1883,3389,5900,8000,8080,8443};
static PrintConsole top, bottom;

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
    consoleInit(GFX_TOP, &top);
    consoleInit(GFX_BOTTOM, &bottom);
    Result ac_result = acInit();
    void *buffer = memalign(0x1000, 0x100000);
    Result soc_result = buffer ? socInit(buffer, 0x100000) : (Result)-1;
    Scanner scanner;
    scanner_init(&scanner);
    char target[16] = "", message[96] = "X: enter a target to begin";
    bool running = false, range = false, dirty = true;
    unsigned first = 1, last = 1024, index = 0, total = 0, opened = 0;
    uint16_t port = 0;
    uint64_t refresh = 0;
    consoleSelect(&top);
    printf("Pocket Recon | TCP connect scanner\n\nResults appear here.\n");

    while (aptMainLoop()) {
        hidScanInput();
        u32 keys = hidKeysDown();
        if (keys & KEY_START) break;
        uint64_t now = osGetTime();
        if (running && (keys & KEY_B)) {
            scanner_cancel(&scanner);
            running = false;
            snprintf(message, sizeof(message), "Cancelled: %u/%u checked", index, total);
            dirty = true;
        }
        if (!running) {
            if (keys & KEY_X) {
                char candidate[16];
                snprintf(candidate, sizeof(candidate), "%s", target);
                if (input(candidate, sizeof(candidate), "Target IPv4 (e.g. 192.168.1.10)")) {
                    if (scanner_valid_ipv4(candidate)) {
                        snprintf(target, sizeof(target), "%s", candidate);
                        snprintf(message, sizeof(message), "Target ready. A: scan");
                    } else snprintf(message, sizeof(message), "Invalid IPv4 address");
                }
                dirty = true;
            }
            if (keys & KEY_Y) { range = !range; dirty = true; }
            if ((keys & KEY_L) && range) {
                unsigned a = first, b = last;
                if (port_input(&a, "First TCP port (1-65535)") &&
                    port_input(&b, "Last TCP port (1-65535)") && a <= b) {
                    first = a; last = b;
                    snprintf(message, sizeof(message), "Range updated");
                } else snprintf(message, sizeof(message), "Range unchanged");
                dirty = true;
            }
            if (keys & KEY_A) {
                u32 status = 0;
                if (R_FAILED(soc_result) || R_FAILED(ac_result))
                    snprintf(message, sizeof(message), "Network initialization failed");
                else if (!scanner_valid_ipv4(target))
                    snprintf(message, sizeof(message), "X: enter an IPv4 target first");
                else if (R_FAILED(ACU_GetWifiStatus(&status)) || !status)
                    snprintf(message, sizeof(message), "Connect Wi-Fi in System Settings");
                else {
                    scanner_cancel(&scanner);
                    running = true; index = 0; opened = 0;
                    total = range ? last - first + 1 : sizeof(common)/sizeof(common[0]);
                    consoleSelect(&top); consoleClear();
                    printf("Pocket Recon | %s\n", target);
                    printf("Open ports and errors:\n\n");
                    snprintf(message, sizeof(message), "Scanning...");
                }
                dirty = true;
            }
        }
        if (running) {
            if (scanner.state == SCAN_IDLE) {
                port = range ? (uint16_t)(first + index) : common[index];
                scanner_start(&scanner, target, port, now, 1000);
            }
            ScanState state = scanner_tick(&scanner, now);
            if (state != SCAN_PENDING) {
                index++;
                if (state == SCAN_OPEN) {
                    opened++;
                    consoleSelect(&top); printf("%5u/tcp  open\n", port);
                } else if (state == SCAN_ERROR) {
                    consoleSelect(&top);
                    printf("%5u/tcp  error %d\n", port, scanner.error);
                }
                snprintf(message, sizeof(message), "%u/tcp: %s", port, scanner_label(state));
                scanner_cancel(&scanner);
                if (index == total) {
                    running = false;
                    consoleSelect(&top);
                    printf("\nDone: %u open / %u checked\n", opened, total);
                }
                dirty = true;
            }
        }
        if (dirty || now >= refresh) {
            consoleSelect(&bottom); consoleClear();
            u32 status = 0;
            bool connected = R_SUCCEEDED(ac_result) &&
                R_SUCCEEDED(ACU_GetWifiStatus(&status)) && status;
            printf("Pocket Recon - early prototype\n\n");
            printf("Wi-Fi: %s\n", connected ? "connected" : "disconnected");
            if (R_SUCCEEDED(soc_result)) {
                struct in_addr local = { .s_addr = (in_addr_t)gethostid() };
                printf("Local IP: %s\n", inet_ntoa(local));
            }
            if (R_FAILED(soc_result)) printf("SOC error: %08lX\n", (unsigned long)(u32)soc_result);
            if (R_FAILED(ac_result)) printf("AC error: %08lX\n", (unsigned long)(u32)ac_result);
            printf("Target: %s\n", *target ? target : "unset");
            if (range) printf("Ports: %u-%u\n", first, last);
            else printf("Ports: 21 common TCP ports\n");
            printf("Timeout: 1 second per port\n\n");
            printf("%s\n%u/%u checked | %u open\n\n", running ? "Scanning" : "Idle", index, total, opened);
            printf("%s\n\n", message);
            printf("A Scan      B Cancel\nX Target    Y Mode\nL Edit range\nSTART Exit\n\n");
            printf("No response is not proof of filtering.\n");
            refresh = now + 1000; dirty = false;
        }
        gfxFlushBuffers(); gfxSwapBuffers(); gspWaitForVBlank();
    }
    scanner_cancel(&scanner);
    if (R_SUCCEEDED(soc_result)) socExit();
    free(buffer);
    if (R_SUCCEEDED(ac_result)) acExit();
    gfxExit();
    return 0;
}
