; Calls a compiler-generated C function with R8/R9 set and checks they survive.
.text
.global _start
.extern abi_target
_start:
    ld r8, #0x1234
    ld r9, #0x5678
    call abi_target
    ld r6, #0x1234
    cmp r8, r6
    bne fail
    ld r6, #0x5678
    cmp r9, r6
    bne fail
    ld r2, #42
    ld r7, #0
    sys
fail:
    ld r2, #1
    ld r7, #0
    sys
