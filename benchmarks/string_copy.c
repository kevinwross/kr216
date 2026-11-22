int main(void) {
    char source[32];
    char destination[32];
    int index = 0;
    while (index < 31) {
        source[index] = (char)('a' + index % 26);
        index++;
    }
    source[31] = 0;
    index = 0;
    while (source[index] != 0) {
        destination[index] = source[index];
        index++;
    }
    destination[index] = 0;
    return destination[0] == 'a' ? 42 : 1;
}
