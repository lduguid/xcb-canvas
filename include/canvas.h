#ifndef CANVAS_H
#define CANVAS_H

/* Platform-neutral game API. Wander/bounce link this header only.
 * Swap src/canvas_x11.c for src/canvas_win32.c; do not edit the games. */

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

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
#ifdef CANVAS_BUILD_DLL
#define CANVAS_API __declspec(dllexport)
#else
#define CANVAS_API __declspec(dllimport)
#endif
#else
#define CANVAS_EXPORT
#define CANVAS_API
#endif

typedef struct Game {
    const char *name;
    int width;
    int height;
    void *(*init)(Canvas *c);
    void (*update)(void *state, Canvas *c, float dt);
    void (*render)(void *state, Canvas *c);
    void (*shutdown)(void *state, Canvas *c);
    /* Optional. After F5, old heap is still live. */
    void (*hot_reload)(void *state, Canvas *c);
    /* Optional. Write one JSON object of observable state into buf (no newline).
     * Used for CANVAS_LOG playthrough captures. Must not change gameplay. */
    void (*observe)(void *state, Canvas *c, char *buf, size_t n);
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
    /* Grid sheet (0 = legacy horizontal strip). */
    int sheet_cols;
    int sheet_first;
    int tex_w, tex_h;
} Sprite;

/* Equal-sized cells packed left-to-right, top-to-bottom. */
typedef struct Sheet {
    unsigned texture;
    int tex_w, tex_h;
    int cell_w, cell_h;
    int cols, rows;
} Sheet;

CANVAS_API int canvas_run(const Game *game);

CANVAS_API int canvas_width(const Canvas *c);
CANVAS_API int canvas_height(const Canvas *c);
CANVAS_API void canvas_set_title(Canvas *c, const char *title);
CANVAS_API void canvas_quit(Canvas *c);
CANVAS_API float canvas_time(const Canvas *c);
CANVAS_API int canvas_resized(const Canvas *c);

CANVAS_API int canvas_key_down(const Canvas *c, CanvasKey key);
CANVAS_API int canvas_key_pressed(const Canvas *c, CanvasKey key);
CANVAS_API int canvas_key_released(const Canvas *c, CanvasKey key);
CANVAS_API int canvas_mouse_x(const Canvas *c);
CANVAS_API int canvas_mouse_y(const Canvas *c);
CANVAS_API int canvas_mouse_down(const Canvas *c, int button);
CANVAS_API int canvas_mouse_pressed(const Canvas *c, int button);
CANVAS_API float canvas_wheel(const Canvas *c);
CANVAS_API float canvas_wheel_h(const Canvas *c);

CANVAS_API void canvas_cam_set(Canvas *c, float x, float y);
CANVAS_API void canvas_cam_get(const Canvas *c, float *x, float *y);
CANVAS_API void canvas_cam_follow(Canvas *c, float x, float y, float stiffness);
CANVAS_API void canvas_cam_bounds(Canvas *c, float x, float y, float w, float h);
CANVAS_API void canvas_cam_zoom(Canvas *c, float zoom);
CANVAS_API float canvas_cam_zoom_get(const Canvas *c);
CANVAS_API void canvas_view(const Canvas *c, float *x, float *y, float *w, float *h);
CANVAS_API void canvas_screen_to_world(const Canvas *c, float sx, float sy, float *wx, float *wy);
CANVAS_API void canvas_world_to_screen(const Canvas *c, float wx, float wy, float *sx, float *sy);

/* Seed from CANVAS_SEED, or from CANVAS_REPLAY's session line, or 0 if the game should pick. */
CANVAS_API unsigned canvas_seed(const Canvas *c);
/* 1 while playing back CANVAS_REPLAY (same engine, injected input). */
CANVAS_API int canvas_replaying(const Canvas *c);
/* No-op unless CANVAS_LOG is set. One JSONL event: kind + formatted msg. */
CANVAS_API void canvas_trace(Canvas *c, const char *kind, const char *fmt, ...);
CANVAS_API void canvas_inject_key(Canvas *c, CanvasKey key, int down);
CANVAS_API void canvas_inject_mouse(Canvas *c, int x, int y);
CANVAS_API void canvas_inject_click(Canvas *c, int button);

