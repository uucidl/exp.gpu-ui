extern "C" {
#include "ui_api.h"
}

#define NamedTrue(X) true

#include "ui_inline_shaders.hpp"

#include "memory.hpp"
#include "memory.ipp"

extern "C" {
#define BGFX_SHARED_LIB_USE 1
#include "bgfx/c99/bgfx.h"
#undef BGFX_SHARED_LIB_USE
}

#include "third-party-deps/bgfx/3rdparty/stb/stb_truetype.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>

// Some scratch memory to make the code simpler
static char temp_arena_buffer[16 * 1024 * 1024];
static MemoryArena temp_arena = {temp_arena_buffer, sizeof temp_arena_buffer};

static void
draw_box_with_border(int x, int y, int width, int height, bool alt_color);

// NOTE(nil): TODO(nil):
// Use of static below for function local state is not safe when reloading the
// DLL
// if the multithreaded renderer is still doing work with those.

// Axis-Aligned Box
struct AAB2 {
    int x_min;
    int x_max;
    int y_min;
    int y_max;
};

// construct AAB2 from position + displacement
static AAB2 aab2_x_d(int x, int y, int width, int height);
// whether a point is contained in an AAB2
static bool aab2_contains(AAB2 const box, int x, int y);

struct Ui_String {
    char *utf8_first;
    size_t utf8_n;
};

struct Ui_CharRange;

struct Ui_FontHandle {
    Ui_CharRange *chars_data;
    bgfx_texture_handle chars_texture;
    Ui_Int2 chars_texture_size;
};

static Ui_FontHandle fallback_font_en_US();
static Ui_Int2 get_string_extent(Ui_String const &string,
                                 Ui_FontHandle const &font);
static void draw_string_at(int x,
                           int y,
                           AAB2 clipbox,
                           Ui_String const &string,
                           Ui_FontHandle const &font);

// text shown to users in the interface (has to be internationalized, is styled,
// etc..)
struct Ui_Label {
    Ui_String string;
    Ui_FontHandle font;
};

static Ui_Label label_makef_temp(char const *format, ...);
static Ui_Int2 label_extent(Ui_Label const &x);
static Ui_String label_string(Ui_Label const &x) { return x.string; }
static Ui_FontHandle label_font(Ui_Label const &x) { return x.font; }

static void
draw_centered_label(int x, int y, int width, int height, Ui_Label const &label);

static void ui_display_invalidate(Ui* ui_)
{
    auto &ui = *ui_;
    ui.outdated_frames_n = std::max(ui.outdated_frames_n, 1);
}

// returns true when activated
static bool button_simple(Ui *ui_,
                          int flags,
                          int x,
                          int y,
                          int width,
                          int height,
                          Ui_Label const &label)
{
    auto &ui = *ui_;
    bool display = 0 != (flags & Ui_UpdateFlags_Display);
    bool input = 0 != (flags & Ui_UpdateFlags_ProcessInputs);
    bool is_on =
        ui.inputs.mouse.left_button.down_p &&
        aab2_contains(aab2_x_d(x, y, width, height),
                      ui.inputs.mouse.position.x,
                      ui.inputs.mouse.position.y);
    bool transitioned_p = ui.inputs.mouse.left_button.pressed_p ||
        ui.inputs.mouse.left_button.released_p;
    if (input && transitioned_p)
        ui_display_invalidate(&ui);
    if (display) {
        draw_box_with_border(x, y, width, height, is_on);
        draw_centered_label(x, y, width, height, label);
    }
    return is_on;
}

// returns the new toggle value
static bool toggle_simple(Ui *ui_,
                          int flags,
                          int x,
                          int y,
                          int width,
                          int height,
                          Ui_Label const &label,
                          bool is_on)
{
    auto &ui = *ui_;
    bool display = 0 != (flags & Ui_UpdateFlags_Display);
    bool input = 0 != (flags & Ui_UpdateFlags_ProcessInputs);
    if (input && ui.inputs.mouse.left_button.released_p &&
        aab2_contains(aab2_x_d(x, y, width, height), ui.inputs.mouse.position.x,
                      ui.inputs.mouse.position.y)) {
        is_on = !is_on;
        ui_display_invalidate(&ui);
    }
    if (display) {
        draw_box_with_border(x, y, width, height, is_on);
        draw_centered_label(x, y, width, height, label);
    }
    return is_on;
}

