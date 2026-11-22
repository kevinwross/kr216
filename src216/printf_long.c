#include <stdio.h>
int main(void) {
    printf("%ld\n", 70000L);
    printf("%ld\n", 0 - 70000L);
    printf("%lu\n", 70000UL);
    printf("%lx\n", 0xABCDEFUL);
    printf("%lld\n", 0x123456789ABCLL);
    printf("%lld\n", 0 - 0x100000000LL);
    printf("%llu\n", 0x123456789ABCULL);
    printf("%llx\n", 0x123456789ABCULL);
    printf("%d %u %x\n", 42, 42U, 42U);
    return 42;
}
