/*
   +----------------------------------------------------------------------+
   | Copyright (c) The PHP Group                                          |
   +----------------------------------------------------------------------+
   | This source file is subject to version 3.01 of the PHP license,      |
   | that is bundled with this package in the file LICENSE, and is        |
   | available through the world-wide-web at the following url:           |
   | https://www.php.net/license/3_01.txt                                 |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
*/

#ifndef PHP_IO_HOOKS_H
#define PHP_IO_HOOKS_H

#include "main/php.h"
#include "Zend/zend_types.h"
#include "ext/standard/file.h"

PHPAPI extern zend_class_entry *php_io_hooks_poll_info_ce;
PHPAPI extern zend_class_entry *php_io_hooks_poll_result_ce;

#define PHP_HAS_IO_POLL_HOOK() ZEND_FCC_INITIALIZED(FG(io_hooks_poll_fcc))
#define PHP_HAS_IO_SLEEP_HOOK() ZEND_FCC_INITIALIZED(FG(io_hooks_sleep_fcc))

PHPAPI zend_object *php_io_hooks_poll_stream(php_stream *stream, int events, const struct timeval *timeout);
PHPAPI void php_io_hooks_sleep(zend_long seconds, zend_long nanoseconds);

PHP_MINIT_FUNCTION(io_hooks);

#endif /* PHP_IO_HOOKS_H */
