#include "fortify/systems/boot/boot.h"
#include "fortify/systems/logging/log.h"
#include "fortify/systems/subsystems/subsystems.h"

namespace {
struct vec3 {
    int32_t x;
    int32_t y;
    int32_t z;
};

struct point2 {
    int32_t x;
    int32_t y;
};

struct face {
    uint8_t i0;
    uint8_t i1;
    uint8_t i2;
    uint8_t color_r;
    uint8_t color_g;
    uint8_t color_b;
};

struct sort_face {
    uint8_t face_index;
    int32_t depth;
};

limine_framebuffer *g_fb = 0;
uint8_t *g_fb_base = 0;
uint8_t *g_draw_base = 0;
uint64_t g_draw_pitch = 0;
bool g_double_buffer_enabled = false;
bool g_video_ready = false;
bool g_demo_enabled = true;
bool g_compositor_ready = false;
uint64_t g_render_style = 2; // 0=solid, 1=wire, 2=both
uint64_t g_spin_step = 1;
uint64_t g_font_style = 0;   // 0=mono, 1=bold, 2=shadow
uint64_t g_text_scale = 2;   // 1..4
uint64_t g_last_render_tick = 0;
uint64_t g_last_phase_tick = 0;
uint64_t g_rotation_phase_q16 = 0;
uint64_t g_fps_window_start_tick = 0;
uint64_t g_fps_frames_in_window = 0;
uint64_t g_fps_display = 0;
bool g_cube_easter_mode = false;
uint64_t g_cube_easter_pulse_until_tick = 0;
char g_input_surface_draft[64] = "ready";
char g_command_center_last_command[64] = "boot";
char g_command_center_status[64] = "idle";
char g_submitted_command[64] = {};
bool g_submitted_command_ready = false;
uint64_t g_input_surface_id = 0;
uint64_t g_command_center_surface_id = 0;
uint64_t g_focused_surface_id = 0;

constexpr uint64_t kStyleSolid = 0;
constexpr uint64_t kStyleWire = 1;
constexpr uint64_t kStyleBoth = 2;
constexpr uint64_t kFontMono = 0;
constexpr uint64_t kFontBold = 1;
constexpr uint64_t kFontShadow = 2;
constexpr uint64_t kGlyphW = 6;
constexpr uint64_t kGlyphH = 8;
constexpr uint64_t kRenderTickStride = 1; // 100Hz PIT -> ~100 FPS panel updates
constexpr uint64_t kPhaseAdvancePerTickQ16 = 13981; // ~12s full rotation at spin=1 on 100Hz PIT
constexpr uint64_t kBackbufferMaxWidth = 1920;
constexpr uint64_t kBackbufferMaxHeight = 1080;
alignas(16) uint32_t g_backbuffer[kBackbufferMaxWidth * kBackbufferMaxHeight];

constexpr uint64_t kMaxSurfaces = 16;
constexpr uint8_t kLayerDesktop = 0;
constexpr uint8_t kLayerWorkspace = 1;
constexpr uint8_t kLayerApplications = 2;
constexpr uint8_t kLayerPanels = 3;

struct compositor_surface_entry {
    bool used;
    surface_info info;
};

compositor_surface_entry g_surfaces[kMaxSurfaces] = {};
uint64_t g_next_surface_id = 0;

uint32_t compose_rgb(uint8_t r, uint8_t g, uint8_t b);
void fill_rect(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color);
void draw_line(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t color);
void draw_text(int32_t x, int32_t y, const char *text, uint32_t fg, uint32_t bg);

constexpr vec3 kCubeVertices[8] = {
    {-96, -96, -96}, {96, -96, -96}, {96, 96, -96}, {-96, 96, -96},
    {-96, -96, 96},  {96, -96, 96},  {96, 96, 96},  {-96, 96, 96},
};

constexpr face kCubeFaces[12] = {
    {0, 1, 2, 180, 64, 255}, {0, 2, 3, 180, 64, 255},
    {4, 6, 5, 64, 190, 255}, {4, 7, 6, 64, 190, 255},
    {0, 3, 7, 80, 220, 160}, {0, 7, 4, 80, 220, 160},
    {1, 5, 6, 255, 200, 80}, {1, 6, 2, 255, 200, 80},
    {3, 2, 6, 255, 96, 120}, {3, 6, 7, 255, 96, 120},
    {0, 4, 5, 220, 220, 220}, {0, 5, 1, 220, 220, 220},
};

uint8_t to_upper_ascii(uint8_t c) {
    if (c >= 'a' && c <= 'z') {
        return static_cast<uint8_t>(c - ('a' - 'A'));
    }
    return c;
}

void set_rows(uint8_t *rows,
              uint8_t r0,
              uint8_t r1,
              uint8_t r2,
              uint8_t r3,
              uint8_t r4,
              uint8_t r5,
              uint8_t r6) {
    rows[0] = r0;
    rows[1] = r1;
    rows[2] = r2;
    rows[3] = r3;
    rows[4] = r4;
    rows[5] = r5;
    rows[6] = r6;
}

void clear_surface_entry(compositor_surface_entry *entry) {
    if (entry == 0) {
        return;
    }
    entry->used = false;
    entry->info.id = 0;
    entry->info.type = surface_type::application;
    entry->info.state = surface_state::floating;
    entry->info.layer = kLayerApplications;
    entry->info.x = 0;
    entry->info.y = 0;
    entry->info.width = 0;
    entry->info.height = 0;
    entry->info.capabilities = 0;
    for (uint64_t i = 0; i < sizeof(entry->info.title); i++) {
        entry->info.title[i] = '\0';
    }
}

bool copy_surface_title(const char *title, char *out, uint64_t out_size) {
    if (out == 0 || out_size < 2) {
        return false;
    }
    uint64_t index = 0;
    if (title != 0) {
        while (title[index] != '\0' && index + 1 < out_size) {
            const uint8_t ch = static_cast<uint8_t>(title[index]);
            if (ch < 0x20 || ch > 0x7e) {
                return false;
            }
            out[index] = static_cast<char>(ch);
            index++;
        }
    }
    if (index == 0) {
        const char *fallback = "Surface";
        while (fallback[index] != '\0' && index + 1 < out_size) {
            out[index] = fallback[index];
            index++;
        }
    }
    out[index] = '\0';
    return true;
}

bool surface_title_equals(const char *title, const char *expected) {
    if (title == 0 || expected == 0) {
        return false;
    }
    uint64_t i = 0;
    while (title[i] != '\0' && expected[i] != '\0') {
        if (title[i] != expected[i]) {
            return false;
        }
        i++;
    }
    return title[i] == '\0' && expected[i] == '\0';
}

bool copy_ascii_text(const char *text, char *out, uint64_t out_size, const char *fallback) {
    if (out == 0 || out_size < 2) {
        return false;
    }
    uint64_t index = 0;
    if (text != 0) {
        while (text[index] != '\0' && index + 1 < out_size) {
            const uint8_t ch = static_cast<uint8_t>(text[index]);
            if (ch < 0x20 || ch > 0x7e) {
                return false;
            }
            out[index] = static_cast<char>(ch);
            index++;
        }
    }
    if (index == 0 && fallback != 0) {
        while (fallback[index] != '\0' && index + 1 < out_size) {
            out[index] = fallback[index];
            index++;
        }
    }
    out[index] = '\0';
    return true;
}

compositor_surface_entry *find_surface_by_id(uint64_t surface_id) {
    if (surface_id == 0) {
        return 0;
    }
    for (uint64_t i = 0; i < kMaxSurfaces; i++) {
        if (g_surfaces[i].used && g_surfaces[i].info.id == surface_id) {
            return &g_surfaces[i];
        }
    }
    return 0;
}

void input_surface_set_ready(void) {
    copy_ascii_text("ready", g_input_surface_draft, sizeof(g_input_surface_draft), "ready");
}

void input_surface_append_char(char value) {
    if (value < 0x20 || value > 0x7e) {
        return;
    }
    const bool was_ready = surface_title_equals(g_input_surface_draft, "ready");
    if (was_ready) {
        g_input_surface_draft[0] = '\0';
    }
    uint64_t len = 0;
    while (len + 1 < sizeof(g_input_surface_draft) && g_input_surface_draft[len] != '\0') {
        len++;
    }
    if (len + 1 >= sizeof(g_input_surface_draft)) {
        return;
    }
    g_input_surface_draft[len++] = value;
    g_input_surface_draft[len] = '\0';
}

void input_surface_backspace(void) {
    if (surface_title_equals(g_input_surface_draft, "ready")) {
        return;
    }
    uint64_t len = 0;
    while (len < sizeof(g_input_surface_draft) && g_input_surface_draft[len] != '\0') {
        len++;
    }
    if (len == 0) {
        input_surface_set_ready();
        return;
    }
    g_input_surface_draft[len - 1] = '\0';
    if (g_input_surface_draft[0] == '\0') {
        input_surface_set_ready();
    }
}

bool input_surface_submit(void) {
    if (surface_title_equals(g_input_surface_draft, "ready") || g_input_surface_draft[0] == '\0') {
        return false;
    }
    if (!copy_ascii_text(g_input_surface_draft, g_submitted_command, sizeof(g_submitted_command), 0)) {
        return false;
    }
    g_submitted_command_ready = true;
    return true;
}

void assign_surface_defaults(surface_type type, surface_info *info, uint64_t ordinal) {
    if (info == 0) {
        return;
    }
    info->type = type;
    info->x = 48 + static_cast<int32_t>((ordinal % 6) * 36);
    info->y = 64 + static_cast<int32_t>((ordinal % 4) * 28);
    info->width = 720;
    info->height = 420;
    info->layer = kLayerApplications;
    info->state = surface_state::floating;
    if (type == surface_type::desktop) {
        info->x = 0;
        info->y = 0;
        info->width = g_fb != 0 ? static_cast<uint32_t>(g_fb->width) : 1280U;
        info->height = g_fb != 0 ? static_cast<uint32_t>(g_fb->height) : 720U;
        info->layer = kLayerDesktop;
        info->state = surface_state::background;
    } else if (type == surface_type::workspace) {
        info->x = 24;
        info->y = 32;
        if (g_fb != 0) {
            info->width = g_fb->width > 96 ? static_cast<uint32_t>(g_fb->width - 48) : static_cast<uint32_t>(g_fb->width);
            info->height = g_fb->height > 128 ? static_cast<uint32_t>(g_fb->height - 96) : static_cast<uint32_t>(g_fb->height);
        } else {
            info->width = 1232U;
            info->height = 624U;
        }
        info->layer = kLayerWorkspace;
        info->state = surface_state::anchored;
    } else if (type == surface_type::dashboard) {
        info->x = 24;
        info->y = 40;
        info->width = 420;
        info->height = 260;
        info->layer = kLayerPanels;
        info->state = surface_state::panel;
    } else if (type == surface_type::tool) {
        info->x = 24;
        info->y = 0;
        if (g_fb != 0) {
            info->width = g_fb->width > 48 ? static_cast<uint32_t>(g_fb->width - 48) : static_cast<uint32_t>(g_fb->width);
            info->height = 72;
            const uint32_t max_y = g_fb->height > info->height + 16 ? static_cast<uint32_t>(g_fb->height - info->height - 16) : 0U;
            info->y = static_cast<int32_t>(max_y);
        } else {
            info->width = 1232U;
            info->height = 72U;
            info->y = 632;
        }
        info->layer = kLayerPanels;
        info->state = surface_state::docked;
    } else if (type == surface_type::document) {
        info->width = 840;
        info->height = 540;
        info->layer = kLayerApplications;
        info->state = surface_state::floating;
    }
}

const char *surface_type_label(surface_type type) {
    switch (type) {
        case surface_type::desktop: return "DESKTOP";
        case surface_type::application: return "APPLICATION";
        case surface_type::document: return "DOCUMENT";
        case surface_type::workspace: return "WORKSPACE";
        case surface_type::dashboard: return "DASHBOARD";
        case surface_type::tool: return "TOOL";
    }
    return "UNKNOWN";
}

const char *surface_state_label(surface_state state) {
    switch (state) {
        case surface_state::floating: return "FLOATING";
        case surface_state::docked: return "DOCKED";
        case surface_state::anchored: return "ANCHORED";
        case surface_state::fullscreen: return "FULLSCREEN";
        case surface_state::collapsed: return "COLLAPSED";
        case surface_state::background: return "BACKGROUND";
        case surface_state::modal: return "MODAL";
        case surface_state::panel: return "PANEL";
    }
    return "UNKNOWN";
}

uint32_t surface_type_color(surface_type type) {
    switch (type) {
        case surface_type::desktop: return compose_rgb(44, 78, 140);
        case surface_type::workspace: return compose_rgb(58, 108, 166);
        case surface_type::application: return compose_rgb(82, 132, 208);
        case surface_type::document: return compose_rgb(64, 144, 186);
        case surface_type::dashboard: return compose_rgb(98, 86, 184);
        case surface_type::tool: return compose_rgb(66, 126, 98);
    }
    return compose_rgb(72, 116, 176);
}

void draw_compositor_surface_cards(void) {
    if (g_fb == 0 || !g_compositor_ready) {
        return;
    }
    uint64_t used_indices[kMaxSurfaces] = {};
    uint64_t used_count = 0;
    for (uint64_t i = 0; i < kMaxSurfaces; i++) {
        if (!g_surfaces[i].used) {
            continue;
        }
        used_indices[used_count++] = i;
    }
    for (uint64_t i = 1; i < used_count; i++) {
        const uint64_t key = used_indices[i];
        int64_t j = static_cast<int64_t>(i) - 1;
        while (j >= 0) {
            const surface_info &left = g_surfaces[used_indices[j]].info;
            const surface_info &right = g_surfaces[key].info;
            if (left.layer < right.layer || (left.layer == right.layer && left.id < right.id)) {
                break;
            }
            used_indices[j + 1] = used_indices[j];
            j--;
        }
        used_indices[j + 1] = key;
    }

    const int32_t pad = 12;
    const int32_t panel_w = 360;
    const int32_t panel_h = static_cast<int32_t>((kGlyphH * g_text_scale * 2) + 20 + (used_count * 48));
    const int32_t panel_x = static_cast<int32_t>(g_fb->width > static_cast<uint64_t>(panel_w + pad)
                                                     ? g_fb->width - static_cast<uint64_t>(panel_w + pad)
                                                     : 0);
    const int32_t panel_y = pad;
    fill_rect(panel_x, panel_y, panel_w, panel_h, compose_rgb(12, 18, 34));
    draw_line(panel_x, panel_y, panel_x + panel_w - 1, panel_y, compose_rgb(70, 122, 220));
    draw_line(panel_x, panel_y + panel_h - 1, panel_x + panel_w - 1, panel_y + panel_h - 1, compose_rgb(70, 122, 220));
    draw_line(panel_x, panel_y, panel_x, panel_y + panel_h - 1, compose_rgb(70, 122, 220));
    draw_line(panel_x + panel_w - 1, panel_y, panel_x + panel_w - 1, panel_y + panel_h - 1, compose_rgb(70, 122, 220));
    draw_text(panel_x + 8, panel_y + 6, "SURFACE COMPOSITOR", compose_rgb(238, 246, 255), compose_rgb(12, 18, 34));

    const int32_t card_w = panel_w - 16;
    const int32_t card_h = 40;
    for (uint64_t i = 0; i < used_count; i++) {
        const surface_info &info = g_surfaces[used_indices[i]].info;
        const int32_t card_x = panel_x + 8;
        const int32_t card_y = panel_y + 28 + static_cast<int32_t>(i * 48);
        const uint32_t card_bg = compose_rgb(20, 28, 48);
        const uint32_t accent = surface_type_color(info.type);
        const bool focused = (info.id != 0 && info.id == g_focused_surface_id);
        const uint32_t border = focused ? compose_rgb(124, 186, 255) : compose_rgb(58, 90, 152);
        fill_rect(card_x, card_y, card_w, card_h, card_bg);
        fill_rect(card_x, card_y, 5, card_h, accent);
        draw_line(card_x, card_y, card_x + card_w - 1, card_y, border);
        draw_line(card_x, card_y + card_h - 1, card_x + card_w - 1, card_y + card_h - 1, border);
        draw_line(card_x, card_y, card_x, card_y + card_h - 1, border);
        draw_line(card_x + card_w - 1, card_y, card_x + card_w - 1, card_y + card_h - 1, border);
        draw_text(card_x + 10, card_y + 4, info.title, compose_rgb(232, 240, 255), card_bg);
        draw_text(card_x + 10, card_y + 20, surface_type_label(info.type), compose_rgb(188, 208, 248), card_bg);
        draw_text(card_x + 148, card_y + 20, surface_state_label(info.state), compose_rgb(178, 196, 228), card_bg);
    }

    for (uint64_t i = 0; i < used_count; i++) {
        const surface_info &info = g_surfaces[used_indices[i]].info;
        if (!surface_title_equals(info.title, "Input Surface")) {
            continue;
        }
        const int32_t input_x = info.x;
        const int32_t input_y = info.y;
        const int32_t input_w = static_cast<int32_t>(info.width);
        const int32_t input_h = static_cast<int32_t>(info.height);
        const uint32_t input_bg = compose_rgb(14, 24, 42);
        fill_rect(input_x, input_y, input_w, input_h, input_bg);
        draw_line(input_x, input_y, input_x + input_w - 1, input_y, compose_rgb(86, 140, 228));
        draw_line(input_x, input_y + input_h - 1, input_x + input_w - 1, input_y + input_h - 1, compose_rgb(86, 140, 228));
        draw_line(input_x, input_y, input_x, input_y + input_h - 1, compose_rgb(86, 140, 228));
        draw_line(input_x + input_w - 1, input_y, input_x + input_w - 1, input_y + input_h - 1, compose_rgb(86, 140, 228));
        const int32_t prompt_h = input_h > 20 ? (input_h - 20) : input_h;
        const int32_t prompt_w = input_w > 20 ? (input_w - 20) : input_w;
        fill_rect(input_x + 10, input_y + 10, prompt_w, prompt_h, compose_rgb(8, 16, 32));
        draw_text(input_x + 16, input_y + 16, "INPUT>", compose_rgb(220, 234, 255), compose_rgb(8, 16, 32));
        draw_text(input_x + 88, input_y + 16, g_input_surface_draft, compose_rgb(196, 218, 255), compose_rgb(8, 16, 32));
        break;
    }

    for (uint64_t i = 0; i < used_count; i++) {
        const surface_info &info = g_surfaces[used_indices[i]].info;
        if (!surface_title_equals(info.title, "Command Center")) {
            continue;
        }
        const int32_t center_x = info.x;
        const int32_t center_y = info.y;
        const int32_t center_w = static_cast<int32_t>(info.width);
        const int32_t center_h = static_cast<int32_t>(info.height);
        const uint32_t center_bg = compose_rgb(18, 20, 44);
        fill_rect(center_x, center_y, center_w, center_h, center_bg);
        draw_line(center_x, center_y, center_x + center_w - 1, center_y, compose_rgb(116, 116, 244));
        draw_line(center_x, center_y + center_h - 1, center_x + center_w - 1, center_y + center_h - 1, compose_rgb(116, 116, 244));
        draw_line(center_x, center_y, center_x, center_y + center_h - 1, compose_rgb(116, 116, 244));
        draw_line(center_x + center_w - 1, center_y, center_x + center_w - 1, center_y + center_h - 1, compose_rgb(116, 116, 244));
        draw_text(center_x + 12, center_y + 10, "COMMAND CENTER", compose_rgb(236, 236, 255), center_bg);
        draw_text(center_x + 12, center_y + 30, "LAST", compose_rgb(192, 206, 255), center_bg);
        draw_text(center_x + 56, center_y + 30, g_command_center_last_command, compose_rgb(226, 236, 255), center_bg);
        draw_text(center_x + 12, center_y + 48, "STATUS", compose_rgb(192, 206, 255), center_bg);
        draw_text(center_x + 76, center_y + 48, g_command_center_status, compose_rgb(206, 220, 255), center_bg);
        break;
    }
}

void glyph_5x7(uint8_t c, uint8_t *rows) {
    set_rows(rows, 0, 0, 0, 0, 0, 0, 0);
    c = to_upper_ascii(c);
    switch (c) {
        case 'A': set_rows(rows, 0x04, 0x0a, 0x11, 0x1f, 0x11, 0x11, 0x11); return;
        case 'B': set_rows(rows, 0x1e, 0x11, 0x11, 0x1e, 0x11, 0x11, 0x1e); return;
        case 'C': set_rows(rows, 0x0e, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0e); return;
        case 'D': set_rows(rows, 0x1c, 0x12, 0x11, 0x11, 0x11, 0x12, 0x1c); return;
        case 'E': set_rows(rows, 0x1f, 0x10, 0x10, 0x1e, 0x10, 0x10, 0x1f); return;
        case 'F': set_rows(rows, 0x1f, 0x10, 0x10, 0x1e, 0x10, 0x10, 0x10); return;
        case 'G': set_rows(rows, 0x0f, 0x10, 0x10, 0x13, 0x11, 0x11, 0x0f); return;
        case 'H': set_rows(rows, 0x11, 0x11, 0x11, 0x1f, 0x11, 0x11, 0x11); return;
        case 'I': set_rows(rows, 0x0e, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0e); return;
        case 'J': set_rows(rows, 0x01, 0x01, 0x01, 0x01, 0x11, 0x11, 0x0e); return;
        case 'K': set_rows(rows, 0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11); return;
        case 'L': set_rows(rows, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1f); return;
        case 'M': set_rows(rows, 0x11, 0x1b, 0x15, 0x15, 0x11, 0x11, 0x11); return;
        case 'N': set_rows(rows, 0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11); return;
        case 'O': set_rows(rows, 0x0e, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0e); return;
        case 'P': set_rows(rows, 0x1e, 0x11, 0x11, 0x1e, 0x10, 0x10, 0x10); return;
        case 'Q': set_rows(rows, 0x0e, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0d); return;
        case 'R': set_rows(rows, 0x1e, 0x11, 0x11, 0x1e, 0x14, 0x12, 0x11); return;
        case 'S': set_rows(rows, 0x0f, 0x10, 0x10, 0x0e, 0x01, 0x01, 0x1e); return;
        case 'T': set_rows(rows, 0x1f, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04); return;
        case 'U': set_rows(rows, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0e); return;
        case 'V': set_rows(rows, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0a, 0x04); return;
        case 'W': set_rows(rows, 0x11, 0x11, 0x11, 0x15, 0x15, 0x15, 0x0a); return;
        case 'X': set_rows(rows, 0x11, 0x11, 0x0a, 0x04, 0x0a, 0x11, 0x11); return;
        case 'Y': set_rows(rows, 0x11, 0x11, 0x0a, 0x04, 0x04, 0x04, 0x04); return;
        case 'Z': set_rows(rows, 0x1f, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1f); return;
        case '0': set_rows(rows, 0x0e, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0e); return;
        case '1': set_rows(rows, 0x04, 0x0c, 0x04, 0x04, 0x04, 0x04, 0x0e); return;
        case '2': set_rows(rows, 0x0e, 0x11, 0x01, 0x06, 0x08, 0x10, 0x1f); return;
        case '3': set_rows(rows, 0x1e, 0x01, 0x01, 0x0e, 0x01, 0x01, 0x1e); return;
        case '4': set_rows(rows, 0x02, 0x06, 0x0a, 0x12, 0x1f, 0x02, 0x02); return;
        case '5': set_rows(rows, 0x1f, 0x10, 0x10, 0x1e, 0x01, 0x01, 0x1e); return;
        case '6': set_rows(rows, 0x0e, 0x10, 0x10, 0x1e, 0x11, 0x11, 0x0e); return;
        case '7': set_rows(rows, 0x1f, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08); return;
        case '8': set_rows(rows, 0x0e, 0x11, 0x11, 0x0e, 0x11, 0x11, 0x0e); return;
        case '9': set_rows(rows, 0x0e, 0x11, 0x11, 0x0f, 0x01, 0x01, 0x0e); return;
        case ':': set_rows(rows, 0x00, 0x04, 0x00, 0x00, 0x00, 0x04, 0x00); return;
        case '-': set_rows(rows, 0x00, 0x00, 0x00, 0x1f, 0x00, 0x00, 0x00); return;
        case '.': set_rows(rows, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0c, 0x0c); return;
        case ',': set_rows(rows, 0x00, 0x00, 0x00, 0x00, 0x06, 0x04, 0x08); return;
        case '/': set_rows(rows, 0x01, 0x02, 0x04, 0x08, 0x10, 0x00, 0x00); return;
        case '(': set_rows(rows, 0x02, 0x04, 0x08, 0x08, 0x08, 0x04, 0x02); return;
        case ')': set_rows(rows, 0x08, 0x04, 0x02, 0x02, 0x02, 0x04, 0x08); return;
        case '+': set_rows(rows, 0x00, 0x04, 0x04, 0x1f, 0x04, 0x04, 0x00); return;
        case '_': set_rows(rows, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1f); return;
        case ' ': return;
        default: set_rows(rows, 0x1f, 0x01, 0x02, 0x04, 0x08, 0x00, 0x08); return;
    }
}

uint32_t compose_rgb(uint8_t r, uint8_t g, uint8_t b) {
    if (g_fb == 0) {
        return 0;
    }
    uint32_t red = (static_cast<uint32_t>(r) >> (8U - g_fb->red_mask_size)) << g_fb->red_mask_shift;
    uint32_t green = (static_cast<uint32_t>(g) >> (8U - g_fb->green_mask_size)) << g_fb->green_mask_shift;
    uint32_t blue = (static_cast<uint32_t>(b) >> (8U - g_fb->blue_mask_size)) << g_fb->blue_mask_shift;
    return red | green | blue;
}

void plot(int32_t x, int32_t y, uint32_t color) {
    if (g_fb == 0 || g_draw_base == 0 || g_draw_pitch == 0) {
        return;
    }
    if (x < 0 || y < 0 || static_cast<uint64_t>(x) >= g_fb->width || static_cast<uint64_t>(y) >= g_fb->height) {
        return;
    }
    uint32_t *row = reinterpret_cast<uint32_t *>(g_draw_base + static_cast<uint64_t>(y) * g_draw_pitch);
    row[x] = color;
}

void blit_backbuffer_to_frontbuffer(void) {
    if (!g_double_buffer_enabled || g_fb == 0 || g_fb_base == 0 || g_draw_base == 0) {
        return;
    }
    const uint64_t width = g_fb->width;
    const uint64_t height = g_fb->height;
    for (uint64_t y = 0; y < height; y++) {
        uint32_t *dst = reinterpret_cast<uint32_t *>(g_fb_base + y * g_fb->pitch);
        const uint32_t *src = reinterpret_cast<const uint32_t *>(g_draw_base + y * g_draw_pitch);
        for (uint64_t x = 0; x < width; x++) {
            dst[x] = src[x];
        }
    }
}

void fill_rect(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color) {
    if (w <= 0 || h <= 0) {
        return;
    }
    for (int32_t yy = 0; yy < h; yy++) {
        for (int32_t xx = 0; xx < w; xx++) {
            plot(x + xx, y + yy, color);
        }
    }
}

void draw_line(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t color) {
    int32_t dx = (x1 > x0) ? (x1 - x0) : (x0 - x1);
    int32_t sx = (x0 < x1) ? 1 : -1;
    int32_t dy = -((y1 > y0) ? (y1 - y0) : (y0 - y1));
    int32_t sy = (y0 < y1) ? 1 : -1;
    int32_t err = dx + dy;
    for (;;) {
        plot(x0, y0, color);
        if (x0 == x1 && y0 == y1) {
            break;
        }
        const int32_t e2 = err << 1;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

int32_t min3(int32_t a, int32_t b, int32_t c) {
    int32_t m = (a < b) ? a : b;
    return (m < c) ? m : c;
}

int32_t max3(int32_t a, int32_t b, int32_t c) {
    int32_t m = (a > b) ? a : b;
    return (m > c) ? m : c;
}

int64_t edge_function(const point2 &a, const point2 &b, const point2 &p) {
    return static_cast<int64_t>(p.x - a.x) * static_cast<int64_t>(b.y - a.y) -
           static_cast<int64_t>(p.y - a.y) * static_cast<int64_t>(b.x - a.x);
}

void fill_triangle(const point2 &a, const point2 &b, const point2 &c, uint32_t color) {
    int32_t min_x = min3(a.x, b.x, c.x);
    int32_t max_x = max3(a.x, b.x, c.x);
    int32_t min_y = min3(a.y, b.y, c.y);
    int32_t max_y = max3(a.y, b.y, c.y);

    if (min_x < 0) min_x = 0;
    if (min_y < 0) min_y = 0;
    if (g_fb != 0) {
        if (max_x >= static_cast<int32_t>(g_fb->width)) max_x = static_cast<int32_t>(g_fb->width) - 1;
        if (max_y >= static_cast<int32_t>(g_fb->height)) max_y = static_cast<int32_t>(g_fb->height) - 1;
    }

    for (int32_t y = min_y; y <= max_y; y++) {
        for (int32_t x = min_x; x <= max_x; x++) {
            point2 p = {x, y};
            const int64_t w0 = edge_function(b, c, p);
            const int64_t w1 = edge_function(c, a, p);
            const int64_t w2 = edge_function(a, b, p);
            if ((w0 >= 0 && w1 >= 0 && w2 >= 0) ||
                (w0 <= 0 && w1 <= 0 && w2 <= 0)) {
                plot(x, y, color);
            }
        }
    }
}

void u64_to_decimal(uint64_t value, char *out, uint64_t out_size) {
    if (out == 0 || out_size < 2) {
        return;
    }
    if (value == 0) {
        out[0] = '0';
        out[1] = '\0';
        return;
    }
    char temp[21];
    uint64_t index = 0;
    while (value != 0 && index < sizeof(temp)) {
        temp[index++] = static_cast<char>('0' + (value % 10));
        value /= 10;
    }
    uint64_t out_index = 0;
    while (index != 0 && out_index + 1 < out_size) {
        out[out_index++] = temp[--index];
    }
    out[out_index] = '\0';
}

void draw_text(int32_t x, int32_t y, const char *text, uint32_t fg, uint32_t bg) {
    if (text == 0) {
        return;
    }
    const uint64_t scale = g_text_scale;
    const uint64_t cell_w = kGlyphW * scale;
    const uint64_t cell_h = kGlyphH * scale;
    const uint32_t shadow = compose_rgb(24, 30, 46);
    int32_t cursor_x = x;
    for (uint64_t i = 0; text[i] != '\0'; i++) {
        const uint8_t c = static_cast<uint8_t>(text[i]);
        if (c == '\n') {
            cursor_x = x;
            y += static_cast<int32_t>(cell_h);
            continue;
        }
        fill_rect(cursor_x, y, static_cast<int32_t>(cell_w), static_cast<int32_t>(cell_h), bg);
        uint8_t rows[7];
        glyph_5x7(c, rows);
        for (uint64_t gy = 0; gy < 7; gy++) {
            for (uint64_t gx = 0; gx < 5; gx++) {
                const bool pixel_on = (rows[gy] & (1U << (4 - gx))) != 0;
                const bool bold_pixel =
                    (g_font_style == kFontBold && gx > 0 && (rows[gy] & (1U << (5 - gx))) != 0);
                if (!pixel_on && !bold_pixel) {
                    continue;
                }
                if (g_font_style == kFontShadow) {
                    const uint64_t shadow_offset = (scale > 1) ? 1 : 0;
                    for (uint64_t sy = 0; sy < scale; sy++) {
                        for (uint64_t sx = 0; sx < scale; sx++) {
                            plot(cursor_x + static_cast<int32_t>(gx * scale + sx + shadow_offset),
                                 y + static_cast<int32_t>(gy * scale + sy + shadow_offset),
                                 shadow);
                        }
                    }
                }
                for (uint64_t sy = 0; sy < scale; sy++) {
                    for (uint64_t sx = 0; sx < scale; sx++) {
                        plot(cursor_x + static_cast<int32_t>(gx * scale + sx),
                             y + static_cast<int32_t>(gy * scale + sy),
                             fg);
                    }
                }
            }
        }
        cursor_x += static_cast<int32_t>(cell_w);
    }
}

int32_t tri_wave_q15(uint64_t phase) {
    const uint64_t p = phase & 0xffULL;
    if (p < 64) return static_cast<int32_t>(p) * 512;
    if (p < 128) return static_cast<int32_t>(128 - p) * 512;
    if (p < 192) return -static_cast<int32_t>(p - 128) * 512;
    return -static_cast<int32_t>(256 - p) * 512;
}

uint64_t phase_base_for_tick(uint64_t tick) {
    if (g_last_phase_tick == 0) {
        g_last_phase_tick = tick;
        return (g_rotation_phase_q16 >> 16) & 0xffULL;
    }
    const uint64_t delta = tick - g_last_phase_tick;
    g_last_phase_tick = tick;
    const uint64_t advance_q16 = delta * g_spin_step * kPhaseAdvancePerTickQ16;
    g_rotation_phase_q16 += advance_q16;
    return (g_rotation_phase_q16 >> 16) & 0xffULL;
}

void rotate_project_vertex(const vec3 &in,
                           int32_t sin_y,
                           int32_t cos_y,
                           int32_t sin_x,
                           int32_t cos_x,
                           int32_t focal,
                           int32_t center_x,
                           int32_t center_y,
                           point2 *out_point,
                           int32_t *out_depth) {
    int32_t x = in.x;
    int32_t y = in.y;
    int32_t z = in.z;

    const int32_t xr = static_cast<int32_t>((static_cast<int64_t>(x) * cos_y - static_cast<int64_t>(z) * sin_y) >> 15);
    const int32_t zr = static_cast<int32_t>((static_cast<int64_t>(x) * sin_y + static_cast<int64_t>(z) * cos_y) >> 15);

    const int32_t yr = static_cast<int32_t>((static_cast<int64_t>(y) * cos_x - static_cast<int64_t>(zr) * sin_x) >> 15);
    const int32_t zz = static_cast<int32_t>((static_cast<int64_t>(y) * sin_x + static_cast<int64_t>(zr) * cos_x) >> 15) + 420;

    const int32_t depth = (zz < 32) ? 32 : zz;
    out_point->x = center_x + static_cast<int32_t>((static_cast<int64_t>(xr) * focal) / depth);
    out_point->y = center_y - static_cast<int32_t>((static_cast<int64_t>(yr) * focal) / depth);
    *out_depth = depth;
}

void draw_panel(uint64_t tick) {
    if (g_fb == 0 || g_draw_base == 0) {
        return;
    }
    const int32_t width = static_cast<int32_t>(g_fb->width);
    const int32_t height = static_cast<int32_t>(g_fb->height);
    if (width < 320 || height < 200) {
        return;
    }

    const int32_t panel_x = width / 3;
    const int32_t panel_y = 60;
    const int32_t panel_w = width - panel_x - 24;
    const int32_t panel_h = height - panel_y - 24;
    if (panel_w < 120 || panel_h < 120) {
        return;
    }
    fill_rect(0, 0, width, height, compose_rgb(8, 12, 22));

    const bool easter_mode = g_cube_easter_mode;
    const bool easter_pulse = easter_mode && (tick < g_cube_easter_pulse_until_tick);
    const uint8_t pulse = static_cast<uint8_t>((tri_wave_q15((tick * 7) & 0xffULL) + 32768) >> 8);
    const uint32_t bg0 = easter_mode
                             ? (easter_pulse ? compose_rgb(20 + (pulse / 16), 6, 30 + (pulse / 8))
                                             : compose_rgb(18, 6, 30))
                             : compose_rgb(8, 12, 20);
    const uint32_t bg1 = easter_mode
                             ? (easter_pulse ? compose_rgb(44 + (pulse / 10), 14, 68 + (pulse / 6))
                                             : compose_rgb(44, 14, 68))
                             : compose_rgb(18, 28, 54);
    const uint32_t fg = easter_mode ? compose_rgb(255, 230, 255) : compose_rgb(232, 242, 255);
    const uint32_t wire = easter_mode
                              ? (easter_pulse ? compose_rgb(255, pulse, static_cast<uint8_t>(255 - pulse))
                                              : compose_rgb(255, 168, 255))
                              : compose_rgb(255, 255, 255);

    for (int32_t y = 0; y < panel_h; y++) {
        const uint8_t blend = static_cast<uint8_t>((y * 255) / panel_h);
        const uint8_t r = static_cast<uint8_t>((((bg0 >> g_fb->red_mask_shift) & ((1U << g_fb->red_mask_size) - 1U)) * (255 - blend) +
                                                 ((bg1 >> g_fb->red_mask_shift) & ((1U << g_fb->red_mask_size) - 1U)) * blend) /
                                                255);
        const uint8_t g = static_cast<uint8_t>((((bg0 >> g_fb->green_mask_shift) & ((1U << g_fb->green_mask_size) - 1U)) * (255 - blend) +
                                                 ((bg1 >> g_fb->green_mask_shift) & ((1U << g_fb->green_mask_size) - 1U)) * blend) /
                                                255);
        const uint8_t b = static_cast<uint8_t>((((bg0 >> g_fb->blue_mask_shift) & ((1U << g_fb->blue_mask_size) - 1U)) * (255 - blend) +
                                                 ((bg1 >> g_fb->blue_mask_shift) & ((1U << g_fb->blue_mask_size) - 1U)) * blend) /
                                                255);
        const uint32_t row_color = compose_rgb(r << (8 - g_fb->red_mask_size),
                                               g << (8 - g_fb->green_mask_size),
                                               b << (8 - g_fb->blue_mask_size));
        fill_rect(panel_x, panel_y + y, panel_w, 1, row_color);
    }

    draw_line(panel_x, panel_y, panel_x + panel_w - 1, panel_y, compose_rgb(64, 110, 220));
    draw_line(panel_x, panel_y + panel_h - 1, panel_x + panel_w - 1, panel_y + panel_h - 1, compose_rgb(64, 110, 220));
    draw_line(panel_x, panel_y, panel_x, panel_y + panel_h - 1, compose_rgb(64, 110, 220));
    draw_line(panel_x + panel_w - 1, panel_y, panel_x + panel_w - 1, panel_y + panel_h - 1, compose_rgb(64, 110, 220));

    const uint64_t active_style = easter_mode ? kStyleBoth : g_render_style;
    if (g_demo_enabled) {
        const uint64_t phase_base = phase_base_for_tick(tick);
        const int32_t sin_y = tri_wave_q15(phase_base);
        const int32_t cos_y = tri_wave_q15(phase_base + 64);
        const int32_t sin_x = tri_wave_q15((phase_base * 3) / 2 + 19);
        const int32_t cos_x = tri_wave_q15((phase_base * 3) / 2 + 83);

        point2 projected[8];
        int32_t depth[8];
        const int32_t cx = panel_x + panel_w / 2;
        const int32_t cy = panel_y + panel_h / 2 + 10;
        const int32_t focal = panel_h / 2;
        for (uint64_t i = 0; i < 8; i++) {
            rotate_project_vertex(kCubeVertices[i], sin_y, cos_y, sin_x, cos_x, focal, cx, cy, &projected[i], &depth[i]);
        }

        sort_face order[12];
        for (uint64_t i = 0; i < 12; i++) {
            const face &f = kCubeFaces[i];
            order[i].face_index = static_cast<uint8_t>(i);
            order[i].depth = (depth[f.i0] + depth[f.i1] + depth[f.i2]) / 3;
        }
        for (uint64_t i = 1; i < 12; i++) {
            sort_face key = order[i];
            int64_t j = static_cast<int64_t>(i) - 1;
            while (j >= 0 && order[j].depth > key.depth) {
                order[j + 1] = order[j];
                j--;
            }
            order[j + 1] = key;
        }

        const bool draw_solid = (active_style == kStyleSolid || active_style == kStyleBoth);
        const bool draw_wire = (active_style == kStyleWire || active_style == kStyleBoth);
        for (int64_t i = 11; i >= 0; i--) {
            const face &f = kCubeFaces[order[i].face_index];
            const point2 &a = projected[f.i0];
            const point2 &b = projected[f.i1];
            const point2 &c = projected[f.i2];
            const int64_t cross = static_cast<int64_t>(b.x - a.x) * static_cast<int64_t>(c.y - a.y) -
                                  static_cast<int64_t>(b.y - a.y) * static_cast<int64_t>(c.x - a.x);
            if (cross <= 0) {
                continue;
            }
            if (draw_solid) {
                fill_triangle(a, b, c, compose_rgb(f.color_r, f.color_g, f.color_b));
            }
            if (draw_wire) {
                draw_line(a.x, a.y, b.x, b.y, wire);
                draw_line(b.x, b.y, c.x, c.y, wire);
                draw_line(c.x, c.y, a.x, a.y, wire);
            }
        }
    } else {
        draw_text(panel_x + 24, panel_y + 48, "3D DEMO PAUSED", compose_rgb(176, 206, 248), compose_rgb(8, 12, 22));
        draw_text(panel_x + 24, panel_y + 48 + static_cast<int32_t>(kGlyphH * g_text_scale),
                  "INPUT/COMMAND SURFACES ACTIVE", compose_rgb(136, 178, 236), compose_rgb(8, 12, 22));
    }

    const int32_t hud_x = panel_x + 12;
    const int32_t hud_y = panel_y + 12;
    const int32_t hud_w = panel_w / 2;
    const int32_t line_h = static_cast<int32_t>(kGlyphH * g_text_scale);
    const int32_t hud_h = 10 + (line_h * (easter_mode ? 5 : 4));
    fill_rect(hud_x, hud_y, hud_w, hud_h, compose_rgb(4, 8, 18));
    draw_text(hud_x + 6, hud_y + 4, g_demo_enabled ? "FORTIFY VIDEO 3D DEMO" : "FORTIFY VIDEO DEMO OFF", fg, compose_rgb(4, 8, 18));
    if (active_style == kStyleSolid) {
        draw_text(hud_x + 6, hud_y + 4 + line_h, "STYLE SOLID", fg, compose_rgb(4, 8, 18));
    } else if (active_style == kStyleWire) {
        draw_text(hud_x + 6, hud_y + 4 + line_h, "STYLE WIRE", fg, compose_rgb(4, 8, 18));
    } else {
        draw_text(hud_x + 6, hud_y + 4 + line_h, "STYLE BOTH", fg, compose_rgb(4, 8, 18));
    }
    if (easter_mode) {
        draw_text(hud_x + 6, hud_y + 4 + line_h * 4, "EASTER MODE", compose_rgb(255, 190, 255), compose_rgb(4, 8, 18));
        if (easter_pulse) {
            draw_text(hud_x + 160, hud_y + 4 + line_h * 4, "PULSE", compose_rgb(255, 220, 160), compose_rgb(4, 8, 18));
        }
    }

    if (g_font_style == kFontMono) {
        draw_text(hud_x + 6, hud_y + 4 + line_h * 2, "TEXT MONO SCALE", fg, compose_rgb(4, 8, 18));
    } else if (g_font_style == kFontBold) {
        draw_text(hud_x + 6, hud_y + 4 + line_h * 2, "TEXT BOLD SCALE", fg, compose_rgb(4, 8, 18));
    } else {
        draw_text(hud_x + 6, hud_y + 4 + line_h * 2, "TEXT SHADOW SCALE", fg, compose_rgb(4, 8, 18));
    }

    char fps_value[24];
    u64_to_decimal(g_fps_display, fps_value, sizeof(fps_value));
    draw_text(hud_x + 6, hud_y + 4 + line_h * 3, "FPS", fg, compose_rgb(4, 8, 18));
    const int32_t fps_x = hud_x + 6 + static_cast<int32_t>(kGlyphW * g_text_scale * 4);
    draw_text(fps_x, hud_y + 4 + line_h * 3, fps_value, fg, compose_rgb(4, 8, 18));

    const int32_t scale_x = hud_x + 6 + static_cast<int32_t>(kGlyphW * g_text_scale * 16);
    if (g_text_scale == 1) {
        draw_text(scale_x, hud_y + 4 + line_h * 2, "1", fg, compose_rgb(4, 8, 18));
    } else if (g_text_scale == 2) {
        draw_text(scale_x, hud_y + 4 + line_h * 2, "2", fg, compose_rgb(4, 8, 18));
    } else if (g_text_scale == 3) {
        draw_text(scale_x, hud_y + 4 + line_h * 2, "3", fg, compose_rgb(4, 8, 18));
    } else {
        draw_text(scale_x, hud_y + 4 + line_h * 2, "4", fg, compose_rgb(4, 8, 18));
    }
    draw_compositor_surface_cards();
    blit_backbuffer_to_frontbuffer();
}
} // namespace

bool video_init(void) {
    limine_framebuffer_response *fb = boot_framebuffer_response();
    if (fb == 0 || fb->framebuffer_count == 0 || fb->framebuffers == 0 || fb->framebuffers[0] == 0) {
        log_error("video: framebuffer unavailable.");
        return false;
    }
    g_fb = fb->framebuffers[0];
    if (g_fb->address == 0 || g_fb->memory_model != LIMINE_FRAMEBUFFER_RGB || g_fb->bpp != 32) {
        log_error("video: unsupported framebuffer mode.");
        return false;
    }
    if (g_fb->width == 0 || g_fb->height == 0 || g_fb->width > 8192 || g_fb->height > 4320) {
        log_error("video: framebuffer dimensions invalid.");
        return false;
    }
    if (g_fb->pitch < g_fb->width * 4) {
        log_error("video: framebuffer pitch invalid.");
        return false;
    }
    uintptr_t addr = reinterpret_cast<uintptr_t>(g_fb->address);
    limine_hhdm_response *hhdm = boot_hhdm_response();
    if (addr < (1ULL << 32) && hhdm != 0) {
        addr += static_cast<uintptr_t>(hhdm->offset);
    }
    g_fb_base = reinterpret_cast<uint8_t *>(addr);
    g_video_ready = (g_fb_base != 0);
    if (!g_video_ready) {
        log_error("video: framebuffer mapping unavailable.");
        return false;
    }
    g_double_buffer_enabled = (g_fb->width <= kBackbufferMaxWidth && g_fb->height <= kBackbufferMaxHeight);
    if (g_double_buffer_enabled) {
        g_draw_base = reinterpret_cast<uint8_t *>(g_backbuffer);
        g_draw_pitch = g_fb->width * 4;
        log_info("video: double buffering enabled.");
    } else {
        g_draw_base = g_fb_base;
        g_draw_pitch = g_fb->pitch;
        log_warn("video: double buffering disabled (framebuffer exceeds internal backbuffer limit).");
    }
    log_info("video: framebuffer attached.");
    return true;
}

bool renderer_init(void) {
    log_info("renderer: software 3D renderer ready.");
    return true;
}

bool compositor_init(void) {
    g_compositor_ready = true;
    g_next_surface_id = 0;
    g_input_surface_id = 0;
    g_command_center_surface_id = 0;
    g_focused_surface_id = 0;
    g_cube_easter_mode = false;
    g_cube_easter_pulse_until_tick = 0;
    g_submitted_command_ready = false;
    g_submitted_command[0] = '\0';
    input_surface_set_ready();
    copy_ascii_text("boot", g_command_center_last_command, sizeof(g_command_center_last_command), "boot");
    copy_ascii_text("idle", g_command_center_status, sizeof(g_command_center_status), "idle");
    for (uint64_t i = 0; i < kMaxSurfaces; i++) {
        clear_surface_entry(&g_surfaces[i]);
    }
    uint64_t desktop_surface_id = 0;
    if (!compositor_surface_create(surface_type::desktop, "Desktop Surface", 0, &desktop_surface_id)) {
        log_error("compositor: failed to create desktop surface.");
        return false;
    }
    uint64_t workspace_surface_id = 0;
    if (!compositor_surface_create(surface_type::workspace, "Workspace Surface", 0, &workspace_surface_id)) {
        log_warn("compositor: failed to create default workspace surface.");
    }
    uint64_t input_surface_id = 0;
    if (!compositor_surface_create(surface_type::tool, "Input Surface", 0, &input_surface_id)) {
        log_warn("compositor: failed to create default input surface.");
    } else {
        g_input_surface_id = input_surface_id;
        g_focused_surface_id = input_surface_id;
    }
    uint64_t command_center_surface_id = 0;
    if (!compositor_surface_create(surface_type::dashboard, "Command Center", 0, &command_center_surface_id)) {
        log_warn("compositor: failed to create default command center surface.");
    } else {
        g_command_center_surface_id = command_center_surface_id;
    }
    log_info("compositor: composition pipeline ready.");
    log_info("compositor: desktop surface initialized.");
    return true;
}

void video_render_tick(uint64_t tick) {
    if (!g_video_ready) {
        return;
    }
    if (g_last_render_tick != 0 && (tick - g_last_render_tick) < kRenderTickStride) {
        return;
    }
    if (g_demo_enabled) {
        if (g_fps_window_start_tick == 0) {
            g_fps_window_start_tick = tick;
        }
        g_fps_frames_in_window++;
        const uint64_t fps_elapsed = tick - g_fps_window_start_tick;
        if (fps_elapsed >= 100) {
            g_fps_display = (g_fps_frames_in_window * 100 + (fps_elapsed / 2)) / fps_elapsed;
            g_fps_window_start_tick = tick;
            g_fps_frames_in_window = 0;
        }
    } else {
        g_fps_display = 0;
    }
    g_last_render_tick = tick;
    draw_panel(tick);
}

bool video_set_demo_enabled(bool enabled) {
    g_demo_enabled = enabled;
    if (!enabled) {
        g_last_render_tick = 0;
        g_last_phase_tick = 0;
        g_fps_window_start_tick = 0;
        g_fps_frames_in_window = 0;
        g_fps_display = 0;
    }
    return true;
}

bool video_demo_enabled(void) {
    return g_demo_enabled;
}

bool video_set_render_style(uint64_t style) {
    if (style > kStyleBoth) {
        return false;
    }
    g_render_style = style;
    return true;
}

uint64_t video_render_style(void) {
    return g_render_style;
}

bool video_set_spin_step(uint64_t step) {
    if (step == 0 || step > 32) {
        return false;
    }
    g_spin_step = step;
    return true;
}

uint64_t video_spin_step(void) {
    return g_spin_step;
}

bool video_set_font_style(uint64_t style) {
    if (style > kFontShadow) {
        return false;
    }
    g_font_style = style;
    return true;
}

uint64_t video_font_style(void) {
    return g_font_style;
}

bool video_set_text_scale(uint64_t scale) {
    if (scale == 0 || scale > 4) {
        return false;
    }
    g_text_scale = scale;
    return true;
}

uint64_t video_text_scale(void) {
    return g_text_scale;
}

bool video_toggle_cube_easter_mode(bool *out_enabled) {
    g_cube_easter_mode = !g_cube_easter_mode;
    if (out_enabled != 0) {
        *out_enabled = g_cube_easter_mode;
    }
    return true;
}

bool video_cube_easter_mode(void) {
    return g_cube_easter_mode;
}

bool video_trigger_cube_easter_pulse(uint64_t duration_ticks) {
    if (duration_ticks == 0) {
        return false;
    }
    const uint64_t now = timer_ticks();
    g_cube_easter_pulse_until_tick = now + duration_ticks;
    return true;
}

bool video_set_input_surface_draft(const char *draft_text) {
    return copy_ascii_text(draft_text, g_input_surface_draft, sizeof(g_input_surface_draft), "ready");
}

bool video_set_command_center_last_command(const char *command_text) {
    return copy_ascii_text(command_text, g_command_center_last_command, sizeof(g_command_center_last_command), "none");
}

bool video_set_command_center_status(const char *status_text) {
    return copy_ascii_text(status_text, g_command_center_status, sizeof(g_command_center_status), "idle");
}

bool compositor_route_event(surface_event_type type, char value) {
    if (!g_compositor_ready) {
        return false;
    }
    uint64_t target_surface_id = g_focused_surface_id;
    if (target_surface_id == 0) {
        target_surface_id = g_input_surface_id;
    }
    compositor_surface_entry *target = find_surface_by_id(target_surface_id);
    if (target == 0) {
        return false;
    }
    g_focused_surface_id = target->info.id;

    const bool input_surface_target =
        (target->info.id == g_input_surface_id) || surface_title_equals(target->info.title, "Input Surface");
    if (!input_surface_target) {
        return true;
    }

    if (type == surface_event_type::key_char) {
        input_surface_append_char(value);
        copy_ascii_text("typing", g_command_center_status, sizeof(g_command_center_status), "typing");
        return true;
    }
    if (type == surface_event_type::key_backspace) {
        input_surface_backspace();
        if (surface_title_equals(g_input_surface_draft, "ready")) {
            copy_ascii_text("ready", g_command_center_status, sizeof(g_command_center_status), "ready");
        } else {
            copy_ascii_text("typing", g_command_center_status, sizeof(g_command_center_status), "typing");
        }
        return true;
    }
    if (type == surface_event_type::key_submit) {
        if (input_surface_submit()) {
            copy_ascii_text(g_submitted_command, g_command_center_last_command, sizeof(g_command_center_last_command), "none");
            copy_ascii_text("dispatch", g_command_center_status, sizeof(g_command_center_status), "dispatch");
        } else {
            copy_ascii_text("ready", g_command_center_status, sizeof(g_command_center_status), "ready");
        }
        return true;
    }
    return false;
}

bool compositor_take_submitted_command(char *out_command, uint64_t out_size) {
    if (out_command == 0 || out_size < 2 || !g_submitted_command_ready) {
        return false;
    }
    if (!copy_ascii_text(g_submitted_command, out_command, out_size, 0)) {
        return false;
    }
    g_submitted_command_ready = false;
    g_submitted_command[0] = '\0';
    input_surface_set_ready();
    return true;
}

bool compositor_surface_create(surface_type type,
                               const char *title,
                               uint64_t capabilities,
                               uint64_t *out_surface_id) {
    if (!g_compositor_ready || out_surface_id == 0) {
        return false;
    }
    uint64_t free_index = kMaxSurfaces;
    uint64_t used_count = 0;
    for (uint64_t i = 0; i < kMaxSurfaces; i++) {
        if (g_surfaces[i].used) {
            used_count++;
        } else if (free_index == kMaxSurfaces) {
            free_index = i;
        }
    }
    if (free_index == kMaxSurfaces) {
        return false;
    }
    surface_info info = {};
    assign_surface_defaults(type, &info, used_count);
    if (!copy_surface_title(title, info.title, sizeof(info.title))) {
        return false;
    }
    info.capabilities = capabilities;
    info.id = __atomic_add_fetch(&g_next_surface_id, 1, __ATOMIC_RELAXED);
    g_surfaces[free_index].used = true;
    g_surfaces[free_index].info = info;
    if (surface_title_equals(info.title, "Input Surface")) {
        g_input_surface_id = info.id;
        if (g_focused_surface_id == 0) {
            g_focused_surface_id = info.id;
        }
    } else if (surface_title_equals(info.title, "Command Center")) {
        g_command_center_surface_id = info.id;
    }
    *out_surface_id = info.id;
    return true;
}

uint64_t compositor_surface_count(void) {
    if (!g_compositor_ready) {
        return 0;
    }
    uint64_t count = 0;
    for (uint64_t i = 0; i < kMaxSurfaces; i++) {
        if (g_surfaces[i].used) {
            count++;
        }
    }
    return count;
}

bool compositor_surface_get(uint64_t index, surface_info *out_info) {
    if (!g_compositor_ready || out_info == 0) {
        return false;
    }
    uint64_t seen = 0;
    for (uint64_t i = 0; i < kMaxSurfaces; i++) {
        if (!g_surfaces[i].used) {
            continue;
        }
        if (seen == index) {
            *out_info = g_surfaces[i].info;
            return true;
        }
        seen++;
    }
    return false;
}
