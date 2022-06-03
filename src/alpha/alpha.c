/* Initialization of the system and the HWRPB.

   Copyright (C) 2011 Richard Henderson

   This file is part of QEMU PALcode.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the text
   of the GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; see the file COPYING.  If not see
   <http://www.gnu.org/licenses/>.

cd /home/cvs/LINUX/qemu-alpha

Architecture:
https://download.majix.org/dec/alpha_arch_ref.pdf

milo:
https://johnvidler.co.uk/linux-journal/LJ/021/1202.html

./qemu-system-alpha -hda hdd.img -kernel vmlinuz.11 -initrd ./initrd.gz.11 -append "console=tty0  "  -drive file=../qemu-images/debian-11.0.0-alpha-NETINST-1.iso,if=ide,media=cdrom -serial mon:stdio -accel tcg,thread=multi -smp cpus=1 -m 1G -bios ../seabios/out/alpha-firmware.img  -d trace:*xcchip*,trace:*conf* -nographic  -d in_asm,nochain,out_asm,exec -D log # -dfilter 0x0000000000000000..0x0000000020000fff #-dfilter 0xfffffc0000000080..0xfffffc0000000084  -nographic

*/

#include "config.h"
#include "util.h"
#include "bregs.h" // struct bregs
#include "stacks.h" // struct mutex_s
#include "std/bda.h" // struct bios_data_area_s
#include "vgabios.h" // struct vgamode_s
#include "hw/pci.h" // pci_enable_mmconfig
#include "block.h" // disk_op_s
#include "hw/blockcmd.h" // scsi_is_ready()
#include "output.h" // printf()
#include "vgahw.h"
#include "hwrpb.h"
#include "osf.h"
#include "ioport.h"
#include "uart.h"
#include "protos.h"
#include "memmap.h"
#include SYSTEM_H

#define PAGE_OFFSET	0xfffffc0000000000UL

// #define VPTPTR		0xfffffffe00000000UL
#define VPTPTR		0x200000000UL

#define PA(VA)		((unsigned long)(VA) & 0xfffffffffful)
#define VA(PA)		((void *)(PA) + PAGE_OFFSET)

#define INIT_BOOTLOADER (char *) 0x20000000
// #define BOOTLOADER_MAXSIZE      128*1024
unsigned long *bootloader_code;
unsigned long bootloader_size;
unsigned long bootloader_stack;
unsigned long bootloader_mem;
#define SECT_SIZE 512

#define HZ	1024

/*
 * Various variables which are needed by x86 code.
 * Defined here to be able to link seabios.
 */
int HaveRunPost;
u8 ExtraStack[BUILD_EXTRA_STACK_SIZE+1] __aligned(8);
u8 *StackPos;

volatile int num_online_cpus;

struct drive_s *boot_drive;

u8 BiosChecksum;

char zonefseg_start, zonefseg_end;  // SYMBOLS
char varlow_start, varlow_end, final_varlow_start;
char final_readonly_start;
char code32flat_start, code32flat_end;
char zonelow_base;

struct bios_data_area_s __VISIBLE bios_data_area;
struct vga_bda_s	__VISIBLE vga_bios_data_area;
struct floppy_dbt_s diskette_param_table;
struct bregs regs;
struct segoff_s ivt_table[256];

void mtrr_setup(void) { }
void mouse_init(void) { }
void pnp_init(void) { }
u16  get_pnp_offset(void) { return 0; }
void mathcp_setup(void) { }
void smp_setup(void) { }
void bios32_init(void) { }
void yield_toirq(void) { }
void farcall16(struct bregs *callregs) { }
void farcall16big(struct bregs *callregs) { }
void mutex_lock(struct mutex_s *mutex) { }
void mutex_unlock(struct mutex_s *mutex) { }
void start_preempt(void) { }
void finish_preempt(void) { }
int wait_preempt(void) { return 0; }
int acpi_dsdt_present_eisaid(u16 eisaid) { return 0; }

void cpuid(u32 index, u32 *eax, u32 *ebx, u32 *ecx, u32 *edx)
{
    *eax = *ebx = *ecx = *edx = 0;
}

void wrmsr_smp(u32 index, u64 val) { }


/* alpha code */
/* Upon entry, register a2 contains configuration information from the VM:

   bits 0-5 -- ncpus
   bit  6   -- "nographics" option (used to initialize CTB)  */

#define CONFIG_NCPUS(x)      ((x) & 63)
#define CONFIG_NOGRAPHICS(x) ((x) & (1ull << 6))

