#define _GNU_SOURCE

#include <stdio.h>
/*
#include <numa.h>
#include <uv/gru/gru.h>
#include <uv/gru/grulib.h>
#include <uv/gru/gru_alloc.h>
#include <uv/gru/gru_instructions.h>
#include <errno.h> 
*/

// Bcopy() is the GRU-supported equivalent to memcpy().
// void gru_bcopy(gru_control_block_t *cb, const gru_addr_t src,
// gru_addr_t dest, unsigned int tri0, unsigned int xtype,
// unsigned long nelem, unsigned int buffersize, unsigned long hints);

int main() {

	// define size of data to be copied
	const size_t dataSize = (size_t) 4 * 1024 * 1024;

	// prepare data structures necessary for GRU access

	gru_cookie_t cookie;
	gru_segment_t *gseg;
	gru_control_block_t *cb;
	unsigned long *dsr;
	int cbrs = 8, maxthreads = 8, dsrbytes = 16 * 1024;

	// create gru context
	// dsrbytes = number of active data segment bytes
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

	// prepare the memory blocks copied from and copied to
	char* src;
	char* dest;
	src = numa_alloc_onnode(dataSize, 0);
	dest = numa_alloc_onnode(dataSize, 0);

	// fill the memory blocks
	memset(src, 1, dataSize);
	memset(dest, 0, dataSize);

	// use bcopy to copy memory block src into dest
	// XType and flag have almost no influence on performance
	// Buffer size (second to last parameter) has biggest influence on performance,
	// but is in turn depending on the size of the parameter dsrbytes of the GRU context
	// datasize is divided by 8 because the used XType is 8 bytes long
	gru_bcopy(cb, src, dest, gru_get_tri(dsr), XTYPE_DW, dataSize / 8, 128, 1);

	// wait until the operation is done
	gru_wait_abort(cb);

	// free gru contexts resources
	gru_destroy_context(cookie);

	// free allocated memory 
	numa_free(src, dataSize);
	numa_free(dest, dataSize);

	return 0;
}
