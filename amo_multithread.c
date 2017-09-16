#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <uv/gru/gru.h>
#include <uv/gru/grulib.h>
#include <uv/gru/gru_alloc.h>
#include <uv/gru/gru_instructions.h>
#include <errno.h> 
#include <unistd.h>

/*
A number of threads are started, each uses the GAMIR operation to 
increment a shared variable by one.
*/ 

struct thread_args { 
	pthread_t thread_id;
	int thread_num;
	long* var;
	gru_cookie_t cookie;
};

void *copy(void *arg)
{	
	// making the args available for use
	struct thread_args *targs = arg;

	// prepare the GRU control block that will execute the operation
	gru_segment_t *gseg;
	gru_control_block_t *cb;
	unsigned long *dsr;

	// get the handle for the gru context
	gseg = gru_get_thread_gru_segment(targs->cookie, targs->thread_num);
	if (gseg == NULL) {
	    printf("There was an error while getting the handle for the GRU context (gseg).\n");
	    printf("Error code %i: %s \n", errno, strerror(errno));
	}

	// get a pointer to a control block 
	// the modulo operation is because there are 16 cbs max in each gru context,
	// but a cb can handle more than one thread at once
	cb = gru_get_cb_pointer(gseg, targs->thread_num % 16);

	// get the pointer to the a cacheline of the data segment 
	// (DSR) of the gru context
	dsr = gru_get_data_pointer(gseg, targs->thread_num);

	// the atomic memory operation
	gru_gamir(cb, EOP_IR_INC, targs->var, XTYPE_DW, 0);

	// wait for the operation to finish
	gru_wait_abort(cb);

	// the function must return something - NULL will do
	return NULL;

}

int main()
{	

	// config
	int num_threads = 50;

	struct thread_args *tinfo;

	// the variable the threads will compete for
	long* var __attribute__ ((aligned (64))) = malloc(sizeof(long));
	*var = 0;

	// prepare the GRU
	gru_cookie_t cookie;
	int cbrs = num_threads, maxthreads = num_threads, dsrbytes = 8 * 1024;

	// create the main gru context
	if (gru_create_context(&cookie, NULL, cbrs, dsrbytes, num_threads, GRU_OPT_MISS_DEFAULT) == -1) {
	    printf("There was an error while creating the GRU context.\n");
	    printf("Error code %i: %s \n", errno, strerror(errno));
	}

	// allocate memory for the subthreads
	tinfo = calloc(num_threads, sizeof(struct thread_args));
	if (tinfo == NULL)
		printf("There was an error handling calloc. Error code %i: %s \n", errno, strerror(errno));

	// prepare thread info and create threads
	int tnum;
	for (tnum = 0; tnum < num_threads; tnum++) {
		tinfo[tnum].thread_num = tnum;

		// giving each the thread the location of the variable they'll be competing for
		tinfo[tnum].var = (long *) var;
		tinfo[tnum].cookie = cookie;

		// The pthread_create() call stores the thread ID into
		// corresponding element of tinfo[]
		if(pthread_create(&tinfo[tnum].thread_id, NULL, copy, &tinfo[tnum])) {
			fprintf(stderr, "Error creating thread.\n");
		}
	}

	// Now join with each thread
	for (tnum = 0; tnum < num_threads; tnum++) {
		if(pthread_join(tinfo[tnum].thread_id, NULL)) {
			fprintf(stderr, "Error joining thread.\n");
		}   
	}

	printf("Final value of the incremented variable: %ld\n", *var);

	// free gru contexts resources
	gru_destroy_context(cookie);
	free((void *) var);


	return 0;

}
