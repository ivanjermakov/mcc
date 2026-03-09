// 0
long* stderr;

void abort();
int fprintf(long* f, char* format, ...);

void assert(int expr) {
    if (!expr) {
        fprintf(stderr, "assertion failed\n");
        abort();
    }
}

int main() {
    assert(0 == 0);
    assert(1 == 1);
    assert(2 + 2 == 4);
    assert(2 - 2 == 0);
    return 0;
}
