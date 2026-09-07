#include "ui.h"
#include <citro2d.h>
#include <stdio.h>

static C3D_RenderTarget *top, *bottom;
static C2D_TextBuf text_buffer;
static UiAction pressed;
static u32 ink, muted, background, panel, accent, green, amber, red;
typedef struct { int x, y, w, h; UiAction action; const char *label; } Button;
static const Button buttons[] = {
    {262, 8, 48, 25, UI_EXIT, "Exit"},
    {10, 139, 96, 34, UI_TARGET, "Target"},
    {112, 139, 96, 34, UI_MODE, "Mode"},
    {214, 139, 96, 34, UI_RANGE, "Edit range"},
    {10, 178, 44, 22, UI_PREV, "<"},
    {60, 178, 200, 22, UI_FILTER, "Filter: All"},
    {266, 178, 44, 22, UI_NEXT, ">"},
    {10, 205, 300, 30, UI_SCAN, "Start scan"},
};

static const Button filters[] = {
    {20, 48, 136, 32, UI_ALL, "All results"},
    {164, 48, 136, 32, UI_OPEN, "Open"},
    {20, 88, 136, 32, UI_CLOSED, "Closed"},
    {164, 88, 136, 32, UI_TIMEOUT, "No response"},
    {20, 128, 136, 32, UI_ERRORS, "Errors"},
    {164, 128, 136, 32, UI_CODE, "Exact code..."},
    {20, 192, 280, 34, UI_FILTER_CLOSE, "Back"},
};

static bool enabled(const UiState *s, UiAction a) {
    switch (a) {
        case UI_TARGET: case UI_MODE: return !s->running;
        case UI_RANGE: return !s->running && s->range;
        case UI_PREV: return s->page > 0;
        case UI_NEXT: return (s->page + 1) * UI_ROWS < s->visible_count;
        case UI_CODE: return !s->running;
        case UI_FILTER: return s->rows && s->visible;
        case UI_SCAN: return s->running || (s->network_ready && s->connected && *s->target);
        default: return true;
    }
}

static UiAction hit(const UiState *s, touchPosition p) {
    const Button *items = s->filter_menu ? filters : buttons;
    unsigned count = s->filter_menu ? sizeof(filters)/sizeof(filters[0]) : sizeof(buttons)/sizeof(buttons[0]);
    for (unsigned i = 0; i < count; ++i) {
        const Button *b = &items[i];
        if (p.px >= b->x && p.px < b->x+b->w && p.py >= b->y &&
            p.py < b->y+b->h && enabled(s, b->action)) return b->action;
    }
    return UI_NONE;
}

UiAction ui_input(const UiState *s) {
    static UiAction current;
    touchPosition p;
    if (hidKeysDown() & KEY_TOUCH) {
        hidTouchRead(&p); pressed = current = hit(s, p);
    } else if (hidKeysHeld() & KEY_TOUCH) {
        hidTouchRead(&p); current = hit(s, p);
        if (current != pressed) pressed = UI_NONE;
    }
    if (hidKeysUp() & KEY_TOUCH) {
        UiAction action = pressed == current ? pressed : UI_NONE;
        pressed = current = UI_NONE;
        return enabled(s, action) ? action : UI_NONE;
    }
    return UI_NONE;
}

static void rect(float x, float y, float w, float h, u32 color) {
    C2D_DrawRectSolid(x, y, 0, w, h, color);
}

static void label(float x, float y, float scale, u32 color, const char *str, float max_width) {
    C2D_Text text;
    C2D_TextParse(&text, text_buffer, str);
    C2D_TextOptimize(&text);
    float sx = scale;
    if (text.width * sx > max_width) sx = max_width / text.width;
    C2D_DrawText(&text, C2D_WithColor, x, y, 0.5f, sx, scale, color);
}

bool ui_init(void) {
    if (!C3D_Init(C3D_DEFAULT_CMDBUF_SIZE)) return false;
    if (!C2D_Init(C2D_DEFAULT_MAX_OBJECTS)) { C3D_Fini(); return false; }
    C2D_Prepare();
    top = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    bottom = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);
    text_buffer = C2D_TextBufNew(4096);
    if (!top || !bottom || !text_buffer) { ui_exit(); return false; }
    ink = C2D_Color32(232,239,248,255); muted = C2D_Color32(156,173,193,255);
    background = C2D_Color32(14,20,29,255); panel = C2D_Color32(26,36,49,255);
    accent = C2D_Color32(30,112,193,255); green = C2D_Color32(83,217,157,255);
    amber = C2D_Color32(245,193,91,255); red = C2D_Color32(237,108,122,255);
    return true;
}

void ui_exit(void) {
    if (text_buffer) C2D_TextBufDelete(text_buffer);
    C2D_Fini(); C3D_Fini();
}

