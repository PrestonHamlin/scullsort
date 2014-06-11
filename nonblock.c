// Code by Preston Hamlin

// This code is structured in the same way as the sculltest test program so as
//  to provide some sense of continuity. This program demonstrates behavior for
//  when O_NONBLOCK is set during a write.

#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>

int main() {
    int fd, result;

// write to pipe that scullread will read from
    if ((fd = open ("/dev/scullsort", O_RDWR | O_NONBLOCK)) == -1) {
        perror("writestuff opening file");
        return -1;
    }

    if ((result = write (fd, "some text", 9)) != 9) {
        perror("writestuff writing");
        return -1;
    }

    close(fd);

    return 0;
}
