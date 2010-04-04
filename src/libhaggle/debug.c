/* Copyright 2008 Uppsala University
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

#include <stdio.h>
#include <stdarg.h>

#define LIBHAGGLE_INTERNAL
#include <libhaggle/haggle.h>

#if defined(WINCE) || defined(OS_ANDROID)
#define TRACE_TO_FILE 1
#endif

#if defined(TRACE_TO_FILE)
FILE *tr_out = NULL;
FILE *tr_err = NULL;
#else
#define tr_out stdout
#define tr_err stderr
#endif

static int trace_level = 2;

void set_trace_level(int level)
{
	trace_level = level;
}

int libhaggle_debug_init()
{
#ifdef TRACE_TO_FILE
	const char *path = platform_get_path(PLATFORM_PATH_PRIVATE, "/libhaggle.txt");

	if (!path || tr_out || tr_err)
		return -1;

	tr_out = tr_err = fopen(path, "w");

	if (!tr_out) {
		fprintf(stderr, "Could not open file %s\n", path);
		return -1;
	}
	
#endif
	return 0;
}

void libhaggle_debug_fini()
{
#ifdef TRACE_TO_FILE
	if (tr_out)
		fclose(tr_out);
#endif
}
int libhaggle_trace(int err, const char *func, const char *fmt, ...)
{
	static char buf[1024];
	struct timeval now;
	va_list args;
	int len;

	if (trace_level == 0 || (trace_level == 1 && err == 0))
		return 0;

#if defined(OS_ANDROID)
	if (tr_out == NULL)
		libhaggle_debug_init();
#endif
	gettimeofday(&now, NULL);

	va_start(args, fmt);
#ifdef WINCE
	len = _vsnprintf(buf, 1024, fmt, args);
#else
	len = vsnprintf(buf, 1024, fmt, args);
#endif
	va_end(args);

	fprintf((err ? tr_err : tr_out), "%ld.%06ld %s: %s", now.tv_sec, now.tv_usec, func, buf);
	fflush(tr_out);

	return 0;
}
