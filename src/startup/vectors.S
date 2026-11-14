.syntax unified
.cpu cortex-m7
.thumb
.section .isr_vector,"a",%progbits
.global vectors
vectors:
.word _estack
.word Reset_Handler
.word Default_Handler
.rept 4
.word HardFault_Handler
.endr
.rept 8
.word Default_Handler
.endr
.word SysTick_Handler
.rept 77
.word Default_Handler
.endr
.word OTG_HS_IRQHandler
.rept 77
.word Default_Handler
.endr
