#include "threadpool.h"
#include <stdlib.h>
#include <stdio.h>
void error(char* err);
void destroyAndNull (void* x);
void destroy_error(char* err, threadpool* pool);
void insert_work(threadpool* pool, work_t* work);
work_t* dequeue(threadpool* pool);
/*Create TreadPool- create_threadpool creates a fixed-sized thread */
threadpool* create_threadpool(int num_threads_in_pool){
	if (num_threads_in_pool<1 || num_threads_in_pool> MAXT_IN_POOL)
		return NULL;
	threadpool* pool = (threadpool*) malloc(sizeof(threadpool));
	if (pool == NULL)
		error("malloc\n");
	pool-> num_threads = num_threads_in_pool;
	pool-> qsize = 0;
	pool-> threads = (pthread_t*)malloc(num_threads_in_pool*sizeof(pthread_t));
	if (pool-> threads == NULL){
		destroyAndNull(pool);
		error("malloc\n");
	}
	pool-> qhead = pool-> qtail = NULL;
	if ((pthread_mutex_init(&(pool->qlock), NULL)) != 0 )
		destroy_error("pthread_mutex_init\n", pool);
	if ((pthread_cond_init(&(pool->q_not_empty), NULL)) != 0 )
		destroy_error("pthread_cond_init\n", pool);
	if ((pthread_cond_init(&(pool->q_empty), NULL)) != 0 )
		destroy_error("pthread_cond_init\n", pool);
	pool->shutdown = 0;
	pool->dont_accept = 0;
	int i;
	for (i = 0; i < num_threads_in_pool; i++)
		if ((pthread_create(pool-> threads+i, NULL, do_work, pool )) != 0 )
			destroy_error("pthread_create\n", pool);
	return pool;
}
/* Dispatch- dispatch enter a "job" of type work_t into the queue */
void dispatch(threadpool* from_me, dispatch_fn dispatch_to_here, void *arg){
	if (from_me == NULL|| dispatch_to_here == NULL)
		return;
	if (from_me-> dont_accept == 1)
		return;
	work_t* work = (work_t*) malloc(sizeof(work_t)); // create work_t
	if (work == NULL)
		destroy_error("malloc\n",from_me);
	work->arg = arg;
	work->routine = dispatch_to_here;
	if ((pthread_mutex_lock(&from_me->qlock)) != 0)//lock
		destroy_error("pthread_mutex_lock\n",from_me);
	insert_work(from_me, work);
	if ((pthread_cond_signal(&from_me->q_not_empty)) !=0 ) // queue is not empty
		destroy_error("pthread_cond_signal\n",from_me);
	if ((pthread_mutex_unlock(&from_me->qlock)) != 0)
		destroy_error("pthread_mutex_unlock\n",from_me);
}
/* Do Work - The work function of the thread */
void* do_work(void* p){
	if (p == NULL)
		return NULL;
	threadpool* pool = (threadpool*)p;
	work_t* work;
	while(1){
		if ((pthread_mutex_lock(&pool->qlock)) != 0) //lock
			destroy_error("pthread_mutex_lock\n",pool);
		if (pool->qsize == 0 && pool->dont_accept == 1)
			if ((pthread_cond_signal(&pool->q_empty)) != 0 )
				destroy_error("pthread_cond_signal\n",pool);
		if (pool->shutdown == 1){
		    if ((pthread_mutex_unlock(&pool->qlock)) != 0 ) // unlock	
			destroy_error("pthread_mutex_unlock\n",pool);
		    return NULL;
		}
		if (pool->qsize == 0)// q_empty
			if ((pthread_cond_wait(&(pool->q_not_empty), &(pool->qlock))) != 0)
				destroy_error("pthread_cond_wait\n",pool);
		if (pool->shutdown == 1){
		    if ((pthread_mutex_unlock(&pool->qlock)) != 0)// unlock
			destroy_error("pthread_mutex_unlock\n",pool);
		    return NULL;
		}
		work = pool->qhead;
		if (work != NULL){
			work = dequeue(pool);
			if ((pthread_mutex_unlock(&pool->qlock)) != 0 )// unlock
				destroy_error("pthread_mutex_unlock\n",pool);
			work->routine(work->arg);
			destroyAndNull(work);
		}
		else if ((pthread_mutex_unlock(&pool->qlock)) != 0) // unlock
			destroy_error("pthread_mutex_unlock\n",pool);
	}
}
/* Destroy ThreadPool - kills the threadpool */
void destroy_threadpool(threadpool* destroyme){
	if (destroyme == NULL)
		return;
	int i;
	destroyme->dont_accept = 1;
	if ((pthread_mutex_lock(&destroyme->qlock)) != 0) //lock
			destroy_error("pthread_mutex_lock\n",destroyme);
	if (destroyme->qsize != 0)////// q_not_empty
		if ((pthread_cond_wait(&destroyme->q_empty, &destroyme->qlock)) != 0 )
			destroy_error("pthread_cond_wait\n",destroyme);		
	destroyme-> shutdown = 1;
	if ((pthread_cond_broadcast(&destroyme->q_not_empty))!= 0 )
		destroy_error("pthread_cond_broadcast\n",destroyme);		
	if ((pthread_mutex_unlock(&destroyme->qlock)) != 0) //unlock
			destroy_error("pthread_mutex_unlock\n",destroyme);
	for (i=0; i<destroyme->num_threads; i++)
		if ((pthread_join(destroyme->threads[i], NULL)) !=0 )
			destroy_error("pthread_join\n", destroyme);
	pthread_cond_destroy(&destroyme->q_not_empty);
	pthread_cond_destroy(&destroyme->q_empty);
	pthread_mutex_destroy(&destroyme->qlock);
	destroyAndNull(destroyme->threads);
	destroyAndNull(destroyme);
}
/* Dequeue - dequeue from job queue */
work_t* dequeue(threadpool* pool){
	if (pool->qhead == NULL)
		return NULL;
	work_t* first = pool->qhead;
	pool->qhead = pool->qhead->next;
	pool->qsize--;
	if (pool->qhead == NULL)
		pool->qtail = pool->qhead;
	return first;
}

/*  Insert Work- insert a job to queue  */
void insert_work(threadpool* pool, work_t* work){
	work-> next = NULL;
	if (pool->qsize == 0)
		pool->qhead = work;
	else 
		pool->qtail->next = work;
	pool->qtail = work;
	pool->qsize++;
}
/*  Destroy Error - clean memory of threadpool and send error message */
void destroy_error(char* err, threadpool* pool){
	if (pool != NULL){
		pthread_cond_destroy(&pool->q_not_empty);
		pthread_cond_destroy(&pool->q_empty);
		pthread_mutex_destroy(&pool->qlock);
		if (pool->threads!= NULL)
			destroyAndNull(pool->threads);
		destroyAndNull(pool);
	}
	error(err);
}
/* Error- show an error meesage and exit(EXIT_FAILURE) */
void error(char* err){
	perror(err);
	exit(EXIT_FAILURE);
}
/* Destroy And Null- Destroys and set as Null */
void destroyAndNull (void* x){
	free(x);
	x = NULL;
}

