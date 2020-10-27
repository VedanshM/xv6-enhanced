#include "types.h"
#include "user.h"
#include "stat.h"

int main(int argc, char **argv) {
	if (argc <= 1) {
		printf(2, "time: error: no arguments provided!!.\n");
		exit();
	}

	int pid = fork();
	if (pid < 0) {
		printf(2, "time: error: can't run program.\n");
	} else if (pid == 0) {
		exec(argv[1], argv + 1);
		printf(2, "time: error: couldn't run. \n");
	} else {
		int wtime, rtime;
		waitx(&wtime, &rtime);
		printf(1, "\nTime Taken: wtime= %d ;rtime= %d \n", wtime, rtime);
	}
	exit();
}
