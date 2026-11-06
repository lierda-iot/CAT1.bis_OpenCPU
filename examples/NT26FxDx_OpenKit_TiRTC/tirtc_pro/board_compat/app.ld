#include "mem_map.h"

/* Enforce the SDK 1.7.1 F6D_D map and preserve the existing internal FS. */
ASSERT(PKGFLXAPP_APP0_LNA == 0xA92000, "TiRTC pro: unexpected F6D_D application address")
ASSERT(PKGFLXAPP_APP0_SIZE == 0x12A000, "TiRTC pro: unexpected application capacity")
ASSERT(PKGFLXFS_IMG_ADDR == 0x3BC000 && PKGFLXFS_IMG_SIZE == 0x28000,
       "TiRTC pro: internal filesystem layout changed; review migration before building")

MEMORY
{
  FLASH (rx) : ORIGIN = PKGFLXAPP_APP0_LNA, LENGTH = PKGFLXAPP_APP0_SIZE
  RAM (rwx)  : ORIGIN = PKGFLXAPP_RAM_ADDR, LENGTH = PKGFLXAPP_RAM_SIZE
}

SECTIONS
{
  .app_header : ALIGN(4)
  {
    KEEP(*(.app_header))
    KEEP(*(.iomap))
    KEEP(*(.cfgmap))
    KEEP(*(.app_info))
  } > FLASH

  .app_nvram : ALIGN(0x1000)
  {
    KEEP(*(.nvramcfg))
  } > FLASH

	.text :
	{
		*(.text.startup.main)
		*(.text*)
		*(.rodata*)
	} > FLASH

  .data : ALIGN(4)
  {
    . = ALIGN(4);
    Load$$LOAD_DRAM_SHARED$$Base = LOADADDR(.data);
    Image$$LOAD_DRAM_SHARED$$Base = .;
    *(.data*)
    *(.sect_ap_psram_p0_data.*)
    *(.sect_ap_ps_psram_p0_data.*)
    *(.sect_platFPSRAMRamcode_text_cust.*)
    *(.sect_platFPSRAMRWData_data_cust.*)
    *(.sect_platFMRamcode_text.*)
    . = ALIGN(4);
  } >RAM AT>FLASH

  Image$$LOAD_DRAM_SHARED$$Length = SIZEOF(.data);

  .load_dram_shared_zi (NOLOAD):
  {
    . = ALIGN(4);
    Image$$LOAD_DRAM_SHARED$$ZI$$Base = .;
		__bss_start__ = .;
		*(.bss*)
    *(.sect_platFMZIData_bss.*)
    *(.sect_*_bss* .bss*)
    *(.sect_ap_psram_p0_bss.*)
    *(.sect_ap_ps_psram_p0_bss.*)
    *(COMMON)	
    *(.sect_decompress_bss.*)
    . = ALIGN(4);
    *(.stack*)               /* stack should be 4 byte align */ 
    __bss_end__ = .;
    Image$$LOAD_DRAM_SHARED$$ZI$$Limit = .;
    *(.USB_NOINIT_DATA_BUF)
  } >RAM

  /* Read-only layout statistics. No output section placement is changed.
   * The NOLOAD section's load address follows all Flash load images, including
   * SDK orphan data sections. The release audit checks it against every LOAD
   * section and the actual APP binary before this value is shipped. */
  __tirtc_app_flash_origin = ABSOLUTE(ORIGIN(FLASH));
  __tirtc_app_flash_capacity = ABSOLUTE(LENGTH(FLASH));
  __tirtc_app_flash_used = ABSOLUTE(LOADADDR(.load_dram_shared_zi) - ORIGIN(FLASH));
  __tirtc_app_ram_origin = ABSOLUTE(ORIGIN(RAM));
  __tirtc_app_ram_capacity = ABSOLUTE(LENGTH(RAM));
  __tirtc_app_static_ram_used = ABSOLUTE(ADDR(.load_dram_shared_zi) + SIZEOF(.load_dram_shared_zi) - ORIGIN(RAM));
}
