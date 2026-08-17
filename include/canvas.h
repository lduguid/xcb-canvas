#ifndef CANVAS_H
#define CANVAS_H

/* Platform-neutral game API. Wander/bounce link this header only.
 * Swap src/canvas_x11.c for src/canvas_win32.c; do not edit the games. */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Canvas Canvas;

typedef enum {
    KEY_UNKNOWN = 0,
    KEY_LEFT,
    KEY_RIGHT,
    KEY_UP,
    KEY_DOWN,
    KEY_SPACE,
    KEY_ENTER,
    KEY_ESCAPE,
    KEY_TAB,
    KEY_SHIFT,
    KEY_CTRL,
    KEY_ALT,
    KEY_A,
    KEY_B,
    KEY_C,
    KEY_D,
    KEY_E,
    KEY_F,
    KEY_G,
    KEY_H,
    KEY_I,
    KEY_J,
    KEY_K,
    KEY_L,
    KEY_M,
    KEY_N,
    KEY_O,
    KEY_P,
    KEY_Q,
    KEY_R,
    KEY_S,
    KEY_T,
    KEY_U,
    KEY_V,
    KEY_W,
    KEY_X,
    KEY_Y,
    KEY_Z,
    KEY_0,
    KEY_1,
    KEY_2,
    KEY_3,
    KEY_4,
    KEY_5,
    KEY_6,
    KEY_7,
    KEY_8,
    KEY_9,
    KEY_F1,
    KEY_F5,
    KEY_F6,
    KEY_COUNT
} CanvasKey;

#ifdef _WIN32
#define CANVAS_EXPORT __declspec(dllexport)
#else
#define CANVAS_EXPORT
#endif

typedef struct Game {
    const char *name;
    int width;
    int height;
    void *(*init)(Canvas *c);
    void (*update)(void *state, Canvas *c, float dt);
    void (*render)(void *state, Canvas *c);
    void (*shutdown)(void *state, Canvas *c);
    /* Optional. Called after a successful F5 hot-reload with the old state. */
    void (*hot_reload)(void *state, Canvas *c);
} Game;

typedef struct Sprite {
    float x, y, w, h;
    float vx, vy;
    float origin_x, origin_y;
    float angle;
    float scale_x, scale_y;
    float r, g, b, a;
    unsigned texture;
    int frame;
    int frames;
    int frame_w;
    int frame_h;
    float fps;
    float anim_t;
    int visible;
    int flip_x;
    int flip_y;
} Sprite;

int canvas_run(const Game *game);

int canvas_width(const Canvas *c);
int canvas_height(const Canvas *c);
void canvas_set_title(Canvas *c, const char *title);
void canvas_quit(Canvas *c);
float canvas_time(const Canvas *c);
int canvas_resized(const Canvas *c);

int canvas_key_down(const Canvas *c, CanvasKey key);
int canvas_key_pressed(const Canvas *c, CanvasKey key);
int canvas_key_released(const Canvas *c, CanvasKey key);
int canvas_mouse_x(const Canvas *c);
int canvas_mouse_y(const Canvas *c);
int canvas_mouse_down(const Canvas *c, int button);
int canvas_mouse_pressed(const Canvas *c, int button);
float canvas_wheel(const Canvas *c);
float canvas_wheel_h(const Canvas *c);

void canvas_cam_set(Canvas *c, float x, float y);
void canvas_cam_get(const Canvas *c, float *x, float *y);
void canvas_cam_follow(Canvas *c, float x, float y, float stiffness);
void canvas_cam_bounds(Canvas *c, float x, float y, float w, float h);
void canvas_cam_zoom(Canvas *c, float zoom);
float canvas_cam_zoom_get(const Canvas *c);
void canvas_view(const Canvas *c, float *x, float *y, float *w, float *h);
void canvas_screen_to_world(const Canvas *c, float sx, float sy, float *wx, float *wy);

void canvas_clear(Canvas *c, float r, float g, float b);
void canvas_fill_rect(Canvas *c, float x, float y, float w, float h, float r, float g, float b, float a);
void canvas_stroke_rect(Canvas *c, float x, float y, float w, float h, float r, float g, float b, float a);
void canvas_draw_line(Canvas *c, float x1, float y1, float x2, float y2, float r, float g, float b, float a);
void canvas_draw_text(Canvas *c, float x, float y, const char *s, float r, float g, float b);

void canvas_begin_hud(Canvas *c);
void canvas_end_hud(Canvas *c);

unsigned canvas_texture_rgba(Canvas *c, int w, int h, const unsigned char *rgba);
unsigned canvas_texture_solid(Canvas *c, float r, float g, float b);
void canvas_texture_nearest(unsigned tex, int nearest);

void sprite_init(Sprite *s, float x, float y, float w, float h, unsigned tex);
void sprite_update(Sprite *s, float dt);
void canvas_draw_sprite(Canvas *c, const Sprite *s);
void canvas_blit(Canvas *c, unsigned tex, float x, float y, float w, float h, float u0, float v0,
                 float u1, float v1, float r, float g, float b, float a);

/* PCM is copied and resampled to the mixer rate. id 0 is invalid / silent. */
unsigned canvas_sound_pcm(Canvas *c, const short *samples, int count, int rate);
unsigned canvas_sound_tone(Canvas *c, float freq_hz, float ms, float amp);
unsigned canvas_sound_noise(Canvas *c, float ms, float amp);
void canvas_sound_play(Canvas *c, unsigned id, float vol);
void canvas_sound_loop(Canvas *c, unsigned id, float vol);
void canvas_sound_stop(Canvas *c, unsigned id);

#ifdef __cplusplus
}
#endif

#endif
