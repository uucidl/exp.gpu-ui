#pragma once

#ifdef __cplusplus
#define MU_EXTERN_BEGIN extern "C" {
#define MU_EXTERN_END }
#else
#define MU_EXTERN_BEGIN
#define MU_EXTERN_END
#endif

#include <stdint.h>

MU_EXTERN_BEGIN

enum {
    MU_FALSE = 0,
    MU_TRUE = 1,
    MU_MAX_KEYS = 256,
    MU_MAX_TEXT = 256,
    MU_MAX_ERROR = 1024,
    MU_CTRL = 0x11, // VK_CONTROL
    MU_ALT = 0x12, // VK_MENU
    MU_SHIFT = 0x10, // VK_SHIFT
    MU_MAX_AUDIO_BUFFER = 2 * 1024
};

typedef uint8_t Mu_Bool;

struct Mu_Int2 {
    int x;
    int y;
};

struct Mu_DigitalButton {
    Mu_Bool down;
    Mu_Bool pressed;
    Mu_Bool released;
};

struct Mu_AnalogButton {
    float threshold;
    float value;
    Mu_Bool down;
    Mu_Bool pressed;
    Mu_Bool released;
};

struct Mu_Stick {
    float threshold;
    float x;
    float y;
};

struct Mu_Gamepad {
    Mu_Bool connected;
    Mu_DigitalButton a_button;
    Mu_DigitalButton b_button;
    Mu_DigitalButton x_button;
    Mu_DigitalButton y_button;
    Mu_AnalogButton left_trigger;
    Mu_AnalogButton right_trigger;
    Mu_DigitalButton left_shoulder_button;
    Mu_DigitalButton right_shoulder_button;
    Mu_DigitalButton up_button;
    Mu_DigitalButton down_button;
    Mu_DigitalButton left_button;
    Mu_DigitalButton right_button;
    Mu_Stick left_thumb_stick;
    Mu_Stick right_thumb_stick;
    Mu_DigitalButton left_thumb_button;
    Mu_DigitalButton right_thumb_button;
    Mu_DigitalButton back_button;
    Mu_DigitalButton start_button;
};

struct Mu_Mouse {
    Mu_DigitalButton left_button;
    Mu_DigitalButton right_button;
    int wheel;
    int delta_wheel;
    Mu_Int2 position;
    Mu_Int2 delta_position;
};

struct Mu_Window {
    char *title;
    Mu_Int2 position;
    Mu_Int2 size;
    Mu_Bool resized;
};

struct Mu_AudioFormat {
    uint32_t samples_per_second;
    uint32_t channels;
    uint32_t bytes_per_sample;
};

struct Mu_AudioBuffer {
    int16_t *samples;
    size_t samples_count;
    Mu_AudioFormat format;
};

typedef void(*Mu_AudioCallback)(Mu_AudioBuffer *buffer);

struct Mu_Audio {
    Mu_AudioFormat format;
    Mu_AudioCallback callback;
};

struct Mu_Time {
    uint64_t delta_ticks;
    uint64_t delta_nanoseconds;
    uint64_t delta_microseconds;
    uint64_t delta_milliseconds;
    float delta_seconds;

    uint64_t ticks;
    uint64_t nanoseconds;
    uint64_t microseconds;
    uint64_t milliseconds;
    float seconds;

    uint64_t initial_ticks;
    uint64_t ticks_per_second;
};

typedef void *HANDLE;
typedef struct _XINPUT_STATE XINPUT_STATE;
typedef unsigned long (__stdcall *XINPUTGETSTATE)(unsigned long dwUserIndex, XINPUT_STATE* pState);

struct IAudioClient;
struct IAudioRenderClient;

struct Mu_Win32 {
    HANDLE window;
    HANDLE device_context;

    void *main_fiber;
    void *message_fiber;

    XINPUTGETSTATE xinput_get_state;

    IAudioClient *audio_client;
    IAudioRenderClient *audio_render_client;

    HANDLE wgl_context;
};

struct Mu {
    Mu_Bool initialized;
    Mu_Bool quit;

    char *error;
    char error_buffer[MU_MAX_ERROR];

    Mu_Window window;
    Mu_DigitalButton keys[MU_MAX_KEYS];
    Mu_Gamepad gamepad;
    Mu_Mouse mouse;

    char text[MU_MAX_TEXT];
    size_t text_length;

    Mu_Time time;
    Mu_Audio audio;
    Mu_Win32 win32;
};

Mu_Bool Mu_Initialize(Mu *mu);
Mu_Bool Mu_Pull(Mu *mu);
void Mu_Push(Mu *mu);

struct Mu_Image {
    uint8_t *pixels;
    uint32_t channels;
    uint32_t width;
    uint32_t height;
};

Mu_Bool Mu_LoadImage(const char *filename, Mu_Image *image);
Mu_Bool Mu_LoadAudio(const char *filename, Mu_AudioBuffer *audio);

MU_EXTERN_END
