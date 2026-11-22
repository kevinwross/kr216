int constructor_ran;

int main(void) {
    return constructor_ran == 1 ? 42 : 1;
}
