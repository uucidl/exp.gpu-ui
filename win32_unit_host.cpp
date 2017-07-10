// Useful references:
//
// @url: https://bkaradzic.github.io/bgfx/overview.html
// @url: https://bkaradzic.github.io/bgfx/bgfx.html

#include "bgfx/bgfx.h"
#include "bgfx/platform.h"
#include "third-party-deps/pervognsen_mu.h"
#include "third-party-deps/uu_win32_reloadable_modules.hpp"

extern "C" {
#include "ui_api.h"
}

#define NO_STRICT
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

/*
** Architecture:
**
** +----------------------------------------------------+
** | Platform layer: mu (audio, window, opengl context) |
** +----------------------------------------------------+
** | Renderer: bgfx                                     |
**
*/

// `bgfx` has a multithreaded renderer, which allows the main loop to run w/o
// blocking the rendering. So users can move the window around even if the main
// loop is stuck in a complex computation.

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <vector>

static int exit_with_message(const wchar_t *message, int exit_code);

struct ProgramPath {
    size_t size;
    size_t directory_end_i;
};

struct UIModule {
    Ui_Vtable_0 vtable_0;
    win32_reloadable_modules::ReloadableModule win32_module;
};

static ProgramPath win32_get_program_path(char *buffer, size_t buffer_size);

typedef void(print_hello_fn)(void);

static void ui_module_load(UIModule *);
static void ui_module_refresh(UIModule *);
static uint64_t now_micros();
static uint64_t global_qpc_origin;
static uint64_t global_qpf_hz;
static void trace(char const *pattern, ...);

static void ui_assign_mu(Ui *ui, Mu const &mu);

