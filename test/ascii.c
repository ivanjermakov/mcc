// 94
void puts(char* s);

int main() {
    char a[94];
    int i = 0;
    while (i < 94) {
        a[i] = i + '!';
        i++;
    }
    puts(a);
    return i;
}