static void advance(Ui_Int2* pos_, Ui_Int2 delta)
{
    auto &pos = *pos_;
    pos.x += delta.x;
    pos.y += delta.y;
}

static void catchupx(Ui_Int2* pos_, Ui_Int2 delta, Ui_Int2 target)
{
    auto &pos = *pos_;
    while (pos.x < target.x) {
        advance(&pos, { delta.x, 0 });
    }
}

static void catchupy(Ui_Int2* pos_, Ui_Int2 delta, Ui_Int2 target)
{
    auto &pos = *pos_;
    while (pos.y < target.y) {
        advance(&pos, { 0, delta.y });
    }
}

static void ui_update(Ui *ui_, int flags)
{
    auto &ui = *ui_;
    auto const &frame_i = ui.inputs.frame_i;
    auto const &frame_ms = ui.inputs.frame_ms;

    temp_arena.free_i = 0; // reset arena

    if (flags & Ui_UpdateFlags_Display)
        bgfx_set_view_clear(0, BGFX_CLEAR_COLOR, 0x96969600, 1.0f, 0);

    static int counter = 0;
    static int counter_frame_i = frame_i;

    if (frame_i != counter_frame_i) {
        counter = 0;
        counter_frame_i = frame_i;
    }

    struct Blinker
    {
        double period_ms;
        bool is_on;
        int toggle;
        double next_ms;
        double error_ms; // measure firing error
    };

    static Blinker blinkers[2] = {
        { 300.0, 0, 0, frame_ms, 0.0 },
        { 600.0, 0, 0, frame_ms - 150.0, 0.0 },
    };

    for (auto& blinker : blinkers) {
        if (blinker.is_on && frame_ms >= blinker.next_ms) {
            blinker.error_ms = fabs(blinker.next_ms - frame_ms);
            blinker.toggle ^= 1;
            while (frame_ms >= blinker.next_ms) {
                blinker.next_ms += blinker.period_ms;
            }
            ui_display_invalidate(&ui);
        }
    }

    // try changing the draw code below to see the reloading mechanism work:
    bool display = 0 != (flags & Ui_UpdateFlags_Display);
    Ui_Int2 row_cursor = { 40, 40 };
    Ui_Int2 row_delta { 0, 24 }; // rhythm for the rows

    auto* cursor = &row_cursor;
    if (display)
        draw_centered_label(
            cursor->x, cursor->y, ui.display.size_px.x, 16,
            label_makef_temp("Pressing Q will quit the application."));
    advance(cursor, row_delta);
    if (display)
        draw_centered_label(
            cursor->x, cursor->y, ui.display.size_px.x, 16,
            label_makef_temp("Pressing D will show debug info."));
    advance(cursor, row_delta);
    advance(cursor, row_delta);

    Ui_Int2 col_delta{ 96, 0 }; // we use a grid layout using 96 width columns
    {
        auto tempcursor = *cursor;
        cursor = &tempcursor;

        int button_i = 0;
        for (int n = 8; n--;) {
            bool pressed = button_simple(&ui, flags, cursor->x, cursor->y, 80, 80,
                                         label_makef_temp("%d", button_i++));
            advance(cursor, col_delta);
        }
        if (display)
            draw_centered_label(cursor->x, cursor->y, 200, 80,
                                label_makef_temp("buttons (no-op)"));
        advance(cursor, { 0, 80 });
        cursor = &row_cursor;
        catchupy(cursor, row_delta, tempcursor);
    }

    /* allow toggling animation on/off */ {
        auto tempcursor = *cursor;
        cursor = &tempcursor;

        int toggle_i = 0;
        for (auto& blinker : blinkers) {
            auto &toggle = blinker.is_on;
            toggle = toggle_simple(&ui, flags, cursor->x, cursor->y, 80, 80,
                                   label_makef_temp("%d", toggle_i++), toggle);
            advance(cursor, col_delta);
        }
        if (display)
            draw_centered_label(cursor->x, cursor->y, 200, 80,
                                label_makef_temp("toggle animation"));
        advance(cursor, { 0, 80 });
        cursor = &row_cursor;
        catchupy(cursor, row_delta, tempcursor);
    }

    {
        auto tempcursor = *cursor;
        cursor = &tempcursor;
        if (display)
            draw_centered_label(cursor->x, cursor->y, 80, 20,
                                label_makef_temp("plonk1"));
        advance(cursor, col_delta);
        if (display)
            draw_centered_label(cursor->x, cursor->y, 80, 20,
                                label_makef_temp("plonk2"));
        advance(cursor, col_delta);
        advance(cursor, { 0, 20 });
        cursor = &row_cursor;
        catchupy(cursor, row_delta, tempcursor);
    }

    /* show blink animation */ {
        auto tempcursor = *cursor;
        cursor = &tempcursor;

        for (auto const& blinker : blinkers) {
            bool toggle = blinker.toggle != 0;
            draw_box_with_border(cursor->x, cursor->y, 80, 80, toggle);
            advance(cursor, col_delta);
        }
        if (display)
            draw_centered_label(cursor->x, cursor->y, 200, 80,
                                label_makef_temp("animation state"));
        advance(cursor, { 0, 80 });
        cursor = &row_cursor;
        catchupy(cursor, row_delta, tempcursor);
    }

    static bool debug_text_shown;
    debug_text_shown =
        toggle_simple(&ui, flags, cursor->x, cursor->y, 140, 40,
                      label_makef_temp("debug text"), debug_text_shown);
    advance(cursor, { 0, 40 });
    advance(cursor, row_delta);

    if (display && debug_text_shown) {
        int row = cursor->y / 14;
        bgfx_dbg_text_printf(0, row++, 0x0f,
                             "Frame: %f (ms) %d (count) "
                             "%d (updates w/o invalidation)",
                             frame_ms, frame_i, counter);
        bgfx_dbg_text_printf(0, row++, 0x0f, "Display size: %d x %d%s",
                             ui.display.size_px.x, ui.display.size_px.y,
                             ui.display.resized_p ? " resizing" : "");
        bgfx_dbg_text_printf(0, row++, 0x0f, "Mouse at %d %d",
                             ui.inputs.mouse.position.x,
                             ui.inputs.mouse.position.y);
        bgfx_dbg_text_printf(0, row++, 0x0f, "Left button: %s",
                             ui.inputs.mouse.left_button.down_p?
                             "down":"up");
        {
            int blinker_i = 0;
            for (auto const& blinker : blinkers) {
                bgfx_dbg_text_printf(4, row++,
                                     blinker.toggle ? 0x1f : 0x0f,
                                     "Plonk%d %f (remaining ms) %f (firing error ms)",
                                     blinker_i,
                                     blinker.next_ms - frame_ms,
                                     blinker.error_ms);
                ++blinker_i;
            }
        }
    }

    // Output
    for (auto const& blinker : blinkers) {
        if (blinker.is_on) {
            ui.validity_ms = std::min(
                ui.validity_ms, blinker.next_ms - frame_ms);
        }
    }

    // Internal state management
    ++counter;
}