CANVAS_API void canvas_clear(Canvas *c, float r, float g, float b);
CANVAS_API void canvas_fill_rect(Canvas *c, float x, float y, float w, float h, float r, float g, float b, float a);
CANVAS_API void canvas_stroke_rect(Canvas *c, float x, float y, float w, float h, float r, float g, float b, float a);
CANVAS_API void canvas_draw_line(Canvas *c, float x1, float y1, float x2, float y2, float r, float g, float b, float a);
CANVAS_API void canvas_draw_pixel(Canvas *c, float x, float y, float r, float g, float b, float a);
CANVAS_API void canvas_fill_triangle(Canvas *c, float x0, float y0, float x1, float y1, float x2, float y2,
                                     float r, float g, float b, float a);
CANVAS_API void canvas_stroke_triangle(Canvas *c, float x0, float y0, float x1, float y1, float x2, float y2,
                                       float r, float g, float b, float a);
/* Affine textured triangle. tex is a canvas_texture_* id; UV is 0–1. Tint with rgba. */
CANVAS_API void canvas_fill_triangle_tex(Canvas *c, unsigned tex, float x0, float y0, float u0, float v0,
                                         float x1, float y1, float u1, float v1, float x2, float y2, float u2,
                                         float v2, float r, float g, float b, float a);
CANVAS_API void canvas_draw_text(Canvas *c, float x, float y, const char *s, float r, float g, float b);

CANVAS_API void canvas_begin_hud(Canvas *c);
CANVAS_API void canvas_end_hud(Canvas *c);

CANVAS_API unsigned canvas_texture_rgba(Canvas *c, int w, int h, const unsigned char *rgba);
CANVAS_API unsigned canvas_texture_solid(Canvas *c, float r, float g, float b);
/* PNG, JPEG, or BMP. Path is relative to the exe directory. Returns 0 on failure. */
CANVAS_API unsigned canvas_texture_file(Canvas *c, const char *path);
/* Probe width/height without uploading. Returns 1 on success. */
CANVAS_API int canvas_image_info(const char *path, int *w, int *h);
CANVAS_API void canvas_texture_nearest(unsigned tex, int nearest);

CANVAS_API void sprite_init(Sprite *s, float x, float y, float w, float h, unsigned tex);
CANVAS_API void sprite_update(Sprite *s, float dt);
CANVAS_API void canvas_draw_sprite(Canvas *c, const Sprite *s);
CANVAS_API void canvas_blit(Canvas *c, unsigned tex, float x, float y, float w, float h, float u0, float v0,
                 float u1, float v1, float r, float g, float b, float a);

/* cell_w / cell_h are pixel size of one tile. Leftover pixels on the right/bottom are ignored. */
CANVAS_API int canvas_sheet_load(Canvas *c, Sheet *sheet, const char *path, int cell_w, int cell_h);
CANVAS_API void canvas_sheet_init(Sheet *sheet, unsigned tex, int tex_w, int tex_h, int cell_w, int cell_h);
CANVAS_API int canvas_sheet_count(const Sheet *sheet);
CANVAS_API void canvas_draw_sheet(Canvas *c, const Sheet *sheet, int index, float x, float y, float w, float h);
CANVAS_API void sprite_from_sheet(Sprite *s, const Sheet *sheet, int index, float x, float y);
/* Cells [first, first+count) play in a loop. count 1 is a still. */
CANVAS_API void sprite_anim(Sprite *s, const Sheet *sheet, int first, int count, float fps);
CANVAS_API void sprite_set_cell(Sprite *s, int index);

/* PCM is copied and resampled to the mixer rate. id 0 is invalid / silent. */
CANVAS_API unsigned canvas_sound_pcm(Canvas *c, const short *samples, int count, int rate);
CANVAS_API unsigned canvas_sound_tone(Canvas *c, float freq_hz, float ms, float amp);
CANVAS_API unsigned canvas_sound_noise(Canvas *c, float ms, float amp);
CANVAS_API void canvas_sound_play(Canvas *c, unsigned id, float vol);
CANVAS_API void canvas_sound_loop(Canvas *c, unsigned id, float vol);
CANVAS_API void canvas_sound_stop(Canvas *c, unsigned id);

/* Heap that lives in canvas.dll. Use these in init/shutdown so F5/F6 can
 * unload the game plugin without free() hitting a dead MinGW CRT. */
CANVAS_API void *canvas_alloc(size_t n);
CANVAS_API void *canvas_calloc(size_t n, size_t sz);
CANVAS_API void canvas_free(void *p);

#ifdef __cplusplus
}
#endif

#endif
