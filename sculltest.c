/*
 * Simple userspace program to test basic functionality of the scull devices.
 */

#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>

int main() {
    int fd, result;
    char buf[10];

    if ((fd = open ("/dev/scull", O_WRONLY)) == -1) {
        perror("opening file");
        return -1;
    }

    if ((result = write (fd, "abcdef", 6)) != 6) {
        perror("writing");
        return -1;
    }
    close(fd);

    if ((fd = open ("/dev/scull", O_RDONLY)) == -1) {
        perror("opening file");
        return -1;
    }

    result = read (fd, &buf, sizeof(buf));
    buf[result] = '\0';
    fprintf (stdout, "read back \"%s\"\n", buf);
    close(fd);


// testing scullpipe

    if ((fd = open ("/dev/scullpipe", O_RDWR)) == -1) {
        perror("opening file");
        return -1;
    }

    if ((result = write (fd, "xyz xxx yyy zzz", 15)) != 15) {
        perror("writing");
        return -1;
    }

    result = read (fd, &buf, sizeof(buf));
    buf[result] = '\0';
    fprintf (stdout, "first: \"%s\"\n", buf);
    result = read (fd, &buf, sizeof(buf));
    buf[result] = '\0';
    fprintf (stdout, "second: \"%s\"\n", buf);
    close(fd);

// testing scullsort
    if ((fd = open ("/dev/scullsort", O_RDWR)) == -1) {
        perror("opening file");
        return -1;
    }

    if ((result = write (fd, "sort me", 7)) != 7) {
        perror("writing");
        return -1;
    }

    result = read (fd, &buf, sizeof(buf));
    buf[result] = '\0';
    fprintf (stdout, "read back \"%s\"\n", buf);
    close(fd);




    return 0;
}