GET_VTABLE_0_FN(ui_get_vtable_0) { return {ui_update}; }

// NOTE(nicolas) Q. How deep should UI concepts penetrate the graphics layer?
// I.e. should shaders talk about mouse/active/hot items etc..

// # Some implementation details:

static bool aab2_contains(AAB2 const box, int x, int y)
{
    if (x < box.x_min)
        return false;
    if (x > box.x_max)
        return false;
    if (y < box.y_min)
        return false;
    if (y > box.y_max)
        return false;
    return true;
}

static AAB2 aab2_x_d(int x, int y, int width, int height)
{
    AAB2 result;
    result.x_min = x;
    result.y_min = y;
    result.x_max = x + width;
    result.y_max = y + height;
    return result;
}

struct Ui_CharRange {
    uint16_t codepoint_first;
    uint16_t codepoint_last;
    stbtt_bakedchar *chars_first;
};

static Ui_CharRange fallback_font_en_US_data;

static Ui_FontHandle fallback_font_en_US_handle;

static Ui_FontHandle fallback_font_en_US()
{
    if (!fallback_font_en_US_handle.chars_data) {
        char const *filename = "c:/windows/fonts/calibri.ttf";
        Memory ttf_data = alloc(1 << 20, &temp_arena);
        fread(ttf_data.bytes_first, 1, ttf_data.bytes_n, fopen(filename, "rb"));

        Ui_Int2 size{512, 512};
        auto bitmap_memory = bgfx_alloc(size.x * size.y);

        auto ascii_first = 32;
        auto ascii_last = 126;
        auto &font = fallback_font_en_US_data;
        font.codepoint_first = ascii_first;
        font.codepoint_last = ascii_last;
        alloc_many_and_assign(&font.chars_first, font.codepoint_last);

        float font_height_px = 24.0;
        stbtt_BakeFontBitmap((unsigned char *)ttf_data.bytes_first, 0,
                             font_height_px, bitmap_memory->data, size.x,
                             size.y, ascii_first, ascii_last,
                             font.chars_first); // no guarantee this fits!

        auto texture_handle = bgfx_create_texture_2d(
            size.x, size.y, !NamedTrue(hasMips), /*num layers*/ 1,
            BGFX_TEXTURE_FORMAT_A8, BGFX_TEXTURE_NONE, bitmap_memory);

        fallback_font_en_US_handle.chars_data = &fallback_font_en_US_data;
        fallback_font_en_US_handle.chars_texture = texture_handle;
        fallback_font_en_US_handle.chars_texture_size = size;
    }
    return fallback_font_en_US_handle;
}

