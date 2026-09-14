/*
 * Linker script for the STM32F746ZG of a NUCLEO-F746ZG board: 1 MiB of
 * flash and 320 KiB of RAM.
 */

#include "kconfig.h"

/* Entry Point */
ENTRY(Reset_Handler)

RAM_ADDR = 0x20000000;
/* The 64 KiB the core reaches through its data bus, the 240 KiB of the first
 * SRAM and the 16 KiB of the second follow one another without a gap, so the
 * whole 320 KiB is one region
 */
RAM_SIZE = 320K;

USER_STACK_SIZE = 20K;

#if (PAGE_SIZE_SELECT == PAGE_SIZE_64K)
PAGE_SECTION_SIZE = 64K;
PAGE_SIZE_MAX = 8K;
#elif (PAGE_SIZE_SELECT == PAGE_SIZE_32K)
PAGE_SECTION_SIZE = 32K;
PAGE_SIZE_MAX = 4K;
#else
#error "Unknown page size setting"
#endif

/* Highest address of the user mode stack */
_estack = 0x20050000;    /* end of the 320 KiB */

/* Specify the memory areas */
MEMORY
{
  FLASH (rx)      : ORIGIN = 0x08000000, LENGTH = 1024K
  RAM (xrw)       : ORIGIN = RAM_ADDR, LENGTH = RAM_SIZE
  MEMORY_B1 (rx)  : ORIGIN = 0x60000000, LENGTH = 0K
}

