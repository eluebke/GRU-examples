#define _GNU_SOURCE

#include <stdio.h>
#include <numa.h>
#include <uv/gru/gru.h>
#include <uv/gru/grulib.h>
#include <uv/gru/gru_alloc.h>
#include <uv/gru/gru_instructions.h>
#include <errno.h>

// The GRU provides a set of atomic memory operations.
// In this example one of them is used to atomically increment a variable.
// The function used is gru_gamir().
// void gru_gamir(gru_control_block_t *cb, int exopc, gru_addr_t addr,
// 		unsigned int xtype, unsigned long hints);
// See man page for further information.

int main() {

	// the variable that will be incremented atomically 
	long* var __attribute__ ((aligned (64))) = numa_alloc_onnode(sizeof(long), 0);
	*var = 0;

	// preparing the data structures necessary for using the GRU
	gru_cookie_t cookie;
	gru_segment_t *gseg;
	gru_control_block_t *cb;
	unsigned long *dsr;
	int cbrs = 8, maxthreads = 16, dsrbytes = 2 * 1024;

	// create gru context
	if (gru_create_context(&cookie, NULL, cbrs, dsrbytes, maxthreads, GRU_OPT_MISS_DEFAULT) == -1) {
		printf("There was an error while creating the GRU context.\n");
		printf("Error code %i: %s \n", errno, strerror(errno));
	}

	// get the handle for the gru context 0
	gseg = gru_get_thread_gru_segment(cookie, 0);
	if (gseg == NULL) {
		printf("There was an error while getting the handle for the GRU context (gseg).\n");
		printf("Error code %i: %s \n", errno, strerror(errno));
	}

	// get the pointer to control block 0 of the gru context
	cb = gru_get_cb_pointer(gseg, 0);

	// get the pointer to the first cacheline of the data segment 
	// (DSR) of the gru context
	dsr = gru_get_data_pointer(gseg, 0);

	// Atomically increment var 
	gru_gamir(cb, EOP_IR_INC, var, XTYPE_DW, 0);

	// wait for operation to complete
	gru_wait_abort(cb);

	// before the operation the value of the variable is stored into the
	// avalue field of the GRU control block
	// this function reads the avalue field and returns its content
	// i.e. this is the value the variable had before the increment operation
	long avalue = gru_get_amo_value(cb);
	printf("Value: %ld\n", avalue);

	// free gru contexts resources
	gru_destroy_context(cookie);

	// free allocated memory for var
	numa_free(var, sizeof(long));

}