struct hwrpb_combine {
  struct hwrpb_struct hwrpb;
  struct percpu_struct processor[4];
  struct memdesc_struct md;
  struct memclust_struct mc[2];
  struct ctb_struct ctb;
  struct crb_struct crb;
  struct procdesc_struct proc_dispatch;
  struct procdesc_struct proc_fixup;
};

extern char stack[PAGE_SIZE] __attribute__((section(".sbss")));
extern char _end[] __attribute__((visibility("hidden"), nocommon));

struct pcb_struct pcb __attribute__((section(".sbss")));

static unsigned long page_dir[1024]
  __attribute__((aligned(PAGE_SIZE), section(".bss.page_dir")));

/* The HWRPB must be aligned because it is exported at INIT_HWRPB.  */
struct hwrpb_combine hwrpb __attribute__((aligned(PAGE_SIZE)));

void *last_alloc;
bool have_vga;
unsigned int pci_vga_bus;
unsigned int pci_vga_dev;

void *
arch_malloc(unsigned long size, unsigned long align)
{
  void *p = (void *)(((unsigned long)last_alloc + align - 1) & ~(align - 1));
  last_alloc = p + size;
  return memset (p, 0, size);
}

static inline unsigned long
pt_index(unsigned long addr, int level)
{
  return (addr >> (PAGE_SHIFT + (10 * level))) & 0x3ff;
}

static inline unsigned long
build_pte (void *page)
{
  unsigned long bits;

  bits = PA((unsigned long)page) << (32 - PAGE_SHIFT);
  bits += _PAGE_VALID | _PAGE_KRE | _PAGE_KWE;

  return bits;
}

static inline void *
pte_page (unsigned long pte)
{
  return VA(pte >> 32 << PAGE_SHIFT);
}

static void
set_pte (unsigned long addr, void *page)
{
  unsigned long *pt = page_dir;
  unsigned long index;

  index = pt_index(addr, 2);
  if (pt[index] != 0)
    pt = pte_page (pt[index]);
  else
    {
      unsigned long *npt = arch_malloc(PAGE_SIZE, PAGE_SIZE);
      pt[index] = build_pte (npt);
      pt = npt;
    }

  index = pt_index(addr, 1);
  if (pt[index] != 0)
    pt = pte_page (pt[index]);
  else
    {
      unsigned long *npt = arch_malloc(PAGE_SIZE, PAGE_SIZE);
      pt[index] = build_pte (npt);
      pt = npt;
    }

  index = pt_index(addr, 0);
  pt[index] = build_pte (page);
}

static void
init_page_table(void)
{
  /* Install the self-reference for the virtual page table base register.  */
  page_dir[pt_index(VPTPTR, 2)] = build_pte(page_dir);

  set_pte ((unsigned long)INIT_HWRPB, &hwrpb);
}

