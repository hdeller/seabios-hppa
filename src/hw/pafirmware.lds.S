#include "src/hw/parisc.h"

OUTPUT_FORMAT("elf32-hppa-linux")
OUTPUT_ARCH(hppa)
ENTRY(startup)
SECTIONS
{
	. = PARISC_FIRMWARE_START;

	.head.text : {
		_head = . ;
		*(.head.text)
		_ehead = . ;
	}

	/* keep __gp below 0x1000000 */

	/* align on next page boundary */
	. = ALIGN(4096);
	.text :	{
		_text = .;	/* Text */
		*(.text)
		*(.text.*)
		_etext = . ;
	}
	. = ALIGN(8);
	.data :	{
		_data = . ;
		*(.data)
		*(.data.*)
		_edata = . ;
	}
	. = ALIGN(8);
	.rodata : {
		_rodata = . ;
		*(.rodata)	 /* read-only data */
		*(.rodata.*)
		_erodata = . ;
	}
	. = ALIGN(8);
	.rodata.compressed : {
		*(.rodata.compressed)
	}
	. = ALIGN(8);
	.bss : {
		_bss = . ;
		*(.bss)
		*(.bss.*)
		*(COMMON)
		. = ALIGN(4096);
		_ebss = .;
	}

	/* STABS_DEBUG */
	.note 0 : { *(.note) }

	/* Sections to be discarded */
	/DISCARD/ : {
	}
}
