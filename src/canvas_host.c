#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif

#include "canvas_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#define LIB_EXT ".dll"
#define DIR_SEP '\\'
#else
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#define LIB_EXT ".so"
#define DIR_SEP '/'
#define HMODULE void *
#endif

typedef struct {
    char name[64];
    char built[256];
    char loaded[256];
    HMODULE lib;
    int gen;
} HotHost;

static void chdir_to_exe(void)
{
#ifdef _WIN32
    char buf[MAX_PATH], *slash;
    if (!GetModuleFileNameA(NULL, buf, MAX_PATH))
        return;
    slash = strrchr(buf, '\\');
    if (slash) {
        *slash = 0;
        SetCurrentDirectoryA(buf);
    }
#else
    char buf[4096], *slash;
    ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (n <= 0)
        return;
    buf[n] = 0;
    slash = strrchr(buf, '/');
    if (slash) {
        *slash = 0;
        if (chdir(buf) != 0)
            (void)errno;
    }
#endif
}

static int copy_file(const char *src, const char *dst)
{
#ifdef _WIN32
    return CopyFileA(src, dst, FALSE) ? 0 : -1;
#else
    int in, out;
    char buf[8192];
    ssize_t n;

    in = open(src, O_RDONLY);
    if (in < 0)
        return -1;
    out = open(dst, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (out < 0) {
        close(in);
        return -1;
    }
    while ((n = read(in, buf, sizeof(buf))) > 0) {
        if (write(out, buf, (size_t)n) != n) {
            close(in);
            close(out);
            return -1;
        }
    }
    close(in);
    close(out);
    return n < 0 ? -1 : 0;
#endif
}

static void close_lib(HMODULE lib)
{
    if (!lib)
        return;
#ifdef _WIN32
    FreeLibrary(lib);
#else
    dlclose(lib);
#endif
}

static HMODULE open_lib(const char *path)
{
#ifdef _WIN32
    return LoadLibraryA(path);
#else
    return dlopen(path, RTLD_NOW | RTLD_LOCAL);
#endif
}

static const Game *lib_game(HMODULE lib)
{
#ifdef _WIN32
    return (const Game *)GetProcAddress(lib, "canvas_game");
#else
    return (const Game *)dlsym(lib, "canvas_game");
#endif
}

static int file_exists(const char *path)
{
#ifdef _WIN32
    return GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES;
#else
    return access(path, R_OK) == 0;
#endif
}

#ifdef _WIN32
#define CC_MAX 32

typedef enum { CC_KIND_NONE = 0, CC_KIND_MINGW, CC_KIND_MSVC, CC_KIND_WSL } CcKind;

static CcKind cc_kind;
static char cc_path[MAX_PATH];
static char vcvars_path[MAX_PATH];
static char wsl_dir[MAX_PATH];
static char cc_label[80];

static void trim_line(char *s)
{
    char *e;
    size_t i = 0;
    while (s[i] == ' ' || s[i] == '\t' || s[i] == '\r' || s[i] == '\n')
        i++;
    if (i)
        memmove(s, s + i, strlen(s + i) + 1);
    e = s + strlen(s);
    while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r' || e[-1] == '\n'))
        *--e = 0;
}

static int strieq(const char *a, const char *b)
{
    while (*a && *b) {
        int ca = *a++, cb = *b++;
        if (ca >= 'A' && ca <= 'Z')
            ca += 32;
        if (cb >= 'A' && cb <= 'Z')
            cb += 32;
        if (ca != cb)
            return 0;
    }
    return *a == 0 && *b == 0;
}

static int stristr_end(const char *s, const char *suf)
{
    size_t n = strlen(s), m = strlen(suf);
    const char *a, *b;
    if (n < m)
        return 0;
    a = s + n - m;
    b = suf;
    while (*b) {
        int ca = *a++, cb = *b++;
        if (ca >= 'A' && ca <= 'Z')
            ca += 32;
        if (cb >= 'A' && cb <= 'Z')
            cb += 32;
        if (ca != cb)
            return 0;
    }
    return 1;
}

/* CANVAS_CC=mingw|msvc|wsl|/path/gcc.exe  or a one-line canvas-cc file. */
static void read_cc_pref(char *out, size_t n)
{
    const char *e = getenv("CANVAS_CC");
    FILE *f;

    out[0] = 0;
    if (e && e[0]) {
        snprintf(out, n, "%s", e);
        trim_line(out);
        if (out[0])
            return;
    }
    f = fopen("canvas-cc", "r");
    if (!f)
        f = fopen("canvas-cc.txt", "r");
    if (!f)
        return;
    if (fgets(out, (int)n, f))
        trim_line(out);
    else
        out[0] = 0;
    fclose(f);
}

static CcKind parse_cc_pref(const char *pref, char *path, size_t pathn)
{
    path[0] = 0;
    if (!pref || !pref[0] || strieq(pref, "auto"))
        return CC_KIND_NONE;
    if (strieq(pref, "mingw") || strieq(pref, "mingw64") || strieq(pref, "gcc") ||
        strieq(pref, "winlibs"))
        return CC_KIND_MINGW;
    if (strieq(pref, "msvc") || strieq(pref, "cl") || strieq(pref, "vs") ||
        strieq(pref, "visualstudio") || strieq(pref, "vs2022"))
        return CC_KIND_MSVC;
    if (strieq(pref, "wsl"))
        return CC_KIND_WSL;
    snprintf(path, pathn, "%s", pref);
    if (stristr_end(pref, "cl.exe") || stristr_end(pref, "\\cl") || stristr_end(pref, "/cl"))
        return CC_KIND_MSVC;
    return CC_KIND_MINGW;
}

static void slash_unix(char *s)
{
    for (; *s; s++) {
        if (*s == '\\')
            *s = '/';
    }
}

/* Y:\home\luke\proj and \\wsl.localhost\Distro\home\luke\proj → /home/luke/proj */
static int win_to_wsl(const char *win, char *out, size_t n)
{
    char buf[MAX_PATH];
    const char *p;

    if (!win || !win[0])
        return 0;
    snprintf(buf, sizeof(buf), "%s", win);

    if (buf[0] == '\\' && buf[1] == '\\') {
        p = buf + 2;
        while (*p && *p != '\\')
            p++;
        if (*p == '\\')
            p++;
        while (*p && *p != '\\')
            p++;
        if (*p != '\\')
            return 0;
        snprintf(out, n, "%s", p);
        slash_unix(out);
        return out[0] == '/';
    }

    if (((buf[0] >= 'A' && buf[0] <= 'Z') || (buf[0] >= 'a' && buf[0] <= 'z'))
        && buf[1] == ':' && (buf[2] == '\\' || buf[2] == '/')) {
        p = buf + 2;
        if ((p[0] == '\\' || p[0] == '/')
            && (p[1] == 'h' || p[1] == 'H')
            && (p[2] == 'o' || p[2] == 'O')
            && (p[3] == 'm' || p[3] == 'M')
            && (p[4] == 'e' || p[4] == 'E')
            && (p[5] == '\\' || p[5] == '/' || p[5] == 0)) {
            snprintf(out, n, "%s", p);
            slash_unix(out);
            return 1;
        }
        snprintf(out, n, "/mnt/%c/%s",
                 (char)((buf[0] >= 'A' && buf[0] <= 'Z') ? buf[0] - 'A' + 'a' : buf[0]),
                 buf + 3);
        slash_unix(out);
        return 1;
    }
    return 0;
}

static void cc_add(char cands[][MAX_PATH], int *n, const char *path)
{
    int i;
    if (!path || !path[0] || *n >= CC_MAX)
        return;
    if (!file_exists(path))
        return;
    for (i = 0; i < *n; i++) {
        if (strcmp(cands[i], path) == 0)
            return;
    }
    snprintf(cands[*n], MAX_PATH, "%s", path);
    (*n)++;
}

static int cc_is_mingw64(const char *cc)
{
    char cmd[768], mach[128];
    FILE *f;
    int ok = 0;

    CreateDirectoryA(".hot", NULL);
    snprintf(cmd, sizeof(cmd), "\"%s\" -dumpmachine > .hot\\_cc_mach.txt 2>nul", cc);
    if (system(cmd) != 0)
        return 0;
    f = fopen(".hot\\_cc_mach.txt", "r");
    if (!f)
        return 0;
    if (fgets(mach, sizeof(mach), f)
        && strstr(mach, "x86_64")
        && (strstr(mach, "mingw") || strstr(mach, "w64") || strstr(mach, "windows")))
        ok = 1;
    fclose(f);
    if (!ok)
        return 0;

    f = fopen(".hot\\_cc_probe.c", "w");
    if (!f)
        return 0;
    fputs("#include <stdio.h>\n"
          "__declspec(dllexport) int canvas_cc_probe(void) { return 1; }\n", f);
    fclose(f);
    snprintf(cmd, sizeof(cmd),
             "\"%s\" -std=c11 -m64 -shared -o .hot\\_cc_probe.dll .hot\\_cc_probe.c >nul 2>nul",
             cc);
    return system(cmd) == 0;
}

static int find_mingw(const char *forced)
{
    static const char *known[] = {
        "C:\\home\\mingw64\\bin\\gcc.exe",
        "C:\\msys64\\ucrt64\\bin\\gcc.exe",
        "C:\\msys64\\mingw64\\bin\\gcc.exe",
        "C:\\mingw64\\bin\\gcc.exe",
        "C:\\w64devkit\\bin\\gcc.exe",
        "C:\\WinLibs\\mingw64\\bin\\gcc.exe",
        "C:\\ProgramData\\mingw64\\mingw64\\bin\\gcc.exe",
        NULL
    };
    char cands[CC_MAX][MAX_PATH];
    const char *path, *start;
    int n = 0, i;

    if (forced && forced[0]) {
        if (cc_is_mingw64(forced)) {
            snprintf(cc_path, sizeof(cc_path), "%s", forced);
            return 1;
        }
        return 0;
    }

    for (i = 0; known[i]; i++)
        cc_add(cands, &n, known[i]);

    path = getenv("PATH");
    start = path ? path : "";
    while (*start) {
        char dir[MAX_PATH], exe[MAX_PATH];
        const char *semi = strchr(start, ';');
        size_t len = semi ? (size_t)(semi - start) : strlen(start);
        if (len > 0 && len < MAX_PATH) {
            memcpy(dir, start, len);
            dir[len] = 0;
            if (len > 0 && (dir[len - 1] == '\\' || dir[len - 1] == '/'))
                dir[len - 1] = 0;
            snprintf(exe, sizeof(exe), "%s\\x86_64-w64-mingw32-gcc.exe", dir);
            cc_add(cands, &n, exe);
            snprintf(exe, sizeof(exe), "%s\\gcc.exe", dir);
            cc_add(cands, &n, exe);
        }
        start = semi ? semi + 1 : start + strlen(start);
    }

    for (i = 0; i < n; i++) {
        if (cc_is_mingw64(cands[i])) {
            snprintf(cc_path, sizeof(cc_path), "%s", cands[i]);
            return 1;
        }
    }
    return 0;
}

static int find_vcvars(void)
{
    static const char *known[] = {
        "C:\\Program Files\\Microsoft Visual Studio\\2022\\Professional\\VC\\Auxiliary\\Build\\vcvars64.bat",
        "C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\VC\\Auxiliary\\Build\\vcvars64.bat",
        "C:\\Program Files\\Microsoft Visual Studio\\2022\\Enterprise\\VC\\Auxiliary\\Build\\vcvars64.bat",
        "C:\\Program Files\\Microsoft Visual Studio\\2022\\BuildTools\\VC\\Auxiliary\\Build\\vcvars64.bat",
        "C:\\Program Files (x86)\\Microsoft Visual Studio\\2019\\Professional\\VC\\Auxiliary\\Build\\vcvars64.bat",
        "C:\\Program Files (x86)\\Microsoft Visual Studio\\2019\\Community\\VC\\Auxiliary\\Build\\vcvars64.bat",
        "C:\\Program Files (x86)\\Microsoft Visual Studio\\2019\\Enterprise\\VC\\Auxiliary\\Build\\vcvars64.bat",
        "C:\\Program Files (x86)\\Microsoft Visual Studio\\2019\\BuildTools\\VC\\Auxiliary\\Build\\vcvars64.bat",
        NULL
    };
    static const char *vswhere =
        "C:\\Program Files (x86)\\Microsoft Visual Studio\\Installer\\vswhere.exe";
    FILE *f;
    char line[MAX_PATH], cmd[768];
    int i;

    for (i = 0; known[i]; i++) {
        if (file_exists(known[i])) {
            snprintf(vcvars_path, sizeof(vcvars_path), "%s", known[i]);
            return 1;
        }
    }
    if (!file_exists(vswhere))
        return 0;
    CreateDirectoryA(".hot", NULL);
    snprintf(cmd, sizeof(cmd),
             "\"%s\" -latest -products * -find **\\vcvars64.bat > .hot\\_vcvars.txt 2>nul",
             vswhere);
    if (system(cmd) != 0)
        return 0;
    f = fopen(".hot\\_vcvars.txt", "r");
    if (!f)
        return 0;
    if (!fgets(line, sizeof(line), f)) {
        fclose(f);
        return 0;
    }
    fclose(f);
    trim_line(line);
    if (!file_exists(line))
        return 0;
    snprintf(vcvars_path, sizeof(vcvars_path), "%s", line);
    return 1;
}

static int find_wsl_mingw(void)
{
    char cwd[MAX_PATH];
    if (system("wsl -e x86_64-w64-mingw32-gcc -dumpmachine >nul 2>nul") != 0)
        return 0;
    if (!GetCurrentDirectoryA(MAX_PATH, cwd) || !win_to_wsl(cwd, wsl_dir, sizeof(wsl_dir)))
        return 0;
    snprintf(cc_path, sizeof(cc_path), "wsl x86_64-w64-mingw32-gcc");
    return 1;
}

static int select_kind(CcKind want, const char *forced_path)
{
    if (want == CC_KIND_MINGW || want == CC_KIND_NONE) {
        if (find_mingw(want == CC_KIND_MINGW ? forced_path : NULL)) {
            cc_kind = CC_KIND_MINGW;
            snprintf(cc_label, sizeof(cc_label), "F1 hide  mingw");
            return 1;
        }
        if (want == CC_KIND_MINGW)
            return 0;
    }
    if (want == CC_KIND_MSVC || want == CC_KIND_NONE) {
        if (find_vcvars()) {
            cc_kind = CC_KIND_MSVC;
            snprintf(cc_path, sizeof(cc_path), "cl");
            snprintf(cc_label, sizeof(cc_label), "F1 hide  msvc");
            return 1;
        }
        if (want == CC_KIND_MSVC)
            return 0;
    }
    if (want == CC_KIND_WSL || want == CC_KIND_NONE) {
        if (find_wsl_mingw()) {
            cc_kind = CC_KIND_WSL;
            snprintf(cc_label, sizeof(cc_label), "F1 hide  wsl-mingw");
            return 1;
        }
    }
    return 0;
}

static int find_cc_win(void)
{
    static int once, ok;
    char pref[MAX_PATH], forced[MAX_PATH];
    CcKind want;

    if (once)
        return ok;
    once = 1;
    cc_kind = CC_KIND_NONE;
    read_cc_pref(pref, sizeof(pref));
    want = parse_cc_pref(pref, forced, sizeof(forced));
    ok = select_kind(want, forced);
    if (!ok && want != CC_KIND_NONE)
        fprintf(stderr, "hot-reload: CANVAS_CC=%s not found\n", pref);
    return ok;
}
#endif

#ifndef _WIN32
static int cc_works(const char *cc)
{
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "%s -dumpversion >/dev/null 2>&1", cc);
    return system(cmd) == 0;
}
#endif

