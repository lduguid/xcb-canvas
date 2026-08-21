#include "canvas_internal.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
#include <pthread.h>
#endif

#ifdef _WIN32
#include <windows.h>
#include <GL/gl.h>
#ifndef GL_CLAMP
#define GL_CLAMP 0x2900
#endif
#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif
#else
#include <GL/gl.h>
#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_ONLY_BMP
#define STBI_NO_HDR
#define STBI_NO_LINEAR
#define STBI_NO_THREAD_LOCALS
#include "stb_image.h"
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

/* Public-domain 8x8 ASCII 32-126, bit7 = leftmost pixel. */
static const unsigned char FONT8[95][8] = {
    {0,0,0,0,0,0,0,0},
    {0x18,0x18,0x18,0x18,0x18,0x00,0x18,0x00},
    {0x6c,0x6c,0x24,0x00,0x00,0x00,0x00,0x00},
    {0x6c,0x6c,0xfe,0x6c,0xfe,0x6c,0x6c,0x00},
    {0x18,0x3e,0x60,0x3c,0x06,0x7c,0x18,0x00},
    {0x62,0x66,0x0c,0x18,0x30,0x66,0x46,0x00},
    {0x38,0x6c,0x38,0x76,0xdc,0xcc,0x76,0x00},
    {0x18,0x18,0x30,0x00,0x00,0x00,0x00,0x00},
    {0x0c,0x18,0x30,0x30,0x30,0x18,0x0c,0x00},
    {0x30,0x18,0x0c,0x0c,0x0c,0x18,0x30,0x00},
    {0x00,0x66,0x3c,0xff,0x3c,0x66,0x00,0x00},
    {0x00,0x18,0x18,0x7e,0x18,0x18,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x30},
    {0x00,0x00,0x00,0x7e,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x00},
    {0x02,0x06,0x0c,0x18,0x30,0x60,0x40,0x00},
    {0x3c,0x66,0x6e,0x76,0x66,0x66,0x3c,0x00},
    {0x18,0x38,0x18,0x18,0x18,0x18,0x7e,0x00},
    {0x3c,0x66,0x06,0x1c,0x30,0x60,0x7e,0x00},
    {0x3c,0x66,0x06,0x1c,0x06,0x66,0x3c,0x00},
    {0x0c,0x1c,0x3c,0x6c,0x7e,0x0c,0x0c,0x00},
    {0x7e,0x60,0x7c,0x06,0x06,0x66,0x3c,0x00},
    {0x1c,0x30,0x60,0x7c,0x66,0x66,0x3c,0x00},
    {0x7e,0x06,0x0c,0x18,0x30,0x30,0x30,0x00},
    {0x3c,0x66,0x66,0x3c,0x66,0x66,0x3c,0x00},
    {0x3c,0x66,0x66,0x3e,0x06,0x0c,0x38,0x00},
    {0x00,0x18,0x18,0x00,0x00,0x18,0x18,0x00},
    {0x00,0x18,0x18,0x00,0x00,0x18,0x18,0x30},
    {0x0c,0x18,0x30,0x60,0x30,0x18,0x0c,0x00},
    {0x00,0x00,0x7e,0x00,0x7e,0x00,0x00,0x00},
    {0x30,0x18,0x0c,0x06,0x0c,0x18,0x30,0x00},
    {0x3c,0x66,0x06,0x0c,0x18,0x00,0x18,0x00},
    {0x3c,0x66,0x6e,0x6e,0x60,0x62,0x3c,0x00},
    {0x18,0x3c,0x66,0x66,0x7e,0x66,0x66,0x00},
    {0x7c,0x66,0x66,0x7c,0x66,0x66,0x7c,0x00},
    {0x3c,0x66,0x60,0x60,0x60,0x66,0x3c,0x00},
    {0x78,0x6c,0x66,0x66,0x66,0x6c,0x78,0x00},
    {0x7e,0x60,0x60,0x7c,0x60,0x60,0x7e,0x00},
    {0x7e,0x60,0x60,0x7c,0x60,0x60,0x60,0x00},
    {0x3c,0x66,0x60,0x6e,0x66,0x66,0x3c,0x00},
    {0x66,0x66,0x66,0x7e,0x66,0x66,0x66,0x00},
    {0x7e,0x18,0x18,0x18,0x18,0x18,0x7e,0x00},
    {0x06,0x06,0x06,0x06,0x66,0x66,0x3c,0x00},
    {0x66,0x6c,0x78,0x70,0x78,0x6c,0x66,0x00},
    {0x60,0x60,0x60,0x60,0x60,0x60,0x7e,0x00},
    {0xc6,0xee,0xfe,0xd6,0xc6,0xc6,0xc6,0x00},
    {0x66,0x76,0x7e,0x7e,0x6e,0x66,0x66,0x00},
    {0x3c,0x66,0x66,0x66,0x66,0x66,0x3c,0x00},
    {0x7c,0x66,0x66,0x7c,0x60,0x60,0x60,0x00},
    {0x3c,0x66,0x66,0x66,0x76,0x6c,0x36,0x00},
    {0x7c,0x66,0x66,0x7c,0x78,0x6c,0x66,0x00},
    {0x3c,0x66,0x60,0x3c,0x06,0x66,0x3c,0x00},
    {0x7e,0x18,0x18,0x18,0x18,0x18,0x18,0x00},
    {0x66,0x66,0x66,0x66,0x66,0x66,0x3c,0x00},
    {0x66,0x66,0x66,0x66,0x66,0x3c,0x18,0x00},
    {0xc6,0xc6,0xc6,0xd6,0xfe,0xee,0xc6,0x00},
    {0x66,0x66,0x3c,0x18,0x3c,0x66,0x66,0x00},
    {0x66,0x66,0x66,0x3c,0x18,0x18,0x18,0x00},
    {0x7e,0x06,0x0c,0x18,0x30,0x60,0x7e,0x00},
    {0x3c,0x30,0x30,0x30,0x30,0x30,0x3c,0x00},
    {0x40,0x60,0x30,0x18,0x0c,0x06,0x02,0x00},
    {0x3c,0x0c,0x0c,0x0c,0x0c,0x0c,0x3c,0x00},
    {0x18,0x3c,0x66,0x00,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xff},
    {0x30,0x18,0x0c,0x00,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x3c,0x06,0x3e,0x66,0x3e,0x00},
    {0x60,0x60,0x7c,0x66,0x66,0x66,0x7c,0x00},
    {0x00,0x00,0x3c,0x66,0x60,0x66,0x3c,0x00},
    {0x06,0x06,0x3e,0x66,0x66,0x66,0x3e,0x00},
    {0x00,0x00,0x3c,0x66,0x7e,0x60,0x3c,0x00},
    {0x1c,0x30,0x7c,0x30,0x30,0x30,0x30,0x00},
    {0x00,0x00,0x3e,0x66,0x66,0x3e,0x06,0x3c},
    {0x60,0x60,0x7c,0x66,0x66,0x66,0x66,0x00},
    {0x18,0x00,0x38,0x18,0x18,0x18,0x3c,0x00},
    {0x06,0x00,0x06,0x06,0x06,0x06,0x66,0x3c},
    {0x60,0x60,0x66,0x6c,0x78,0x6c,0x66,0x00},
    {0x38,0x18,0x18,0x18,0x18,0x18,0x3c,0x00},
    {0x00,0x00,0x6c,0xfe,0xd6,0xc6,0xc6,0x00},
    {0x00,0x00,0x7c,0x66,0x66,0x66,0x66,0x00},
    {0x00,0x00,0x3c,0x66,0x66,0x66,0x3c,0x00},
    {0x00,0x00,0x7c,0x66,0x66,0x7c,0x60,0x60},
    {0x00,0x00,0x3e,0x66,0x66,0x3e,0x06,0x06},
    {0x00,0x00,0x6c,0x76,0x60,0x60,0x60,0x00},
    {0x00,0x00,0x3e,0x60,0x3c,0x06,0x7c,0x00},
    {0x30,0x30,0x7c,0x30,0x30,0x30,0x1c,0x00},
    {0x00,0x00,0x66,0x66,0x66,0x66,0x3e,0x00},
    {0x00,0x00,0x66,0x66,0x66,0x3c,0x18,0x00},
    {0x00,0x00,0xc6,0xd6,0xfe,0x7c,0x6c,0x00},
    {0x00,0x00,0x66,0x3c,0x18,0x3c,0x66,0x00},
    {0x00,0x00,0x66,0x66,0x66,0x3e,0x06,0x3c},
    {0x00,0x00,0x7e,0x0c,0x18,0x30,0x7e,0x00},
    {0x0e,0x18,0x18,0x70,0x18,0x18,0x0e,0x00},
    {0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x00},
    {0x70,0x18,0x18,0x0e,0x18,0x18,0x70,0x00},
    {0x76,0xdc,0x00,0x00,0x00,0x00,0x00,0x00},
};

