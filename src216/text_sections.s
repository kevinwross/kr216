.section .text.main
.global main
main:
    ld r2, #42
    beq skip
skip:
    jmp done
.section .text.done
done:
    ret
