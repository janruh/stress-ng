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

/*
 *  stress_null
 *	stress writing to /dev/null
 */
int stress_null(args_t *args)
{
	int fd;
	char buffer[4096];

	if ((fd = open("/dev/null", O_WRONLY)) < 0) {
		pr_fail_err("open");
		return EXIT_FAILURE;
	}

	memset(buffer, 0xff, sizeof(buffer));
	do {
		ssize_t ret;

		ret = write(fd, buffer, sizeof(buffer));
		if (ret <= 0) {
			if ((errno == EAGAIN) || (errno == EINTR))
				continue;
			if (errno) {
				pr_fail_err("write");
				(void)close(fd);
				return EXIT_FAILURE;
			}
			continue;
		}
		inc_counter(args);
	} while (opt_do_run && (!args->max_ops || *args->counter < args->max_ops));
	(void)close(fd);

	return EXIT_SUCCESS;
}
