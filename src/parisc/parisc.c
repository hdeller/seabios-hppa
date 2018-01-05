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
#include "hw/pcidevice.h" // foreachpci
#include "hw/pci.h" // pci_config_readl
#include "hw/pci_regs.h" // PCI_BASE_ADDRESS_0
#include "hw/ata.h"
#include "hw/blockcmd.h" // scsi_is_ready()
#include "hw/rtc.h"
#include "fw/paravirt.h" // PlatformRunningOn
#include "vgahw.h"
#include "parisc/hppa_hardware.h" // DINO_UART_BASE
#include "parisc/pdc.h"
#include "parisc/b160l.h"

#include "vgabios.h"

/*
 * Various variables which are needed by x86 code.
 * Defined here to be able to link seabios.
 */
int HaveRunPost;
u8 ExtraStack[BUILD_EXTRA_STACK_SIZE+1] __aligned(8);
u8 *StackPos;
u8 __VISIBLE parisc_stack[32*1024] __aligned(64);

u8 BiosChecksum;

char zonefseg_start, zonefseg_end;  // SYMBOLS
char varlow_start, varlow_end, final_varlow_start;
char final_readonly_start;
char code32flat_start, code32flat_end;
char zonelow_base;

struct bios_data_area_s __VISIBLE bios_data_area;
struct vga_bda_s	__VISIBLE vga_bios_data_area;
struct bregs regs;
unsigned long parisc_vga_mem;
unsigned long parisc_vga_mmio;
struct segoff_s ivt_table[256];

void mtrr_setup(void) { }
void mathcp_setup(void) { }
void smp_setup(void) { }
void bios32_init(void) { }
void yield_toirq(void) { }
void farcall16(struct bregs *callregs) { }
void farcall16big(struct bregs *callregs) { }

void cpuid(u32 index, u32 *eax, u32 *ebx, u32 *ecx, u32 *edx)
{
	*eax = *ebx = *ecx = *edx = 0;
}

void wrmsr_smp(u32 index, u64 val) { }

/********************************************************
 * PA-RISC specific constants and functions.
 ********************************************************/

/* Pointer to zero-page of PA-RISC */
#define PAGE0 ((volatile struct zeropage *) 0UL)

/* args as handed over for firmware calls */
#define ARG0 arg[7-0]
#define ARG1 arg[7-1]
#define ARG2 arg[7-2]
#define ARG3 arg[7-3]
#define ARG4 arg[7-4]
#define ARG5 arg[7-5]
#define ARG6 arg[7-6]
#define ARG7 arg[7-7]

/* size of I/O block used in HP firmware */
#define FW_BLOCKSIZE    2048

void __noreturn hlt(void)
{
    printf("SeaBIOS wants SYSTEM HALT.\n\n");
    asm volatile("\t.word 0xfffdead0": : :"memory");
    while (1);
}

void __noreturn reset(void)
{
    printf("SeaBIOS wants SYSTEM RESET.\n\n");
    PAGE0->imm_soft_boot = 1;
    asm volatile("\t.word 0xfffdead1": : :"memory");
    while (1);
}

/********************************************************
 * FIRMWARE IO Dependent Code (IODC) HANDLER
 ********************************************************/

typedef struct {
	unsigned long hpa;
	struct pdc_iodc *iodc;
	struct pdc_system_map_mod_info *mod_info;
	struct pdc_module_path *mod_path;
	int num_addr;
	int add_addr[5];
} hppa_device_t;

static hppa_device_t parisc_devices[] = { PARISC_DEVICE_LIST };

#define PARISC_KEEP_LIST \
       0xffc00000,\
       0xfff80000,\
       0xfff83000,\
       0xfffbe000,\
       0xfffbf000,\

/* drop all devices which are not listed in PARISC_KEEP_LIST */
static void remove_parisc_devices(void)
{
	static unsigned long keep_list[] = { PARISC_KEEP_LIST };
	int i,p, t;
	i = p = t = 0;
	while (keep_list[i] && parisc_devices[p].hpa) {
		if (parisc_devices[p].hpa == keep_list[i]) {
			parisc_devices[t++] = parisc_devices[p];
			i++;
		}
		p++;
	}
	while (t < ARRAY_SIZE(parisc_devices)) {
		memset(&parisc_devices[t], 0, sizeof(parisc_devices[0]));
		t++;
	}
}

static struct drive_s *boot_drive;

static int find_hpa_index(unsigned long hpa)
{
	int i;
	if (!hpa)
		return -1;
	for (i = 0; i < (ARRAY_SIZE(parisc_devices)-1); i++)
		if (hpa == parisc_devices[i].hpa)
			return i;
	return -1;
}


#define SERIAL_TIMEOUT 20
static unsigned long parisc_serial_in(char *c, unsigned long maxchars)
{
	const portaddr_t addr = DINO_UART_HPA+0x800;
	unsigned long end = timer_calc(SERIAL_TIMEOUT);
	unsigned long count = 0;
	while (count < maxchars) {
		u8 lsr = inb(addr+SEROFF_LSR);
		if (lsr & 0x01) {
			// Success - can read data
			*c++ = inb(addr+SEROFF_DATA);
			count++;
		}
	        if (timer_check(end))
			break;
	}
	return count;
}