static const char *find_cc(void)
{
#ifdef _WIN32
    return find_cc_win() ? cc_path : NULL;
#else
    static const char *cands[] = { "gcc", "cc", NULL };
    int i;
    for (i = 0; cands[i]; i++) {
        if (cc_works(cands[i]))
            return cands[i];
    }
    return NULL;
#endif
}

#ifdef _WIN32
static int rebuild_msvc(const char *name)
{
    FILE *bat;

    if (!file_exists("canvas.def")) {
        fprintf(stderr, "hot-reload: missing canvas.def (needed to link MSVC plugins)\n");
        return -1;
    }
    if (!file_exists("canvas.dll")) {
        fprintf(stderr, "hot-reload: missing canvas.dll\n");
        return -1;
    }
    CreateDirectoryA(".hot", NULL);
    bat = fopen(".hot\\rebuild.bat", "w");
    if (!bat) {
        fprintf(stderr, "hot-reload: cannot write .hot\\rebuild.bat\n");
        return -1;
    }
    fprintf(bat,
            "@echo off\n"
            "call \"%s\" >nul\n"
            "if errorlevel 1 exit /b 1\n"
            "lib /nologo /def:canvas.def /machine:x64 /out:canvas.lib\n"
            "if errorlevel 1 exit /b 1\n"
            "cl /nologo /O2 /W3 /DCANVAS_PLUGIN /D_CRT_SECURE_NO_WARNINGS "
            "/Iinclude /Isrc /LD /Fe:%s.dll /Fo.hot\\%s.obj games\\%s.c "
            "/link /INCREMENTAL:NO /IMPLIB:.hot\\%s.lib canvas.lib\n",
            vcvars_path, name, name, name, name);
    fclose(bat);
    fprintf(stderr, "hot-reload: msvc cl /LD games\\%s.c -> %s.dll\n", name, name);
    return system(".hot\\rebuild.bat");
}
#endif

