// Glue code for parisc architecture
//
// Copyright (C) 2017  Helge Deller <deller@gmx.de>
//
// This file may be distributed under the terms of the GNU LGPLv3 license.

#include "biosvar.h" // GET_BDA
#include "bregs.h" // struct bregs
#include "hw/pic.h" // enable_hwirq
#include "output.h" // debug_enter
#include "stacks.h" // call16_int
#include "string.h" // memset
#include "util.h" // serial_setup
#include "malloc.h" // malloc
#include "hw/serialio.h" // qemu_debug_port
#include "fw/paravirt.h" // PlatformRunningOn
#include "parisc/pdc.h"

int HaveRunPost;
u8 ExtraStack[BUILD_EXTRA_STACK_SIZE+1] __aligned(8);
u8 *StackPos;

u8 BiosChecksum;

char zonefseg_start, zonefseg_end;  // SYMBOLS
char varlow_start, varlow_end, final_varlow_start;
char final_readonly_start;
char code32flat_start, code32flat_end;
char zonelow_base;

void mtrr_setup(void) { }
void mathcp_setup(void) { }
void smp_setup(void) { }
void bios32_init(void) { }

void cpuid(u32 index, u32 *eax, u32 *ebx, u32 *ecx, u32 *edx)
{
	eax = ebx = ecx = edx = 0;
}

void wrmsr_smp(u32 index, u64 val) { }

#define PAGE0 ((volatile struct zeropage *) 0UL)

extern char pdc_entry;

void __VISIBLE start_parisc_firmware(unsigned long ram_size,
	unsigned long linux_kernel_entry,
	unsigned long cmdline,
	unsigned long initrd_start,
	unsigned long initrd_end)
{
	/* Initialize PAGE0 */
	PAGE0->memc_cont = ram_size;
	PAGE0->memc_phsize = ram_size;
	PAGE0->mem_free = 4*4096; // 16k ??
	PAGE0->mem_hpa = 0; // /* HPA of the boot-CPU */
	PAGE0->mem_pdc = (unsigned long) &pdc_entry;
	PAGE0->mem_10msec = 1024; /* FIXME */
	PAGE0->imm_max_mem = ram_size;

	// while (1) outb('X', GET_GLOBAL(DebugOutputPort));
	// PlatformRunningOn = PF_QEMU;  // emulate runningOnQEMU()
	// qemu_debug_putc('A');

	dprintf(0, "\n");
	dprintf(0, "PARISC Firmware started, %lu MB RAM.\n", ram_size/1024/1024);
	// handle_post();
	serial_debug_preinit();
	debug_banner();
	// maininit();
	qemu_preinit();
	// coreboot_preinit();
	// malloc_preinit();
	// malloc_low(1);
 	// reloc_preinit(maininit, NULL);

	serial_setup();

	if (linux_kernel_entry) {
		void (*start_kernel)(unsigned long mem_free, unsigned long cmdline,
			unsigned long rdstart, unsigned long rdend);

		start_kernel = (void *) linux_kernel_entry;
		start_kernel(PAGE0->mem_free, cmdline, initrd_start, initrd_end);
	}

	hlt(); /* this ends the emulator */
}
