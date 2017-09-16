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

// every thread gets its own gru segment aka gseg
// but there is only one context total

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

	gru_segment_t *gseg;
	gru_control_block_t *cb;
	unsigned long *dsr;

	// get the handle for the gru context
	gseg = gru_get_thread_gru_segment(targs->cookie, targs->thread_num);
	if (gseg == NULL) {
	    printf("There was an error while getting the handle for the GRU context (gseg).\n");
	    printf("Error code %i: %s \n", errno, strerror(errno));
	}

	// get the pointer to control block 0 of the gru context
	cb = gru_get_cb_pointer(gseg, targs->thread_num);

	// get the pointer to the first cacheline of the data segment 
	// (DSR) of the gru context
	dsr = gru_get_data_pointer(gseg, targs->thread_num);

	gru_gamir(cb, EOP_IR_INC, targs->var, XTYPE_DW, 0);
	gru_wait_abort(cb);

	// the function must return something - NULL will do
	return NULL;

}

int main()
{	
	struct thread_args *tinfo;

	// config
	int num_threads = 50;

	long* var __attribute__ ((aligned (64))) = malloc(sizeof(long));
	*var = 0;

	gru_cookie_t cookie;
	int cbrs = num_threads, maxthreads = num_threads, dsrbytes = 8 * 1024;

	// create the main gru context
	if (gru_create_context(&cookie, NULL, cbrs, dsrbytes, num_threads, GRU_OPT_MISS_DEFAULT) == -1) {
	    printf("There was an error while creating the GRU context.\n");
	    printf("Error code %i: %s \n", errno, strerror(errno));
	}

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

	// free gru contexts resources
	gru_destroy_context(cookie);
	free((void *) var);


	return 0;

}