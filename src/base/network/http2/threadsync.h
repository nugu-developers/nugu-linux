/*
 * Copyright (c) 2019 SK Telecom Co., Ltd. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __HTTP2_SYNC_H__
#define __HTTP2_SYNC_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _thread_sync ThreadSync;

enum thread_sync_result {
	THREAD_SYNC_RESULT_OK,
	THREAD_SYNC_RESULT_FAILURE,
	THREAD_SYNC_RESULT_TIMEOUT
};

ThreadSync *thread_sync_new(void);
void thread_sync_free(ThreadSync *s);

int thread_sync_check(ThreadSync *s);

void thread_sync_wait(ThreadSync *s);
enum thread_sync_result thread_sync_wait_secs(ThreadSync *s, unsigned int secs);

void thread_sync_signal(ThreadSync *s);

#ifdef __cplusplus
}
#endif

#endif