/* Define output sections */
SECTIONS
{
  /* The startup code goes first into FLASH */
  .isr_vector :
  {
    . = ALIGN(4);
    KEEP(*(.isr_vector)) /* Startup code */
    . = ALIGN(4);
  } >FLASH

  /* The program code and other data goes into FLASH */
  .text :
  {
    . = ALIGN(4);
    *(.text)           /* .text sections (code) */
    *(.text*)          /* .text* sections (code) */
    *(.rodata)         /* .rodata sections (constants, strings, etc.) */
    *(.rodata*)        /* .rodata* sections (constants, strings, etc.) */
    *(.glue_7)         /* glue arm to thumb code */
    *(.glue_7t)        /* glue thumb to arm code */
    *(.eh_frame)

    _shell_cmds_start = .;
    KEEP (*(.shell_cmds))
    _shell_cmds_end = .;

    _tasks_start = .;
    KEEP (*(.tasks))
    _tasks_end = .;

    _rom_start = .;
    KEEP (*(.rom*))
    _rom_end = .;

    KEEP (*(.init))
    KEEP (*(.fini))

    . = ALIGN(4);
    _etext = .;        /* define a global symbols at end of code */
  } >FLASH

  .ARM.extab : { *(.ARM.extab* .gnu.linkonce.armextab.*) } >FLASH
    .ARM : {
    __exidx_start = .;
    *(.ARM.exidx*)
    __exidx_end = .;
  } >FLASH

  .preinit_array     :
  {
    PROVIDE_HIDDEN (__preinit_array_start = .);
    KEEP (*(.preinit_array*))
    PROVIDE_HIDDEN (__preinit_array_end = .);
  } >FLASH
  .init_array :
  {
    PROVIDE_HIDDEN (__init_array_start = .);
    KEEP (*(SORT(.init_array.*)))
    KEEP (*(.init_array*))
    PROVIDE_HIDDEN (__init_array_end = .);
  } >FLASH
  .fini_array :
  {
    PROVIDE_HIDDEN (__fini_array_start = .);
    KEEP (*(.fini_array*))
    KEEP (*(SORT(.fini_array.*)))
    PROVIDE_HIDDEN (__fini_array_end = .);
  } >FLASH

  /* used by the startup to initialize data */
  _sidata = .;

  /* The buddy page allocator derives the address of a page from its index
   * and recovers the address of a slab by masking out the offset within the
   * page. Both only work if the section itself is aligned to the size of the
   * largest page. Placing the section at the very beginning of the RAM keeps
   * the alignment free of any padding.
   */
  .pgmem (NOLOAD) : ALIGN(PAGE_SIZE_MAX)
  {
    PROVIDE (_page_mem_start = .);
    . += PAGE_SECTION_SIZE;
    PROVIDE (_page_mem_end = .);
  } >RAM

  /* What the Ethernet controller reaches as a bus master.
   *
   * The core keeps a copy of what it reads and writes, and the controller
   * knows nothing of that copy: what the core wrote may still be in it when
   * the controller reads, and what the controller wrote is not in it when the
   * core reads. So the whole of what the two share is gathered here and the
   * protection unit is told to leave this much of memory uncached.
   *
   * That unit can only cover a run of memory whose length is a power of two
   * and whose start is a multiple of that length, which is why this is aligned
   * and padded rather than merely placed.
   */
  .eth_dma (NOLOAD) : ALIGN(8K)
  {
    PROVIDE (_eth_dma_start = .);
    *(.eth_dma)
    . = ALIGN(8K);
    PROVIDE (_eth_dma_end = .);
  } >RAM

  /* The protection unit is told this region is 8 KiB and cannot be told a
   * length that is not a power of two. Should something later be added to the
   * section that pushes it past that, the linker would quietly grow it and the
   * unit would go on covering only the first half -- leaving the rest cached,
   * shared with a bus master, and wrong in a way nothing reports. So the build
   * stops here instead.
   */
  ASSERT(_eth_dma_end - _eth_dma_start <= 8K,
         "the .eth_dma section no longer fits the region the MPU marks uncached; grow both together in platform/system_stm32f746.c")

  /* Initialized data sections goes into RAM, load LMA copy after code */
  .data : AT ( _sidata )
  {
    . = ALIGN(4);
    _sdata = .;        /* create a global symbol at data start */

    /* BusyBox is written for a system where a command runs as a process and
     * therefore starts from the initial state of the program. Its globals are
     * kept apart so that applet_run() can restore them before every run.
     */
    . = ALIGN(4);
    _bb_data_start = .;
    *lib*busybox*.o(.data .data*)
    . = ALIGN(4);
    _bb_data_end = .;

    *(.data)           /* .data sections */
    *(.data*)          /* .data* sections */

    . = ALIGN(4);
    _edata = .;        /* define a global symbol at data end */
  } >RAM

  _bb_data_load = LOADADDR(.data) + (_bb_data_start - ADDR(.data));

  /* Uninitialized data section */
  . = ALIGN(4);
  .bss :
  {
    /* This is used by the startup in order to initialize the .bss secion */
    _sbss = .;         /* define a global symbol at bss start */
    __bss_start__ = _sbss;

    /* Restored by applet_run(), see the note in .data */
    . = ALIGN(4);
    _bb_bss_start = .;
    *lib*busybox*.o(.bss .bss* COMMON)
    . = ALIGN(4);
    _bb_bss_end = .;

    *(.bss)
    *(.bss*)
    *(COMMON)

    . = ALIGN(4);
    _ebss = .;         /* define a global symbol at bss end */
    __bss_end__ = _ebss;
  } >RAM

  . = ALIGN(4);
  .user_stack (NOLOAD) :
  {
    PROVIDE (_user_stack_start = .);
    . += USER_STACK_SIZE;
    PROVIDE (_user_stack_end = .);
  } >RAM

  /* MEMORY_bank1 section, code must be located here explicitly            */
  /* Example: extern int foo(void) __attribute__ ((section (".mb1text"))); */
  .memory_b1_text :
  {
    *(.mb1text)        /* .mb1text sections (code) */
    *(.mb1text*)       /* .mb1text* sections (code)  */
    *(.mb1rodata)      /* read-only data (constants) */
    *(.mb1rodata*)
  } >MEMORY_B1

  .ARM.attributes 0 : { *(.ARM.attributes) }
}
