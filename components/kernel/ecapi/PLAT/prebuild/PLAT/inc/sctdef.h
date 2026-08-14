
#ifndef SCT_DEF_H
#define SCT_DEF_H

#include "mem_map.h"

/*
each different chip has its own sctdef.h

for 618 no need to define SECTION_DEF_IMPL related macro, keep as before for mass production branch

for 618s/718 always define SECTION_DEF_IMPL related macro and always define sect_xxx_xxxx_filename_line


*/
#if defined(__GNUC__)
#define SECTION_DEF_IMPL_STRINGNIFY(s) #s
#define SECTION_DEF_STRINGNIFY(s) SECTION_DEF_IMPL_STRINGNIFY(s)

#define SECTION_DEF_IMPL_CONCAT3(_1, _2, _3) _1._2._3
#define SECTION_DEF_CONCAT3(_1, _2, _3) SECTION_DEF_IMPL_CONCAT3(_1, _2, _3)
#define SECTION_DEF_UNIQUE_STRING(a) SECTION_DEF_STRINGNIFY(SECTION_DEF_CONCAT3(a,__CURRENT_FILE_NAME__, __LINE__))
#define SECTION_DEF_IMPL(s) __attribute__((__section__(SECTION_DEF_UNIQUE_STRING(s))))
#define SECTION_DEF_RAMCODE __attribute__((noinline))
#else
#define SECTION_DEF_IMPL(s) __attribute__((__section__("s")))
#endif





/* OS section, OS is common part when merge 618/618s/718 as one branch

for 618 define as empty to keep mass production branch as before

for 618s(this file) always define as sect_xxx_xxxx__filename_line

for 718 same as 618s

if any other common part as OS add below
*/

#if defined FEATURE_FREERTOS_ENABLE


#define FREERTOS_CMSISOS2_TEXT_SECTION     SECTION_DEF_IMPL(.sect_freertos_cmsisos2_text) SECTION_DEF_RAMCODE
#define FREERTOS_CMSISOS2_RODATA_SECTION   SECTION_DEF_IMPL(.sect_freertos_cmsisos2_rodata)
#define FREERTOS_CMSISOS2_DATA_SECTION     SECTION_DEF_IMPL(.sect_freertos_cmsisos2_data)
#define FREERTOS_CMSISOS2_BSS_SECTION      SECTION_DEF_IMPL(.sect_freertos_cmsisos2_bss)



#define FREERTOS_TASKS_TEXT_SECTION     SECTION_DEF_IMPL(.sect_freertos_tasks_text) SECTION_DEF_RAMCODE
#define FREERTOS_TASKS_RODATA_SECTION   SECTION_DEF_IMPL(.sect_freertos_tasks_rodata)
#define FREERTOS_TASKS_DATA_SECTION     SECTION_DEF_IMPL(.sect_freertos_tasks_data)
#define FREERTOS_TASKS_BSS_SECTION      SECTION_DEF_IMPL(.sectfreertos_tasks_bss)



#define FREERTOS_EVENTGROUPS_TEXT_SECTION     SECTION_DEF_IMPL(.sect_freertos_eventgroups_text) SECTION_DEF_RAMCODE
#define FREERTOS_EVENTGROUPS_RODATA_SECTION   SECTION_DEF_IMPL(.sect_freertos_eventgroups_rodata)
#define FREERTOS_EVENTGROUPS_DATA_SECTION     SECTION_DEF_IMPL(.sect_freertos_eventgroups_data)
#define FREERTOS_EVENTGROUPS_BSS_SECTION      SECTION_DEF_IMPL(.sect_freertos_eventgroups_bss)


#define FREERTOS_LIST_TEXT_SECTION     SECTION_DEF_IMPL(.sect_freertos_list_text) SECTION_DEF_RAMCODE
#define FREERTOS_LIST_RODATA_SECTION   SECTION_DEF_IMPL(.sect_freertos_list_rodata)
#define FREERTOS_LIST_DATA_SECTION     SECTION_DEF_IMPL(.sect_freertos_list_data)
#define FREERTOS_LIST_BSS_SECTION      SECTION_DEF_IMPL(.sect_freertos_list_bss)