void ui_draw(const UiState *s) {
    char line[128];
    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
    C2D_TextBufClear(text_buffer);
    C2D_TargetClear(top, background); C2D_SceneBegin(top);
    rect(0, 0, 400, 39, panel);
    label(12, 7, .65f, ink, "Scan results", 190);
    label(222, 13, .45f, muted, *s->result_target ? s->result_target : "No scan yet", 166);
    rect(10, 45, 380, 23, accent);
    label(20, 48, .45f, ink, "PORT / TCP", 90);
    label(123, 48, .45f, ink, "STATUS", 110);
    label(264, 48, .45f, ink, "DETAIL", 115);
    for (unsigned i = 0; i < UI_ROWS; i++) {
        unsigned n = s->page * UI_ROWS + i;
        float y = 70 + 20*i;
        rect(10, y, 380, 19, i % 2 ? background : panel);
        if (n >= s->visible_count) continue;
        const UiRow *r = &s->rows[s->visible[n]];
        u32 color = r->state == SCAN_OPEN ? green : r->state == SCAN_TIMEOUT ? amber :
                    r->state == SCAN_ERROR ? red : muted;
        rect(10, y, 3, 19, color);
        snprintf(line, sizeof(line), "%u", r->port);
        label(20, y+1, .45f, ink, line, 90);
        label(123, y+1, .45f, color, scanner_label(r->state), 130);
        const char *detail = r->state == SCAN_OPEN ? "Connected" :
                             r->state == SCAN_CLOSED ? "Refused" : "Timed out";
        if (r->error) {
            snprintf(line, sizeof(line), "Error %d", r->error); detail = line;
        }
        label(264, y+1, .42f, muted, detail, 116);
    }
    if (!s->checked) label(34, 118, .5f, muted, "Choose a target, then tap Start scan.", 330);
    else if (!s->visible_count) label(34, 118, .5f, muted, "No results match this filter.", 330);
    snprintf(line, sizeof(line), "%u shown / %u checked", s->visible_count, s->checked);
    label(12, 216, .43f, ink, line, 195);
    unsigned pages = s->visible_count ? (s->visible_count + UI_ROWS - 1) / UI_ROWS : 1;
    snprintf(line, sizeof(line), "Page %u / %u | %u open", s->page+1, pages, s->opened);
    label(210, 216, .4f, muted, line, 178);

    C2D_TargetClear(bottom, background); C2D_SceneBegin(bottom);
    if (!s->filter_menu) {
    rect(0, 0, 320, 86, panel);
    label(10, 6, .65f, ink, "Pocket Recon", 240);
    snprintf(line, sizeof(line), "Wi-Fi %s  |  %s", s->connected ? "connected" : "offline", s->local_ip);
    label(10, 34, .4f, muted, line, 300);
    snprintf(line, sizeof(line), "Target: %s", *s->target ? s->target : "tap Target to set");
    label(10, 50, .45f, ink, line, 300);
    if (s->range) snprintf(line, sizeof(line), "TCP %u-%u  |  1s timeout", s->first, s->last);
    else snprintf(line, sizeof(line), "21 common TCP ports  |  1s timeout");
    label(10, 68, .4f, muted, line, 300);
    unsigned percent = s->total ? s->checked * 100 / s->total : 0;
    snprintf(line, sizeof(line), "%u/%u checked   -   %u%%", s->checked, s->total, percent);
    label(10, 89, .42f, ink, line, 300);
    rect(10, 108, 300, 8, C2D_Color32(46,61,79,255));
    if (s->total) rect(10, 108, 300.0f*s->checked/s->total, 8, s->running ? accent : green);
    label(10, 119, .4f, muted, s->message, 300);
    } else {
        label(20, 12, .65f, ink, "Filter results", 280);
        label(20, 169, .4f, muted, s->running ? "Exact code entry available after scan." :
              "Exact code matches the signed number shown.", 280);
    }
    const Button *items = s->filter_menu ? filters : buttons;
    unsigned count = s->filter_menu ? sizeof(filters)/sizeof(filters[0]) : sizeof(buttons)/sizeof(buttons[0]);
    for (unsigned i = 0; i < count; i++) {
        const Button *b = &items[i];
        bool active = enabled(s, b->action);
        u32 color = active ? panel : C2D_Color32(21,28,38,255);
        if (b->action == UI_SCAN && active) color = s->running ? C2D_Color32(156,43,62,255) : accent;
        if (pressed == b->action) color = C2D_Color32(52,79,110,255);
        rect(b->x, b->y+2, b->w, b->h, C2D_Color32(5,10,17,255));
        rect(b->x, b->y, b->w, b->h, color);
        const char *title = b->action == UI_SCAN && s->running ? "Cancel scan" : b->label;
        if (b->action == UI_FILTER) {
            if (s->filter == FILTER_CODE) snprintf(line, sizeof(line), "Filter: code %d", s->error_code);
            else snprintf(line, sizeof(line), "Filter: %s", ui_filter_label(s->filter));
            title = line;
        }
        label(b->x+8, b->y+(b->h-18)/2, .48f,
              active ? ink : muted, title, b->w-16);
    }
    C3D_FrameEnd(0);
}
