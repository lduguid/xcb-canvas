#ifdef _WIN32

#include "canvas_internal.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <windowsx.h>
#include <mmsystem.h>
#include <GL/gl.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WIN_SND_BUFS 4
#define WIN_SND_FRAMES 1024

typedef struct {
    HWND hwnd;
    HDC hdc;
    HGLRC hglrc;
    Canvas *canvas;
    HWAVEOUT wo;
    HANDLE snd_ev;
    HANDLE snd_th;
    WAVEHDR hdr[WIN_SND_BUFS];
    short pcm[WIN_SND_BUFS][WIN_SND_FRAMES];
    volatile int audio_run;
} WinPlat;

static DWORD WINAPI audio_thread(LPVOID arg)
{
    WinPlat *p = (WinPlat *)arg;
    while (p->audio_run) {
        int i;
        WaitForSingleObject(p->snd_ev, 80);
        if (!p->audio_run)
            break;
        for (i = 0; i < WIN_SND_BUFS; i++) {
            if (p->hdr[i].dwFlags & WHDR_DONE) {
                canvas_mix(p->canvas, p->pcm[i], WIN_SND_FRAMES);
                waveOutWrite(p->wo, &p->hdr[i], sizeof(WAVEHDR));
            }
        }
    }
    return 0;
}

void canvas_plat_audio_start(Canvas *c)
{
    WinPlat *p = (WinPlat *)c->plat;
    WAVEFORMATEX fmt;
    int i;

    p->canvas = c;
    p->audio_run = 0;
    p->wo = NULL;
    p->snd_ev = CreateEventA(NULL, FALSE, FALSE, NULL);
    if (!p->snd_ev)
        return;
    memset(&fmt, 0, sizeof(fmt));
    fmt.wFormatTag = WAVE_FORMAT_PCM;
    fmt.nChannels = 1;
    fmt.nSamplesPerSec = CANVAS_SND_RATE;
    fmt.wBitsPerSample = 16;
    fmt.nBlockAlign = 2;
    fmt.nAvgBytesPerSec = CANVAS_SND_RATE * 2;
    if (waveOutOpen(&p->wo, WAVE_MAPPER, &fmt, (DWORD_PTR)p->snd_ev, 0, CALLBACK_EVENT) != MMSYSERR_NOERROR) {
        CloseHandle(p->snd_ev);
        p->snd_ev = NULL;
        p->wo = NULL;
        return;
    }
    memset(p->hdr, 0, sizeof(p->hdr));
    for (i = 0; i < WIN_SND_BUFS; i++) {
        p->hdr[i].lpData = (LPSTR)p->pcm[i];
        p->hdr[i].dwBufferLength = WIN_SND_FRAMES * sizeof(short);
        waveOutPrepareHeader(p->wo, &p->hdr[i], sizeof(WAVEHDR));
        canvas_mix(c, p->pcm[i], WIN_SND_FRAMES);
        waveOutWrite(p->wo, &p->hdr[i], sizeof(WAVEHDR));
    }
    p->audio_run = 1;
    p->snd_th = CreateThread(NULL, 0, audio_thread, p, 0, NULL);
    if (!p->snd_th)
        p->audio_run = 0;
}

void canvas_plat_audio_stop(Canvas *c)
{
    WinPlat *p = (WinPlat *)c->plat;
    int i;

    p->audio_run = 0;
    if (p->snd_ev)
        SetEvent(p->snd_ev);
    if (p->snd_th) {
        WaitForSingleObject(p->snd_th, 1000);
        CloseHandle(p->snd_th);
        p->snd_th = NULL;
    }
    if (p->wo) {
        waveOutReset(p->wo);
        for (i = 0; i < WIN_SND_BUFS; i++)
            waveOutUnprepareHeader(p->wo, &p->hdr[i], sizeof(WAVEHDR));
        waveOutClose(p->wo);
        p->wo = NULL;
    }
    if (p->snd_ev) {
        CloseHandle(p->snd_ev);
        p->snd_ev = NULL;
    }
}

static double now_sec(void)
{
    static LARGE_INTEGER freq;
    LARGE_INTEGER t;
    if (!freq.QuadPart)
        QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&t);
    return (double)t.QuadPart / (double)freq.QuadPart;
}

