#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef enum { SCAN_IDLE, SCAN_PENDING, SCAN_OPEN, SCAN_CLOSED,
               SCAN_TIMEOUT, SCAN_ERROR } ScanState;
typedef struct {
    int fd;
    int error;
    uint64_t deadline;
    ScanState state;
} Scanner;
bool scanner_valid_ipv4(const char *target);
void scanner_init(Scanner *s);
void scanner_cancel(Scanner *s);
void scanner_start(Scanner *s, const char *target, uint16_t port,
                   uint64_t now_ms, unsigned timeout_ms);
ScanState scanner_tick(Scanner *s, uint64_t now_ms);
const char *scanner_label(ScanState state);
