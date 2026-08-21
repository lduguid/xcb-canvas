#ifndef CANVAS_INTERNAL_H
#define CANVAS_INTERNAL_H

#include "canvas.h"

#include <stdio.h>

#define CANVAS_SND_RATE 22050
#define CANVAS_SND_CLIPS 48
#define CANVAS_SND_VOICES 16

struct Canvas {
    int width, height;
    int running;
    int resized;
    int hud;

    float time;
    float cam_x, cam_y;
    float cam_tx, cam_ty;
    float cam_stiff;
    int cam_has_bounds;
    float bound_x, bound_y, bound_w, bound_h;
    float zoom;

    int key_down[KEY_COUNT];
    int key_pressed[KEY_COUNT];
    int key_released[KEY_COUNT];
    int mouse_x, mouse_y;
    int mouse_down[4];
    int mouse_pressed[4];
    float wheel, wheel_h;

    unsigned font_tex;
    void *plat;

    short *snd_pcm[CANVAS_SND_CLIPS];
    int snd_len[CANVAS_SND_CLIPS];
    int snd_n;
    int snd_v_clip[CANVAS_SND_VOICES];
    int snd_v_pos[CANVAS_SND_VOICES];
    int snd_v_vol[CANVAS_SND_VOICES];
    int snd_v_loop[CANVAS_SND_VOICES];
    void *snd_lock;

    char hot_msg[160];
    float hot_msg_t;
    int hot_hud;
    float frame_dt;
    float fps;

    unsigned seed;
    FILE *log;
    char log_obs[768];
    float log_obs_t;
    float lock_dt;
    float sim_acc;
    int replaying;
};

typedef int (*CanvasHotReloadFn)(void *userdata, Game *game);

CANVAS_API void canvas_hot_setup(CanvasHotReloadFn fn, void *userdata);
CANVAS_API void canvas_hot_set_tool(const char *name);
void canvas_hot_status(Canvas *c, const char *msg, float seconds);
int canvas_hot_active(void);
void canvas_hot_tick(Canvas *c, Game *game, void **state);
void canvas_hot_overlay(Canvas *c);

void canvas_sound_init(Canvas *c);
void canvas_sound_shutdown(Canvas *c);
void canvas_mix(Canvas *c, short *out, int frames);
void canvas_plat_audio_start(Canvas *c);
void canvas_plat_audio_stop(Canvas *c);

void canvas_core_reset(Canvas *c, int width, int height);
void canvas_core_gl_setup(Canvas *c);
void canvas_set_key(Canvas *c, CanvasKey k, int down);
void canvas_keys_release_all(Canvas *c);
void canvas_clear_edges(Canvas *c);
void canvas_apply_camera(Canvas *c, float dt);
void canvas_set_world_proj(const Canvas *c);
void canvas_set_hud_proj(const Canvas *c);
void canvas_plat_set_title(Canvas *c, const char *title);

void canvas_session_begin(Canvas *c, const char *game_name);
void canvas_session_tick(Canvas *c, const Game *g, void *state);
void canvas_session_end(Canvas *c);
void canvas_session_pump(Canvas *c, Game *g, void **state, float wall_dt);
void canvas_session_overlay(Canvas *c);

#endif
