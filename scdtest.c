typedef unsigned int uint;
#include "stat.h"
#include "types.h"
#include "user.h"

// just for creating multiple child procs
// and then seeing scheduler behaviour
int main() {
	printf(1, "Parent pid: %d\n", getpid());

	for (int i = 0; i < 5; i++) {
		int f = fork();

		if (f == 0) {
			printf(1, "child: %d\n", i);
			for (volatile long long j = 0; j < 2e8; j++) {
				j++;
				j--;
			}
			printf(1, "child: %d\n", i);
			exit();
		}
	}
	exit();
}
