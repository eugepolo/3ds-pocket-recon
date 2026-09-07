#include "ui.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    UiRow rows[] = {
        {22, SCAN_OPEN, 0}, {23, SCAN_CLOSED, 14},
        {80, SCAN_OPEN, 0}, {81, SCAN_TIMEOUT, 76},
        {82, SCAN_ERROR, -40}, {83, SCAN_ERROR, -14}
    };
    unsigned visible[6];
    UiState s = { .rows = rows, .visible = visible, .checked = 6, .page = 4 };
    ui_refilter(&s);
    assert(s.visible_count == 6 && s.page == 0 && visible[5] == 5);
    s.filter = FILTER_OPEN;
    ui_refilter(&s);
    assert(s.visible_count == 2 && visible[0] == 0 && visible[1] == 2);
    s.filter = FILTER_CLOSED;
    ui_refilter(&s);
    assert(s.visible_count == 1 && visible[0] == 1);
    s.filter = FILTER_TIMEOUT;
    ui_refilter(&s);
    assert(s.visible_count == 1 && visible[0] == 3);
    s.filter = FILTER_ERRORS;
    ui_refilter(&s);
    assert(s.visible_count == 2 && visible[0] == 4 && visible[1] == 5);
    s.filter = FILTER_CODE;
    s.error_code = -14;
    ui_refilter(&s);
    assert(s.visible_count == 1 && visible[0] == 5);
    s.error_code = 14;
    ui_refilter(&s);
    assert(s.visible_count == 1 && visible[0] == 1);
    s.error_code = 999;
    ui_refilter(&s);
    assert(s.visible_count == 0 && s.checked == 6);
    UiRow incoming = {8080, SCAN_ERROR, 999};
    assert(ui_row_matches(&s, &incoming));
    incoming.state = SCAN_OPEN;
    assert(!ui_row_matches(&s, &incoming));
    s.checked = 0;
    ui_refilter(&s);
    assert(s.visible_count == 0 && s.page == 0);
    puts("PASS: status filters, signed codes, ordering, empty results, page reset, incoming rows");
}
