ENTRY(_start)
OUTPUT_FORMAT(elf32-i386)
OUTPUT_ARCH(i386:i386)

SECTIONS
{
    . = 0x100000;

    kernelStartMarker = .;

    .text : {
        *(multiboot)
        *(.text)
    }
    .data ALIGN(4096) : {
        start_ctors = .;
        KEEP(*( .ctors ));
        KEEP(*(SORT_BY_INIT_PRIORITY( .ctors.* )));
        end_ctors = .;

        start_dtors = .;
        KEEP(*( .fini_array ));
        end_dtors = .;

        *(.data)
    }
    .rodata ALIGN(4096) : {
        *(.rodata)
    }
    .bss ALIGN(4096) : {
        *(.bss)
    }

    /* Align the end of the kernel to the page size */
    . = ALIGN(4096);
    kernelEndMarker = .;
}