int __VISIBLE parisc_iodc_entry(unsigned int *arg)
{
	unsigned long hpa = ARG0;
	unsigned long option = ARG1;
	unsigned long spa = ARG2;
	unsigned long layers = ARG3;
	unsigned long *result = (unsigned long *)ARG4;
	int ret, len;
	char *c;
	struct disk_op_s disk_op;

#if 0
	dprintf(0, "\nIODC option start #%ld : hpa=%lx spa=%lx layers=%lx ", option, hpa, spa, layers);
	dprintf(0, "result=%p arg5=0x%x arg6=0x%x arg7=0x%x\n", result, ARG5, ARG6, ARG7);
#endif
	/* console I/O */
	if (hpa == DINO_UART_HPA || hpa == LASI_UART_HPA)
	switch (option) {
	case ENTRY_IO_COUT: /* console output */
		c = (char*)ARG6;
		result[0] = len = ARG7;
		while (len--)
			printf("%c", *c++);
		return PDC_OK;
	case ENTRY_IO_CIN: /* console input, with 5 seconds timeout */
		c = (char*)ARG6;
		result[0] = parisc_serial_in(c, ARG7);
		return PDC_OK;
	}

	/* boot medium I/O */
	if (hpa == IDE_HPA)
	switch (option) {
	case ENTRY_IO_BOOTIN: /* boot medium IN */
		disk_op.drive_fl = boot_drive;
		disk_op.buf_fl = (void*)ARG6;
		disk_op.command = CMD_READ;
		disk_op.count = (ARG7 / disk_op.drive_fl->blksize);
		disk_op.lba = (ARG5 / disk_op.drive_fl->blksize);
		ret = process_op(&disk_op);
		// dprintf(0, "\nBOOT IO res %d count = %d\n", ret, ARG7);
		result[0] = ARG7;
		if (ret)
			return PDC_ERROR;
		return PDC_OK;
	}

	dprintf(0, "\nIODC option #%ld called: hpa=0x%lx spa=0x%lx layers=0x%lx ", option, hpa, spa, layers);
	dprintf(0, "result=0x%p arg5=0x%x arg6=0x%x arg7=0x%x\n", result, ARG5, ARG6, ARG7);

	hlt();
	return PDC_BAD_OPTION;
}


/********************************************************
 * FIRMWARE PDC HANDLER
 ********************************************************/

#define STABLE_STORAGE_SIZE	256
static unsigned char stable_storage[STABLE_STORAGE_SIZE];

static void init_stable_storage(void)
{
	/* see ENGINEERING NOTE in PDC2.0 doc */
	memset(&stable_storage, 0, STABLE_STORAGE_SIZE);
	// no intial paths
	stable_storage[0x07] = 0xff;
	stable_storage[0x67] = 0xff;
	stable_storage[0x87] = 0xff;
	stable_storage[0xa7] = 0xff;
	// 0x0e/0x0f => fastsize = all, needed for HPUX
	stable_storage[0x5f] = 0x0f;
}


/*
 * Trivial time conversion helper functions.
 * Not accurate before year 2000 and beyond year 2099.
 * Taken from:
 * https://codereview.stackexchange.com/questions/38275/convert-between-date-time-and-time-stamp-without-using-standard-library-routines
 */

static unsigned short days[4][12] =
{
    {   0,  31,  60,  91, 121, 152, 182, 213, 244, 274, 305, 335},
    { 366, 397, 425, 456, 486, 517, 547, 578, 609, 639, 670, 700},
    { 731, 762, 790, 821, 851, 882, 912, 943, 974,1004,1035,1065},
    {1096,1127,1155,1186,1216,1247,1277,1308,1339,1369,1400,1430},
};

static inline int rtc_from_bcd(int a)
{
	return ((a >> 4) * 10) + (a & 0x0f);
}

#define SECONDS_2000_JAN_1 946684800
/* assumption: only dates between 01/01/2000 and 31/12/2099 */

static unsigned long seconds_since_1970(void)
{
	unsigned long ret;
	unsigned int second = rtc_from_bcd(rtc_read(CMOS_RTC_SECONDS));
	unsigned int minute = rtc_from_bcd(rtc_read(CMOS_RTC_MINUTES));
	unsigned int hour   = rtc_from_bcd(rtc_read(CMOS_RTC_HOURS));
	unsigned int day    = rtc_from_bcd(rtc_read(CMOS_RTC_DAY_MONTH)) - 1;
	unsigned int month  = rtc_from_bcd(rtc_read(CMOS_RTC_MONTH)) - 1;
	unsigned int year   = rtc_from_bcd(rtc_read(CMOS_RTC_YEAR));
	ret = (((year/4*(365*4+1)+days[year%4][month]+day)*24+hour)*60+minute)
			*60+second + SECONDS_2000_JAN_1;

	if (year >= 100)
		dprintf(0, "\nSeaBIOS WARNING: READ RTC_YEAR=%d is above year 2100.\n", year);

	return ret;
}

