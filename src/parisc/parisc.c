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
#include "hw/ata.h"
#include "fw/paravirt.h" // PlatformRunningOn
#include "parisc/hppa_hardware.h" // DINO_UART_BASE
#include "parisc/pdc.h"

int HaveRunPost;
u8 ExtraStack[BUILD_EXTRA_STACK_SIZE+1] __aligned(8);
u8 *StackPos;
u8 __VISIBLE parisc_stack[16*1024] __aligned(64);

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
void yield_toirq(void) { }
void farcall16(struct bregs *callregs) { }
void farcall16big(struct bregs *callregs) { }

void cpuid(u32 index, u32 *eax, u32 *ebx, u32 *ecx, u32 *edx)
{
	eax = ebx = ecx = edx = 0;
}

void wrmsr_smp(u32 index, u64 val) { }

#define ARG0 arg[7-0]
#define ARG1 arg[7-1]
#define ARG2 arg[7-2]
#define ARG3 arg[7-3]
#define ARG4 arg[7-4]
#define ARG5 arg[7-5]
#define ARG6 arg[7-6]
#define ARG7 arg[7-7]

/*********** IODC ******/

int __VISIBLE parisc_iodc_entry(unsigned int *arg)
{
	unsigned long hpa = ARG0;
	unsigned long option = ARG1;
	unsigned long spa = ARG2;
	unsigned long layers = ARG3;
	unsigned long *result = (unsigned long *)ARG4;
	
	/* search for hpa */

	if (hpa == DINO_UART_HPA || hpa == LASI_UART_HPA)
	switch (option) {
	case ENTRY_IO_COUT: /* console output */
		dprintf(0, (char*)ARG6);
		result[0] = ARG7;
		return PDC_OK;
	}

	dprintf(0, "\nIODC option #%lx called: hpa=%lx spa=%lx layers=%lx ", option, hpa, spa, layers);
	dprintf(0, "result=%p %x %x %x\n", result, ARG5, ARG6, ARG7);

	hlt();
	return PDC_BAD_OPTION;
}

/*********** PDC *******/

int __VISIBLE parisc_pdc_entry(unsigned int *arg)
{
	//unsigned long hpa = ARG0;
//	unsigned long option = ARG1;
	//unsigned long spa = ARG2;
	//unsigned long layers = ARG3;
//	unsigned long *result = (unsigned long *)ARG4;
	
#if 0
	switch (option) {
	case ENTRY_IO_COUT: /* console output */
		dprintf(0, (char*)ARG6);
		result[0] = ARG7;
		return PDC_OK;
	}
#endif
	dprintf(0, "\nPDC called %x %x %x %x ", ARG0, ARG1, ARG2, ARG3);
	dprintf(0, "%x %x %x %x\n", ARG4, ARG5, ARG6, ARG7);

	hlt();
	return PDC_BAD_OPTION;
}

/*********** BOOT MENU *******/

extern int parisc_boot_menu(void);

// struct disk_op_s  // CMD_READ


/*********** MAIN *******/

extern char pdc_entry;
extern char iodc_entry;

static const struct pz_device mem_cons_boot = {
	.hpa = DINO_UART_HPA,
	.iodc_io = (unsigned long) &iodc_entry,
	.cl_class = CL_DUPLEX,
};

static const struct pz_device mem_boot_boot = {
	.hpa = IDE_HPA,
	.iodc_io = (unsigned long) &iodc_entry,
	.cl_class = CL_RANDOM,
};

static const struct pz_device mem_kbd_boot = {
	.hpa = DINO_UART_HPA,
	.iodc_io = (unsigned long) &iodc_entry,
	.cl_class = CL_DUPLEX,
	// .cl_class = CL_KEYBD,
};


#define PAGE0 ((volatile struct zeropage *) 0UL)

void __VISIBLE start_parisc_firmware(unsigned long ram_size,
	unsigned long linux_kernel_entry,
	unsigned long cmdline,
	unsigned long initrd_start,
	unsigned long initrd_end)
{
	unsigned int cpu_hz;

	/* Initialize PAGE0 */
	PAGE0->memc_cont = ram_size;
	PAGE0->memc_phsize = ram_size;
	PAGE0->mem_free = 4*4096; // 16k ??
	PAGE0->mem_hpa = 0; // /* HPA of the boot-CPU */
	PAGE0->mem_pdc = (unsigned long) &pdc_entry;
	PAGE0->mem_10msec = CPU_CLOCK_MHZ*(1000000ULL/100);

	PAGE0->imm_max_mem = ram_size;
	memcpy((void*)&(PAGE0->mem_cons), &mem_cons_boot, sizeof(mem_cons_boot));
	memcpy((void*)&(PAGE0->mem_boot), &mem_boot_boot, sizeof(mem_boot_boot));
	memcpy((void*)&(PAGE0->mem_kbd),  &mem_kbd_boot, sizeof(mem_kbd_boot));

	malloc_preinit();

	// set Qemu serial debug port
	DebugOutputPort = PORT_SERIAL1;
	// PlatformRunningOn = PF_QEMU;  // emulate runningOnQEMU()

	printf("\n");
	printf("PARISC SeaBIOS Firmware started, %lu MB RAM.\n", ram_size/1024/1024);

	cpu_hz = 100 * PAGE0->mem_10msec; /* Hz of this PARISC */
	printf("1 CPU at %d.%06d MHz\n",
			cpu_hz / 1000000, cpu_hz % 1000000 );

	// mdelay(1000); // test: "wait 1 second"

	// handle_post();
	serial_debug_preinit();
	debug_banner();
	// maininit();
	qemu_preinit();
	// coreboot_preinit();

	serial_setup();
	ata_setup();

	parisc_boot_menu();

	if (linux_kernel_entry) {
		void (*start_kernel)(unsigned long mem_free, unsigned long cmdline,
			unsigned long rdstart, unsigned long rdend);

		start_kernel = (void *) linux_kernel_entry;
		start_kernel(PAGE0->mem_free, cmdline, initrd_start, initrd_end);
	}

	hlt(); /* this ends the emulator */
}