void canvas_core_reset(Canvas *c, int width, int height)
{
    memset(c, 0, sizeof(*c));
    c->width = width > 0 ? width : 800;
    c->height = height > 0 ? height : 600;
    c->running = 1;
    c->zoom = 1.0f;
    c->hot_hud = 1;
}

void canvas_core_gl_setup(Canvas *c)
{
    unsigned char atlas[16 * 8 * 16 * 8 * 4];
    int ch, row, col, x, y, i;

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_LINE_SMOOTH);
    glDisable(GL_CULL_FACE);

    memset(atlas, 0, sizeof(atlas));
    for (ch = 0; ch < 95; ch++) {
        int gx = (ch % 16) * 8;
        int gy = (ch / 16) * 8;
        for (row = 0; row < 8; row++) {
            unsigned char bits = FONT8[ch][row];
            for (col = 0; col < 8; col++) {
                if (bits & (0x80 >> col)) {
                    x = gx + col;
                    y = gy + row;
                    i = (y * 128 + x) * 4;
                    atlas[i] = atlas[i + 1] = atlas[i + 2] = atlas[i + 3] = 255;
                }
            }
        }
    }
    c->font_tex = canvas_texture_rgba(c, 128, 128, atlas);
}

void canvas_set_key(Canvas *c, CanvasKey k, int down)
{
    if (k <= KEY_UNKNOWN || k >= KEY_COUNT)
        return;
    if (down && !c->key_down[k])
        c->key_pressed[k] = 1;
    if (!down && c->key_down[k])
        c->key_released[k] = 1;
    c->key_down[k] = down;
}

void canvas_keys_release_all(Canvas *c)
{
    memset(c->key_down, 0, sizeof(c->key_down));
    memset(c->key_pressed, 0, sizeof(c->key_pressed));
    memset(c->key_released, 0, sizeof(c->key_released));
}

void canvas_clear_edges(Canvas *c)
{
    memset(c->key_pressed, 0, sizeof(c->key_pressed));
    memset(c->key_released, 0, sizeof(c->key_released));
    memset(c->mouse_pressed, 0, sizeof(c->mouse_pressed));
    c->wheel = 0;
    c->wheel_h = 0;
    c->resized = 0;
}

void canvas_apply_camera(Canvas *c, float dt)
{
    float k, vw, vh;

    if (c->cam_stiff > 0.0f) {
        k = 1.0f - expf(-c->cam_stiff * dt);
        c->cam_x += (c->cam_tx - c->cam_x) * k;
        c->cam_y += (c->cam_ty - c->cam_y) * k;
    } else {
        c->cam_x = c->cam_tx;
        c->cam_y = c->cam_ty;
    }

    if (c->zoom < 0.1f)
        c->zoom = 0.1f;
    vw = (float)c->width / c->zoom;
    vh = (float)c->height / c->zoom;
    if (c->cam_has_bounds) {
        if (vw >= c->bound_w)
            c->cam_x = c->bound_x + (c->bound_w - vw) * 0.5f;
        else {
            if (c->cam_x < c->bound_x)
                c->cam_x = c->bound_x;
            if (c->cam_x + vw > c->bound_x + c->bound_w)
                c->cam_x = c->bound_x + c->bound_w - vw;
        }
        if (vh >= c->bound_h)
            c->cam_y = c->bound_y + (c->bound_h - vh) * 0.5f;
        else {
            if (c->cam_y < c->bound_y)
                c->cam_y = c->bound_y;
            if (c->cam_y + vh > c->bound_y + c->bound_h)
                c->cam_y = c->bound_y + c->bound_h - vh;
        }
    }
}

