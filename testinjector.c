#define _GNU_SOURCE

#include <stdio.h>
#include <dlfcn.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
//TODO alternative for non musl/glibc (does this driver even run on other systems?)
#include <link.h>
#include <signal.h>

#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"
#include <sys/mman.h>
#include <ucontext.h>
#include <pthread.h>
#include <sched.h>
#include <unistd.h>

typedef struct Library {
    uintptr_t base;
    size_t size;
    const char* name;
} Library;

typedef struct Watchpoint {
    uintptr_t address;
    size_t len;
    //instruction to restore
    uint16_t instr;
    uintptr_t bp_addr;
    //address that caused the last fault
    uint32_t addr;
    //last offset from the base of a library
    size_t addr_off;
    const char* lib_name;
    void (*callback_pre) (uintptr_t address, struct Watchpoint hit);
    void (*callback_post) (uintptr_t addr, struct Watchpoint hit);
} Watchpoint;

static Library*    libraries;
static Watchpoint* watchpoints;

static void sev_handler(int i, siginfo_t* siginfo, void* uap) {
    uintptr_t addr = (uintptr_t)siginfo->si_addr;
    ucontext_t *context = (ucontext_t *)uap;
    printf("fault at address %p\n", siginfo->si_addr);

    for (int i = 0; i < arrlen(watchpoints); i++) {
        Watchpoint cur = watchpoints[i];

        if (IN_RANGE((size_t)addr, cur.address, cur.len)) {
            printf("hit watchpoint %d\n", i);

            uintptr_t page = ALIGN_DOWN(((uintptr_t)addr), (getpagesize()));
            mprotect((void*)page, cur.len, PROT_READ | PROT_WRITE);

            uintptr_t codepage = ALIGN_DOWN(((uintptr_t)context->uc_mcontext.arm_pc), (getpagesize()));
            mprotect((void*)codepage, getpagesize(), PROT_READ | PROT_WRITE | PROT_EXEC);

            uint16_t* ins = (uint16_t*)((uintptr_t)context->uc_mcontext.arm_pc);
            uint16_t* c_instruction = (uint16_t*)0;

            const char* lib;
            size_t off;

            int j = 0;
            for (j = 0; j < arrlen(libraries); j++)  {
                if (IN_RANGE((context->uc_mcontext.arm_pc), libraries[j].base, libraries[j].size)) {
                    lib = libraries[j].name;
                    off = (context->uc_mcontext.arm_pc) - libraries[j].base;
                    break;
                }
            }

            printf("in lib %s\n", lib);

            if (((*ins >> 13) & 0b111) == 0b111) {
                c_instruction = (uint16_t*)((uintptr_t)context->uc_mcontext.arm_pc + 4);
            } else {
                c_instruction = (uint16_t*)((uintptr_t)context->uc_mcontext.arm_pc + 2);
            }
            watchpoints[i].instr = (*c_instruction);
            watchpoints[i].bp_addr = (uintptr_t)(c_instruction);
            watchpoints[i].addr = addr;
            watchpoints[i].addr_off = off;
            watchpoints[i].lib_name = lib;

            if (cur.callback_pre) {
                cur.callback_pre(addr, cur);
            }

            *c_instruction = 0xde01;
            __clear_cache((char*)(codepage), (char*)(codepage + getpagesize() * 10));

            return;
        }
    }

    printf("real segmentation fault\n");
    exit(1);
}

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