static inline int rtc_to_bcd(int a)
{
	return ((a / 10) << 4) | (a % 10);
}

void epoch_to_date_time(unsigned long epoch)
{
    epoch -= SECONDS_2000_JAN_1;

    unsigned int second = epoch%60; epoch /= 60;
    unsigned int minute = epoch%60; epoch /= 60;
    unsigned int hour   = epoch%24; epoch /= 24;

    unsigned int years = epoch/(365*4+1)*4; epoch %= 365*4+1;

    unsigned int year;
    for (year=3; year>0; year--)
    {
        if (epoch >= days[year][0])
            break;
    }

    unsigned int month;
    for (month=11; month>0; month--)
    {
        if (epoch >= days[year][month])
            break;
    }

    unsigned int rtc_year  = years + year;
    unsigned int rtc_month = month + 1;
    unsigned int rtc_day   = epoch - days[year][month] + 1;

    /* set date into RTC */
    rtc_write(CMOS_RTC_SECONDS, rtc_to_bcd(second));
    rtc_write(CMOS_RTC_MINUTES, rtc_to_bcd(minute));
    rtc_write(CMOS_RTC_HOURS, rtc_to_bcd(hour));
    rtc_write(CMOS_RTC_DAY_MONTH, rtc_to_bcd(rtc_day));
    rtc_write(CMOS_RTC_MONTH, rtc_to_bcd(rtc_month));
    rtc_write(CMOS_RTC_YEAR, rtc_to_bcd(rtc_year));

    if (rtc_year >= 100)
	dprintf(0, "\nSeaBIOS WARNING: WRITE RTC_YEAR=%d above year 2100.\n", rtc_year);
}

static const char *pdc_name(unsigned long num)
{
	#define DO(x) if (num == x) return #x;
	DO(PDC_POW_FAIL)
	DO(PDC_CHASSIS)
	DO(PDC_PIM)
	DO(PDC_MODEL)
	DO(PDC_CACHE)
	DO(PDC_HPA)
	DO(PDC_COPROC)
	DO(PDC_IODC)
	DO(PDC_TOD)
	DO(PDC_STABLE)
	DO(PDC_NVOLATILE)
	DO(PDC_ADD_VALID)
	DO(PDC_INSTR)
	DO(PDC_PROC)
	DO(PDC_BLOCK_TLB)
	DO(PDC_TLB)
	DO(PDC_MEM)
	DO(PDC_PSW)
	DO(PDC_SYSTEM_MAP)
	DO(PDC_SOFT_POWER)
	DO(PDC_MEM_MAP)
	DO(PDC_EEPROM)
	DO(PDC_NVM)
	DO(PDC_SEED_ERROR)
	DO(PDC_IO)
	DO(PDC_BROADCAST_RESET)
	DO(PDC_LAN_STATION_ID)
	DO(PDC_CHECK_RANGES)
	DO(PDC_NV_SECTIONS)
	DO(PDC_PERFORMANCE)
	DO(PDC_SYSTEM_INFO)
	DO(PDC_RDR)
	DO(PDC_INTRIGUE)
	DO(PDC_STI)
	DO(PDC_PCI_INDEX)
	DO(PDC_INITIATOR)
	DO(PDC_LINK)
	return "UNKNOWN!";
}

