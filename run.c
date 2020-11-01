#include "types.h"
#include "user.h"

char *arg[] = {"task", "0", 0};

int main() {
	int no_of_proc = 6; //keep less than 9
    // int a[9] = {4,1,6,5,9,2,7,3,8};
	for (int i = 0; i < no_of_proc; i++) {
		arg[1][0] = '0' + i;
		if (fork() == 0) {
			exec(arg[0], arg);
			printf(1, "error");
		}
	}
	int totw = 0, totr = 0;
	for (int i = 0; i < no_of_proc + 15; i++) {
		int w, r;
		int pid = waitx(&w, &r);
		if (pid <= 0)
			continue;
		totw += w;
		totr += r;
		printf(1, "Pid: %d wtime: %d rtime: %d\n", pid, w, r);
	}
	printf(1, "\n\ntotal wtime: %d rtime: %d tot_time:%d\n", totw, totr, totw+totr);
	exit();
}
