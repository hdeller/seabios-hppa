/*
 * Startup glue code for parisc firmware
 *
 *   (C) 2017 Helge Deller <deller@gmx.de>
 */

#include "parisc/parisc.h"

	/* load 32-bit 'value' into 'reg' compensating for the ldil
	 * sign-extension when running in wide mode.
	 * WARNING!! neither 'value' nor 'reg' can be expressions
	 * containing '.'!!!! */
	.macro	load32 value, reg
	ldil	L%\value, \reg
	ldo	R%\value(\reg), \reg
	.endm

#define ENTRY(name) \
	.export name !\
	.align 4 !\
name:

#define END(name) \
	.size name, .-name

#define ENDPROC(name) \
	.type name, @function !\
	END(name)

#define BOOTADDR(x)	(x)

	.macro loadgp
	ldil		L%$global$, %r27
	ldo		R%$global$(%r27), %r27
	.endm

#ifdef CONFIG_64BIT
#define LDREG	ldd
#define STREG	std
#define LDREGX  ldd,s
#define LDREGM	ldd,mb
#define STREGM	std,ma
#define SHRREG	shrd
#define SHLREG	shld
#define ANDCM   andcm,*
#define	COND(x)	* ## x
#define RP_OFFSET	16
#define FRAME_SIZE	128
#define CALLEE_REG_FRAME_SIZE	144
#define ASM_ULONG_INSN	.dword
#else	/* CONFIG_64BIT */
#define LDREG	ldw
#define STREG	stw
#define LDREGX  ldwx,s
#define LDREGM	ldwm
#define STREGM	stwm
#define SHRREG	shr
#define SHLREG	shlw
#define ANDCM   andcm
#define COND(x)	x
#define RP_OFFSET	20
#define FRAME_SIZE	64
#define CALLEE_REG_FRAME_SIZE	128
#define ASM_ULONG_INSN	.word
#endif

	.import	$global$
	.section ".head.text","ax"
	 .level 1.1

ENTRY(startup)

	/* Make sure space registers are set to zero */
	mtsp    %r0,%sr0
	mtsp    %r0,%sr1
	mtsp    %r0,%sr2
	mtsp    %r0,%sr3
	mtsp    %r0,%sr4
	mtsp    %r0,%sr5
	mtsp    %r0,%sr6
	mtsp    %r0,%sr7

#define PSW_W_SM	0x200
#define PSW_W_BIT       36

	;! nuke the W bit, saving original value
	.level 2.0
	rsm	PSW_W_SM, %r1

	.level 1.1
	extrw,u	%r1, PSW_W_BIT-32, 1, %r1
	copy	%r1, %arg0

	/* Clear BSS */

	.import _bss,data
	.import _ebss,data

	load32	BOOTADDR(_bss),%r3
	load32	BOOTADDR(_ebss),%r4
	ldo	FRAME_SIZE(%r4),%sp	/* stack at end of bss */
$bss_loop:
	cmpb,<<,n %r3,%r4,$bss_loop
	stw,ma	%r0,4(%r3)

	/* Initialize the global data pointer */
	loadgp

	/* arg0..arg4 were set by palo. */
	copy	%arg1, %r6		/* command line */
	copy	%arg2, %r7		/* rd-start */
	copy	%arg3, %r8		/* rd-end */
	load32	BOOTADDR(start_parisc_firmware),%r3

#ifdef CONFIG_64BIT
	.level LEVEL
	ssm	PSW_W_SM, %r0		/* set W-bit */
	depdi	0, 31, 32, %r3
#endif
	load32	BOOTADDR(startup_continue), %r2
	bv,n	0(%r3)

startup_continue:
#ifdef CONFIG_64BIT
	.level LEVEL
	rsm	PSW_W_SM, %r0		/* clear W-bit */
#endif

	load32	0 /*XXXX*/, %arg0 /* free mem */
	copy	%r6, %arg1		/* command line */
	copy	%r7, %arg2		/* rd-start */
	copy	%r8, %arg3		/* rd-end */

	bv,n	0(%ret0)
END(startup)


ENTRY(pdc_entry)
	stw %rp,-20(%sp)
	stw %r27,-32(%sp)
	stw %arg0,-36(%sp)
	stw %arg1,-40(%sp)
	stw %arg2,-44(%sp)
	stw %arg3,-48(%sp)
	ldo -FRAME_SIZE(%sp),%arg0

	loadgp
	b,l parisc_pdc_entry, %rp
	ldo FRAME_SIZE(%sp),%sp

	ldo -FRAME_SIZE(%sp),%sp
	ldw -20(%sp),%rp
	bv %r0(%rp)
	ldw -32(%sp),%r27
END(pdc_entry)

ENTRY(iodc_entry)
	.import parisc_iodc_entry
	stw %rp,-20(%sp)
	stw %r27,-32(%sp)
	stw %arg0,-36(%sp)
	stw %arg1,-40(%sp)
	stw %arg2,-44(%sp)
	stw %arg3,-48(%sp)
	ldo -FRAME_SIZE(%sp),%arg0

	loadgp
	b,l parisc_iodc_entry, %rp
	ldo FRAME_SIZE(%sp),%sp

	ldo -FRAME_SIZE(%sp),%sp
	ldw -20(%sp),%rp
	bv %r0(%rp)
	ldw -32(%sp),%r27
END(iodc_entry)