void canvas_set_world_proj(const Canvas *c)
{
    float vw = (float)c->width / c->zoom;
    float vh = (float)c->height / c->zoom;

    glViewport(0, 0, c->width, c->height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho((double)c->cam_x, (double)(c->cam_x + vw), (double)(c->cam_y + vh), (double)c->cam_y, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

void canvas_set_hud_proj(const Canvas *c)
{
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0, (double)c->width, (double)c->height, 0.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

int canvas_width(const Canvas *c) { return c->width; }
int canvas_height(const Canvas *c) { return c->height; }
float canvas_time(const Canvas *c) { return c->time; }
int canvas_resized(const Canvas *c) { return c->resized; }
void canvas_quit(Canvas *c) { c->running = 0; }

void *canvas_alloc(size_t n)
{
    return n ? malloc(n) : NULL;
}

void *canvas_calloc(size_t n, size_t sz)
{
    if (!n || !sz)
        return NULL;
    return calloc(n, sz);
}

void canvas_free(void *p)
{
    free(p);
}

void canvas_set_title(Canvas *c, const char *title)
{
    canvas_plat_set_title(c, title ? title : "");
}

int canvas_key_down(const Canvas *c, CanvasKey key)
{
    return key > 0 && key < KEY_COUNT && c->key_down[key];
}
int canvas_key_pressed(const Canvas *c, CanvasKey key)
{
    return key > 0 && key < KEY_COUNT && c->key_pressed[key];
}
int canvas_key_released(const Canvas *c, CanvasKey key)
{
    return key > 0 && key < KEY_COUNT && c->key_released[key];
}
int canvas_mouse_x(const Canvas *c) { return c->mouse_x; }
int canvas_mouse_y(const Canvas *c) { return c->mouse_y; }
int canvas_mouse_down(const Canvas *c, int button)
{
    return button >= 1 && button <= 3 && c->mouse_down[button];
}
int canvas_mouse_pressed(const Canvas *c, int button)
{
    return button >= 1 && button <= 3 && c->mouse_pressed[button];
}
float canvas_wheel(const Canvas *c) { return c->wheel; }
float canvas_wheel_h(const Canvas *c) { return c->wheel_h; }

void canvas_cam_set(Canvas *c, float x, float y)
{
    c->cam_x = c->cam_tx = x;
    c->cam_y = c->cam_ty = y;
    c->cam_stiff = 0.0f;
}
void canvas_cam_get(const Canvas *c, float *x, float *y)
{
    if (x)
        *x = c->cam_x;
    if (y)
        *y = c->cam_y;
}
void canvas_cam_follow(Canvas *c, float x, float y, float stiffness)
{
    c->cam_tx = x;
    c->cam_ty = y;
    c->cam_stiff = stiffness;
}
void canvas_cam_bounds(Canvas *c, float x, float y, float w, float h)
{
    c->cam_has_bounds = 1;
    c->bound_x = x;
    c->bound_y = y;
    c->bound_w = w;
    c->bound_h = h;
}
void canvas_cam_zoom(Canvas *c, float zoom) { c->zoom = zoom > 0.1f ? zoom : 0.1f; }
float canvas_cam_zoom_get(const Canvas *c) { return c->zoom; }

void canvas_view(const Canvas *c, float *x, float *y, float *w, float *h)
{
    if (x)
        *x = c->cam_x;
    if (y)
        *y = c->cam_y;
    if (w)
        *w = (float)c->width / c->zoom;
    if (h)
        *h = (float)c->height / c->zoom;
}

void canvas_screen_to_world(const Canvas *c, float sx, float sy, float *wx, float *wy)
{
    if (wx)
        *wx = c->cam_x + sx / c->zoom;
    if (wy)
        *wy = c->cam_y + sy / c->zoom;
}

void canvas_world_to_screen(const Canvas *c, float wx, float wy, float *sx, float *sy)
{
    if (sx)
        *sx = (wx - c->cam_x) * c->zoom;
    if (sy)
        *sy = (wy - c->cam_y) * c->zoom;
}

unsigned canvas_seed(const Canvas *c)
{
    return c ? c->seed : 0;
}

int canvas_replaying(const Canvas *c)
{
    return c && c->replaying;
}

void canvas_inject_key(Canvas *c, CanvasKey key, int down)
{
    canvas_set_key(c, key, down);
}

void canvas_inject_mouse(Canvas *c, int x, int y)
{
    if (!c)
        return;
    c->mouse_x = x;
    c->mouse_y = y;
}

void canvas_inject_click(Canvas *c, int button)
{
    if (!c || button < 1 || button > 3)
        return;
    c->mouse_down[button] = 1;
    c->mouse_pressed[button] = 1;
}

static void json_esc(char *dst, size_t n, const char *s)
{
    size_t o = 0;

    if (!dst || n == 0)
        return;
    if (!s)
        s = "";
    while (*s && o + 2 < n) {
        if (*s == '"' || *s == '\\') {
            if (o + 3 >= n)
                break;
            dst[o++] = '\\';
            dst[o++] = *s++;
        } else if ((unsigned char)*s < 32) {
            s++;
        } else {
            dst[o++] = *s++;
        }
    }
    dst[o] = 0;
}

static const char *log_key_name(int k)
{
    static const char *n[KEY_COUNT] = {
        "?", "left", "right", "up", "down", "space", "enter", "esc", "tab", "shift", "ctrl", "alt",
        "a", "b", "c", "d", "e", "f", "g", "h", "i", "j", "k", "l", "m", "n", "o", "p", "q", "r",
        "s", "t", "u", "v", "w", "x", "y", "z", "0", "1", "2", "3", "4", "5", "6", "7", "8", "9",
        "f1", "f5", "f6"
    };
    if (k <= 0 || k >= KEY_COUNT)
        return "?";
    return n[k] ? n[k] : "?";
}

static int log_key_from_name(const char *s)
{
    int k;

    if (!s || !s[0])
        return 0;
    for (k = 1; k < KEY_COUNT; k++) {
        if (strcmp(log_key_name(k), s) == 0)
            return k;
    }
    return 0;
}

static void keys_csv(const int *on, char *dst, size_t n)
{
    int i, first = 1;

    if (!dst || n == 0)
        return;
    dst[0] = 0;
    for (i = 1; i < KEY_COUNT; i++) {
        if (!on[i])
            continue;
        if (!first)
            strncat(dst, ",", n - strlen(dst) - 1);
        strncat(dst, log_key_name(i), n - strlen(dst) - 1);
        first = 0;
    }
}

static const char *json_field(const char *line, const char *key)
{
    char pat[72];
    const char *p;

    snprintf(pat, sizeof(pat), "\"%s\":", key);
    p = strstr(line, pat);
    if (!p)
        return NULL;
    p += strlen(pat);
    while (*p == ' ')
        p++;
    return p;
}

static int json_str(const char *line, const char *key, char *out, size_t n)
{
    const char *p = json_field(line, key);
    size_t o = 0;

    if (!out || n == 0)
        return 0;
    out[0] = 0;
    if (!p || *p != '"')
        return 0;
    p++;
    while (*p && *p != '"' && o + 1 < n) {
        if (*p == '\\' && p[1])
            p++;
        out[o++] = *p++;
    }
    out[o] = 0;
    return 1;
}

static int json_num(const char *line, const char *key, double *out)
{
    const char *p = json_field(line, key);

    if (!p || !out)
        return 0;
    *out = strtod(p, NULL);
    return 1;
}

typedef struct {
    float t;
    int mx, my, click, pulse;
    char down[64];
    char up[64];
} ReplayEv;

static struct {
    ReplayEv *ev;
    int n, i, cap, done;
    int down[KEY_COUNT];
    char pulse_up[64];
} replay;

static void replay_free(void)
{
    free(replay.ev);
    memset(&replay, 0, sizeof(replay));
}

static void replay_push(ReplayEv e)
{
    if (replay.n >= replay.cap) {
        int cap = replay.cap ? replay.cap * 2 : 256;
        ReplayEv *n = realloc(replay.ev, (size_t)cap * sizeof(*n));
        if (!n)
            return;
        replay.ev = n;
        replay.cap = cap;
    }
    replay.ev[replay.n++] = e;
}

static void replay_apply_csv(Canvas *c, const char *csv, int down)
{
    char buf[64], *p, *tok;

    if (!csv || !csv[0])
        return;
    snprintf(buf, sizeof(buf), "%s", csv);
    p = buf;
    while (p && *p) {
        int k;
        tok = p;
        p = strchr(p, ',');
        if (p)
            *p++ = 0;
        k = log_key_from_name(tok);
        if (!k || k == KEY_F5 || k == KEY_F6 || k == KEY_ESCAPE)
            continue;
        if (down) {
            if (!replay.down[k])
                c->key_pressed[k] = 1;
            replay.down[k] = 1;
        } else {
            if (replay.down[k])
                c->key_released[k] = 1;
            replay.down[k] = 0;
        }
        c->key_down[k] = replay.down[k];
    }
}

static int replay_load(Canvas *c, const char *path)
{
    FILE *f;
    char line[1024], ev[24];
    double t, seed, dt, mx, my, click;

    f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "canvas: cannot open CANVAS_REPLAY %s\n", path);
        return 0;
    }
    replay_free();
    while (fgets(line, sizeof(line), f)) {
        if (!json_str(line, "ev", ev, sizeof(ev)))
            continue;
        if (strcmp(ev, "session") == 0) {
            if (json_num(line, "seed", &seed))
                c->seed = (unsigned)seed;
            if (json_num(line, "dt", &dt) && dt > 0.0)
                c->lock_dt = (float)dt;
        } else if (strcmp(ev, "input") == 0) {
            ReplayEv e;
            memset(&e, 0, sizeof(e));
            if (json_num(line, "t", &t))
                e.t = (float)t;
            if (json_num(line, "mx", &mx))
                e.mx = (int)mx;
            if (json_num(line, "my", &my))
                e.my = (int)my;
            if (json_num(line, "click", &click))
                e.click = (int)click;
            json_str(line, "down", e.down, sizeof(e.down));
            if (!e.down[0]) {
                if (json_str(line, "keys", e.down, sizeof(e.down)) && e.down[0])
                    e.pulse = 1; /* older logs had no key-up */
            }
            json_str(line, "up", e.up, sizeof(e.up));
            replay_push(e);
        }
    }
    fclose(f);
    c->replaying = 1;
    if (c->lock_dt <= 0.0f)
        c->lock_dt = 1.0f / 60.0f;
    fprintf(stderr, "canvas: replay %s  (%d input events, seed %u)\n", path, replay.n, c->seed);
    return 1;
}

static void canvas_replay_apply(Canvas *c)
{
    int esc, i;

    if (!c || !c->replaying)
        return;
    esc = c->key_pressed[KEY_ESCAPE] || c->key_down[KEY_ESCAPE] || c->key_pressed[KEY_Q];
    memset(c->key_pressed, 0, sizeof(c->key_pressed));
    memset(c->key_released, 0, sizeof(c->key_released));
    memset(c->mouse_pressed, 0, sizeof(c->mouse_pressed));
    if (replay.pulse_up[0]) {
        replay_apply_csv(c, replay.pulse_up, 0);
        replay.pulse_up[0] = 0;
    }
    memcpy(c->key_down, replay.down, sizeof(c->key_down));
    while (replay.i < replay.n && replay.ev[replay.i].t <= c->time + 0.0005f) {
        ReplayEv *e = &replay.ev[replay.i++];
        c->mouse_x = e->mx;
        c->mouse_y = e->my;
        replay_apply_csv(c, e->down, 1);
        replay_apply_csv(c, e->up, 0);
        if (e->pulse && e->down[0])
            snprintf(replay.pulse_up, sizeof(replay.pulse_up), "%s", e->down);
        if (e->click >= 1 && e->click <= 3) {
            c->mouse_pressed[e->click] = 1;
            c->mouse_down[e->click] = 1;
        }
    }
    if (replay.i >= replay.n)
        replay.done = 1;
    for (i = 1; i <= 3; i++) {
        if (!c->mouse_pressed[i])
            c->mouse_down[i] = 0;
    }
    if (esc) {
        c->key_down[KEY_ESCAPE] = 1;
        c->key_pressed[KEY_ESCAPE] = 1;
    }
}

void canvas_session_pump(Canvas *c, Game *g, void **state, float wall_dt)
{
    float dt;
    int steps = 0;

    if (!c)
        return;
    if (wall_dt < 0.0f)
        wall_dt = 0.0f;
    c->frame_dt = wall_dt > 1e-6f ? (float)wall_dt : 1e-6f;
    if (c->lock_dt > 0.0f) {
        if (wall_dt > 0.25f)
            wall_dt = 0.25f;
        c->sim_acc += (float)wall_dt;
        while (c->sim_acc >= c->lock_dt && steps < 8) {
            c->sim_acc -= c->lock_dt;
            dt = c->lock_dt;
            c->time += dt;
            canvas_replay_apply(c);
            canvas_hot_tick(c, g, state);
            if (g && g->update)
                g->update(*state, c, dt);
            canvas_session_tick(c, g, state ? *state : NULL);
            canvas_apply_camera(c, dt);
            canvas_clear_edges(c);
            steps++;
        }
        return;
    }
    dt = (float)wall_dt;
    if (dt > 0.05f)
        dt = 0.05f;
    c->time += dt;
    canvas_replay_apply(c);
    canvas_hot_tick(c, g, state);
    if (g && g->update)
        g->update(*state, c, dt);
    canvas_session_tick(c, g, state ? *state : NULL);
    canvas_apply_camera(c, dt);
}

void canvas_trace(Canvas *c, const char *kind, const char *fmt, ...)
{
    char msg[192], esc[256], kesc[48];
    va_list ap;

    if (!c || !c->log)
        return;
    msg[0] = 0;
    if (fmt) {
        va_start(ap, fmt);
        vsnprintf(msg, sizeof(msg), fmt, ap);
        va_end(ap);
    }
    json_esc(esc, sizeof(esc), msg);
    json_esc(kesc, sizeof(kesc), kind ? kind : "event");
    fprintf(c->log, "{\"t\":%.3f,\"ev\":\"trace\",\"kind\":\"%s\",\"msg\":\"%s\"}\n", (double)c->time, kesc, esc);
    fflush(c->log);
}

void canvas_session_begin(Canvas *c, const char *game_name)
{
    const char *path, *seed_e, *rep;
    char gesc[64];

    replay_free();
    c->replaying = 0;
    c->lock_dt = 0;
    seed_e = getenv("CANVAS_SEED");
    if (seed_e && seed_e[0])
        c->seed = (unsigned)strtoul(seed_e, NULL, 10);
    rep = getenv("CANVAS_REPLAY");
    if (rep && rep[0])
        replay_load(c, rep);
    path = getenv("CANVAS_LOG");
    if (c->replaying || (path && path[0]))
        c->lock_dt = c->lock_dt > 0.0f ? c->lock_dt : (1.0f / 60.0f);
    if (!path || !path[0] || c->replaying)
        return;
    if (path[0] == '-' && path[1] == 0)
        c->log = stdout;
    else
        c->log = fopen(path, "w");
    if (!c->log)
        return;
    json_esc(gesc, sizeof(gesc), game_name ? game_name : "game");
    fprintf(c->log, "{\"t\":0,\"ev\":\"session\",\"game\":\"%s\",\"seed\":%u,\"dt\":%.6f,\"w\":%d,\"h\":%d}\n",
            gesc, c->seed, (double)c->lock_dt, c->width, c->height);
    fflush(c->log);
}

void canvas_session_tick(Canvas *c, const Game *g, void *state)
{
    char obs[768], down[96], up[96], held[96];
    int click = 0, changed, any;

    if (!c || !c->log || c->replaying)
        return;
    keys_csv(c->key_pressed, down, sizeof(down));
    keys_csv(c->key_released, up, sizeof(up));
    keys_csv(c->key_down, held, sizeof(held));
    if (c->mouse_pressed[1])
        click = 1;
    else if (c->mouse_pressed[2])
        click = 2;
    else if (c->mouse_pressed[3])
        click = 3;
    any = down[0] || up[0] || click;
    if (any)
        fprintf(c->log,
                "{\"t\":%.3f,\"ev\":\"input\",\"down\":\"%s\",\"up\":\"%s\",\"held\":\"%s\","
                "\"mx\":%d,\"my\":%d,\"click\":%d}\n",
                (double)c->time, down, up, held, c->mouse_x, c->mouse_y, click);
    obs[0] = 0;
    if (g && g->observe)
        g->observe(state, c, obs, sizeof(obs));
    changed = obs[0] && strcmp(obs, c->log_obs) != 0;
    if (changed || (obs[0] && c->time - c->log_obs_t >= 1.0f)) {
        fprintf(c->log, "{\"t\":%.3f,\"ev\":\"state\",\"data\":%s}\n", (double)c->time, obs[0] ? obs : "{}");
        snprintf(c->log_obs, sizeof(c->log_obs), "%s", obs);
        c->log_obs_t = c->time;
        fflush(c->log);
    }
}

void canvas_session_end(Canvas *c)
{
    if (c && c->log) {
        fprintf(c->log, "{\"t\":%.3f,\"ev\":\"end\"}\n", (double)c->time);
        if (c->log != stdout)
            fclose(c->log);
        c->log = NULL;
    }
    replay_free();
}

void canvas_session_overlay(Canvas *c)
{
    char line[64];

    if (!c || !c->replaying)
        return;
    canvas_begin_hud(c);
    if (replay.done)
        snprintf(line, sizeof(line), "REPLAY  ended   %.1fs", (double)c->time);
    else
        snprintf(line, sizeof(line), "REPLAY  %.1fs   %d/%d", (double)c->time, replay.i, replay.n);
    canvas_draw_text(c, 12, 40, line, 0.95f, 0.82f, 0.35f);
    canvas_end_hud(c);
}

void canvas_clear(Canvas *c, float r, float g, float b)
{
    (void)c;
    glClearColor(r, g, b, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
}

void canvas_fill_rect(Canvas *c, float x, float y, float w, float h, float r, float g, float b, float a)
{
    (void)c;
    glDisable(GL_TEXTURE_2D);
    glColor4f(r, g, b, a);
    glBegin(GL_QUADS);
    glVertex2f(x, y);
    glVertex2f(x + w, y);
    glVertex2f(x + w, y + h);
    glVertex2f(x, y + h);
    glEnd();
}

void canvas_stroke_rect(Canvas *c, float x, float y, float w, float h, float r, float g, float b, float a)
{
    (void)c;
    glDisable(GL_TEXTURE_2D);
    glColor4f(r, g, b, a);
    glBegin(GL_LINE_LOOP);
    glVertex2f(x + 0.5f, y + 0.5f);
    glVertex2f(x + w - 0.5f, y + 0.5f);
    glVertex2f(x + w - 0.5f, y + h - 0.5f);
    glVertex2f(x + 0.5f, y + h - 0.5f);
    glEnd();
}

void canvas_draw_line(Canvas *c, float x1, float y1, float x2, float y2, float r, float g, float b, float a)
{
    (void)c;
    glDisable(GL_TEXTURE_2D);
    glColor4f(r, g, b, a);
    glBegin(GL_LINES);
    glVertex2f(x1, y1);
    glVertex2f(x2, y2);
    glEnd();
}

void canvas_draw_pixel(Canvas *c, float x, float y, float r, float g, float b, float a)
{
    canvas_fill_rect(c, x, y, 1.0f, 1.0f, r, g, b, a);
}

void canvas_fill_triangle(Canvas *c, float x0, float y0, float x1, float y1, float x2, float y2, float r,
                          float g, float b, float a)
{
    (void)c;
    glDisable(GL_TEXTURE_2D);
    glColor4f(r, g, b, a);
    glBegin(GL_TRIANGLES);
    glVertex2f(x0, y0);
    glVertex2f(x1, y1);
    glVertex2f(x2, y2);
    glEnd();
}

void canvas_stroke_triangle(Canvas *c, float x0, float y0, float x1, float y1, float x2, float y2, float r,
                            float g, float b, float a)
{
    (void)c;
    glDisable(GL_TEXTURE_2D);
    glColor4f(r, g, b, a);
    glBegin(GL_LINE_LOOP);
    glVertex2f(x0, y0);
    glVertex2f(x1, y1);
    glVertex2f(x2, y2);
    glEnd();
}

void canvas_fill_triangle_tex(Canvas *c, unsigned tex, float x0, float y0, float u0, float v0, float x1,
                              float y1, float u1, float v1, float x2, float y2, float u2, float v2, float r,
                              float g, float b, float a)
{
    (void)c;
    if (!tex) {
        canvas_fill_triangle(c, x0, y0, x1, y1, x2, y2, r, g, b, a);
        return;
    }
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, tex);
    glColor4f(r, g, b, a);
    glBegin(GL_TRIANGLES);
    glTexCoord2f(u0, v0);
    glVertex2f(x0, y0);
    glTexCoord2f(u1, v1);
    glVertex2f(x1, y1);
    glTexCoord2f(u2, v2);
    glVertex2f(x2, y2);
    glEnd();
}

void canvas_draw_text(Canvas *c, float x, float y, const char *s, float r, float g, float b)
{
    float cx = x;
    float cy = y - 8.0f;

    if (!s || !c->font_tex)
        return;
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, c->font_tex);
    glColor3f(r, g, b);
    glBegin(GL_QUADS);
    for (; *s; s++) {
        unsigned char ch = (unsigned char)*s;
        int idx, gx, gy;
        float u0, v0, u1, v1;
        if (ch < 32 || ch > 126)
            ch = '?';
        idx = ch - 32;
        gx = (idx % 16) * 8;
        gy = (idx / 16) * 8;
        u0 = (float)gx / 128.0f;
        v0 = (float)gy / 128.0f;
        u1 = (float)(gx + 8) / 128.0f;
        v1 = (float)(gy + 8) / 128.0f;
        glTexCoord2f(u0, v0);
        glVertex2f(cx, cy);
        glTexCoord2f(u1, v0);
        glVertex2f(cx + 8, cy);
        glTexCoord2f(u1, v1);
        glVertex2f(cx + 8, cy + 8);
        glTexCoord2f(u0, v1);
        glVertex2f(cx, cy + 8);
        cx += 8;
    }
    glEnd();
}

void canvas_begin_hud(Canvas *c)
{
    c->hud = 1;
    canvas_set_hud_proj(c);
}

void canvas_end_hud(Canvas *c)
{
    c->hud = 0;
    canvas_set_world_proj(c);
}

unsigned canvas_texture_rgba(Canvas *c, int w, int h, const unsigned char *rgba)
{
    unsigned tex = 0;
    (void)c;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    return tex;
}

unsigned canvas_texture_solid(Canvas *c, float r, float g, float b)
{
    unsigned char px[4] = {(unsigned char)(r * 255.0f), (unsigned char)(g * 255.0f),
                           (unsigned char)(b * 255.0f), 255};
    return canvas_texture_rgba(c, 1, 1, px);
}

unsigned canvas_texture_file(Canvas *c, const char *path)
{
    int w = 0, h = 0, n = 0;
    unsigned char *rgba;
    unsigned tex;

    if (!path || !path[0])
        return 0;
    rgba = stbi_load(path, &w, &h, &n, 4);
    if (!rgba) {
        fprintf(stderr, "canvas: cannot load '%s': %s\n", path,
                stbi_failure_reason() ? stbi_failure_reason() : "unknown");
        return 0;
    }
    tex = canvas_texture_rgba(c, w, h, rgba);
    stbi_image_free(rgba);
    return tex;
}

int canvas_image_info(const char *path, int *w, int *h)
{
    int tw = 0, th = 0, n = 0;
    if (!path || !path[0] || !stbi_info(path, &tw, &th, &n))
        return 0;
    if (w)
        *w = tw;
    if (h)
        *h = th;
    return 1;
}

void canvas_texture_nearest(unsigned tex, int nearest)
{
    int filter = nearest ? GL_NEAREST : GL_LINEAR;
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
}

void sprite_init(Sprite *s, float x, float y, float w, float h, unsigned tex)
{
    memset(s, 0, sizeof(*s));
    s->x = x;
    s->y = y;
    s->w = w;
    s->h = h;
    s->scale_x = s->scale_y = 1.0f;
    s->r = s->g = s->b = s->a = 1.0f;
    s->texture = tex;
    s->frames = 1;
    s->visible = 1;
}

void sprite_update(Sprite *s, float dt)
{
    s->x += s->vx * dt;
    s->y += s->vy * dt;
    if (s->frames > 1 && s->fps > 0.0f) {
        s->anim_t += dt * s->fps;
        while (s->anim_t >= 1.0f) {
            s->anim_t -= 1.0f;
            s->frame = (s->frame + 1) % s->frames;
        }
    }
}

void canvas_blit(Canvas *c, unsigned tex, float x, float y, float w, float h, float u0, float v0,
                 float u1, float v1, float r, float g, float b, float a)
{
    (void)c;
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, tex);
    glColor4f(r, g, b, a);
    glBegin(GL_QUADS);
    glTexCoord2f(u0, v0);
    glVertex2f(x, y);
    glTexCoord2f(u1, v0);
    glVertex2f(x + w, y);
    glTexCoord2f(u1, v1);
    glVertex2f(x + w, y + h);
    glTexCoord2f(u0, v1);
    glVertex2f(x, y + h);
    glEnd();
}