static Ui_Int2 get_string_extent(Ui_String const &string,
                                 Ui_FontHandle const &font)
{
    char *chars = string.utf8_first;
    uint32_t chars_n = (uint32_t)string.utf8_n;
    auto const &chars_data = *(font.chars_data);

    float char_x = 0.0f;
    float char_y = 0.0f;
    float max_char_x = char_x;
    float max_char_y = char_y;
    for (size_t chars_i = 0; chars_i < chars_n; ++chars_i) {
        auto c = chars[chars_i];
        if (c < chars_data.codepoint_first || c >= chars_data.codepoint_last) {
            c = '?';
        }
        if (c < chars_data.codepoint_first || c >= chars_data.codepoint_last) {
            // disappointing
            continue;
        }
        int glyph_index = c - chars_data.codepoint_first;
        stbtt_aligned_quad q;
        stbtt_GetBakedQuad(
            chars_data.chars_first, font.chars_texture_size.x,
            font.chars_texture_size.y, glyph_index, &char_x, &char_y, &q,
            /*fill-rule*/ 1); // TODO(nil): how to get the fill rule for bgfx?
        // probably we want better metrics than that
        max_char_x = std::max(char_x, max_char_x);
        max_char_y = std::max(
            -q.y0,
            max_char_y); // register total height for lack of x-height metrics
    }
    Ui_Int2 result;
    result.x = int(max_char_x - 0.0f);
    result.y = int(max_char_y - 0.0f);
    return result;
}