static void
init_hwrpb (unsigned long memsize, unsigned long config)
{
  unsigned long pal_pages;
  unsigned long amask;
  unsigned long i;
  unsigned long proc_type = EV4_CPU;
  unsigned long cpus = CONFIG_NCPUS(config);
  
  hwrpb.hwrpb.phys_addr = PA(&hwrpb);

  /* Yes, the 'HWRPB' magic is in big-endian byte ordering.  */
  hwrpb.hwrpb.id = ( (long)'H' << 56
		   | (long)'W' << 48
		   | (long)'R' << 40
		   | (long)'P' << 32
		   | (long)'B' << 24);

  hwrpb.hwrpb.size = sizeof(struct hwrpb_struct);

  ((int *)hwrpb.hwrpb.ssn)[0] = ( 'Q' << 0
				| 'E' << 8
				| 'M' << 16
				| 'U' << 24);

  amask = ~__builtin_alpha_amask(-1);
dprintf(1,"amask %lx\n", amask);
  switch (__builtin_alpha_implver())
    {
    case 0: /* EV4 */
      proc_type = EV4_CPU;
      hwrpb.hwrpb.max_asn = 63;
      break;

    case 1: /* EV5 */
      proc_type
	= ((amask & 0x101) == 0x101 ? PCA56_CPU		/* MAX+BWX */
	   : amask & 1 ? EV56_CPU			/* BWX */
	   : EV5_CPU);
      hwrpb.hwrpb.max_asn = 127;
      break;

    case 2: /* EV6 */
      proc_type = (amask & 4 ? EV67_CPU : EV6_CPU);     /* CIX */
      hwrpb.hwrpb.max_asn = 255;
      break;
    }

dprintf(1,"proc_type %lx\n", proc_type);
  /* This field is the WHAMI of the primary CPU.  Just initialize
     this to 0; CPU #0 is always the primary on real Alpha systems
     (except for the TurboLaser).  */
  hwrpb.hwrpb.cpuid = 0;

  hwrpb.hwrpb.pagesize = PAGE_SIZE;
  hwrpb.hwrpb.pa_bits = 40;
  hwrpb.hwrpb.sys_type = SYS_TYPE;
  hwrpb.hwrpb.sys_variation = SYS_VARIATION;
  hwrpb.hwrpb.sys_revision = SYS_REVISION;
  for (i = 0; i < cpus; ++i)
    {
      /* Set the following PCS flags:
	 (bit 2) Processor Available
	 (bit 3) Processor Present
	 (bit 6) PALcode Valid
	 (bit 7) PALcode Memory Valid
	 (bit 8) PALcode Loaded

	 ??? We really should be intializing the PALcode memory and
	 scratch space fields if we're setting PMV, or not set PMV,
	 but Linux checks for it, so...  */
      hwrpb.processor[i].flags = 0x1cc;
      hwrpb.processor[i].type = proc_type;
    }

  hwrpb.hwrpb.intr_freq = HZ * 4096;
  hwrpb.hwrpb.cycle_freq = 250000000;	/* QEMU architects 250MHz.  */

  hwrpb.hwrpb.vptb = VPTPTR;

  hwrpb.hwrpb.nr_processors = cpus;
  hwrpb.hwrpb.processor_size = sizeof(struct percpu_struct);
  hwrpb.hwrpb.processor_offset = offsetof(struct hwrpb_combine, processor);

  hwrpb.hwrpb.mddt_offset = offsetof(struct hwrpb_combine, md);
  hwrpb.md.numclusters = 2;

  pal_pages = (PA(last_alloc) + PAGE_SIZE - 1) >> PAGE_SHIFT;

  hwrpb.mc[0].numpages = pal_pages;
  hwrpb.mc[0].usage = 1;
  hwrpb.mc[1].start_pfn = pal_pages;
  hwrpb.mc[1].numpages = (memsize >> PAGE_SHIFT) - pal_pages;

  hwrpb.hwrpb.ctbt_offset = offsetof(struct hwrpb_combine, ctb);
  hwrpb.hwrpb.ctb_size = sizeof(hwrpb.ctb);
  hwrpb.ctb.len = sizeof(hwrpb.ctb) - offsetof(struct ctb_struct, ipl);
  if (have_vga && !CONFIG_NOGRAPHICS(config))
    {
      hwrpb.ctb.type = CTB_MULTIPURPOSE;
      hwrpb.ctb.term_type = CTB_GRAPHICS;
      hwrpb.ctb.turboslot = (CTB_TURBOSLOT_TYPE_PCI << 16) |
			    (pci_vga_bus << 8) | pci_vga_dev;
    }
  else
    {
      hwrpb.ctb.type = CTB_PRINTERPORT;
      hwrpb.ctb.term_type = CTB_PRINTERPORT;
    }

  hwrpb.hwrpb.crb_offset = offsetof(struct hwrpb_combine, crb);
  hwrpb.crb.dispatch_va = &hwrpb.proc_dispatch;
  hwrpb.crb.dispatch_pa = PA(&hwrpb.proc_dispatch);
  hwrpb.crb.fixup_va = &hwrpb.proc_fixup;
  hwrpb.crb.fixup_pa = PA(&hwrpb.proc_fixup);
  hwrpb.crb.map_entries = 1;
  hwrpb.crb.map_pages = 1;
  hwrpb.crb.map[0].va = &hwrpb;
  hwrpb.crb.map[0].pa = PA(&hwrpb);
  hwrpb.crb.map[0].count = 1;

  /* See crb.c for how we match the VMS calling conventions to Unix.  */
  hwrpb.proc_dispatch.address = (unsigned long)crb_dispatch;
  hwrpb.proc_fixup.address = (unsigned long)crb_fixup;

  hwrpb_update_checksum(&hwrpb.hwrpb);
}

static void
init_pcb (void)
{
  pcb.ksp = (unsigned long)stack + sizeof(stack);
  pcb.ptbr = PA(page_dir) >> PAGE_SHIFT;
  pcb.flags = 1; /* FEN */
}