int __VISIBLE parisc_pdc_entry(unsigned int *arg)
{
	static unsigned long psw_defaults = PDC_PSW_ENDIAN_BIT;
	static unsigned long cache_info[] = { PARISC_PDC_CACHE_INFO };
	static struct pdc_cache_info *machine_cache_info
				= (struct pdc_cache_info *) &cache_info;
	static unsigned long model[] = { PARISC_PDC_MODEL };
	static const char model_str[] = PARISC_MODEL;

	unsigned long proc = ARG0;
	unsigned long option = ARG1;
	unsigned long *result = (unsigned long *)ARG2;

	int hpa_index;
	struct pdc_iodc *iodc_p;
	unsigned char *c;

	struct pdc_system_map_mod_info *pdc_mod_info;
	struct pdc_module_path *mod_path;
#if 0
	dprintf(0, "\nSeaBIOS: Start PDC proc %s(%d) option %d result=0x%x ARG3=0x%x ", pdc_name(ARG0), ARG0, ARG1, ARG2, ARG3);
	dprintf(0, "ARG4=0x%x ARG5=0x%x ARG6=0x%x ARG7=0x%x\n", ARG4, ARG5, ARG6, ARG7);
#endif
	switch (proc) {
	case PDC_POW_FAIL:
		break;
	case PDC_CHASSIS: /* chassis functions */
		// dprintf(0, "\n\nSeaBIOS: Unimplemented PDC_CHASSIS function %d %x %x %x %x\n", ARG1, ARG2, ARG3, ARG4, ARG5);
		return PDC_BAD_PROC;
	case PDC_PIM:
		break;
	case PDC_MODEL: /* model information */
		switch (option) {
		case PDC_MODEL_INFO:
			memcpy(result, model, sizeof(model));
			return PDC_OK;
		case PDC_MODEL_VERSIONS:
			if (ARG3 == 0) {
				result[0] = PARISC_PDC_VERSION;
				return PDC_OK;
			}
			return -4; // invalid c_index
		case PDC_MODEL_SYSMODEL:
			result[0] = sizeof(model_str) - 1;
			strtcpy((char *)ARG4, model_str, sizeof(model_str));
			return PDC_OK;
		case PDC_MODEL_CPU_ID:
			result[0] = PARISC_PDC_CPUID;
			return PDC_OK;
		case PDC_MODEL_CAPABILITIES:
			result[0] = PARISC_PDC_CAPABILITIES;
			return PDC_OK;
		}
		dprintf(0, "\n\nSeaBIOS: Unimplemented PDC_MODEL function %d %x %x %x %x\n", ARG1, ARG2, ARG3, ARG4, ARG5);
		return PDC_BAD_OPTION;
	case PDC_CACHE:
		switch (option) {
		case PDC_CACHE_INFO:
			if (sizeof(cache_info) != sizeof(*machine_cache_info))
				hlt();
#if 1
			// XXX: number of TLB entries should be aligned with qemu
			machine_cache_info->it_size = 256;
			machine_cache_info->dt_size = 256;
			machine_cache_info->it_loop = 1;
			machine_cache_info->dt_loop = 1;
#endif
			memcpy(result, cache_info, sizeof(cache_info));
			return PDC_OK;
		}
		dprintf(0, "\n\nSeaBIOS: Unimplemented PDC_CACHE function %d %x %x %x %x\n", ARG1, ARG2, ARG3, ARG4, ARG5);
		return PDC_BAD_OPTION;
	case PDC_HPA:
		switch (option) {
		case PDC_HPA_PROCESSOR:
			result[0] = CPU_HPA;
			result[1] = 0;
			return PDC_OK;
		case PDC_HPA_MODULES:
			return PDC_BAD_OPTION; // all modules on same board as the processor.
		}
		break;
	case PDC_COPROC:
		switch (option) {
		case PDC_COPROC_CFG:
			memset(result, 0, 32 * sizeof(unsigned long));
			result[0] = 1; // XXX: num_cpus
			result[1] = 1;
			result[17] = 1; // Revision
			result[18] = 19; // Model
			return PDC_OK;
		}
		return PDC_BAD_OPTION;
	case PDC_IODC: /* Call IODC functions */
		// dprintf(0, "\n\nSeaBIOS: Info PDC_IODC function %ld ARG3=%x ARG4=%x ARG5=%x\n", option, ARG3, ARG4, ARG5);
		switch (option) {
		case 0:
			if (ARG3 == IDE_HPA) {
				iodc_p = &iodc_data_hpa_fff8c000; // workaround for PCI ATA
			} else {
				hpa_index = find_hpa_index(ARG3); // index in hpa list
				if (hpa_index < 0)
					return -3; // not found
				iodc_p = parisc_devices[hpa_index].iodc;
			}

			if (ARG4 == PDC_IODC_RI_DATA_BYTES) {
				memcpy((void*) ARG5, iodc_p, ARG6);
				c = (unsigned char *) ARG5;
				c[0] = iodc_p->hversion_model; // FIXME.
				// c[1] = iodc_p->hversion_rev || (iodc_p->hversion << 4);
				*result = ARG6;
				return PDC_OK;
			}

			if (ARG4 == PDC_IODC_RI_INIT) {
				result[0] = 0;
				result[1] = (ARG3 == DINO_UART_HPA) ? CL_DUPLEX:
					    (ARG3 == IDE_HPA) ? CL_RANDOM : 0;
				result[2] = 0;
				result[3] = 0;
				return PDC_OK;
			}

			if (ARG4 == PDC_IODC_RI_IO) { // Get ENTRY_IO
				// TODO for HP-UX 10.20
			}
		}
		dprintf(0, "\n\nSeaBIOS: Unimplemented PDC_IODC function %ld ARG3=%x ARG4=%x ARG5=%x\n", option, ARG3, ARG4, ARG5);
		return PDC_BAD_OPTION;
	case PDC_TOD:	/* Time of day */
		switch (option) {
		case PDC_TOD_READ:
			result[0] = seconds_since_1970();
			result[1] = result[2] = result[3] = 0;
			return PDC_OK;
		case PDC_TOD_WRITE:
			/* we ignore the usecs in ARG3 */
			epoch_to_date_time(ARG2);
			return PDC_OK;
		case 2: /* PDC_TOD_CALIBRATE_TIMERS */
			/* double-precision floating-point with frequency of Interval Timer in megahertz: */
			*(double*)&result[0] = (double)CPU_CLOCK_MHZ;
			/* unsigned 64-bit integers representing  clock accuracy in parts per billion: */
			result[2] = 0x5a6c; /* TOD_acc */
			result[3] = 0x5a6c; /* CR_acc (interval timer) */
			return PDC_OK;
		}
		dprintf(0, "\n\nSeaBIOS: Unimplemented PDC_TOD function %ld ARG2=%x ARG3=%x ARG4=%x\n", option, ARG2, ARG3, ARG4);
		return PDC_BAD_OPTION;
	case PDC_STABLE:
		// dprintf(0, "\n\nSeaBIOS: PDC_STABLE function %ld ARG2=%x ARG3=%x ARG4=%x\n", option, ARG2, ARG3, ARG4);
		switch (option) {
		case PDC_STABLE_READ:
			if ((ARG2 + ARG4) > STABLE_STORAGE_SIZE)
				return PDC_INVALID_ARG;
			memcpy((unsigned char *) ARG3, &stable_storage[ARG2], ARG4);
			return PDC_OK;
		case PDC_STABLE_WRITE:
			if ((ARG2 + ARG4) > STABLE_STORAGE_SIZE)
				return PDC_INVALID_ARG;
			memcpy(&stable_storage[ARG2], (unsigned char *) ARG3, ARG4);
			return PDC_OK;
		case PDC_STABLE_RETURN_SIZE:
			result[0] = STABLE_STORAGE_SIZE;
			return PDC_OK;
		case PDC_STABLE_VERIFY_CONTENTS:
			return PDC_OK;
		case PDC_STABLE_INITIALIZE:
			init_stable_storage();
			return PDC_OK;
		}
		return PDC_BAD_OPTION;
	case PDC_NVOLATILE:
		return PDC_BAD_PROC;
	case PDC_ADD_VALID:
		// dprintf(0, "\n\nSeaBIOS: PDC_ADD_VALID function %ld ARG2=%x called.\n", option, ARG2);
		if (ARG2 < PAGE0->memc_phsize)
			return PDC_OK;
		if ((ARG2 >= FIRMWARE_START) && (ARG2 <= 0xffffffff))
			return PDC_OK;
		return PDC_ERROR;
	case PDC_INSTR:
		return PDC_BAD_PROC;
	case PDC_CONFIG:	/* Obsolete */
		return PDC_BAD_PROC;
	case PDC_BLOCK_TLB:	/* not needed on virtual machine */
		return PDC_BAD_PROC;
	case PDC_TLB:		/* not used on Linux. Maybe on HP-UX? */
		return PDC_BAD_PROC;
	case PDC_MEM:		/* replaced by PDC_SYSTEM_MAP, might be needed for 64-bit */
		// dprintf(0, "\n\nSeaBIOS: Check PDC_MEM option %ld ARG3=%x ARG4=%x ARG5=%x\n", option, ARG3, ARG4, ARG5);
		return PDC_BAD_PROC;
	case PDC_PSW:	/* Get/Set default System Mask  */
		if (option > PDC_PSW_SET_DEFAULTS)
			return PDC_BAD_OPTION;
		/* FIXME: For 64bit support enable PDC_PSW_WIDE_BIT too! */
		if (option == PDC_PSW_MASK)
			*result = PDC_PSW_ENDIAN_BIT;
		if (option == PDC_PSW_GET_DEFAULTS)
			*result = psw_defaults;
		if (option == PDC_PSW_SET_DEFAULTS) {
			// if ((ARG2 & PDC_PSW_ENDIAN_BIT) == 0)
			psw_defaults = ARG2;
		}
		return PDC_OK;
	case PDC_SYSTEM_MAP:
		// dprintf(0, "\n\nSeaBIOS: Info: PDC_SYSTEM_MAP function %ld ARG3=%x ARG4=%x ARG5=%x\n", option, ARG3, ARG4, ARG5);
		switch (option) {
		case PDC_FIND_MODULE:
			if (ARG4 >= ARRAY_SIZE(parisc_devices))
				return PDC_NE_MOD; // Module not found
			if (!parisc_devices[ARG4].hpa)
				return PDC_NE_MOD; // Module not found

			pdc_mod_info = (struct pdc_system_map_mod_info *)ARG2;
			mod_path = (struct pdc_module_path *)ARG3;

			*pdc_mod_info = *parisc_devices[ARG4].mod_info;
			*mod_path = *parisc_devices[ARG4].mod_path;

			// FIXME: Implement additional addresses
			pdc_mod_info->add_addrs = 0;

			/* Work around firmware bug. Some device like
			 * "Merlin+ 132 Dino PS/2 Port" don't set hpa.
			 */
			if (!pdc_mod_info->mod_addr) {
				pdc_mod_info->mod_addr = parisc_devices[ARG4].hpa;
				mod_path->path.mod = 1;
			}

			return PDC_OK;
		case PDC_FIND_ADDRESS:
		case PDC_TRANSLATE_PATH:
			break; // return PDC_OK;
		}
		break;
	case PDC_SOFT_POWER: // don't have a soft-power switch
		switch (option) {
		case PDC_SOFT_POWER_ENABLE:
			if (ARG3 == 0) // put soft power button under hardware control.
				hlt();
			return PDC_OK;
		}
		return PDC_BAD_OPTION;
		dprintf(0, "\n\nSeaBIOS: PDC_SOFT_POWER called with ARG2=%x ARG3=%x ARG4=%x\n", ARG2, ARG3, ARG4);
	case 26: // PDC_SCSI_PARMS is the architected firmware interface to replace the Hversion PDC_INITIATOR procedure.
		return PDC_BAD_PROC;
	case PDC_IO:
		switch (option) {
		case PDC_IO_RESET:
		case PDC_IO_RESET_DEVICES:
			return PDC_OK;
		}
		break;
	case PDC_BROADCAST_RESET:
		dprintf(0, "\n\nSeaBIOS: PDC_BROADCAST_RESET (reset system) called with ARG3=%x ARG4=%x\n", ARG3, ARG4);
		reset();
		return PDC_OK;
	case PDC_PCI_INDEX: // not needed for Dino PCI bridge
		return PDC_BAD_PROC;
	case PDC_INITIATOR:
		switch (option) {
		case PDC_GET_INITIATOR:
			// ARG3 has hwpath
			result[0] = 7; // host_id: 7 to 15 ?
			result[1] = 40; // 1, 2, 5 or 10 for 5, 10, 20 or 40 MT/s
			result[2] = 0; // ??
			result[3] = 0; // ??
			result[4] = 0; // width: 0:"Narrow, 1:"Wide"
			result[5] = 0; // mode: 0:SMODE_SE, 1:SMODE_HVD, 2:SMODE_LVD
			return PDC_OK;
		case PDC_SET_INITIATOR:
		case PDC_DELETE_INITIATOR:
		case PDC_RETURN_TABLE_SIZE:
		case PDC_RETURN_TABLE:
			break;
		}
		dprintf(0, "\n\nSeaBIOS: Unimplemented PDC_INITIATOR function %ld ARG3=%x ARG4=%x ARG5=%x\n", option, ARG3, ARG4, ARG5);
		return PDC_BAD_OPTION;
	}

	dprintf(0, "\nSeaBIOS: Unimplemented PDC proc %s(%d) option %d result=%x ARG3=%x ",
			pdc_name(ARG0), ARG0, ARG1, ARG2, ARG3);
	dprintf(0, "ARG4=%x ARG5=%x ARG6=%x ARG7=%x\n", ARG4, ARG5, ARG6, ARG7);

	hlt();
	return PDC_BAD_PROC;
}


