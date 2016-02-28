#include <unistd.h>

int main() {
    const int n = 1024;

    char buf[n];

    int i = 0;
    while ((i = read(STDIN_FILENO, buf, n)) > 0) {
        write(STDOUT_FILENO, buf, i);
    }
    return 0;
}
