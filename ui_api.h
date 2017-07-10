#ifndef UI_API_H
#define UI_API_H

enum {
    UI_TEXT_CAPACITY = 256,
};

struct Ui_Int2 {
    int x;
    int y;
};

struct Ui_DigitalButton {
    int down_p;
    int pressed_p;
    int released_p;
};

struct Ui_Mouse {
    struct Ui_DigitalButton left_button;
    struct Ui_DigitalButton right_button;
    int wheel;
    int delta_wheel;
    struct Ui_Int2 position;
    struct Ui_Int2 delta_position;
};

struct Ui_Inputs {
    double frame_ms;       // current frame time in ms
    int frame_i;           // current frame id
    struct Ui_Mouse mouse; // main mouse
    char text[UI_TEXT_CAPACITY];
    size_t text_n;
};

struct Ui_Display {
    int resized_p;          // ui was resized
    struct Ui_Int2 size_px; // current size in framebuffer pixels
};

struct Ui {
    struct Ui_Inputs inputs;
    struct Ui_Display display; // one display per ui

    int outdated_frames_n; // output: number of dirty frames the host should
                           // expect from this point
    double validity_ms; // output: expected time to live for the current frame
};

enum Ui_UpdateFlags
{
    Ui_UpdateFlags_Null          = 0,
    Ui_UpdateFlags_ProcessInputs = 1<<0,
    Ui_UpdateFlags_Display       = 1<<1,
};

// API is versioned. This is version 0:
struct Ui_Vtable_0 {
    // NOTE(nil): in this version we use a flag to control whether display is to
    // be made or not. This is to keep the control flow static and as much
    // "immediate" as possible
    //
    // However this forces onto the user the need to check the flags for every
    // call, and forces the host to call update more than once.
    //
    // Alternatively, a better design is to not let the ui render itself, but
    // instead offer a rendercommand type API which it is supposed to feed onto.
    //
    // This is anyway what's going on at the bgfx level. If bgfx allowed us to
    // discard output, then we'd be able to do that cheaply (discard renders for
    // a bit)
    //
    // This however forces us to have a queue of commands somewhere ...
    //
    // You can use `RenderDoc` to verify that every frame the UI emits is different
    // than the previous one. Which is proof that the smart drawing mechanism
    // is working!
    //
    // IM is just an API style. Can we preserve it w/o losing the ability of not
    // drawing more than necessary & w/o doing too much state diffing?
    void (*update)(struct Ui *ui, int /* Ui_UpdateFlags */ flags);
};

#define GET_VTABLE_0_FN(name) struct Ui_Vtable_0 name(void)

typedef GET_VTABLE_0_FN(ui_api_get_vtable_0_fn);

/* Documentation:

Resources we care about:
- CPU usage (time taken from computation, battery life)
- Memory

Goals:
A user interface takes available input devices, interprets those
continuous and discrete actions to trigger data transformations,
computation and communication.

An user interface presents itself and the application to output
devices (displays)

Displays

For ergonomy, the position of elements of an UI is stable in the
coordinate system of that UI. Their position generally shifts as
the result of an user input. Exceptions: timeline editors, graphs.

Display elements are in general not occluding each other, except
in windowing systems. Most apps today follow instead a tiling
arrangement.

Modern displays are framebuffer based and therefore can be seen as local
caches. Going further, graphics processing units (i.e. display accelerators)
go beyond that and can store bitmap elements, textures and GPU programs.

Although modern GPUs can re-render most UI within one display frame, to
preserve resources (CPU resources, for computing/battery life) an UI can
implement:
- just-needed rendering: rather than rendering at the display frame rate
  (144hz, 60hz) the UI can be rendered only the "cache" is out-of-date
- partial rendering: only render what as changed

This applies to CPU-bound computations. Computations done on the GPU would
save on CPU computations. Implications however for battery life depend on
whether the computation is more efficient on the GPU than on the CPU and
whether the entire GPU can go back to IDLE quickly enough.

Just-needed rendering:
- UI is called when input devices receive inputs,
- UI is called on spontaneous state changes: timers, network or when
  a frame ceases to be valid.
- UI elements define validity of a rendering frame:
  For animation, there is a certain time-to-live attached to the rendered
  frame.
  For corner cases such as layouting where the UI needs to converge to a
  stable state, elements may opt to mark more than one frame as out-dated.

  On these conditions the UI will trigger a re-render of the UI.

Partial rendering strategies:
- by subregion (keep track of "dirty regions" and re-render only that)
- overlays (dirty or fast updated regions are rendered separately
  and blit on top of unchanged areas)
Goal: preserve resources.

Failure mode: complete re-render

Quality checklist:
- Can I mark and copy text? Is it any text, or just specific things?
- Can I enlarge the font or the entire view without breaking the app?
- Can I resize the window without breaking the app?
- Can I use the app with just the keyboard, or just the mouse (with an
OS-provided on-screen keyboard)?
- Does it work with a screen reader?
- Does it play nicely with other OS accessibility features (high-contrast mode
or DPI settings)?
- Does it support localisation?
- Does it have legible and high-quality text rendering and various sizes?
- Does it have standard OS chrome (Window icons, menu-bar)

Technical quality checklist:
- Good efficiency (resource un-used when application is idling, minimal
data-retention)
- Styling
- Layouting
- Custom UI elements, canvas for custom drawing
- Scalable UI elements with nevertheless sharp edges
- Good platform sympathy: DPI settings, accessibility,

Platform:
- Desktop + Mobile class platforms w/ GPU

References

Reference: Compositing Tree
@url: https://nothings.org/gamedev/compositing_tree/


Text is often on top

*/

#endif
