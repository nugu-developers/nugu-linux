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

#ifndef __HTTP2_MULTIPART_PARSER_H__
#define __HTTP2_MULTIPART_PARSER_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _multipart_parser MultipartParser;

typedef void (*ParserCallback)(MultipartParser *parser, const char *data,
			       size_t length, void *userdata);

MultipartParser *multipart_parser_new();
void multipart_parser_free(MultipartParser *parser);

void multipart_parser_set_boundary(MultipartParser *parser, const char *src,
				   size_t length);
int multipart_parser_parse(MultipartParser *parser, const char *src,
			   size_t length, ParserCallback onFoundHeader,
			   ParserCallback onFoundBody, void *userdata);
void multipart_parser_reset(MultipartParser *parser);

#ifdef __cplusplus
}
#endif

#endif
