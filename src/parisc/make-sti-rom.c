/*
 * Build STI ROM for qemu
 *
 * (C) 2024 Helge Deller <deller@gmx.de>
 *
 * See:
 *      https://parisc.wiki.kernel.org/images-parisc/e/e3/Sti.pdf
 */

#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdint.h>
#include <assert.h>
#include <string.h>

#include "out/sti-rom-offsets.h"
#include "src/hw/pci_ids.h"

#define BUILD_STI_ROM
#include "src/parisc/sticore.h"

#define ROMSIZE         64 * 1024
#define BLOCKSIZE       512
#define STI_ROM_OFFSET  sizeof(struct pci_rom_header)   /* STI ROM starts after PCI ROM HEADER */

#define offsetof(TYPE, MEMBER) ((unsigned long) &((TYPE *)0)->MEMBER)

struct pci_data_structure {
    char        sig[4];
    uint16_t    ven, dev, res1, ds_len;
    uint32_t    class_and_rev;
    uint16_t    len, rev;
    char        code, indicator, res2, res3;
};

struct pci_rom_header {
    uint32_t    sig;
    uint32_t    rom_type;
    uint32_t    sti_image_offset;
    uint16_t    rom_size;       /* multiple of 512 */
    uint16_t    sti_region_mapper_offset;
    uint32_t    empty[2];
    uint16_t    pci_ds_offset;
    struct pci_data_structure ds;
    char        pci_region_mapper[16];
};

char *rom;
struct pci_rom_header *rh;
char *sti_rom_start;

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    #define host_to_be16(x)     __builtin_bswap16(x)
    #define host_to_be32(x)     __builtin_bswap32(x)
    #define host_to_le16(x)     (x)
    #define host_to_le32(x)     (x)
#else
    #define host_to_be16(x)     (x)
    #define host_to_be32(x)     (x)
    #define host_to_le16(x)     __builtin_bswap16(x)
    #define host_to_le32(x)     __builtin_bswap32(x)
#endif

