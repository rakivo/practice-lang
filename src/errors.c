// Copyright (c) 2019 Christoffer Lerno. All rights reserved.
// Use of this source code is governed by the GNU LGPLv3.0 license
// a copy of which can be found in the LICENSE file.

#include "lib.h"
#include "common.h"

#include <stdarg.h>

void evprintf(const char *format, va_list list)
{
	vfprintf(stderr, format, list);
}

void eprintf(const char *format, ...)
{
	va_list arglist;
	va_start(arglist, format);
	vfprintf(stderr, format, arglist);
	va_end(arglist);
}

void
report_error_noexit(const char *func, const char *file,
							const size_t line, const char *format, ...)
{
#ifdef DEBUG
	eprintf("%s:%d: in %s(...)\n", file, line, func);
#else
	UNUSED_VAR(func);
	UNUSED_VAR(file);
	UNUSED_VAR(line);
#endif
	va_list arglist;
	va_start(arglist, format);
	vfprintf(stderr, format, arglist);
	fprintf(stderr, "\n");
	va_end(arglist);
}

NORETURN void
report_error_(const char *func, const char *file,
							const size_t line, const char *format, ...)
{
#ifdef DEBUG
	eprintf("%s:%d: in %s(...)\n", file, line, func);
#else
	UNUSED_VAR(func);
	UNUSED_VAR(file);
	UNUSED_VAR(line);
#endif
	va_list arglist;
	va_start(arglist, format);
	vfprintf(stderr, format, arglist);
	fprintf(stderr, "\n");
	va_end(arglist);
	exit(EXIT_FAILURE);
}

NORETURN void error_exit(const char *format, ...)
{
	va_list arglist;
	va_start(arglist, format);
	vfprintf(stderr, format, arglist);
	fprintf(stderr, "\n");
	va_end(arglist);
	exit(EXIT_FAILURE);
}
