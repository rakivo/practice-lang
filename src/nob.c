// A little subset of <https://github.com/tsoding/musializer/blob/master/nob.h>, just to run a sync command.

// Copyright 2023 Alexey Kutepov <reximkut@gmail.com>
//
// Permission is hereby granted, free of charge, to any person obtaining
// a copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to
// the following conditions:
//
// The above copyright notice and this permission notice shall be
// included in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
// LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
// OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
// WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#include "nob.h"

void nob_cmd_render(Nob_Cmd cmd, Nob_String_Builder *render)
{
	for (size_t i = 0; i < cmd.count; ++i) {
		const char *arg = cmd.items[i];
		if (arg == NULL) break;
		if (i > 0) nob_sb_append_cstr(render, " ");
		if (!strchr(arg, ' ')) {
			nob_sb_append_cstr(render, arg);
		} else {
			nob_da_append(render, '\'');
			nob_sb_append_cstr(render, arg);
			nob_da_append(render, '\'');
		}
	}
}

Nob_Proc nob_cmd_run_async(Nob_Cmd cmd, bool echo)
{
	if (cmd.count < 1) {
		nob_log(NOB_ERROR, "Could not run empty command");
		return NOB_INVALID_PROC;
	}

	Nob_String_Builder sb = {0};
	if (echo) {
		nob_cmd_render(cmd, &sb);
		nob_sb_append_null(&sb);
		nob_log(NOB_INFO, "CMD: %s", sb.items);
		nob_sb_free(sb);
	}
	memset(&sb, 0, sizeof(sb));

#ifdef _WIN32
	// https://docs.microsoft.com/en-us/windows/win32/procthread/creating-a-child-process-with-redirected-input-and-output

	STARTUPINFO siStartInfo;
	ZeroMemory(&siStartInfo, sizeof(siStartInfo));
	siStartInfo.cb = sizeof(STARTUPINFO);
	// NOTE: theoretically setting NULL to std handles should not be a problem
	// https://docs.microsoft.com/en-us/windows/console/getstdhandle?redirectedfrom=MSDN#attachdetach-behavior
	// TODO: check for errors in GetStdHandle
	siStartInfo.hStdError = GetStdHandle(STD_ERROR_HANDLE);
	siStartInfo.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
	siStartInfo.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
	siStartInfo.dwFlags |= STARTF_USESTDHANDLES;

	PROCESS_INFORMATION piProcInfo;
	ZeroMemory(&piProcInfo, sizeof(PROCESS_INFORMATION));

	// TODO: use a more reliable rendering of the command instead of cmd_render
	// cmd_render is for logging primarily
	nob_cmd_render(cmd, &sb);
	nob_sb_append_null(&sb);
	BOOL bSuccess = CreateProcessA(NULL, sb.items, NULL, NULL, TRUE, 0, NULL, NULL, &siStartInfo, &piProcInfo);
	nob_sb_free(sb);

	if (!bSuccess) {
		nob_log(NOB_ERROR, "Could not create child process: %lu", GetLastError());
		return NOB_INVALID_PROC;
	}

	CloseHandle(piProcInfo.hThread);

	return piProcInfo.hProcess;
#else
	pid_t cpid = fork();
	if (cpid < 0) {
		nob_log(NOB_ERROR, "Could not fork child process: %s", strerror(errno));
		return NOB_INVALID_PROC;
	}

	if (cpid == 0) {
		// NOTE: This leaks a bit of memory in the child process.
		// But do we actually care? It's a one off leak anyway...
		Nob_Cmd cmd_null = {0};
		nob_da_append_many(&cmd_null, cmd.items, cmd.count);
		nob_cmd_append(&cmd_null, NULL);

		if (execvp(cmd.items[0], (char * const*) cmd_null.items) < 0) {
			nob_log(NOB_ERROR, "Could not exec child process: %s", strerror(errno));
			exit(1);
		}
		NOB_ASSERT(0 && "unreachable");
	}

	return cpid;
#endif
}

void nob_log(Nob_Log_Level level, const char *fmt, ...)
{
	switch (level) {
	case NOB_INFO:
		fprintf(stderr, "[INFO] ");
		break;
	case NOB_WARNING:
		fprintf(stderr, "[WARNING] ");
		break;
	case NOB_ERROR:
		fprintf(stderr, "[ERROR] ");
		break;
	default:
		NOB_ASSERT(0 && "unreachable");
	}

	va_list args;
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
	fprintf(stderr, "\n");
}

bool nob_proc_wait(Nob_Proc proc)
{
	if (proc == NOB_INVALID_PROC) return false;

#ifdef _WIN32
	DWORD result = WaitForSingleObject(
		proc,	// HANDLE hHandle,
		INFINITE // DWORD  dwMilliseconds
	);

	if (result == WAIT_FAILED) {
		nob_log(NOB_ERROR, "could not wait on child process: %lu", GetLastError());
		return false;
	}

	DWORD exit_status;
	if (!GetExitCodeProcess(proc, &exit_status)) {
		nob_log(NOB_ERROR, "could not get process exit code: %lu", GetLastError());
		return false;
	}

	if (exit_status != 0) {
		nob_log(NOB_ERROR, "command exited with exit code %lu", exit_status);
		return false;
	}

	CloseHandle(proc);

	return true;
#else
	for (;;) {
		int wstatus = 0;
		if (waitpid(proc, &wstatus, 0) < 0) {
			nob_log(NOB_ERROR, "could not wait on command (pid %d): %s", proc, strerror(errno));
			return false;
		}

		if (WIFEXITED(wstatus)) {
			int exit_status = WEXITSTATUS(wstatus);
			if (exit_status != 0) {
				nob_log(NOB_ERROR, "command exited with exit code %d", exit_status);
				return false;
			}

			break;
		}
	}

	return true;
#endif
}

bool nob_cmd_run_sync(Nob_Cmd cmd, bool echo)
{
	Nob_Proc p = nob_cmd_run_async(cmd, echo);
	if (p == NOB_INVALID_PROC) return false;
	return nob_proc_wait(p);
}
