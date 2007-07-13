; Generated by PSoC Designer ver 4.2  b1013 : 02 September, 2004
;
;==========================================================================
;  PSoCConfig.asm
;  @PSOC_VERSION
;
;  Version: 0.85
;  Revised: June 22, 2004
;  Copyright Cypress MicroSystems 2000-2004. All Rights Reserved.
;
;  This file is generated by the Device Editor on Application Generation.
;  It contains code which loads the configuration data table generated in
;  the file PSoCConfigTBL.asm
;
;  DO NOT EDIT THIS FILE MANUALLY, AS IT IS OVERWRITTEN!!!
;  Edits to this file will not be preserved.
;==========================================================================
;
include "m8c.inc"
include "memory.inc"
include "GlobalParams.inc"

export LoadConfigInit
export _LoadConfigInit
export LoadConfig_usb_ir
export _LoadConfig_usb_ir
export Port_1_Data_SHADE
export _Port_1_Data_SHADE


export NO_SHADOW
export _NO_SHADOW

FLAG_CFG_MASK:      equ 10h         ;M8C flag register REG address bit mask
END_CONFIG_TABLE:   equ ffh         ;end of config table indicator

AREA psoc_config(rom, rel)


;---------------------------------------------------------------------------
; LoadConfigInit - Establish the start-up configuration (except for a few
;                  parameters handled by boot code, like CPU speed). This
;                  function can be called from user code, but typically it
;                  is only called from boot.
;
;       INPUTS: None.
;      RETURNS: Nothing.
; SIDE EFFECTS: Registers are volatile: the A and X registers can be modified!
;               In the large memory model currently only the page
;               pointer registers listed below are modified.  This does
;               not guarantee that in future implementations of this
;               function other page pointer registers will not be
;               modified.
;          
;               Page Pointer Registers Modified: 
;               CUR_PP
;
_LoadConfigInit:
 LoadConfigInit:
    RAM_PROLOGUE RAM_USE_CLASS_4
    
	mov		[Port_1_Data_SHADE], 0h

	lcall	LoadConfig_usb_ir

    RAM_EPILOGUE RAM_USE_CLASS_4
    ret

;---------------------------------------------------------------------------
; Load Configuration usb_ir
;
;    Load configuration registers for usb_ir.
;    IO Bank 0 registers a loaded first,then those in IO Bank 1.
;
;       INPUTS: None.
;      RETURNS: Nothing.
; SIDE EFFECTS: Registers are volatile: the CPU A and X registers may be
;               modified as may the Page Pointer registers!
;               In the large memory model currently only the page
;               pointer registers listed below are modified.  This does
;               not guarantee that in future implementations of this
;               function other page pointer registers will not be
;               modified.
;          
;               Page Pointer Registers Modified: 
;               CUR_PP
;
_LoadConfig_usb_ir:
 LoadConfig_usb_ir:
    RAM_PROLOGUE RAM_USE_CLASS_4
    lcall   LoadConfigTBL_usb_ir            ; Call load config table routine


    RAM_EPILOGUE RAM_USE_CLASS_4
    ret



AREA InterruptRAM(ram, rel)

NO_SHADOW:
_NO_SHADOW:
; write only register shadows
_Port_1_Data_SHADE:
Port_1_Data_SHADE:	BLK	1

