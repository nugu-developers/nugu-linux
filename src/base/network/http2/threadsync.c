#include <pthread.h>
#include <stdlib.h>
#include <errno.h>

#include <glib.h>

#include "base/nugu_log.h"
#include "threadsync.h"

struct _thread_sync {
	pthread_mutex_t lock;
	pthread_cond_t cond;
	int flag;
};

ThreadSync *thread_sync_new(void)
{
	ThreadSync *s;

	s = malloc(sizeof(struct _thread_sync));
	if (!s) {
		nugu_error_nomem();
		return NULL;
	}

	s->flag = 0;
	pthread_mutex_init(&s->lock, NULL);
	pthread_cond_init(&s->cond, NULL);

	return s;
}

void thread_sync_free(ThreadSync *s)
{
	g_return_if_fail(s != NULL);

	free(s);
}

int thread_sync_check(ThreadSync *s)
{
	int flag;

	g_return_val_if_fail(s != NULL, -1);

	pthread_mutex_lock(&s->lock);
	flag = s->flag;
	pthread_mutex_unlock(&s->lock);

	return flag;
}

void thread_sync_wait(ThreadSync *s)
{
	g_return_if_fail(s != NULL);

	pthread_mutex_lock(&s->lock);
	if (s->flag == 0)
		pthread_cond_wait(&s->cond, &s->lock);
	else
		nugu_dbg("signal has already emitted");
	pthread_mutex_unlock(&s->lock);
}

enum thread_sync_result thread_sync_wait_secs(ThreadSync *s, unsigned int secs)
{
	struct timespec spec;
	int status = 0;
	gint64 microseconds;

	g_return_val_if_fail(s != NULL, THREAD_SYNC_RESULT_FAILURE);

	microseconds = g_get_real_time();
	spec.tv_nsec = (microseconds % 1000000) * 1000;
	spec.tv_sec = microseconds / 1000000 + secs;

	pthread_mutex_lock(&s->lock);
	if (s->flag == 0)
		status = pthread_cond_timedwait(&s->cond, &s->lock, &spec);
	else
		nugu_dbg("signal has already emitted");
	pthread_mutex_unlock(&s->lock);

	if (status == ETIMEDOUT)
		return THREAD_SYNC_RESULT_TIMEOUT;

	return THREAD_SYNC_RESULT_OK;
}

void thread_sync_signal(ThreadSync *s)
{
	g_return_if_fail(s != NULL);

	pthread_mutex_lock(&s->lock);
	s->flag = 1;
	pthread_cond_signal(&s->cond);
	pthread_mutex_unlock(&s->lock);
}
