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

#ifndef __HTTP2_V1_EVENT_ATTACHMENT_H__
#define __HTTP2_V1_EVENT_ATTACHMENT_H__

#include "http2_network.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _v1_event_attachment V1EventAttachment;

V1EventAttachment *v1_event_attachment_new(const char *host);
void v1_event_attachment_free(V1EventAttachment *attach);

void v1_event_attachment_set_query(V1EventAttachment *attach,
				   const char *name_space, const char *name,
				   const char *version,
				   const char *parent_msg_id,
				   const char *msg_id, const char *dialog_id,
				   int seq, int is_end);

int v1_event_attachment_set_data(V1EventAttachment *attach,
				 const unsigned char *data, size_t length);

int v1_event_attachment_send_with_free(V1EventAttachment *attach,
				       HTTP2Network *net);

#ifdef __cplusplus
}
#endif

#endif
