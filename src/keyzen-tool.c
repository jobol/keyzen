#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

int main(int argc, char **argv)
{
	argv++;
	if (*argv && !strcmp(*argv,"add")) {
		while (*++argv)
			mknod(*argv,S_IFREG|0644,0);
	} else if (*argv && !strcmp(*argv,"drop")) {
		while (*++argv)
			unlink(*argv);
	} else if (*argv && !strcmp(*argv,"ask")) {
		while (*++argv)
			printf("%s: %s\n",*argv,access(*argv,F_OK) ? "DENIED" : "ALLOWED");
	} else if (*argv && !strcmp(*argv,"server")) {
		int f = open(*++argv, O_RDWR|O_NONBLOCK);
		if (f < 0)
			printf("can't open %s\n",*argv);
		else {
			char buffer[2048];
			int n;
			for (;;) {
				n = read(f, buffer, sizeof buffer);
				if (n <= 0) {
					printf("error while reading dial %d %m\n",n);
					break;
				}
				printf("received %d bytes: [%.*s]\n",n,n,buffer);
				if (!fgets(buffer, sizeof buffer, stdin)) {
					printf("error while reading stdin %m\n");
					break;
				}
				n = write(f, buffer, strlen(buffer));
				if (n != strlen(buffer)) {
					printf("error while writing dial %d/%d %m\n",n,(int)strlen(buffer));
					break;
				}
			}
		}
	} else {
		printf("usage: %s add|drop|ask|server files...\n",*--argv);
	}
	return 0;
}
