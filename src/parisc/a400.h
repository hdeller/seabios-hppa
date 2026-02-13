/* HP A400-44 server (MPE: e3000/A400-100-11#A) */
/* AUTO-GENERATED HEADER FILE FOR SEABIOS FIRMWARE */
/* generated with Linux kernel */
/* search for PARISC_QEMU_MACHINE_HEADER in Linux */
#if 0
Found devices:
1. Crescendo DC- 440 [160] at 0xfffffffffffa0000 { type:0, hv:0x5d6, sv:0x4, rev:0x0 }
2. Memory [8] at 0xfffffffffed08000 { type:1, hv:0x9b, sv:0x9, rev:0x0 }
3. Astro BC Runway Port [0] at 0xfffffffffed00000 { type:12, hv:0x582, sv:0xb, rev:0x0 }
4. Elroy PCI Bridge [0:0] at 0xfffffffffed30000 { type:13, hv:0x782, sv:0xa, rev:0x0 }
5. Elroy PCI Bridge [0:2] at 0xfffffffffed34000 { type:13, hv:0x782, sv:0xa, rev:0x0 }
6. Elroy PCI Bridge [0:4] at 0xfffffffffed38000 { type:13, hv:0x782, sv:0xa, rev:0x0 }
7. Elroy PCI Bridge [0:6] at 0xfffffffffed3c000 { type:13, hv:0x782, sv:0xa, rev:0x0 }
#endif

#define PARISC_MODEL     "9000/800/A400-44"
#define PARISC_MODEL_MPE "e3000/A400-100-11#A"

#define PARISC_PDC_MODEL 0x5d60, 0x491, 0x0, 0x1, 2004000400, 0x10000000, 0x8, 0xb2, 0xb2, 0x1

#define PARISC_PDC_VERSION 0x0203

#define PARISC_PDC_CPUID 0x0268

#define PARISC_PDC_CAPABILITIES 0x0005

#define PARISC_PDC_ENTRY_ORG 0xfd20

#define PARISC_PDC_CACHE_INFO \
        0xc0000, 0x91802000, 0x20000, 0x0040, 0x0c00 \
        , 0x0001, 0x180000, 0xb1802000, 0x0000, 0x0040 \
        , 0x6000, 0x0001, 0x00a0, 0xd2300, 0x0000 \
        , 0x0000, 0x0001, 0x0000, 0x0000, 0x0001 \
        , 0x0001, 0x00a0, 0xd2000, 0x0000, 0x0000 \
        , 0x0001, 0x0000, 0x0000, 0x0001, 0x0001


#define HPA_fffffffffffa0000_DESCRIPTION "Crescendo DC- 440"
static struct pdc_system_map_mod_info mod_info_hpa_fffffffffffa0000 = {
        .mod_addr = CPU_HPA /* 0x8 */,
        .mod_pgs = 0x0,
        .add_addrs = 0x0,
};
static struct pdc_module_path mod_path_hpa_fffffffffffa0000 = {
        .path = { .flags = 0xff, .bc = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff }, .mod = 0xa0 }
};
static struct pdc_iodc iodc_data_hpa_fffffffffffa0000 = {
        .hversion_model = 0x005d,
        .hversion = 0x0060,
        .spa = 0x0000,
        .type = 0x0000,
        .sversion_rev = 0x0000,
        .sversion_model = 0x0002,
        .sversion_opt = 0x0048,
        .rev = 0x0000,
        .dep = 0x0000,
        .features = 0x0000,
        .checksum = 0x0000,
        .length = 0x0000,
};
#define HPA_fffffffffffa0000_num_addr 0
#define HPA_fffffffffffa0000_add_addr 0


#define HPA_fffffffffed08000_DESCRIPTION "Memory"
static struct pdc_system_map_mod_info mod_info_hpa_fffffffffed08000 = {
        .mod_addr = 0x8,
        .mod_pgs = 0x0,
        .add_addrs = 0x0,
};
static struct pdc_module_path mod_path_hpa_fffffffffed08000 = {
        .path = { .flags = 0xff, .bc = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff }, .mod = 0x8 }
};
static struct pdc_iodc iodc_data_hpa_fffffffffed08000 = {
        .hversion_model = 0x0009,
        .hversion = 0x00b0,
        .spa = 0x001f,
        .type = 0x0041,
        .sversion_rev = 0x0000,
        .sversion_model = 0x0004,
        .sversion_opt = 0x0080,
        .rev = 0x00ff,
        .dep = 0x00ff,
        .features = 0x00ff,
        .checksum = 0xffff,
        .length = 0xffff,
};
#define HPA_fffffffffed08000_num_addr 0
#define HPA_fffffffffed08000_add_addr 0


