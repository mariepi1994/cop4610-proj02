/*
 * encrypting.c
 *
 *	Adapted from the filetest.c program provided with this assignment.
 *	Reads data into a file then takes that file and encrypts it.
 *
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <err.h>

int
main(int argc, char *argv[])
{
	//Data to read in.
	static char writebuf[40] = "111111111111111111111111111111111111122";
	static char readbuf[41];

	const char *file;
	int fd, rv;

	if (argc == 0) {
		/*warnx("No arguments - running on \"testfile\"");*/
		file = "testfile";
	}
	else if (argc == 2) {
		file = argv[1];
	}
	else {
		errx(1, "Usage: filetest <filename>");
	}

	fd = open(file, O_WRONLY|O_CREAT|O_TRUNC, 0664);
	if (fd<0) {
		err(1, "%s: open for write", file);
	}

	rv = write(fd, writebuf, 40);
	if (rv<0) {
		err(1, "%s: write", file);
	}

	rv = close(fd);
	if (rv<0) {
		err(1, "%s: close (1st time)", file);
	}

	
	(void)readbuf;
	return encrypt(file, 40);

}