static int rebuild(const char *name)
{
    const char *cc = find_cc();
    char cmd[768];
    int rc;

    if (!cc) {
        fprintf(stderr,
#ifdef _WIN32
                "hot-reload: no compiler.\n"
                "  Set CANVAS_CC=mingw or CANVAS_CC=msvc, or put one of those\n"
                "  words in a canvas-cc file next to the .exe.\n"
                "  MinGW-w64: C:\\home\\mingw64 or winlibs.com / MSYS2.\n"
                "  MSVC: Visual Studio 2022 C++ toolset (vcvars64).\n"
#else
                "hot-reload: no C compiler on PATH (need gcc).\n"
#endif
        );
        return -1;
    }
#ifdef _WIN32
    if (cc_kind == CC_KIND_MSVC) {
        rc = rebuild_msvc(name);
    } else if (cc_kind == CC_KIND_WSL) {
        FILE *sh;
        CreateDirectoryA(".hot", NULL);
        sh = fopen(".hot\\rebuild.sh", "wb");
        if (!sh) {
            fprintf(stderr, "hot-reload: cannot write .hot\\rebuild.sh\n");
            return -1;
        }
        fprintf(sh,
                "#!/bin/bash\nset -e\ncd '%s'\n"
                "x86_64-w64-mingw32-gcc -std=c11 -Wall -O2 -m64 -shared "
                "-DCANVAS_PLUGIN -Iinclude -Isrc -o %s.dll games/%s.c -L. -lcanvas\n",
                wsl_dir, name, name);
        fclose(sh);
        snprintf(cmd, sizeof(cmd), "wsl -e bash %s/.hot/rebuild.sh", wsl_dir);
        fprintf(stderr, "hot-reload: %s (%s)\n", cc, wsl_dir);
        rc = system(cmd);
    } else {
        snprintf(cmd, sizeof(cmd),
                 "\"%s\" -std=c11 -Wall -O2 -m64 -shared -DCANVAS_PLUGIN -Iinclude -Isrc "
                 "-o %s.dll games/%s.c -L. -lcanvas",
                 cc, name, name);
        fprintf(stderr, "hot-reload: %s\n", cmd);
        rc = system(cmd);
    }
#else
    snprintf(cmd, sizeof(cmd),
             "%s -std=c11 -Wall -O2 -shared -fPIC -DCANVAS_PLUGIN -Iinclude -Isrc "
             "-o %s.so games/%s.c -lm",
             cc, name, name);
    fprintf(stderr, "hot-reload: %s\n", cmd);
    rc = system(cmd);
#endif
    if (rc != 0) {
        fprintf(stderr, "hot-reload: build failed (%d)\n", rc);
        return -1;
    }
    return 0;
}

