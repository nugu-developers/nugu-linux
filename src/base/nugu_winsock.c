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

#ifdef __MSYS__

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#include <iphlpapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <glib.h>
#include <unistd.h>

#include "base/nugu_log.h"
#include "base/nugu_winsock.h"

#define DEFAULT_SELECT_TIMEOUT_MSEC 10
#define UNIX_PATH_MAX 108

struct _sockaddr_un_t {
	ADDRESS_FAMILY sun_family;
	char sun_path[UNIX_PATH_MAX];
};

struct _nugu_winsock_t {
	SOCKET server_handle;
	SOCKET listen_handle;
	SOCKET client_handle;
};

static GList *_wsocks;
static WSADATA _wsa_data;
static int _initialze;
static int _sock_num;

static void _nugu_winsock_free(NuguWinSocket *wsock)
{
	if (!wsock)
		return;

	if (wsock->server_handle != INVALID_SOCKET)
		closesocket(wsock->server_handle);

	if (wsock->listen_handle != INVALID_SOCKET)
		closesocket(wsock->listen_handle);

	if (wsock->client_handle != INVALID_SOCKET)
		closesocket(wsock->client_handle);

	free(wsock);
}

static int _create_socket_pair(NuguWinSocket *wsock, const char *sock_path)
{
	struct _sockaddr_un_t sock_addr;
	int ret;

	unlink(sock_path);

	wsock->listen_handle = socket(AF_UNIX, SOCK_STREAM, 0);
	if (wsock->listen_handle == INVALID_SOCKET) {
		nugu_error("socket failed with error: %ld", WSAGetLastError());
		return -1;
	}

	memset((char *)&sock_addr, 0, sizeof(sock_addr));
	sock_addr.sun_family = AF_UNIX;
	strcpy(sock_addr.sun_path, sock_path);

	ret = bind(wsock->listen_handle, (struct sockaddr *)&sock_addr,
		   sizeof(sock_addr));
	if (ret == SOCKET_ERROR) {
		nugu_error("bind failed with error: %d", WSAGetLastError());
		return -1;
	}

	ret = listen(wsock->listen_handle, SOMAXCONN);
	if (ret == SOCKET_ERROR) {
		nugu_error("listen failed with error: %d", WSAGetLastError());
		return -1;
	}

	wsock->server_handle = socket(AF_UNIX, SOCK_STREAM, 0);
	if (wsock->server_handle == INVALID_SOCKET) {
		nugu_error("server failed with error: %d", WSAGetLastError());
		return -1;
	}

	ret = connect(wsock->server_handle, (struct sockaddr *)&sock_addr,
		      sizeof(sock_addr));
	if (ret == SOCKET_ERROR) {
		nugu_error("[client] Unable to connect to server!");
		return -1;
	}

	wsock->client_handle = accept(wsock->listen_handle, NULL, NULL);
	if (wsock->client_handle == INVALID_SOCKET) {
		nugu_error("client failed with error: %d", WSAGetLastError());
		return -1;
	}

	nugu_info("socket pair(path: %s) is created", sock_path);
	return 0;
}

int nugu_winsock_init(void)
{
	if (_initialze)
		return 0;

	nugu_info("initialize window socket");

	if (WSAStartup(MAKEWORD(2, 2), &_wsa_data) != 0) {
		nugu_error("WSAStartup failed");
		return -1;
	}

	_initialze = 1;
	_sock_num = 0;
	return 0;
}

void nugu_winsock_deinit(void)
{
	nugu_info("deinitialize window socket");

	if (_wsocks != NULL) {
		g_list_free_full(_wsocks, (GDestroyNotify)_nugu_winsock_free);
		_wsocks = NULL;
	}

	WSACleanup();
	_initialze = 0;
	_sock_num = 0;
}

NuguWinSocket *nugu_winsock_create(void)
{
	char sock_path[128];

	if (_initialze == 0) {
		nugu_error("nugu_winsock_init() is not called");
		return NULL;
	}

	NuguWinSocket *wsock =
		(NuguWinSocket *)calloc(1, sizeof(NuguWinSocket));
	wsock->server_handle = INVALID_SOCKET;
	wsock->listen_handle = INVALID_SOCKET;
	wsock->client_handle = INVALID_SOCKET;

	snprintf(sock_path, 128, "./server.sock.%d", _sock_num);

	if (_create_socket_pair(wsock, sock_path) != 0) {
		nugu_error("Failed to create socketpair");
		_nugu_winsock_free(wsock);
		return NULL;
	}

	_wsocks = g_list_append(_wsocks, wsock);

	_sock_num++;
	return wsock;
}

void nugu_winsock_remove(NuguWinSocket *wsock)
{
	GList *l;

	l = g_list_find(_wsocks, wsock);
	if (l) {
		_wsocks = g_list_remove_link(_wsocks, l);
		g_list_free_1(l);
	}

	_nugu_winsock_free(wsock);
}

int nugu_winsock_get_handle(NuguWinSocket *wsock, NuguWinSocketType type)
{
	if (type == NUGU_WINSOCKET_SERVER)
		return wsock->server_handle;
	else
		return wsock->client_handle;
}

int nugu_winsock_check_for_data(int handle)
{
	fd_set readfds;
	TIMEVAL timeout;
	int ret;

	FD_ZERO(&readfds);
	FD_SET((SOCKET)handle, &readfds);

	timeout.tv_sec = 0;
	timeout.tv_usec = DEFAULT_SELECT_TIMEOUT_MSEC * 1000;

	ret = select(0, &readfds, NULL, NULL, (const TIMEVAL *)&timeout);
	if (ret == 0 || ret == SOCKET_ERROR)
		return -1;

	return 0;
}

int nugu_winsock_read(int handle, char *buf, int len)
{
	int ret = recv((SOCKET)handle, buf, len, 0);

	if (ret <= 0) {
		nugu_error("receive failed");
		return -1;
	}

	return ret;
}

int nugu_winsock_write(int handle, const char *buf, int len)
{
	int ret = send((SOCKET)handle, buf, len, 0);

	if (ret <= 0) {
		nugu_error("write failed");
		return -1;
	}

	return ret;
}

#endif // __MSYS__
