#pragma once
#include "scanner.h"

#define UI_ROWS 7
typedef enum { FILTER_ALL, FILTER_OPEN, FILTER_CLOSED, FILTER_TIMEOUT,
               FILTER_ERRORS, FILTER_CODE } ResultFilter;
typedef struct { unsigned port; ScanState state; int error; } UiRow;
typedef struct {
    char target[16], result_target[16], local_ip[16], message[96];
    bool running, range, connected, network_ready;
    unsigned first, last, checked, total, opened, page;
    UiRow *rows;
    unsigned *visible, visible_count;
    ResultFilter filter;
    int error_code;
    bool filter_menu;
} UiState;
typedef enum { UI_NONE, UI_TARGET, UI_MODE, UI_RANGE, UI_SCAN,
               UI_PREV, UI_NEXT, UI_EXIT, UI_FILTER, UI_FILTER_CLOSE,
               UI_ALL, UI_OPEN, UI_CLOSED, UI_TIMEOUT, UI_ERRORS, UI_CODE } UiAction;
bool ui_init(void);
void ui_exit(void);
UiAction ui_input(const UiState *s);
void ui_draw(const UiState *s);

bool ui_row_matches(const UiState *s, const UiRow *row);
void ui_refilter(UiState *s);
const char *ui_filter_label(ResultFilter filter);