static int load_copy(HotHost *h, Game *out)
{
    const Game *g;
    HMODULE lib;
    char hot[256];

#ifdef _WIN32
    CreateDirectoryA(".hot", NULL);
#else
    mkdir(".hot", 0755);
#endif
    h->gen++;
    snprintf(hot, sizeof(hot), ".hot%c%s_%d%s", DIR_SEP, h->name, h->gen, LIB_EXT);
    if (copy_file(h->built, hot) != 0) {
        fprintf(stderr, "hot-reload: cannot copy %s -> %s\n", h->built, hot);
        return -1;
    }
    lib = open_lib(hot);
    if (!lib) {
#ifdef _WIN32
        DWORD err = GetLastError();
        char msg[256];
        msg[0] = 0;
        FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                       NULL, err, 0, msg, sizeof(msg), NULL);
        fprintf(stderr, "hot-reload: LoadLibrary %s failed (%lu) %s\n",
                hot, (unsigned long)err, msg);
        fprintf(stderr, "  %s.dll must sit next to canvas.dll and the .exe\n", h->name);
#else
        fprintf(stderr, "hot-reload: dlopen: %s\n", dlerror());
#endif
        return -1;
    }
    g = lib_game(lib);
    if (!g || !g->update || !g->render) {
        fprintf(stderr, "hot-reload: missing canvas_game in %s\n", hot);
        close_lib(lib);
        return -1;
    }
    close_lib(h->lib);
    if (h->loaded[0])