#define FREERTOS_PORT_TEXT_SECTION     SECTION_DEF_IMPL(.sect_freertos_port_text) SECTION_DEF_RAMCODE
#define FREERTOS_PORT_RODATA_SECTION   SECTION_DEF_IMPL(.sect_freertos_port_rodata)
#define FREERTOS_PORT_DATA_SECTION     SECTION_DEF_IMPL(.sect_freertos_port_data)
#define FREERTOS_PORT_BSS_SECTION      SECTION_DEF_IMPL(.sect_freertos_port_bss)


#define FREERTOS_QUEUE_TEXT_SECTION     SECTION_DEF_IMPL(.sect_freertos_queue_text) SECTION_DEF_RAMCODE
#define FREERTOS_QUEUE_RODATA_SECTION   SECTION_DEF_IMPL(.sect_freertos_queue_rodata)
#define FREERTOS_QUEUE_DATA_SECTION     SECTION_DEF_IMPL(.sect_freertos_queue_data)
#define FREERTOS_QUEUE_BSS_SECTION      SECTION_DEF_IMPL(.sect_freertos_queue_bss)


#define FREERTOS_TIMERS_TEXT_SECTION     SECTION_DEF_IMPL(.sect_freertos_timers_text) SECTION_DEF_RAMCODE
#define FREERTOS_TIMERS_RODATA_SECTION   SECTION_DEF_IMPL(.sect_freertos_timers_rodata)
#define FREERTOS_TIMERS_DATA_SECTION     SECTION_DEF_IMPL(.sect_freertos_timers_data)
#define FREERTOS_TIMERS_BSS_SECTION      SECTION_DEF_IMPL(.sect_freertos_timers_bss)


#define FREERTOS_TLSF_TEXT_SECTION     SECTION_DEF_IMPL(.sect_freertos_tlsf_text) SECTION_DEF_RAMCODE
#define FREERTOS_TLSF_RODATA_SECTION   SECTION_DEF_IMPL(.sect_freertos_tlsf_rodata)
#define FREERTOS_TLSF_DATA_SECTION     SECTION_DEF_IMPL(.sect_freertos_tlsf_data)
#define FREERTOS_TLSF_BSS_SECTION      SECTION_DEF_IMPL(.sect_freertos_tlsf_bss)


#define FREERTOS_HEAP6_TEXT_SECTION     SECTION_DEF_IMPL(.sect_freertos_heap6_text) SECTION_DEF_RAMCODE
#define FREERTOS_HEAP6_RODATA_SECTION   SECTION_DEF_IMPL(.sect_freertos_heap6_rodata)
#define FREERTOS_HEAP6_DATA_SECTION     SECTION_DEF_IMPL(.sect_freertos_heap6_data)
#define FREERTOS_HEAP6_BSS_SECTION      SECTION_DEF_IMPL(.sect_freertos_heap6_bss)

#elif defined FEATURE_LITEOS_ENABLE
#define LITE_OS_SEC_TEXT_SECTION        SECTION_DEF_IMPL(.sect_liteos_sec_text) SECTION_DEF_RAMCODE

#define LITE_OS_CMSISOS2_TEXT_SECTION      SECTION_DEF_IMPL(.sect_liteos_cmsisos2_text) SECTION_DEF_RAMCODE
#define LITE_OS_CMSISOS2_RODATA_SECTION    SECTION_DEF_IMPL(.sect_liteos_cmsisos2_rodata)
#define LITE_OS_CMSISOS2_DATA_SECTION      SECTION_DEF_IMPL(.sect_liteos_cmsisos2_data)
#define LITE_OS_CMSISOS2_BSS_SECTION       SECTION_DEF_IMPL(.sect_liteos_cmsisos2_bss)

#endif

//ccio part
#define UART_DEVICE_TEXT_SECTION     SECTION_DEF_IMPL(.sect_uart_device_text)
#define UART_DEVICE_RODATA_SECTION   SECTION_DEF_IMPL(.sect_uart_device_rodata)
#define UART_DEVICE_DATA_SECTION     SECTION_DEF_IMPL(.sect_uart_device_data)
#define UART_DEVICE_BSS_SECTION      SECTION_DEF_IMPL(.sect_uart_device_bss)

