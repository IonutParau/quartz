#include "quartz.h"
#include <stdio.h>

int main() {
	qrtz_Context ctx;
	qrtz_initContext(&ctx);

	qrtz_VM *vm = qrtz_create(&ctx);

	// do shi
	printf("VM: %p\n", vm);

	qrtz_destroy(vm);
	return 0;
}
