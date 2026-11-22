; A constructor entry: the runtime's crt0 must walk .init_array before main.
.init_array
.word init_function
.text
.global init_function
.extern constructor_ran
init_function:
    ld r6, #constructor_ran
    ld r7, #1
    st [r6], r7
    ret