int main(int argc, char *argv[])
{
    int fh;
    ssize_t size;
    unsigned long addr;

    rom = malloc(ROMSIZE);
    assert(rom);
    memset(rom, 0, ROMSIZE);

    fh = open(argv[1], O_RDONLY);
    assert(fh >= 0);
    sti_rom_start = &rom[STI_ROM_OFFSET];
    size = read(fh, sti_rom_start, 2 * ROMSIZE);
    assert(size < ROMSIZE);
    close(fh);

    rh = (void *)rom;
    rh->sig = host_to_le32(0xaa55);
    rh->rom_type = host_to_le32(1 << 24);
    rh->sti_image_offset = host_to_le32(STI_ROM_OFFSET);
    rh->rom_size = host_to_le16(ROMSIZE / BLOCKSIZE);
    rh->sti_region_mapper_offset = host_to_le16(
        offsetof(struct pci_rom_header, pci_region_mapper));
    rh->pci_region_mapper[0] = 0x10; /* PCI_BASE_ADDRESS_0 */
    rh->pci_region_mapper[1] = 0x10; /* PCI_BASE_ADDRESS_0 */
    rh->pci_region_mapper[2] = 0x10; /* PCI_BASE_ADDRESS_0 */
    rh->pci_region_mapper[3] = 0x10; /* PCI_BASE_ADDRESS_0 */
    rh->pci_ds_offset = host_to_le16(offsetof(struct pci_rom_header, ds));

    rh->ds.sig[0] = 'P';
    rh->ds.sig[1] = 'C';
    rh->ds.sig[2] = 'I';
    rh->ds.sig[3] = 'R';
    rh->ds.ven = host_to_le16(PCI_VENDOR_ID_HP);
    rh->ds.dev = host_to_le16(PCI_DEVICE_ID_HP_VISUALIZE_EG);
    rh->ds.ds_len = host_to_le16(sizeof(rh->ds));
    rh->ds.class_and_rev = host_to_le32(PCI_CLASS_DISPLAY_OTHER << 16);
    rh->ds.len = host_to_le16(ROMSIZE / BLOCKSIZE);
    rh->ds.rev = host_to_le16(0);
    rh->ds.code = 0x10; /* Note: NOT 2, as documented in PCI spec! */
    rh->ds.indicator = 1<<7;    /* last image */


#define STI_OFFSET(_x) ((off_ ## _x) - off_sti_proc_rom)

    struct sti_rom *sti_proc_rom;
    unsigned int sti_rom_size;

    sti_proc_rom = (void *)sti_rom_start;

#if 0
    region_t *sti_region_list = sti_rom_start + STI_OFFSET(sti_region_list);

    // sti_rom_size = (_sti_rom_end - _sti_rom_start) / 4096;
    sti_rom_size = STI_OFFSET(sti_end) / 4096;
    if (sti_region_list[0].region_desc.length != sti_rom_size) {
        /* The STI ROM size is wrong. Fix it. */
        region_t *rp = (region_t *) &sti_region_list[0];
        rp->region_desc.length = sti_rom_size;
    }
#endif

#if 0
    extern int sti_font; /* in parisc.c, from opt/font fw_cfg option */
    struct font *font_ptr, *font_next, *font_start;
    int i;

    /* Swap selected font as first, then chain all. */
    if (sti_font <= 0 || sti_font > ARRAY_SIZE(fontlist))
	sti_font = 1;

    /* Swap selected font in as first font */
    font_next = fontlist[0];
    fontlist[0] = fontlist[sti_font - 1];
    fontlist[sti_font - 1] = font_next;

#define ENABLE_FONTCOPY
#ifdef ENABLE_FONTCOPY
    {
    struct font *fontlist_temp[ARRAY_SIZE(fontlist)];
    unsigned long font_size, font_addr;
    /* * The trivial swapping above breaks the sti driver on older 64-bit Linux
     * kernels which take the "next_font" pointer as unsigned int (instead of
     * signed int) and thus calculates a wrong font start address.  Avoid the crash
     * by sorting the fonts in memory. Maybe HP-UX has problems with that too?
     * A Linux kernel patch to avoid that was added in kernel 6.7.
     */
    #define FONT_SIZE(f) (sizeof(f->hdr) + f->hdr.bytes_per_char * (unsigned int)(f->hdr.last_char - f->hdr.first_char + 1))
    font_start = fontlist[0];   /* the lowest font ptr */
    for (i = 0; i < ARRAY_SIZE(fontlist); i++) {
	font_ptr = fontlist[i];
        if (font_ptr < font_start)      /* is font ptr lower? */
            font_start = font_ptr;
        font_size = FONT_SIZE(font_ptr);
        font_next = malloc_tmplow(font_size);
        memcpy(font_next, font_ptr, font_size);
        fontlist_temp[i] = font_next;
    }
    /* font_start is now starting address */
    font_addr = (unsigned long) font_start;
    for (i = 0; i < ARRAY_SIZE(fontlist); i++) {
	font_ptr = fontlist_temp[i];
        font_size = FONT_SIZE(font_ptr);
        memcpy((void *)font_addr, font_ptr, font_size);
        malloc_pfree((unsigned long) font_ptr);
        fontlist[i] = (struct font *) font_addr;
        font_addr += font_size;
        font_addr = ALIGN(font_addr, 32);
    }
    }
#endif

    /* Now chain all fonts */
    font_start = fontlist[0];
    sti_proc_rom.font_start = STI_OFFSET(*font_start);
    for (i = 0; i < ARRAY_SIZE(fontlist)-1; i++) {
	font_ptr = fontlist[i];
        if (i ==  ARRAY_SIZE(fontlist)-1) {
            /* last entry? */
            font_ptr->hdr.next_font = 0;
            continue;
        }
	font_next = fontlist[i+1];
	font_ptr->hdr.next_font = STI_FONT_OFFSET(font_next, font_start);
    }

#endif


    addr = STI_OFFSET(sti_end);
    addr = (addr + 4096) & ~(4096-1);   // align to page boundary
    addr -= 1;
    sti_proc_rom->last_addr = host_to_be32(addr);

    sti_proc_rom->font_start = host_to_be32(STI_OFFSET(sti_rom_font_10x20));

    sti_proc_rom->state_mgmt = host_to_be32(STI_OFFSET(sti_state_mgmt));
    sti_proc_rom->inq_conf = host_to_be32(STI_OFFSET(sti_inq_conf));
    sti_proc_rom->init_graph =  host_to_be32(STI_OFFSET(sti_init_graph));
    sti_proc_rom->block_move = host_to_be32(STI_OFFSET(sti_bmove));
    sti_proc_rom->region_list = host_to_be32(STI_OFFSET(sti_region_list));
    sti_proc_rom->font_unpmv = host_to_be32(STI_OFFSET(sti_font_unpmv));
    sti_proc_rom->self_test = host_to_be32(STI_OFFSET(sti_self_test));
    sti_proc_rom->mon_tbl_addr = host_to_be32(STI_OFFSET(sti_mon_table));
    sti_proc_rom->user_data_addr = host_to_be32(STI_OFFSET(sti_user_data));
    sti_proc_rom->excep_hdlr = host_to_be32(STI_OFFSET(sti_excep_hdlr));
    sti_proc_rom->set_cm_entry = host_to_be32(STI_OFFSET(sti_set_cm_entry));
    sti_proc_rom->dma_ctrl = host_to_be32(STI_OFFSET(sti_dma_ctrl));
    sti_proc_rom->end = host_to_be32(STI_OFFSET(sti_end));
//     update_crc(&sti_proc_rom);


    /* finally write ROM image */
    fh = creat(argv[2], 0600);
    assert(fh >= 0);
    write(fh, rom, ROMSIZE);
    close(fh);
}