#ifdef _WIN32
        DeleteFileA(h->loaded);
#else
        unlink(h->loaded);
#endif
    h->lib = lib;
    snprintf(h->loaded, sizeof(h->loaded), "%s", hot);
    *out = *g;
    return 0;
}

static int hot_reload(void *ud, Game *game)
{
    HotHost *h = ud;
    if (rebuild(h->name) != 0)
        return -1;
    return load_copy(h, game);
}

int canvas_host_run(const char *name)
{
    HotHost host;
    Game game;

    memset(&host, 0, sizeof(host));
    chdir_to_exe();
    if (!name || !name[0] || strlen(name) >= sizeof(host.name)) {
        fprintf(stderr, "canvas: bad game name\n");
        return 1;
    }
    snprintf(host.name, sizeof(host.name), "%s", name);
    snprintf(host.built, sizeof(host.built), "%s%s", name, LIB_EXT);

    fprintf(stderr, "canvas: loading %s  (F5 reload, F6 reload+reset)\n", name);
#ifdef _WIN32
    if (find_cc())
        fprintf(stderr,
                "canvas: hot-reload cc: %s  (CANVAS_CC=mingw|msvc or canvas-cc file)\n",
                cc_kind == CC_KIND_MSVC ? "msvc" :
                cc_kind == CC_KIND_WSL ? "wsl-mingw" : "mingw");
    else
        fprintf(stderr, "canvas: no hot-reload compiler yet (set CANVAS_CC=mingw|msvc)\n");
#endif
    if (!file_exists(host.built)) {
        if (rebuild(name) != 0)
            return 1;
    }
    if (load_copy(&host, &game) != 0)
        return 1;

    canvas_hot_setup(hot_reload, &host);
#ifdef _WIN32
    if (cc_label[0])
        canvas_hot_set_tool(cc_label);
#endif
    {
        int rc = canvas_run(&game);
        close_lib(host.lib);
        return rc;
    }
}

#ifdef CANVAS_LAUNCH
int main(void)
{
    return canvas_host_run(CANVAS_LAUNCH);
}
#else
int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr,
                "usage: canvas <game>\n"
                "  games: wander bounce pacman plat\n"
                "  or run ./plat ./wander ./bounce ./pacman\n"
                "  F5  rebuild + reload plugin, keep heap state\n"
                "  F6  rebuild + reload plugin, re-init state\n"
#ifdef _WIN32
                "  CANVAS_CC=mingw|msvc  or a canvas-cc file next to the .exe\n"
#endif
        );
        return 1;
    }
    return canvas_host_run(argv[1]);
}
#endif