static void draw_string_at(int x,
                           int y,
                           AAB2 clipbox,
                           Ui_String const &string,
                           Ui_FontHandle const &font)
{
    static bool has_init;
    static bgfx_vertex_decl chars_vertex_decl;
    static bgfx_program_handle chars_program;
    if (!has_init) {
        bgfx_vertex_decl_begin(&chars_vertex_decl, BGFX_RENDERER_TYPE_NOOP);
        bgfx_vertex_decl_add(&chars_vertex_decl, BGFX_ATTRIB_POSITION, 2,
                             BGFX_ATTRIB_TYPE_FLOAT, !NamedTrue(Normalized),
                             !NamedTrue(asInt));
        bgfx_vertex_decl_add(&chars_vertex_decl, BGFX_ATTRIB_TEXCOORD0, 2,
                             BGFX_ATTRIB_TYPE_FLOAT, !NamedTrue(Normalized),
                             !NamedTrue(asInt));
        bgfx_vertex_decl_add(&chars_vertex_decl, BGFX_ATTRIB_COLOR0, 4,
                             BGFX_ATTRIB_TYPE_UINT8, !NamedTrue(Normalized),
                             !NamedTrue(asInt));
        bgfx_vertex_decl_end(&chars_vertex_decl);

        auto chars_vsh = bgfx_create_shader(
            bgfx_make_ref(ui_chars_vs_bin, sizeof ui_chars_vs_bin));
        auto chars_fsh = bgfx_create_shader(
            bgfx_make_ref(ui_chars_fs_bin, sizeof ui_chars_fs_bin));
        chars_program = bgfx_create_program(chars_vsh, chars_fsh,
                                            NamedTrue(DestroyShaders));
        has_init = true;
    }

    char *chars = string.utf8_first;
    uint32_t chars_n = (uint32_t)string.utf8_n;
    auto const &chars_data = *(font.chars_data);

    bgfx_transient_index_buffer_t chars_trilist_ib;
    bgfx_alloc_transient_index_buffer(&chars_trilist_ib, 6 * chars_n);

    uint16_t *trilist = (uint16_t *)chars_trilist_ib.data;
    for (size_t char_i = 0; char_i < chars_n; char_i++) {
        auto vertex_first = 4 * char_i;
        auto tri_first = 6 * char_i;
        // tri1
        trilist[tri_first + 0] = uint16_t(vertex_first + 0);
        trilist[tri_first + 1] = uint16_t(vertex_first + 1);
        trilist[tri_first + 2] = uint16_t(vertex_first + 2);
        // tri2
        trilist[tri_first + 3] = uint16_t(vertex_first + 0);
        trilist[tri_first + 4] = uint16_t(vertex_first + 2);
        trilist[tri_first + 5] = uint16_t(vertex_first + 3);
    }

    bgfx_transient_vertex_buffer_t chars_vb;
    bgfx_alloc_transient_vertex_buffer(&chars_vb, 4 * chars_n,
                                       &chars_vertex_decl);

    float char_x = float(x);
    float char_y = float(y);

#pragma pack(push, 1)
    struct CharVertex {
        float x, y;
        float u, v;
        uint32_t rgba;
    };
#pragma pack(pop)
    CharVertex *d_chars = (CharVertex *)chars_vb.data;

    uint16_t emitted_chars_n = 0;
    for (size_t chars_i = 0; chars_i < chars_n; ++chars_i) {
        auto c = chars[chars_i];
        if (c < chars_data.codepoint_first || c >= chars_data.codepoint_last) {
            c = '?';
        }
        if (c < chars_data.codepoint_first || c >= chars_data.codepoint_last) {
            // disappointing
            continue;
        }
        int glyph_index = c - chars_data.codepoint_first;
        stbtt_aligned_quad q;
        stbtt_GetBakedQuad(
            chars_data.chars_first, font.chars_texture_size.x,
            font.chars_texture_size.y, glyph_index, &char_x, &char_y, &q,
            /*fill-rule*/ 1); // TODO(nil): how to get the fill rule for bgfx?
        /* emit char */
        auto &v0 = *d_chars++;
        auto &v1 = *d_chars++;
        auto &v2 = *d_chars++;
        auto &v3 = *d_chars++;
        v0.x = q.x0;
        v0.y = q.y0;
        v0.u = q.s0;
        v0.v = q.t0;
        v1.x = q.x1;
        v1.y = q.y0;
        v1.u = q.s1;
        v1.v = q.t0;
        v2.x = q.x1;
        v2.y = q.y1;
        v2.u = q.s1;
        v2.v = q.t1;
        v3.x = q.x0;
        v3.y = q.y1;
        v3.u = q.s0;
        v3.v = q.t1;
        v0.rgba = v1.rgba = v2.rgba = v3.rgba = 0x0;
        emitted_chars_n++;
    }

    bgfx_set_transient_vertex_buffer(0, &chars_vb, 0, 4 * emitted_chars_n);
    bgfx_set_transient_index_buffer(&chars_trilist_ib, 0, 6 * emitted_chars_n);
    bgfx_set_state(BGFX_STATE_BLEND_ALPHA | BGFX_STATE_RGB_WRITE, 0);
    auto alphaTextureProgramUniform =
        bgfx_create_uniform("s_texAlpha", BGFX_UNIFORM_TYPE_INT1, 1);
    bgfx_set_texture(/* stage */ 0, alphaTextureProgramUniform,
                     font.chars_texture, /*flags*/ UINT32_MAX);
    bgfx_submit(/*view*/ 0, chars_program, 0, !NamedTrue(PreserveState));
}

static Ui_Label label_makef_temp(char const *format, ...)
{
    va_list format_args;
    va_start(format_args, format);

    Memory content;
    /* grab needed size */ {
        va_list format_args2;
        va_copy(format_args2, format_args);
        auto res = std::vsnprintf(0, 0, format, format_args);
        if (res < 0)
            return {};
        auto needed_bytes_n = 1 + res;
        content = alloc(needed_bytes_n, &temp_arena);
        if (content.bytes_n == 0)
            return {};
    }
    std::vsnprintf(content.bytes_first, content.bytes_n, format, format_args);
    Ui_Label result;
    result.string = {
        content.bytes_first, content.bytes_n - 1,
    };
    result.font = fallback_font_en_US();
    return result;
}

static Ui_Int2 label_extent(Ui_Label const &x)
{
    auto e1 = get_string_extent(x.string, x.font);
    auto e2 = get_string_extent({"x", 1}, x.font);
    return {e1.x, e2.y};
}

