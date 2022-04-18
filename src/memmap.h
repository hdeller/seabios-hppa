#ifndef __MEMMAP_H
#define __MEMMAP_H

#include "types.h" // u32

#if CONFIG_ALPHA
#define PAGE_SIZE 8192
#define PAGE_SHIFT 13
#else
// A typical OS page size
#define PAGE_SIZE 4096
#define PAGE_SHIFT 12
#endif

static inline u32 virt_to_phys(void *v) {
    return (unsigned long)v;
}
static inline void *memremap(unsigned long addr, u32 len) {
    return (void*)addr;
}

// Return the value of a linker script symbol (see scripts/layoutrom.py)
#define SYMBOL(SYM) ({ extern char SYM; (u32)&SYM; })
#define VSYMBOL(SYM) ((void*)SYMBOL(SYM))

#endif // memmap.h
