#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

const char *typename(char t)
{
	switch(t) {
	case '!': return "blanket";
	case '+': return "session";
	case '*': return "one-shot";
	case '=': return "permit";
	case '-': return "deny";
	}
	return 0;
}

void server(const char *dial)
{
	int f = open(dial, O_RDWR|O_NONBLOCK);
	if (f < 0)
		printf("can't open %s\n",dial);
	else {
		char buffer[2048], name[2048], type, ans, *pstr;
		int n, pid, seq, w;
		printf("keyzen authorization server started\n");
		for (;;) {
			printf("waiting..."); fflush(stdout);
			n = read(f, buffer, sizeof buffer - 1);
			printf("\n");
			if (n <= 0) {
				printf("error while reading dial %d %m\n",n);
				break;
			}
			buffer[n] = 0;
			n = sscanf(buffer, "%d %d %c%s", &seq, &pid, &type, name);
			if (n != 4) {
				printf("error while scanning dial %d field(s) read\n",n);
				break;
			}
			printf("For pid=%d, key %s of type %c: %s\n",pid,name,type,typename(type));
			do {
				printf(" .. do you grant (y/n)? ");
				errno = 0;
				pstr = fgets(buffer, sizeof buffer, stdin);
			} while (pstr && buffer[0]!='y' && buffer[0]!='n');
			if (!pstr) {
				if (errno)
					printf("error while reading stdin %m\n");
				else
					printf("disconnecting\n");
				break;
			}
			ans = buffer[0];
			if (ans != 'y' && ans != 'n') {
				printf("internal error ans=%c\n",ans);
				break;
			}
			if (type != '*')
				type = ans == 'y' ? '=' : '-';
			n = sprintf(buffer, "%d %c%c\n", seq, ans, type);
			w = (int)write(f, buffer, n);
			if (n != w) {
				printf("error while writing dial %d/%d %m\n",n,w);
				break;
			}
		}
		printf("keyzen authorization server stopped\n");
	}
}

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
		server(*++argv);
	} else {
		printf("usage: %s add|drop|ask|server files...\n",*--argv);
	}
	return 0;
}
