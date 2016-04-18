    .code   16
    .text                   @@ - Code section (text -> ROM)

    .align  2
    .syntax unified

    .global release_processor
    .thumb_func
release_processor:
    push {lr}
    svc #1
    pop  {pc}

    .global switch_stacks
    .thumb_func
switch_stacks:
    ldr r0, =next_sp
    ldr r0, [r0]
    msr MSP, r0
    isb
    pop {r4, r5, r6, r7, r8, r9, r10, r11}
    pop {PC}

    .global SVC_Handler
    .thumb_func
SVC_Handler:
    push {LR, r4, r5, r6, r7, r8, r9, r10, r11}
    mrs r0, MSP       @@  Make a copy of the process stack pointer
    mov r1, #0        @@  Use 0 to identify an entry from a kernel call 
    b c_SVC_Handler

    .global SysTick_Handler
    .thumb_func
SysTick_Handler:
    push {LR, r4, r5, r6, r7, r8, r9, r10, r11}
    bl HAL_IncTick    @@  Required by other things like USB communication
    pop {r4, r5, r6, r7, r8, r9, r10, r11}
    pop {PC}
