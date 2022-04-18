#ifndef ALPHA_H
#define ALPHA_H
/* this file is included by x86.h */

#include "alpha/ioport.h"

#ifndef __ASSEMBLY__

#include "types.h" // u32
#include "byteorder.h" // le16_to_cpu

static inline void irq_disable(void)
{
    /* TODO ? */
}

static inline void irq_enable(void)
{
    /* TODO ? */
}

static inline void cpu_relax(void)
{
    /* TODO ? */
}

extern void hlt(void);

static inline void nop(void)
{
    asm volatile("nop");
}

static inline void wbinvd(void)
{
    barrier();  //  __sync_synchronize();
}

static inline u32 __ffs(unsigned long x)
{
    return __builtin_ctzl(x);
    /*
    ({ unsigned long __kir;                                               \
     __asm__(".arch ev67; cttz %1,%0" : "=r"(__kir) : "r"(x));          \
     __kir; })
    */
}

static inline u32 __fls(unsigned long x)
{
    return 64 - __ffs(x);
}


static inline u32 rol(u32 val, u16 rol) {
    u32 res, resr;
    res  = val << rol;
    resr = val >> (32-rol);
    res |= resr;
    return res;
}

static inline u32 ror(u32 word, unsigned int shift)
{
        return (word >> (shift & 31)) | (word << ((-shift) & 31));
}

#include "alpha/protos.h"       /* for outl, outw, inb ... */

static inline void insb(portaddr_t port, u8 *data, u32 count) {
    while (count--)
	*data++ = inb(port);
}
static inline void insw(portaddr_t port, u16 *data, u32 count) {
    while (count--)
        *data++ = inw(port);
}
static inline void insl(portaddr_t port, u32 *data, u32 count) {
    while (count--)
        *data++ = inl(port);
}
// XXX - outs not limited to es segment
static inline void outsb(portaddr_t port, u8 *data, u32 count) {
    while (count--)
	outb(*data++, port);
}
static inline void outsw(portaddr_t port, u16 *data, u32 count) {
    while (count--) {
        outw(*data, port);
	data++;
    }
}
static inline void outsl(portaddr_t port, u32 *data, u32 count) {
    while (count--) {
        outl(*data, port);
	data++;
    }
}

/* Compiler barrier is enough as an x86 CPU does not reorder reads or writes */
static inline void smp_rmb(void) {
    barrier();
}
static inline void smp_wmb(void) {
    barrier();
}

static inline void writel(void *addr, u32 val) {
    barrier();
    *(volatile u32 *)addr = val;
}
static inline void writew(void *addr, u16 val) {
    barrier();
    *(volatile u16 *)addr = val;
}
static inline void writeb(void *addr, u8 val) {
    barrier();
    *(volatile u8 *)addr = val;
}
static inline u64 readq(const void *addr) {
    u64 val = *(volatile const u64 *)addr;
    barrier();
    return val;
}
static inline u32 readl(const void *addr) {
    u32 val = *(volatile const u32 *)addr;
    barrier();
    return val;
}
static inline u16 readw(const void *addr) {
    u16 val = *(volatile const u16 *)addr;
    barrier();
    return val;
}
static inline u8 readb(const void *addr) {
    u8 val = *(volatile const u8 *)addr;
    barrier();
    return val;
}

// FLASH_FLOPPY not supported
#define GDT_CODE     (0)
#define GDT_DATA     (0)
#define GDT_B        (0)
#define GDT_G        (0)
#define GDT_BASE(v)  ((v) & 0)
#define GDT_LIMIT(v) ((v) & 0)
#define GDT_GRANLIMIT(v) ((v) & 0)

static inline u8 get_a20(void) {
    return 0;
}

static inline u8 set_a20(u8 cond) {
    return 0;
}

static inline void wrmsr(u32 index, u64 val)
{
}

// x86.c
void cpuid(u32 index, u32 *eax, u32 *ebx, u32 *ecx, u32 *edx);

#endif // !__ASSEMBLY__
#endif
