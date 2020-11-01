#define QCNT 5
#define NPROC 64

struct pinfo {
	uint pid;
	uint priority;
	char state[120];
	uint rtime;
	uint wtime;
	uint n_run;
	uint cur_q;
	uint ticks[QCNT];
};
