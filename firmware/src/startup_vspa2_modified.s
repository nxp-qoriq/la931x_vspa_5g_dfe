// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2022 - 2026  NXP



//This file is modified based on Codewarrior file startup_vspa2.s which doesn't not support Go events
//The modified startup file supports go event well
//All modified places are marked with //remarked by Michael Liu



    .file    "startup_vspa2_modified.s"

    .section .init, "ax", @progbits
    .global  _start
    .type    _start, @function
_start
    /* Disable VSPA1 mode / enable half-word addressing */
    set.creg 22, 0;
    set.creg 4, 0;                /* Disable CC update */
    mv       sp, __stack;         /* Initialize Stack Pointer */
    mv		 a0, sp;              /* clear stack */
    mv		 g0, StackSize/2
    set.loop g0, 1
    mv		 g2, 0xdeadbeef;
    st       [a0]+2, g2;	loop_begin; loop_end;

    jsr      _init_bss;           /* Initialize bss to 0 */
    fnop;
    fnop;

    ld          g11, ___entry           // Entry-point address.
    nop                                 // Pipeline restriction.
    nop                                 // Pipeline restriction.
    nop                                 // Pipeline restriction.
    nop                                 // Pipeline restriction.
    jmp        g11                      // Jump to entry-point.
    
    
    fnop; //mv       a0, __debug_argv;    /* Move argv to a0 */
    fnop;
    .size    _start, .-_start



    /* Initialize the bss with zeros */
    .section .text
    .global  _init_bss
    .type    _init_bss, @function
_init_bss
    mv       a0, __bss_start;
	mv       g0, __bss_end - __bss_start
	set.loop g0, 1
    mv       g2, 0;
    sth       [a0]+1, g2; loop_begin; loop_end

	mv       g0, __ibss_end - __ibss_start
	set.loop g0, 1
    mv       a0, __ibss_start;
    sth       [a0]+1, g2; loop_begin; loop_end

    rts;
    fnop;
    fnop;
    .size    _init_bss, .-_init_bss

//    .section .data, "aw", @progbits
//    .global  __debug_argv
//    .global  __debug_argc
    
//    .type    __debug_argc, @object
//    .size    __debug_argc, 4
//__debug_argc
//    .symbol_align __debug_argc, 4
//    .space   0x4    /* 4 bytes */

//    .type    __debug_argv, @object
//    .size    __debug_argv, 0x400
//__debug_argv
//    .symbol_align __debug_argv, 4
//    .space   0x400  /* 256 words */





//added all the following by Michael Liu 


//
// void __reset(void) __noreturn
//
//  Parameters:
//      None.
//
//  Return value:
//      This function does not return.
//
//  Clobbered registers:
//      g0
//
    .global     ___reset
    .type       ___reset, %function
    .size       ___reset, ___reset_end-___reset

    .section    .init, "ax", %progbits
___reset:
    // Invoke user initialization:
    jsr     ___init                     // Invoke user init procedure.
    st      ___entry, _main
    fnop     
    jsr		_main						//run init() and then main()
                               // Pipeline restriction.
    fnop
    fnop
    done
    fnop
    fnop
___reset_end:
    



//
// void main(void) __noreturn
//
    .global     _main
    .type       _main, %function
    .size       _main, _main_end-_main
    .weak       _main

    .section    .text, "ax", %progbits
_main:
    nop
    done
    fnop
    fnop
_main_end:

//
// void __init(void)
//
    .global     ___init
    .type       ___init, %function
    .size       ___init, ___init_end-___init
    .weak       ___init

    .section    .text, "ax", %progbits
___init:
    rts
    fnop
    fnop
___init_end:

//
// __entry_t __entry = __reset
//
    .global     ___entry
    .type       ___entry, %object
    .size       ___entry, ___entry_end-___entry

    .section    .data
___entry:
    .word       ___reset
___entry_end:

  