#define USB_DEVICE_TEXT_SECTION     SECTION_DEF_IMPL(.sect_usb_device_text)
#define USB_DEVICE_RODATA_SECTION   SECTION_DEF_IMPL(.sect_usb_device_rodata)
#define USB_DEVICE_DATA_SECTION     SECTION_DEF_IMPL(.sect_usb_device_data)
#define USB_DEVICE_BSS_SECTION      SECTION_DEF_IMPL(.sect_usb_device_bss)

#define SPI_DEVICE_TEXT_SECTION     SECTION_DEF_IMPL(.sect_spi_device_text)
#define SPI_DEVICE_RODATA_SECTION   SECTION_DEF_IMPL(.sect_spi_device_rodata)
#define SPI_DEVICE_DATA_SECTION     SECTION_DEF_IMPL(.sect_spi_device_data)
#define SPI_DEVICE_BSS_SECTION      SECTION_DEF_IMPL(.sect_spi_device_bss)

#define SCOM_DEVICE_TEXT_SECTION    SECTION_DEF_IMPL(.sect_scom_device_text)
#define SCOM_DEVICE_RODATA_SECTION  SECTION_DEF_IMPL(.sect_scom_device_rodata)
#define SCOM_DEVICE_DATA_SECTION    SECTION_DEF_IMPL(.sect_scom_device_data)
#define SCOM_DEVICE_BSS_SECTION     SECTION_DEF_IMPL(.sect_scom_device_bss)

//mm_debug part

#ifdef CORE_IS_CP //CP need put this into ramcode
#define MMDEBUG_TEXT_SECTION     SECTION_DEF_IMPL(.sect_mm_debug_text) SECTION_DEF_RAMCODE
#define MMDEBUG_RODATA_SECTION   SECTION_DEF_IMPL(.sect_mm_debug_rodata)
#define MMDEBUG_DATA_SECTION     SECTION_DEF_IMPL(.sect_mm_debug_data)
#define MMDEBUG_BSS_SECTION      SECTION_DEF_IMPL(.sect_mm_debug_bss)
#else
#define MMDEBUG_TEXT_SECTION
#define MMDEBUG_RODATA_SECTION
#define MMDEBUG_DATA_SECTION
#define MMDEBUG_BSS_SECTION
#endif



#define COMMONLY_USED_RO                SECTION_DEF_IMPL(.sect_commonly_used_ro)
#define COMMONLY_USED_TEXT              SECTION_DEF_IMPL(.sect_commonly_used_text)


#ifdef FEATURE_BOOTLOADER_PROJECT_ENABLE
//BL section define start
#define PLAT_BL_UNCOMP_FLASH_TEXT SECTION_DEF_IMPL(.sect_bl_uncompress_flash_text) SECTION_DEF_RAMCODE
#define PLAT_BL_UNCOMP_FLASH_RODATA SECTION_DEF_IMPL(.sect_bl_uncompress_flash_rodata)

#define PLAT_BL_AIRAM_PRE1_TEXT SECTION_DEF_IMPL(.sect_bl_airam_pre1_text) SECTION_DEF_RAMCODE
#define PLAT_BL_AIRAM_PRE1_RODATA SECTION_DEF_IMPL(.sect_bl_airam_pre1_rodata)

#define PLAT_BL_AIRAM_PRE2_TEXT SECTION_DEF_IMPL(.sect_bl_airam_pre2_text) SECTION_DEF_RAMCODE
#define PLAT_BL_AIRAM_PRE2_RODATA SECTION_DEF_IMPL(.sect_bl_airam_pre2_rodata)

#define PLAT_BL_CIRAM_FLASH_TEXT SECTION_DEF_IMPL(.sect_bl_ciram_flash_text) SECTION_DEF_RAMCODE
#define PLAT_BL_CIRAM_FLASH_RODATA SECTION_DEF_IMPL(.sect_bl_ciram_flash_rodata)