static void
init_i8259 (void)
{
  /* ??? MILO initializes the PIC as edge triggered; I do not know how SRM
     initializes them.  However, Linux seems to expect that these are level
     triggered.  That may be a kernel bug, but level triggers are more
     reliable anyway so lets go with that.  */

  /* Initialize the slave PIC.  */
  outb(0x11, PORT_PIC2_CMD);	/* ICW1: edge trigger, cascade, ICW4 req */
  outb(0x08, PORT_PIC2_DATA);	/* ICW2: irq offset = 8 */
  outb(0x02, PORT_PIC2_DATA);	/* ICW3: slave ID 2 */
  outb(0x01, PORT_PIC2_DATA);	/* ICW4: not special nested, normal eoi */

  /* Initialize the master PIC.  */
  outb(0x11, PORT_PIC1_CMD);	/* ICW1 */
  outb(0x00, PORT_PIC1_DATA);	/* ICW2: irq offset = 0 */
  outb(0x04, PORT_PIC1_DATA);	/* ICW3: slave control INTC2 */
  outb(0x01, PORT_PIC1_DATA);	/* ICW4 */

  /* Initialize level triggers.  The CY82C693UB that's on some real alpha
     hardware doesn't have this; this is a PIIX extension.  However,
     QEMU doesn't implement regular level triggers.  */
  outb(0xff, PORT_PIC2_ELCR);
  outb(0xff, PORT_PIC1_ELCR);

  /* Disable all interrupts.  */
  outb(0xff, PORT_PIC2_DATA);
  outb(0xff, PORT_PIC1_DATA);

  /* Non-specific EOI, clearing anything the might be pending.  */
  outb(0x20, PORT_PIC2_CMD);
  outb(0x20, PORT_PIC1_CMD);
}

static void __attribute__((noreturn))
swppal(void *entry, void *pcb, unsigned long vptptr, unsigned long pv)
{
  register int variant __asm__("$16") = 2;	/* OSF/1 PALcode */
  register void *pc __asm__("$17") = entry;
  register unsigned long pa_pcb __asm__("$18") = PA(pcb);
  register unsigned long newvptptr __asm__("$19") = vptptr;
  register unsigned long newpv __asm__("$20") = pv;

  asm("call_pal 0x0a" : :
      "r"(variant), "r"(pc), "r"(pa_pcb), "r"(newvptptr), "r"(newpv));
  __builtin_unreachable ();
}

/********************************************************
 * BOOT MENU
 ********************************************************/

extern void find_initial_parisc_boot_drives(
        struct drive_s **harddisc,
        struct drive_s **cdrom);
extern struct drive_s *select_parisc_boot_drive(char bootdrive);

