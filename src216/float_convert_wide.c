int main(void) {
    long value = 70000;
    double d = (double)value;
    if ((int)(d / 10000) != 7) return 1;
    if ((long)d != 70000) return 2;
    long negative = 0 - 70000;
    double nd = (double)negative;
    if ((long)nd != negative) return 3;
    long long big = 0x100000000LL;
    double bd = (double)big;
    if (bd != 4294967296.0) return 4;
    if ((long long)bd != big) return 5;
    long long nbig = 0 - 0x100000000LL;
    double nbd = (double)nbig;
    if ((long long)nbd != nbig) return 6;
    return 42;
}