#define PLAT_BL_MIRAM_USB_DATA SECTION_DEF_IMPL(.sect_bl_usb_data)
#define PLAT_BL_MIRAM_USB_BSS SECTION_DEF_IMPL(.sect_bl_usb_bss)

#define PLAT_BL_CUST_XIP_TEXT SECTION_DEF_IMPL(.sect_bl_cust_xip_text) SECTION_DEF_RAMCODE
#define PLAT_BL_CUST_XIP_RODATA SECTION_DEF_IMPL(.sect_bl_cust_xip_rodata)

#define PLAT_BL_CUST_XIP_DATA SECTION_DEF_IMPL(.sect_bl_cust_xip_data)
#define PLAT_BL_CUST_XIP_BSS SECTION_DEF_IMPL(.sect_bl_cust_xip_bss)


// for plat use only, only used in bootloader for HW SHA256, since SCT could only access MSMB
// But this area should init by user
#define PLAT_BL_SCT_ZI SECTION_DEF_IMPL(.sect_platBlSctZIData_bss)
#define FOTA_PLAT_SCT_ZI PLAT_BL_SCT_ZI

#else
#define PLAT_BL_UNCOMP_FLASH_TEXT
#define PLAT_BL_UNCOMP_FLASH_RODATA

#define PLAT_BL_AIRAM_PRE1_TEXT
#define PLAT_BL_AIRAM_PRE1_RODATA

#define PLAT_BL_AIRAM_PRE2_TEXT
#define PLAT_BL_AIRAM_PRE2_RODATA


#define PLAT_BL_MIRAM_USB_DATA
#define PLAT_BL_MIRAM_USB_BSS

#define PLAT_BL_CIRAM_FLASH_TEXT
#define PLAT_BL_CIRAM_FLASH_RODATA

#define PLAT_BL_SCT_ZI
#define FOTA_PLAT_SCT_ZI

#define PLAT_BL_CUST_XIP_TEXT
#define PLAT_BL_CUST_XIP_RODATA

#define PLAT_BL_CUST_XIP_DATA
#define PLAT_BL_CUST_XIP_BSS

#endif

#if defined(FEATURE_BOOTLOADER_PROJECT_ENABLE) && (defined(CHIP_EC718)||defined(CHIP_EC716))
#define PLAT_UNCOMP_FLASH_TEXT   PLAT_BL_UNCOMP_FLASH_TEXT
#define PLAT_UNCOMP_FLASH_RODATA PLAT_BL_UNCOMP_FLASH_RODATA
#else
#define PLAT_UNCOMP_FLASH_TEXT
#define PLAT_UNCOMP_FLASH_RODATA
#endif

#if defined CHIP_EC618 || defined CHIP_EC618_Z0
#define EXCEP_DUMP_DATA
#define EXCEP_DUMP_BSS
#elif defined CHIP_EC718 || defined CHIP_EC716
#define EXCEP_DUMP_DATA   SECTION_DEF_IMPL(.sect_excep_dump_data)
#define EXCEP_DUMP_BSS  SECTION_DEF_IMPL(.sect_excep_dump_bss)
#endif

#define RAM_BOOT_NOINIT SECTION_DEF_IMPL(.sect_boot_noInit)

#ifdef TYPE_EC718M
// ap section define for boot
//#define RAM_BOOT_DATA SECTION_DEF_IMPL(.sect_boot_data)
//#define RAM_BOOT_BSS  SECTION_DEF_IMPL(.sect_boot_bss)
//#define RAM_BOOT_NOINIT SECTION_DEF_IMPL(.sect_boot_noInit)

#define PLAT_MSMB_DATA_NOINIT SECTION_DEF_IMPL(.sect_platMsmbNoInit)