static void
draw_centered_label(int x, int y, int width, int height, Ui_Label const &label)
{
    auto extent = label_extent(label);
    int label_x = int(x + (width - extent.x) / 2.0);
    int label_y = int(y + (height - extent.y) / 2.0 + extent.y /* baseline */);
    draw_string_at(label_x, label_y, aab2_x_d(x, y, width, height),
                   label_string(label), label_font(label));
};

static void
draw_box_with_border(int x, int y, int width, int height, bool alt_color)
{
    static bool has_init;
    static bgfx_vertex_decl box_vertex_decl;
    static bgfx_index_buffer_handle box_ib;

    static int16_t box_tristrip_indices[] = {
        0, 1, 2, 3,
    };
    static int box_tristrip_indices_n =
        sizeof box_tristrip_indices / sizeof *box_tristrip_indices;
    static bgfx_program_handle box_program;

#pragma pack(push, 1)
    struct BoxVertex {
        float x, y;
        float aab2_x_min;
        float aab2_x_max;
        float aab2_y_min;
        float aab2_y_max;
        uint8_t on_p;
    };
#pragma pack(pop)

    if (!has_init) {
        bgfx_vertex_decl_begin(&box_vertex_decl, BGFX_RENDERER_TYPE_NOOP);
        bgfx_vertex_decl_add(&box_vertex_decl, BGFX_ATTRIB_POSITION, 2,
                             BGFX_ATTRIB_TYPE_FLOAT, !NamedTrue(Normalized),
                             !NamedTrue(asInt));
        bgfx_vertex_decl_add(&box_vertex_decl, BGFX_ATTRIB_TEXCOORD0, 4,
                             BGFX_ATTRIB_TYPE_FLOAT, !NamedTrue(Normalized),
                             !NamedTrue(asInt));
        bgfx_vertex_decl_add(&box_vertex_decl, BGFX_ATTRIB_COLOR0, 1,
                             BGFX_ATTRIB_TYPE_UINT8, !NamedTrue(Normalized),
                             !NamedTrue(asInt));
        bgfx_vertex_decl_end(&box_vertex_decl);

        // @leak
        box_ib = bgfx_create_index_buffer(
            bgfx_make_ref(box_tristrip_indices, sizeof box_tristrip_indices),
            /* flags */ 0);
        auto box_vsh = bgfx_create_shader(
            bgfx_make_ref(ui_button_vs_bin, sizeof ui_button_vs_bin));
        auto box_fsh = bgfx_create_shader(
            bgfx_make_ref(ui_button_fs_bin, sizeof ui_button_fs_bin));
        box_program =
            bgfx_create_program(box_vsh, box_fsh, NamedTrue(DestroyShaders));

        has_init = true;
    }

    uint8_t alt_color_uint = alt_color ? 1 : 0;
    BoxVertex box_vertex_attributes[] = {
        {
            float(x), float(y),
        },
        {
            float(x + width), float(y),
        },
        {
            float(x), float(y + height),
        },
        {
            float(x + width), float(y + height),
        },
    };
    auto aab2 = aab2_x_d(x, y, width, height);
    for (auto &v : box_vertex_attributes) {
        v.aab2_x_min = float(aab2.x_min);
        v.aab2_x_max = float(aab2.x_max);
        v.aab2_y_min = float(aab2.y_min);
        v.aab2_y_max = float(aab2.y_max);
        v.on_p = alt_color_uint;
    }
    static int box_vertex_n = 4;
    bgfx_transient_vertex_buffer_t box_vb;
    bgfx_alloc_transient_vertex_buffer(&box_vb, box_vertex_n, &box_vertex_decl);
    memcpy(box_vb.data, box_vertex_attributes, sizeof box_vertex_attributes);
    box_vb.size = sizeof box_vertex_attributes;

    bgfx_set_transient_vertex_buffer(0, &box_vb, 0, box_vertex_n);
    bgfx_set_index_buffer(box_ib, 0, box_tristrip_indices_n);
    bgfx_set_state(0 | BGFX_STATE_PT_TRISTRIP | BGFX_STATE_RGB_WRITE, 0);

    bgfx_submit(/*view*/ 0, box_program, 0, !NamedTrue(PreserveState));
}

#include "memory.cpp"

#define STB_TRUETYPE_IMPLEMENTATION
#include "third-party-deps/bgfx/3rdparty/stb/stb_truetype.h"
