#!/usr/bin/env python3
# Build the PCI expansion ROM of an HP VISUALIZE-EG (Artist on PCI) card:
# a PCI ROM header, the region map and an STI ROM image with 32-bit code
# and 64-bit (alternate) code.
#
# This file may be distributed under the terms of the GNU LGPLv3 license.
#
# The STI header is filled in here, as sti_rom_init() does it at run time
# for the STI ROM in the firmware. Inputs are the .sti section of
# src/parisc/stirom.c built for 32 and for 64 bits, with nm output.
#
# HP-UX copies each STI routine to RAM on its own: from its offset in the
# header to the next offset. So the routines keep their linker order and
# call nothing outside of themselves.

import argparse, struct, sys

# Routines in header and linker order (32-bit offsets at 0x40, 64-bit
# offsets at 0x80). 'end' follows the last routine.
ROUTINES = [
    ("init_graph", "sti_init_graph"),
    ("state_mgmt", "sti_state_mgmt"),
    ("font_unpmv", "sti_font_unpmv"),
    ("block_move", "sti_bmove"),
    ("self_test", "sti_self_test"),
    ("excep_hdlr", "sti_excep_hdlr"),
    ("inq_conf", "sti_inq_conf"),
    ("set_cm_entry", "sti_set_cm_entry"),
    ("dma_ctrl", "sti_dma_ctrl"),
]
FONTS = ["sti_rom_font_8x16", "sti_rom_font_6x13",
         "sti_rom_font_10x20", "sti_rom_font_16x32"]  # fontlist[] order

# struct sti_rom offsets
OFF_GRAPHICS_ID = 0x08
OFF_FONT_START = 0x10
OFF_LAST_ADDR = 0x18
OFF_REGION_LIST = 0x1c
OFF_MON_TBL = 0x24
OFF_USER_DATA = 0x28
OFF_ALT_CODE_TYPE = 0x38
OFF_ROUTINES32 = 0x40           # init_graph .. sti_util, then end
OFF_END32 = 0x74
OFF_ROUTINES64 = 0x80           # init_graph_addr ..
ALT_CODE_TYPE_PA_RISC_64 = 0x01

STI_OFFSET = 0x100              # STI image in the PCI ROM
REGION_MAP_OFFSET = 0x40
PCIR_OFFSET = 0x1c


def read_nm(path):
    syms = {}
    for line in open(path):
        f = line.split()
        if len(f) == 3:
            syms[f[2]] = int(f[0], 16)
    return syms


def align(v, a):
    return (v + a - 1) & ~(a - 1)


def put32(buf, off, val):
    buf[off:off + 4] = struct.pack(">I", val)


def crc(data):
    # update_crc() in src/parisc/stirom.c
    code = accum = 0
    for j, c in enumerate(data):
        accum = ((accum << 8) | c) & 0xffff
        if j & 1:
            accum ^= code
            for i in range(16):
                if accum & 0x8000:
                    accum = ((accum << 1) | 1) & 0xffff
                    accum ^= 0x8408
                else:
                    accum = (accum << 1) & 0xffff
            code = accum
    return code


