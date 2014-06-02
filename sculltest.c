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

	if ((fd = open ("/dev/scullpipe", O_RDWR)) == -1) {
		perror("opening file");
		return -1;
	}

	if ((result = write (fd, "xyz", 3)) != 3) {
		perror("writing");
		return -1;
	}

	result = read (fd, &buf, sizeof(buf));
	buf[result] = '\0';
	fprintf (stdout, "read back \"%s\"\n", buf);
	close(fd);

	return 0;
}
