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

#ifndef PHP_HOOKS_IO_HOOKS_H
#define PHP_HOOKS_IO_HOOKS_H

#include "php.h"

/* The handle is always a weak singleton:
 * - References the inner stream/socket weakly. The Poll API will automatically
 *   stop watching this Handle when the inner stream/socket is collected.
 * - The instance is always the same for a given stream/socket.
 */
typedef struct _php_io_hooks_poll_info {
	zend_object *handle;   /* Io\Poll\Handle */
	uint32_t events;       /* PHP_POLL_* bitmask */
	zend_long timeout_ms;  /* -1 = no timeout */
} php_io_hooks_poll_info;

typedef struct _php_io_hooks_poll_result {
	zend_object *handle;   /* handle that triggered */
	uint32_t events;       /* PHP_POLL_* bitmask of triggered events */
	bool timeout;
} php_io_hooks_poll_result;

typedef struct _php_io_hooks {
	/* Called when a single stream/handle needs to be polled.
	 * Returns NULL on error. */
	php_io_hooks_poll_result *(*poll)(void *data, php_io_hooks_poll_info *info);

	/* Called to poll multiple handles simultaneously.
	 * Returns NULL on error (EG(exception) is set) or on timeout. */
	php_io_hooks_poll_result *(*poll_multi)(void *data, zend_long timeout_ms,
			uint32_t num_infos, php_io_hooks_poll_info *infos);

	/* Called by nanosleep()/usleep()/sleep(). */
	void (*sleep)(void *data, zend_long seconds, zend_long nanoseconds);

	/* Called when hooks are unregistered or replaced. */
	void (*dtor)(void *data);
} php_io_hooks;

/* Register a set of I/O hooks. Pass NULL to unregister.
 * size is sizeof(*hooks) as seen by the caller; trailing unknown fields are zeroed. */
PHPAPI void php_set_io_hooks(const php_io_hooks *hooks, size_t size, void *data);

/* Convert POSIX poll(2) events to a PHP_POLL_* bitmask. */
PHPAPI uint32_t php_posix_poll_to_php_poll(int posix_events);

/* Poll a single stream; events is a PHP_POLL_* bitmask. */
PHPAPI php_io_hooks_poll_result *php_io_hooks_poll_stream(struct _php_stream *stream, uint32_t events,
		const struct timeval *timeout);

/* Call the sleep hook. */
PHPAPI void php_io_hooks_sleep(zend_long seconds, zend_long nanoseconds);

#endif /* PHP_HOOKS_IO_HOOKS_H */
