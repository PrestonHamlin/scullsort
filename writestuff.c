// Code by Preston Hamlin

// This code is structured in the same way as the sculltest test program so as
//  to provide some sense of continuity. This program and its companion are to
//  be used in the testing the persistance of data in the scullsort device.
//  That is, the data is to be persistant and remain in waiting for a read
//  until the data is removed (read) or the module is unloaded.

#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>

int main() {
    int fd, result;

// write to pipe that scullread will read from
    if ((fd = open ("/dev/scullsort", O_RDWR)) == -1) {
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