int canvas_sheet_count(const Sheet *sheet)
{
    if (!sheet || sheet->cols <= 0 || sheet->rows <= 0)
        return 0;
    return sheet->cols * sheet->rows;
}

static int sheet_clamp(const Sheet *sheet, int index)
{
    int n = canvas_sheet_count(sheet);
    if (n <= 0)
        return 0;
    if (index < 0)
        return 0;
    if (index >= n)
        return n - 1;
    return index;
}

static void sheet_uv(const Sheet *sheet, int index, float *u0, float *v0, float *u1, float *v1)
{
    int col, row;
    float tw, th;

    index = sheet_clamp(sheet, index);
    col = sheet->cols > 0 ? index % sheet->cols : 0;
    row = sheet->cols > 0 ? index / sheet->cols : 0;
    tw = sheet->tex_w > 0 ? (float)sheet->tex_w : 1.0f;
    th = sheet->tex_h > 0 ? (float)sheet->tex_h : 1.0f;
    /* Inset half a texel so filtering never samples the next cell or the black wrap border. */
    *u0 = ((float)(col * sheet->cell_w) + 0.5f) / tw;
    *v0 = ((float)(row * sheet->cell_h) + 0.5f) / th;
    *u1 = ((float)((col + 1) * sheet->cell_w) - 0.5f) / tw;
    *v1 = ((float)((row + 1) * sheet->cell_h) - 0.5f) / th;
}

