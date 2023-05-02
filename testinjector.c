#define _GNU_SOURCE

#include <stdio.h>
#include <dlfcn.h>
#include <stdint.h>
#include <link.h>

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
    printf("is it even here yet \n");
    void* ret = real_dlopen(filename, flags | RTLD_GLOBAL);
    if (ret) {
        printf("\ndlopened: %s\n", filename);
    }

    //dl_iterate_phdr(iterate_callback, 0);
    return ret;

}
