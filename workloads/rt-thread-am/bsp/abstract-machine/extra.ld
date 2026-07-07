SECTIONS {
  .rodata.extra : {
    . = ALIGN(8);
    /* section information for finsh shell */
    __fsymtab_start = .;
    KEEP(*(FSymTab))
    __fsymtab_end = .;
    . = ALIGN(8);
    __vsymtab_start = .;
    KEEP(*(VSymTab))
    __vsymtab_end = .;
    . = ALIGN(8);

    /* section information for initial.
     * ld.lld's SORT() does not reliably order these by level, so list the
     * standard RT-Thread init sections explicitly. */
    __rt_init_start = .;
    KEEP(*(.rti_fn.0))
    KEEP(*(.rti_fn.0.end))
    KEEP(*(.rti_fn.1))
    KEEP(*(.rti_fn.1.end))
    KEEP(*(.rti_fn.2))
    KEEP(*(.rti_fn.3))
    KEEP(*(.rti_fn.4))
    KEEP(*(.rti_fn.5))
    KEEP(*(.rti_fn.6))
    KEEP(*(.rti_fn.6.end))
    __rt_init_end = .;
    . = ALIGN(8);

    __rt_utest_tc_tab_start = .;
    KEEP(*(UtestTcTab))
    __rt_utest_tc_tab_end = .;
    . = ALIGN(8);
  }
}
INSERT AFTER .rodata;

SECTIONS {
  .data.extra : {
    . = ALIGN(8);
    __am_apps_data_start = .;
    *(__am_apps.data*)
    *(__am_apps.sdata*)
    __am_apps_data_end = .;
    . = ALIGN(8);
  }
}
INSERT BEFORE .data;

SECTIONS {
  .bss.extra : {
    . = ALIGN(8);
    __am_apps_bss_start = .;
    *(__am_apps.bss*)
    *(__am_apps.sbss*)
    *(__am_apps.scommon*)
    __am_apps_bss_end = .;
    . = ALIGN(8);
  }
}
INSERT BEFORE .bss;