void canvas_sheet_init(Sheet *sheet, unsigned tex, int tex_w, int tex_h, int cell_w, int cell_h)
{
    if (!sheet)
        return;
    memset(sheet, 0, sizeof(*sheet));
    if (cell_w <= 0 || cell_h <= 0 || tex_w <= 0 || tex_h <= 0)
        return;
    sheet->texture = tex;
    sheet->tex_w = tex_w;
    sheet->tex_h = tex_h;
    sheet->cell_w = cell_w;
    sheet->cell_h = cell_h;
    sheet->cols = tex_w / cell_w;
    sheet->rows = tex_h / cell_h;
}

int canvas_sheet_load(Canvas *c, Sheet *sheet, const char *path, int cell_w, int cell_h)
{
    int w = 0, h = 0, n = 0;
    unsigned char *rgba;
    unsigned tex;

    if (sheet)
        memset(sheet, 0, sizeof(*sheet));
    if (!c || !sheet || !path || !path[0] || cell_w <= 0 || cell_h <= 0)
        return 0;
    rgba = stbi_load(path, &w, &h, &n, 4);
    if (!rgba) {
        fprintf(stderr, "canvas: cannot load sheet '%s': %s\n", path,
                stbi_failure_reason() ? stbi_failure_reason() : "unknown");
        return 0;
    }
    if (w < cell_w || h < cell_h) {
        fprintf(stderr, "canvas: sheet '%s' is %dx%d, smaller than a %dx%d cell\n",
                path, w, h, cell_w, cell_h);
        stbi_image_free(rgba);
        return 0;
    }
    tex = canvas_texture_rgba(c, w, h, rgba);
    stbi_image_free(rgba);
    canvas_sheet_init(sheet, tex, w, h, cell_w, cell_h);
    return sheet->cols > 0 && sheet->rows > 0;
}

