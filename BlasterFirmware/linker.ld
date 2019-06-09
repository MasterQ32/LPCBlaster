/*******************************************
   Memory Definitions für LPC 1768
*******************************************/
MEMORY
{
  ram (RW) : ORIGIN = 0x10001000, LENGTH = 32k-32-0x1000	/*  RAM - Sprungtabelle - IAP-Memory */
}

/*
 * IAP routine location. 1 makes the routine a thumb routine!
 */
IAP = 0x1FFF1FF1;

ahbram = 0x2007C000;


ENTRY(LPCBlasterEntry)

/*******************************************
   Section Definitions
*******************************************/
SECTIONS
{
    /* Code-Speicher im Flash ***********/
    .text :
    {
        /* start with interrupt vector table */
        . = ALIGN(4);
        KEEP(*( .isr_vector ));

        . = ALIGN(4);
        *(.text)
        *(.text.*)

        . = ALIGN(4);

        *(.gnu.linkonce.t.*)
        *(.glue_7)
        *(.glue_7t)
        *(.gcc_except_table)
        *(.gnu.linkonce.r.*)

    } >ram

    . = ALIGN(4);

    /* Konstanten im Flash ****************/
    .rodata :
    {
        *(.rodata)
        *(.rodata.*)

        start_ctors = .;
        KEEP(*( .init_array ));
        KEEP(*(SORT_BY_INIT_PRIORITY( .init_array.* )));
        end_ctors = .;

        start_dtors = .;
        KEEP(*( .fini_array ));
        KEEP(*(SORT_BY_INIT_PRIORITY( .fini_array.* )));
        end_dtors = .;

    } >ram

    /* Initialisierte Variablen im Flash */
    .data :
    {
      *(.data)
      *(.data.*)
      *(.gnu.linkonce.d*)
      SORT(CONSTRUCTORS)
    } >ram

   .bss :
   {
        *(.bss)
        *(.bss.*)
        *(.gnu.linkonce.b*)
        . = ALIGN(4);
    } >ram
}

