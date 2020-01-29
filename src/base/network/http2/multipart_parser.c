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

#include <stdlib.h>
#include <string.h>

#include <glib.h>

#include "base/nugu_log.h"
#include "base/nugu_buffer.h"

#include "multipart_parser.h"

#define MARK_CR '\r'
#define MARK_LF '\n'
#define MARK_HYPHEN '-'

enum bodyparser_step {
	STEP_READY,
	STEP_START_HYPHEN,
	STEP_CHECK_BOUNDARY,
	STEP_END_BOUNDARY,
	STEP_END_CR,
	STEP_END_HYPHEN,
	STEP_END_HYPHEN2,
	STEP_END_HYPHEN2_CR,
	STEP_CONTENT_START,
	STEP_HEADER,
	STEP_HEADER_CR,
	STEP_CONTENT_EMPTY_CR,
	STEP_BODY,
	STEP_BODY_CR,
	STEP_BODY_ENDLINE,
	STEP_BODY_ENDLINE_CR,
	STEP_FINISH
};

struct _multipart_parser {
	NuguBuffer *header;
	NuguBuffer *body;
	char *boundary;
	size_t boundary_length;
	enum bodyparser_step step;
	void *data;
};

MultipartParser *multipart_parser_new()
{
	struct _multipart_parser *parser;

	parser = calloc(1, sizeof(struct _multipart_parser));
	if (!parser) {
		error_nomem();
		return NULL;
	}

	parser->header = nugu_buffer_new(0);
	parser->body = nugu_buffer_new(0);
	parser->step = STEP_READY;
	parser->data = NULL;

	return parser;
}

void multipart_parser_free(MultipartParser *parser)
{
	g_return_if_fail(parser != NULL);

	if (parser->boundary)
		free(parser->boundary);

	if (nugu_buffer_get_size(parser->header) > 0) {
		nugu_dbg("remain header size: %d",
			 nugu_buffer_get_size(parser->header));
		nugu_dbg("\n%s", (char *)nugu_buffer_peek(parser->header));
	}

	if (nugu_buffer_get_size(parser->body) > 0) {
		nugu_dbg("remain body size: %d",
			 nugu_buffer_get_size(parser->body));
		nugu_dbg("\n%s", (char *)nugu_buffer_peek(parser->body));
	}

	nugu_buffer_free(parser->header, 1);
	nugu_buffer_free(parser->body, 1);

	memset(parser, 0, sizeof(MultipartParser));
	free(parser);
}

void multipart_parser_set_data(MultipartParser *parser, void *data)
{
	g_return_if_fail(parser != NULL);

	parser->data = data;
}

void *multipart_parser_get_data(MultipartParser *parser)
{
	g_return_val_if_fail(parser != NULL, NULL);

	return parser->data;
}

void multipart_parser_set_boundary(MultipartParser *parser, const char *src,
				   size_t length)
{
	g_return_if_fail(parser != NULL);

	if (parser->boundary) {
		free(parser->boundary);
		parser->boundary = NULL;
	}

	if (!src || length == 0)
		return;

	parser->boundary_length = length;

	parser->boundary = (char *)malloc(parser->boundary_length + 1);
	memcpy(parser->boundary, src, parser->boundary_length);
	parser->boundary[parser->boundary_length] = '\0';

	nugu_dbg("multipart boundary: '%s'", parser->boundary);
}

