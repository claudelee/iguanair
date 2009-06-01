; **************************************************************************
; * boot.asm ***************************************************************
; **************************************************************************
; *
; * Based on a generated file, this contains only the parts necessary
; * to define the basic areas.
; *
; * Copyright (C) 2007, IguanaWorks Incorporated (http://iguanaworks.net)
; * Author: Joseph Dunn <jdunn@iguanaworks.net>
; *
; * Distributed under the GPL version 2.
; * See LICENSE for license details.
; */

include "loader.inc"

;-----------------------------------------------------------------------------
; RAM segments for C CONST, static & global items
;-----------------------------------------------------------------------------
    AREA lit
__lit_start:

    AREA idata
__idata_start:

    AREA func_lit
__func_lit_start:

    AREA psoc_config(ROM,REL,CON)
__psoc_config_start:

    AREA UserModules(ROM,REL,CON)
__usermodules_start:

    AREA gpio_isr(ROM,REL,CON)
__gpio_isr_start:

;---------------------------------------------
;         CODE segment for general use
;---------------------------------------------
    AREA text(ROM,REL,CON)
__text_start:

;---------------------------------------------
;         Begin RAM area usage
;---------------------------------------------
    AREA data              (RAM, REL, CON)   ; initialized RAM
__data_start:

    AREA virtual_registers (RAM, REL, CON)   ; Temp vars of C compiler
    AREA InterruptRAM      (RAM, REL, CON)   ; Interrupts, on Page 0
    AREA bss               (RAM, REL, CON)   ; general use
  BLK BODY_SKIP_BYTES - 3
__bss_start: