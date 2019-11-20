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

#ifndef __HTTP2_MANAGE_H__
#define __HTTP2_MANAGE_H__

#ifdef __cplusplus
extern "C" {
#endif

enum h2manager_status {
	H2_STATUS_ERROR,
	H2_STATUS_READY,
	H2_STATUS_CONNECTING,
	H2_STATUS_DISCONNECTED,
	H2_STATUS_CONNECTED,
	H2_STATUS_TOKEN_FAILED
};

struct equeue_data_attachment {
	void *data;
	size_t length;
	char *parent_msg_id;
	char *media_type;
	int is_end;
};

typedef void (*H2ManagerStatusCallback)(void *userdata);

typedef struct _h2_manager H2Manager;

H2Manager *h2manager_new(void);
void h2manager_free(H2Manager *manager);

const char *h2manager_peek_host(H2Manager *manager);

int h2manager_set_status_callback(H2Manager *manager,
				  H2ManagerStatusCallback callback,
				  void *userdata);
int h2manager_set_status(H2Manager *manager, enum h2manager_status status);
enum h2manager_status h2manager_get_status(H2Manager *manager);

int h2manager_send_event(H2Manager *manager, const char *name_space,
			 const char *name, const char *version,
			 const char *context, const char *msg_id,
			 const char *dialog_id, const char *referrer_id,
			 const char *json);

int h2manager_send_event_attachment(H2Manager *manager, const char *name_space,
				    const char *name, const char *version,
				    const char *parent_msg_id,
				    const char *msg_id, const char *dialog_id,
				    int seq, int is_end, size_t length,
				    unsigned char *data);

int h2manager_connect(H2Manager *manager);
int h2manager_disconnect(H2Manager *manager);

#ifdef __cplusplus
}
#endif

#endif
