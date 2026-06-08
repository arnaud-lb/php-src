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

#ifndef PHP_IO_POLL_H
#define PHP_IO_POLL_H

#include "Zend/zend_types.h"
#include "main/php.h"

PHPAPI extern zend_class_entry *php_io_poll_event_class_entry;
PHPAPI extern zend_class_entry *php_io_poll_handle_class_entry;
PHPAPI extern zend_class_entry *php_stream_poll_handle_class_entry;

PHPAPI zend_result php_io_poll_events_to_event_enums(uint32_t events, zval *event_enums);

PHPAPI void php_stream_poll_handle_from_stream(zval *dest, php_stream *stream);

#endif /* PHP_IO_POLL_H */