def main():
    p = argparse.ArgumentParser()
    p.add_argument("bin32"); p.add_argument("nm32")
    p.add_argument("bin64"); p.add_argument("nm64")
    p.add_argument("out")
    p.add_argument("--vendor", type=lambda x: int(x, 0), default=0x103c)
    p.add_argument("--device", type=lambda x: int(x, 0), default=0x1005)
    # Keep the Artist ID of stirom.c: HP-UX has its own routines for the
    # real EG ID (0x2d08c0a7), and they use STI regions this card lacks.
    p.add_argument("--graphics-id", type=lambda x: int(x, 0))
    a = p.parse_args()

    img = bytearray(open(a.bin32, "rb").read())
    s32 = read_nm(a.nm32)
    code64 = open(a.bin64, "rb").read()
    s64 = read_nm(a.nm64)

    # 32-bit routines, in place as linked
    for i, (_, sym) in enumerate(ROUTINES):
        put32(img, OFF_ROUTINES32 + 4 * i, s32[sym])
    put32(img, OFF_END32, s32["sti_end"])
    put32(img, OFF_REGION_LIST, s32["sti_region_list"])
    put32(img, OFF_MON_TBL, s32["sti_mon_table"])
    put32(img, OFF_USER_DATA, s32["user_data"])
    if a.graphics_id is not None:
        put32(img, OFF_GRAPHICS_ID, a.graphics_id)

    # Fonts: lay them out again in fontlist[] order and chain them with
    # offsets that go forward only, as sti_rom_init() does.
    fbase = min(s32[f] for f in FONTS)
    fonts = []
    for f in FONTS:
        o = s32[f]
        first, last, _, _, _, bpc = struct.unpack_from(">HHBBBB", img, o)
        size = 16 + bpc * (last - first + 1)
        fonts.append(bytes(img[o:o + size]))
    fend = s32["sti_end"]
    img[fbase:fend] = bytes(fend - fbase)
    pos, starts = fbase, []
    for f in fonts:
        starts.append(pos)
        img[pos:pos + len(f)] = f
        pos = align(pos + len(f), 32)
    assert pos <= fend, "fonts do not fit"
    for i, st in enumerate(starts):
        nxt = starts[i + 1] - st if i + 1 < len(starts) else 0
        put32(img, st + 8, nxt)
    put32(img, OFF_FONT_START, starts[0])

    # 64-bit routines after the 32-bit image
    first64 = s64[ROUTINES[0][1]]
    end64 = s64["_sti_code_end"]
    base64 = align(len(img), 32)
    img += bytes(base64 - len(img)) + code64[first64:end64]
    for i, (_, sym) in enumerate(ROUTINES):
        put32(img, OFF_ROUTINES64 + 4 * i, base64 + s64[sym] - first64)
    put32(img, OFF_ROUTINES64 + 4 * len(ROUTINES), base64 + end64 - first64)
    img[OFF_ALT_CODE_TYPE] = ALT_CODE_TYPE_PA_RISC_64

    # Room for the CRC in the last 2 bytes, then the region 0 length
    img += bytes(align(len(img) + 4, 32) - len(img))
    total = STI_OFFSET + len(img)
    rom_size = align(total, 512)
    pages = align(rom_size, 4096) // 4096
    r0 = s32["sti_region_list"]
    reg0 = struct.unpack_from(">I", img, r0)[0]
    reg0 = (reg0 & ~0x3fff) | pages          # length: low 14 bits
    put32(img, r0, reg0)
    put32(img, OFF_LAST_ADDR, len(img) - 1)
    img[-2:] = struct.pack(">H", crc(img[:-2]))

    # PCI ROM header (little-endian fields, as PCI wants)
    rom = bytearray(rom_size)
    rom[0:2] = b"\x55\xaa"
    rom[4:8] = struct.pack(">I", 1)                  # an STI image
    rom[8:12] = struct.pack("<I", STI_OFFSET)
    rom[12:16] = struct.pack("<HH", rom_size // 512, REGION_MAP_OFFSET)
    rom[0x18:0x1a] = struct.pack("<H", PCIR_OFFSET)
    rom[PCIR_OFFSET:PCIR_OFFSET + 0x18] = (
        b"PCIR" + struct.pack("<HHHHB", a.vendor, a.device, 0, 0x18, 0)
        + bytes([0x00, 0x80, 0x03])                  # class: display, other
        + struct.pack("<HH", rom_size // 512, 1)
        + bytes([0x10, 0x80, 0, 0]))                 # code type, last image
    # Region map: config space offset of the BAR for each STI region. All
    # are in BAR 0, also region 0 (the ROM): the card shows the ROM at the
    # start of BAR 0 too. HP-UX finds the card by the region 0 address.
    rom[REGION_MAP_OFFSET:REGION_MAP_OFFSET + 4] = bytes([0x10] * 4)
    rom[STI_OFFSET:STI_OFFSET + len(img)] = img
    open(a.out, "wb").write(rom)


if __name__ == "__main__":
    main()
