#define _GNU_SOURCE

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <uv/gru/gru.h>
#include <uv/gru/grulib.h>
#include <uv/gru/gru_alloc.h>
#include <uv/gru/gru_instructions.h>
#include <errno.h> 

// every subthread only has its own gru mapping/gseg, not an own context

struct thread_args { 
	pthread_t thread_id;
	int thread_num;
	char* src;
	char* dest;
	size_t size;
	gru_cookie_t cookie;
	int bufSize;
};

void *copy(void *arg)
{	
	// making the args available for use
	struct thread_args *targs = arg;

	// setting up the GRU control block that will execute the operation
	gru_segment_t *gseg;
	gru_control_block_t *cb;
	unsigned long *dsr;

	// get the handle for the gru context
	gseg = gru_get_thread_gru_segment(targs->cookie, targs->thread_num);
	if (gseg == NULL) {
	    printf("There was an error while getting the handle for the GRU context (gseg).\n");
	    printf("Error code %i: %s \n", errno, strerror(errno));
	}

	// get the pointer to control block x of the gru context
	cb = gru_get_cb_pointer(gseg, targs->thread_num);

	// get the pointer to the first cacheline of the data segment 
	// (DSR) assigned to the control block
	// number of control block multiplied with size of buffer
	dsr = gru_get_data_pointer(gseg, targs->thread_num * targs->bufSize);

	gru_bcopy(cb, targs->src, targs->dest, gru_get_tri(dsr), XTYPE_DW, targs->size / 8, targs->bufSize, IMA_CB_DELAY);

	// wait for the operation to finish
	// for asynchronous copying comment out next line
	gru_wait_abort(cb);

	// the function must return something - NULL will do
	return NULL;

}

int main()
{	
	// config 

	size_t dataSize = (size_t) 2 * 1024 * 1024 * 1024;
	int bufSize = 64;

	// number of threads must be a power of two
	// or else funny things could happen when 
	// dividing the memory block to be copied
	int num_threads = 4;

	struct thread_args *tinfo;

	// allocating memory 
	char* src;
	char* dest;	
	src = malloc(dataSize);
	dest = malloc(dataSize);

	// filling memory blocks
	memset(src, 1, dataSize);
	memset(dest, 0, dataSize);

	// setting up the GRU context
	gru_cookie_t cookie;
	int cbrs = 8, maxthreads = 8;

	// dsrbytes must be at least bufsize * num_threads * 64;
	// because bufsize is in cache lines and the size of one cache line is 64 bytes 
	int dsrbytes = 16 * 1024;

	// create gru context
	if (gru_create_context(&cookie, NULL, cbrs, dsrbytes, maxthreads, GRU_OPT_MISS_DEFAULT) == -1) {
	    printf("There was an error while creating the GRU context.\n");
	    printf("Error code %i: %s \n", errno, strerror(errno));
	}

	// allocate memory for the threads
	tinfo = calloc(num_threads, sizeof(struct thread_args));
	if (tinfo == NULL)
		printf("There was an error handling calloc. Error code %i: %s \n", errno, strerror(errno));

	// prepare thread info and create threads
	int tnum;
	for (tnum = 0; tnum < num_threads; tnum++) {
		tinfo[tnum].thread_num = tnum;

		// assigning each the thread the slice it has to copy
		tinfo[tnum].src = (char *) src + tnum * dataSize / num_threads;
		tinfo[tnum].dest =  (char *) dest + tnum * dataSize / num_threads;
		tinfo[tnum].size = dataSize / num_threads;
		tinfo[tnum].cookie = cookie;
		tinfo[tnum].bufSize = bufSize;

		// The pthread_create() call stores the thread ID into
		// corresponding element of tinfo[]

		if(pthread_create(&tinfo[tnum].thread_id, NULL, copy, &tinfo[tnum])) {
			fprintf(stderr, "Error creating thread.\n");
		}
	}

	// Now join with each thread
	// TODO: return a real return value, not just NULL
	for (tnum = 0; tnum < num_threads; tnum++) {
		if(pthread_join(tinfo[tnum].thread_id, NULL)) {
			fprintf(stderr, "Error joining thread.\n");
		}     
	}

	// checking whether the copying was done right
	int res = memcmp(src, dest, dataSize);
	if (res) {
		printf ("Memcmp() Error, result is: %i \n", res);
	}

	// free allocated memory
	free(src);
	free(dest);

	// free gru context resources
	gru_destroy_context(cookie);

	return 0;

}