void canvas_draw_sheet(Canvas *c, const Sheet *sheet, int index, float x, float y, float w, float h)
{
    float u0, v0, u1, v1;
    if (!sheet || !sheet->texture)
        return;
    if (w <= 0.0f)
        w = (float)sheet->cell_w;
    if (h <= 0.0f)
        h = (float)sheet->cell_h;
    sheet_uv(sheet, index, &u0, &v0, &u1, &v1);
    canvas_blit(c, sheet->texture, x, y, w, h, u0, v0, u1, v1, 1, 1, 1, 1);
}

void sprite_from_sheet(Sprite *s, const Sheet *sheet, int index, float x, float y)
{
    if (!s || !sheet)
        return;
    index = sheet_clamp(sheet, index);
    sprite_init(s, x, y, (float)sheet->cell_w, (float)sheet->cell_h, sheet->texture);
    s->frame_w = sheet->cell_w;
    s->frame_h = sheet->cell_h;
    s->sheet_cols = sheet->cols;
    s->sheet_first = index;
    s->tex_w = sheet->tex_w;
    s->tex_h = sheet->tex_h;
    s->frames = 1;
    s->frame = 0;
    s->fps = 0.0f;
}

void sprite_anim(Sprite *s, const Sheet *sheet, int first, int count, float fps)
{
    int n, maxc;
    if (!s || !sheet)
        return;
    n = canvas_sheet_count(sheet);
    first = sheet_clamp(sheet, first);
    maxc = n - first;
    if (count < 1)
        count = 1;
    if (count > maxc)
        count = maxc > 0 ? maxc : 1;
    if (!s->texture)
        sprite_init(s, s->x, s->y, (float)sheet->cell_w, (float)sheet->cell_h, sheet->texture);
    s->texture = sheet->texture;
    if (s->w <= 0.0f)
        s->w = (float)sheet->cell_w;
    if (s->h <= 0.0f)
        s->h = (float)sheet->cell_h;
    s->frame_w = sheet->cell_w;
    s->frame_h = sheet->cell_h;
    s->sheet_cols = sheet->cols;
    s->sheet_first = first;
    s->tex_w = sheet->tex_w;
    s->tex_h = sheet->tex_h;
    s->frames = count;
    s->frame = 0;
    s->anim_t = 0.0f;
    s->fps = fps;
}

void sprite_set_cell(Sprite *s, int index)
{
    if (!s)
        return;
    s->sheet_first = index < 0 ? 0 : index;
    s->frames = 1;
    s->frame = 0;
    s->fps = 0.0f;
    s->anim_t = 0.0f;
}

void canvas_draw_sprite(Canvas *c, const Sprite *s)
{
    float u0 = 0.0f, v0 = 0.0f, u1 = 1.0f, v1 = 1.0f;
    float w, h, ox, oy;

    if (!s || !s->visible)
        return;
    if (s->sheet_cols > 0 && s->frame_w > 0 && s->tex_w > 0) {
        int cell = s->sheet_first + s->frame;
        int col = cell % s->sheet_cols;
        int row = cell / s->sheet_cols;
        float tw = (float)s->tex_w, th = s->tex_h > 0 ? (float)s->tex_h : 1.0f;
        u0 = (float)(col * s->frame_w) / tw;
        v0 = (float)(row * s->frame_h) / th;
        u1 = (float)((col + 1) * s->frame_w) / tw;
        v1 = (float)((row + 1) * s->frame_h) / th;
    } else if (s->frames > 1 && s->frame_w > 0) {
        u0 = (float)(s->frame * s->frame_w) / (float)(s->frame_w * s->frames);
        u1 = (float)((s->frame + 1) * s->frame_w) / (float)(s->frame_w * s->frames);
    }
    if (s->flip_x) {
        float t = u0;
        u0 = u1;
        u1 = t;
    }
    if (s->flip_y) {
        float t = v0;
        v0 = v1;
        v1 = t;
    }

    w = s->w * s->scale_x;
    h = s->h * s->scale_y;
    ox = s->x - s->origin_x * w;
    oy = s->y - s->origin_y * h;

    glPushMatrix();
    if (s->angle != 0.0f) {
        glTranslatef(s->x, s->y, 0.0f);
        glRotatef(s->angle, 0.0f, 0.0f, 1.0f);
        glTranslatef(-s->x, -s->y, 0.0f);
    }
    if (s->texture)
        canvas_blit(c, s->texture, ox, oy, w, h, u0, v0, u1, v1, s->r, s->g, s->b, s->a);
    else
        canvas_fill_rect(c, ox, oy, w, h, s->r, s->g, s->b, s->a);
    glPopMatrix();
}

