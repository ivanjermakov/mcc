#include "stdio.h"

char input_buf[1 << 10];

int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "missing file argument");
        return 1;
    }
    char* source_path = argv[1];
    FILE* source_file = fopen(source_path, "r");
    size_t s = fread(&input_buf, 1, sizeof input_buf, source_file);
    fwrite(input_buf, 1, s, stdout);

    return 0;
}
