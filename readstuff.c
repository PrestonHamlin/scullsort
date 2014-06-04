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
    char buf[10];

// testing scullsort
    if ((fd = open ("/dev/scullsort", O_RDWR)) == -1) {
        perror("readstuff opening file");
        return -1;
    }

    result = read (fd, &buf, sizeof(buf));
    buf[result] = '\0';
    fprintf (stdout, "\nreadstuff: \"%s\"\n", buf);
    close(fd);

    return 0;
}