/********************************************************
 * BOOT MENU
 ********************************************************/

extern struct drive_s *select_parisc_boot_drive(char bootdrive);

static int parisc_boot_menu(unsigned long *iplstart, unsigned long *iplend,
		char bootdrive)
{
	int ret;
	unsigned int *target = (void *)(PAGE0->mem_free + 32*1024);
	struct disk_op_s disk_op = {
		.buf_fl = target,
		.command = CMD_SEEK,
		.count = 0,
		.lba = 0,
	};

	boot_drive = select_parisc_boot_drive(bootdrive);
	disk_op.drive_fl = boot_drive;
	if (boot_drive == NULL) {
		printf("SeaBIOS: No boot device.\n");
		return 0;
	}

	/* seek to beginning of disc/CD */
	disk_op.drive_fl = boot_drive;
	ret = process_op(&disk_op);
	// printf("DISK_SEEK returned %d\n", ret);
	if (ret)
		return 0;

	// printf("Boot disc type is 0x%x\n", boot_drive->type);
	disk_op.drive_fl = boot_drive;
	if (boot_drive->type == DTYPE_ATA_ATAPI ||
	    boot_drive->type == DTYPE_ATA) {
		disk_op.command = CMD_ISREADY;
		ret = process_op(&disk_op);
	} else {
		ret = scsi_is_ready(&disk_op);
	}
	// printf("DISK_READY returned %d\n", ret);

	/* read boot sector of disc/CD */
	disk_op.drive_fl = boot_drive;
	disk_op.buf_fl = target;
	disk_op.command = CMD_READ;
	disk_op.count = (FW_BLOCKSIZE / disk_op.drive_fl->blksize);
	disk_op.lba = 0;
	// printf("blocksize is %d, count is %d\n", disk_op.drive_fl->blksize, disk_op.count);
	ret = process_op(&disk_op);
	// printf("DISK_READ(count=%d) = %d\n", disk_op.count, ret);
	if (ret)
		return 0;

	unsigned int ipl_addr = be32_to_cpu(target[0xf0/sizeof(int)]); /* offset 0xf0 in bootblock */
	unsigned int ipl_size = be32_to_cpu(target[0xf4/sizeof(int)]);
	unsigned int ipl_entry= be32_to_cpu(target[0xf8/sizeof(int)]);

	/* check LIF header of bootblock */
	if ((target[0]>>16) != 0x8000) {
		printf("Not a PA-RISC boot image. LIF magic is 0x%x, should be 0x8000.\n", target[0]>>16);
		return 0;
	}
	// printf("ipl start at 0x%x, size %d, entry 0x%x\n", ipl_addr, ipl_size, ipl_entry);
	// TODO: check ipl values, out of range, ... ?

	/* seek to beginning of IPL */
	disk_op.drive_fl = boot_drive;
	disk_op.command = CMD_SEEK;
	disk_op.count = 0; // (ipl_size / disk_op.drive_fl->blksize);
	disk_op.lba = (ipl_addr / disk_op.drive_fl->blksize);
	ret = process_op(&disk_op);
	// printf("DISK_SEEK to IPL returned %d\n", ret);

	/* read IPL */
	disk_op.drive_fl = boot_drive;
	disk_op.buf_fl = target;
	disk_op.command = CMD_READ;
	disk_op.count = (ipl_size / disk_op.drive_fl->blksize);
	disk_op.lba = (ipl_addr / disk_op.drive_fl->blksize);
	ret = process_op(&disk_op);
	// printf("DISK_READ IPL returned %d\n", ret);

	// printf("First word at %p is 0x%x\n", target, target[0]);

	/* execute IPL */
	// TODO: flush D- and I-cache, not needed in emulation ?
	*iplstart = *iplend = (unsigned long) target;
	*iplstart += ipl_entry;
	*iplend += ALIGN(ipl_size, sizeof(unsigned long));
	return 1;
}