int WINAPI WinMain(void *, void *, char *argv, int)
{
    Mu mu = {0};
    char exe_filepath_buffer[4096];
    ProgramPath exe_filepath =
        win32_get_program_path(exe_filepath_buffer, sizeof exe_filepath_buffer);
    if (exe_filepath.size == 0 || exe_filepath.directory_end_i == 0) {
        return exit_with_message(L"Could not find program directory", __LINE__);
    }

    const char *module_name = "ui";
    UIModule ui_module = {};
    // Load ui module
    {
        auto &output_module = ui_module;
        // NOTE(nil): this disallows multiple apps from running in parallel,
        // since the dll is "grabbed" at load time
        win32_reloadable_modules::make_in_path(
            &output_module.win32_module,
            {exe_filepath_buffer,
             exe_filepath_buffer + exe_filepath.directory_end_i},
            {module_name, module_name + std::strlen(module_name)});
        ui_module_load(&output_module);
    }

    if (!Mu_Initialize(&mu)) {
        return exit_with_message(L"Could not initialize `mu`", __LINE__);
    }

    /* plug Mu into bgfx */ {
        bgfx::PlatformData pd = {0};
        pd.nwh = mu.win32.window;
        pd.context = mu.win32.wgl_context;
        bgfx::setPlatformData(pd);
    }
    if (!bgfx::init(bgfx::RendererType::OpenGL)) {
        return exit_with_message(L"Could not initialize `bgfx`", __LINE__);
    }

    uint32_t bgfx_debug_flags = BGFX_DEBUG_TEXT | 0 * BGFX_DEBUG_STATS;

    bgfx::setDebug(bgfx_debug_flags);
    auto caps = bgfx::getCaps();

    int bgfxMustBeReset = true;

    // Should regions of the UI be separate views? What would happen if the card
    // wasn't able to support the number of expected regions? Anyhow, supporting
    // a view per region would allow caching per region w/ a framebuffer for
    // quick redraw.
    int const bgfxLastFreeViewId = caps->limits.maxViews;
    int bgfxNextFreeViewId = 0;
    uint8_t bgfxMainViewId = bgfxNextFreeViewId++;
    bgfx::setViewName(bgfxMainViewId, "MainView");

    // for a 2d UI, drawing primitives in sequential order is more predictable.
    // Otherwise, we would need to use depth to control overdraw.
    bgfx::setViewMode(bgfxMainViewId, bgfx::ViewMode::Sequential);

    // So what should be the clearing strategy?
    // The GUI display is a representation of the UI state at a given point in
    // time.
    //
    // To preserve resources, and because UIs for pragmatic reasons are only
    // changing slightly from frame to frame (because otherwise they would be
    // hard to operate) we assume that the display is not redrawn every frame
    // and only if the screen is dirty.
    //
    // We also could potentially draw only bits of the screen, additively,
    // rather than redrawing everything, to benefit from previously drawn data.
    //
    // Q. how does this interact with double buffering? To do any additive
    // drawing on a double buffered screen, we have to know whether the buffer
    // we're writing on corresponds to the correct state of the UI.

    // So for now we're presuming no additive drawing and force a clear:
    bgfx::setViewClear(bgfxMainViewId, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0,
                       1.0f, 0);

    Ui ui = {0};
    int frame_i = bgfx::frame();
    int outdated_frames_end = frame_i;
    // set up monotonic counter:
    {
        LARGE_INTEGER x;
        QueryPerformanceFrequency(&x);
        global_qpf_hz = x.QuadPart;
        QueryPerformanceCounter(&x);
        global_qpc_origin = x.QuadPart;
    }
    double ms = now_micros() * 1e-3;
    // TODO(nil): if absolute frame validity is kept in a double it might
    // expire for long running apps.
    // Same for absolute frame count, we must guard against wrap-around.
    double validity_ms = ms;
    while (Mu_Pull(&mu)) {
        ms = now_micros() * 1e-3;
        bgfxMustBeReset |= mu.window.resized;
        if (bgfxMustBeReset) {
            bgfx::reset(mu.window.size.x, mu.window.size.y,
                        BGFX_RESET_SRGB_BACKBUFFER);
            bgfx::setViewRect(bgfxMainViewId, 0, 0, mu.window.size.x,
                              mu.window.size.y);
            bgfxMustBeReset = false;
        }

        // reset ui frame state
        ui = {0};
        ui.validity_ms = 1e6; // +Inf
        ui_assign_mu(&ui, mu);
        ui.inputs.frame_ms = ms;
        ui.inputs.frame_i = frame_i;

        if (ui.display.resized_p) {
            // force redraw on resize
            ui.outdated_frames_n = ui.display.resized_p ? 1 : 0;
        }
        if (bgfx_debug_flags & BGFX_DEBUG_STATS) {
            ui.outdated_frames_n = 1;
        } else {
            bgfx::dbgTextClear();
        }

        int host_outdated_frames_n = ui.outdated_frames_n; // or not
        ui_module_refresh(&ui_module);
        if (ui_module.vtable_0.update) {
            Ui module_ui = {0};
            /* UI init */ {
                module_ui.display = ui.display;
                module_ui.inputs = ui.inputs;
                module_ui.outdated_frames_n = host_outdated_frames_n;
                module_ui.validity_ms = 1e6; // +Inf
            }
            ui_module.vtable_0.update(&module_ui, Ui_UpdateFlags_ProcessInputs);
            ui.outdated_frames_n =
                std::max(ui.outdated_frames_n, module_ui.outdated_frames_n);
            ui.validity_ms = std::min(ui.validity_ms, module_ui.validity_ms);
        }

        size_t text_i = 0;
        /* top level keys low-level handling */
        for (size_t i = text_i; i < mu.text_length; ++i) {
            char c = mu.text[i];
            if (c == 'q' || c == 'Q') {
                mu.quit = MU_TRUE;
            } else if (c == 'd' || c == 'D') {
                bgfx_debug_flags ^= BGFX_DEBUG_STATS;
                bgfx::setDebug(bgfx_debug_flags);
            }
            ++text_i;
        }

        outdated_frames_end = frame_i + ui.outdated_frames_n;
        validity_ms = std::min(ms + ui.validity_ms, validity_ms);

        if (0 == (bgfx_debug_flags & BGFX_DEBUG_STATS)) {
            bgfx::dbgTextPrintf(0, 0, 0x0f, "Debug:");
            bgfx::dbgTextPrintf(4, 1, 0x0f, "FreeViewCount: %d",
                                bgfxLastFreeViewId - bgfxNextFreeViewId);
            bgfx::dbgTextPrintf(4, 2, 0x0f, "Validity ms: %f", validity_ms);
        }

        if (frame_i < outdated_frames_end || ms > validity_ms) {
            // dummy call to trigger redraw of this view
            bgfx::touch(bgfxMainViewId);
            if (ui_module.vtable_0.update) {
                Ui module_ui = ui;
                ui_module.vtable_0.update(&module_ui, Ui_UpdateFlags_Display);
            }

            // We cannot nor should call Mu_Push because SwapBuffer is being
            // called by bgfx
            // Mu_Push(&mu);
            frame_i = bgfx::frame();
            validity_ms = std::numeric_limits<double>::max(); // +Inf
        } else {
            SetTimer(mu.win32.window, 2, UINT(validity_ms - ms), nullptr);
            WaitMessage();
        }
    }
    bgfx::shutdown();
    return 0;
}

