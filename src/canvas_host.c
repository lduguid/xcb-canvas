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

static int rebuild(const char *name)
{
    char cmd[256];
    int rc;

#ifdef _WIN32
    snprintf(cmd, sizeof(cmd), "make -B -f Makefile.win32 %s.dll", name);
#else
    snprintf(cmd, sizeof(cmd), "make -B %s.so", name);
#endif
    fprintf(stderr, "hot-reload: %s\n", cmd);
    rc = system(cmd);
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
        fprintf(stderr, "hot-reload: LoadLibrary failed (%lu)\n", (unsigned long)GetLastError());
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
    if (rebuild(name) != 0)
        return 1;
    if (load_copy(&host, &game) != 0)
        return 1;

    canvas_hot_setup(hot_reload, &host);
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
                "  F6  rebuild + reload plugin, re-init state\n");
        return 1;
    }
    return canvas_host_run(argv[1]);
}
#endif
