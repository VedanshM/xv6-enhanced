#include "types.h"
#include "user.h"
#include "stat.h"

int toint(char *s) {
	int x = 0;
	for (int i = 0; s[i]; i++) {
		x = x * 10 + (s[i] - '0');
	}
	return x;
}

int main(int argc, char **argv) {
	if (argc != 3) {
		printf(1, "error: provide 2 arguments, priority and pid");
	} else {
		int pty = toint(argv[1]);
		int pid = toint(argv[2]);
		printf(1, "return code: %d", set_priority(pty, pid));
	}
}
