#include "types.h"
#include "user.h"
#include "stat.h"

int main(int argc, char **argv) {
	if (argc != 3) {
		cprintf("error: provide 2 arguments, priority and pid");
	} else {
		cprintf("return code: %d", set_priority(argv[1], argv[2]));
	}
}
