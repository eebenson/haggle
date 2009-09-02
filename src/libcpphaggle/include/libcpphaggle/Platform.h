/* Copyright 2008-2009 Uppsala University
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
#ifndef _PLATFORM_H
#define _PLATFORM_H

#include "PlatformDetect.h"

/* This file will include platform dependent headers. */

/*
	Windows-specific stuff:
*/
#if defined(OS_WINDOWS)

/* This sets a limit on the number of select:able sockets in Windows.
* Defaults to 64, unless overridden here. */
//#define FD_SETSIZE 512
#include <stdlib.h>
#include <stddef.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <time.h>
#include <winioctl.h>
#include <winerror.h>

#if defined(OS_WINDOWS_DESKTOP)
#include <signal.h>
#endif

#define inline __inline
#pragma warning( disable : 4996 ) /* This disables warnings against
* unsecure versions of strcpy,
* vsnprintf, etc. */

// Prototype for conversion functions
static inline wchar_t *strtowstr(const char *str)
{
	wchar_t *wstr;

	wstr = (wchar_t *)malloc(sizeof(wchar_t) * (strlen(str) + 1));

	if (!wstr)
		return NULL;

	MultiByteToWideChar(CP_UTF8, 0, str, strlen(str) + 1, wstr, strlen(str) + 1);

	return wstr;
}

/* Data types */
typedef	CHAR int8_t;
typedef	BYTE u_int8_t;
typedef SHORT int16_t;
typedef USHORT u_int16_t;
typedef DWORD32 u_int32_t;
typedef DWORD32 in_addr_t;
typedef INT64 int64_t;
typedef UINT64 u_int64_t;
typedef DWORD pid_t;
#if defined(OS_WINDOWS_MOBILE)
typedef unsigned int uintptr_t;
#endif

/* Limits */
#define SIZE_T_MAX UINT_MAX

/* Stuff related to socket programming */
#define CLOSE_SOCKET(sock) closesocket(sock)
#define CLOSE_FILE(fileHandle) CloseHandle(fileHandle)
#define ERRNO WSAGetLastError()
#define STRERROR(err) StrError(err)

// Winsock errors
#define ENOTSOCK WSAENOTSOCK
#define EOPNOTSUPP WSAEOPNOTSUPP
//#define EBADF WSAEBADF
//#define EINVAL WSAEINVAL

//typedef unsigned long socklen_t;
typedef SSIZE_T ssize_t;
#define snprintf _snprintf  // Not sure this is totally safe

/*
	Code for both Mac OS X and Linux:
*/

#elif defined(OS_UNIX)
#if !defined(OS_MACOSX) && !defined(OS_MACOSX_IPHONE) && !defined(OS_ANDROID) 
#include <config.h>
#ifndef PACKAGE
#error "You need to generate config.h via autoreconf and configure. See the README file!"
#endif /* PACKAGE */
#endif
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <string.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/time.h>
#include <netdb.h>
#include <signal.h>
#include <errno.h>

typedef int SOCKET; /* This makes it easier to be compatible to the Windows API */
#define CLOSE_SOCKET(sock) close(sock)
#define CLOSE_FILE(fd) close(fd)
#define SOCKET_ERROR -1 // For compatibility with Windows
#define INVALID_SOCKET -1
#define ERRNO errno
#define STRERROR(err) strerror(err)

#endif

#ifdef OS_MACOSX
// This is a trick to be able to put ObjC id types in C++ class definitions
#if defined(__OBJC__)
typedef id objc_id_t;
#else
typedef void* objc_id_t;
#endif

#endif /* OS_MACOSX */

#endif /* _PLATFORM_H */
