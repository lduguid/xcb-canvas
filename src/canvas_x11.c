#define _POSIX_C_SOURCE 200809L

#include "canvas_internal.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <time.h>

#include <alsa/asoundlib.h>

#include <X11/Xlib.h>
#include <X11/XKBlib.h>
#include <X11/keysym.h>
#include <xcb/xcb.h>
#include <GL/glx.h>

extern xcb_connection_t *XGetXCBConnection(Display *dpy);
extern void XSetEventQueueOwner(Display *dpy, int owner);
#ifndef XCBOwnsEventQueue
#define XCBOwnsEventQueue 1
#endif

typedef struct {
    Display *dpy;
    xcb_connection_t *conn;
    xcb_window_t win;
    xcb_screen_t *screen;
    GLXContext glctx;
    xcb_intern_atom_reply_t *wm_delete;
    xcb_intern_atom_reply_t *wm_proto;
    Canvas *canvas;
    snd_pcm_t *pcm;
    pthread_t audio_th;
    int audio_run;
} X11Plat;

static void *audio_thread(void *arg)
{
    X11Plat *p = arg;
    short buf[512];

    while (p->audio_run) {
        snd_pcm_sframes_t n;
        canvas_mix(p->canvas, buf, 512);
        n = snd_pcm_writei(p->pcm, buf, 512);
        if (n < 0)
            snd_pcm_recover(p->pcm, (int)n, 1);
    }
    return NULL;
}

void canvas_plat_audio_start(Canvas *c)
{
    X11Plat *p = c->plat;
    int err;

    p->canvas = c;
    p->audio_run = 0;
    p->pcm = NULL;
    err = snd_pcm_open(&p->pcm, "default", SND_PCM_STREAM_PLAYBACK, 0);
    if (err < 0)
        return;
    err = snd_pcm_set_params(p->pcm, SND_PCM_FORMAT_S16_LE, SND_PCM_ACCESS_RW_INTERLEAVED, 1,
                             CANVAS_SND_RATE, 1, 40000);
    if (err < 0) {
        snd_pcm_close(p->pcm);
        p->pcm = NULL;
        return;
    }
    p->audio_run = 1;
    if (pthread_create(&p->audio_th, NULL, audio_thread, p) != 0) {
        p->audio_run = 0;
        snd_pcm_close(p->pcm);
        p->pcm = NULL;
    }
}

void canvas_plat_audio_stop(Canvas *c)
{
    X11Plat *p = c->plat;
    if (!p->audio_run && !p->pcm)
        return;
    p->audio_run = 0;
    if (p->pcm)
        snd_pcm_drop(p->pcm);
    if (p->audio_th)
        pthread_join(p->audio_th, NULL);
    p->audio_th = 0;
    if (p->pcm)
        snd_pcm_close(p->pcm);
    p->pcm = NULL;
}

static double now_sec(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec + t.tv_nsec / 1000000000.0;
}

void canvas_plat_set_title(Canvas *c, const char *title)
{
    X11Plat *p = c->plat;
    if (!p)
        return;
    xcb_change_property(p->conn, XCB_PROP_MODE_REPLACE, p->win, XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 8,
                        (uint32_t)strlen(title), title);
    xcb_flush(p->conn);
}

static CanvasKey keysym_to_key(KeySym ks)
{
    if (ks == XK_Left || ks == XK_KP_Left)
        return KEY_LEFT;
    if (ks == XK_Right || ks == XK_KP_Right)
        return KEY_RIGHT;
    if (ks == XK_Up || ks == XK_KP_Up)
        return KEY_UP;
    if (ks == XK_Down || ks == XK_KP_Down)
        return KEY_DOWN;
    if (ks == XK_space)
        return KEY_SPACE;
    if (ks == XK_Return || ks == XK_KP_Enter)
        return KEY_ENTER;
    if (ks == XK_Escape)
        return KEY_ESCAPE;
    if (ks == XK_Tab)
        return KEY_TAB;
    if (ks == XK_Shift_L || ks == XK_Shift_R)
        return KEY_SHIFT;
    if (ks == XK_Control_L || ks == XK_Control_R)
        return KEY_CTRL;
    if (ks == XK_Alt_L || ks == XK_Alt_R)
        return KEY_ALT;
    if (ks >= XK_a && ks <= XK_z)
        return (CanvasKey)(KEY_A + (int)(ks - XK_a));
    if (ks >= XK_A && ks <= XK_Z)
        return (CanvasKey)(KEY_A + (int)(ks - XK_A));
    if (ks >= XK_0 && ks <= XK_9)
        return (CanvasKey)(KEY_0 + (int)(ks - XK_0));
    return KEY_UNKNOWN;
}