// for plat use only, placed in psram part0, init in fullimage
#define PLAT_FPSRAM_P0_RAMCODE SECTION_DEF_IMPL(.sect_platRamcode_psram_p0_text) SECTION_DEF_RAMCODE
// for ps use only, placed in psram part0, init in fullimage
#define PS_FPSRAM_P0_RAMCODE   SECTION_DEF_IMPL(.sect_psRamcode_psram_p0_text) SECTION_DEF_RAMCODE
// placed in psram part0, init in fullimage
#define AP_FPSRAM_P0_DATA    SECTION_DEF_IMPL(.sect_ap_psram_p0_data)
// placed in psram part0, init in fullimage
#define AP_FPSRAM_P0_BSS     SECTION_DEF_IMPL(.sect_ap_psram_p0_bss)
// placed in psram part0, init in fullimage
#define AP_PS_FPSRAM_P0_DATA SECTION_DEF_IMPL(.sect_ap_ps_psram_p0_data)
// placed in psram part0, init in fullimage
#define AP_PS_FPSRAM_P0_BSS  SECTION_DEF_IMPL(.sect_ap_ps_psram_p0_bss)

// P1 init in fullimage
#define AP_PS_FPSRAM_P1_DATA SECTION_DEF_IMPL(.sect_ap_ps_psram_p1_data)
#define AP_PS_FPSRAM_P1_BSS  SECTION_DEF_IMPL(.sect_ap_ps_psram_p1_bss)
#define AP_PLAT_FPSRAM_P1_DATA SECTION_DEF_IMPL(.sect_ap_plat_psram_p1_data)
#define AP_PLAT_FPSRAM_P1_BSS  SECTION_DEF_IMPL(.sect_ap_plat_psram_p1_bss)

// for customer use only, placed in psram part2
#define CUST_FPSRAM_P2_RAMCODE SECTION_DEF_IMPL(.cust_sect_ap_psram_p2_text) SECTION_DEF_RAMCODE
// for customer use only, placed in psram part2
#define CUST_FPSRAM_P2_DATA    SECTION_DEF_IMPL(.cust_sect_ap_psram_p2_data)
// for customer use only, placed in psram part2
#define CUST_FPSRAM_P2_BSS     SECTION_DEF_IMPL(.cust_sect_ap_psram_p2_bss)

// CCIO BUF
#define PLAT_CCIO_BUF SECTION_DEF_IMPL(.sect_ccio_buf)
#else
// ap section define for boot
//#define RAM_BOOT_DATA
//#define RAM_BOOT_BSS
//#define RAM_BOOT_NOINIT

#define PLAT_MSMB_DATA_NOINIT

// for plat use only, placed in psram part0
#define PLAT_FPSRAM_P0_RAMCODE
// for ps use only, placed in psram part0
#define PS_FPSRAM_P0_RAMCODE
// placed in psram part0
#define AP_FPSRAM_P0_DATA
// placed in psram part0
#define AP_FPSRAM_P0_BSS
#define AP_PS_FPSRAM_P0_BSS

// placed in psram part1
#define AP_PS_FPSRAM_P1_DATA
#define AP_PS_FPSRAM_P1_BSS

#define AP_PLAT_FPSRAM_P1_DATA
#define AP_PLAT_FPSRAM_P1_BSS

// for customer use only, placed in psram part2
#define CUST_FPSRAM_P2_RAMCODE
// for customer use only, placed in psram part2
#define CUST_FPSRAM_P2_DATA
// for customer use only, placed in psram part2
#define CUST_FPSRAM_P2_BSS

// CCIO BUF
#define PLAT_CCIO_BUF
#endif

#ifdef TYPE_EC718M
#define AP_PLAT_COMMON_DATA AP_FPSRAM_P0_DATA
#define AP_PLAT_COMMON_BSS AP_FPSRAM_P0_BSS
#else
#define AP_PLAT_COMMON_DATA
#define AP_PLAT_COMMON_BSS
#endif


/*used by both bl/ap*/
#define FLASH_TEXT_SECTION      SECTION_DEF_IMPL(.sect_flash_text) SECTION_DEF_RAMCODE
#define FLASH_TEXT_PRE2SECTION  SECTION_DEF_IMPL(.sect_flash_pre2text) SECTION_DEF_RAMCODE
#define FLASH_DATA_SECTION      SECTION_DEF_IMPL(.sect_flash_data)
#define FLASH_BSS_SECTION       SECTION_DEF_IMPL(.sect_flash_bss)


