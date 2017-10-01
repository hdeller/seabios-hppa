/* defines for parisc firmware */

//#define PARISC_FIRMWARE_START 0xff000000
// #define PARISC_FIRMWARE_START     0x60000
#define PARISC_FIRMWARE_START   0x80000000

/* serial port of DINO */
#define DINO_UART_BASE 0xfff83800

#if 0
[    2.160168] 1. Phantom PseudoBC GSC+ Port at 0xffc00000 [8] { 7, 0x0, 0x504, 0x00000 }
[    2.260162] 2. Dino PCI Bridge at 0xfff80000 [8/0] { 13, 0x3, 0x680, 0x0000a }
[    2.350158] 3. Merlin+ 132 Dino RS-232 at 0xfff83000 [8/0/63] { 10, 0x0, 0x022, 0x0008c }
[    2.450160] 4. Merlin 160 Core FW-SCSI at 0xfff8c000 [8/12] { 4, 0x0, 0x03d, 0x00089 }
[    2.550161] 5. Merlin 160 Core BA at 0xffd00000 [8/16] { 11, 0x0, 0x03d, 0x00081 }, additional addresses: 0xffd0c000 0xffc00000
[    2.700165] 6. Merlin 160 Core RS-232 at 0xffd05000 [8/16/4] { 10, 0x0, 0x03d, 0x0008c }
[    2.800160] 7. Merlin 160 Core SCSI at 0xffd06000 [8/16/5] { 10, 0x0, 0x03d, 0x00082 }
[    2.900162] 8. Merlin 160 Core LAN (802.3) at 0xffd07000 [8/16/6] { 10, 0x0, 0x03d, 0x0008a }
[    3.010187] 9. Merlin 160 Core Centronics at 0xffd02000 [8/16/0] { 10, 0x0, 0x03d, 0x00074 }, additional addresses: 0xffd01000 0xffd03000
[    3.170204] 10. Merlin 160 Core Audio at 0xffd04000 [8/16/1] { 10, 0x4, 0x03d, 0x0007b }
[    3.270157] 11. Merlin 160 Core PS/2 Port at 0xffd08000 [8/16/7] { 10, 0x0, 0x03d, 0x00084 }
[    3.370151] 12. Merlin 160 Core PS/2 Port at 0xffd08100 [8/16/8] { 10, 0x0, 0x03d, 0x00084 }
[    3.480148] 13. Coral SGC Graphics at 0xfa000000 [8/4] { 10, 0x0, 0x004, 0x00077 }
[    3.570145] 14. Coral SGC Graphics at 0xf4000000 [8/8] { 10, 0x0, 0x004, 0x00077 }
[    3.670146] 15. Gecko GSC Core Graphics at 0xf8000000 [8/24] { 10, 0x0, 0x016, 0x00085 }, additional addresses: 0xf0011000
[    3.810147] 16. Merlin L2 160 (9000/778/B160L) at 0xfffbe000 [62] { 0, 0x0, 0x502, 0x00004 }
[    3.920139] 17. Memory at 0xfffbf000 [63] { 1, 0x0, 0x067, 0x00009 }
[    4.000142] 18. Merlin+ 132 Dino PS/2 Port at 0xfff81000 [1] { 10, 0x0, 0x022, 0x00096 }
[    4.231309] CPU(s): 1 out of 1 PA7300LC (PCX-L2) at 160.000000 MHz online

http://nairobi-embedded.org/qemu_serial_port_system_console.html
qemu-system-i386 -enable-kvm -kernel bzImage -drive file=test.img -append "root=/dev/sda2 console=tty0 console=ttyS0 rw" -serial stdio
qemu-system-i386 -enable-kvm -kernel bzImage -drive file=test.img -append "root=/dev/sda2 console=tty0 console=ttyS0 rw" -nographic

./hppa-softmmu/qemu-system-hppa  -kernel bios.bin -m 3500 -nographic  -serial mon:stdio
#endif