/********************************************************
 * FIRMWARE MAIN ENTRY POINT
 ********************************************************/

extern char pdc_entry;
extern char iodc_entry;

static const struct pz_device mem_cons_boot = {
	.hpa = DINO_UART_HPA,
	.iodc_io = (unsigned long) &iodc_entry,
	.cl_class = CL_DUPLEX,
};

static const struct pz_device mem_boot_boot = {
	.dp.flags = PF_AUTOBOOT,
	.hpa = IDE_HPA,
	.iodc_io = (unsigned long) &iodc_entry,
	.cl_class = CL_RANDOM,
};

static const struct pz_device mem_kbd_boot = {
	.hpa = DINO_UART_HPA,
	.iodc_io = (unsigned long) &iodc_entry,
	// .cl_class = CL_DUPLEX,
	.cl_class = CL_KEYBD,
};

static void parisc_vga_init(void)
{
	extern void vga_post(struct bregs *);
	struct pci_device *pci;

	foreachpci(pci) {
		if (!is_pci_vga(pci))
			continue;

		VgaBDF = pci->bdf;

		parisc_vga_mem = pci_config_readl(pci->bdf, PCI_BASE_ADDRESS_0);
		parisc_vga_mem &= PCI_BASE_ADDRESS_MEM_MASK;
		VBE_framebuffer = parisc_vga_mem;
		parisc_vga_mmio = pci_config_readl(pci->bdf, PCI_BASE_ADDRESS_2);
		parisc_vga_mmio &= PCI_BASE_ADDRESS_MEM_MASK;

		dprintf(1, "\n");

		regs.ax = VgaBDF;
		vga_post(&regs);

		// flag = MF_NOCLEARMEM ?
		//
		// vga_set_mode(0x119, MF_NOCLEARMEM); // bochs: MM_DIRECT, 1280, 1024, 15, 8, 16,
		// vga_set_mode(0x105, 0); // bochs:     { 0x105, { MM_PACKED, 1024, 768,  8,  8, 16, SEG_GRAPH } },
		// vga_set_mode(0x107, 0); // bochs:  { 0x107, { MM_PACKED, 1280, 1024, 8,  8, 16, SEG_GRAPH } },
		// vga_set_mode(0x11c, 0); // bochs:  { 0x11C, { MM_PACKED, 1600, 1200, 8,  8, 16, SEG_GRAPH } },
		// vga_set_mode(0x11f, 0); // bochs:  { 0x11F, { MM_DIRECT, 1600, 1200, 24, 8, 16, SEG_GRAPH } },
		// vga_set_mode(0x101, 0); // bochs:  { 0x101, { MM_PACKED, 640,  480,  8,  8, 16, SEG_GRAPH } },
		vga_set_mode(0x100, 0); // bochs:  { 0x100, { MM_PACKED, 640,  400,  8,  8, 16, SEG_GRAPH } },

		u32 endian = *(u32 *)(parisc_vga_mmio + 0x0604);
		dprintf(1, "parisc: VGA at %pP, mem 0x%lx  mmio 0x%lx endian 0x%x found.\n",
			pci, parisc_vga_mem, parisc_vga_mmio, endian);

		struct vgamode_s *vmode_g = get_current_mode();
		int bpp = vga_bpp(vmode_g);
		int linelength = vgahw_get_linelength(vmode_g);

		dprintf(1, "parisc: VGA resolution: %dx%d-%d  memmodel:%d  bpp:%d  linelength:%d\n",
			    vmode_g->width, vmode_g->height, vmode_g->depth,
			    vmode_g->memmodel, bpp, linelength);
        }
}