int multipart_parser_parse(MultipartParser *parser, const char *src,
			   size_t length, ParserCallback onFoundHeader,
			   ParserCallback onFoundBody, void *userdata)
{
	const char *pos;
	const char *end;
	const char *b_pos;
	const char *b_end;

	g_return_val_if_fail(parser != NULL, -1);
	g_return_val_if_fail(src != NULL, -1);

	end = src + length;
	b_pos = parser->boundary;
	b_end = parser->boundary + parser->boundary_length - 1;

	for (pos = src; pos < end; pos++) {
		switch (parser->step) {
		case STEP_READY:
			/* prev: '' */
			if (*pos == MARK_HYPHEN)
				parser->step = STEP_START_HYPHEN;
			break;

		case STEP_START_HYPHEN:
			/* prev: '-' */
			if (*pos == MARK_HYPHEN) {
				parser->step = STEP_CHECK_BOUNDARY;
				b_pos = parser->boundary;
				break;
			}
			parser->step = STEP_READY;
			break;

		case STEP_CHECK_BOUNDARY:
			/* prev: '--' */
			if (*pos != *b_pos) {
				nugu_error("boundary mismatch !");
				parser->step = STEP_READY;
				break;
			}
			if (b_pos < b_end)
				b_pos++;
			else
				parser->step = STEP_END_BOUNDARY;
			break;

		case STEP_END_BOUNDARY:
			/* prev: '--boundary' */
			if (*pos == MARK_CR)
				parser->step = STEP_END_CR;
			else if (*pos == MARK_HYPHEN)
				parser->step = STEP_END_HYPHEN;
			break;

		case STEP_END_HYPHEN:
			/* prev: '--boundary-' */
			if (*pos == MARK_HYPHEN) {
				parser->step = STEP_END_HYPHEN2;
				break;
			}
			parser->step = STEP_READY;
			break;

		case STEP_END_HYPHEN2:
			/* prev: '--boundary--' */
			if (*pos == MARK_CR) {
				parser->step = STEP_END_HYPHEN2_CR;
				break;
			}
			parser->step = STEP_READY;
			break;

		case STEP_END_HYPHEN2_CR:
			/* prev: '--boundary--\r' */
			if (*pos == MARK_LF) {
				parser->step = STEP_FINISH;
				break;
			}
			parser->step = STEP_READY;
			break;

		case STEP_END_CR:
			/* prev: '--boundary\r' */
			if (*pos == MARK_LF) {
				parser->step = STEP_CONTENT_START;
				break;
			}
			parser->step = STEP_READY;
			break;

		case STEP_CONTENT_START:
			/* prev: '--boundary\r\n' */
			if (*pos != MARK_CR) {
				nugu_buffer_add(parser->header, pos, 1);
				parser->step = STEP_HEADER;
				break;
			}
			parser->step = STEP_CONTENT_EMPTY_CR;
			break;

		case STEP_HEADER:
			/* prev: '{string}' */
			nugu_buffer_add(parser->header, pos, 1);
			if (*pos == MARK_CR)
				parser->step = STEP_HEADER_CR;
			break;

		case STEP_HEADER_CR:
			/* prev: '{string}\r' */
			nugu_buffer_add(parser->header, pos, 1);
			if (*pos == MARK_LF)
				parser->step = STEP_CONTENT_START;
			else
				parser->step = STEP_HEADER;
			break;

		case STEP_CONTENT_EMPTY_CR:
			/* prev: '\r' */
			if (*pos != MARK_LF) {
				nugu_buffer_add(parser->header, pos, 1);
				parser->step = STEP_CONTENT_START;
			}
			onFoundHeader(parser, nugu_buffer_peek(parser->header),
				      nugu_buffer_get_size(parser->header),
				      userdata);
			nugu_buffer_clear(parser->header);
			parser->step = STEP_BODY;
			break;

		case STEP_BODY:
			/* prev: '\r\n' or '{string}' */
			nugu_buffer_add(parser->body, pos, 1);
			if (*pos == MARK_CR)
				parser->step = STEP_BODY_CR;
			break;

		case STEP_BODY_CR:
			/* prev: '{string}\r' */
			nugu_buffer_add(parser->body, pos, 1);
			if (*pos == MARK_LF)
				parser->step = STEP_BODY_ENDLINE;
			else if (*pos == MARK_CR)
				break;
			else
				parser->step = STEP_BODY;
			break;

		case STEP_BODY_ENDLINE:
			/* prev: '{string}\r\n' */
			nugu_buffer_add(parser->body, pos, 1);
			if (*pos == MARK_CR)
				parser->step = STEP_BODY_ENDLINE_CR;
			else
				parser->step = STEP_BODY;
			break;

		case STEP_BODY_ENDLINE_CR:
			/* prev: '{string}\r\n\r' */
			nugu_buffer_add(parser->body, pos, 1);
			if (*pos != MARK_LF) {
				parser->step = STEP_BODY;
				break;
			}
			parser->step = STEP_READY;
			if (nugu_buffer_get_size(parser->body) > 0) {
				/* Remove trailing (body)CRLF+(emptyline)CRLF */
				nugu_buffer_clear_from(
					parser->body,
					nugu_buffer_get_size(parser->body) - 4);
			}
			onFoundBody(parser, nugu_buffer_peek(parser->body),
				    nugu_buffer_get_size(parser->body),
				    userdata);
			nugu_buffer_clear(parser->body);
			break;

		default:
			break;
		}

		if (parser->step == STEP_FINISH)
			break;
	}

	return 0;
}

void multipart_parser_reset(MultipartParser *parser)
{
	g_return_if_fail(parser != NULL);

	nugu_buffer_clear(parser->header);
	nugu_buffer_clear(parser->body);
}