#define HPA_fffffffffed00000_DESCRIPTION "Astro BC Runway Port"
static struct pdc_system_map_mod_info mod_info_hpa_fffffffffed00000 = {
        .mod_addr = 0x8,
        .mod_pgs = 0x0,
        .add_addrs = 0x0,
};
static struct pdc_module_path mod_path_hpa_fffffffffed00000 = {
        .path = { .flags = 0xff, .bc = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff }, .mod = 0x0 }
};
static struct pdc_iodc iodc_data_hpa_fffffffffed00000 = {
        .hversion_model = 0x0058,
        .hversion = 0x0020,
        .spa = 0x0000,
        .type = 0x000c,
        .sversion_rev = 0x0000,
        .sversion_model = 0x0005,
        .sversion_opt = 0x0080,
        .rev = 0x00ff,
        .dep = 0x00ff,
        .features = 0x00ff,
        .checksum = 0xffff,
        .length = 0xffff,
};
#define HPA_fffffffffed00000_num_addr 0
#define HPA_fffffffffed00000_add_addr 0


#define HPA_fffffffffed30000_DESCRIPTION "Elroy PCI Bridge"
static struct pdc_system_map_mod_info mod_info_hpa_fffffffffed30000 = {
        .mod_addr = 0x8,
        .mod_pgs = 0x0,
        .add_addrs = 0x0,
};
static struct pdc_module_path mod_path_hpa_fffffffffed30000 = {
        .path = { .flags = 0xff, .bc = { 0xff, 0xff, 0xff, 0xff, 0xff, 0x0 }, .mod = 0x0 }
};
static struct pdc_iodc iodc_data_hpa_fffffffffed30000 = {
        .hversion_model = 0x0078,
        .hversion = 0x0020,
        .spa = 0x0000,
        .type = 0x000d,
        .sversion_rev = 0x0000,
        .sversion_model = 0x0005,
        .sversion_opt = 0x0000,
        .rev = 0x00ff,
        .dep = 0x00ff,
        .features = 0x00ff,
        .checksum = 0xffff,
        .length = 0xffff,
};
#define HPA_fffffffffed30000_num_addr 0
#define HPA_fffffffffed30000_add_addr 0


#define HPA_fffffffffed34000_DESCRIPTION "Elroy PCI Bridge"
static struct pdc_system_map_mod_info mod_info_hpa_fffffffffed34000 = {
        .mod_addr = 0x8,
        .mod_pgs = 0x0,
        .add_addrs = 0x0,
};
static struct pdc_module_path mod_path_hpa_fffffffffed34000 = {
        .path = { .flags = 0xff, .bc = { 0xff, 0xff, 0xff, 0xff, 0xff, 0x0 }, .mod = 0x1 /* 0x2 */ }
};
static struct pdc_iodc iodc_data_hpa_fffffffffed34000 = {
        .hversion_model = 0x0078,
        .hversion = 0x0020,
        .spa = 0x0000,
        .type = 0x000d,
        .sversion_rev = 0x0000,
        .sversion_model = 0x0005,
        .sversion_opt = 0x0000,
        .rev = 0x00ff,
        .dep = 0x00ff,
        .features = 0x00ff,
        .checksum = 0xffff,
        .length = 0xffff,
};
#define HPA_fffffffffed34000_num_addr 0
#define HPA_fffffffffed34000_add_addr 0


#define HPA_fffffffffed38000_DESCRIPTION "Elroy PCI Bridge"
static struct pdc_system_map_mod_info mod_info_hpa_fffffffffed38000 = {
        .mod_addr = 0x8,
        .mod_pgs = 0x0,
        .add_addrs = 0x0,
};
static struct pdc_module_path mod_path_hpa_fffffffffed38000 = {
        .path = { .flags = 0xff, .bc = { 0xff, 0xff, 0xff, 0xff, 0xff, 0x0 }, .mod = 0x4 }
};
static struct pdc_iodc iodc_data_hpa_fffffffffed38000 = {
        .hversion_model = 0x0078,
        .hversion = 0x0020,
        .spa = 0x0000,
        .type = 0x000d,
        .sversion_rev = 0x0000,
        .sversion_model = 0x0005,
        .sversion_opt = 0x0000,
        .rev = 0x00ff,
        .dep = 0x00ff,
        .features = 0x00ff,
        .checksum = 0xffff,
        .length = 0xffff,
};
#define HPA_fffffffffed38000_num_addr 0
#define HPA_fffffffffed38000_add_addr 0


