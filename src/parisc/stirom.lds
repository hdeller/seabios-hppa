/*
 * Linker script for the STI ROM of the PCI graphics card (see
 * scripts/buildstirom.py). The routines stay in header order, because
 * HP-UX copies each one from its offset to the next offset.
 */
SECTIONS
{
    . = 0;
    .sti : {
        _sti_rom_start = .;
        *(.sti.hdr)
        *(.sti.text.init_graph)
        *(.sti.text.state_mgmt)
        *(.sti.text.font_unpmv)
        *(.sti.text.block_move)
        *(.sti.text.self_test)
        *(.sti.text.excep_hdlr)
        *(.sti.text.inq_conf)
        *(.sti.text.set_cm_entry)
        *(.sti.text.dma_ctrl)
        *(.sti.text)
        _sti_code_end = .;
        *(.sti.data)
        *(.sti.font)
        *(.sti.text.end)
        _sti_rom_end = .;
    }
    /DISCARD/ : { *(*) }
}
