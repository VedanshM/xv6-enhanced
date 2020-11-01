typedef unsigned int uint;
#include "stat.h"
#include "types.h"
#include "user.h"
#include "pinfo.h"

// just for creating multiple child procs
// and then seeing scheduler behaviour
int main() {
    struct pinfo *p = malloc(100*sizeof(struct pinfo));
    get_pinfos(p);
	exit();
}
