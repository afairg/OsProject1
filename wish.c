#include <stdio.h>
#include <stdlib.h>


int main() {
    char* line = NULL;  // user input
    size_t len = 0;     // initial buffer size


    // interactive mode loop
    while (1) {
        printf("wish> ");
        __ssize_t read = getline(&line, &len, stdin);   // read user input
    }

    return 0;
}