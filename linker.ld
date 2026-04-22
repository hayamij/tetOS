ENTRY(_start)

SECTIONS
{
    . = 0x1000;

    .text : ALIGN(16)
    {
        *(.text._start)
        *(.text)
        *(.text.*)
    }

    .rodata : ALIGN(16)
    {
        *(.rodata)
        *(.rodata.*)
    }

    .data : ALIGN(16)
    {
        *(.data)
        *(.data.*)
    }

    .bss : ALIGN(16)
    {
        bss_start = .;
        *(COMMON)
        *(.bss)
        *(.bss.*)
        bss_end = .;
    }

    kernel_end = .;

    /DISCARD/ :
    {
        *(.eh_frame)
        *(.eh_frame_hdr)
        *(.comment)
        *(.note*)
    }
}
