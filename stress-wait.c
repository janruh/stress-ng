/*
 * Copyright (C) 2013-2017 Canonical, Ltd.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * This code is a complete clean re-write of the stress tool by
 * Colin Ian King <colin.king@canonical.com> and attempts to be
 * backwardly compatible with the stress tool by Amos Waterland
 * <apw@rossby.metr.ou.edu> but has more stress tests and more
 * functionality.
 *
 */
#include "stress-ng.h"

#if !defined(__gnu_hurd__) && !defined(__NetBSD__)

#define ABORT_TIMEOUT	(8.0)

/*
 *  spawn()
 *	spawn a process
 */
static pid_t spawn(
	args_t *args,
	void (*func)(args_t *args, const pid_t pid),
	pid_t pid_arg)
{
	pid_t pid;

again:
	pid = fork();
	if (pid < 0) {
		if (opt_do_run && (errno == EAGAIN))
			goto again;
		return -1;
	}
	if (pid == 0) {
		(void)setpgid(0, pgrp);
		stress_parent_died_alarm();

		func(args, pid_arg);
		exit(EXIT_SUCCESS);
	}
	(void)setpgid(pid, pgrp);
	return pid;
}

/*
 *  runner()
 *	this process pauses, but is continually being
 *	stopped and continued by the killer process
 */
static void runner(
	args_t *args,
	const pid_t pid)
{
	(void)pid;

	pr_dbg(stderr, "%s: wait: runner started [%d]\n", args->name, (int)getpid());

	do {
		(void)pause();
	} while (opt_do_run && (!args->max_ops || *args->counter < args->max_ops));

	kill(getppid(), SIGALRM);
	exit(EXIT_SUCCESS);
}

/*
 *  killer()
 *	this continually stops and continues the runner process
 */
static void killer(
	args_t *args,
	const pid_t pid)
{
	double start = time_now();
	uint64_t last_counter = *args->counter;

	pr_dbg(stderr, "%s: wait: killer started [%d]\n", args->name, (int)getpid());

	do {
		(void)kill(pid, SIGSTOP);
		(void)kill(pid, SIGCONT);

		/*
		 *  The waits may be blocked and
		 *  so the counter is not being updated.
		 *  If it is blocked for too long bail out
		 *  so we don't get stuck in the parent
		 *  waiter indefintely.
		 */
		if (last_counter == *args->counter) {
			if (time_now() - start > ABORT_TIMEOUT) {
				pr_dbg(stderr, "%s: waits were blocked, "
					"aborting\n", args->name);
				break;
			}
		} else {
			start = time_now();
			last_counter = *args->counter;
		}
	} while (opt_do_run && (!args->max_ops || *args->counter < args->max_ops));

	/* forcefully kill runner, wait is in parent */
	(void)kill(pid, SIGKILL);
	/* tell parent to wake up! */
	(void)kill(getppid(), SIGALRM);
	exit(EXIT_SUCCESS);
}

/*
 *  stress_wait
 *	stress wait*() family of calls
 */
int stress_wait(args_t *args)
{
	int status, ret = EXIT_SUCCESS;
	pid_t pid_r, pid_k;

	pr_dbg(stderr, "%s: waiter started [%d]\n",
		args->name, (int)args->pid);

	pid_r = spawn(args, runner, 0);
	if (pid_r < 0) {
		pr_fail_dbg("fork");
		exit(EXIT_FAILURE);
	}

	pid_k = spawn(args, killer, pid_r);
	if (pid_k < 0) {
		pr_fail_dbg("fork");
		ret = EXIT_FAILURE;
		goto tidy;
	}

	do {
#if defined(WCONINUED)
		(void)waitpid(pid_r, &status, WCONTINUED);
#else
		(void)waitpid(pid_r, &status, 0);
#endif
		if (!opt_do_run)
			break;
#if defined(WIFCONINUED)
		if (WIFCONTINUED(status))
			inc_counter(args);
#else
		inc_counter(args);
#endif

#if _SVID_SOURCE || _XOPEN_SOURCE >= 500 || \
    _XOPEN_SOURCE && _XOPEN_SOURCE_EXTENDED || \
    _POSIX_C_SOURCE >= 200809L
		{
			siginfo_t info;

#if defined(WCONINUED)
			(void)waitid(P_PID, pid_r, &info, WCONTINUED);
#else
			(void)waitid(P_PID, pid_r, &info, 0);
#endif
			if (!opt_do_run)
				break;
#if defined(WIFCONINUED)
			if (WIFCONTINUED(status))
				inc_counter(args);
#else
			inc_counter(args);
#endif
		}
#endif
	} while (opt_do_run && (!args->max_ops || *args->counter < args->max_ops));

	(void)kill(pid_k, SIGKILL);
	(void)waitpid(pid_k, &status, 0);
tidy:
	(void)kill(pid_r, SIGKILL);
	(void)waitpid(pid_r, &status, 0);

	return ret;
}
#else
int stress_wait(args_t *args)
{
	return stress_not_implemented(args);
}
#endif
