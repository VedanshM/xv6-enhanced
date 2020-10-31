typedef unsigned int uint;
#include "stat.h"
#include "types.h"
#include "user.h"

// just for creating multiple child procs
// and then seeing scheduler behaviour
int main() {
    printf(1, " hio");
	sleep(100);
    printf(1, "bye");
	exit();
}