// ap section define start
#define RAM_BOOT_CODE SECTION_DEF_IMPL(.sect_ramBootCode_text) SECTION_DEF_RAMCODE
// for ps use only, load before paging image, placed in asmb
#define PS_PA_RAMCODE SECTION_DEF_IMPL(.sect_psPARamcode_text) SECTION_DEF_RAMCODE
// for ps use only, load before paging image, placed in msmb
#define PS_PM_RAMCODE SECTION_DEF_IMPL(.sect_psPMRamcode_text) SECTION_DEF_RAMCODE
// for ps use only, load before full image, placed in asmb
#define PS_FA_RAMCODE SECTION_DEF_IMPL(.sect_psFARamcode_text) SECTION_DEF_RAMCODE
// for ps use only, load before full image, placed in msmb
#define PS_FM_RAMCODE SECTION_DEF_IMPL(.sect_psFMRamcode_text) SECTION_DEF_RAMCODE

#ifdef TYPE_EC718M
// for ps use only, data in MSMB
#define PS_FM_ZI    SECTION_DEF_IMPL(.sect_psFMZIData_bss)
#define PS_FM_DATA  SECTION_DEF_IMPL(.sect_psFMRWData_data)
#else
// for ps use only, data in MSMB
#define PS_FM_ZI
#define PS_FM_DATA
#endif

// for plat use only, load before paging image, placed in asmb
#define PLAT_PA_RAMCODE SECTION_DEF_IMPL(.sect_platPARamcode_text) SECTION_DEF_RAMCODE
// for plat use only, load before paging image, placed in msmb
#define PLAT_PM_RAMCODE SECTION_DEF_IMPL(.sect_platPMRamcode_text) SECTION_DEF_RAMCODE
// for plat use only, load before full image, placed in asmb
#define PLAT_FA_RAMCODE SECTION_DEF_IMPL(.sect_platFARamcode_text) SECTION_DEF_RAMCODE
// for plat use only, load before full image, placed in msmb
#define PLAT_FM_RAMCODE SECTION_DEF_IMPL(.sect_platFMRamcode_text) SECTION_DEF_RAMCODE

#ifdef TYPE_EC718M
// for ps use only, data in MSMB
#define PLAT_FM_ZI      SECTION_DEF_IMPL(.sect_platFMZIData_bss)
#define PLAT_FM_DATA    SECTION_DEF_IMPL(.sect_platFMRWData_data)
#else
// for ps use only, data in MSMB
#define PLAT_FM_ZI
#define PLAT_FM_DATA
#endif



#define PLAT_PM_UNCOMP_RAMCODE  SECTION_DEF_IMPL(.sect_uncompress_platPMRamcode_text) SECTION_DEF_RAMCODE

// data placed in asmb
#define PLAT_FA_ZI SECTION_DEF_IMPL(.sect_platFAZIData_bss)
#define PLAT_FA_DATA SECTION_DEF_IMPL(.sect_platFARWData_data)
#define PLAT_PA_NOINIT SECTION_DEF_IMPL(.sect_platPANoInit_bss)
#define PLAT_PC_NOINIT SECTION_DEF_IMPL(.sect_platPCNoInit_bss)
#define PLAT_PM_NOINIT SECTION_DEF_IMPL(.sect_platPMNoInit_bss)

// rw and zi for ps use, init when fullimage start(only from hibernate and power on)
// never re-init if asmb not power off
#define PS_FA_ZI SECTION_DEF_IMPL(.sect_psFAZIData_bss)
#define PS_FA_DATA SECTION_DEF_IMPL(.sect_psFARWData_data)
#define PS_FA_NOINIT SECTION_DEF_IMPL(.sect_psFANoInit_data)

/*Lierda Begin: Add Sleep2 used 1k sram by zhaoliechang in 20231202*/
#define CUST_FA_ZI SECTION_DEF_IMPL(.custFAZIData)
/*Lierda End: Add Sleep2 used 1k sram by zhaoliechang in 20231202*/


// buffer for voice engine, no need to init
#define PS_FM_VOICE_NOINIT SECTION_DEF_IMPL(.sect_voiceEngSharebuf)
// up and down buffer
#define CAT_PSPHY_SHAREDATA SECTION_DEF_IMPL(.sect_catShareBuf_data)



