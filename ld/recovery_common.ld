ENTRY(Reset_Handler)
MEMORY {
    BOOT (rx) : ORIGIN = __image_origin, LENGTH = 128K
    CODE (rwx) : ORIGIN = 0x24040000, LENGTH = 128K
    RAM (rw) : ORIGIN = 0x20000000, LENGTH = 0x1FF00
}
_estack = ORIGIN(RAM) + LENGTH(RAM);
SECTIONS {
    .isr_vector : { KEEP(*(.isr_vector)) } > BOOT
    .bootstrap : { KEEP(*(.bootstrap*)) . = ALIGN(4); } > BOOT
    .text : {
        _stext = .;
        *(.text*) *(.rodata*)
        . = ALIGN(4);
        KEEP(*(.init)) KEEP(*(.fini))
        __preinit_array_start = .; KEEP(*(.preinit_array*)) __preinit_array_end = .;
        __init_array_start = .; KEEP(*(.init_array*)) __init_array_end = .;
        __fini_array_start = .; KEEP(*(.fini_array*)) __fini_array_end = .;
        . = ALIGN(4); _etext = .;
    } > CODE AT> BOOT
    _sitext = LOADADDR(.text);
    .ARM.extab : { *(.ARM.extab*) } > CODE AT> BOOT
    .ARM.exidx : { __exidx_start = .; *(.ARM.exidx*) __exidx_end = .; } > CODE AT> BOOT
    .data : { . = ALIGN(4); _sdata = .; *(.data*) . = ALIGN(4); _edata = .; } > RAM AT> BOOT
    _sidata = LOADADDR(.data);
    .bss (NOLOAD) : { . = ALIGN(1024); _sbss = .; *(.bss*) *(COMMON) . = ALIGN(8); _ebss = .; } > RAM
    . = ALIGN(8); _end = .; PROVIDE(end = .);
    /DISCARD/ : { *(.comment) *(.note*) }
    ASSERT(_ebss + 98304 < _estack, "Recovery RAM leaves less than 96 KiB stack")
    ASSERT(LOADADDR(.data) + SIZEOF(.data) <= ORIGIN(BOOT) + LENGTH(BOOT), "Recovery exceeds fixed 128 KiB reservation")
}

