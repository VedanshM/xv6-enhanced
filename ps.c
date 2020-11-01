#include "types.h"
#include "pinfo.h"
#include "user.h"

int main() {
	set_priority(10, getpid());
	struct pinfo *arr = malloc(100*sizeof(struct pinfo));
	int n = get_pinfos(arr);
	printf(1, "PID\tPriority\tState\t\tr_time\tw_time\tn_run\tcur_q\tq0\tq1\tq2\tq3\tq4\n");
	for (int i = 0; i < n; i++) {
		printf(1, "%d\t%d\t\t%s\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\n",
			   arr[i].pid,
			   arr[i].priority,
			   arr[i].state,
			   arr[i].rtime,
			   arr[i].wtime,
			   arr[i].n_run,
			   arr[i].cur_q,
			   arr[i].ticks[0],
			   arr[i].ticks[1],
			   arr[i].ticks[2],
			   arr[i].ticks[3],
			   arr[i].ticks[4]);
	}
	exit();
}