#ifdef _WIN32
#define SND_LOCK(c) EnterCriticalSection((CRITICAL_SECTION *)(c)->snd_lock)
#define SND_UNLOCK(c) LeaveCriticalSection((CRITICAL_SECTION *)(c)->snd_lock)
#else
#define SND_LOCK(c) pthread_mutex_lock((pthread_mutex_t *)(c)->snd_lock)
#define SND_UNLOCK(c) pthread_mutex_unlock((pthread_mutex_t *)(c)->snd_lock)
#endif

void canvas_sound_init(Canvas *c)
{
#ifdef _WIN32
    CRITICAL_SECTION *cs = (CRITICAL_SECTION *)malloc(sizeof(CRITICAL_SECTION));
    if (cs)
        InitializeCriticalSection(cs);
    c->snd_lock = cs;
#else
    pthread_mutex_t *mu = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
    if (mu)
        pthread_mutex_init(mu, NULL);
    c->snd_lock = mu;
#endif
}

/* Drop clips and voices but keep the mixer lock. Used on F6 so init() can
 * allocate sounds again without leaking into CANVAS_SND_CLIPS. */
static void canvas_sound_reset(Canvas *c)
{
    int i;
    if (!c)
        return;
    if (c->snd_lock)
        SND_LOCK(c);
    for (i = 1; i <= c->snd_n && i < CANVAS_SND_CLIPS; i++) {
        free(c->snd_pcm[i]);
        c->snd_pcm[i] = NULL;
        c->snd_len[i] = 0;
    }
    c->snd_n = 0;
    memset(c->snd_v_clip, 0, sizeof(c->snd_v_clip));
    memset(c->snd_v_pos, 0, sizeof(c->snd_v_pos));
    memset(c->snd_v_vol, 0, sizeof(c->snd_v_vol));
    memset(c->snd_v_loop, 0, sizeof(c->snd_v_loop));
    if (c->snd_lock)
        SND_UNLOCK(c);
}

void canvas_sound_shutdown(Canvas *c)
{
    int i;
    if (c->snd_lock)
        SND_LOCK(c);
    for (i = 1; i <= c->snd_n && i < CANVAS_SND_CLIPS; i++) {
        free(c->snd_pcm[i]);
        c->snd_pcm[i] = NULL;
        c->snd_len[i] = 0;
    }
    c->snd_n = 0;
    memset(c->snd_v_clip, 0, sizeof(c->snd_v_clip));
    if (c->snd_lock) {
        SND_UNLOCK(c);
#ifdef _WIN32
        DeleteCriticalSection((CRITICAL_SECTION *)c->snd_lock);
#else
        pthread_mutex_destroy((pthread_mutex_t *)c->snd_lock);
#endif
        free(c->snd_lock);
        c->snd_lock = NULL;
    }
}

void canvas_mix(Canvas *c, short *out, int frames)
{
    int i, v;

    if (!c->snd_lock) {
        memset(out, 0, (size_t)frames * sizeof(short));
        return;
    }
    SND_LOCK(c);
    for (i = 0; i < frames; i++) {
        int acc = 0;
        for (v = 0; v < CANVAS_SND_VOICES; v++) {
            int id = c->snd_v_clip[v];
            short *pcm;
            int n;
            if (id <= 0 || id >= CANVAS_SND_CLIPS)
                continue;
            pcm = c->snd_pcm[id];
            n = c->snd_len[id];
            if (!pcm || n <= 0) {
                c->snd_v_clip[v] = 0;
                continue;
            }
            acc += ((int)pcm[c->snd_v_pos[v]] * c->snd_v_vol[v]) >> 8;
            c->snd_v_pos[v]++;
            if (c->snd_v_pos[v] >= n) {
                if (c->snd_v_loop[v])
                    c->snd_v_pos[v] = 0;
                else
                    c->snd_v_clip[v] = 0;
            }
        }
        if (acc > 32767)
            acc = 32767;
        if (acc < -32768)
            acc = -32768;
        out[i] = (short)acc;
    }
    SND_UNLOCK(c);
}

unsigned canvas_sound_pcm(Canvas *c, const short *samples, int count, int rate)
{
    int nout, i, id;
    short *dst;

    if (!c || !samples || count <= 0 || rate <= 0)
        return 0;
    if (c->snd_n + 1 >= CANVAS_SND_CLIPS)
        return 0;
    if (rate == CANVAS_SND_RATE)
        nout = count;
    else
        nout = (int)(((long long)count * CANVAS_SND_RATE) / rate);
    if (nout < 1)
        nout = 1;
    dst = (short *)malloc((size_t)nout * sizeof(short));
    if (!dst)
        return 0;
    if (rate == CANVAS_SND_RATE)
        memcpy(dst, samples, (size_t)nout * sizeof(short));
    else {
        for (i = 0; i < nout; i++) {
            float src = (float)i * (float)rate / (float)CANVAS_SND_RATE;
            int i0 = (int)src;
            int i1 = i0 + 1 < count ? i0 + 1 : count - 1;
            float f = src - (float)i0;
            dst[i] = (short)(samples[i0] * (1.0f - f) + samples[i1] * f);
        }
    }
    if (c->snd_lock)
        SND_LOCK(c);
    id = ++c->snd_n;
    c->snd_pcm[id] = dst;
    c->snd_len[id] = nout;
    if (c->snd_lock)
        SND_UNLOCK(c);
    return (unsigned)id;
}

unsigned canvas_sound_tone(Canvas *c, float freq_hz, float ms, float amp)
{
    int n, i, half, sign = 1;
    unsigned id;
    short *s;

    if (freq_hz < 20.0f)
        freq_hz = 20.0f;
    if (ms < 8.0f)
        ms = 8.0f;
    if (ms > 2500.0f)
        ms = 2500.0f;
    if (amp < 0.0f)
        amp = 0.0f;
    if (amp > 1.0f)
        amp = 1.0f;
    n = (int)(CANVAS_SND_RATE * (ms / 1000.0f));
    if (n < 8)
        n = 8;
    s = (short *)malloc((size_t)n * sizeof(short));
    if (!s)
        return 0;
    half = (int)((float)CANVAS_SND_RATE / (freq_hz * 2.0f));
    if (half < 1)
        half = 1;
    for (i = 0; i < n; i++) {
        float env = 1.0f - (float)i / (float)n;
        if ((i % half) == 0)
            sign = -sign;
        s[i] = (short)(sign * env * env * amp * 28000.0f);
    }
    id = canvas_sound_pcm(c, s, n, CANVAS_SND_RATE);
    free(s);
    return id;
}