static CanvasKey event_key(X11Plat *p, xcb_keycode_t code)
{
    CanvasKey k = keysym_to_key(XkbKeycodeToKeysym(p->dpy, code, 0, 0));
    if (k != KEY_UNKNOWN)
        return k;
    if (code == 113)
        return KEY_LEFT;
    if (code == 114)
        return KEY_RIGHT;
    if (code == 111)
        return KEY_UP;
    if (code == 116)
        return KEY_DOWN;
    return KEY_UNKNOWN;
}

static void take_focus(X11Plat *p)
{
    xcb_set_input_focus(p->conn, XCB_INPUT_FOCUS_POINTER_ROOT, p->win, XCB_CURRENT_TIME);
}

static void handle_event(Canvas *c, xcb_generic_event_t *ev)
{
    X11Plat *p = c->plat;

    switch (ev->response_type & ~0x80) {
    case XCB_MAP_NOTIFY:
    case XCB_ENTER_NOTIFY:
        take_focus(p);
        break;
    case XCB_CONFIGURE_NOTIFY: {
        xcb_configure_notify_event_t *cfg = (xcb_configure_notify_event_t *)ev;
        if (cfg->width > 0 && cfg->height > 0 && (cfg->width != c->width || cfg->height != c->height)) {
            c->width = cfg->width;
            c->height = cfg->height;
            c->resized = 1;
        }
        break;
    }
    case XCB_KEY_PRESS:
        canvas_set_key(c, event_key(p, ((xcb_key_press_event_t *)ev)->detail), 1);
        break;
    case XCB_KEY_RELEASE:
        canvas_set_key(c, event_key(p, ((xcb_key_release_event_t *)ev)->detail), 0);
        break;
    case XCB_BUTTON_PRESS: {
        xcb_button_press_event_t *bp = (xcb_button_press_event_t *)ev;
        take_focus(p);
        if (bp->detail == 4)
            c->wheel += 1.0f;
        else if (bp->detail == 5)
            c->wheel -= 1.0f;
        else if (bp->detail == 6)
            c->wheel_h -= 1.0f;
        else if (bp->detail == 7)
            c->wheel_h += 1.0f;
        else if (bp->detail >= 1 && bp->detail <= 3) {
            c->mouse_down[bp->detail] = 1;
            c->mouse_pressed[bp->detail] = 1;
        }
        c->mouse_x = bp->event_x;
        c->mouse_y = bp->event_y;
        break;
    }
    case XCB_BUTTON_RELEASE: {
        xcb_button_release_event_t *br = (xcb_button_release_event_t *)ev;
        if (br->detail >= 1 && br->detail <= 3)
            c->mouse_down[br->detail] = 0;
        break;
    }
    case XCB_MOTION_NOTIFY: {
        xcb_motion_notify_event_t *mv = (xcb_motion_notify_event_t *)ev;
        c->mouse_x = mv->event_x;
        c->mouse_y = mv->event_y;
        break;
    }
    case XCB_CLIENT_MESSAGE: {
        xcb_client_message_event_t *cm = (xcb_client_message_event_t *)ev;
        if (p->wm_delete && cm->data.data32[0] == p->wm_delete->atom)
            c->running = 0;
        break;
    }
    case XCB_DESTROY_NOTIFY:
        c->running = 0;
        break;
    default:
        break;
    }
}

