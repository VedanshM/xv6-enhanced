typedef unsigned int uint;
#include "stat.h"
#include "types.h"
#include "user.h"

// just for creating multiple child procs
// and then seeing scheduler behaviour
int main() {
	printf(1, "Parent pid: %d\n", getpid());
	set_priority(10, getpid());
	for (int i = 0; i < 5; i++) {
		int f = fork();

		if (f == 0) {
			set_priority(60 + i - 2 * i * (i % 2), getpid());
			printf(1, "child: %d\n", i);
			for (volatile long long j = 0; j < 2e8; j++) {
				j++;
				j--;
				if (j == 1e8)
					set_priority(80 + i - 2 * i * (i % 2), getpid());
			}
			printf(1, "child: %d\n", i);
			exit();
		}
	}
	for (int i = 0; i < 5; i++)
		wait();
	exit();
}