static int alpha_boot_menu(unsigned long *iplstart, unsigned long *iplend,
        char bootdrive)
{
    int i, ret;
    unsigned long *target;
    unsigned long bootblock[2048 / sizeof(long)];

    printf("(boot dka500.5.0.2000.0 -flags 0)\n");
    target = bootblock;
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
    disk_op.count = 1;
    disk_op.lba = 0;
    // printf("blocksize is %d, count is %d\n", disk_op.drive_fl->blksize, disk_op.count);
    ret = process_op(&disk_op);
    // printf("DISK_READ(count=%d) = %d\n", disk_op.count, ret);
    if (ret)
        return 0;
    // printf("bootloader says: %s\n", (char *)target);

    unsigned int  ipl_size = target[0x1e0/sizeof(long)];
    unsigned long ipl_sect = target[0x1e8/sizeof(long)];
    unsigned long ipl_magic= target[0x1f0/sizeof(long)];
    // unsigned long ipl_sum  = target[0x1f8/sizeof(long)];

    /* calc checksum of bootblock and verify */
    u64 sum = 0;
    for (i = 0; i < 63; i++)
        sum += target[i];
    if (sum != target[63]) {
        printf("FAIL: Bootsector checksum error.\n");
        return 0;
    }

    bootloader_size = ipl_size * SECT_SIZE;
    bootloader_mem = (bootloader_size + PAGE_SIZE-1) & ~(PAGE_SIZE-1);
    bootloader_stack = bootloader_mem;
    bootloader_mem += 256*1024;         // add 256k stack size
    // TODO: Add 3 stack guard pages adjacent to the bootstrap stack page

    /* check LIF header of bootblock */
    if (ipl_magic != 0) {
        printf("FAIL: Not an ALPHA boot disc. Magic is wrong.\n");
        return 0;
    }
    // printf("ipl start sector is %ld, ipl size %d\n", ipl_sect, ipl_size);

    if ((ipl_sect * SECT_SIZE) & (disk_op.drive_fl->blksize-1)) {
        printf("FAIL: Bootloader start sector not multiple of device block size.\n");
        return 0;
    }
    printf("block 0 of dka500.5.0.2000.0 is a valid boot block\n");

#if 0
>>>boot
(boot dka500.5.0.2000.0 -flags 0)
block 0 of dka500.5.0.2000.0 is a valid boot block
reading 163 blocks from dka500.5.0.2000.0
bootstrap code read in
Building FRU table
FRU table size = 0x912
base = 1b8000, image_start = 0, image_bytes = 14600
initializing HWRPB at 2000
initializing page table at 3ffce000
initializing machine state
setting affinity to the primary CPU
jumping to bootstrap code
aboot: Linux/Alpha SRM bootloader version 1.0_pre20040408
aboot: switching to OSF/1 PALcode version 1.45
aboot: booting from device 'SCSI 0 2000 0 5 500 0 0'
aboot: no disklabel found.
iso: Max size:203726   Log zone size:2048
iso: First datazone:19   Root inode number 38912
aboot: loading uncompressed boot/vmlinuz...
aboot: loading compressed boot/vmlinuz...
aboot: zero-filling 454300 bytes at 0xfffffc0002531d24
aboot: loading initrd (16616295 bytes/16226 blocks) at 0xfffffc003ec76000
aboot: starting kernel boot/vmlinuz with arguments ramdisk_size=50342  root=/dev/ram devfs=mount,dall


>>>show dev
dka0.0.0.2000.0            DKA0               SEAGATE ST15150N  HP12
dka500.5.0.2000.0          DKA500                        RRD46  1337
dva0.0.0.1000.0            DVA0                               
mka400.4.0.2000.0          MKA400                        TLZ07  553B
ewa0.0.0.11.0              EWA0              00-00-F8-21-DD-2A
pka0.7.0.2000.0            PKA0                  SCSI Bus ID 7  5.57

#endif

    /* seek to beginning of IPL */
    disk_op.drive_fl = boot_drive;
    disk_op.command = CMD_SEEK;
    disk_op.count = 0;
    disk_op.lba = ipl_sect / (disk_op.drive_fl->blksize / SECT_SIZE);
    ret = process_op(&disk_op);
    // printf("DISK_SEEK to IPL returned %d\n", ret);

    /* read IPL */
    bootloader_code = arch_malloc(bootloader_mem, PAGE_SIZE);
    dprintf(1, "bootloader will be loaded to %p\n", bootloader_code);
    target = bootloader_code;
    printf("reading %d blocks from dka500.5.0.2000.0\n", ipl_size);
    disk_op.drive_fl = boot_drive;
    disk_op.buf_fl = target;
    disk_op.command = CMD_READ;
    disk_op.count = (ipl_size * SECT_SIZE / disk_op.drive_fl->blksize) + 1;
    disk_op.lba = ipl_sect / (disk_op.drive_fl->blksize / SECT_SIZE);
    ret = process_op(&disk_op);
    // printf("DISK_READ IPL returned %d  count=%d  lba=%d\n", ret, disk_op.count, disk_op.lba);

    printf("bootstrap code read in\n");
    // printf("First word at %p is 0x%lx\n", target, target[0]);

    /* execute IPL */
    ret = bootloader_mem;
    unsigned long pageno = 0;
    while (ret > 0) {
        unsigned long virt;
        char *phys;
        virt = (unsigned long)INIT_BOOTLOADER + pageno * PAGE_SIZE;
        phys = (char *)target;
        phys += pageno * PAGE_SIZE;
        set_pte (virt, phys);
        dprintf(1, "set bootloader PTE addr 0x%lx  -> phys addr 0x%p\n", virt, phys);
        pageno++;
        ret -= PAGE_SIZE;
    }
    // set HWRPB pte entry
    set_pte ((unsigned long)INIT_HWRPB, &hwrpb);

#if 0
    dprintf(1, "PTE VPTR %d \n", pt_index(VPTPTR, 2));
  unsigned long addr = 0x200802000UL;
  // unsigned long addr = 0x200800000UL;
  unsigned long *pt = page_dir;
  unsigned long index;
    set_pte (addr, &page_dir); // create page at addr for aboot

    index = pt_index(addr, 2);
    dprintf(1, "PTE VPTR 2 -> index %d  pt=0x%lx\n", index, pt);
    pt = pte_page (pt[index]);
    index = pt_index(addr, 1);
    dprintf(1, "PTE VPTR 1 -> index %d  pt=0x%lx\n", index, pt);
    pt = pte_page (pt[index]);
    index = pt_index(addr, 0);
    dprintf(1, "PTE VPTR 0 -> index %d  pt=0x%lx\n", index, pt);
//     pt = pte_page (pt[index]);
//    pt[1] = page_dir[pt_index(VPTPTR, 2)];
    dprintf(1, "PTE VPTR pt 0x%lx \n", pt);

    // L1[1023] = L1[1];

    See: Bootstrap Address Space -> PDF
#endif
  // set level 1 page table pte entry for aboot (see: pal_init())
    // set_pte (0x200000000UL, &page_dir); // XXX
    set_pte (0x200802000UL, &page_dir); // store addr for aboot

    unsigned long *L = page_dir; // (unsigned long *) 0x200802000UL; /* (1<<33 | 1<<23 | 1<<13) */
    dprintf(1, "L1  0x%lx\n", L[1]);
    dprintf(1, "L9  0x%lx\n", L[1023]);
    dprintf(1, "en  0x%lx\n", pt_index(VPTPTR, 2));
    dprintf(1, "La  0x%lx\n", page_dir[pt_index(VPTPTR, 2)]);
    dprintf(1, "L1  0x%lx  0x%lx\n phys", PA((L[1] >> 32) << PAGE_SHIFT), PA(&page_dir));

    void *new_pc = INIT_BOOTLOADER; // target;
    dprintf(1,"STARTING BOOTLOADER NOW at %p\n\n", new_pc);
    swppal(new_pc, &pcb, VPTPTR, (unsigned long)target);

// page 27–17

    return 1;
}