#define HPA_fffffffffed3c000_DESCRIPTION "Elroy PCI Bridge"
static struct pdc_system_map_mod_info mod_info_hpa_fffffffffed3c000 = {
        .mod_addr = 0x8,
        .mod_pgs = 0x0,
        .add_addrs = 0x0,
};
static struct pdc_module_path mod_path_hpa_fffffffffed3c000 = {
        .path = { .flags = 0xff, .bc = { 0xff, 0xff, 0xff, 0xff, 0xff, 0x0 }, .mod = 0x6 }
};
static struct pdc_iodc iodc_data_hpa_fffffffffed3c000 = {
        .hversion_model = 0x0078,
        .hversion = 0x0020,
        .spa = 0x0000,
        .type = 0x000d,
        .sversion_rev = 0x0000,
        .sversion_model = 0x0005,
        .sversion_opt = 0x0000,
        .rev = 0x00ff,
        .dep = 0x00ff,
        .features = 0x00ff,
        .checksum = 0xffff,
        .length = 0xffff,
};
#define HPA_fffffffffed3c000_num_addr 0
#define HPA_fffffffffed3c000_add_addr 0



#define PARISC_DEVICE_LIST \
        {       .hpa = ASTRO_MEMORY_HPA_A400 /* 0xfffffffffed08000 */ ,\
                .iodc = &iodc_data_hpa_fffffffffed08000,\
                .mod_info = &mod_info_hpa_fffffffffed08000,\
                .mod_path = &mod_path_hpa_fffffffffed08000,\
                .num_addr = HPA_fffffffffed08000_num_addr,\
                .add_addr = { HPA_fffffffffed08000_add_addr } },\
        {       .hpa = ASTRO_HPA  /* 0xfffffffffed00000 */ ,\
                .iodc = &iodc_data_hpa_fffffffffed00000,\
                .mod_info = &mod_info_hpa_fffffffffed00000,\
                .mod_path = &mod_path_hpa_fffffffffed00000,\
                .num_addr = HPA_fffffffffed00000_num_addr,\
                .add_addr = { HPA_fffffffffed00000_add_addr } },\
        {       .hpa = ELROY0_HPA  /* 0xfffffffffed30000 */ ,\
                .iodc = &iodc_data_hpa_fffffffffed30000,\
                .mod_info = &mod_info_hpa_fffffffffed30000,\
                .mod_path = &mod_path_hpa_fffffffffed30000,\
                .num_addr = HPA_fffffffffed30000_num_addr,\
                .add_addr = { HPA_fffffffffed30000_add_addr } },\
        {       .hpa = ELROY2_HPA  /* XXX 0xfffffffffed34000 */ ,\
                .iodc = &iodc_data_hpa_fffffffffed34000,\
                .mod_info = &mod_info_hpa_fffffffffed34000,\
                .mod_path = &mod_path_hpa_fffffffffed34000,\
                .num_addr = HPA_fffffffffed34000_num_addr,\
                .add_addr = { HPA_fffffffffed34000_add_addr } },\
        {       .hpa = ELROY8_HPA  /* 0xfffffffffed38000 */,\
                .iodc = &iodc_data_hpa_fffffffffed38000,\
                .mod_info = &mod_info_hpa_fffffffffed38000,\
                .mod_path = &mod_path_hpa_fffffffffed38000,\
                .num_addr = HPA_fffffffffed38000_num_addr,\
                .add_addr = { HPA_fffffffffed38000_add_addr } },\
        {       .hpa = ELROYc_HPA  /* 0xfffffffffed3c000 */,\
                .iodc = &iodc_data_hpa_fffffffffed3c000,\
                .mod_info = &mod_info_hpa_fffffffffed3c000,\
                .mod_path = &mod_path_hpa_fffffffffed3c000,\
                .num_addr = HPA_fffffffffed3c000_num_addr,\
                .add_addr = { HPA_fffffffffed3c000_add_addr } },\
        {       .hpa = CPU_HPA /* 0xfffffffffffa0000 */,\
                .iodc = &iodc_data_hpa_fffffffffffa0000,\
                .mod_info = &mod_info_hpa_fffffffffffa0000,\
                .mod_path = &mod_path_hpa_fffffffffffa0000,\
                .num_addr = HPA_fffffffffffa0000_num_addr,\
                .add_addr = { HPA_fffffffffffa0000_add_addr } },\
        { 0, }