static void ui_module_load(UIModule *module_)
{
    auto &module = *module_;
    win32_reloadable_modules::ReloadAttemptResultDetails load_details;
    auto result =
        win32_reloadable_modules::load(&module.win32_module, &load_details);
    if (result == win32_reloadable_modules::ReloadAttemptResult::Unloaded) {
        module.vtable_0 = {0};
        return;
    }

    ui_api_get_vtable_0_fn *get_vtable_0 =
        reinterpret_cast<ui_api_get_vtable_0_fn *>(
            GetProcAddress(module.win32_module.dll, "ui_get_vtable_0"));
    if (get_vtable_0) {
        module.vtable_0 = get_vtable_0();
    }
}

#include <cstdarg>
#include <cstdio>

static void trace(char const *pattern, ...)
{
    va_list vl;
    va_start(vl, pattern);
    if (auto fh = fopen("trace.out", "ab+")) {
        vfprintf(fh, pattern, vl);
        fflush(fh);
        fclose(fh);
    }
}

static void ui_module_refresh(UIModule *module_)
{
    auto &module = *module_;
    if (win32_reloadable_modules::has_changed(&module.win32_module)) {
        ui_module_load(&module);
    }
}

static uint64_t now_micros()
{
    LARGE_INTEGER pc;
    QueryPerformanceCounter(&pc);
    uint64_t y = pc.QuadPart - global_qpc_origin;
    y *= 1'000'000;
    y /= global_qpf_hz;
    return y;
}

static int exit_with_message(const wchar_t *message, int exit_code)
{
    MessageBoxW(NULL, message, L"Unrecoverable error", 0);
    std::exit(exit_code);
    return exit_code;
}

static void ui_assign_mu(Ui *ui_, Mu const &mu)
{
    auto &ui = *ui_;
    ui.display.resized_p = mu.window.resized;
    ui.display.size_px.x = mu.window.size.x;
    ui.display.size_px.y = mu.window.size.y;
    /* copy mouse */ {
        auto &y = ui.inputs.mouse;
        auto const &x = mu.mouse;
        struct {
            Ui_DigitalButton &y;
            Mu_DigitalButton const &x;
        } buttons[] = {
            {y.left_button, x.left_button}, {y.right_button, x.right_button},
        };
        for (auto const &pair : buttons) {
            pair.y.down_p = pair.x.down;
            pair.y.pressed_p = pair.x.pressed;
            pair.y.released_p = pair.x.released;
        }
        y.wheel = x.wheel;
        y.delta_wheel = x.delta_wheel;
        y.position.x = x.position.x;
        y.position.y = x.position.y;
        y.delta_position.x = x.delta_position.x;
        y.delta_position.y = x.delta_position.y;
    }
    ui.inputs.text_n = std::min((size_t)UI_TEXT_CAPACITY, mu.text_length);
    memcpy(ui.inputs.text, mu.text, std::min(ui.inputs.text_n, mu.text_length));
}

static ProgramPath win32_get_program_path(char *buffer, size_t buffer_size)
{
    size_t size = GetModuleFileNameA(0, buffer, (uint32_t)buffer_size);
    size_t dir_end_i = 0;
    // find last dir separator
    {
        char const x = '\\'; // directory separator
        char const *input = buffer;
        size_t const input_size = size;
        size_t y_i = 0;
        for (size_t i = 0; i < input_size; ++i) {
            if (input[i] == x) {
                y_i = i;
            }
        }
        dir_end_i = y_i;
    }
    ProgramPath result = {0};
    result.size = size;
    result.directory_end_i = dir_end_i;
    return result;
}

#include "third-party-deps/pervognsen_mu.cpp"

#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "Ole32.lib")
#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfplay.lib")
#pragma comment(lib, "Mfplat.lib")
#pragma comment(lib, "Mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")