void __VISIBLE
do_start(unsigned long memsize, void (*kernel_entry)(void),
         unsigned long config)
{
  last_alloc = _end;

  init_page_table();
  init_pcb();
  init_i8259();
  uart_init();
  ps2port_setup();
  pci_enable_mmconfig((unsigned long)pci_conf_base, "clipper");
  pci_setup();
  vga_set_mode(0x5f, 0); // statt mode 3 / 0x5c
  serial_setup();
  block_setup();
  init_hwrpb(memsize, config);

  alpha_boot_menu(0,0, 'd');

  void *new_pc = kernel_entry ? kernel_entry : do_console;

dprintf(1,"STARTING KERNEL NOW at %p\n", new_pc);
  swppal(new_pc, &pcb, VPTPTR, (unsigned long)new_pc);
}

void __VISIBLE __noreturn hlt(void)
{
    /* FIXME */
    // asm volatile ("call_pal CallPal_Halt");
    while (1);
}

void __VISIBLE __noreturn reset(void)
{
    /* TODO */
    while (1);
}

void __VISIBLE
do_start_wait(unsigned long cpuid)
{
  while (1)
    {
      /* Wait 1ms for the kernel to wake us.  */
      ndelay(1000000);

      if (hwrpb.hwrpb.rxrdy & (1ull << cpuid))
	{
	  /* ??? The only message I know of is "START\r\n".
	     I can't be bothered to verify more than 4 characters.  */

	  /* Use use a private extension to SWPPAL to get the
	     CPU_restart_data into $27.  Linux fills it in, but does
	     not require it. Other operating systems, however, do use
	     CPU_restart_data as part of secondary CPU start-up.  */

	  unsigned int len = hwrpb.processor[cpuid].ipc_buffer[0];
	  unsigned int msg = hwrpb.processor[cpuid].ipc_buffer[1];
	  void *CPU_restart = hwrpb.hwrpb.CPU_restart;
	  unsigned long CPU_restart_data = hwrpb.hwrpb.CPU_restart_data;
	  __sync_synchronize();
	  hwrpb.hwrpb.rxrdy = 0;

          if (len == 7 && msg == ('S' | 'T' << 8 | 'A' << 16 | 'R' << 24))
	    {
	      /* Set bootstrap in progress */
	      hwrpb.processor[cpuid].flags |= 1;
	      swppal(CPU_restart, hwrpb.processor[cpuid].hwpcb,
		     hwrpb.hwrpb.vptb, CPU_restart_data);
	    }
	}
    }
}
