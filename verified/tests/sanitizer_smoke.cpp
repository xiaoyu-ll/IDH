#include <cstdio>
int main() {
    std::fputs("SANITIZER_MAIN_ENTERED\n", stderr);
    return 0;
}