static XVisualInfo *choose_visual(Display *dpy)
{
    int msaa[] = {
        GLX_RGBA, GLX_DOUBLEBUFFER, GLX_RED_SIZE, 8, GLX_GREEN_SIZE, 8, GLX_BLUE_SIZE, 8,
        GLX_ALPHA_SIZE, 8, GLX_DEPTH_SIZE, 16, GLX_SAMPLE_BUFFERS, 1, GLX_SAMPLES, 4, None,
    };
    XVisualInfo *vi = glXChooseVisual(dpy, DefaultScreen(dpy), msaa);
    if (vi)
        return vi;
    {
        int basic[] = {
            GLX_RGBA, GLX_DOUBLEBUFFER, GLX_RED_SIZE, 8, GLX_GREEN_SIZE, 8, GLX_BLUE_SIZE, 8,
            GLX_ALPHA_SIZE, 8, GLX_DEPTH_SIZE, 16, None,
        };
        return glXChooseVisual(dpy, DefaultScreen(dpy), basic);
    }
}

static void set_size_hints(X11Plat *p, int w, int h)
{
    enum { HINT_P_SIZE = 8, HINT_P_MIN_SIZE = 16, HINT_P_BASE_SIZE = 256, HINT_P_WIN_GRAVITY = 512 };
    struct {
        uint32_t flags;
        int32_t x, y, width, height;
        int32_t min_width, min_height;
        int32_t max_width, max_height;
        int32_t width_inc, height_inc;
        int32_t min_aspect_num, min_aspect_den;
        int32_t max_aspect_num, max_aspect_den;
        int32_t base_width, base_height;
        uint32_t win_gravity;
    } hints;
    xcb_intern_atom_cookie_t nh = xcb_intern_atom(p->conn, 0, 15, "WM_NORMAL_HINTS");
    xcb_intern_atom_cookie_t sh = xcb_intern_atom(p->conn, 0, 13, "WM_SIZE_HINTS");
    xcb_intern_atom_reply_t *normal = xcb_intern_atom_reply(p->conn, nh, NULL);
    xcb_intern_atom_reply_t *size = xcb_intern_atom_reply(p->conn, sh, NULL);

    memset(&hints, 0, sizeof(hints));
    hints.flags = HINT_P_SIZE | HINT_P_MIN_SIZE | HINT_P_BASE_SIZE | HINT_P_WIN_GRAVITY;
    hints.width = w;
    hints.height = h;
    hints.min_width = 320;
    hints.min_height = 240;
    hints.base_width = 320;
    hints.base_height = 240;
    hints.win_gravity = XCB_GRAVITY_NORTH_WEST;
    if (normal && size)
        xcb_change_property(p->conn, XCB_PROP_MODE_REPLACE, p->win, normal->atom, size->atom, 32,
                            sizeof(hints) / 4, &hints);
    free(normal);
    free(size);
}