void __VISIBLE start_parisc_firmware(void)
{
	unsigned int cpu_hz;
	unsigned long iplstart, iplend;

	extern unsigned long boot_args[5];
	unsigned long ram_size		 = boot_args[0];
	unsigned long linux_kernel_entry = boot_args[1];
	unsigned long cmdline		 = boot_args[2];
	unsigned long initrd_start	 = boot_args[3];
	unsigned long initrd_end	 = boot_args[4];

	unsigned long interactive = (linux_kernel_entry == 1) ? 1:0;
	char bootdrive = (char) cmdline; // c = hdd, d = CD/DVD

	/* Initialize device list */
	remove_parisc_devices();

	/* Initialize PAGE0 */
	memset((void*)PAGE0, 0, sizeof(*PAGE0));
	PAGE0->memc_cont = ram_size;
	PAGE0->memc_phsize = ram_size;
	PAGE0->mem_free = 0x4000; // min PAGE_SIZE
	PAGE0->mem_hpa = CPU_HPA; // HPA of boot-CPU
	PAGE0->mem_pdc = (unsigned long) &pdc_entry;
	PAGE0->mem_10msec = CPU_CLOCK_MHZ*(1000000ULL/100);

	memcpy((char*)&PAGE0->pad0, "SeaBIOS", 8); /* QEMU/SeaBIOS marker */

	// PAGE0->imm_hpa = XXX; TODO?
	PAGE0->imm_max_mem = ram_size;
	memcpy((void*)&(PAGE0->mem_cons), &mem_cons_boot, sizeof(mem_cons_boot));
	memcpy((void*)&(PAGE0->mem_boot), &mem_boot_boot, sizeof(mem_boot_boot));
	memcpy((void*)&(PAGE0->mem_kbd),  &mem_kbd_boot, sizeof(mem_kbd_boot));

	init_stable_storage();

	malloc_preinit();

	// set Qemu serial debug port
	DebugOutputPort = PORT_SERIAL1;
	// PlatformRunningOn = PF_QEMU;  // emulate runningOnQEMU()

	cpu_hz = 100 * PAGE0->mem_10msec; /* Hz of this PARISC */
	printf("\n");
	printf("PARISC SeaBIOS Firmware, 1 x PA7300LC (PCX-L2) at %d.%06d MHz, %lu MB RAM.\n",
		cpu_hz / 1000000, cpu_hz % 1000000,
		ram_size/1024/1024);

	if (ram_size < 16*1024*1024) {
		printf("\nSeaBIOS: Machine configured with too little "
			"memory (%ld MB), minimum is 16 MB.\n\n",
			ram_size/1024/1024);
		hlt();
	}

	// handle_post();
	serial_debug_preinit();
	debug_banner();
	// maininit();
	qemu_preinit();
	// coreboot_preinit();

	pci_setup();

	serial_setup();
	block_setup();

	// We don't have VGA BIOS, so init now.
	parisc_vga_init();

	printf("\n");

	/* directly start Linux kernel if it was given on qemu command line. */
	if (linux_kernel_entry > 1) {
		void (*start_kernel)(unsigned long mem_free, unsigned long cmdline,
			unsigned long rdstart, unsigned long rdend);

		printf("Starting Linux kernel which was loaded by qemu.\n\n");
		start_kernel = (void *) linux_kernel_entry;
		start_kernel(PAGE0->mem_free, cmdline, initrd_start, initrd_end);
		hlt(); /* this ends the emulator */
	}

	printf("Firmware Version  6.1\n"
		"\n"
		"Duplex Console IO Dependent Code (IODC) revision 1\n"
		"\n"
		"Memory Test/Initialization Completed\n\n");
	printf("------------------------------------------------------------------------------\n"
		"   (c) Copyright 2017 Helge Deller <deller@gmx.de> and SeaBIOS developers.\n"
		"------------------------------------------------------------------------------\n\n");
	printf("  Processor   Speed            State           Coprocessor State  Cache Size\n"
		"  ---------  --------   ---------------------  -----------------  ----------\n"
		"      0      " __stringify(CPU_CLOCK_MHZ) " MHz    Active                 Functional          64 KB\n"
		"                                                                    1 MB ext\n\n\n");
	printf("  Available memory:     %llu MB\n"
		"  Good memory required: 64 MB\n\n",
		(unsigned long long)ram_size/1024/1024);
	printf("  Primary boot path:    FWSCSI.6.0\n"
		"  Alternate boot path:  LAN.0.0.0.0.0.0\n"
		"  Console path:         SERIAL_1.9600.8.none\n"
		"  Keyboard path:        PS2\n\n");
#if 0
	printf("------- Main Menu -------------------------------------------------------------\n\n"
		"        Command                         Description\n"
		"        -------                         -----------\n"
		"        BOot [PRI|ALT|<path>]           Boot from specified path\n"
		"        PAth [PRI|ALT|CON|KEY] [<path>] Display or modify a path\n"
		"        SEArch [DIsplay|IPL] [<path>]   Search for boot devices\n\n"
		"        COnfiguration [<command>]       Access Configuration menu/commands\n"
		"        INformation [<command>]         Access Information menu/commands\n"
		"        SERvice [<command>]             Access Service menu/commands\n\n"
		"        DIsplay                         Redisplay the current menu\n"
		"        HElp [<menu>|<command>]         Display help for menu or command\n"
		"        RESET                           Restart the system\n"
		"-------\n"
		"Main Menu: Enter command > ");
#endif

	/* check for bootable drives, and load and start IPL bootloader if possible */
	if (parisc_boot_menu(&iplstart, &iplend, bootdrive)) {
		void (*start_ipl)(long interactive, long iplend);

		printf("\nBooting...\n"
			"Boot IO Dependent Code (IODC) revision 153\n\n"
			"%s Booted.\n", PAGE0->imm_soft_boot ? "SOFT":"HARD");
		start_ipl = (void *) iplstart;
		start_ipl(interactive, iplend);
	}

	hlt(); /* this ends the emulator */
}
