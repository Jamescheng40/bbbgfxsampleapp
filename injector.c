#include <stdio.h>
#include <stdlib.h>

static void* (*real_malloc)(size_t size);

void* malloc(size_t size) {
        if (!real_malloc) {
            real_malloc = dlsym(RTLD_NEXT, "malloc");
            //check if the pointer is valid
        }

        printf("ok we hit our function \n");
        void *ptr = real_malloc(size);
        return ptr;
}