int canvas_run(const Game *game)
{
    Canvas c;
    X11Plat plat;
    void *state = NULL;
    XVisualInfo *vi;
    int fd;
    double prev, t, dt;
    const char *title;

    if (!game) {
        fprintf(stderr, "canvas_run: no game\n");
        return 1;
    }

    memset(&plat, 0, sizeof(plat));
    canvas_core_reset(&c, game->width, game->height);
    c.plat = &plat;

    plat.dpy = XOpenDisplay(NULL);
    if (!plat.dpy) {
        fprintf(stderr, "cannot open X display\n");
        return 1;
    }
    plat.conn = XGetXCBConnection(plat.dpy);
    if (!plat.conn || xcb_connection_has_error(plat.conn)) {
        fprintf(stderr, "cannot get XCB connection\n");
        return 1;
    }
    XSetEventQueueOwner(plat.dpy, XCBOwnsEventQueue);
    XkbSetDetectableAutoRepeat(plat.dpy, True, NULL);

    vi = choose_visual(plat.dpy);
    if (!vi) {
        fprintf(stderr, "no double-buffered GLX visual\n");
        return 1;
    }

    plat.screen = xcb_setup_roots_iterator(xcb_get_setup(plat.conn)).data;
    plat.win = xcb_generate_id(plat.conn);
    {
        xcb_colormap_t cmap = xcb_generate_id(plat.conn);
        uint32_t mask = XCB_CW_BACK_PIXMAP | XCB_CW_EVENT_MASK | XCB_CW_COLORMAP;
        uint32_t values[] = {
            XCB_BACK_PIXMAP_NONE,
            XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_KEY_PRESS | XCB_EVENT_MASK_KEY_RELEASE |
                XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE | XCB_EVENT_MASK_POINTER_MOTION |
                XCB_EVENT_MASK_ENTER_WINDOW | XCB_EVENT_MASK_FOCUS_CHANGE |
                XCB_EVENT_MASK_STRUCTURE_NOTIFY,
            cmap,
        };
        xcb_create_colormap(plat.conn, XCB_COLORMAP_ALLOC_NONE, cmap, plat.screen->root,
                            (xcb_visualid_t)vi->visualid);
        xcb_create_window(plat.conn, (uint8_t)vi->depth, plat.win, plat.screen->root, 80, 60,
                          (uint16_t)c.width, (uint16_t)c.height, 0, XCB_WINDOW_CLASS_INPUT_OUTPUT,
                          (xcb_visualid_t)vi->visualid, mask, values);
    }

    title = game->name ? game->name : "Canvas";
    canvas_plat_set_title(&c, title);
    set_size_hints(&plat, c.width, c.height);

    {
        xcb_intern_atom_cookie_t proto_c = xcb_intern_atom(plat.conn, 1, 12, "WM_PROTOCOLS");
        xcb_intern_atom_cookie_t del_c = xcb_intern_atom(plat.conn, 0, 16, "WM_DELETE_WINDOW");
        plat.wm_proto = xcb_intern_atom_reply(plat.conn, proto_c, NULL);
        plat.wm_delete = xcb_intern_atom_reply(plat.conn, del_c, NULL);
        if (plat.wm_proto && plat.wm_delete)
            xcb_change_property(plat.conn, XCB_PROP_MODE_REPLACE, plat.win, plat.wm_proto->atom,
                                XCB_ATOM_ATOM, 32, 1, &plat.wm_delete->atom);
    }

    xcb_map_window(plat.conn, plat.win);
    take_focus(&plat);
    xcb_flush(plat.conn);

    plat.glctx = glXCreateContext(plat.dpy, vi, NULL, True);
    XFree(vi);
    if (!plat.glctx || !glXMakeCurrent(plat.dpy, plat.win, plat.glctx)) {
        fprintf(stderr, "cannot create GLX context\n");
        return 1;
    }
    {
        typedef int (*SwapInt)(Display *, GLXDrawable, int);
        SwapInt fn = (SwapInt)glXGetProcAddressARB((const GLubyte *)"glXSwapIntervalEXT");
        if (fn)
            fn(plat.dpy, plat.win, 1);
    }

    canvas_core_gl_setup(&c);
    canvas_sound_init(&c);
    canvas_plat_audio_start(&c);
    if (game->init)
        state = game->init(&c);

    fd = xcb_get_file_descriptor(plat.conn);
    prev = now_sec();

    while (c.running) {
        xcb_generic_event_t *ev;
        fd_set fds;
        struct timeval tv = {0, 0};

        while ((ev = xcb_poll_for_event(plat.conn))) {
            handle_event(&c, ev);
            free(ev);
        }
        FD_ZERO(&fds);
        FD_SET(fd, &fds);
        select(fd + 1, &fds, NULL, NULL, &tv);
        while ((ev = xcb_poll_for_event(plat.conn))) {
            handle_event(&c, ev);
            free(ev);
        }

        t = now_sec();
        dt = t - prev;
        prev = t;
        if (dt < 0.0)
            dt = 0.0;
        if (dt > 0.05)
            dt = 0.05;
        c.time += (float)dt;

        if (game->update)
            game->update(state, &c, (float)dt);
        canvas_apply_camera(&c, (float)dt);
        canvas_set_world_proj(&c);
        if (game->render)
            game->render(state, &c);
        glXSwapBuffers(plat.dpy, plat.win);
        canvas_clear_edges(&c);
    }

    if (game->shutdown)
        game->shutdown(state, &c);
    canvas_plat_audio_stop(&c);
    canvas_sound_shutdown(&c);

    glXMakeCurrent(plat.dpy, None, NULL);
    glXDestroyContext(plat.dpy, plat.glctx);
    free(plat.wm_proto);
    free(plat.wm_delete);
    XCloseDisplay(plat.dpy);
    return 0;
}
