// 101
void inc(int* i) {
    (*i)++;
}
int main() {
    int i = 100;
    inc(&i);
    return i;
}