unsigned canvas_sound_noise(Canvas *c, float ms, float amp)
{
    int n, i, rng = 0xACE1u;
    unsigned id;
    short *s;

    if (ms < 8.0f)
        ms = 8.0f;
    if (ms > 2500.0f)
        ms = 2500.0f;
    if (amp < 0.0f)
        amp = 0.0f;
    if (amp > 1.0f)
        amp = 1.0f;
    n = (int)(CANVAS_SND_RATE * (ms / 1000.0f));
    if (n < 8)
        n = 8;
    s = (short *)malloc((size_t)n * sizeof(short));
    if (!s)
        return 0;
    for (i = 0; i < n; i++) {
        float env = 1.0f - (float)i / (float)n;
        rng = (rng >> 1) ^ (-(rng & 1) & 0xB400);
        s[i] = (short)(((rng & 0xFFFF) - 32768) * env * amp * 0.55f);
    }
    id = canvas_sound_pcm(c, s, n, CANVAS_SND_RATE);
    free(s);
    return id;
}

static void snd_start_voice(Canvas *c, unsigned id, float vol, int loop)
{
    int v, pick = -1, best_pos = -1;

    if (!id || id >= (unsigned)CANVAS_SND_CLIPS || !c->snd_pcm[id])
        return;
    if (vol < 0.0f)
        vol = 0.0f;
    if (vol > 1.0f)
        vol = 1.0f;
    if (!c->snd_lock)
        return;
    SND_LOCK(c);
    for (v = 0; v < CANVAS_SND_VOICES; v++) {
        if (!c->snd_v_clip[v]) {
            pick = v;
            break;
        }
        if (!c->snd_v_loop[v] && c->snd_v_pos[v] > best_pos) {
            best_pos = c->snd_v_pos[v];
            pick = v;
        }
    }
    if (pick < 0)
        pick = 0;
    c->snd_v_clip[pick] = (int)id;
    c->snd_v_pos[pick] = 0;
    c->snd_v_vol[pick] = (int)(vol * 256.0f);
    c->snd_v_loop[pick] = loop;
    SND_UNLOCK(c);
}

void canvas_sound_play(Canvas *c, unsigned id, float vol)
{
    snd_start_voice(c, id, vol, 0);
}

void canvas_sound_loop(Canvas *c, unsigned id, float vol)
{
    snd_start_voice(c, id, vol, 1);
}

void canvas_sound_stop(Canvas *c, unsigned id)
{
    int v;
    if (!c->snd_lock)
        return;
    SND_LOCK(c);
    for (v = 0; v < CANVAS_SND_VOICES; v++) {
        if (!id || c->snd_v_clip[v] == (int)id)
            c->snd_v_clip[v] = 0;
    }
    SND_UNLOCK(c);
}

static CanvasHotReloadFn g_hot_fn;
static void *g_hot_ud;
static int g_hot_kept_state;

static char g_hot_tool[64];

void canvas_hot_setup(CanvasHotReloadFn fn, void *userdata)
{
    g_hot_fn = fn;
    g_hot_ud = userdata;
}

void canvas_hot_set_tool(const char *name)
{
    snprintf(g_hot_tool, sizeof(g_hot_tool), "%s", name ? name : "");
}

void canvas_hot_status(Canvas *c, const char *msg, float seconds)
{
    if (!msg)
        msg = "";
    snprintf(c->hot_msg, sizeof(c->hot_msg), "%s", msg);
    c->hot_msg_t = seconds;
    if (msg[0])
        fprintf(stderr, "canvas: %s\n", msg);
}

int canvas_hot_active(void)
{
    return g_hot_fn != NULL;
}

void canvas_hot_tick(Canvas *c, Game *game, void **state)
{
    int reset, reload;

    if (canvas_key_pressed(c, KEY_F1)) {
        c->hot_hud = !c->hot_hud;
        if (c->hot_hud && !c->hot_msg[0])
            snprintf(c->hot_msg, sizeof(c->hot_msg), "ready");
    }
    if (c->hot_msg_t > 0.0f) {
        c->hot_msg_t -= 1.0f / 60.0f;
        if (c->hot_msg_t <= 0.0f && g_hot_fn)
            snprintf(c->hot_msg, sizeof(c->hot_msg), "ready");
        else if (c->hot_msg_t <= 0.0f)
            c->hot_msg[0] = 0;
    }
    reload = canvas_key_pressed(c, KEY_F5);
    reset = canvas_key_pressed(c, KEY_F6);
    if (!reload && !reset)
        return;
    if (!g_hot_fn) {
        c->hot_hud = 1;
        canvas_hot_status(c, "static build — use ./plat after rebuild", 6.0f);
        return;
    }
    c->hot_hud = 1;
    canvas_hot_status(c, reset ? "rebuilding (reset)..." : "rebuilding...", 8.0f);
    /* After F5 the live heap may still belong to an unloaded plugin CRT.
     * free() then aborts the process. Only shut down when this plugin allocated. */
    if (reset) {
        if (*state && game->shutdown && !g_hot_kept_state)
            game->shutdown(*state, c);
        *state = NULL;
        canvas_sound_reset(c);
    }
    {
        Game next = *game;
        if (g_hot_fn(g_hot_ud, &next) != 0) {
            if (reset && game->init)
                *state = game->init(c);
            g_hot_kept_state = 0;
            canvas_hot_status(c, "FAILED — see terminal", 8.0f);
            return;
        }
        *game = next;
        if (reset) {
            *state = game->init ? game->init(c) : NULL;
            g_hot_kept_state = 0;
        } else {
            if (game->hot_reload && *state)
                game->hot_reload(*state, c);
            g_hot_kept_state = 1;
        }
        canvas_hot_status(c, reset ? "reloaded + reset" : "reloaded, state kept", 6.0f);
    }
}

static void hot_draw_line(Canvas *c, float x, float y, const char *s, float r, float g, float b)
{
    canvas_draw_text(c, x, y, s, r, g, b);
}

void canvas_hot_overlay(Canvas *c)
{
    const char *status, *line3;
    char line2[48];
    float inst, pw = 268.0f, ph = 52.0f, px, py;
    int alert;

    if (!c->hot_hud)
        return;
    if (!g_hot_fn && !c->hot_msg[0])
        return;

    inst = 1.0f / (c->frame_dt > 1e-6f ? c->frame_dt : 1e-6f);
    if (c->fps <= 1.0f)
        c->fps = inst;
    else
        c->fps += (inst - c->fps) * 0.12f;
    snprintf(line2, sizeof(line2), "F5 reload  F6 reset  %.0f fps", (double)c->fps);

    status = c->hot_msg[0] ? c->hot_msg : "ready";
    alert = (strstr(status, "FAIL") != NULL) || (strstr(status, "rebuild") != NULL) ||
            (strstr(status, "reload") != NULL);
    line3 = g_hot_tool[0] ? g_hot_tool : "F1 hide overlay";
    px = (float)c->width - pw - 10.0f;
    py = 8.0f;
    if (px < 8.0f)
        px = 8.0f;

    canvas_begin_hud(c);
    canvas_fill_rect(c, px, py, pw, ph, 0.04f, 0.05f, 0.07f, 0.42f);
    canvas_stroke_rect(c, px, py, pw, ph, 1.0f, 0.85f, 0.25f, 0.35f);
    hot_draw_line(c, px + 8, py + 16, status, alert ? 1.0f : 0.85f, alert ? 0.88f : 0.9f,
                  alert ? 0.3f : 0.55f);
    hot_draw_line(c, px + 8, py + 30, line2, 0.75f, 0.78f, 0.7f);
    hot_draw_line(c, px + 8, py + 44, line3, 0.55f, 0.58f, 0.52f);
    canvas_end_hud(c);
}
