/* elsim - plugin registry + dynamic loader */
#include "internal.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#include <dirent.h>
#endif

typedef const EcPlugin *(*ec_export_fn)(void);

EcPluginRegistry *ec_registry_new(void)
{
    return calloc(1, sizeof(EcPluginRegistry));
}

void ec_registry_free(EcPluginRegistry *reg)
{
    if (!reg) return;
    free(reg->plugins);
    free(reg);
}

void ec_registry_add(EcPluginRegistry *reg, const EcPlugin *p)
{
    if (!reg || !p) return;
    if (reg->n == reg->cap) {
        reg->cap = reg->cap ? reg->cap * 2 : 16;
        reg->plugins = realloc(reg->plugins, (size_t)reg->cap * sizeof(EcPlugin *));
    }
    reg->plugins[reg->n++] = (EcPlugin *)p;
}

EcPlugin *ec_registry_find(EcPluginRegistry *reg, const char *id)
{
    if (!reg) return NULL;
    for (int i = 0; i < reg->n; i++)
        if (reg->plugins[i]->id && strcmp(reg->plugins[i]->id, id) == 0)
            return reg->plugins[i];
    return NULL;
}

#if defined(_WIN32)

static int load_one(EcPluginRegistry *reg, const char *path, char *err, size_t errsz)
{
    HMODULE h = LoadLibraryA(path);
    if (!h) {
        if (err && !*err) snprintf(err, errsz, "LoadLibrary failed: %s", path);
        return -1;
    }
    ec_export_fn fn = (ec_export_fn)GetProcAddress(h, "ec_plugin_export");
    if (!fn) {
        FreeLibrary(h);
        if (err && !*err) snprintf(err, errsz, "no ec_plugin_export in %s", path);
        return -1;
    }
    const EcPlugin *p = fn();
    if (!p || p->abi != EC_ABI_VERSION) {
        if (err && !*err) snprintf(err, errsz, "ABI mismatch in %s", path);
        return -1;
    }
    if (ec_registry_find(reg, p->id)) {
        if (err && !*err) snprintf(err, errsz, "duplicate plugin id '%s' in %s", p->id, path);
        return -1;
    }
    ec_registry_add(reg, p);
    return 0;
}

int ec_registry_load_dir(EcPluginRegistry *reg, const char *dir,
                         char *err, size_t errsz)
{
    if (err && errsz) err[0] = '\0';
    char pattern[1024];
    snprintf(pattern, sizeof pattern, "%s\\*.dll", dir);
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) return 0;
    int loaded = 0;
    do {
        char path[1200];
        snprintf(path, sizeof path, "%s\\%s", dir, fd.cFileName);
        if (load_one(reg, path, err, errsz) == 0) loaded++;
    } while (FindNextFileA(h, &fd));
    FindClose(h);
    return loaded;
}

#else /* POSIX */

static int load_one(EcPluginRegistry *reg, const char *path, char *err, size_t errsz)
{
    void *h = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (!h) {
        if (err && !*err) snprintf(err, errsz, "%s", dlerror());
        return -1;
    }
    ec_export_fn fn = (ec_export_fn)dlsym(h, "ec_plugin_export");
    if (!fn) {
        dlclose(h);
        if (err && !*err) snprintf(err, errsz, "no ec_plugin_export in %s", path);
        return -1;
    }
    const EcPlugin *p = fn();
    if (!p || p->abi != EC_ABI_VERSION) {
        if (err && !*err) snprintf(err, errsz, "ABI mismatch in %s (rebuild against core headers)", path);
        return -1;
    }
    if (ec_registry_find(reg, p->id)) {
        if (err && !*err) snprintf(err, errsz, "duplicate plugin id '%s' in %s", p->id, path);
        return -1;
    }
    ec_registry_add(reg, p);
    return 0; /* h stays open for the process lifetime */
}

int ec_registry_load_dir(EcPluginRegistry *reg, const char *dir,
                         char *err, size_t errsz)
{
    if (err && errsz) err[0] = '\0';
    DIR *d = opendir(dir);
    if (!d) return 0; /* missing directory is not an error */
    int loaded = 0;
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        size_t len = strlen(e->d_name);
        int is_so = len > 3 && strcmp(e->d_name + len - 3, ".so") == 0;
        int is_dll = len > 4 && strcmp(e->d_name + len - 4, ".dll") == 0;
        if (!is_so && !is_dll) continue;
        char path[1200];
        snprintf(path, sizeof path, "%s/%s", dir, e->d_name);
        if (load_one(reg, path, err, errsz) == 0) loaded++;
    }
    closedir(d);
    return loaded;
}

#endif
