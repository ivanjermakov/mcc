// 0
void* stderr;
int fprintf(void* stream, char* format, ...);

int main() {
    fprintf(stderr, "ok\n");
    return 0;
}
