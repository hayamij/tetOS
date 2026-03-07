ENTRY(_start)

SECTIONS
{
    . = 0x1000;
    
    .text : ALIGN(16)
    {
        *(.text._start)
        *(.text)
    }
    
    .rodata : ALIGN(16)
    {
        *(.rodata)
    }
    
    .data : ALIGN(16)
    {
        *(.data)
    }
    
    .bss : ALIGN(16)
    {
        *(COMMON)
        *(.bss)
    }
}
