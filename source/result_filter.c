#include "ui.h"

bool ui_row_matches(const UiState *s, const UiRow *row) {
    switch (s->filter) {
        case FILTER_OPEN: return row->state == SCAN_OPEN;
        case FILTER_CLOSED: return row->state == SCAN_CLOSED;
        case FILTER_TIMEOUT: return row->state == SCAN_TIMEOUT;
        case FILTER_ERRORS: return row->state == SCAN_ERROR;
        case FILTER_CODE: return row->state != SCAN_OPEN && row->error == s->error_code;
        default: return true;
    }
}

void ui_refilter(UiState *s) {
    s->visible_count = 0;
    s->page = 0;
    for (unsigned i = 0; i < s->checked; ++i)
        if (ui_row_matches(s, &s->rows[i])) s->visible[s->visible_count++] = i;
}

const char *ui_filter_label(ResultFilter filter) {
    switch (filter) {
        case FILTER_OPEN: return "Open";
        case FILTER_CLOSED: return "Closed";
        case FILTER_TIMEOUT: return "No response";
        case FILTER_ERRORS: return "Errors";
        case FILTER_CODE: return "Error code";
        default: return "All";
    }
}
