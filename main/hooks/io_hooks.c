/*
   +----------------------------------------------------------------------+
   | Copyright © The PHP Group and Contributors.                          |
   +----------------------------------------------------------------------+
   | This source file is subject to the Modified BSD License that is      |
   | bundled with this package in the file LICENSE, and is available      |
   | through the World Wide Web at <https://www.php.net/license/>.        |
   |                                                                      |
   | SPDX-License-Identifier: BSD-3-Clause                                |
   +----------------------------------------------------------------------+
*/

#include "php.h"
#include "main/php_poll.h"
#include "ext/standard/file.h"
#include "ext/standard/io_poll.h"
#include "main/hooks/io_hooks.h"

#ifdef HAVE_POLL_H
#include <poll.h>
#elif HAVE_SYS_POLL_H
#include <sys/poll.h>
#endif

PHPAPI uint32_t php_posix_poll_to_php_poll(int posix_events)
{
	uint32_t result = 0;
	if (posix_events & POLLIN)  result |= PHP_POLL_READ;
	if (posix_events & POLLOUT) result |= PHP_POLL_WRITE;
	if (posix_events & POLLERR) result |= PHP_POLL_ERROR;
	if (posix_events & POLLHUP) result |= PHP_POLL_HUP;
#ifdef POLLRDHUP
	if (posix_events & POLLRDHUP) result |= PHP_POLL_RDHUP;
#endif
	/* TODO: poll API doesn't support POLLPRI */
	return result;
}

PHPAPI void php_set_io_hooks(const php_io_hooks *hooks, size_t size, void *data)
{
	ZEND_ASSERT((!FG(io_hooks).poll && !FG(io_hooks).poll_multi
			&& !FG(io_hooks).sleep && !FG(io_hooks).dtor) || !hooks);

	if (FG(io_hooks).dtor) {
		FG(io_hooks).dtor(FG(io_hooks_data));
	}

	if (hooks == NULL) {
		memset(&FG(io_hooks), 0, sizeof(FG(io_hooks)));
		FG(io_hooks_data) = NULL;
	} else {
		ZEND_ASSERT(size <= sizeof(php_io_hooks));
		memcpy(&FG(io_hooks), hooks, size);
		memset((char *)&FG(io_hooks) + size, 0, sizeof(php_io_hooks) - size);
		FG(io_hooks_data) = data;
	}
}

PHPAPI php_io_hooks_poll_result *php_io_hooks_poll_stream(php_stream *stream, uint32_t events,
		const struct timeval *timeout)
{
	zval handle_zv;
	php_stream_poll_weak_handle_from_stream(&handle_zv, stream);

	zend_long timeout_ms;
	if (timeout == NULL) {
		timeout_ms = -1;
	} else {
		timeout_ms = (zend_long)timeout->tv_sec * 1000 + (zend_long)timeout->tv_usec / 1000;
	}

	php_io_hooks_poll_info info = {
		.handle     = Z_OBJ(handle_zv),
		.events     = events,
		.timeout_ms = timeout_ms,
	};

	ZEND_ASSERT(!(stream->flags & PHP_STREAM_FLAG_BEING_POLLED));
	stream->flags |= PHP_STREAM_FLAG_BEING_POLLED;
	php_io_hooks_poll_result *result = FG(io_hooks).poll(FG(io_hooks_data), &info);
	stream->flags &= ~PHP_STREAM_FLAG_BEING_POLLED;

	zval_ptr_dtor(&handle_zv);
	return result;
}

PHPAPI void php_io_hooks_sleep(zend_long seconds, zend_long nanoseconds)
{
	FG(io_hooks).sleep(FG(io_hooks_data), seconds, nanoseconds);
}
