#define _GNU_SOURCE

#include <stdio.h>
#include <dlfcn.h>
#include <stdint.h>
#include <link.h>
#include "stb_ds.h"

typedef struct Library {
    uintptr_t base;
    size_t size;
    const char* name;
} Library;

static Library*    libraries;

static int iterate_callback (struct dl_phdr_info *info, size_t sze, void * ptr)  {
    size_t last_addr = 0;

    for (int i = 0; i < info->dlpi_phnum; i++) {
        ElfW(Phdr) cur = info->dlpi_phdr[i];
        uintptr_t  first = info->dlpi_addr + cur.p_vaddr;
        uintptr_t  last  = first + cur.p_memsz - 1;

        if (last > last_addr) {
            last_addr = last;
        }
    }
    Library new = { .base = info->dlpi_addr, .size = (last_addr - info->dlpi_addr), .name = (info->dlpi_name) };

    //TODO reduce memory waste with a hashmap to avoid having the same library multiple times since they are all listed upon dlopen
    arrput(libraries, new);

    return 0;
}

static void* real_dlopen(const char* filename, int flags)
{

    void* ret = dlsym(RTLD_NEXT, "dlopen");
    if (ret == NULL) {
        fprintf(stderr, "could not dlsym %s\n", "dlopen");
    }

    return ret;

}

void* dlopen(const char* filename, int flags)
{
    void* ret = real_dlopen(filename, flags | RTLD_GLOBAL);
    if (ret) {
        printf("\ndlopened: %s\n", filename);
    }

    dl_iterate_phdr(iterate_callback, 0);
    return ret;

}
