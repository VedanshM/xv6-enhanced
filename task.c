#include "types.h"
#include "user.h"

int main(int argc, char **argv) {
	int x = atoi(argv[1]);
	printf(1, "[[%d, %d]]\n", x, getpid());
	if (x % 2 == 0) {
		if (x == 0)
			sleep(15e2);
		else
			sleep(5e2);
		// for (volatile int i = 0; i < 1e8; i++)
		// 	i++, i--;
	} else {
		for (volatile int i = 0; i < 1e8; i++)
			i++, i--, i++, i--;
		if (x == 1)
			for (volatile int i = 0; i < 2e8; i++)
				i++, i--, i++, i--;
		// sleep(1e3);
	}

	// int stime = 2e5;
	// int rnval = 1 << (10 - i);
	// int prior = 50 - i;
	// set_priority(prior, getpid());
	// int t = 1 << i;
	// for (int i = 0; i < t; i++)
	// 	sleep(stime);
	// for (volatile int i = 0; i < rnval; i++) {
	// 	i++;
	// 	i--;
	// }
	exit();
}