void canvas_plat_set_title(Canvas *c, const char *title)
{
    WinPlat *p = c->plat;
    wchar_t wide[512];
    int n;

    if (!p || !p->hwnd)
        return;
    n = MultiByteToWideChar(CP_UTF8, 0, title, -1, wide, 512);
    if (n > 0)
        SetWindowTextW(p->hwnd, wide);
    else
        SetWindowTextA(p->hwnd, title);
}

static CanvasKey vk_to_key(WPARAM vk)
{
    switch (vk) {
    case VK_LEFT:
        return KEY_LEFT;
    case VK_RIGHT:
        return KEY_RIGHT;
    case VK_UP:
        return KEY_UP;
    case VK_DOWN:
        return KEY_DOWN;
    case VK_SPACE:
        return KEY_SPACE;
    case VK_RETURN:
        return KEY_ENTER;
    case VK_ESCAPE:
        return KEY_ESCAPE;
    case VK_TAB:
        return KEY_TAB;
    case VK_SHIFT:
    case VK_LSHIFT:
    case VK_RSHIFT:
        return KEY_SHIFT;
    case VK_CONTROL:
    case VK_LCONTROL:
    case VK_RCONTROL:
        return KEY_CTRL;
    case VK_MENU:
        return KEY_ALT;
    default:
        break;
    }
    if (vk >= 'A' && vk <= 'Z')
        return (CanvasKey)(KEY_A + (int)(vk - 'A'));
    if (vk >= '0' && vk <= '9')
        return (CanvasKey)(KEY_0 + (int)(vk - '0'));
    if (vk == VK_F1)
        return KEY_F1;
    if (vk == VK_F5)
        return KEY_F5;
    if (vk == VK_F6)
        return KEY_F6;
    return KEY_UNKNOWN;
}

static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    WinPlat *p = (WinPlat *)GetWindowLongPtrA(hwnd, GWLP_USERDATA);
    Canvas *c = p ? p->canvas : NULL;

    switch (msg) {
    case WM_CLOSE:
        if (c)
            c->running = 0;
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        if (c)
            c->running = 0;
        return 0;
    case WM_SIZE:
        if (c && wparam != SIZE_MINIMIZED) {
            int w = LOWORD(lparam), h = HIWORD(lparam);
            if (w > 0 && h > 0 && (w != c->width || h != c->height)) {
                c->width = w;
                c->height = h;
                c->resized = 1;
            }
        }
        return 0;
    case WM_KILLFOCUS:
        if (c)
            canvas_keys_release_all(c);
        return 0;
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        if (c)
            canvas_set_key(c, vk_to_key(wparam), 1);
        return 0;
    case WM_KEYUP:
    case WM_SYSKEYUP:
        if (c)
            canvas_set_key(c, vk_to_key(wparam), 0);
        return 0;
    case WM_LBUTTONDOWN:
    case WM_MBUTTONDOWN:
    case WM_RBUTTONDOWN:
        if (c) {
            int b = (msg == WM_LBUTTONDOWN) ? 1 : (msg == WM_RBUTTONDOWN) ? 3 : 2;
            SetCapture(hwnd);
            c->mouse_down[b] = 1;
            c->mouse_pressed[b] = 1;
            c->mouse_x = GET_X_LPARAM(lparam);
            c->mouse_y = GET_Y_LPARAM(lparam);
        }
        return 0;
    case WM_LBUTTONUP:
    case WM_MBUTTONUP:
    case WM_RBUTTONUP:
        if (c) {
            int b = (msg == WM_LBUTTONUP) ? 1 : (msg == WM_RBUTTONUP) ? 3 : 2;
            c->mouse_down[b] = 0;
            if (!c->mouse_down[1] && !c->mouse_down[2] && !c->mouse_down[3])
                ReleaseCapture();
        }
        return 0;
    case WM_MOUSEMOVE:
        if (c) {
            c->mouse_x = GET_X_LPARAM(lparam);
            c->mouse_y = GET_Y_LPARAM(lparam);
        }
        return 0;
    case WM_MOUSEWHEEL:
        if (c)
            c->wheel += (float)GET_WHEEL_DELTA_WPARAM(wparam) / (float)WHEEL_DELTA;
        return 0;
#ifdef WM_MOUSEHWHEEL
    case WM_MOUSEHWHEEL:
        if (c)
            c->wheel_h += (float)GET_WHEEL_DELTA_WPARAM(wparam) / (float)WHEEL_DELTA;
        return 0;
#endif
    case WM_SETFOCUS:
        return 0;
    default:
        return DefWindowProcA(hwnd, msg, wparam, lparam);
    }
}

