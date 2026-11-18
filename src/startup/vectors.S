.syntax unified
.cpu cortex-m7
.thumb
/* Two tables of the same 171 entries (16 core, IRQ 0..154). The Cortex-M7 reserved entries 7..10
   and 13 are 0 in both.
   - image_vectors (.isr_vector, the image's first bytes): what a loader reads and hands over
     (factory boot: word 0 checked, word 1 called; our Handoff also sets VTOR here). Every handler
     is Bootstrap_Fault, which lies in the image itself (.bootstrap): in Recovery the RAM code is not
     copied yet when a fault before Reset_Handler's VTOR switch could take one.
   - vectors (.rodata, relocated with the code in Recovery): the handlers Reset_Handler installs in
     runtime_vectors once the code is in place. */
.macro core_reserved
.word 0, 0, 0, 0
.endm

.section .isr_vector,"a",%progbits
.global image_vectors
image_vectors:
.word _estack
.word Reset_Handler
.rept 5                 /* NMI, HardFault, MemManage, BusFault, UsageFault */
.word Bootstrap_Fault
.endr
core_reserved           /* 7..10 */
.word Bootstrap_Fault   /* SVCall */
.word Bootstrap_Fault   /* DebugMonitor */
.word 0                 /* 13 */
.rept 157               /* PendSV, SysTick, IRQ 0..154 */
.word Bootstrap_Fault
.endr

.section .rodata.vectors,"a",%progbits
.balign 4
.global vectors
vectors:
.word _estack
.word Reset_Handler
.word Default_Handler   /* NMI */
.rept 4                 /* HardFault, MemManage, BusFault, UsageFault */
.word HardFault_Handler
.endr
core_reserved           /* 7..10 */
.word Default_Handler   /* SVCall */
.word Default_Handler   /* DebugMonitor */
.word 0                 /* 13 */
.word Default_Handler   /* PendSV */
.word SysTick_Handler
.rept 77                /* IRQ 0..76 */
.word Default_Handler
.endr
.word OTG_HS_IRQHandler /* IRQ 77 */
.rept 77                /* IRQ 78..154 */
.word Default_Handler
.endr
