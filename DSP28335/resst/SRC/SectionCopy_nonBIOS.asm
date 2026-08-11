;############################################################################
;
; FILE:   DSP28xxx_SectionCopy_nonBIOS.asm
;
; DESCRIPTION:  Provides functionality for copying intialized sections from 
;				flash to ram at runtime before entering the _c_int00 startup
;				routine
;############################################################################
; Author: Tim Love
; Release Date: March 2008	
;############################################################################


	.ref _c_int00
	.global copy_sections
	.global _RamfuncsLoadStart, _RamfuncsRunStart, _RamfuncsLoadEnd
***********************************************************************
* Function: copy_sections
*
* Description: Copies initialized sections from flash to ram
***********************************************************************

	.sect "copysections"

copy_sections:


	MOVL XAR5,#_RamfuncsLoadEnd				; Store Section Size in XAR5
	MOVL ACC,@XAR5						; Move Section Size to ACC
	MOVL XAR6,#_RamfuncsLoadStart			; Store Load Starting Address in XAR6
    MOVL XAR7,#_RamfuncsRunStart			; Store Run Address in XAR7
    LCR  copy

  	LB _c_int00				 			; Branch to start of boot.asm in RTS library

copy:	
	B return,EQ							; Return if ACC is Zero (No section to copy)

	SUBB ACC,#1

    RPT AL								; Copy Section From Load Address to
    || PWRITE  *XAR7, *XAR6++			; Run Address

return:
	LRETR								; Return

	.end
	
;//===========================================================================
;// End of file.
;//===========================================================================