static int create_gl(WinPlat *p, int width, int height, const char *title)
{
    WNDCLASSEXA wc;
    RECT rc;
    PIXELFORMATDESCRIPTOR pfd;
    int fmt;
    DWORD style = WS_OVERLAPPEDWINDOW;
    HINSTANCE inst = GetModuleHandleA(NULL);

    memset(&wc, 0, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = wnd_proc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = "CanvasGL";
    RegisterClassExA(&wc);

    rc.left = 0;
    rc.top = 0;
    rc.right = width;
    rc.bottom = height;
    AdjustWindowRect(&rc, style, FALSE);

    p->hwnd = CreateWindowExA(0, "CanvasGL", title ? title : "Canvas", style, CW_USEDEFAULT, CW_USEDEFAULT,
                              rc.right - rc.left, rc.bottom - rc.top, NULL, NULL, inst, NULL);
    if (!p->hwnd)
        return 0;
    SetWindowLongPtrA(p->hwnd, GWLP_USERDATA, (LONG_PTR)p);

    p->hdc = GetDC(p->hwnd);
    memset(&pfd, 0, sizeof(pfd));
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 24;
    pfd.cAlphaBits = 8;
    pfd.cDepthBits = 16;
    pfd.iLayerType = PFD_MAIN_PLANE;
    fmt = ChoosePixelFormat(p->hdc, &pfd);
    if (!fmt || !SetPixelFormat(p->hdc, fmt, &pfd))
        return 0;
    p->hglrc = wglCreateContext(p->hdc);
    if (!p->hglrc || !wglMakeCurrent(p->hdc, p->hglrc))
        return 0;

    {
        typedef BOOL(WINAPI * SwapInt)(int);
        SwapInt fn = (SwapInt)(void *)wglGetProcAddress("wglSwapIntervalEXT");
        if (fn)
            fn(1);
    }
    return 1;
}

int canvas_run(const Game *game)
{
    Canvas c;
    WinPlat plat;
    Game g;
    void *state = NULL;
    double prev, t, dt;
    const char *title;

    if (!game) {
        fprintf(stderr, "canvas_run: no game\n");
        return 1;
    }
    g = *game;

    memset(&plat, 0, sizeof(plat));
    canvas_core_reset(&c, g.width, g.height);
    plat.canvas = &c;
    c.plat = &plat;
    canvas_session_begin(&c, g.name);

    title = g.name ? g.name : "Canvas";
    if (!create_gl(&plat, c.width, c.height, title)) {
        fprintf(stderr, "cannot create Win32 OpenGL window\n");
        return 1;
    }

    ShowWindow(plat.hwnd, SW_SHOW);
    UpdateWindow(plat.hwnd);
    SetForegroundWindow(plat.hwnd);
    SetFocus(plat.hwnd);

    canvas_core_gl_setup(&c);
    canvas_sound_init(&c);
    canvas_plat_audio_start(&c);
    if (g.init)
        state = g.init(&c);
    if (canvas_hot_active())
        canvas_hot_status(&c, "ready", 4.0f);

    prev = now_sec();
    while (c.running) {
        MSG msg;
        while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                c.running = 0;
                break;
            }
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }

        t = now_sec();
        dt = t - prev;
        prev = t;
        if (dt < 0.0)
            dt = 0.0;
        canvas_session_pump(&c, &g, &state, (float)dt);
        canvas_set_world_proj(&c);
        if (g.render)
            g.render(state, &c);
        canvas_hot_overlay(&c);
        canvas_session_overlay(&c);
        SwapBuffers(plat.hdc);
        if (c.lock_dt <= 0.0f)
            canvas_clear_edges(&c);
    }

    canvas_session_end(&c);
    if (g.shutdown)
        g.shutdown(state, &c);
    canvas_plat_audio_stop(&c);
    canvas_sound_shutdown(&c);

    wglMakeCurrent(NULL, NULL);
    if (plat.hglrc)
        wglDeleteContext(plat.hglrc);
    if (plat.hdc && plat.hwnd)
        ReleaseDC(plat.hwnd, plat.hdc);
    if (plat.hwnd)
        DestroyWindow(plat.hwnd);
    return 0;
}

#endif /* _WIN32 */