#if defined (PSRAM_FEATURE_ENABLE) && (PSRAM_EXIST==1) || (defined TYPE_EC718M)
// for ap plat use only, load before full image and after psram init, placed in psram
#define PLAT_FPSRAM_RAMCODE SECTION_DEF_IMPL(.sect_platFPSRAMRamcode_text)
// data placed in psram
#define PLAT_FPSRAM_ZI SECTION_DEF_IMPL(.sect_platFPSRAMZIData_bss)
#define PLAT_FPSRAM_DATA SECTION_DEF_IMPL(.sect_platFPSRAMRWData_data)

// heap6_psram use only
#define PLAT_PSRAM_HEAP6_RAMCODE SECTION_DEF_IMPL(.sect_freertos_psram_heap6_text)
#define PLAT_FPSRAM_HEAP6_ZI SECTION_DEF_IMPL(.sect_platFPSRAMZIData_HEAP6_bss)

#ifndef TYPE_EC718M
// for customer use only, load before full image and after psram init, placed in psram
#define PLAT_FPSRAM_RAMCODE_CUST SECTION_DEF_IMPL(.sect_platFPSRAMRamcode_text_cust)
// data placed in psram for customer
#define PLAT_FPSRAM_ZI_CUST      SECTION_DEF_IMPL(.sect_platFPSRAMZIData_bss_cust)
#define PLAT_FPSRAM_DATA_CUST    SECTION_DEF_IMPL(.sect_platFPSRAMRWData_data_cust)
#else
// for customer use only, load before full image and after psram init, placed in psram
#define PLAT_FPSRAM_RAMCODE_CUST CUST_FPSRAM_P2_RAMCODE
// data placed in psram for customer
#define PLAT_FPSRAM_ZI_CUST      CUST_FPSRAM_P2_BSS
#define PLAT_FPSRAM_DATA_CUST    CUST_FPSRAM_P2_DATA
#endif
#endif
// cp section define
#ifdef TYPE_EC718M
#define CP_RAMCODE_IN_PSRAM_P0 SECTION_DEF_IMPL(.sect_cp_psram_p0_text) SECTION_DEF_RAMCODE
#define CP_DATA_IN_PSRAM_P0    SECTION_DEF_IMPL(.sect_cp_psram_p0_data)
#define CP_BSS_IN_PSRAM_P0     SECTION_DEF_IMPL(.sect_cp_psram_p0_bss)
#else
#define CP_RAMCODE_IN_PSRAM_P0
#define CP_DATA_IN_PSRAM_P0
#define CP_BSS_IN_PSRAM_P0
#endif

// Most timing critical code, both used in paging&full image
// so need to copy from FLASH when wakeup from HIB
// NOTE: Paging&PreSync related CODE should only allocated to PHY_CODE_IN_CSMB0 or FLASH
#define CP_PLAT_CODE_IN_CSMB SECTION_DEF_IMPL(.sect_cpPlatCodeCsmb_text) SECTION_DEF_RAMCODE
// Most timing critical code, only used in full image
// TX/RA/CSI/ConnMeas/CDRX should be allocate to CSMB1 or MSMB1
// so NO need to copy from FLASH when wakeup from HIB(these code do not used in paging image)
#define CP_PLAT_CODE_IN_MSMB SECTION_DEF_IMPL(.sect_cpPlatCodeMsmb_text) SECTION_DEF_RAMCODE
#define CP_PLAT_CODE_IN_FLASH SECTION_DEF_IMPL(.sect_cpPlatCodeFlash_text)


// OS related, all dynamic allocated data, other static global data
#define CP_PLAT_ZI_IN_MSMB  SECTION_DEF_IMPL(.sect_cpPlatZIMsmb_bss)

#define CP_PLAT_DATA_IN_FLASH SECTION_DEF_IMPL(.sect_cpPlatDataFlash_data)

#define LIOT_CHENGED_P2_RAM CUST_FPSRAM_P2_DATA

#ifndef USED
#define USED      __attribute__((used))
#endif



#endif

