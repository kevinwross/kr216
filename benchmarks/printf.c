#include <stdio.h>
int main(void) {
    int total = 0;
    int index = 1;
    while (index <= 20) {
        printf("%d %x\n", index, index);
        total += index;
        index++;
    }
    return total == 210 ? 42 : 1;
}
